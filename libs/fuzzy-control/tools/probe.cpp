// fuzzy-control-probe : affiche tout ce que fait la Maschine (pads, boutons, encodeurs,
// molette 4D, boutons inconnus) et allume ses lumieres (etape 2).
//
// Lumieres : groupes dans leurs couleurs (groupe choisi en vif) ; pads dans la couleur du
// groupe, vifs quand on les frappe ; boutons connus en doux, vifs quand on les appuie.
//
// Usage : sudo ./build/fuzzy-control-probe              (courbe de velocite "soft")
//         sudo ./build/fuzzy-control-probe --curve hard
//         sudo ./build/fuzzy-control-probe --no-lights
#include "fuzzy_control/lights.h"
#include "fuzzy_control/maschine_mk3.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

using namespace fuzzy::control;

namespace {
std::atomic<bool> gRunning { true };

// Boutons allumes en doux dans la demo (ceux qui ont un emplacement de lumiere connu)
const char* kDemoButtons[] = { "play", "rec", "stop", "shift", "pad_mode", "keyboard", "chords",
                               "step", "mute", "solo", "tempo", "volume", "note_repeat", "lock",
                               "follow", "mixer", "midi_channel", "nav_prev", "nav_next",
                               "screen_3", "screen_5", "screen_6", "screen_7", "screen_8" };
}

int main (int argc, char* argv[])
{
    double exponent = 0.5;
    bool lights = true;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp (argv[i], "--no-lights") == 0)
            lights = false;
        else if (std::strcmp (argv[i], "--curve") == 0 && i + 1 < argc)
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
    maschine.decoder().setVelocityExponent (exponent);
    std::printf ("%s ouverte. Joue avec les pads, boutons, encodeurs. Ctrl+C pour quitter.\n",
                 maschine.productName().c_str());

    // La tache de lecture met les evenements en file ; la boucle principale les affiche et
    // met a jour les lumieres (on n'ecrit jamais vers la Maschine depuis le rappel).
    std::mutex lock;
    std::vector<Event> pending;
    maschine.start ([&] (const Event& e)
    {
        std::lock_guard<std::mutex> g (lock);
        pending.push_back (e);
    }, error);

    Mk3Lights leds;
    int group = 0;
    std::set<int> padsDown;
    std::set<std::string> held;
    const auto t0 = std::chrono::steady_clock::now();
    std::vector<Event> batch;

    while (gRunning)
    {
        std::this_thread::sleep_for (std::chrono::milliseconds (5));
        {
            std::lock_guard<std::mutex> g (lock);
            batch.swap (pending);
        }
        for (const auto& e : batch)
        {
            const double ms = std::chrono::duration<double, std::milli> (
                                  std::chrono::steady_clock::now() - t0).count();
            std::printf ("%9.1f ms  %s\n", ms, describe (e).c_str());
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
        }
        if (! batch.empty())
            std::fflush (stdout);
        batch.clear();

        if (! lights)
            continue;
        const int hue = kDefaultGroupHues[(size_t) group];
        for (int g = 0; g < 8; ++g)
            leds.setGroup (g, kDefaultGroupHues[(size_t) g], g == group ? Shade::Bright : Shade::Dim);
        for (int p = 1; p <= 16; ++p)
            leds.setPad (p, hue, padsDown.count (p) ? Shade::Bright : Shade::Dim);
        for (const char* b : kDemoButtons)
            leds.setButton (b, held.count (b) ? Shade::Bright : Shade::Dim);
        leds.send (maschine);                         // n'envoie que ce qui a change
    }

    if (lights)
    {
        leds.clear();
        leds.send (maschine);                         // tout eteindre en partant
    }
    maschine.close();
    std::puts ("\nLumieres eteintes, Maschine liberee.");
    return 0;
}
