#!/bin/bash
# Fuzzy Turtle - lance le mini DAW Pulses en une seule commande :
# interface Maschine + moteur audio (demarre automatiquement par l'interface).
# Usage : ./start.sh            (ou : ./start.sh --output "Haut-parleurs")
# Arret : Ctrl+C (arrete les deux).
cd "$(dirname "$0")/maschine-hid" || exit 1
PY="$(command -v python3)"
echo "Mot de passe Mac demande (acces aux ecrans de la Maschine)."
exec sudo "$PY" mk3_live.py "$@"
