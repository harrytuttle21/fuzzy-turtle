// fuzzy-control-probe : outil de test de la bibliotheque sur la Maschine.
//  - affiche tout ce que fait la Maschine (pads, boutons, encodeurs, molette 4D, inconnus) ;
//  - lumieres : groupes dans leurs couleurs, pads vifs quand on les frappe, boutons connus ;
//  - ecrans : gauche = groupe et pads ; droit = derniers evenements et vitesse d'affichage.
//
// Usage : sudo ./build/fuzzy-control-probe [--curve soft|softer|linear|hard]
//                                          [--no-lights] [--no-screens] [--no-correction]
#include "fuzzy_control/display.h"
#include "fuzzy_control/lights.h"
#include "fuzzy_control/maschine_mk3.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

using namespace fuzzy::control;
using Clock = std::chrono::steady_clock;

namespace {
std::atomic<bool> gRunning { true };

const char* kDemoButtons[] = { "play", "rec", "stop", "shift", "pad_mode", "keyboard", "chords",
                               "step", "mute", "solo", "tempo", "volume", "note_repeat", "lock",
                               "follow", "mixer", "midi_channel", "nav_prev", "nav_next",
                               "screen_3", "screen_5", "screen_6", "screen_7", "screen_8" };

Rgb shade (Rgb c, double k) { return { (uint8_t) (c.r * k), (uint8_t) (c.g * k), (uint8_t) (c.b * k) }; }

void drawLeft (Canvas& c, int group, const std::set<int>& down)
{
    const Rgb acc = kGroupScreenColors[(size_t) group];
    c.fill ({ 10, 10, 14 });
    c.fillRect (0, 0, Canvas::kWidth, 26, acc);
    c.text (8, 5, std::string ("FUZZY CONTROL  -  Group ") + (char) ('A' + group), kBlack);
    for (int pad = 1; pad <= 16; ++pad)
    {
        const int col = (pad - 1) % 4, row = 3 - (pad - 1) / 4;     // pad 1 en bas a gauche
        const int x = 8 + col * 117, y = 34 + row * 58;
        const bool lit = down.count (pad) > 0;
        c.fillRect (x, y, 110, 52, lit ? acc : Rgb { 26, 26, 34 });
        c.frameRect (x, y, 110, 52, shade (acc, 0.6));
        const std::string label = "Pad " + std::to_string (pad);
        c.text (x + 55 - Canvas::textWidth (label) / 2, y + 18, label, lit ? kBlack : kWhite);
    }
}

void drawRight (Canvas& c, int group, const std::deque<std::string>& log, const std::string& stats)
{
    const Rgb acc = kGroupScreenColors[(size_t) group];
    c.fill ({ 10, 10, 14 });
    c.fillRect (0, 0, Canvas::kWidth, 26, acc);
    c.text (8, 5, "EVENTS", kBlack);
    int y = 32;
    for (const auto& line : log)
    {
        c.text (8, y, line, kWhite);
        y += 17;
    }
    c.fillRect (0, 250, Canvas::kWidth, 22, { 24, 24, 32 });
    c.text (8, 253, stats, { 180, 180, 180 });
}
} // namespace

