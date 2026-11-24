// fuzzy-control-probe : outil de test de la bibliotheque sur la Maschine.
//  - affiche tout ce que fait la Maschine (pads, boutons, encodeurs, molette 4D, inconnus) ;
//  - lumieres : groupes dans leurs couleurs, pads vifs quand on les frappe, boutons connus ;
//  - ecrans : gauche = groupe et pads ; droit = derniers evenements et vitesse d'affichage ;
//  - calibration : lue dans ~/.config/fuzzy-turtle/maschine-mk3.conf (ou maschine-plus.conf).
//
// Usage : ./build/fuzzy-control-probe [options]       (plus besoin de sudo)
//   --curve soft|softer|linear|hard   courbe de velocite (sinon celle du fichier)
//   --no-lights --no-screens --no-correction
//   --keep-ni                         ne pas mettre en pause l'agent NI (macOS) : pas d'ecrans
//   --exclusive                       ancien mode (sudo, la carte son de la Maschine disparait)
//   --calibration FICHIER             autre fichier de calibration
//   --save-calibration                ecrit le fichier (valeurs actuelles), pour l'editer
//   --scan-lights                     cherche les lumieres : la molette 4D allume les
//                                     emplacements un par un ; appuie sur le bouton qui
//                                     s'allume pour l'enregistrer dans le fichier
#include "fuzzy_control/calibration.h"
#include "fuzzy_control/display.h"
#include "fuzzy_control/lights.h"
#include "fuzzy_control/maschine_mk3.h"
#include "fuzzy_control/ni_agent.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <deque>
#include <fstream>
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
                               "screen_1", "screen_2", "screen_3", "screen_4",
                               "screen_5", "screen_6", "screen_7", "screen_8" };

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

