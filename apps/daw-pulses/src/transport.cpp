// Fuzzy John DAW Pulses — transport : horloge, sequenceur, metronome. Licence : GPLv3.
#include "transport.h"
#include "engine_link.h"
#include "sequencer.h"

#include <QMetaObject>

#include <algorithm>
#include <chrono>
#include <vector>

using Clock = std::chrono::steady_clock;

Transport::Transport (const EngineLink& e, seq::Sequencer& s, session::State& st, QObject* parent)
    : QObject (parent), engine (e), sequencer (s), state (st) {}

Transport::~Transport()
{
    // Fermeture : seulement arreter l'horloge, sans prevenir personne (la fenetre et le
    // sequenceur sont peut-etre deja detruits)
    quit = true;
    if (clock.joinable())
        clock.join();
}

void Transport::setBpm (double bpm)
{
    tempo = std::clamp (bpm, 30.0, 300.0);
}

double Transport::position() const
{
    if (! isPlaying)
        return rowIndex;
    const long long now = std::chrono::duration_cast<std::chrono::nanoseconds> (
                              Clock::now().time_since_epoch()).count();
    const double into = double (now - currentRowStartNs.load()) * 1e-9 / rowSeconds();
    return currentRow + std::clamp (into, 0.0, 1.0);
}

void Transport::play()
{
    if (isPlaying)
        return;
    if (clock.joinable())
        clock.join();
    quit = false;
    isPlaying = true;
    clock = std::thread ([this] { run(); });
    emit playingChanged (true);
}

void Transport::pause()
{
    if (! isPlaying && ! clock.joinable())
        return;
    quit = true;
    if (clock.joinable())
        clock.join();
    const bool was = isPlaying.exchange (false);
    if (was)
        emit playingChanged (false);
}

void Transport::stop()
{
    pause();
    rowIndex = 0;
    currentRow = 0;
    sequencer.resetPlayback();                       // patterns en attente : maintenant ; tout repart du debut
    QMetaObject::invokeMethod (this, [this] { emit row (0); }, Qt::QueuedConnection);
}

void Transport::restart()
{
    stop();
    play();
}

void Transport::run()
{
    struct Pending { Clock::time_point when; seq::Hit hit; };
    std::vector<Pending> pending;                // notes decalees (timing fin)

    const auto playHit = [this] (const seq::Hit& h)
    {
        const int sound = h.track + 1;
        const int v = state.outVelocity (h.group, sound, h.velocity);   // volume du son, du groupe, master
        if (v > 0 && state.audible (h.group, sound)    // mute / solo ; son vide : rien
            && state.hasInstrument[(size_t) state.index (h.group, sound)])
        {
            engine.hit (sound, v, state.pitched (h.group, sound, h.note), h.group, state.held (h.group, sound),
                        state.releaseMs[(size_t) state.index (h.group, sound)], std::max (10, h.gateMs),
                        state.effSynth (h.group, sound), state.pan (h.group, sound), state.poly (h.group, sound));
            emit played (h.group, sound, h.note, h.gateMs);
        }
    };
    const auto playDue = [&] (Clock::time_point now)
    {
        for (auto it = pending.begin(); it != pending.end();)
            if (it->when <= now) { playHit (it->hit); it = pending.erase (it); }
            else ++it;
    };

    auto at = Clock::now();                      // heure de la prochaine ligne
    while (! quit)
    {
        // Attendre la ligne, en jouant les notes decalees qui tombent avant
        while (! quit)
        {
            auto wake = at;
            for (const auto& p : pending)
                wake = std::min (wake, p.when);
            // par petits morceaux : STOP repond vite, meme a tempo lent
            wake = std::min (wake, Clock::now() + std::chrono::milliseconds (20));
            std::this_thread::sleep_until (wake);
            playDue (Clock::now());
            if (Clock::now() >= at)
                break;
        }
        if (quit)
            break;

        const int r = rowIndex;
        const double rowSec = rowSeconds();
        currentRow = r;
        currentRowStartNs = std::chrono::duration_cast<std::chrono::nanoseconds> (at.time_since_epoch()).count();
        if (metro && r % 4 == 0)
            engine.click (r % 16 == 0, metroVolume);           // un coup par temps, accent sur la mesure
        for (const auto& h : sequencer.hitsForRow (r, rowSec * 1000.0))
        {
            if (h.delayRows <= 0.0005)
                playHit (h);
            else
                pending.push_back ({ at + std::chrono::duration_cast<Clock::duration> (
                                         std::chrono::duration<double> (h.delayRows * rowSec)), h });
        }
        if (r % 4 == 0)
            QMetaObject::invokeMethod (this, [this, r] { emit beat ((r / 4) % 4); }, Qt::QueuedConnection);
        QMetaObject::invokeMethod (this, [this, r] { emit row (r); }, Qt::QueuedConnection);
        rowIndex = r + 1;
        at += std::chrono::duration_cast<Clock::duration> (std::chrono::duration<double> (rowSec));
    }
}
