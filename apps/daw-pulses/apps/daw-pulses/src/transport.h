// Fuzzy John DAW Pulses — transport (lecture / arret, tempo) et metronome.
// L'horloge tourne dans sa propre tache, a l'heure precise : jamais retardee par l'interface.
// Licence : GPLv3.
#pragma once
#include <QObject>

#include <atomic>
#include <thread>

class EngineLink;

class Transport : public QObject
{
    Q_OBJECT
public:
    explicit Transport (const EngineLink& engine, QObject* parent = nullptr);
    ~Transport() override;

    void play();
    void stop();
    bool playing() const { return isPlaying; }

    void setBpm (double bpm);
    double bpm() const { return tempo; }
    void setMetronome (bool on) { metro = on; }
    bool metronome() const { return metro; }
    void setMetronomeVolume (int v) { metroVolume = v; }
    int metronomeVolume() const { return metroVolume; }

signals:
    void beat (int beatInBar);                   // 0-3, dans la tache de l'interface
    void playingChanged (bool playing);

private:
    void run();

    const EngineLink& engine;
    std::thread clock;
    std::atomic<bool> isPlaying { false }, quit { false };
    std::atomic<double> tempo { 120.0 };
    std::atomic<bool> metro { true };
    std::atomic<int> metroVolume { 90 };
};
