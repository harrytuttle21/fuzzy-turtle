// fuzzy-control : l'agent Native Instruments sur macOS.
//
// NIHostIntegrationAgent (programme NI en arriere-plan) tient l'interface des ecrans de la
// Maschine. Pour afficher nos propres ecrans SANS sudo, on le met en pause, puis on le relance
// en partant : la Maschine revient au logiciel NI sans la debrancher.
// Sur Linux (pas d'agent NI), ces fonctions ne font rien.
#pragma once

namespace fuzzy::control::ni {

bool agentRunning();
bool pauseAgent();     // true si l'agent tournait et a ete arrete
void resumeAgent();    // le relance (a appeler en quittant si pauseAgent() a renvoye true)

} // namespace fuzzy::control::ni
