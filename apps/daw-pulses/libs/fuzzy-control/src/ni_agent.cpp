// fuzzy-control : mise en pause / relance de l'agent Native Instruments (macOS).
#include "fuzzy_control/ni_agent.h"

#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>

namespace fuzzy::control::ni {

#ifdef __APPLE__
namespace {
constexpr const char* kAgentApp =
    "/Library/Application Support/Native Instruments/Hardware/NIHostIntegrationAgent.app";
}

bool agentRunning()
{
    return std::system ("pgrep -x NIHostIntegrationAgent >/dev/null 2>&1") == 0;
}

bool pauseAgent()
{
    if (! agentRunning())
        return false;
    if (std::system ("killall NIHostIntegrationAgent >/dev/null 2>&1") != 0)
        return false;
    for (int i = 0; i < 20 && agentRunning(); ++i)              // jusqu'a 2 s
        std::this_thread::sleep_for (std::chrono::milliseconds (100));
    std::this_thread::sleep_for (std::chrono::milliseconds (300)); // l'interface se libere
    return true;
}

void resumeAgent()
{
    const std::string cmd = std::string ("open -g \"") + kAgentApp + "\" >/dev/null 2>&1";
    if (std::system (cmd.c_str()) != 0)
        return;   // pas grave : l'agent repartira au prochain demarrage du Mac
}
#else
bool agentRunning() { return false; }
bool pauseAgent() { return false; }
void resumeAgent() {}
#endif

} // namespace fuzzy::control::ni
