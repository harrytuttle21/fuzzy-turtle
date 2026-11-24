// Fuzzy John DAW Pulses — transport et metronome. Licence : GPLv3.
#include "transport.h"
#include "engine_link.h"

#include <QMetaObject>
#include <chrono>

using Clock = std::chrono::steady_clock;

Transport::Transport (const EngineLink& e, QObject* parent) : QObject (parent), engine (e) {}

Transport::~Transport()
{
    stop();
}

void Transport::setBpm (double bpm)
{
    tempo = qBound (30.0, bpm, 300.0);
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

void Transport::stop()
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

void Transport::run()
{
    auto next = Clock::now();
    int beatIndex = 0;
    while (! quit)
    {
        std::this_thread::sleep_until (next);
        if (quit)
            break;
        const int inBar = beatIndex % 4;
        if (metro)
            engine.click (inBar == 0, metroVolume);           // envoye tout de suite, a l'heure
        QMetaObject::invokeMethod (this, [this, inBar] { emit beat (inBar); }, Qt::QueuedConnection);
        ++beatIndex;
        next += std::chrono::duration_cast<Clock::duration> (std::chrono::duration<double> (60.0 / tempo));
        // On attend par petits morceaux pour que STOP reponde vite meme a tempo lent.
        while (! quit && Clock::now() + std::chrono::milliseconds (20) < next)
            std::this_thread::sleep_for (std::chrono::milliseconds (10));
    }
}
