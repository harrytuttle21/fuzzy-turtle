# fuzzy-control

Bibliotheque C++ de Fuzzy Turtle Software pour piloter directement les controleurs,
en commencant par la **Maschine MK3** (et Maschine+ en mode controleur).
Elle reprend tout ce qui a ete valide avec les prototypes Python (`prototypes/maschine-hid`).

Licence : GPLv3. Protocole MK3 repris du pilote « Maschine Zynthian Prototype » r22yb3m (MIT).

## Etapes

| Etape | Contenu | Etat |
| --- | --- | --- |
| 1 | Connexion libusb, lecture des pads, boutons, encodeurs, molette 4D | valide le 20/11 |
| 2 | Lumieres : pads, boutons, groupes (palette NI calibree, LOCK = 24) | valide le 20/11 |
| 3 | Ecrans 480 x 272 : envoi d'images, zones modifiees, correction de couleur | valide le 20/11 |
| 4 | Calibration dans un fichier (boutons, lumieres) ; Maschine+ | valide sur MK3 le 20/11 (Maschine+ a tester) |

## Compiler et tester (Mac)

```
cd ~/Developer/fuzzy-turtle/libs/fuzzy-control
cmake -S . -B build -G Ninja
cmake --build build
./build/fuzzy-control-tests            # tests du decodage, sans Maschine
sudo ./build/fuzzy-control-probe       # avec la Maschine : evenements, lumieres, ecrans
# options : --no-screens  --no-lights  --no-correction  --curve soft|softer|linear|hard
#           --save-calibration  --scan-lights  --calibration FICHIER
```

`sudo` est necessaire sur macOS : prendre la Maschine retire le pilote Native Instruments
(et la carte son de la Maschine) tant que le programme tourne.

## Calibration

Les numeros des boutons et des lumieres sont dans un fichier texte, un par modele :
`~/.config/fuzzy-turtle/maschine-mk3.conf` ou `maschine-plus.conf`.

- `sudo ./build/fuzzy-control-probe --save-calibration` cree le fichier avec les valeurs actuelles.
- On le modifie avec un editeur de texte (`open -e ~/.config/fuzzy-turtle/maschine-mk3.conf`),
  sans recompiler. Une ligne absente garde la valeur par defaut ; `nom = -` desactive.
- Les boutons inconnus s'affichent dans la sonde (« BOUTON INCONNU bit N ») : il suffit
  d'ajouter `nom = N` dans `[boutons]`.
- `--scan-lights` cherche les lumieres : la molette 4D allume les emplacements un par un,
  on appuie sur le bouton qui s'allume, et c'est enregistre a la sortie (Ctrl+C).

## Organisation

- `include/fuzzy_control/calibration.h`, `src/calibration.cpp` : fichier de calibration.
- `include/fuzzy_control/events.h` : les evenements (pad, bouton, encodeur, molette).
- `include/fuzzy_control/decoder.h`, `src/decoder.cpp` : decodage des rapports USB, sans materiel.
- `include/fuzzy_control/maschine_mk3.h`, `src/maschine_mk3.cpp` : connexion USB et lecture
  dans une tache a part (jamais retardee par l'interface).
- `include/fuzzy_control/lights.h`, `src/lights.cpp` : lumieres (palette NI, emplacements
  calibres, envoi seulement de ce qui change).
- `include/fuzzy_control/display.h`, `src/display.cpp` : ecrans (dessin en memoire, texte,
  envoi des seules lignes modifiees, RGB565 avec correction du vert gamma 1,6).
- `src/font8x16.h` : police 8 x 16 integree (glyphes DejaVu Sans Mono).
- `tools/probe.cpp` : outil de test sur le materiel (evenements, lumieres, ecrans).
- `tests/decoder_tests.cpp` : tests automatiques du decodage.
