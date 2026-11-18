#!/usr/bin/env python3
"""Fuzzy Turtle - prototype 3 : pads + encodeurs + ecrans ensemble (mini DAW Pulses).

Ecran gauche : les 16 sons du groupe et 4 reglages du son choisi.
Ecran droit  : la vue Tracker (32 lignes) avec curseur et tete de lecture.

Commandes :
  Pads          choisir un son ; en enregistrement (Rec), ecrire une note a la ligne du curseur
  Shift + pad   effacer la ligne du curseur
  Groupes A-H   changer de groupe
  Encodeurs 1-4 reglages du son (Volume, Pan, Pitch, Cutoff)
  Encodeur 5    deplacer le curseur du tracker
  Encodeur 6    note de la ligne (demi-tons)     Encodeur 7  volume de la ligne
  Encodeur 8    tempo (BPM)
  Play / Stop   lecture du pattern (les pads s'allument quand leur note joue)
  Rec           enregistrement on / off
  Eclairage     boutons utiles en doux, fonction active (Play, Rec, Keyboard...) en vif,
                groupe choisi en couleur vive
  Keyboard      ecran droit : vue Tracker <-> vue Clavier
                En vue Clavier : les pads jouent la gamme (pad 1 = fondamentale),
                encodeur 5 = octave, 6 = fondamentale, 7 = gamme, 8 = tempo ;
                le clavier montre les notes tenues et celles du son choisi en lecture.

Aucun son n'est produit : on teste l'interface, pas encore le moteur audio.

Usage :  sudo "$(which python3)" mk3_live.py
         python3 mk3_live.py --preview     (sans Maschine : enregistre des PNG)
A la fin : debrancher / rebrancher la Maschine pour rendre les ecrans au pilote NI.
Necessite mk3_probe.py et mk3_screens.py dans le meme dossier (hidapi n'est pas utilise ici).
"""
from __future__ import annotations
import argparse, sys, time

from PIL import Image, ImageDraw

from mk3_probe import Decoder, CURVES, pad_led_frame
from mk3_screens import Screens, Preview, MK3Error, W, H, font, header

ROWS, VISIBLE = 32, 11
NOTES = ['C-', 'C#', 'D-', 'D#', 'E-', 'F-', 'F#', 'G-', 'G#', 'A-', 'A#', 'B-']
DRUMS = ['Kick', 'Snare', 'Clap', 'HH fer', 'HH ouv', 'Tom 1', 'Tom 2', 'Rim',
         'Crash', 'Ride', 'Shaker', 'Cowbell', 'Perc 1', 'Perc 2', 'FX 1', 'FX 2']
PARAMS = ['Volume', 'Pan', 'Pitch', 'Cutoff']
GROUPS = 'ABCDEFGH'
SCALES = [('majeur', (0, 2, 4, 5, 7, 9, 11)), ('mineur', (0, 2, 3, 5, 7, 8, 10)),
          ('dorien', (0, 2, 3, 5, 7, 9, 10)), ('penta maj', (0, 2, 4, 7, 9)),
          ('penta min', (0, 3, 5, 7, 10)), ('blues', (0, 3, 5, 6, 7, 10)),
          ('chromatique', tuple(range(12)))]
CHORDS = [((0, 4, 7, 11), 'maj7'), ((0, 3, 7, 10), 'm7'), ((0, 4, 7, 10), '7'),
          ((0, 3, 6, 10), 'm7b5'), ((0, 4, 7), ''), ((0, 3, 7), 'm'), ((0, 3, 6), 'dim'),
          ((0, 4, 8), 'aug'), ((0, 2, 7), 'sus2'), ((0, 5, 7), 'sus4'), ((0, 7), '5')]
KEY_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']


