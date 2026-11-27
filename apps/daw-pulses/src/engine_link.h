// Fuzzy John DAW Pulses — lien avec le moteur audio (fuzzy_pad_sound --osc, JUCE).
// Le moteur tourne dans son propre programme ; on lui parle en OSC (UDP local) :
// port 57390 pour lui envoyer les notes, port 57391 pour recevoir son etat.
// Licence : GPLv3.
#pragma once
#include <QObject>
#include <QProcess>
#include <QStringList>

#include <array>
#include <vector>
#include <atomic>

class QUdpSocket;

struct AudioState
{
    QString device;
    double rate = 0;
    int buffer = 0;
    double latencyMs = 0;
    int xruns = 0;
    QStringList outputs, inputs;
    QList<int> rates, buffers;                    // choix possibles pour la carte en cours
    double cpu = -1;                              // charge du calcul audio (%), -1 = inconnue
    bool valid() const { return rate > 0; }
};

class EngineLink : public QObject
{
    Q_OBJECT
public:
    static constexpr quint16 kEnginePort = 57390, kReplyPort = 57391;

    explicit EngineLink (QObject* parent = nullptr);
    ~EngineLink() override;

    // Cherche le moteur : variable FUZZY_ENGINE, sinon prototypes/pad-sound/build/...
    // en remontant depuis l'application. Chaine vide si introuvable.
    static QString findEngine();

    bool start (const QString& output = {});    // demarre le moteur (s'il n'est pas deja la)
    void stop();
    bool running() const;
    QString error() const { return lastError; }
    const AudioState& state() const { return audio; }

    // Envoi : utilisable depuis n'importe quelle tache (pads lus dans une tache a part).
    void hit (int sound, int velocity, int note, int group, bool adsr, int releaseMs = 150,
              int gateMs = 0, int synth = 0, int pan = 64, int poly = 4) const;   // poly : notes en meme temps du son
    void noteOff (int sound, int note, int group) const;
    void offLive() const;                            // plus rien de tenu : le moteur relache les notes jouees en direct
    void click (bool accent, int volume) const;   // metronome
    void panic() const;
    // Sample d'un son (WAV, AIFF, FLAC, Ogg, MP3... lu directement) ; "" = retour au son de synthese
    void setSample (int group, int sound, const QString& path, int rootNote) const;
    // Boucle d'un son (debut et fin en fractions du fichier) ; forme d'onde pour l'editeur
    void setLoop (int group, int sound, bool on, float start, float end) const;
    void requestPeaks (int group, int sound) const;
    void setReverse (int group, int sound, bool on) const;   // sample a l'envers
    void setOutput (const QString& output) const;
    // Reglages audio complets (vide / 0 = garder la valeur actuelle)
    void setAudio (const QString& output, const QString& input, int rate, int buffer) const;

signals:
    void stateChanged();                          // nouvel etat audio recu
    void meters (const std::array<float, 17>& peaks);   // vumetres : groupes 0-15 + master
    void soundMeters (const std::vector<float>& peaks); // vumetres des 256 sons (groupe x 16 + son - 1)
    void stopped (const QString& why);
    void sampleError (const QString& path);       // fichier illisible
    void samplePeaks (int group, int sound, double seconds, const QList<float>& peaks);   // 0-1

private:
    void send (const QByteArray& packet) const;
    void readReplies();

    QProcess process;
    QUdpSocket* replies = nullptr;
    int sendSocket = -1;                          // socket UDP simple : envoi sans verrou
    mutable std::atomic<int> lost { 0 };          // messages perdus malgre les nouveaux essais
    AudioState audio;
    QString lastError;
};
