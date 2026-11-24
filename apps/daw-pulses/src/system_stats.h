// Fuzzy John DAW Pulses — charge de l'ordinateur : processeur, memoire, memoire de l'app.
// macOS : appels Mach (host_statistics, task_info) ; Linux : /proc. Licence : GPLv3.
#pragma once
#include <cstdint>

class SystemStats
{
public:
    // A appeler regulierement (ex. toutes les secondes) : le CPU se mesure entre deux appels.
    void update();

    double cpuPercent() const { return cpu; }            // tout l'ordinateur, 0-100
    double memUsedGB() const { return memUsed; }
    double memTotalGB() const { return memTotal; }
    double appMB() const { return app; }                  // memoire de cette application
    bool valid() const { return memTotal > 0; }

private:
    uint64_t lastBusy = 0, lastTotal = 0;
    double cpu = 0, memUsed = 0, memTotal = 0, app = 0;
};
