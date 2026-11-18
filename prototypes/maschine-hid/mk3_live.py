#!/usr/bin/env python3
"""Fuzzy Turtle - prototype 3 : pads + encodeurs + ecrans ensemble (mini DAW Pulses).

Modes, comme sur Maschine :
  PAD MODE      chaque pad joue un son du groupe (mode par defaut)
                ecran gauche : les 16 sons et 4 reglages ; ecran droit : la piste du son choisi
  Bouton 6      (au-dessus de l'ecran droit, Pad Mode et Keyboard) : vue TRACKER 16 pistes
                sur les deux ecrans ; un 2e appui revient a la vue precedente
  TEMPO         taper plusieurs fois en rythme = tap tempo ; TEMPO tenu + molette 4D = +/- 1 BPM
  Molette 4D    tourner : ligne du curseur ; pousser haut/bas : ligne ; gauche/droite : piste
  Encodeur 8    longueur du pattern (1 a 64 lignes), dans tous les modes
  CHORDS        pads 1-12 = les 12 accords du Chord Set (16 sets : Maj 1-8, Min 1-8) avec le
                son choisi ; encodeur 5 root, 6 Chord Set, 8 octave ; Shift + pads 13-16 comme
                en Keyboard. En REC, l'accord entier s'ecrit dans une case du tracker.
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
                Keyboard : 5 root, 6 banque de gammes, 7 gamme, 8 octave ; la nouvelle gamme
                           s'affiche et ne s'applique qu'au clic de la molette 4D (jeu live) ;
                           Shift + pad 13 / 14 : root -/+ ; Shift + pad 15 / 16 : octave -/+

Son : lancer d'abord le moteur C++ dans un autre terminal :
         prototypes/pad-sound/build/fuzzy_pad_sound_artefacts/Release/fuzzy_pad_sound --osc
      mk3_live.py lui envoie chaque note par OSC (port 57390). Sans moteur : interface muette.

Usage :  sudo "$(which python3)" mk3_live.py
         python3 mk3_live.py --preview     (sans Maschine : enregistre des PNG)
A la fin : debrancher / rebrancher la Maschine pour rendre les ecrans au pilote NI.
Necessite mk3_probe.py et mk3_screens.py dans le meme dossier (hidapi n'est pas utilise ici).
"""
from __future__ import annotations
import argparse, queue, socket, struct, sys, threading, time

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
_DEGREE = {'1': 0, '#1': 1, 'b2': 1, '2': 2, '#2': 3, 'b3': 3, '3': 4, 'b4': 4, '4': 5,
           '#4': 6, 'b5': 6, '5': 7, '#5': 8, 'b6': 8, '6': 9, '#6': 10, 'b7': 10, '7': 11}
BANKS = [(bank, [(name, tuple(_DEGREE[d] for d in formula.split())) for name, formula in scales])
         for bank, scales in _BANKS.items()]
CHORDS = [((0, 4, 7, 11), 'maj7'), ((0, 3, 7, 10), 'm7'), ((0, 4, 7, 10), '7'),
          ((0, 3, 6, 10), 'm7b5'), ((0, 4, 7), ''), ((0, 3, 7), 'm'), ((0, 3, 6), 'dim'),
          ((0, 4, 8), 'aug'), ((0, 2, 7), 'sus2'), ((0, 5, 7), 'sus4'), ((0, 7), '5')]