def note_name(n: int) -> str:
    return f'{NOTES[n % 12]}{n // 12 - 1}'


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
        self.rows: list[tuple[int, int, int] | None] = [None] * ROWS   # (note, son, volume)
        for r, s in ((0, 1), (4, 2), (8, 1), (10, 1), (12, 2), (2, 4), (6, 4), (14, 4)):
            self.rows[r] = (60, s, 0x7F if s != 4 else 0x50)
        self.cursor, self.bpm = 0, 120
        self.playing = self.recording = False
        self.playhead, self.next_row_time = 0, 0.0
        self.down: set[int] = set()
        self.right_view = 'tracker'                  # ou 'keyboard' (bouton Keyboard)
        self.octave, self.root, self.scale_idx = 3, 0, 1     # C mineur, a partir de C-3
        self.held_notes: dict[int, tuple[int, int]] = {}     # pad -> (note, velocite)
        self.note_flash: dict[int, tuple[float, int]] = {}   # note -> (fin, velocite)
        self.held: set[str] = set()
        self.flash: dict[int, float] = {}            # pad -> fin de l'allumage
        self.knobs = [Knob(8), Knob(8), Knob(8), Knob(8), Knob(12), Knob(14), Knob(6), Knob(10)]

    def pad_note(self, pad: int) -> int:
        """Mode clavier : les 16 pads jouent les degres de la gamme, du grave a l'aigu."""
        scale = SCALES[self.scale_idx][1]
        deg = pad - 1
        return min(127, 12 * (self.octave + 1) + self.root + scale[deg % len(scale)]
                   + 12 * (deg // len(scale)))

    def active_notes(self) -> dict[int, int]:
        """Notes a allumer sur le clavier : tenues aux pads + jouees par le pattern."""
        now = time.monotonic()
        notes = {n: v for n, (end, v) in self.note_flash.items() if end > now}
        for n, v in self.held_notes.values():
            notes[n] = max(v, notes.get(n, 0))
        return notes

    def sound_name(self, s: int) -> str:
        return DRUMS[s - 1] if self.group == 'A' else f'Son {s:02d}'


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

    def read(self) -> bytes:
        n = C.c_int(0)
        rc = self.lib.libusb_interrupt_transfer(self.h, self.ep_in, self.buf, 64, C.byref(n), 2)
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
    acc = group_rgb(st)
    img = Image.new('RGB', (W, H), 'black')
    d = ImageDraw.Draw(img)
    header(d, f'FUZZY TURTLE  -  Groupe {st.group}', f'{st.bpm} BPM', acc)
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
    d.text((10, 222), f'Son choisi : {st.sound:02d} {st.sound_name(st.sound)}', font=font(15), fill=acc)
    state = ('REC ' if st.recording else '') + ('PLAY' if st.playing else 'STOP')
    d.text((10, 248), f'{state}   Shift+pad = effacer la ligne', font=font(12), fill=(140, 140, 140))
    return img


def draw_tracker(st: State) -> Image.Image:
    acc = group_rgb(st)
    img = Image.new('RGB', (W, H), (12, 12, 16))
    d = ImageDraw.Draw(img)
    header(d, 'VUE TRACKER', ('REC ' if st.recording else '') + f'Groupe {st.group}',
           group_rgb(st))
    f = font(14)
    d.text((8, 31), 'Lig  Note Ins Vol', font=f, fill=acc)
    top = min(max(0, st.cursor - VISIBLE // 2), ROWS - VISIBLE)
    for k in range(VISIBLE):
        r = top + k
        y = 50 + k * 18
        if r == st.cursor:
            d.rectangle((0, y - 1, W, y + 16), fill=shade(acc, 0.44) if st.recording else shade(acc, 0.28))
        elif st.playing and r == st.playhead:
            d.rectangle((0, y - 1, W, y + 16), fill=(0, 60, 40))
        elif r % 4 == 0:
            d.rectangle((0, y - 1, W, y + 16), fill=(30, 30, 38))
        cell = st.rows[r]
        txt = (f'{r:02X}   {note_name(cell[0])}  {cell[1]:02X}  {cell[2]:02X}' if cell
               else f'{r:02X}   ---  --  --')
        d.text((8, y), txt, font=f, fill='white' if cell else (110, 110, 110))
        if cell and cell[1] <= 16:
            d.text((220, y), st.sound_name(cell[1]), font=font(12), fill=(150, 150, 150))
    labels = ['Ligne', 'Note', 'Volume', 'Tempo']
    cell = st.rows[st.cursor]
    values = [f'{st.cursor:02X}', note_name(cell[0]) if cell else '---',
              f'{cell[2]:02X}' if cell else '--', f'{st.bpm}']
    for i in range(4):
        x = i * 120
        d.text((x + 10, 250), f'{labels[i]} {values[i]}', font=font(12), fill=(180, 180, 180))
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
    scale_name, scale = SCALES[st.scale_idx]
    header(d, 'VUE CLAVIER', f'{KEY_NAMES[st.root]} {scale_name}  -  Groupe {st.group}',
           group_rgb(st))
    low = 12 * (st.octave + 1)                     # 3 octaves a partir du C de l'octave
    high = low + 36
    notes = st.active_notes()
    # Nom de l'accord ou de la note, et fleches si une note sort de la plage
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
    labels = [f'Octave {st.octave}', f'Fond. {KEY_NAMES[st.root]}', scale_name.capitalize(), f'{st.bpm} BPM']
    for i, t in enumerate(labels):
        d.text((i * 120 + 10, 254), t, font=font(12), fill=(180, 180, 180))
    return img




def draw_right(st: State) -> Image.Image:
    return draw_keyboard(st) if st.right_view == 'keyboard' else draw_tracker(st)


# --- Logique -------------------------------------------------------------------
def handle(st: State, ev: tuple) -> None:
    kind = ev[0]
    if kind == 'pad_on':
        pad, vel = ev[1], ev[2]
        st.down.add(pad)
        if 'shift' in st.held:
            st.rows[st.cursor] = None
            return
        if st.right_view == 'keyboard':            # pads = notes de la gamme
            note = st.pad_note(pad)
            st.held_notes[pad] = (note, vel)
            if st.recording:
                st.rows[st.cursor] = (note, st.sound, vel)
                st.cursor = min(ROWS - 1, st.cursor + 1)
            return
        st.sound = pad                              # pads = sons du groupe
        if st.recording:
            st.rows[st.cursor] = (60, pad, vel)
            st.cursor = min(ROWS - 1, st.cursor + 1)
    elif kind == 'pad_off':
        st.down.discard(ev[1])
        st.held_notes.pop(ev[1], None)
    elif kind == 'button':
        name, pressed = ev[1], ev[2]
        (st.held.add if pressed else st.held.discard)(name)
        if not pressed:
            return
        if name.startswith('group_'):
            st.group = name[-1].upper()
        elif name == 'play':
            st.playing = not st.playing
            st.next_row_time = time.monotonic()
        elif name == 'stop':
            st.playing, st.playhead = False, 0
        elif name == 'rec':
            st.recording = not st.recording
        elif name == 'keyboard':
            st.right_view = 'tracker' if st.right_view == 'keyboard' else 'keyboard'
            st.held_notes.clear()
    elif kind == 'knob':
        i, n = ev[1] - 1, st.knobs[ev[1] - 1].steps(ev[2])
        if not n:
            return
        if i < 4:
            vals = st.params[(st.group, st.sound)]
            vals[i] = max(0, min(127, vals[i] + n))
        elif st.right_view == 'keyboard' and i in (4, 5, 6):
            if i == 4:
                st.octave = max(0, min(7, st.octave + n))
            elif i == 5:
                st.root = (st.root + n) % 12
            else:
                st.scale_idx = (st.scale_idx + n) % len(SCALES)
        elif i == 4:
            st.cursor = max(0, min(ROWS - 1, st.cursor + n))
        elif i in (5, 6) and st.rows[st.cursor]:
            note, s, vol = st.rows[st.cursor]
            if i == 5:
                note = max(0, min(119, note + n))
            else:
                vol = max(1, min(127, vol + n))
            st.rows[st.cursor] = (note, s, vol)
        elif i == 7:
            st.bpm = max(40, min(240, st.bpm + n))


def tick(st: State) -> bool:
    """Fait avancer la lecture : 4 lignes par temps. Renvoie True si une ligne a joue."""
    if not st.playing:
        return False
    now = time.monotonic()
    moved = False
    while now >= st.next_row_time:
        cell = st.rows[st.playhead]
        if cell and cell[1] <= 16:
            st.flash[cell[1]] = now + 0.08
            if cell[1] == st.sound:                     # le clavier montre le son choisi
                st.note_flash[cell[0]] = (now + 0.15, cell[2])
        st.playhead = (st.playhead + 1) % ROWS
        st.next_row_time += 60 / st.bpm / 4
        moved = True
    return moved


# Retro-eclairage des boutons (rapport 0x80, 63 octets) - emplacements et
# niveaux repris du pilote r22yb3m (MIT). Boutons mono : 4 = doux, 7 = vif.
# Groupes A-H : couleurs de la palette NI, (douce, vive) pour chaque groupe.
BUTTON_SLOTS = {'play': 41, 'rec': 42, 'stop': 43, 'shift': 44, 'keyboard': 47}
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
              'shift': 'shift' in st.held, 'keyboard': st.right_view == 'keyboard'}
    for name, slot in BUTTON_SLOTS.items():
        f[1 + slot] = BTN_BRIGHT if active[name] or name in st.held else BTN_DIM
    for i, (dim, bright) in enumerate(GROUP_PALETTE):
        f[1 + GROUP_SLOT + i] = bright if GROUPS[i] == st.group else dim
    return bytes(f)


def led_frame(st: State) -> bytes:
    """Pads dans la couleur du groupe : douce au repos, vive si frappe, joue ou choisi."""
    now = time.monotonic()
    dim, bright = GROUP_PALETTE[GROUPS.index(st.group)]
    return pad_led_frame({p: bright if (p in st.down or st.flash.get(p, 0) > now or p == st.sound)
                          else dim for p in range(1, 17)})


def palette_mode(dev, screens, dec) -> int:
    """Nuancier : pad n = teinte n-1 en vif. Frapper un pad l'affiche en doux / vif / pale."""
    shown = {p: 6 + 4 * (p - 1) for p in range(1, 17)}
    def draw():
        img = Image.new('RGB', (W, H), 'black')
        d = ImageDraw.Draw(img)
        header(d, 'NUANCIER MASCHINE', 'pad = teinte')
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
        st.held_notes = {1: (60, 110), 3: (63, 80), 5: (67, 100), 7: (70, 60)}
        draw_keyboard(st).save('mk3_ecran_clavier.png')
        print('Apercu enregistre : mk3_ecran_clavier.png')
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

    dirty, last_draw, last_leds, last_buttons = True, 0.0, b'', b''
    try:
        while True:
            got = False
            while True:                                   # vider tous les messages en attente
                data = dev.read()
                if not data:
                    break
                for ev in dec.parse(data):
                    handle(st, ev); got = dirty = True
            if tick(st):
                dirty = True
            now = time.monotonic()
            if (any(t > now - 0.1 for t in st.flash.values())
                    or any(e > now - 0.1 for e, _ in st.note_flash.values())):
                dirty = True
            leds = led_frame(st)
            if leds != last_leds:
                dev.write(leds); last_leds = leds
            buttons = button_frame(st)
            if buttons != last_buttons:
                dev.write(buttons); last_buttons = buttons
            if dirty and now - last_draw >= 1 / 25:       # 25 images/s maximum
                screens.send(0, draw_left(st))
                screens.send(1, draw_right(st))
                dirty, last_draw = False, now
            if not got:
                time.sleep(0.003)
    except KeyboardInterrupt:
        pass
    except MK3Error as e:
        print('Erreur :', e)
    finally:
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