int main (int argc, char* argv[])
{
    double exponent = 0.5;
    bool lights = true, screens = true, correction = true;
    for (int i = 1; i < argc; ++i)
    {
        if (! std::strcmp (argv[i], "--no-lights")) lights = false;
        else if (! std::strcmp (argv[i], "--no-screens")) screens = false;
        else if (! std::strcmp (argv[i], "--no-correction")) correction = false;
        else if (! std::strcmp (argv[i], "--curve") && i + 1 < argc)
        {
            const char* c = argv[++i];
            exponent = ! std::strcmp (c, "linear") ? 1.0 : ! std::strcmp (c, "softer") ? 0.35
                     : ! std::strcmp (c, "hard") ? 1.6 : 0.5;
        }
    }
    std::signal (SIGINT, [] (int) { gRunning = false; });

    MaschineMk3 maschine;
    std::string error;
    if (! maschine.open (error))
    {
        std::printf ("Erreur : %s\n", error.c_str());
        return 1;
    }
    if (screens && ! maschine.openDisplay (error))
    {
        std::printf ("Ecrans : %s (on continue sans)\n", error.c_str());
        screens = false;
    }
    maschine.decoder().setVelocityExponent (exponent);
    std::printf ("%s ouverte. Joue avec les pads, boutons, encodeurs. Ctrl+C pour quitter.\n",
                 maschine.productName().c_str());

    std::mutex lock;
    std::vector<Event> pending;
    maschine.start ([&] (const Event& e)
    {
        std::lock_guard<std::mutex> g (lock);
        pending.push_back (e);
    }, error);

    Mk3Lights leds;
    Mk3Display display;
    display.setColorCorrection (correction);
    int group = 0;
    std::set<int> padsDown;
    std::set<std::string> held;
    std::deque<std::string> log;
    std::string stats = "...";
    const auto t0 = Clock::now();
    auto lastFrame = Clock::now();
    bool dirty = true;
    std::vector<Event> batch;
    int frames = 0;
    double sendMsTotal = 0, sendMsMax = 0;
    long bytesTotal = 0;

    while (gRunning)
    {
        std::this_thread::sleep_for (std::chrono::milliseconds (3));
        {
            std::lock_guard<std::mutex> g (lock);
            batch.swap (pending);
        }
        for (const auto& e : batch)
        {
            const double ms = std::chrono::duration<double, std::milli> (Clock::now() - t0).count();
            const std::string text = describe (e);
            std::printf ("%9.1f ms  %s\n", ms, text.c_str());
            log.push_back (text);
            if (log.size() > 12)
                log.pop_front();
            switch (e.type)
            {
                case Event::Type::PadOn:  padsDown.insert (e.id); break;
                case Event::Type::PadOff: padsDown.erase (e.id); break;
                case Event::Type::ButtonDown:
                    held.insert (e.name);
                    if (std::strncmp (e.name, "group_", 6) == 0)
                        group = e.name[6] - 'a';
                    break;
                case Event::Type::ButtonUp: held.erase (e.name); break;
                default: break;
            }
            dirty = true;
        }
        if (! batch.empty())
            std::fflush (stdout);
        batch.clear();

        if (lights)
        {
            const int hue = kDefaultGroupHues[(size_t) group];
            for (int g = 0; g < 8; ++g)
                leds.setGroup (g, kDefaultGroupHues[(size_t) g], g == group ? Shade::Bright : Shade::Dim);
            for (int p = 1; p <= 16; ++p)
                leds.setPad (p, hue, padsDown.count (p) ? Shade::Bright : Shade::Dim);
            for (const char* b : kDemoButtons)
                leds.setButton (b, held.count (b) ? Shade::Bright : Shade::Dim);
            leds.send (maschine);
        }

        // Ecrans : au plus 30 images/s, et seulement si quelque chose a change
        if (screens && dirty && Clock::now() - lastFrame >= std::chrono::milliseconds (33))
        {
            drawLeft (display.screen (0), group, padsDown);
            drawRight (display.screen (1), group, log, stats);
            const auto s0 = Clock::now();
            const long bytes = display.send (maschine);
            const double sendMs = std::chrono::duration<double, std::milli> (Clock::now() - s0).count();
            if (bytes < 0)
            {
                std::puts ("Erreur d'envoi aux ecrans : on continue sans.");
                screens = false;
            }
            else if (bytes > 0)
            {
                ++frames;
                sendMsTotal += sendMs;
                sendMsMax = std::max (sendMsMax, sendMs);
                bytesTotal += bytes;
                char buf[96];
                std::snprintf (buf, sizeof buf, "image %.1f ms (max %.1f)  %ld Ko  %d images",
                               sendMsTotal / frames, sendMsMax, bytes / 1024, frames);
                stats = buf;
            }
            lastFrame = Clock::now();
            dirty = false;
        }
    }

    if (lights)
    {
        leds.clear();
        leds.send (maschine);
    }
    if (screens)
    {
        display.screen (0).fill (kBlack);
        display.screen (1).fill (kBlack);
        display.send (maschine);
    }
    maschine.close();
    std::printf ("\nMaschine liberee. Ecrans : %d images, %.1f ms en moyenne, %ld Ko envoyes.\n",
                 frames, frames ? sendMsTotal / frames : 0.0, bytesTotal / 1024);
    return 0;
}
