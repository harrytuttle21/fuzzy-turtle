#include <QThread>
// Fuzzy John DAW Pulses — lien avec le moteur audio. Licence : GPLv3.
#include "engine_link.h"
#include "osc.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QNetworkDatagram>
#include <QUdpSocket>

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <thread>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {
const char* kEngineRelative = "prototypes/pad-sound/build/fuzzy_pad_sound_artefacts/Release/fuzzy_pad_sound";
}

EngineLink::EngineLink (QObject* parent) : QObject (parent)
{
    sendSocket = ::socket (AF_INET, SOCK_DGRAM, 0);
    // macOS : tampon d'envoi UDP de 9 Ko par defaut ; en rafale, les messages en trop sont
    // jetes (ENOBUFS). Une fin de note jetee = une note bloquee. On l'agrandit.
    const int size = 1 << 20;
    ::setsockopt (sendSocket, SOL_SOCKET, SO_SNDBUF, &size, sizeof (size));
    replies = new QUdpSocket (this);
    if (! replies->bind (QHostAddress::LocalHost, kReplyPort))
        lastError = tr ("Port %1 already in use (is the Python prototype still running?)").arg (kReplyPort);
    connect (replies, &QUdpSocket::readyRead, this, &EngineLink::readReplies);
    process.setProcessChannelMode (QProcess::ForwardedChannels);   // messages du moteur dans le terminal
    connect (&process, &QProcess::finished, this, [this] (int code, QProcess::ExitStatus)
    {
        audio = {};
        emit stopped (tr ("Audio engine stopped (code %1)").arg (code));
    });
}

EngineLink::~EngineLink()
{
    stop();
    if (sendSocket >= 0)
        ::close (sendSocket);
}

QString EngineLink::findEngine()
{
    const QString env = qEnvironmentVariable ("FUZZY_ENGINE");
    if (! env.isEmpty() && QFileInfo (env).isExecutable())
        return env;
    // L'app est dans apps/daw-pulses/build/... (ou dans un .app) : on remonte jusqu'au depot.
    QDir dir (QCoreApplication::applicationDirPath());
    for (int i = 0; i < 10; ++i)
    {
        const QString candidate = dir.filePath (kEngineRelative);
        if (QFileInfo (candidate).isExecutable())
            return candidate;
        if (! dir.cdUp())
            break;
    }
    return {};
}

bool EngineLink::start (const QString& output)
{
    if (running())
        return true;
    const QString program = findEngine();
    if (program.isEmpty())
    {
        lastError = tr ("Audio engine not found: build prototypes/pad-sound (cmake --build build)");
        return false;
    }
    QStringList args { "--osc", "--quiet" };
    if (! output.isEmpty())
        args << "--output" << output;
#ifndef Q_OS_WIN
    // Un ancien moteur reste parfois en marche (appli plantee) : il garde le port, le nouveau
    // ne peut pas demarrer et les samples ne sont plus envoyes. On l'arrete d'abord.
    if (QProcess::execute ("pkill", { "-f", QFileInfo (program).fileName() + " --osc" }) == 0)
        QThread::msleep (300);
#endif
    process.start (program, args);
    if (! process.waitForStarted (3000))
    {
        lastError = tr ("Cannot start the audio engine: %1").arg (process.errorString());
        return false;
    }
    lastError.clear();
    return true;
}

void EngineLink::stop()
{
    if (process.state() == QProcess::NotRunning)
        return;
    panic();
    process.terminate();
    if (! process.waitForFinished (1500))
        process.kill();
}

bool EngineLink::running() const
{
    return process.state() == QProcess::Running;
}

void EngineLink::send (const QByteArray& packet) const
{
    if (sendSocket < 0)
        return;
    sockaddr_in to {};
    to.sin_family = AF_INET;
    to.sin_port = htons (kEnginePort);
    to.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
    // Si macOS refuse un instant (tampon plein), on reessaie au lieu de perdre le message.
    for (int attempt = 0; attempt < 40; ++attempt)
    {
        if (::sendto (sendSocket, packet.constData(), (size_t) packet.size(), 0,
                      reinterpret_cast<const sockaddr*> (&to), sizeof (to)) >= 0)
            return;
        if (errno != ENOBUFS && errno != EAGAIN && errno != EWOULDBLOCK)
            return;                                  // moteur absent... : rien a faire
        std::this_thread::sleep_for (std::chrono::microseconds (200));
    }
    const int n = ++lost;
    qWarning ("[moteur] message perdu (%d en tout) : envois trop nombreux", n);
}

