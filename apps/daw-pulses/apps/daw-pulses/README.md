# Fuzzy John DAW Pulses — application Qt (premier squelette)

Licence : GPLv3. Premiere vraie fenetre de DAW Pulses sur l'ordinateur. Elle reprend :

- `libs/fuzzy-control` : la Maschine MK3 / Maschine+ (pads, lumieres, ecrans), en mode partage, sans sudo ;
- `prototypes/pad-sound` : le moteur audio JUCE, demarre automatiquement et pilote en OSC
  (ports 57390 / 57391, les memes que le prototype).

## Ce que fait ce squelette

- **16 groupes** A-H / A2-H2 (couleurs des boutons de la Maschine ; A2-H2 en ordre inverse).
- **16 pads** a l'ecran : clic = jouer (plus fort en haut du pad) ; le pad clique devient le son du clavier.
- **Clavier dessine** de 2 octaves, jouable a la souris et au **clavier de l'ordinateur**,
  avec des **notes tenues tant que la touche est appuyee** (AZERTY ou QWERTY) :
  W / X octave (QWERTY : Z / X), C / V velocite, espace = PLAY / STOP.
- **Transport** : PLAY / STOP, tempo, **metronome** avec son volume ; l'horloge tourne dans sa
  propre tache, a l'heure precise.
- **Audio** : choix de la sortie, etat du moteur (frequence, tampon, latence, decrochages).
- **Maschine** : les pads jouent (le son part directement depuis la tache des pads), groupes
  A-H (Shift = A2-H2), PLAY, STOP, TAP (metronome ; TAP tenu + molette = volume), TEMPO tenu
  + molette = tempo, molette = son choisi, fleches haut / bas = octave ; lumieres et deux
  ecrans (sons du groupe, transport). Bouton « Give back to NI » : rend la Maschine au
  logiciel NI sans quitter.

A venir : mixeur avec vumetres, sequenceur (patterns, scenes), projets `.fuzzy`, puis tout ce
qui a ete valide dans le prototype.

## Installer et lancer (Mac)

Une seule fois, Qt 6 avec Homebrew (libusb et hidapi sont deja la avec MacPorts) :

```
brew install qt
```

Compiler (le moteur audio du prototype doit etre compile aussi : `prototypes/pad-sound/build`) :

```
cd ~/Developer/fuzzy-turtle/apps/daw-pulses
cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build
```

Lancer depuis le terminal (on y voit les messages du moteur audio) :

```
"./build/Fuzzy DAW Pulses.app/Contents/MacOS/Fuzzy DAW Pulses"
```

Ou double-cliquer sur `build/Fuzzy DAW Pulses.app` dans le Finder.

Le prototype Python et l'app ne peuvent pas tourner en meme temps : ils utilisent la meme
Maschine et les memes ports. Quitte l'un avant de lancer l'autre.

Options : `--no-maschine` (ne pas ouvrir la Maschine), `--no-engine` (pas de moteur audio),
`--screenshot FICHIER` (image de la fenetre, pour les tests). Variable `FUZZY_ENGINE` : chemin
d'un autre moteur audio.

## Organisation du code

| Fichier | Role |
| --- | --- |
| `src/main_window.*` | fenetre, liaison de tous les elements |
| `src/engine_link.*` | demarre le moteur audio, lui envoie les notes, lit son etat |
| `src/maschine_link.*` | la Maschine (fuzzy-control) dans l'app Qt |
| `src/transport.*` | lecture / arret, tempo, metronome (tache a part) |
| `src/pad_grid.*`, `src/piano_keys.*` | les pads et le clavier dessines |
| `src/osc.*` | messages OSC |
| `src/session.h` | groupes, sons, couleurs, octave (partage avec la tache des pads) |