KEY_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']


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
        self.octave, self.root = 3, 0                         # a partir de C-3
        self.bank_idx, self.type_idx = 0, 2                   # Main / Minor
        self.held_notes: dict[int, list[tuple[int, int]]] = {}   # pad -> [(note, velocite)]
        self.chord_set = 0                                    # mode CHORDS : Chord Set choisi
        self.note_flash: dict[int, tuple[float, int]] = {}   # note -> (fin, velocite)
        self.held: set[str] = set()
        self.flash: dict[int, float] = {}            # pad -> fin de l'allumage
        self.knobs = [Knob(8), Knob(8), Knob(8), Knob(8), Knob(12), Knob(14), Knob(6), Knob(20)]
        # Vue Clavier : octave, root et gamme demandent un vrai tour d'encodeur par cran
        # Keyboard : 5 root, 6 banque, 7 type de gamme (un vrai tour d'encodeur par cran)
        self.kb_knobs = {4: Knob(40), 5: Knob(50), 6: Knob(40), 7: Knob(45)}
        # Vue 16 pistes : ligne, piste, note, volume
        self.trk_knobs = {0: Knob(12), 1: Knob(25), 2: Knob(14), 3: Knob(6)}

    @property
    def grid(self):
        """Le pattern du groupe choisi."""
        return self.grids[self.group]

    def audible(self, g: str) -> bool:
        """Un groupe joue s'il n'est pas coupe et, si un Solo existe, s'il en fait partie."""
        return g not in self.muted and (not self.solo or g in self.solo)

    def melodic(self) -> bool:
        """Keyboard ou Chords : les pads jouent des notes avec le son choisi."""
        return self.right_view in ('keyboard', 'chords')

    def chord_notes(self, pad: int) -> tuple[int, ...]:
        """Mode CHORDS : notes de l'accord du pad (pads 1-12), transpose en root / octave."""
        if pad > 12:
            return ()
        chord = CHORD_SETS[self.chord_set][1][pad - 1]
        return chord.midi_notes(12 * (self.octave + 1) + self.root)

    def chord_label(self, pad: int) -> str:
        return CHORD_SETS[self.chord_set][1][pad - 1].display(self.root) if pad <= 12 else ''

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
    """Couleur du theme sur les ecrans : celle du groupe choisi."""
    return GROUP_RGB[GROUPS.index(st.group)]


def shade(color, k: float):
    return tuple(round(c * k) for c in color)


def draw_left(st: State) -> Image.Image:
    if st.tracker16:
        return draw_tracks(st, 0)
    acc = group_rgb(st)
    img = Image.new('RGB', (W, H), 'black')
    d = ImageDraw.Draw(img)
    info = (f'SOLO {"".join(sorted(st.solo))}  ' if st.solo else '') + \
           (f'MUTE {"".join(sorted(st.muted))}  ' if st.muted else '')
    header(d, f'FUZZY TURTLE  -  Group {st.group}', f'{info}{st.bpm} BPM', acc)
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
    d.text((10, 248), f'{state}   Shift+pad = clear', font=font(12), fill=(140, 140, 140))
    tempo_overlay(st, d)
    return img


def tempo_overlay(st: State, d: ImageDraw.ImageDraw) -> None:
    """TEMPO tenu : grand affichage du tempo (molette 4D pour l'ajuster)."""
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
    return (play_row(st) if st.playing else st.cursor) // 16