void EngineLink::hit (int sound, int velocity, int note, int group, bool adsr, int releaseMs,
                      int gateMs, int synth, int pan, int poly) const
{
    send (fuzzy::osc::message ("/fuzzy/proto/hit",
                               { sound, velocity, note, group, adsr ? 1 : 0, releaseMs, gateMs, synth, pan, poly }));
}

void EngineLink::cut (int sound, int group) const
{
    send (fuzzy::osc::message ("/fuzzy/proto/cut", { sound, group }));
}

void EngineLink::noteOff (int sound, int note, int group) const
{
    send (fuzzy::osc::message ("/fuzzy/proto/off", { sound, note, group }));
}

void EngineLink::offLive() const
{
    send (fuzzy::osc::message ("/fuzzy/proto/off_live"));
}

void EngineLink::click (bool accent, int volume) const
{
    const int vel = qBound (0, (accent ? 127 : 95) * volume / 127, 127);
    if (vel > 0)   // synthe 6 = Click du moteur, groupe -1 : jamais coupe par Mute / Solo
        send (fuzzy::osc::message ("/fuzzy/proto/hit", { 1, vel, accent ? 96 : 84, -1, 0, 50, 0, 6, 64 }));
}

void EngineLink::panic() const
{
    send (fuzzy::osc::message ("/fuzzy/proto/panic"));
}

void EngineLink::setSample (int group, int sound, const QString& path, int rootNote) const
{
    send (fuzzy::osc::message ("/fuzzy/proto/sample", { group, sound, path, rootNote }));
}

void EngineLink::setLoop (int group, int sound, bool on, float start, float end) const
{
    send (fuzzy::osc::message ("/fuzzy/proto/sample/loop", { group, sound, on ? 1 : 0, start, end }));
}

void EngineLink::setReverse (int group, int sound, bool on) const
{
    send (fuzzy::osc::message ("/fuzzy/proto/sample/reverse", { group, sound, on ? 1 : 0 }));
}

void EngineLink::requestPeaks (int group, int sound) const
{
    send (fuzzy::osc::message ("/fuzzy/proto/sample/peaks", { group, sound }));
}

void EngineLink::setOutput (const QString& output) const
{
    send (fuzzy::osc::message ("/fuzzy/proto/audio/set", { output, QString(), 0, 0 }));
}

void EngineLink::setAudio (const QString& output, const QString& input, int rate, int buffer) const
{
    send (fuzzy::osc::message ("/fuzzy/proto/audio/set", { output, input, rate, buffer }));
}

void EngineLink::readReplies()
{
    while (replies->hasPendingDatagrams())
    {
        const QNetworkDatagram d = replies->receiveDatagram();
        QString address;
        QVariantList a;
        if (! fuzzy::osc::parse (d.data(), address, a))
            continue;
        if (address == "/fuzzy/proto/audio/state" && a.size() >= 6)
        {
            audio.device = a[0].toString();
            audio.rate = a[1].toDouble();
            audio.buffer = a[2].toInt();
            audio.latencyMs = a[3].toDouble();
            audio.xruns = a[4].toInt();
            audio.outputs = a[5].toString().split ('\n', Qt::SkipEmptyParts);
            audio.cpu = a.size() >= 10 ? a[9].toDouble() : -1;
            const auto ints = [] (const QVariant& v)
            {
                QList<int> out;
                for (const auto& part : v.toString().split (',', Qt::SkipEmptyParts))
                    out << part.toInt();
                return out;
            };
            audio.rates = a.size() > 6 ? ints (a[6]) : QList<int>();
            audio.buffers = a.size() > 7 ? ints (a[7]) : QList<int>();
            audio.inputs = a.size() > 8 ? a[8].toString().split ('\n', Qt::SkipEmptyParts) : QStringList();
            emit stateChanged();
        }
        else if (address == "/fuzzy/proto/sample/peaks" && a.size() >= 3)
        {
            QList<float> peaks;
            for (int i = 3; i < a.size(); ++i)
                peaks.append ((float) a[i].toDouble());
            emit samplePeaks (a[0].toInt(), a[1].toInt(), a[2].toDouble(), peaks);
        }
        else if (address == "/fuzzy/proto/sample/error" && ! a.isEmpty())
            emit sampleError (a[0].toString());
        else if (address == "/fuzzy/proto/meters" && a.size() >= 17)
        {
            std::array<float, 17> peaks {};
            for (size_t i = 0; i < 17; ++i)
                peaks[i] = (float) a[(int) i].toDouble();
            emit meters (peaks);
        }
        else if (address == "/fuzzy/proto/soundmeters" && a.size() >= 256)
        {
            std::vector<float> peaks (256);
            for (int i = 0; i < 256; ++i)
                peaks[(size_t) i] = (float) a[i].toDouble();
            emit soundMeters (peaks);
        }
    }
}
