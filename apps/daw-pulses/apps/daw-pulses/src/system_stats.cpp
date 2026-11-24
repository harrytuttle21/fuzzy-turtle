// Fuzzy John DAW Pulses — charge de l'ordinateur. Licence : GPLv3.
#include "system_stats.h"

#if defined(__APPLE__)
  #include <mach/mach.h>
  #include <mach/mach_host.h>
  #include <sys/sysctl.h>
#else
  #include <fstream>
  #include <sstream>
  #include <string>
  #include <unistd.h>
#endif

void SystemStats::update()
{
    uint64_t busy = 0, total = 0;
#if defined(__APPLE__)
    static const mach_port_t host = mach_host_self();
    // Processeur : temps passe par etat depuis le demarrage, compare au tour precedent
    host_cpu_load_info_data_t load;
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    if (host_statistics (host, HOST_CPU_LOAD_INFO, reinterpret_cast<host_info_t> (&load), &count) == KERN_SUCCESS)
    {
        busy = (uint64_t) load.cpu_ticks[CPU_STATE_USER] + load.cpu_ticks[CPU_STATE_SYSTEM] + load.cpu_ticks[CPU_STATE_NICE];
        total = busy + load.cpu_ticks[CPU_STATE_IDLE];
    }
    // Memoire utilisee : pages actives + reservees + compressees
    vm_statistics64_data_t vm;
    count = HOST_VM_INFO64_COUNT;
    vm_size_t page = 4096;
    host_page_size (host, &page);
    if (host_statistics64 (host, HOST_VM_INFO64, reinterpret_cast<host_info64_t> (&vm), &count) == KERN_SUCCESS)
        memUsed = double ((uint64_t) vm.active_count + vm.wire_count + vm.compressor_page_count) * page / (1 << 30);
    int64_t bytes = 0;
    size_t len = sizeof (bytes);
    if (sysctlbyname ("hw.memsize", &bytes, &len, nullptr, 0) == 0)
        memTotal = double (bytes) / (1 << 30);
    // Memoire de l'application
    mach_task_basic_info_data_t task;
    count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info (mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t> (&task), &count) == KERN_SUCCESS)
        app = double (task.resident_size) / (1 << 20);
#else
    {
        std::ifstream stat ("/proc/stat");
        std::string cpuLabel;
        uint64_t user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq = 0, softirq = 0;
        stat >> cpuLabel >> user >> nice >> system >> idle >> iowait >> irq >> softirq;
        busy = user + nice + system + irq + softirq;
        total = busy + idle + iowait;
    }
    {
        std::ifstream info ("/proc/meminfo");
        std::string key, unit;
        uint64_t value = 0, memTotalKb = 0, availableKb = 0;
        while (info >> key >> value >> unit)
        {
            if (key == "MemTotal:") memTotalKb = value;
            if (key == "MemAvailable:") availableKb = value;
        }
        memTotal = double (memTotalKb) / (1 << 20);
        memUsed = double (memTotalKb - availableKb) / (1 << 20);
    }
    {
        std::ifstream statm ("/proc/self/statm");
        uint64_t size = 0, resident = 0;
        statm >> size >> resident;
        app = double (resident) * (double) sysconf (_SC_PAGESIZE) / (1 << 20);
    }
#endif
    if (lastTotal > 0 && total > lastTotal)
        cpu = 100.0 * double (busy - lastBusy) / double (total - lastTotal);
    lastBusy = busy;
    lastTotal = total;
}
