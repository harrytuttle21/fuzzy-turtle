#!/bin/bash
# Fuzzy Turtle - lance le mini DAW Pulses en une seule commande :
# interface Maschine + moteur audio (demarre automatiquement par l'interface).
# Usage : ./start.sh            (ou : ./start.sh --output "Haut-parleurs")
# Arret : Ctrl+C (arrete les deux ; l'agent NI est relance, la Maschine revient a NI).
# Plus besoin de sudo depuis le 20/11/2026 (mode partage).
cd "$(dirname "$0")/maschine-hid" || exit 1
exec python3 mk3_live.py "$@"
