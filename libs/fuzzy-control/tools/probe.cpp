// fuzzy-control-probe : affiche tout ce que fait la Maschine (pads, boutons, encodeurs,
// molette 4D, boutons inconnus). Equivalent C++ du prototype mk3_probe.py.
//
// Usage : sudo ./build/fuzzy-control-probe            (courbe de velocite "soft")
//         sudo ./build/fuzzy-control-probe --curve hard
#include "fuzzy_control/maschine_mk3.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>

using namespace fuzzy::control;

namespace {
std::atomic<bool> gRunning { true };
}

int main (int argc, char* argv[])
{
    double exponent = 0.5;
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp (argv[i], "--curve") == 0)
        {
            const char* c = argv[i + 1];
            exponent = ! std::strcmp (c, "linear") ? 1.0 : ! std::strcmp (c, "softer") ? 0.35
                     : ! std::strcmp (c, "hard") ? 1.6 : 0.5;
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

    std::mutex printLock;
    const auto t0 = std::chrono::steady_clock::now();
    maschine.start ([&] (const Event& e)
    {
        const double ms = std::chrono::duration<double, std::milli> (
                              std::chrono::steady_clock::now() - t0).count();
        std::lock_guard<std::mutex> lock (printLock);
        std::printf ("%9.1f ms  %s\n", ms, describe (e).c_str());
        std::fflush (stdout);
    }, error);

    while (gRunning)
        std::this_thread::sleep_for (std::chrono::milliseconds (50));

    maschine.close();
    std::puts ("\nMaschine liberee.");
    return 0;
}
