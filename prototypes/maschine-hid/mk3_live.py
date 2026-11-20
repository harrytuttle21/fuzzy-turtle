#!/usr/bin/env python3
"""Fuzzy Turtle - prototype 3 : pads + encodeurs + ecrans ensemble (mini DAW Pulses).

Modes, comme sur Maschine :
  PAD MODE      chaque pad joue un son du groupe (mode par defaut)
                ecran gauche : les 16 sons et 4 reglages ; ecran droit : la piste du son choisi
  Bouton 3      (au-dessus de l'ecran gauche, tous les modes sauf MIXER) : vue TRACKER 16 pistes
                sur les deux ecrans ; un 2e appui revient a la vue precedente
  TEMPO         taper plusieurs fois en rythme = tap tempo ; TEMPO tenu + molette 4D = +/- 1 BPM
  Molette 4D    tourner : ligne du curseur ; pousser haut/bas : ligne ; gauche/droite : piste
  Encodeur 8    longueur du pattern (1 a 64 lignes), dans tous les modes
  CHORDS        encodeur 5 gamme (liste), 6 mode HARM / SET / USER, 7 choix du mode
                (HARM : 1-3 ... 1-3-5-7-9, Oct +1/+2/-1/-2 ; SET : Maj 1-8, Min 1-8 ;
                USER : User 1-4), 8 position (renversement) ; Shift + pads 13-16 : root / octave.
                USER : frapper un pad le choisit ; bouton 8 = EDIT (les pads = 16 demi-tons a
                allumer / eteindre) ; bouton 8 a nouveau = sauvegarde.
                En REC, l'accord entier s'ecrit dans une case du tracker.
  MIXER         8 voies : sons du groupe (Pad Mode) ou groupes ; encodeurs 1-8 = volumes ;
                molette 4D = volume master ; 4D gauche / droite = page 1 / 2.
  VOLUME        tenu + molette 4D = volume master
  Bouton 7      (Keyboard / Chords) LINK : chaque groupe a sa tonalite (root, octave, gamme) ;
                bouton 7 puis boutons de groupe = lier des groupes (ils partagent la tonalite) ;
                Shift + 7 = tous les groupes / aucun ; bouton 7 = termine
  Shift + MIDI  SETTINGS > COLORS : encodeur 1 cible GROUP / PADS ; GROUP : pad n = couleur n ;
                PADS : choisir des pads puis encodeur 2 = leur couleur, Shift + pad = couleur du
                groupe ; Shift + MIDI a nouveau = fermer : bouton 5 YES (sauvegarde dans
                fuzzy_settings.json) / bouton 6 NO (retour aux couleurs d'avant)
  FOLLOW        l'affichage suit (ou non) la lecture
  NOTE REPEAT   un appui : marche (menu sur l'ecran gauche), un 2e appui : arret ;
  LOCK          garde Note Repeat / Arp en marche et ferme le menu : on navigue librement
                (Tracker, ecran gauche...) ; NOTE REPEAT rouvre le menu pour modifier ;
                LOCK a nouveau (menu ferme) = arret
                encodeurs 1 vitesse (1/4 a 1/32, triolets T), 2 mode REPEAT / ARP,
                3 sens de l'arpege (Up, Down, Up&Down, Down&Up, Random), 4 octaves (-3 a +3)
                Shift + CHORDS : affichage clavier (nom de l'accord + notes allumees).
  KEYBOARD      les 16 pads jouent la gamme avec le son choisi ; ecran droit : clavier
                gammes de Maschine en 8 banques : Main, Major, Minor, Modes, Jazz, World,
                5-Tone, Modern (15 gammes chacune)

Commandes :
  Rec           enregistrement : un pad ecrit sa note sur SA piste ; plusieurs pads frappes
                ensemble remplissent la meme ligne ; le curseur avance quand on relache tout.
                Pendant la lecture, la note s'ecrit a la ligne en train de jouer.
  Shift + pad   effacer la case de la piste a la ligne du curseur
  STEP          (Pad Mode, Keyboard, Tracker) pas a pas : les 16 pads = 16 pas de la piste choisie (pad 13 = pas 1) ;
                avec REC, frapper un pad ajoute / enleve la note (sans REC : on ecoute) ; page = celle du curseur (16 lignes),
                ou celle qui joue pendant Play (les pages defilent) ; boutons < > : changer de page ;
                Shift + pad = choisir la piste. Pads : note = vif, vide = eteint, lecture = blanc.
  Lecture       l'affichage suit la ligne qui joue
  Play / Stop   lecture des patterns (un par groupe, tous les groupes ensemble)
  MUTE + groupe couper / reactiver un groupe ; SOLO + groupe(s) : n'ecouter que ceux-la
  Shift + MUTE  panique : tous les sons se coupent
  Shift + pad 3 / 4   Step Undo / Step Redo (100 etapes)
  Groupes A-H   changer de groupe
  Encodeurs     Pad Mode : 1-4 reglages du son, 5 ligne, 6 note, 7 volume, 8 longueur
                Tracker  : 1 ligne, 2 piste, 3 note, 4 volume, 8 longueur
                Keyboard : 5 gamme (liste par categories Main, Major, Minor... ; la gamme en
                           surbrillance s'applique apres 1,5 s sans mouvement ou au clic 4D),
                           6 root, 8 octave ;
                           Shift + pad 13 / 14 : root -/+ ; Shift + pad 15 / 16 : octave -/+

Son : le moteur C++ (prototypes/pad-sound) est demarre automatiquement apres la prise de la
      Maschine, sur une sortie autre que la Maschine (--output "nom" pour l'imposer,
      --no-sound pour s'en passer). Lancement en une commande : prototypes/start.sh
      Ancienne methode, moteur a la main dans un autre terminal :
         prototypes/pad-sound/build/fuzzy_pad_sound_artefacts/Release/fuzzy_pad_sound --osc
      mk3_live.py lui envoie chaque note par OSC (port 57390). Sans moteur : interface muette.

Usage :  sudo "$(which python3)" mk3_live.py
         python3 mk3_live.py --preview     (sans Maschine : enregistre des PNG)
A la fin : debrancher / rebrancher la Maschine pour rendre les ecrans au pilote NI.
Necessite mk3_probe.py et mk3_screens.py dans le meme dossier (hidapi n'est pas utilise ici).
"""
from __future__ import annotations
import argparse, json, os, queue, random, signal, socket, struct, subprocess, sys, threading, time
from pathlib import Path

from PIL import Image, ImageDraw

from mk3_probe import Decoder, CURVES, pad_led_frame
from maschine_chord_sets import MACHINE_CHORD_SET_SOURCES   # 16 Chord Sets (pilote r22yb3m, MIT)
from mk3_screens import Screens, Preview, MK3Error, W, H, font, header

ROWS, VISIBLE = 64, 11          # ROWS = longueur maximale d'un pattern
NOTES = ['C-', 'C#', 'D-', 'D#', 'E-', 'F-', 'F#', 'G-', 'G#', 'A-', 'A#', 'B-']
DRUMS = ['Kick', 'Snare', 'Closed HH', 'Open HH']        # sons 1-4 du moteur JUCE ; 5-16 = notes
OSC_PORT = 57390                                      # fuzzy_pad_sound --osc