def view_top(st: State, visible: int) -> int:
    """Premiere ligne affichee : suit la ligne qui joue pendant la lecture, sinon le curseur."""
    center = play_row(st) if st.playing else st.cursor
    return max(0, min(center - visible // 2, st.length - visible))


def softkey_header(d: ImageDraw.ImageDraw, st: State, right: str) -> None:
    """Bandeau de l'ecran droit : nom du mode a gauche (PAD MODE ou KEYBOARD), puis
    l'etiquette du bouton 6 (TRACKER) : sombre = disponible, blanche = vue Tracker active."""
    acc = group_rgb(st)
    header(d, {'keyboard': 'KEYBOARD', 'chords': 'CHORDS'}.get(st.right_view, 'PAD MODE'), right, acc)
    x0, x1 = 124, 232                              # sous le bouton 6
    if st.tracker16:
        d.rectangle((x0, 3, x1, 23), fill='white')
        d.text((x0 + 10, 4), 'TRACKER', font=font(16), fill='black')
    else:
        d.rectangle((x0, 3, x1, 23), fill=shade(acc, 0.35), outline=(20, 20, 20))
        d.text((x0 + 10, 4), 'TRACKER', font=font(16), fill=(235, 235, 235))


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
        header(d, 'TRACKER  Pattern 01', f'{rec}Group {st.group}  -  tracks 1-8', acc)
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
        softkey_header(d, st, step_label(st) + f'Set {CHORD_SETS[st.chord_set][0]}')
        scale = ()                                 # pas de points de gamme en mode Chords
    else:
        softkey_header(d, st, step_label(st) + f'Oct {st.octave}  {KEY_NAMES[st.root]} {scale_name}')
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
        labels = [f'Root {KEY_NAMES[st.root]}', f'Set {CHORD_SETS[st.chord_set][0]}', '',
                  f'Octave {st.octave}']
    else:
        labels = [f'Root {KEY_NAMES[st.root]}', f'Bank {BANKS[st.bank_idx][0]}', scale_name,
                  f'Octave {st.octave}']
    for i, t in enumerate(labels):
        d.text((i * 120 + 10, 254), t, font=font(12), fill=(180, 180, 180))
    if st.pending_scale and not chords:            # gamme choisie mais pas encore appliquee
        b, t = st.pending_scale
        msg = f'Next: {BANKS[b][0]} / {BANKS[b][1][t][0]}   (4D click = apply)'
        d.rectangle((0, 34, W, 70), fill=(15, 15, 20))
        d.text((10, 44), msg, font=font(14), fill='white')
    return img




def draw_right(st: State) -> Image.Image:
    if st.tracker16:
        return draw_tracks(st, 8)
    if st.right_view == 'chords':
        return draw_keyboard(st) if st.chord_keyboard else draw_chords(st)
    return draw_keyboard(st) if st.right_view == 'keyboard' else draw_tracker(st)


def draw_chords(st: State) -> Image.Image:
    """Mode CHORDS : les 12 accords du Chord Set, disposes comme les pads 1-12."""
    acc = group_rgb(st)
    img = Image.new('RGB', (W, H), (12, 12, 16))
    d = ImageDraw.Draw(img)
    softkey_header(d, st, step_label(st) + f'Set {CHORD_SETS[st.chord_set][0]}')
    for i in range(12):
        pad = 9 + i % 4 - 4 * (i // 4)               # pads 9-12 en haut, 1-4 en bas
        x, y = 8 + (i % 4) * 117, 34 + (i // 4) * 52
        lit = pad in st.down
        d.rectangle((x, y, x + 110, y + 46), fill=shade(acc, 0.45) if lit else (24, 24, 30),
                    outline=acc if lit else (70, 70, 70), width=2)
        d.text((x + 6, y + 4), f'{pad:02d}', font=font(11), fill=(140, 140, 140))
        d.text((x + 8, y + 19), st.chord_label(pad), font=font(17), fill='white')
    notes = st.active_notes()
    names = ' '.join(note_name(n) for n in sorted(notes)) if notes else ''
    d.text((10, 196), names, font=font(14), fill=acc)
    labels = [f'Root {KEY_NAMES[st.root]}', f'Set {CHORD_SETS[st.chord_set][0]}', '',
              f'Octave {st.octave}']
    for i, t in enumerate(labels):
        d.text((i * 120 + 10, 254), t, font=font(12), fill=(180, 180, 180))
    return img


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
    if 'shift' in st.held:
        return []
    if st.step_mode:                       # sans REC : on ecoute le son ; avec REC : seulement
        if not st.recording or st.grid[step_row(st, pad)][st.sound - 1] is None:   # en ajoutant
            return [(st.sound, n) for n in cell_notes(step_note(st))]
        return []
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
        if 'tempo' in st.held:                       # TEMPO tenu + molette : tempo au BPM pres
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
            else:
                st.group = g
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
            if st.sound <= 4:                        # sons 1-4 = batterie : prendre un son melodique
                st.sound = 5
            st.right_view, st.tracker16, st.step_mode = 'keyboard', False, False
            st.held_notes.clear()
        elif name == 'chords' and 'shift' in st.held:   # Shift + CHORDS : clavier des accords
            st.chord_keyboard = not st.chord_keyboard
        elif name == 'chords':
            if st.sound <= 4:                        # sons 1-4 = batterie : prendre un son melodique
                st.sound = 5
            st.right_view, st.step_mode = 'chords', False
            st.held_notes.clear()
        elif name == 'pad_mode':
            st.right_view, st.tracker16, st.step_mode = 'tracker', False, False
            st.held_notes.clear()
        elif name == 'nav_click' and st.pending_scale:   # clic 4D : appliquer la gamme choisie
            st.bank_idx, st.type_idx = st.pending_scale
            st.pending_scale = None
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
        elif name == 'screen_6':                     # TRACKER, en Pad Mode comme en Keyboard
            st.tracker16 = not st.tracker16          # 2e appui : retour a la vue precedente
        elif name == 'tempo':                        # tap tempo (TEMPO tenu + molette : jog)
            tap_tempo(st)
    elif kind == 'knob':
        i = ev[1] - 1
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
        elif st.right_view == 'chords':
            if i == 4:
                st.root = (st.root + n) % 12
            elif i == 5:                             # encodeur 6 : Chord Set (Maj 1-8, Min 1-8)
                st.chord_set = (st.chord_set + n) % len(CHORD_SETS)
        elif st.right_view == 'keyboard':
            if i == 4:
                st.root = (st.root + n) % 12
            elif i in (5, 6):                        # on CHOISIT une gamme sans la jouer encore :
                b, t = st.pending_scale or (st.bank_idx, st.type_idx)   # clic 4D pour l'appliquer
                if i == 5:
                    b = (b + n) % len(BANKS)
                    t = min(t, len(BANKS[b][1]) - 1)
                else:
                    t = (t + n) % len(BANKS[b][1])
                st.pending_scale = None if (b, t) == (st.bank_idx, st.type_idx) else (b, t)
        elif i == 4:
            st.cursor = max(0, min(st.length - 1, st.cursor + n))
        elif i in (5, 6):
            edit_cell(st, i == 5, n)


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
                for n in cell_notes(note):               # une note ou un accord
                    SOUND.hit(t + 1, vol, n)
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
                'mute': 57, 'solo': 56}   # SOLO : emplacement suppose (a cote de MUTE), a confirmer   # STEP : emplacement suppose (apres CHORDS 48), a confirmer
SCREEN6_SLOT = 17                       # bouton 6 au-dessus des ecrans (TRACKER)
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


def button_frame(st: State) -> bytes:
    """Boutons qui ont une fonction : eclairage doux ; fonction active ou bouton tenu : vif.
    Les boutons sans fonction dans ce prototype restent eteints."""
    f = bytearray(63)
    f[0] = 0x80
    active = {'play': st.playing, 'rec': st.recording, 'stop': not st.playing,
              'shift': 'shift' in st.held, 'keyboard': st.right_view == 'keyboard',
              'pad_mode': not st.melodic(), 'chords': st.right_view == 'chords',
              'step': st.step_mode, 'tempo': False, 'nav_prev': False, 'nav_next': False,
              'mute': bool(st.muted), 'solo': bool(st.solo)}
    for name, slot in BUTTON_SLOTS.items():
        if name in ('nav_prev', 'nav_next') and not st.step_mode:
            continue                                   # < > : utiles seulement en STEP
        f[1 + slot] = BTN_BRIGHT if active[name] or name in st.held else BTN_DIM
    for i, (dim, bright) in enumerate(GROUP_PALETTE):
        g = GROUPS[i]
        if not st.audible(g):
            f[1 + GROUP_SLOT + i] = 0                  # groupe coupe (Mute, ou hors Solo) : eteint
        else:
            f[1 + GROUP_SLOT + i] = bright if g == st.group else dim
    # Bouton 6 : TRACKER, disponible (doux) dans les deux modes, vif quand la vue est ouverte
    f[1 + SCREEN6_SLOT] = SCREEN_ACTIVE if st.tracker16 or 'screen_6' in st.held else SCREEN_IDLE
    return bytes(f)


STEP_PLAYHEAD = 69          # blanc doux (teinte 16 de la palette NI) : pas en cours de lecture


def led_frame(st: State) -> bytes:
    """Pads dans la couleur du groupe : douce au repos, vive si frappe, joue ou choisi.
    Mode STEP : pas avec note = vif, pas vide = eteint, pas en lecture = blanc."""
    now = time.monotonic()
    dim, bright = GROUP_PALETTE[GROUPS.index(st.group)]
    if st.melodic() and not st.step_mode:
        shift = 'shift' in st.held
        colors = {}
        for p in range(1, 17):
            if shift:                                  # Shift : pads 13-16 = root / octave
                colors[p] = bright if p >= 13 else 0
            elif st.right_view == 'chords' and p > 12:
                colors[p] = 0                          # Chords : seuls les pads 1-12 jouent
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
    return pad_led_frame({p: bright if (p in st.down or st.flash.get(p, 0) > now or p == st.sound)
                          else dim for p in range(1, 17)})


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
    ap.add_argument('--vert', type=int, choices=range(1, 9), metavar='1-8',
                    help="correction de couleur des ecrans (voir mk3_screens.py couleurs)")
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
                    for snd, note in sound_for(st, ev[1]):
                        SOUND.hit(snd, ev[2], note)
                events.put(ev)

    def player():
        while running.is_set():
            with lock:
                if tick(st):
                    events.put(('redraw',))
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
                        or any(e > now - 0.1 for e, _ in st.note_flash.values())):
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
