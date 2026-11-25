// Fuzzy John DAW Pulses — transport : horloge, sequenceur, metronome.
// L'horloge tourne dans sa propre tache, a l'heure precise : 4 lignes par temps ; elle joue
// les notes des patterns (decalages fins compris) et le metronome. Jamais retardee par
// l'interface. Licence : GPLv3.
#pragma once
#include "session.h"

#include <QObject>

#include <atomic>
#include <thread>

class EngineLink;
namespace seq { class Sequencer; }

class Transport : public QObject
{
    Q_OBJECT
public:
    Transport (const EngineLink& engine, seq::Sequencer& sequencer, session::State& state,
               QObject* parent = nullptr);
    ~Transport() override;

    void play();                                 // reprend la ou on s'etait arrete
    void pause();                                // arret, la position est gardee
    void stop();                                 // arret et retour au debut
    void restart();                              // retour au debut et lecture
    bool playing() const { return isPlaying; }

    void setBpm (double bpm);
    double bpm() const { return tempo; }
    void setMetronome (bool on) { metro = on; }
    bool metronome() const { return metro; }
    void setMetronomeVolume (int v) { metroVolume = v; }
    int metronomeVolume() const { return metroVolume; }

    // Position de lecture en lignes, avec la fraction (pour l'enregistrement). Sure depuis
    // n'importe quelle tache.
    double position() const;
    double rowSeconds() const { return 60.0 / tempo / 4.0; }

signals:
    void beat (int beatInBar);                   // 0-3, dans la tache de l'interface
    void row (int row);                          // ligne jouee (tete de lecture)
    void playingChanged (bool playing);
    void played (int group, int sound, int note, int gateMs);   // note du pattern (affichage clavier)

private:
    void run();

    const EngineLink& engine;
    seq::Sequencer& sequencer;
    session::State& state;
    std::thread clock;
    std::atomic<bool> isPlaying { false }, quit { false };
    std::atomic<double> tempo { 120.0 };
    std::atomic<bool> metro { true };
    std::atomic<int> metroVolume { 90 };
    std::atomic<int> rowIndex { 0 };             // prochaine ligne a jouer
    std::atomic<int> currentRow { 0 };           // ligne en cours
    std::atomic<long long> currentRowStartNs { 0 };   // heure de la ligne en cours (steady_clock)
};
