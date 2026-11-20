# fuzzy-control

Bibliotheque C++ de Fuzzy Turtle Software pour piloter directement les controleurs,
en commencant par la **Maschine MK3** (et Maschine+ en mode controleur).
Elle reprend tout ce qui a ete valide avec les prototypes Python (`prototypes/maschine-hid`).

Licence : GPLv3. Protocole MK3 repris du pilote « Maschine Zynthian Prototype » r22yb3m (MIT).

## Etapes

| Etape | Contenu | Etat |
| --- | --- | --- |
| 1 | Connexion libusb, lecture des pads, boutons, encodeurs, molette 4D | en test |
| 2 | Lumieres : pads, boutons, groupes (palette NI calibree) | a faire |
| 3 | Ecrans 480 x 272 : envoi d'images, zones modifiees, correction de couleur | a faire |
| 4 | Calibration dans un fichier (boutons, lumieres) ; Maschine+ | a faire |

## Compiler et tester (Mac)

```
cd ~/Developer/fuzzy-turtle/libs/fuzzy-control
cmake -S . -B build -G Ninja
cmake --build build
./build/fuzzy-control-tests            # tests du decodage, sans Maschine
sudo ./build/fuzzy-control-probe       # avec la Maschine : affiche tout ce que tu fais
```

`sudo` est necessaire sur macOS : prendre la Maschine retire le pilote Native Instruments
(et la carte son de la Maschine) tant que le programme tourne.

## Organisation

- `include/fuzzy_control/events.h` : les evenements (pad, bouton, encodeur, molette).
- `include/fuzzy_control/decoder.h`, `src/decoder.cpp` : decodage des rapports USB, sans materiel.
- `include/fuzzy_control/maschine_mk3.h`, `src/maschine_mk3.cpp` : connexion USB et lecture
  dans une tache a part (jamais retardee par l'interface).
- `tools/probe.cpp` : outil de test sur le materiel.
- `tests/decoder_tests.cpp` : tests automatiques du decodage.