def default_note(pad: int) -> int:
    """Hauteur par defaut d'un son, comme dans le moteur C++ (C mineur pentatonique des C3)."""
    if pad <= 4:
        return 60
    scale = (0, 3, 5, 7, 10)
    deg = pad - 5
    return 48 + scale[deg % 5] + 12 * (deg // 5)


class Sound:
    """Envoie les notes au moteur audio fuzzy_pad_sound par OSC (UDP local).
    Si le moteur n'est pas lance, les messages sont simplement perdus : pas d'erreur."""

    def __init__(self, port: int = OSC_PORT):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.addr = ('127.0.0.1', port)

    @staticmethod
    def _pad4(b: bytes) -> bytes:
        return b + b'\0' * (4 - len(b) % 4)

    def hit(self, sound: int, velocity: int, note: int) -> None:
        msg = (self._pad4(b'/fuzzy/proto/hit') + self._pad4(b',iii')
               + struct.pack('>iii', sound, velocity, note))
        try:
            self.sock.sendto(msg, self.addr)
        except OSError:
            pass


    def panic(self) -> None:
        """Coupe tous les sons du moteur (Shift + MUTE)."""
        try:
            self.sock.sendto(self._pad4(b'/fuzzy/proto/panic') + self._pad4(b','), self.addr)
        except OSError:
            pass


SOUND = Sound()
PARAMS = ['Volume', 'Pan', 'Pitch', 'Cutoff']
GROUPS = 'ABCDEFGH'
# Gammes rangees en banques, comme sur Maschine (formules de degres du manuel MK3).
_BANKS = {
    'Main': [('Chromatic', '1 b2 2 b3 3 4 b5 5 b6 6 b7 7'), ('Major', '1 2 3 4 5 6 7'),
             ('Minor', '1 2 b3 4 5 b6 b7'), ('Harm Min', '1 2 b3 4 5 b6 7'),
             ('Maj Pent', '1 2 3 5 6'), ('Min Pent', '1 b3 4 5 b7'), ('Blues', '1 b3 4 #4 5 b7'),
             ('Japanese', '1 2 b3 5 b6'), ('Freygish', '1 b2 3 4 5 b6 b7'),
             ('Gypsy', '1 2 b3 #4 5 b6 7'), ('Arabic', '1 b2 3 4 5 b6 7'),
             ('Altered', '1 b2 #2 3 #4 b6 b7'), ('Whole Tone', '1 2 3 #4 #5 b7'),
             ('H-W Dim', '1 b2 #2 3 #4 5 6 b7'), ('W-H Dim', '1 2 b3 4 #4 #5 6 7')],
    'Major': [('Natural', '1 2 3 4 5 6 7'), ('Lydian', '1 2 3 #4 5 6 7'),
              ('Mixolydian', '1 2 3 4 5 6 b7'), ('Major Minor', '1 2 3 4 5 b6 b7'),
              ('Harmonic Major', '1 2 3 4 5 b6 7'), ('Dbl Har Major', '1 b2 3 4 5 b6 7'),
              ('Neapolitan Maj', '1 b2 3 4 5 6 7'), ('Major Locrian', '1 2 3 4 b5 b6 b7'),
              ('Blues Major', '1 2 b3 3 5 6'), ('Bebop Major', '1 2 3 4 5 #5 6 7'),
              ('Hexa 1', '1 2 3 5 6 7'), ('Hexa 2', '1 2 3 4 5 6'), ('Penta 1', '1 2 3 5 6'),
              ('Penta 2', '1 3 4 5 7'), ('Penta 3', '1 3 5 6 7')],
    'Minor': [('Natural', '1 2 b3 4 5 b6 b7'), ('Dorian', '1 2 b3 4 5 6 b7'),
              ('Phrygian', '1 b2 b3 4 5 b6 b7'), ('Minor Major', '1 2 b3 4 5 6 7'),
              ('Harmonic Minor', '1 2 b3 4 5 b6 7'), ('Dbl Har Minor', '1 2 b3 #4 5 b6 7'),
              ('Neapolitan Min', '1 b2 b3 4 5 b6 7'), ('Minor Locrian', '1 2 b3 4 b5 b6 b7'),
              ('Blues Min', '1 b3 4 #4 5 b7'), ('Bebop Minor', '1 2 b3 4 5 b6 b7 7'),
              ('Hexa 1', '1 2 b3 5 b6 b7'), ('Hexa 2', '1 2 b3 4 5 b6'), ('Penta 1', '1 2 b3 5 b6'),
              ('Penta 2', '1 b3 4 5 b7'), ('Penta 3', '1 b3 5 b6 b7')],
    'Modes': [('Ionian', '1 2 3 4 5 6 7'), ('Dorian', '1 2 b3 4 5 6 b7'),
              ('Phrygian', '1 b2 b3 4 5 b6 b7'), ('Lydian', '1 2 3 #4 5 6 7'),
              ('Mixolydian', '1 2 3 4 5 6 b7'), ('Aeolian', '1 2 b3 4 5 b6 b7'),
              ('Locrian', '1 b2 b3 4 b5 b6 b7'), ('Ionian b2', '1 b2 3 4 5 6 7'),
              ('Dorian b5', '1 2 b3 4 b5 6 b7'), ('Harm Phryg', '1 b2 b3 4 5 b6 7'),
              ('Phryg Major', '1 b2 b3 4 5 6 7'), ('Lydian b3', '1 2 b3 #4 5 6 7'),
              ('Major Locrian', '1 2 3 4 b5 b6 b7'), ('Minor Locrian', '1 2 b3 4 b5 b6 b7'),
              ('Super Locrian', '1 b2 b3 b4 b5 b6 b7')],
    'Jazz': [('Lydian b7', '1 2 3 #4 5 6 b7'), ('Altered', '1 b2 #2 3 #4 b6 b7'),
             ('Diminished', '1 b2 #2 3 #4 5 6 b7'), ('Mixo b13', '1 2 3 4 5 b6 b7'),
             ('Mixo b9 b13', '1 b2 3 4 5 b6 b7'), ('Lydian b7 b2', '1 b2 3 #4 5 6 b7'),
             ('Bebop', '1 2 3 4 5 6 b7 7'), ('Whole Tone', '1 2 3 #4 #5 b7'),
             ('Blues Maj', '1 2 b3 3 5 6'), ('Blues Min', '1 b3 4 #4 5 b7'),
             ('Blues Combined', '1 2 b3 3 4 #4 5 6 b7'), ('Lydian #5', '1 2 3 #4 #5 6 7'),
             ('Jazz Minor', '1 2 b3 4 5 6 7'), ('Half Dim', '1 2 b3 4 b5 b6 b7'),
             ('Augmented', '1 b3 3 5 #5 7')],
    'World': [('Hungarian Min', '1 2 b3 #4 5 b6 7'), ('Hungarian Maj', '1 #2 3 #4 5 6 b7'),
              ('Neapolitan', '1 b2 b3 4 5 b6 7'), ('Spanish', '1 b2 b3 3 4 5 b6 b7'),
              ('Greek', '1 2 b3 b4 5 b6 b7'), ('Jewish 1', '1 b2 3 4 5 b6 b7'),
              ('Jewish 2', '1 2 b3 #4 5 6 b7'), ('Indian 1', '1 b2 b3 #4 5 b6 7'),
              ('Indian 2', '1 2 b3 #4 5 6 7'), ('Indian 3', '1 b2 2 4 5 b6 6'),
              ('Indian 4', '1 #2 3 4 5 #6 7'), ('Mid East 1', '1 b2 3 4 5 b6 7'),
              ('Mid East 2', '1 b2 3 4 b5 b6 7'), ('Mid East 3', '1 b2 b3 4 b5 6 b7'),
              ('Mid East 4', '1 b2 3 4 b5 6 b7')],
    '5-Tone': [('Penta I', '1 2 3 5 6'), ('Penta II', '1 2 4 5 b7'), ('Penta III', '1 b3 4 b6 b7'),
               ('Penta IV', '1 2 4 5 6'), ('Penta V', '1 b3 4 5 b7'), ('Hirajoshi', '1 2 b3 5 b6'),
               ('Insen', '1 b2 4 5 b7'), ('Kokin Joshi', '1 2 4 5 b6'), ('Akebono', '1 2 b3 5 6'),
               ('Ryukuan', '1 3 4 5 7'), ('Abhogi', '1 2 b3 4 6'), ('Bhupkali', '1 2 3 5 b6'),
               ('Hindolam', '1 b3 4 b6 b7'), ('Bhupalam', '1 b2 b3 5 b6'),
               ('Amritavarshini', '1 3 #4 5 7')],
    'Modern': [('Octatonic', '1 2 b3 4 #4 #5 6 7'), ('Acoustic', '1 2 3 #4 5 6 b7'),
               ('Augmented', '1 b3 3 5 #5 7'), ('Tritone', '1 b2 3 b5 5 b7'),
               ('Leading Wh Tone', '1 2 3 #4 #5 #6 7'), ('Enigmatic', '1 b2 3 #4 #5 #6 7'),
               ('Scriabin', '1 2 3 #4 6 b7'), ('Tcherepnin', '1 #1 #2 3 4 5 #5 6 7'),
               ('Messiaen I', '1 2 3 #4 #5 #6'), ('Messiaen II', '1 b2 #2 3 #4 5 6 b7'),
               ('Messiaen III', '1 2 b3 3 #4 5 b6 b7 7'), ('Messiaen IV', '1 b2 2 4 #4 5 b6 7'),
               ('Messiaen V', '1 b2 4 #4 5 7'), ('Messiaen VI', '1 2 3 4 #4 #5 #6 7'),
               ('Messiaen VII', '1 b2 2 b3 4 #4 5 b6 6 7')],
}
# Chord Sets : 8 majeurs + 8 mineurs, 12 accords chacun (pads 1 a 12)
CHORD_SETS = ([(f'Maj {i + 1}', s) for i, s in enumerate(MACHINE_CHORD_SET_SOURCES['MAJ'])]
              + [(f'Min {i + 1}', s) for i, s in enumerate(MACHINE_CHORD_SET_SOURCES['MIN'])])
# Mode CHORDS > Harmonizer : intervalles empiles en degres de la gamme (0 = note jouee)
HARM_TYPES = [('1-3', (0, 2)), ('1-5', (0, 4)), ('1-3-5', (0, 2, 4)), ('1-4-5', (0, 3, 4)),
              ('1-3-5-7', (0, 2, 4, 6)), ('1-4-5-7', (0, 3, 4, 6)), ('1-3-5-7-9', (0, 2, 4, 6, 8)),
              ('Oct +1', 12), ('Oct +2', 24), ('Oct -1', -12), ('Oct -2', -24)]
CHORD_MODES = ('HARM', 'SET', 'USER')
USER_BANKS = 10                                        # User 1-10, 12 accords chacun
USER_FILE = Path(__file__).with_name('user_chords.json')
SCALE_DELAY = 1.5                                      # liste des gammes : choix applique apres
                                                       # 1,5 s sans mouvement
_DEGREE = {'1': 0, '#1': 1, 'b2': 1, '2': 2, '#2': 3, 'b3': 3, '3': 4, 'b4': 4, '4': 5,
           '#4': 6, 'b5': 6, '5': 7, '#5': 8, 'b6': 8, '6': 9, '#6': 10, 'b7': 10, '7': 11}
BANKS = [(bank, [(name, tuple(_DEGREE[d] for d in formula.split())) for name, formula in scales])
         for bank, scales in _BANKS.items()]
# Liste unique pour l'encodeur 5 : toutes les gammes, banque apres banque
SCALE_ITEMS = [(b, t) for b, (_, scales) in enumerate(BANKS) for t in range(len(scales))]
CHORDS = [((0, 4, 7, 11), 'maj7'), ((0, 3, 7, 10), 'm7'), ((0, 4, 7, 10), '7'),
          ((0, 3, 6, 10), 'm7b5'), ((0, 4, 7), ''), ((0, 3, 7), 'm'), ((0, 3, 6), 'dim'),
          ((0, 4, 8), 'aug'), ((0, 2, 7), 'sus2'), ((0, 5, 7), 'sus4'), ((0, 7), '5')]
KEY_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']


def invert(notes, position: int) -> tuple:
    """Position (renversement) : +1 = la note la plus grave monte d'une octave, -1 = la plus
    aigue descend d'une octave ; pour varier les voicings."""
    notes = sorted(notes)
    for _ in range(abs(position)):
        if not notes:
            break
        if position > 0:
            notes.append(notes.pop(0) + 12)
        else:
            notes.insert(0, notes.pop() - 12)
        notes.sort()
    return tuple(n for n in notes if 0 <= n <= 127)


def load_user_chords() -> list:
    """Accords USER sauvegardes (user_chords.json, a cote du script)."""
    try:
        data = json.loads(USER_FILE.read_text())
        banks = [[tuple(c) for c in bank][:12] for bank in data['banks']][:USER_BANKS]
        while len(banks) < USER_BANKS:
            banks.append([()] * 12)
        return [bank + [()] * (12 - len(bank)) for bank in banks]
    except (OSError, ValueError, KeyError, TypeError):
        return [[()] * 12 for _ in range(USER_BANKS)]


def load_settings(st) -> None:
    try:
        data = json.loads(SETTINGS_FILE.read_text())
        st.group_hue.update({g: int(h) for g, h in data.get('group_hue', {}).items() if g in GROUPS})
        for g, k in data.get('keys', {}).items():
            if g in GROUPS:
                st.keys[g].update({n: int(v) for n, v in k.items() if n in st.keys[g]})
        st.linked = set(data.get('linked', [])) & set(GROUPS)
        for key, h in data.get('pad_hue', {}).items():
            g, p = key.split(':')
            st.pad_hue[(g, int(p))] = int(h)
    except (OSError, ValueError, KeyError, TypeError, AttributeError):
        pass


def save_settings(st) -> None:
    try:
        SETTINGS_FILE.write_text(json.dumps({
            'group_hue': st.group_hue,
            'pad_hue': {f'{g}:{p}': h for (g, p), h in st.pad_hue.items() if h is not None},
            'keys': st.keys, 'linked': sorted(st.linked)},
            indent=1))
    except OSError as e:
        print('Sauvegarde des reglages impossible :', e)


def save_user_chords(st) -> None:
    try:
        USER_FILE.write_text(json.dumps({'banks': [[list(c) for c in b] for b in st.user_chords]},
                                        indent=1))
    except OSError as e:
        print('Sauvegarde des accords USER impossible :', e)


def note_name(n: int) -> str:
    return f'{NOTES[n % 12]}{n // 12 - 1}'


def cell_notes(note) -> tuple:
    """Une case du tracker contient une note, ou un accord (plusieurs notes)."""
    return note if isinstance(note, tuple) else (note,)


def cell_label(note) -> str:
    if not isinstance(note, tuple):
        return note_name(note)
    return (chord_name(note) or note_name(min(note)) + '+')[:4].ljust(3)


class Knob:
    """Les encodeurs envoient de petites valeurs (3 a 100) : on les cumule en pas."""

    def __init__(self, per_step: int):
        self.per_step, self.acc = per_step, 0

    def steps(self, delta: int) -> int:
        self.acc += delta
        n = int(self.acc / self.per_step)
        self.acc -= n * self.per_step
        return n


class State:
    def __init__(self):
        self.group = 'A'
        self.sound = 1
        self.params = {(g, s): [100, 64, 64, 90] for g in GROUPS for s in range(1, 17)}
        # Pattern : 32 lignes x 16 pistes (une par son du groupe) ; case = (note, volume)
        # Un pattern par groupe : 64 lignes x 16 pistes ; case = (note ou accord, volume)
        self.grids = {g: [[None] * 16 for _ in range(ROWS)] for g in GROUPS}
        demo = self.grids['A']
        for r in (0, 8, 10):
            demo[r][0] = (60, 0x7F)                            # Kick
        for r in (4, 12):
            demo[r][1] = (60, 0x70)                            # Snare
        for r in (2, 6, 14):
            demo[r][2] = (60, 0x50)                            # Closed HH
        self.muted: set[str] = set()                           # MUTE + groupe
        self.solo: set[str] = set()                            # SOLO + groupe(s)
        self.undo_stack: list = []                             # Shift + pad 3 / 4
        self.redo_stack: list = []
        self.last_undo_push = 0.0
        self.pending_scale: tuple[int, int] | None = None      # gamme choisie, pas encore appliquee
        self.chord_keyboard = False                            # Shift + CHORDS : clavier
        self.last_chord_pad = 0
        self.scale_deadline = 0.0                              # liste deroulante des gammes
        self.chord_mode = 'SET'                                # HARM / SET / USER (bouton 7)
        self.harm_type = 2                                     # 1-3-5
        self.position = 0                                      # renversement (encodeur 8)
        self.user_bank = 0
        self.user_chords = load_user_chords()                  # [banque][pad 0-11] = intervalles
        self.user_edit = False                                 # Shift + bouton 7 en USER
        self.user_slot = 1
        # Mixeur (bouton MIXER) : volumes 0-127, 100 = niveau normal
        self.mixer = False
        self.mixer_page = 0
        self.master = 100
        self.group_vol = {g: 100 for g in tuple(GROUPS) + tuple(f'{g}2' for g in GROUPS)}
        self.sound_vol = {(g, s): 100 for g in GROUPS for s in range(1, 17)}
        self.mix_knobs = [Knob(4) for _ in range(8)]
        # Settings (Shift + MIDI) : couleurs des groupes et des pads
        self.group_hue = dict(zip(GROUPS, GROUP_HUES))
        self.pad_hue: dict[tuple[str, int], int | None] = {}
        self.settings = False
        self.set_target = 'GROUP'                    # GROUP ou PADS (encodeur 1)
        self.set_selected: set[int] = set()          # pads choisis (cible PADS)
        self.set_knobs = {0: Knob(40), 1: Knob(30)}
        # Follow : l'affichage suit la lecture (bouton FOLLOW)
        self.follow = True
        # Note Repeat (tenu) et arpegiateur (Shift + Note Repeat)
        self.nr_mode = 'REPEAT'                      # REPEAT ou ARP (encodeur 2 du menu)
        self.nr_on = False                           # Note Repeat / Arp en marche
        self.nr_menu = False                         # menu visible sur l'ecran gauche
        self.nr_lock = False                         # LOCK : la fonction reste active, menu ferme
        self.nr_rate = 4                             # 1/16
        self.arp_dir = 0                             # Up
        self.arp_oct = 0                             # octaves ajoutees : -3 a +3 (0 = aucune)
        self.nr_knobs = {0: Knob(30), 1: Knob(40), 2: Knob(35), 3: Knob(40)}
        self.set_confirm = False                     # Settings : sauvegarder ? oui / non
        self.set_backup = None
        self.held_play: dict[int, tuple[float, int, list]] = {}   # pad -> (frappe, velo, notes)
        self.nr_next = 0.0
        self.arp_index = 0
        self.tracker16 = False                         # vue 16 pistes (bouton 6)
        self.length = 32                               # longueur du pattern (lignes), encodeur 8
        self.taps: list[float] = []                    # tap tempo
        self.step_mode = False                         # bouton STEP : pads = 16 pas de la piste
        self.step_manual: int | None = None            # page choisie avec < > (sinon : suit)
        self.advance_on_release = False
        self.cursor, self.bpm = 0, 120
        self.playing = self.recording = False
        self.playhead, self.next_row_time = 0, 0.0
        self.down: set[int] = set()
        self.right_view = 'tracker'                  # ou 'keyboard' (bouton Keyboard)
        # Tonalite PAR GROUPE (root, octave, gamme) ; des groupes peuvent etre lies (LINK)
        self.keys = {g: {'root': 0, 'octave': 3, 'bank_idx': 0, 'type_idx': 2} for g in GROUPS}
        self.linked: set[str] = set()                         # groupes qui partagent la tonalite
        self.link_edit = False                                # bouton 7 : choisir les groupes
        load_settings(self)                                   # couleurs, tonalites, liens
        self.held_notes: dict[int, list[tuple[int, int]]] = {}   # pad -> [(note, velocite)]
        self.chord_set = 0                                    # mode CHORDS : Chord Set choisi
        self.note_flash: dict[int, tuple[float, int]] = {}   # note -> (fin, velocite)
        self.held: set[str] = set()
        self.flash: dict[int, float] = {}            # pad -> fin de l'allumage
        self.knobs = [Knob(8), Knob(8), Knob(8), Knob(8), Knob(12), Knob(14), Knob(6), Knob(20)]
        # Vue Clavier : octave, root et gamme demandent un vrai tour d'encodeur par cran
        # Keyboard : 5 root, 6 banque, 7 type de gamme (un vrai tour d'encodeur par cran)
        # Keyboard : 5 gamme (liste), 6 root, 8 octave
        self.kb_knobs = {4: Knob(30), 5: Knob(40), 7: Knob(45)}
        # Chords : 5 gamme (liste), 6 mode HARM / SET / USER, 7 choix du mode, 8 position
        self.ch_knobs = {4: Knob(30), 5: Knob(50), 6: Knob(40), 7: Knob(40)}
        # Vue 16 pistes : ligne, piste, note, volume
        self.trk_knobs = {0: Knob(12), 1: Knob(25), 2: Knob(14), 3: Knob(6)}

    # --- tonalite du groupe choisi (propagee aux groupes lies) ---
    def _key_get(name):
        return property(lambda self: self.keys[self.group][name],
                        lambda self, v: self._key_set(name, v))

    root = _key_get('root')
    octave = _key_get('octave')
    bank_idx = _key_get('bank_idx')
    type_idx = _key_get('type_idx')

    def _key_set(self, name, value):
        targets = self.linked if self.group in self.linked else {self.group}
        for g in targets:
            self.keys[g][name] = value

    def link_toggle(self, g: str) -> None:
        """LINK : ajouter / retirer un groupe ; il prend la tonalite du groupe choisi."""
        self.linked.add(self.group)
        if g == self.group:
            return
        if g in self.linked:
            self.linked.discard(g)
        else:
            self.linked.add(g)
            self.keys[g] = dict(self.keys[self.group])
        if self.linked == {self.group}:
            self.linked.clear()

    @property
    def grid(self):
        """Le pattern du groupe choisi."""
        return self.grids[self.group]

    def audible(self, g: str) -> bool:
        """Un groupe joue s'il n'est pas coupe et, si un Solo existe, s'il en fait partie."""
        return g not in self.muted and (not self.solo or g in self.solo)

    @property
    def nr_active(self) -> bool:
        """Note Repeat / Arp en marche (menu ouvert ou non)."""
        return self.nr_on

    @property
    def arp(self) -> bool:
        """Arpegiateur en marche : Note Repeat actif (tenu ou LOCK) en mode ARP."""
        return self.nr_active and self.nr_mode == 'ARP'

    def melodic(self) -> bool:
        """Keyboard ou Chords : les pads jouent des notes avec le son choisi."""
        return self.right_view in ('keyboard', 'chords')

    def chord_notes(self, pad: int) -> tuple[int, ...]:
        """Mode CHORDS : notes de l'accord du pad, selon le mode HARM / SET / USER."""
        base = 12 * (self.octave + 1) + self.root
        if self.chord_mode == 'HARM':                  # 16 pads : degres de la gamme harmonises
            scale = self.scale()[1]
            deg = pad - 1
            note = lambda k: base + scale[k % len(scale)] + 12 * (k // len(scale))
            steps = HARM_TYPES[self.harm_type][1]
            if isinstance(steps, int):                 # Oct +1 / +2 / -1 / -2 : note doublee
                notes = (note(deg), note(deg) + steps)
            else:
                notes = tuple(note(deg + k) for k in steps)
        elif pad > 12:
            return ()
        elif self.chord_mode == 'SET':
            notes = CHORD_SETS[self.chord_set][1][pad - 1].midi_notes(base)
        else:
            notes = tuple(base + i for i in self.user_chords[self.user_bank][pad - 1])
        return invert(notes, self.position)

    def chord_label(self, pad: int) -> str:
        if self.chord_mode == 'SET':
            return CHORD_SETS[self.chord_set][1][pad - 1].display(self.root) if pad <= 12 else ''
        notes = self.chord_notes(pad)
        if not notes:
            return '' if self.chord_mode == 'HARM' else '-'
        return chord_name(notes) or note_name(min(notes))

    def chord_set_label(self) -> str:
        if self.chord_mode == 'HARM':
            return HARM_TYPES[self.harm_type][0]
        if self.chord_mode == 'SET':
            return CHORD_SETS[self.chord_set][0]
        return f'User {self.user_bank + 1}'

    def scale(self):
        """(nom, intervalles) de la gamme choisie."""
        return BANKS[self.bank_idx][1][self.type_idx]

    def pad_note(self, pad: int) -> int:
        """Mode clavier : les 16 pads jouent les degres de la gamme, du grave a l'aigu."""
        scale = self.scale()[1]
        deg = pad - 1
        return min(127, 12 * (self.octave + 1) + self.root + scale[deg % len(scale)]
                   + 12 * (deg // len(scale)))

    def active_notes(self) -> dict[int, int]:
        """Notes a allumer sur le clavier : tenues aux pads + jouees par le pattern."""
        now = time.monotonic()
        notes = {n: v for n, (end, v) in self.note_flash.items() if end > now}
        for held in self.held_notes.values():
            for n, v in held:
                notes[n] = max(v, notes.get(n, 0))
        return notes

    def sound_name(self, s: int) -> str:
        return DRUMS[s - 1] if s <= 4 else f'Note {note_name(default_note(s))}'


# --- Pads / boutons / LED par libusb ---------------------------------------------
# Sur macOS, liberer l'ecran (--detach) prend toute la Maschine au pilote NI :
# hidapi ne peut plus l'ouvrir. On lit donc aussi les pads par libusb, sur la
# meme connexion que les ecrans (interface 4).
import ctypes as C

HID_INTERFACE = 4


class _Endpoint(C.Structure):
    _fields_ = [('bLength', C.c_uint8), ('bDescriptorType', C.c_uint8),
                ('bEndpointAddress', C.c_uint8), ('bmAttributes', C.c_uint8),
                ('wMaxPacketSize', C.c_uint16), ('bInterval', C.c_uint8),
                ('bRefresh', C.c_uint8), ('bSynchAddress', C.c_uint8),
                ('extra', C.POINTER(C.c_uint8)), ('extra_length', C.c_int)]


class _AltSetting(C.Structure):
    _fields_ = [('bLength', C.c_uint8), ('bDescriptorType', C.c_uint8),
                ('bInterfaceNumber', C.c_uint8), ('bAlternateSetting', C.c_uint8),
                ('bNumEndpoints', C.c_uint8), ('bInterfaceClass', C.c_uint8),
                ('bInterfaceSubClass', C.c_uint8), ('bInterfaceProtocol', C.c_uint8),
                ('iInterface', C.c_uint8), ('endpoint', C.POINTER(_Endpoint)),
                ('extra', C.POINTER(C.c_uint8)), ('extra_length', C.c_int)]


class _Interface(C.Structure):
    _fields_ = [('altsetting', C.POINTER(_AltSetting)), ('num_altsetting', C.c_int)]


class _Config(C.Structure):
    _fields_ = [('bLength', C.c_uint8), ('bDescriptorType', C.c_uint8),
                ('wTotalLength', C.c_uint16), ('bNumInterfaces', C.c_uint8),
                ('bConfigurationValue', C.c_uint8), ('iConfiguration', C.c_uint8),
                ('bmAttributes', C.c_uint8), ('MaxPower', C.c_uint8),
                ('interface', C.POINTER(_Interface)),
                ('extra', C.POINTER(C.c_uint8)), ('extra_length', C.c_int)]


class UsbHid:
    """Lecture des rapports 0x01/0x02 et ecriture des LED, via libusb."""

    def __init__(self, screens):
        lib, h = screens.lib, screens.handle
        self.lib, self.h = lib, h
        lib.libusb_get_device.argtypes = [C.c_void_p]
        lib.libusb_get_device.restype = C.c_void_p
        lib.libusb_get_active_config_descriptor.argtypes = [C.c_void_p, C.POINTER(C.POINTER(_Config))]
        lib.libusb_get_active_config_descriptor.restype = C.c_int
        lib.libusb_free_config_descriptor.argtypes = [C.POINTER(_Config)]
        lib.libusb_free_config_descriptor.restype = None
        lib.libusb_interrupt_transfer.argtypes = [C.c_void_p, C.c_ubyte, C.POINTER(C.c_ubyte),
                                                  C.c_int, C.POINTER(C.c_int), C.c_uint]
        lib.libusb_interrupt_transfer.restype = C.c_int
        lib.libusb_control_transfer.argtypes = [C.c_void_p, C.c_uint8, C.c_uint8, C.c_uint16,
                                                C.c_uint16, C.POINTER(C.c_ubyte), C.c_uint16, C.c_uint]
        lib.libusb_control_transfer.restype = C.c_int
        # Trouver les points d'entree / sortie de l'interface 4
        self.ep_in = self.ep_out = None
        cfg = C.POINTER(_Config)()
        if lib.libusb_get_active_config_descriptor(lib.libusb_get_device(h), C.byref(cfg)) == 0:
            for i in range(cfg.contents.bNumInterfaces):
                alt = cfg.contents.interface[i].altsetting[0]
                if alt.bInterfaceNumber != HID_INTERFACE:
                    continue
                for e in range(alt.bNumEndpoints):
                    addr = alt.endpoint[e].bEndpointAddress
                    if addr & 0x80:
                        self.ep_in = self.ep_in or addr
                    else:
                        self.ep_out = self.ep_out or addr
            lib.libusb_free_config_descriptor(cfg)
        print(f'[diag] interface 4 : entree {self.ep_in and hex(self.ep_in)}, '
              f'sortie {self.ep_out and hex(self.ep_out)}')
        if not self.ep_in:
            raise MK3Error("Point d'entree des pads introuvable sur l'interface 4")
        rc = lib.libusb_claim_interface(h, HID_INTERFACE)
        if rc != 0:
            lib.libusb_detach_kernel_driver(h, HID_INTERFACE)
            rc = lib.libusb_claim_interface(h, HID_INTERFACE)
        print(f'[diag] reservation de l\'interface 4 : code {rc} (0 = OK)')
        if rc != 0:
            raise MK3Error(f'Interface 4 (pads) indisponible (code {rc})')
        self.buf = (C.c_ubyte * 64)()

    def read(self, timeout_ms: int = 50) -> bytes:
        n = C.c_int(0)
        rc = self.lib.libusb_interrupt_transfer(self.h, self.ep_in, self.buf, 64, C.byref(n), timeout_ms)
        return bytes(self.buf[:n.value]) if rc == 0 else b''     # -7 = rien a lire (delai)

    def write(self, report: bytes) -> None:
        data = (C.c_ubyte * len(report)).from_buffer_copy(report)
        if self.ep_out:
            n = C.c_int(0)
            self.lib.libusb_interrupt_transfer(self.h, self.ep_out, data, len(report), C.byref(n), 100)
        else:   # HID SET_REPORT (sortie) par le canal de controle
            self.lib.libusb_control_transfer(self.h, 0x21, 0x09, 0x0200 | report[0],
                                             HID_INTERFACE, data, len(report), 100)

    def close(self) -> None:
        self.lib.libusb_release_interface(self.h, HID_INTERFACE)


# --- Dessin des ecrans ---------------------------------------------------------
def group_rgb(st: State):
    """Couleur du theme sur les ecrans : celle du groupe choisi (personnalisable)."""
    return hue_rgb(st.group_hue[st.group])


def hue_rgb(h: int):
    i = GROUP_HUES.index(h) if h in GROUP_HUES else -1
    return GROUP_RGB[i] if i >= 0 else HUE_RGB[h]


def hue_pair(h: int):
    """(douce, vive) d'une teinte de la palette NI."""
    return 5 + 4 * h, 6 + 4 * h


def pad_pair(st: State, pad: int):
    """Couleur d'un pad : sa couleur personnalisee, sinon celle de son groupe."""
    h = st.pad_hue.get((st.group, pad))
    return hue_pair(st.group_hue[st.group] if h is None else h)


def shade(color, k: float):
    return tuple(round(c * k) for c in color)


def mixer_channels(st: State):
    """Les 8 voies affichees : sons du groupe en Pad Mode, groupes sinon (pages 1 / 2)."""
    if not st.melodic():
        sounds = range(1 + 8 * st.mixer_page, 9 + 8 * st.mixer_page)
        return [(f'{s:02d} {st.sound_name(s)}'[:10], ('sound', st.group, s)) for s in sounds]
    groups = tuple(GROUPS) if st.mixer_page == 0 else tuple(f'{g}2' for g in GROUPS)
    return [(f'Group {g}', ('group', g)) for g in groups]


def mix_get(st: State, key) -> int:
    return st.sound_vol[key[1:]] if key[0] == 'sound' else st.group_vol[key[1]]


def mix_set(st: State, key, v: int) -> None:
    v = max(0, min(127, v))
    if key[0] == 'sound':
        st.sound_vol[key[1:]] = v
    else:
        st.group_vol[key[1]] = v


def draw_mixer(st: State, half: int) -> Image.Image:
    """Mixeur sur les 2 ecrans : 4 voies par ecran, chacune sous son encodeur."""
    acc = group_rgb(st)
    img = Image.new('RGB', (W, H), (10, 10, 14))
    d = ImageDraw.Draw(img)
    what = f'Group {st.group} sounds' if not st.melodic() else 'Groups'
    if half == 0:
        header(d, f'MIXER  {what}', f'page {st.mixer_page + 1}/2', acc)
    else:
        header(d, f'MASTER {st.master}', '4D: master   < > page', acc)
    chans = mixer_channels(st)[half * 4: half * 4 + 4]
    for k, (name, key) in enumerate(chans):
        x = k * 120
        v = mix_get(st, key)
        off = key[0] == 'group' and len(key[1]) == 1 and not st.audible(key[1])
        d.text((x + 10, 34), name, font=font(13), fill=(120, 120, 120) if off else 'white')
        rounded(d, (x + 40, 58, x + 80, 230), outline=(70, 70, 80), r=6)
        hgt = round(168 * v / 127)
        if hgt:
            d.rectangle((x + 43, 228 - hgt, x + 77, 227), fill=(90, 90, 90) if off else acc)
        d.line((x + 36, 228 - round(168 * 100 / 127), x + 84, 228 - round(168 * 100 / 127)),
               fill=(220, 220, 220), width=1)            # repere du niveau normal (100)
        d.text((x + 34, 238), f'{v * 100 // 100:3d}', font=font(14), fill='white')
        if off:
            d.text((x + 86, 60), 'M', font=font(14), fill=(200, 60, 60))
    return img


def draw_settings(st: State, half: int) -> Image.Image:
    """SETTINGS > COLORS : gauche = reglages, droite = les 16 pads en couleur."""
    acc = group_rgb(st)
    img = Image.new('RGB', (W, H), (10, 10, 14))
    d = ImageDraw.Draw(img)
    if half == 0:
        header(d, 'SETTINGS  COLORS', f'Group {st.group}', acc)
        d.text((10, 40), f'Target : {st.set_target}', font=font(20), fill='white')
        if st.set_target == 'GROUP':
            lines = ['Pads = the 16 colors.', 'Tap a pad: color of group ' + st.group + '.',
                     'Group buttons: choose the group.']
        else:
            lines = ['Tap pads to select them (several OK).', 'Encoder 2: color of the selection.',
                     'Shift + pad: back to the group color.']
            sel = ', '.join(str(p) for p in sorted(st.set_selected)) or '-'
            d.text((10, 160), f'Selected : {sel}', font=font(14), fill=acc)
        for k, line in enumerate(lines):
            d.text((10, 80 + k * 22), line, font=font(14), fill=(190, 190, 190))
        labels = ['Target', 'Color', 'Shift+MIDI = close', '']
        for k, t in enumerate(labels):
            d.text((k * 120 + 10, 254), t, font=font(12), fill=(160, 160, 160))
        return img
    header(d, 'PADS', 'saved on close', acc)
    for pad in range(1, 17):
        col, row = (pad - 1) % 4, 3 - (pad - 1) // 4
        x, y = 8 + col * 117, 32 + row * 58
        if st.set_target == 'GROUP':
            color, label = hue_rgb(pad - 1), f'color {pad}'
            chosen = st.group_hue[st.group] == pad - 1
        else:
            h = st.pad_hue.get((st.group, pad))
            color = hue_rgb(st.group_hue[st.group] if h is None else h)
            label, chosen = f'pad {pad}', pad in st.set_selected
        rounded(d, (x, y, x + 108, y + 50), fill=color, outline='white' if chosen else None,
                width=3 if chosen else 1)
        tw = d.textlength(label, font=font(14))
        d.text((x + 54 - tw / 2, y + 16), label, font=font(14), fill='black')
    if st.set_confirm:                                 # fermer : sauvegarder ?
        d.rectangle((0, 0, W, 26), fill=(10, 10, 14))
        softkey(d, st, 0, 'YES', False)
        softkey(d, st, 1, 'NO', False)
        rounded(d, (70, 90, 410, 180), fill=(14, 14, 20), outline=acc, width=2)
        d.text((100, 105), 'Save the colors?', font=font(20), fill='white')
        d.text((100, 140), 'Button 5 = YES    Button 6 = NO', font=font(13), fill=(180, 180, 180))
    return img


def draw_nr_panel(st: State) -> Image.Image:
    """Menu NOTE REPEAT / ARPEGGIATOR sur l'ecran gauche (tant qu'il est actif)."""
    acc = group_rgb(st)
    img = Image.new('RGB', (W, H), (10, 10, 14))
    d = ImageDraw.Draw(img)
    title = 'ARPEGGIATOR' if st.nr_mode == 'ARP' else 'NOTE REPEAT'
    header(d, title, 'ON  LOCK' if st.nr_lock else 'ON', acc)
    rate = NR_RATES[st.nr_rate][0]
    d.text((240 - d.textlength(rate, font=font(44)) / 2, 40), rate, font=font(44), fill='white')
    # petite frise : une barre par note sur un temps
    per_beat = NR_RATES[st.nr_rate][1]
    for k in range(int(round(per_beat * 2))):
        x = 40 + k * 400 / (per_beat * 2)
        d.rectangle((x, 112, x + max(3, 360 / (per_beat * 2) - 6), 124),
                    fill=acc if k % max(1, int(round(per_beat))) == 0 else shade(acc, 0.5))
    params = [('Rate', rate), ('Mode', st.nr_mode),
              ('Direction', ARP_DIRS[st.arp_dir] if st.nr_mode == 'ARP' else '-'),
              ('Octaves', f'Oct {st.arp_oct:+d}' if st.arp_oct else 'Oct 0') if st.nr_mode == 'ARP'
              else ('Octaves', '-')]
    for k, (name, val) in enumerate(params):
        x = k * 120
        rounded(d, (x + 6, 150, x + 114, 236), fill=(24, 24, 32), outline=shade(acc, 0.6))
        d.text((x + 14, 158), name, font=font(13), fill=(160, 160, 160))
        d.text((x + 14, 190), val, font=font(18), fill='white' if val != '-' else (90, 90, 90))
    hint = ('LOCK on: NOTE REPEAT = hide menu, it keeps running' if st.nr_lock else
            'NOTE REPEAT again = stop    LOCK = keep running, free the screens')
    d.text((10, 248), hint, font=font(11), fill=(150, 150, 150))
    return img


def draw_left(st: State) -> Image.Image:
    if st.nr_menu and not st.settings:
        return draw_nr_panel(st)
    if st.settings:
        return draw_settings(st, 0)
    if st.mixer:
        return draw_mixer(st, 0)
    if st.tracker16:
        return draw_tracks(st, 0)
    acc = group_rgb(st)
    img = Image.new('RGB', (W, H), 'black')
    d = ImageDraw.Draw(img)
    info = (f'SOLO {"".join(sorted(st.solo))}  ' if st.solo else '') + \
           (f'MUTE {"".join(sorted(st.muted))}  ' if st.muted else '')
    left_header(d, st, f'FUZZY TURTLE  {st.group}', f'{st.bpm} BPM')
    f = font(13)
    for i in range(16):
        pad = 13 + i % 4 - 4 * (i // 4)            # pad 13 en haut a gauche, comme sur la Maschine
        x, y = 8 + (i % 4) * 117, 34 + (i // 4) * 34
        lit = pad in st.down or st.flash.get(pad, 0) > time.monotonic()
        d.rectangle((x, y, x + 110, y + 28), fill=shade(acc, 0.36) if lit else None,
                    outline=acc if pad == st.sound else (70, 70, 70), width=2)
        d.text((x + 6, y + 7), f'{pad:02d} {st.sound_name(pad)}', font=f, fill='white')
    vals = st.params[(st.group, st.sound)]
    for i, name in enumerate(PARAMS):
        x = i * 120
        d.text((x + 10, 176), name, font=f, fill=(180, 180, 180))
        d.text((x + 82, 176), f'{vals[i]:3d}', font=f, fill='white')
        d.rectangle((x + 10, 198, x + 110, 208), outline=(90, 90, 90))
        d.rectangle((x + 10, 198, x + 10 + round(100 * vals[i] / 127), 208), fill=acc)
    d.text((10, 222), f'Sound : {st.sound:02d} {st.sound_name(st.sound)}', font=font(15), fill=acc)
    state = ('REC ' if st.recording else '') + ('PLAY' if st.playing else 'STOP')
    d.text((10, 248), f'{state}   {info}Shift+pad = clear', font=font(12), fill=(140, 140, 140))
    tempo_overlay(st, d)
    return img


def tempo_overlay(st: State, d: ImageDraw.ImageDraw) -> None:
    """TEMPO (ou VOLUME) tenu : grand affichage du tempo (ou du volume master)."""
    if 'volume' in st.held:
        acc = group_rgb(st)
        d.rectangle((90, 70, 390, 200), fill=(15, 15, 20), outline=acc, width=3)
        d.text((110, 80), 'MASTER VOLUME', font=font(16), fill=acc)
        txt = f'{st.master}'
        d.text((240 - d.textlength(txt, font=font(40)) / 2, 108), txt, font=font(40), fill='white')
        d.text((110, 172), 'hold VOLUME + turn 4D', font=font(13), fill=(170, 170, 170))
        return
    if 'tempo' not in st.held:
        return
    acc = group_rgb(st)
    d.rectangle((90, 70, 390, 200), fill=(15, 15, 20), outline=acc, width=3)
    d.text((110, 80), 'TEMPO', font=font(16), fill=acc)
    txt = f'{st.bpm} BPM'
    d.text((240 - d.textlength(txt, font=font(40)) / 2, 108), txt, font=font(40), fill='white')
    d.text((110, 172), 'tap TEMPO  or  hold + turn 4D', font=font(13), fill=(170, 170, 170))


def step_label(st: State) -> str:
    return f'STEP {step_page(st) + 1}/{(st.length + 15) // 16}  ' if st.step_mode else ''


def play_row(st: State) -> int:
    """Ligne en train de jouer (-1 a l'arret)."""
    return (st.playhead - 1) % st.length if st.playing else -1


def step_page(st: State) -> int:
    """Page STEP affichee : suit la lecture en Play, sinon la page du curseur."""
    if st.step_manual is not None:
        return min(st.step_manual, (st.length - 1) // 16)
    return (play_row(st) if st.playing and st.follow else st.cursor) // 16


def view_top(st: State, visible: int) -> int:
    """Premiere ligne affichee : suit la ligne qui joue pendant la lecture, sinon le curseur."""
    center = play_row(st) if st.playing and st.follow else st.cursor
    return max(0, min(center - visible // 2, st.length - visible))


def softkey(d: ImageDraw.ImageDraw, st: State, slot: int, text: str, active: bool) -> None:
    """Etiquette d'un bouton au-dessus de l'ecran (slot 0-3 de cet ecran) :
    sombre = disponible, blanche = fonction active."""
    x0 = 4 + slot * 120
    if active:
        d.rectangle((x0, 3, x0 + 108, 23), fill='white')
        d.text((x0 + 10, 4), text, font=font(16), fill='black')
    else:
        d.rectangle((x0, 3, x0 + 108, 23), fill=shade(group_rgb(st), 0.35), outline=(20, 20, 20))
        d.text((x0 + 10, 4), text, font=font(16), fill=(235, 235, 235))


def softkey_header(d: ImageDraw.ImageDraw, st: State, right: str) -> None:
    """Bandeau de l'ecran droit : nom du mode (PAD MODE, KEYBOARD, CHORDS) et infos."""
    acc = group_rgb(st)
    header(d, {'keyboard': 'KEYBOARD', 'chords': 'CHORDS'}.get(st.right_view, 'PAD MODE'), right, acc)


def left_header(d: ImageDraw.ImageDraw, st: State, title: str, right: str) -> None:
    """Bandeau de l'ecran gauche avec l'etiquette TRACKER sous le bouton 3."""
    header(d, title, right, group_rgb(st))
    softkey(d, st, 2, 'TRACKER', st.tracker16)


def draw_tracker(st: State) -> Image.Image:
    """Vue compacte (Pad Mode) : la piste du son choisi, ligne par ligne."""
    acc = group_rgb(st)
    img = Image.new('RGB', (W, H), (12, 12, 16))
    d = ImageDraw.Draw(img)
    softkey_header(d, st, step_label(st) + ('REC ' if st.recording else '')
                   + f'Trk {st.sound:02d} {st.group}')
    f = font(14)
    t = st.sound - 1
    d.text((8, 31), f'Lin  Note Vol   {st.sound_name(st.sound)}', font=f, fill=acc)
    top = view_top(st, VISIBLE)
    for k in range(min(VISIBLE, st.length)):
        r = top + k
        y = 50 + k * 18
        if r == st.cursor:
            d.rectangle((0, y - 1, W, y + 16), fill=shade(acc, 0.44) if st.recording else shade(acc, 0.28))
        elif r == play_row(st):
            d.rectangle((0, y - 1, W, y + 16), fill=(0, 60, 40))
        elif r % 4 == 0:
            d.rectangle((0, y - 1, W, y + 16), fill=(30, 30, 38))
        cell = st.grid[r][t]
        txt = f'{r:02X}   {cell_label(cell[0])}  {cell[1]:02X}' if cell else f'{r:02X}   ---  --'
        d.text((8, y), txt, font=f, fill='white' if cell else (110, 110, 110))
        others = sum(1 for i, c in enumerate(st.grid[r]) if c and i != t)
        if others:
            d.text((200, y), f'+{others} other track' + ('s' if others > 1 else ''),
                   font=font(12), fill=(130, 130, 130))
    labels = ['Line', 'Note', 'Volume', 'Length']
    cell = st.grid[st.cursor][t]
    values = [f'{st.cursor:02X}', cell_label(cell[0]) if cell else '---',
              f'{cell[1]:02X}' if cell else '--', f'{st.length}']
    for i in range(4):
        d.text((i * 120 + 10, 250), f'{labels[i]} {values[i]}', font=font(12), fill=(180, 180, 180))
    return img


TRK_ROWS, TRK_X0, TRK_W = 15, 24, 57
SHORT = ['Kick', 'Snare', 'CHH', 'OHH']


def draw_tracks(st: State, first: int) -> Image.Image:
    """Vue Tracker 16 pistes : pistes first+1 a first+8 (gauche 1-8, droite 9-16)."""
    acc = group_rgb(st)
    img = Image.new('RGB', (W, H), (12, 12, 16))
    d = ImageDraw.Draw(img)
    rec = step_label(st) + ('REC  ' if st.recording else '')
    if first == 0:
        left_header(d, st, f'Pattern 01  {st.group}', f'{rec}1-8')
    else:
        softkey_header(d, st, f'{rec}9-16')
    f = font(11)
    for t in range(8):
        x = TRK_X0 + t * TRK_W
        tr = first + t
        name = SHORT[tr] if tr < 4 else note_name(default_note(tr + 1))
        sel = tr == st.sound - 1
        d.rectangle((x, 29, x + TRK_W - 3, 41), fill=acc if sel else shade(acc, 0.35))
        d.text((x + 3, 29), f'{tr + 1:02d} {name}', font=f, fill='black' if sel else 'white')
    top = view_top(st, TRK_ROWS)
    playing_row = play_row(st)
    for k in range(min(TRK_ROWS, st.length)):
        r = top + k
        y = 45 + k * 14
        if r == st.cursor:
            d.rectangle((0, y - 1, W, y + 12), fill=(45, 45, 55))
        elif r == playing_row:
            d.rectangle((0, y - 1, W, y + 12), fill=(0, 55, 38))
        elif r % 4 == 0:
            d.rectangle((0, y - 1, W, y + 12), fill=(26, 26, 32))
        d.text((2, y), f'{r:02X}', font=f, fill=acc if r % 4 == 0 else (120, 120, 120))
        for t in range(8):
            tr = first + t
            x = TRK_X0 + t * TRK_W
            if r == st.cursor and tr == st.sound - 1:
                d.rectangle((x - 1, y - 1, x + TRK_W - 4, y + 12),
                            fill=shade(acc, 0.6 if st.recording else 0.45))
            cell = st.grid[r][tr]
            d.text((x + 3, y), f'{cell_label(cell[0])} {cell[1]:02X}' if cell else '--- --',
                   font=f, fill='white' if cell else (80, 80, 80))
    if first == 0:
        cell = st.grid[st.cursor][st.sound - 1]
        info = (f'Line {st.cursor:02X}   Track {st.sound:02d}   '
                f'Note {cell_label(cell[0]) if cell else "---"}   Vol {cell[1]:02X}' if cell else
                f'Line {st.cursor:02X}   Track {st.sound:02d}   Note ---   Vol --')
        d.text((6, 256), info + f'   Len {st.length}', font=font(11), fill=(170, 170, 170))
        tempo_overlay(st, d)
    return img


def chord_name(notes) -> str:
    pcs = sorted({n % 12 for n in notes})
    if len(pcs) == 1:
        return note_name(min(notes))
    for root in pcs:
        shape = tuple(sorted((p - root) % 12 for p in pcs))
        for tpl, suffix in CHORDS:
            if shape == tpl:
                return KEY_NAMES[root] + suffix
    return ''


WHITE = (0, 2, 4, 5, 7, 9, 11)


def draw_keyboard(st: State) -> Image.Image:
    acc = group_rgb(st)
    img = Image.new('RGB', (W, H), (12, 12, 16))
    d = ImageDraw.Draw(img)
    chords = st.right_view == 'chords'
    scale_name, scale = st.scale()
    if chords:
        softkey_header(d, st, step_label(st) + f'{st.chord_mode} {st.chord_set_label()}')
        if st.chord_mode != 'HARM':
            scale = ()                             # points de gamme seulement en Harmonizer
    else:
        softkey_header(d, st, step_label(st) + f'{KEY_NAMES[st.root]}{st.octave} {scale_name}'[:24])
        d.text((10, 30), link_label(st), font=font(11), fill=(150, 150, 150))
    low = 12 * (st.octave + 1)                     # 3 octaves a partir du C de l'octave
    high = low + 36
    notes = st.active_notes()
    # Nom de l'accord ou de la note, et fleches si une note sort de la plage
    if chords and st.last_chord_pad in st.down:    # nom exact de l'accord du Chord Set
        name = st.chord_label(st.last_chord_pad)
    else:
        name = chord_name(notes) if notes else ''
    d.text((W // 2 - d.textlength(name, font=font(26)) // 2, 34), name, font=font(26), fill=acc)
    if any(n < low for n in notes):
        d.text((8, 38), '<', font=font(24), fill=acc)
    if any(n >= high for n in notes):
        d.text((W - 22, 38), '>', font=font(24), fill=acc)

    def lit(v):                                    # orange plus ou moins vif selon la velocite
        k = 0.35 + 0.65 * v / 127
        return shade(acc, k)

    kw, top, bottom = W / 21, 76, 236
    in_scale = {(st.root + i) % 12 for i in scale}
    blacks = []
    for o in range(3):
        for wi, pc in enumerate(WHITE):
            n = low + 12 * o + pc
            x0 = round((o * 7 + wi) * kw)
            x1 = round((o * 7 + wi + 1) * kw) - 1
            d.rectangle((x0, top, x1, bottom), fill=lit(notes[n]) if n in notes else (225, 225, 225),
                        outline=(40, 40, 40))
            if pc in in_scale:
                r = 5 if pc == st.root else 3
                cx, cy = (x0 + x1) // 2, bottom - 14
                d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(60, 60, 60))
            if pc == 0:
                d.text((x0 + 3, bottom + 4), f'C{12 * o // 12 + st.octave}', font=font(11),
                       fill=(150, 150, 150))
            if pc in (0, 2, 5, 7, 9):                  # touche noire a droite de C, D, F, G, A
                blacks.append((n + 1, round((o * 7 + wi + 1) * kw)))
    for n, xc in blacks:
        x0, x1 = xc - 8, xc + 8
        d.rectangle((x0, top, x1, top + 96), fill=lit(notes[n]) if n in notes else (20, 20, 20))
        if n % 12 in in_scale:
            r = 4 if n % 12 == st.root else 2
            d.ellipse((xc - r, top + 82 - r, xc + r, top + 82 + r), fill=(200, 200, 200))
    if chords:
        labels = [f'{st.scale()[0]}'[:14], f'Mode {st.chord_mode}', st.chord_set_label(),
                  f'Pos {st.position:+d}']
    else:
        labels = [scale_name[:14], f'Root {KEY_NAMES[st.root]}', BANKS[st.bank_idx][0],
                  f'Octave {st.octave}']
    for i, t in enumerate(labels):
        d.text((i * 120 + 10, 254), t, font=font(12), fill=(180, 180, 180))
    scale_dropdown(st, d)
    return img




def draw_right(st: State) -> Image.Image:
    img = _draw_right(st)
    if st.nr_active and not st.settings:             # Note Repeat / Arp : vitesses sur 5-8
        d = ImageDraw.Draw(img)
        title = (f'ARP {NR_RATES[st.nr_rate][0]}' if st.arp else f'REPEAT {NR_RATES[st.nr_rate][0]}') \
            + (' L' if st.nr_lock else '')
        d.rectangle((W - 130, 230, W - 2, 252), fill=group_rgb(st))
        d.text((W - 122, 233), title, font=font(14), fill='black')
    return img


def _draw_right(st: State) -> Image.Image:
    if st.settings:
        return draw_settings(st, 1)
    if st.mixer:
        return draw_mixer(st, 1)
    if st.tracker16:
        return draw_tracks(st, 8)
    if st.right_view == 'chords':
        img = draw_keyboard(st) if st.chord_keyboard else draw_chords(st)
        return link_overlay(st, img)
    if st.right_view == 'keyboard':
        return link_overlay(st, draw_keyboard(st))
    return draw_keyboard(st) if st.right_view == 'keyboard' else draw_tracker(st)


def rounded(d, box, fill=None, outline=None, width=1, r=8):
    d.rounded_rectangle(box, radius=r, fill=fill, outline=outline, width=width)


def draw_chords(st: State) -> Image.Image:
    """Mode CHORDS : grille 4 x 4 disposee comme les pads (pad 1 en bas a gauche)."""
    acc = group_rgb(st)
    img = Image.new('RGB', (W, H), (10, 10, 14))
    d = ImageDraw.Draw(img)
    header(d, 'CHORDS', '', acc)
    d.text((130, 5), f'{st.chord_mode} {st.chord_set_label()}'[:13], font=font(14), fill='black')
    if st.chord_mode == 'USER':
        softkey(d, st, 3, 'EDIT', st.user_edit)
    if st.user_edit:
        return draw_user_edit(st, img, d)
    hints = {13: 'Shift Root-', 14: 'Shift Root+', 15: 'Shift Oct-', 16: 'Shift Oct+'}
    cw, ch, top = 117, 50, 30
    for pad in range(1, 17):
        col, row = (pad - 1) % 4, 3 - (pad - 1) // 4      # pad 1 en bas a gauche
        x, y = 4 + col * cw, top + row * ch
        box = (x, y, x + cw - 6, y + ch - 5)
        if st.chord_mode != 'HARM' and pad > 12:          # rangee du haut : fonctions Shift
            rounded(d, box, outline=(45, 45, 52))
            tw = d.textlength(hints[pad], font=font(12))
            d.text((x + (cw - 6) / 2 - tw / 2, y + 17), hints[pad], font=font(12), fill=(100, 100, 112))
            continue
        label = st.chord_label(pad)
        playing = pad in st.down
        if playing:
            rounded(d, box, fill=acc)
        else:
            rounded(d, box, fill=(26, 26, 34), outline=shade(acc, 0.55), width=1)
            d.rectangle((x + 1, y + 1, x + 4, y + ch - 7), fill=shade(acc, 0.8))   # liseret
        ink = 'black' if playing else 'white'
        d.text((x + 10, y + 3), f'{pad}', font=font(11), fill='black' if playing else (130, 130, 140))
        text = label[:9]
        tw = d.textlength(text, font=font(18))
        d.text((x + (cw - 6) / 2 - tw / 2, y + 18), text, font=font(18), fill=ink)
    labels = [f'{st.scale()[0]}'[:14], f'Mode {st.chord_mode}', st.chord_set_label(),
              f'Pos {st.position:+d}']
    for k, t in enumerate(labels):
        d.text((k * 120 + 10, 254), t, font=font(12), fill=(180, 180, 180))
    scale_dropdown(st, d)
    return img


def link_label(st: State) -> str:
    if not st.linked:
        return f'Key: group {st.group} only'
    if len(st.linked) == len(GROUPS):
        return 'Key: all groups'
    return 'Key: ' + ' '.join(sorted(st.linked))


def link_overlay(st: State, img: Image.Image) -> Image.Image:
    """Etiquette LINK sous le bouton 7 ; en mode LINK, panneau de choix des groupes."""
    d = ImageDraw.Draw(img)
    softkey(d, st, 2, 'LINK', st.link_edit)
    if not st.link_edit:
        return img
    acc = group_rgb(st)
    rounded(d, (40, 40, 440, 230), fill=(14, 14, 20), outline=acc, width=2)
    d.text((60, 50), 'LINK KEY (root, octave, scale)', font=font(15), fill=acc)
    for i, g in enumerate(GROUPS):
        x, y = 60 + (i % 4) * 92, 82 + (i // 4) * 46
        on = g in st.linked or (g == st.group and st.linked)
        rounded(d, (x, y, x + 80, y + 36), fill=hue_rgb(st.group_hue[g]) if on else (30, 30, 38),
                outline=hue_rgb(st.group_hue[g]))
        d.text((x + 34, y + 9), g, font=font(16), fill='black' if on else 'white')
    d.text((60, 178), 'Group buttons: link / unlink', font=font(12), fill=(170, 170, 170))
    d.text((60, 196), 'Shift + 7: all / none     7: done', font=font(12), fill=(170, 170, 170))
    return img


def draw_user_edit(st: State, img, d) -> Image.Image:
    """USER > EDIT : les 16 pads = 16 demi-tons a partir du root ; on allume / eteint les notes."""
    acc = group_rgb(st)
    chord = st.user_chords[st.user_bank][st.user_slot - 1]
    d.text((10, 34), f'EDIT  User {st.user_bank + 1}', font=font(15), fill=acc)
    d.text((10, 54), f'Pad {st.user_slot}', font=font(15), fill=acc)
    name = chord_name([12 * (st.octave + 1) + st.root + i for i in chord]) if chord else '(empty)'
    d.text((10, 78), name, font=font(24), fill='white')
    for pad in range(1, 17):
        col, row = (pad - 1) % 4, 3 - (pad - 1) // 4
        x, y = 250 + col * 57, 32 + row * 54
        on = (pad - 1) in chord
        rounded(d, (x, y, x + 50, y + 44), fill=acc if on else (26, 26, 34), outline=shade(acc, 0.5))
        d.text((x + 6, y + 14), NOTES[(st.root + pad - 1) % 12], font=font(15),
               fill='black' if on else (200, 200, 200))
    for k, line in enumerate(('Pads = root + 0..15', 'Tap = add / remove', 'Button 8 = done')):
        d.text((10, 150 + k * 20), line, font=font(12), fill=(160, 160, 160))
    return img


def scale_dropdown(st: State, d: ImageDraw.ImageDraw) -> None:
    """Liste des gammes en arborescence : intitules de categories (non selectionnables) et
    gammes ; la gamme en surbrillance s'applique apres 1,5 s sans mouvement (ou clic 4D)."""
    if not st.pending_scale:
        return
    acc = group_rgb(st)
    rows = []                                          # ('cat', nom) ou ('scale', b, t)
    for b, (bank, scales) in enumerate(BANKS):
        rows.append(('cat', bank))
        rows += [('scale', b, t) for t in range(len(scales))]
    sel = rows.index(('scale',) + tuple(st.pending_scale))
    rounded(d, (60, 30, 420, 248), fill=(14, 14, 20), outline=acc, width=2)
    left = max(0, st.scale_deadline - time.monotonic())
    d.text((76, 36), 'SCALE', font=font(15), fill=acc)
    d.text((300, 38), f'apply in {left:.1f}s', font=font(11), fill=(150, 150, 150))
    first = max(0, min(sel - 4, len(rows) - 9))
    for k, row in enumerate(rows[first:first + 9]):
        y = 58 + k * 20
        if row[0] == 'cat':
            d.text((72, y), row[1].upper(), font=font(13), fill=acc)
            d.line((72 + d.textlength(row[1].upper(), font=font(13)) + 6, y + 9, 410, y + 9),
                   fill=shade(acc, 0.5))
            continue
        b, t = row[1], row[2]
        if (b, t) == tuple(st.pending_scale):
            d.rectangle((84, y - 1, 414, y + 17), fill=acc)
        current = (b, t) == (st.bank_idx, st.type_idx)
        d.text((92, y), ('* ' if current else '  ') + BANKS[b][1][t][0], font=font(14),
               fill='black' if (b, t) == tuple(st.pending_scale) else 'white')


# --- Logique -------------------------------------------------------------------
def step_of_pad(pad: int) -> int:
    """Mode STEP : pas 1-4 sur la rangee du haut (pads 13-16), comme sur Maschine."""
    return (3 - (pad - 1) // 4) * 4 + (pad - 1) % 4


def step_note(st: State) -> int:
    """Note posee en STEP : hauteur du son en Pad Mode, root de la gamme en Keyboard."""
    if st.right_view == 'chords':
        return st.chord_notes(1)
    return st.pad_note(1) if st.right_view == 'keyboard' else default_note(st.sound)


def step_row(st: State, pad: int) -> int:
    return step_page(st) * 16 + step_of_pad(pad)         # page de 16 pas (suit la lecture)


def sound_for(st: State, pad: int) -> list:
    """Les notes a jouer tout de suite pour un pad frappe : [(son, note), ...]."""
    if 'shift' in st.held or st.settings:
        return []
    if st.step_mode:                       # sans REC : on ecoute le son ; avec REC : seulement
        if not st.recording or st.grid[step_row(st, pad)][st.sound - 1] is None:   # en ajoutant
            return [(st.sound, n) for n in cell_notes(step_note(st))]
        return []
    if st.right_view == 'chords' and st.user_edit:
        return [(st.sound, 12 * (st.octave + 1) + st.root + pad - 1)]
    if st.right_view == 'chords':
        return [(st.sound, n) for n in st.chord_notes(pad)]
    if st.right_view == 'keyboard':
        return [(st.sound, st.pad_note(pad))]
    return [(pad, default_note(pad))]


def snapshot(st: State, coalesce: bool = False) -> None:
    """Memorise le pattern avant une modification (pour Undo). coalesce : les reglages
    a l'encodeur tres rapproches ne comptent que pour une seule etape."""
    now = time.monotonic()
    if coalesce and now - st.last_undo_push < 0.8:
        st.last_undo_push = now
        return
    st.last_undo_push = now
    st.undo_stack.append((st.group, [row[:] for row in st.grid]))
    del st.undo_stack[:-100]                     # 100 etapes maximum
    st.redo_stack.clear()


def undo(st: State, redo: bool = False) -> None:
    src, dst = (st.redo_stack, st.undo_stack) if redo else (st.undo_stack, st.redo_stack)
    if not src:
        return
    group, grid = src.pop()
    dst.append((group, [row[:] for row in st.grids[group]]))
    st.grids[group] = grid


def record(st: State, track: int, note, vel: int) -> None:
    """Ecrit une note sur une piste : a la ligne qui joue (lecture) ou a la ligne du curseur."""
    snapshot(st, coalesce=st.playing)
    if st.playing:
        st.grid[play_row(st)][track - 1] = (note, vel)
    else:
        st.grid[st.cursor][track - 1] = (note, vel)
        st.advance_on_release = True                 # avancer quand tous les pads sont relaches


def handle(st: State, ev: tuple) -> None:
    kind = ev[0]
    if kind == 'pad_on':
        pad, vel = ev[1], ev[2]
        st.down.add(pad)
        keyboard = st.melodic()                      # Keyboard ou Chords
        if st.settings:                              # SETTINGS > COLORS
            if st.set_confirm:
                return
            if st.set_target == 'GROUP':
                st.group_hue[st.group] = pad - 1     # pad n = teinte n
            elif 'shift' in st.held:
                st.pad_hue.pop((st.group, pad), None)   # retour a la couleur du groupe
            else:
                st.set_selected ^= {pad}
            return
        if st.right_view == 'chords' and st.user_edit:   # EDIT USER : pads = 16 demi-tons
            chord = set(st.user_chords[st.user_bank][st.user_slot - 1])
            chord ^= {pad - 1}
            st.user_chords[st.user_bank][st.user_slot - 1] = tuple(sorted(chord))
            return
        if 'shift' in st.held and pad in (3, 4):     # Shift + pad 3 / 4 : Step Undo / Redo
            undo(st, redo=pad == 4)
            return
        if st.step_mode:
            if 'shift' in st.held:                   # Shift + pad : choisir la piste
                st.sound = pad
                return
            if not st.recording:                     # sans REC : on ecoute, rien n'est modifie
                return
            r, t = step_row(st, pad), st.sound - 1   # pad = un pas : ajouter / enlever la note
            snapshot(st)
            st.grid[r][t] = None if st.grid[r][t] else (step_note(st), vel)
            return
        if keyboard and 'shift' in st.held and pad >= 13:   # comme serigraphie sur la MK3
            if pad == 13:
                st.root = (st.root - 1) % 12             # Shift + pad 13 : root - 1 demi-ton
            elif pad == 14:
                st.root = (st.root + 1) % 12             # Shift + pad 14 : root + 1 demi-ton
            elif pad == 15:
                st.octave = max(0, st.octave - 1)        # Shift + pad 15 : octave -
            else:
                st.octave = min(7, st.octave + 1)        # Shift + pad 16 : octave +
            return
        if 'shift' in st.held:                       # effacer la case
            snapshot(st)
            st.grid[st.cursor][(st.sound if keyboard else pad) - 1] = None
            return
        if st.right_view == 'chords':                # pads 1-12 = accords du Chord Set
            if pad <= 12:
                st.user_slot = pad                   # pad a modifier en EDIT (mode USER)
            notes = st.chord_notes(pad)
            if notes:
                st.last_chord_pad = pad
                st.held_notes[pad] = [(n, vel) for n in notes]
                if st.recording:
                    record(st, st.sound, notes, vel)
            return
        if keyboard:                                 # pads = notes de la gamme
            note = st.pad_note(pad)
            st.held_notes[pad] = [(note, vel)]
            if st.recording:
                record(st, st.sound, note, vel)
            return
        st.sound = pad                               # pads = sons du groupe
        if st.recording:
            record(st, pad, default_note(pad), vel)
    elif kind == 'pad_off':
        st.down.discard(ev[1])
        st.held_notes.pop(ev[1], None)
        if not st.down and st.advance_on_release:
            st.cursor = min(st.length - 1, st.cursor + 1)
            st.advance_on_release = False
    elif kind == 'jog':                              # grosse molette 4D
        if st.mixer or 'volume' in st.held:          # MIXER, ou VOLUME tenu : volume master
            st.master = max(0, min(127, st.master + 2 * ev[1]))
        elif 'tempo' in st.held:                     # TEMPO tenu + molette : tempo au BPM pres
            st.bpm = max(40, min(240, st.bpm + ev[1]))
            st.taps.clear()
        elif not st.melodic() or st.tracker16:
            st.cursor = max(0, min(st.length - 1, st.cursor + ev[1]))
    elif kind == 'unknown_button':
        print(f'[diag] bouton inconnu : bit {ev[1]} ({"appuye" if ev[2] else "relache"})')
    elif kind == 'button':
        name, pressed = ev[1], ev[2]
        (st.held.add if pressed else st.held.discard)(name)
        if not pressed:
            return
        if name.startswith('group_'):
            g = name[-1].upper()
            if 'mute' in st.held:                    # MUTE + groupe : couper / reactiver
                st.muted ^= {g}
            elif 'solo' in st.held:                  # SOLO + groupe(s) : n'ecouter que ceux-la
                st.solo ^= {g}
            elif st.link_edit:                       # LINK : lier / delier ce groupe
                st.link_toggle(g)
            else:
                st.group = g
        elif name == 'screen_7' and st.melodic() and not st.mixer and not st.nr_menu:
            if 'shift' in st.held:                   # Shift + 7 : lier tous les groupes / aucun
                if len(st.linked) == len(GROUPS):
                    st.linked.clear()
                else:
                    for g in GROUPS:
                        if g != st.group:
                            st.keys[g] = dict(st.keys[st.group])
                    st.linked = set(GROUPS)
            else:
                st.link_edit = not st.link_edit      # 7 : choisir les groupes lies
        elif name == 'midi_channel' and 'shift' in st.held:   # Shift + MIDI : SETTINGS
            if not st.settings:
                st.settings, st.set_confirm = True, False
                st.set_backup = (dict(st.group_hue), dict(st.pad_hue))
                st.set_selected.clear()
            else:
                st.set_confirm = True                # fermer : sauvegarder ? YES / NO
        elif name in ('screen_5', 'screen_6') and st.set_confirm:
            if name == 'screen_5':                   # YES : sauvegarder
                save_settings(st)
            else:                                    # NO : revenir aux couleurs d'avant
                st.group_hue, st.pad_hue = st.set_backup
            st.settings = st.set_confirm = False
            st.set_selected.clear()
        elif name == 'follow':                       # FOLLOW : l'affichage suit la lecture
            st.follow = not st.follow
        elif name == 'note_repeat':
            if not st.nr_on:                         # 1er appui : marche + menu
                st.nr_on = st.nr_menu = True
                st.arp_index = 0
            elif not st.nr_menu:                     # verrouille, menu ferme : rouvrir le menu
                st.nr_menu = True
            elif st.nr_lock:                         # menu ouvert + LOCK : fermer le menu,
                st.nr_menu = False                   # la fonction continue
            else:                                    # menu ouvert sans LOCK : arret
                st.nr_on = st.nr_menu = False
        elif name == 'lock':                         # LOCK : garder Note Repeat / Arp actif
            st.nr_lock = not st.nr_lock
            if st.nr_lock and st.nr_on:
                st.nr_menu = False                   # on peut naviguer dans les autres ecrans
            elif not st.nr_lock and not st.nr_menu:
                st.nr_on = False                     # deverrouille sans menu : arret
        elif name == 'mute' and 'shift' in st.held:  # Shift + MUTE : panique, tout se tait
            SOUND.panic()
            st.note_flash.clear()
        elif name == 'play':
            st.playing = not st.playing
            st.step_manual = None                    # en lecture, les pages suivent a nouveau
            st.next_row_time = time.monotonic()
        elif name == 'stop':
            st.playing, st.playhead = False, 0
        elif name == 'rec':
            st.recording = not st.recording
        elif name == 'keyboard':
            st.mixer = False
            if st.sound <= 4:                        # sons 1-4 = batterie : prendre un son melodique
                st.sound = 5
            st.right_view, st.tracker16, st.step_mode = 'keyboard', False, False
            st.held_notes.clear()
        elif name == 'chords' and 'shift' in st.held:   # Shift + CHORDS : clavier des accords
            st.chord_keyboard = not st.chord_keyboard
        elif name == 'chords':
            st.mixer = False
            if st.sound <= 4:                        # sons 1-4 = batterie : prendre un son melodique
                st.sound = 5
            st.right_view, st.step_mode = 'chords', False
            st.held_notes.clear()
        elif name == 'pad_mode' and st.link_edit:
            st.link_edit = False
            st.right_view, st.tracker16, st.step_mode = 'tracker', False, False
        elif name == 'pad_mode' and st.mixer:
            st.mixer = False
            st.right_view, st.tracker16, st.step_mode = 'tracker', False, False
        elif name == 'pad_mode':
            st.right_view, st.tracker16, st.step_mode = 'tracker', False, False
            st.held_notes.clear()
        elif name == 'nav_click' and st.pending_scale:   # clic 4D : appliquer tout de suite
            apply_pending_scale(st, force=True)
        elif name == 'mixer':                        # MIXER : ouvrir / fermer le mixeur
            st.mixer = not st.mixer
            st.mixer_page = 0
        elif name in ('nav_left', 'nav_right', 'nav_prev', 'nav_next') and st.mixer:   # page 1 / 2
            st.mixer_page = 1 if name in ('nav_right', 'nav_next') else 0
        elif name == 'screen_8' and st.right_view == 'chords' and st.chord_mode == 'USER':
            st.user_edit = not st.user_edit          # bouton 8 : EDIT ; 2e appui = sauvegarde
            if not st.user_edit:
                save_user_chords(st)
        elif name in ('nav_prev', 'nav_next') and st.step_mode:   # < > : page STEP
            pages = (st.length + 15) // 16
            page = max(0, min(pages - 1, step_page(st) + (1 if name == 'nav_next' else -1)))
            st.step_manual = page
            if not st.playing:
                st.cursor = min(st.length - 1, page * 16 + st.cursor % 16)
        elif name == 'step':                         # STEP : depuis Pad Mode, Keyboard ou Tracker
            st.step_mode = not st.step_mode          # 2e appui : retour au mode d'avant
            st.step_manual = None
            st.held_notes.clear()
        elif name in ('nav_up', 'nav_down') and st.melodic() and not st.tracker16:
            st.octave = max(0, min(7, st.octave + (1 if name == 'nav_up' else -1)))  # octave
        elif name in ('nav_up', 'nav_down'):          # 4D pousse haut / bas : ligne
            st.cursor = max(0, min(st.length - 1, st.cursor + (1 if name == 'nav_down' else -1)))
        elif name in ('nav_left', 'nav_right'):       # 4D pousse gauche / droite : piste
            st.sound = max(1, min(16, st.sound + (1 if name == 'nav_right' else -1)))
        elif name == 'screen_3' and not st.mixer:    # TRACKER (bouton 3), dans tous les modes
            st.tracker16 = not st.tracker16          # 2e appui : retour a la vue precedente
        elif name == 'tempo':                        # tap tempo (TEMPO tenu + molette : jog)
            tap_tempo(st)
    elif kind == 'knob':
        i = ev[1] - 1
        if st.settings:
            if i in st.set_knobs:
                n = st.set_knobs[i].steps(ev[2])
                if n and i == 0:                     # 1 : cible GROUP / PADS
                    st.set_target = 'PADS' if st.set_target == 'GROUP' else 'GROUP'
                elif n and i == 1 and st.set_target == 'PADS':   # 2 : couleur des pads choisis
                    for p in st.set_selected:
                        h = st.pad_hue.get((st.group, p), st.group_hue[st.group])
                        st.pad_hue[(st.group, p)] = (h + n) % 16
            return
        if st.nr_menu and i in st.nr_knobs:          # menu Note Repeat / Arp (ecran gauche)
            n = st.nr_knobs[i].steps(ev[2])
            if n and i == 0:
                st.nr_rate = (st.nr_rate + n) % len(NR_RATES)
            elif n and i == 1:
                st.nr_mode = 'ARP' if st.nr_mode == 'REPEAT' else 'REPEAT'
                st.arp_index = 0
            elif n and i == 2:
                st.arp_dir = (st.arp_dir + n) % len(ARP_DIRS)
            elif n and i == 3:
                st.arp_oct = max(-3, min(3, st.arp_oct + n))
            return
        if st.mixer:                                 # MIXER : un encodeur par voie
            n = st.mix_knobs[i].steps(ev[2])
            if n:
                key = mixer_channels(st)[i][1]
                mix_set(st, key, mix_get(st, key) + n)
            return
        if st.right_view == 'chords' and not st.tracker16 and i in st.ch_knobs:
            n = st.ch_knobs[i].steps(ev[2])
            if not n:
                return
            if i == 4:                               # 5 : gamme (liste deroulante)
                choose_scale(st, n)
            elif i == 5:                             # 6 : mode HARM / SET / USER
                st.chord_mode = CHORD_MODES[(CHORD_MODES.index(st.chord_mode) + n) % len(CHORD_MODES)]
                if st.chord_mode != 'USER' and st.user_edit:
                    st.user_edit = False
                    save_user_chords(st)
            elif i == 6:                             # 7 : type Harm / Set Maj-Min / banque User
                if st.chord_mode == 'HARM':
                    st.harm_type = (st.harm_type + n) % len(HARM_TYPES)
                elif st.chord_mode == 'SET':
                    st.chord_set = (st.chord_set + n) % len(CHORD_SETS)
                else:
                    st.user_bank = (st.user_bank + n) % USER_BANKS
            else:                                    # 8 : position (renversement)
                st.position = max(-3, min(3, st.position + n))
            return
        if st.tracker16 and i in st.trk_knobs:
            knob = st.trk_knobs[i]
        elif st.melodic() and i in st.kb_knobs:
            knob = st.kb_knobs[i]
        else:
            knob = st.knobs[i]
        n = knob.steps(ev[2])
        if not n:
            return
        if i == 7 and st.melodic() and not st.tracker16:   # Keyboard / Chords : octave
            st.octave = max(0, min(7, st.octave + n))
        elif i == 7:                                 # encodeur 8 : longueur du pattern
            st.length = max(1, min(ROWS, st.length + n))
            st.cursor = min(st.cursor, st.length - 1)
        elif st.tracker16:
            if i == 0:
                st.cursor = max(0, min(st.length - 1, st.cursor + n))
            elif i == 1:
                st.sound = max(1, min(16, st.sound + n))
            elif i in (2, 3):
                edit_cell(st, i == 2, n)
        elif i < 4:
            vals = st.params[(st.group, st.sound)]
            vals[i] = max(0, min(127, vals[i] + n))
        elif st.right_view == 'keyboard':
            if i == 4:                               # 5 : gamme (liste deroulante)
                choose_scale(st, n)
            elif i == 5:                             # 6 : root
                st.root = (st.root + n) % 12
        elif i == 4:
            st.cursor = max(0, min(st.length - 1, st.cursor + n))
        elif i in (5, 6):
            edit_cell(st, i == 5, n)


def choose_scale(st: State, step: int) -> None:
    """Encodeur 5 : parcourt la liste unique des gammes (categories Main, Major, Minor...)
    sans changer celle qui joue ; appliquee apres SCALE_DELAY s sans mouvement."""
    idx = SCALE_ITEMS.index(st.pending_scale or (st.bank_idx, st.type_idx))
    st.pending_scale = SCALE_ITEMS[(idx + step) % len(SCALE_ITEMS)]
    st.scale_deadline = time.monotonic() + SCALE_DELAY


def apply_pending_scale(st: State, force: bool = False) -> bool:
    if st.pending_scale and (force or time.monotonic() >= st.scale_deadline):
        st.bank_idx, st.type_idx = st.pending_scale
        st.pending_scale = None
        return True
    return False


def out_vel(st: State, group: str, sound: int, vel: int) -> int:
    """Velocite envoyee au moteur, apres volumes du son, du groupe et master."""
    v = vel * st.sound_vol[(group, sound)] / 100 * st.group_vol[group] / 100 * st.master / 100
    return max(0, min(127, round(v)))


def nr_interval(st: State) -> float:
    return 60 / st.bpm / NR_RATES[st.nr_rate][1]


def arp_sequence(st: State, base: list) -> list:
    """Ordre de lecture de l'arpegiateur selon le sens choisi et le nombre d'octaves."""
    sign = 1 if st.arp_oct >= 0 else -1              # octaves ajoutees au-dessus ou au-dessous
    seq = [(n + sign * 12 * o, snd, v) for o in range(abs(st.arp_oct) + 1) for n, snd, v in base]
    seq = [x for x in seq if 0 <= x[0] <= 127]
    seq = sorted(set(seq))
    d = ARP_DIRS[st.arp_dir]
    if d == 'Down':
        seq.reverse()
    elif d == 'Up&Down' and len(seq) > 2:
        seq = seq + seq[-2:0:-1]
    elif d == 'Down&Up' and len(seq) > 2:
        rev = seq[::-1]
        seq = rev + rev[-2:0:-1]
    return seq


def repeat_tick(st: State) -> None:
    """Note Repeat (NOTE REPEAT tenu, ou LOCK) : les pads tenus se rejouent a la vitesse
    choisie. Arpegiateur (mode ARP) : les notes tenues jouees une par une, selon le sens."""
    if not st.nr_active or not st.held_play:
        return
    now = time.monotonic()
    if now < st.nr_next:
        return
    st.nr_next = now + nr_interval(st)
    held = list(st.held_play.values())
    if st.arp:
        base = sorted({(n, s, v) for _, v, lst in held for s, n in lst})
        if not base:
            return
        seq = arp_sequence(st, base)
        if ARP_DIRS[st.arp_dir] == 'Random':
            n, snd, vel = random.choice(seq)
        else:
            n, snd, vel = seq[st.arp_index % len(seq)]
        st.arp_index += 1
        v = out_vel(st, st.group, snd, vel)
        if v:
            SOUND.hit(snd, v, n)
        return
    for _, vel, lst in held:
        for snd, n in lst:
            v = out_vel(st, st.group, snd, vel)
            if v:
                SOUND.hit(snd, v, n)


def tap_tempo(st: State) -> None:
    """Chaque appui sur TEMPO est un 'tap' : le tempo suit l'ecart moyen des derniers taps."""
    now = time.monotonic()
    st.taps = [t for t in st.taps if now - t < 2.0] + [now]   # au-dela de 2 s : on recommence
    st.taps = st.taps[-5:]
    if len(st.taps) >= 2:
        gaps = [b - a for a, b in zip(st.taps, st.taps[1:])]
        st.bpm = max(40, min(240, round(60 / (sum(gaps) / len(gaps)))))


def edit_cell(st: State, is_note: bool, n: int) -> None:
    """Change la note (demi-tons) ou le volume de la case sous le curseur, si elle existe."""
    cell = st.grid[st.cursor][st.sound - 1]
    if not cell:
        return
    snapshot(st, coalesce=True)
    note, vol = cell
    if is_note and isinstance(note, tuple):           # accord : tout l'accord est transpose
        if all(0 <= x + n <= 119 for x in note):
            note = tuple(x + n for x in note)
    elif is_note:
        note = max(0, min(119, note + n))
    else:
        vol = max(1, min(127, vol + n))
    st.grid[st.cursor][st.sound - 1] = (note, vol)


def tick(st: State) -> bool:
    """Fait avancer la lecture : 4 lignes par temps. Renvoie True si une ligne a joue."""
    if not st.playing:
        return False
    now = time.monotonic()
    moved = False
    while now >= st.next_row_time:
        st.playhead %= st.length                            # si la longueur a ete reduite
        for g in GROUPS:                                    # tous les groupes qui sonnent
            if not st.audible(g):
                continue
            current = g == st.group
            for t, cell in enumerate(st.grids[g][st.playhead]):   # toutes les pistes
                if not cell:
                    continue
                note, vol = cell
                if current:
                    st.flash[t + 1] = now + 0.08
                v = out_vel(st, g, t + 1, vol)
                for n in cell_notes(note):               # une note ou un accord
                    if v:
                        SOUND.hit(t + 1, v, n)
                    if current and t + 1 == st.sound:   # le clavier montre le son choisi
                        st.note_flash[n] = (now + 0.15, vol)
        st.playhead = (st.playhead + 1) % st.length
        st.next_row_time += 60 / st.bpm / 4
        moved = True
    return moved


# Retro-eclairage des boutons (rapport 0x80, 63 octets) - emplacements et
# niveaux repris du pilote r22yb3m (MIT). Boutons mono : 4 = doux, 7 = vif.
# Groupes A-H : couleurs de la palette NI, (douce, vive) pour chaque groupe.
BUTTON_SLOTS = {'tempo': 23, 'play': 41, 'rec': 42, 'stop': 43, 'shift': 44, 'pad_mode': 46, 'keyboard': 47,
                'chords': 48, 'step': 49, 'nav_prev': 6, 'nav_next': 7,
                'mute': 57, 'solo': 56,
                'mixer': 3, 'volume': 20,  # MIXER / VOLUME : emplacements supposes (21 = SWING)
                'screen_3': 14, 'screen_5': 16, 'screen_6': 17, 'screen_7': 18, 'screen_8': 19,
                'midi_channel': 0, 'follow': 40, 'note_repeat': 22, 'lock': 24}
                # LOCK : emplacement suppose (apres TEMPO 23) ; verifier avec --led-scan
                # FOLLOW et NOTE REPEAT : emplacements de lumiere supposes, a confirmer   # SOLO : emplacement suppose (a cote de MUTE), a confirmer   # STEP : emplacement suppose (apres CHORDS 48), a confirmer
SCREEN_IDLE, SCREEN_ACTIVE = 12, 50     # niveaux des boutons d'ecran (pilote r22yb3m)
# Palette NI : 16 teintes, 4 nuances chacune ; teinte n -> indices 4+4n a 7+4n
# (+1 = douce, +2 = vive, +3 = pale). Le pilote d'origine melangeait deux teintes
# pour E, G et H (bouton jaune puis vert clair) : corrige ici.
# Couleurs des groupes comme sur la Maschine+, choisies au nuancier (--palette) le 18/11 :
# numero de pad du nuancier - 1 = teinte.  A 2, B 3, C 6, D 7, E 10, F 13, G 14, H 16
GROUP_HUES = tuple(p - 1 for p in (2, 3, 6, 7, 10, 13, 14, 16))
GROUP_PALETTE = tuple((5 + 4 * h, 6 + 4 * h) for h in GROUP_HUES)
GROUP_SLOT = 29
# Couleur d'ecran de chaque groupe, proche de celle de son bouton (meme ordre).
GROUP_RGB = ((235, 35, 25), (252, 95, 0), (235, 200, 40), (0, 200, 165),
             (25, 90, 240), (105, 55, 240), (230, 40, 225), (240, 20, 70))
BTN_DIM, BTN_BRIGHT = 4, 7
# Couleur d'ecran approximative de chacune des 16 teintes de la palette NI (Settings > couleurs).
# Les teintes des groupes reprennent GROUP_RGB ; les autres sont estimees.
HUE_RGB = [(230, 30, 60), (235, 35, 25), (252, 95, 0), (250, 150, 30), (245, 180, 30),
           (235, 200, 40), (0, 200, 165), (0, 210, 120), (0, 180, 230), (25, 90, 240),
           (60, 60, 235), (85, 55, 240), (105, 55, 240), (230, 40, 225), (240, 60, 150),
           (240, 20, 70)]
SETTINGS_FILE = Path(__file__).with_name('fuzzy_settings.json')
# Vitesses (notes par temps) ; T = triolet
NR_RATES = [('1/4', 1), ('1/4T', 1.5), ('1/8', 2), ('1/8T', 3), ('1/16', 4), ('1/16T', 6),
            ('1/32', 8), ('1/32T', 12)]
ARP_DIRS = ['Up', 'Down', 'Up&Down', 'Down&Up', 'Random']


def available_buttons(st: State) -> set:
    """Boutons qui ont une fonction dans le mode en cours (les autres restent eteints)."""
    on = {'play', 'rec', 'stop', 'shift', 'pad_mode', 'keyboard', 'chords', 'step', 'mixer',
          'volume', 'tempo', 'mute', 'solo', 'follow', 'note_repeat'}
    on.add('midi_channel')                             # Shift + MIDI : SETTINGS
    if st.nr_on:
        on.add('lock')                                 # LOCK : utile quand Note Repeat tourne
    if st.set_confirm:
        return on | {'screen_5', 'screen_6'}           # YES / NO
    if st.nr_menu:
        return on                                      # menu Note Repeat : tout aux encodeurs
    if st.step_mode or st.mixer:
        on |= {'nav_prev', 'nav_next'}                 # < > : pages STEP ou pages du mixeur
    if not st.mixer:
        on.add('screen_3')                             # TRACKER
        if st.right_view == 'chords' and st.chord_mode == 'USER':
            on.add('screen_8')                         # EDIT des accords User
        if st.melodic():
            on.add('screen_7')                         # LINK des tonalites
    return on


def button_frame(st: State) -> bytes:
    """Boutons qui ont une fonction dans le mode : doux ; fonction active ou tenu : vif ;
    sans fonction : eteint."""
    f = bytearray(63)
    f[0] = 0x80
    active = {'play': st.playing, 'rec': st.recording, 'stop': not st.playing,
              'shift': 'shift' in st.held, 'keyboard': st.right_view == 'keyboard',
              'pad_mode': not st.melodic(), 'chords': st.right_view == 'chords',
              'step': st.step_mode, 'tempo': False, 'nav_prev': False, 'nav_next': False,
              'mute': bool(st.muted), 'solo': bool(st.solo),
              'mixer': st.mixer, 'volume': False, 'screen_3': st.tracker16,
              'screen_8': st.user_edit, 'screen_7': st.link_edit,
              'midi_channel': st.settings, 'follow': st.follow,
              'note_repeat': st.nr_active, 'lock': st.nr_lock,
              'screen_5': False, 'screen_6': False}
    for name in available_buttons(st):
        slot = BUTTON_SLOTS[name]
        lit = active[name] or name in st.held
        if name.startswith('screen_'):
            f[1 + slot] = SCREEN_ACTIVE if lit else SCREEN_IDLE
        else:
            f[1 + slot] = BTN_BRIGHT if lit else BTN_DIM
    for i, g in enumerate(GROUPS):
        dim, bright = hue_pair(st.group_hue[g])
        if st.link_edit:                               # LINK : groupes lies vifs, autres doux
            f[1 + GROUP_SLOT + i] = bright if (g in st.linked or g == st.group) else dim
            continue
        if not st.audible(g):
            f[1 + GROUP_SLOT + i] = 0                  # groupe coupe (Mute, ou hors Solo) : eteint
        else:
            f[1 + GROUP_SLOT + i] = bright if g == st.group else dim
    return bytes(f)


STEP_PLAYHEAD = 69          # blanc doux (teinte 16 de la palette NI) : pas en cours de lecture


def led_frame(st: State) -> bytes:
    """Pads dans la couleur du groupe : douce au repos, vive si frappe, joue ou choisi.
    Mode STEP : pas avec note = vif, pas vide = eteint, pas en lecture = blanc."""
    now = time.monotonic()
    dim, bright = hue_pair(st.group_hue[st.group])
    if st.settings:
        if st.set_target == 'GROUP':                   # pad n = teinte n (vive)
            return pad_led_frame({p: hue_pair(p - 1)[1] for p in range(1, 17)})
        return pad_led_frame({p: pad_pair(st, p)[1 if p in st.set_selected else 0]
                              for p in range(1, 17)})
    if st.melodic() and not st.step_mode:
        shift = 'shift' in st.held
        colors = {}
        for p in range(1, 17):
            if shift:                                  # Shift : pads 13-16 = root / octave
                colors[p] = bright if p >= 13 else 0
            elif st.right_view == 'chords' and st.user_edit:
                on = (p - 1) in st.user_chords[st.user_bank][st.user_slot - 1]
                colors[p] = bright if on else dim
            elif st.right_view == 'chords' and st.chord_mode != 'HARM' and (
                    p > 12 or not st.chord_notes(p)):
                colors[p] = 0                          # SET / USER : pads 1-12 (accords definis)
            else:
                colors[p] = bright if (p in st.down or st.flash.get(p, 0) > now) else dim
        return pad_led_frame(colors)
    if st.step_mode:
        page = step_page(st) * 16
        playing = play_row(st)
        colors = {}
        for p in range(1, 17):
            r = page + step_of_pad(p)
            if r >= st.length:
                colors[p] = 0                          # au-dela de la longueur du pattern
            elif r == playing:
                colors[p] = STEP_PLAYHEAD
            elif st.grid[r][st.sound - 1]:
                colors[p] = bright
            else:
                colors[p] = 0
        return pad_led_frame(colors)
    return pad_led_frame({p: pad_pair(st, p)[1] if (p in st.down or st.flash.get(p, 0) > now
                                                   or p == st.sound) else pad_pair(st, p)[0]
                          for p in range(1, 17)})


def led_scan(dev, screens, dec) -> int:
    """--led-scan : allume UN emplacement de lumiere de bouton a la fois (rapport 0x80).
    Encodeur 1 : emplacement suivant / precedent. Sert a trouver la lumiere d'un bouton."""
    slot, knob = 0, Knob(20)

    def show():
        f = bytearray(63); f[0] = 0x80; f[1 + slot] = 63
        dev.write(bytes(f))
        img = Image.new('RGB', (W, H), 'black')
        d = ImageDraw.Draw(img)
        header(d, 'LED SCAN', 'encoder 1 = next', (252, 95, 0))
        d.text((150, 80), f'slot {slot}', font=font(48), fill='white')
        d.text((40, 200), 'Which button is lit? Note the slot number.', font=font(14), fill=(180, 180, 180))
        screens.send(0, img)
        print(f'emplacement {slot} allume')
    show()
    try:
        while True:
            data = dev.read(50)
            for ev in dec.parse(data) if data else []:
                if ev[0] == 'knob' and ev[1] == 1:
                    n = knob.steps(ev[2])
                    if n:
                        slot = max(0, min(61, slot + n))
                        show()
    except KeyboardInterrupt:
        pass
    finally:
        try: dev.write(bytes([0x80]) + bytes(62))
        except Exception: pass
        dev.close(); screens.close()
    return 0


ENGINE = Path(__file__).resolve().parent.parent / 'pad-sound' / 'build' / \
    'fuzzy_pad_sound_artefacts' / 'Release' / 'fuzzy_pad_sound'


def start_engine(output: str | None):
    """Demarre le moteur audio APRES avoir pris la Maschine : sa carte son a disparu, le
    moteur choisit donc une autre sortie. Il tourne avec ton compte (pas en administrateur)."""
    if not ENGINE.exists():
        print(f'Moteur audio introuvable : {ENGINE}')
        print('Compile-le : cd prototypes/pad-sound && cmake --build build   (interface sans son)')
        return None
    cmd = [str(ENGINE), '--osc', '--quiet'] + (['--output', output] if output else [])
    uid, gid = os.environ.get('SUDO_UID'), os.environ.get('SUDO_GID')

    def drop_root():                                   # lance sous sudo : revenir a ton compte
        os.setsid()                                    # Ctrl+C est gere par l'interface
        if gid and uid:
            os.setgid(int(gid))
            os.setuid(int(uid))
    env = dict(os.environ)
    if os.environ.get('SUDO_USER'):
        env['HOME'] = os.path.expanduser('~' + os.environ['SUDO_USER'])
        env['USER'] = env['LOGNAME'] = os.environ['SUDO_USER']
    try:
        proc = subprocess.Popen(cmd, preexec_fn=drop_root, env=env)
    except OSError as e:
        print('Impossible de demarrer le moteur audio :', e)
        return None
    time.sleep(0.8)                                    # le temps d'ouvrir la sortie audio
    if proc.poll() is not None:
        print('Le moteur audio s\'est arrete au demarrage (voir le message ci-dessus).')
        return None
    return proc


def stop_engine(proc) -> None:
    if proc and proc.poll() is None:
        proc.send_signal(signal.SIGINT)                # arret propre (fondu, fermeture audio)
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()


def palette_mode(dev, screens, dec) -> int:
    """Nuancier : pad n = teinte n-1 en vif. Frapper un pad l'affiche en doux / vif / pale."""
    shown = {p: 6 + 4 * (p - 1) for p in range(1, 17)}
    def draw():
        img = Image.new('RGB', (W, H), 'black')
        d = ImageDraw.Draw(img)
        header(d, 'MASCHINE PALETTE', 'pad = hue')
        for i in range(16):
            pad = 13 + i % 4 - 4 * (i // 4)
            x, y = 8 + (i % 4) * 117, 34 + (i // 4) * 58
            d.rectangle((x, y, x + 110, y + 52), outline=(90, 90, 90), width=2)
            d.text((x + 6, y + 16), f'Pad {pad}', font=font(15), fill='white')
        return img
    img = draw()
    screens.send(0, img); screens.send(1, img)
    print('Nuancier : chaque pad montre une teinte. Frappe un pad pour voir ses nuances '
          '(douce, vive, pale). Ctrl+C pour quitter.')
    level = {p: 2 for p in range(1, 17)}
    try:
        dev.write(pad_led_frame(shown))
        while True:
            data = dev.read()
            for ev in dec.parse(data) if data else []:
                if ev[0] == 'pad_on':
                    p = ev[1]
                    level[p] = level[p] % 3 + 1          # 1 douce -> 2 vive -> 3 pale
                    shown[p] = 4 + 4 * (p - 1) + level[p]
                    print(f'Pad {p:2d} : nuance '
                          f'{("douce", "vive", "pale")[level[p] - 1]}, code {shown[p]}')
                    dev.write(pad_led_frame(shown))
            if not data:
                time.sleep(0.005)
    except KeyboardInterrupt:
        pass
    finally:
        try: dev.write(pad_led_frame({}))
        except Exception: pass
        dev.close(); screens.close()
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description='Fuzzy Turtle - mini DAW Pulses sur Maschine MK3')
    ap.add_argument('--preview', action='store_true', help='sans Maschine : enregistrer des PNG')
    ap.add_argument('--curve', choices=CURVES, default='soft')
    ap.add_argument('--no-sound', action='store_true',
                    help='ne pas demarrer le moteur audio (fuzzy_pad_sound)')
    ap.add_argument('--output', metavar='NOM',
                    help='sortie audio du moteur (ex. "Haut-parleurs"), sinon automatique')
    ap.add_argument('--vert', type=int, choices=range(1, 9), metavar='1-8',
                    help="correction de couleur des ecrans (voir mk3_screens.py couleurs)")
    ap.add_argument('--led-scan', action='store_true',
                    help="allumer les lumieres des boutons une par une (calibration)")
    ap.add_argument('--palette', action='store_true',
                    help='nuancier : les 16 pads montrent les 16 teintes de la Maschine')
    a = ap.parse_args()
    if a.vert:
        import mk3_screens
        mk3_screens.GREEN_GAMMA = mk3_screens.GREEN_GAMMAS[a.vert - 1]
    st = State()

    if a.preview:
        st.recording, st.cursor = True, 4
        Preview().send(0, draw_left(st))
        Preview().send(1, draw_tracker(st))
        st.right_view, st.sound = 'keyboard', 1
        st.held_notes = {1: [(60, 110)], 3: [(63, 80)], 5: [(67, 100)], 7: [(70, 60)]}
        draw_keyboard(st).save('mk3_ecran_clavier.png')
        print('Apercu enregistre : mk3_ecran_clavier.png')
        st.right_view, st.tracker16, st.cursor = 'tracker', True, 4
        draw_left(st).save('mk3_tracker_gauche.png'); draw_right(st).save('mk3_tracker_droit.png')
        print('Apercus enregistres : mk3_tracker_gauche.png, mk3_tracker_droit.png')
        return 0

    try:
        screens = Screens(detach=True)
        dev = UsbHid(screens)
    except MK3Error as e:
        print('Erreur :', e)
        print('As-tu lance avec sudo ? Sinon, debranche / rebranche la Maschine.')
        if 'screens' in locals():
            screens.close()
        return 1
    dec = Decoder(CURVES[a.curve])
    if a.palette:
        return palette_mode(dev, screens, dec)
    if a.led_scan:
        return led_scan(dev, screens, dec)
    engine = None if a.no_sound else start_engine(a.output)
    print('Mini DAW Pulses lance. Ctrl+C pour quitter.')

    # Trois taches en parallele, pour que le dessin des ecrans ne retarde jamais le jeu :
    #  - lecteur : lit la Maschine en continu et declenche le son TOUT DE SUITE ;
    #  - lecture du pattern : avance au rythme du tempo ;
    #  - boucle principale : etat, lumieres et ecrans (peut prendre ~30 ms par image).
    lock = threading.Lock()
    events: queue.Queue = queue.Queue()
    running = threading.Event(); running.set()

    def reader():
        while running.is_set():
            data = dev.read(50)
            if not data:
                continue
            for ev in dec.parse(data):
                if ev[0] == 'pad_on':
                    # Lecture simple de l'etat, sans attendre le verrou (pris pendant le dessin)
                    notes = sound_for(st, ev[1])
                    st.held_play[ev[1]] = (time.monotonic(), ev[2], notes)
                    if st.arp and notes:                 # Arp : c'est l'arpegiateur qui joue
                        if len(st.held_play) == 1:
                            st.arp_index = 0
                        st.nr_next = min(st.nr_next, time.monotonic()) if len(st.held_play) > 1 \
                            else time.monotonic()
                        notes = []
                    elif st.nr_active and notes:
                        st.nr_next = time.monotonic() + nr_interval(st)
                    for snd, note in notes:
                        v = out_vel(st, st.group, snd, ev[2])
                        if v:
                            SOUND.hit(snd, v, note)
                elif ev[0] == 'pad_off':
                    st.held_play.pop(ev[1], None)
                events.put(ev)

    def player():
        while running.is_set():
            with lock:
                if tick(st):
                    events.put(('redraw',))
                repeat_tick(st)
            time.sleep(0.001)

    threads = [threading.Thread(target=reader, daemon=True), threading.Thread(target=player, daemon=True)]
    for t in threads:
        t.start()

    dirty, last_draw, last_leds, last_buttons = True, 0.0, b'', b''
    try:
        while True:
            try:
                ev = events.get(timeout=0.01)
                while True:                               # vider tout ce qui attend
                    with lock:
                        if ev[0] != 'redraw':
                            handle(st, ev)
                    dirty = True
                    ev = events.get_nowait()
            except queue.Empty:
                pass
            now = time.monotonic()
            with lock:
                if (any(t > now - 0.1 for t in st.flash.values())
                        or any(e > now - 0.1 for e, _ in st.note_flash.values())
                        or st.pending_scale):            # liste des gammes : compte a rebours
                    dirty = True
                if apply_pending_scale(st):
                    dirty = True
                leds, buttons = led_frame(st), button_frame(st)
                images = None
                if dirty and now - last_draw >= 1 / 25:   # 25 images/s maximum
                    images = draw_left(st), draw_right(st)
            if leds != last_leds:
                dev.write(leds); last_leds = leds
            if buttons != last_buttons:
                dev.write(buttons); last_buttons = buttons
            if images:                                    # envoi USB hors du verrou
                screens.send(0, images[0])
                screens.send(1, images[1])
                dirty, last_draw = False, now
    except KeyboardInterrupt:
        pass
    except MK3Error as e:
        print('Erreur :', e)
    finally:
        running.clear()
        for t in threads:
            t.join(timeout=0.5)
        stop_engine(engine)
        save_settings(st)                              # tonalites et liens des groupes
        try:
            dev.write(pad_led_frame({}))
            dev.write(bytes([0x80]) + bytes(62))       # eteindre les boutons
        except Exception:
            pass
        black = Image.new('RGB', (W, H), 'black')
        try:
            screens.send(0, black); screens.send(1, black)
        except MK3Error:
            pass
        dev.close()
        screens.close()
        print('\nArrete. Debranche / rebranche la Maschine pour rendre les ecrans au pilote NI.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