void drawScan (Canvas& c, int slot, const std::string& owner, const std::string& last)
{
    c.fill ({ 10, 10, 14 });
    c.fillRect (0, 0, Canvas::kWidth, 26, kFuzzyOrange);
    c.text (8, 5, "RECHERCHE DES LUMIERES", kBlack);
    const std::string big = "Emplacement " + std::to_string (slot);
    c.text (240 - Canvas::textWidth (big, 3) / 2, 50, big, kWhite, 3);
    c.text (8, 120, owner.empty() ? "(aucun bouton connu ici)" : "connu : " + owner, { 180, 180, 180 });
    c.text (8, 160, "Molette 4D : emplacement suivant / precedent", kWhite);
    c.text (8, 180, "Appuie sur le bouton qui s'allume pour", kWhite);
    c.text (8, 196, "l'enregistrer (Ctrl+C : quitter et sauver)", kWhite);
    if (! last.empty())
        c.text (8, 236, last, kFuzzyOrange);
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
    double exponent = -1;                     // -1 : celle du fichier de calibration
    bool lights = true, screens = true, correction = true, saveCal = false, scan = false;
    bool keepNi = false, exclusive = false, niPaused = false;
    std::string calPath;
    for (int i = 1; i < argc; ++i)
    {
        if (! std::strcmp (argv[i], "--no-lights")) lights = false;
        else if (! std::strcmp (argv[i], "--no-screens")) screens = false;
        else if (! std::strcmp (argv[i], "--no-correction")) correction = false;
        else if (! std::strcmp (argv[i], "--save-calibration")) saveCal = true;
        else if (! std::strcmp (argv[i], "--scan-lights")) scan = true;
        else if (! std::strcmp (argv[i], "--keep-ni")) keepNi = true;
        else if (! std::strcmp (argv[i], "--exclusive")) exclusive = true;
        else if (! std::strcmp (argv[i], "--calibration") && i + 1 < argc) calPath = argv[++i];
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
    if (! maschine.open (error, exclusive ? MaschineMk3::Access::Exclusive : MaschineMk3::Access::Shared))
    {
        std::printf ("Erreur : %s\n", error.c_str());
        return 1;
    }
    if (screens && ! maschine.openDisplay (error))
    {
        // macOS : l'agent NI tient les ecrans -> pause, puis relance en quittant
        if (! keepNi && ni::agentRunning())
        {
            std::puts ("Ecrans tenus par l'agent Native Instruments : mise en pause (relance en quittant).");
            niPaused = ni::pauseAgent();
            error.clear();
        }
        if (! error.empty() || ! maschine.openDisplay (error))
        {
            std::printf ("Ecrans : %s (on continue sans)\n", error.c_str());
            screens = false;
        }
    }
    // Calibration : valeurs par defaut du modele, puis le fichier s'il existe
    Calibration cal = Calibration::defaultsFor (maschine.productName());
    if (calPath.empty())
        calPath = defaultCalibrationPath (maschine.productName());
    std::vector<std::string> warnings;
    if (cal.load (calPath, warnings))
        std::printf ("Calibration : %s\n", calPath.c_str());
    else
        std::printf ("Calibration par defaut (%s n'existe pas encore ; --save-calibration le cree)\n",
                     calPath.c_str());
    for (const auto& w : warnings)
        std::printf ("  ATTENTION %s\n", w.c_str());
    if (exponent > 0)
        cal.velocityExponent = exponent;
    if (saveCal)
    {
        if (cal.save (calPath, error))
            std::printf ("Calibration enregistree dans %s (modifiable avec un editeur de texte)\n", calPath.c_str());
        else
            std::printf ("Erreur : %s\n", error.c_str());
    }
    maschine.decoder().applyCalibration (cal);
    std::printf ("%s ouverte. %s Ctrl+C pour quitter.\n", maschine.productName().c_str(),
                 scan ? "Recherche des lumieres : tourne la molette 4D." : "Joue avec les pads, boutons, encodeurs.");

    std::mutex lock;
    std::vector<Event> pending;
    maschine.start ([&] (const Event& e)
    {
        std::lock_guard<std::mutex> g (lock);
        pending.push_back (e);
    }, error);

    Mk3Lights leds (cal);
    int scanSlot = 0;
    std::string scanLast;
    bool scanChanged = false;
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
                case Event::Type::Jog:
                    if (scan)
                        scanSlot = (scanSlot + e.value + 62) % 62;
                    break;
                case Event::Type::ButtonDown:
                    if (scan && std::strncmp (e.name, "group_", 6) == 0)
                    {
                        // groupe X allume : les 8 groupes commencent X emplacements plus tot
                        cal.groupSlot = std::max (0, scanSlot - (e.name[6] - 'a'));
                        leds.applyCalibration (cal);
                        scanChanged = true;
                        scanLast = "groupes a partir de " + std::to_string (cal.groupSlot) + " enregistre";
                        std::printf ("  -> %s\n", scanLast.c_str());
                    }
                    else if (scan)
                    {
                        const auto old = cal.lights.find (e.name);
                        const bool screenButton = old != cal.lights.end() ? old->second.screenButton
                                                                          : std::strncmp (e.name, "screen_", 7) == 0;
                        cal.lights[e.name] = { scanSlot, screenButton };
                        leds.applyCalibration (cal);
                        scanChanged = true;
                        scanLast = std::string (e.name) + " = " + std::to_string (scanSlot) + " enregistre";
                        std::printf ("  -> lumiere %s\n", scanLast.c_str());
                    }
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

        if (lights && scan)
        {
            bool screenSlot = false;
            for (const auto& [name, l] : cal.lights)
                screenSlot = screenSlot || (l.slot == scanSlot && l.screenButton);
            const bool groupSlot = scanSlot >= cal.groupSlot && scanSlot < cal.groupSlot + 8;
            for (int sl = 0; sl < 62; ++sl)       // seulement l'emplacement cherche
                leds.setSlot (sl, sl != scanSlot ? 0
                                  : groupSlot ? paletteColor (kWhiteHue, Shade::Bright)
                                  : screenSlot ? cal.screenActive : cal.buttonBright);
            for (int p = 1; p <= 16; ++p)
                leds.setPad (p, (uint8_t) 0);
            leds.send (maschine);
        }
        else if (lights)
        {
            const int hue = cal.groupHues[(size_t) group];
            for (int g = 0; g < 8; ++g)
                leds.setGroup (g, g == group ? Shade::Bright : Shade::Dim);
            for (int p = 1; p <= 16; ++p)
                leds.setPad (p, hue, padsDown.count (p) ? Shade::Bright : Shade::Dim);
            for (const char* b : kDemoButtons)
                leds.setButton (b, held.count (b) ? Shade::Bright : Shade::Dim);
            leds.send (maschine);
        }

        // Ecrans : au plus 30 images/s, et seulement si quelque chose a change
        if (screens && dirty && Clock::now() - lastFrame >= std::chrono::milliseconds (33))
        {
            if (scan)
            {
                std::string owner;
                for (const auto& [name, l] : cal.lights)
                    if (l.slot == scanSlot)
                        owner += (owner.empty() ? "" : ", ") + name;
                if (scanSlot >= cal.groupSlot && scanSlot < cal.groupSlot + 8)
                    owner = std::string ("groupe ") + (char) ('A' + scanSlot - cal.groupSlot);
                drawScan (display.screen (0), scanSlot, owner, scanLast);
            }
            else
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
    if (niPaused)
    {
        ni::resumeAgent();
        std::puts ("Agent Native Instruments relance : la Maschine revient au logiciel NI.");
    }
    if (scanChanged)
    {
        if (cal.save (calPath, error))
            std::printf ("\nLumieres trouvees enregistrees dans %s\n", calPath.c_str());
        else
            std::printf ("\nErreur : %s\n", error.c_str());
    }
    std::printf ("\nMaschine liberee. Ecrans : %d images, %.1f ms en moyenne, %ld Ko envoyes.\n",
                 frames, frames ? sendMsTotal / frames : 0.0, bytesTotal / 1024);
    return 0;
}
