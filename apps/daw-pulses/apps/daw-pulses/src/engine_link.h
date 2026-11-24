// Fuzzy John DAW Pulses — lien avec le moteur audio (fuzzy_pad_sound --osc, JUCE).
// Le moteur tourne dans son propre programme ; on lui parle en OSC (UDP local) :
// port 57390 pour lui envoyer les notes, port 57391 pour recevoir son etat.
// Licence : GPLv3.
#pragma once
#include <QObject>
#include <QProcess>
#include <QStringList>

#include <atomic>

class QUdpSocket;

struct AudioState
{
    QString device;
    double rate = 0;
    int buffer = 0;
    double latencyMs = 0;
    int xruns = 0;
    QStringList outputs;
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
              int gateMs = 0, int synth = 0, int pan = 64) const;
    void noteOff (int sound, int note, int group) const;
    void click (bool accent, int volume) const;   // metronome
    void panic() const;
    void setOutput (const QString& output) const;

signals:
    void stateChanged();                          // nouvel etat audio recu
    void stopped (const QString& why);

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
