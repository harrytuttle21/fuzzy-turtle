#!/usr/bin/env python3
"""Fuzzy Turtle - prototype 1 : Maschine MK3 en HID direct (macOS / Linux).

- affiche les pads frappes (velocite), les boutons et les encodeurs ;
- allume la LED du pad frappe (couleur vive), les autres en couleur douce ;
- eteint les LED a la sortie (Ctrl+C).

Protocole repris du pilote "Maschine Zynthian Prototype" r22yb3m (licence MIT,
Copyright (c) 2026 Contributors to Maschine Zynthian Prototype) :
rapports d'entree 0x01 (boutons/encodeurs) et 0x02 (pads), rapport LED 0x81.
Seul le transport change : hidapi (multiplateforme) au lieu de /dev/hidraw.

Usage :  python3 mk3_probe.py            (lecture + LED)
         python3 mk3_probe.py --no-leds  (lecture seule)
         python3 mk3_probe.py --curve softer   (courbe de velocite : linear, soft, softer, hard)
         python3 mk3_probe.py --list     (lister les interfaces HID NI)
Prerequis : pip install hidapi ; quitter les logiciels Native Instruments.
"""
from __future__ import annotations
import argparse, sys, time

VID, PID_MK3, PID_PLUS = 0x17CC, 0x1600, 0x1820
HID_INTERFACE = 4

# --- Protocole (extrait du pilote r22yb3m, MIT) -----------------------------
CONTROL_BITS = {**{f'group_{chr(97+i)}': 8+i for i in range(8)},
                'shift': 6, 'midi_channel': 56, 'mute': 37, 'solo': 36,   # SOLO suppose, a confirmer
                'browser': 58,
                'restart': 41, 'tap': 43, 'play': 45, 'rec': 46, 'stop': 47,
                'tempo': 19, 'pad_mode': 24, 'keyboard': 25, 'chords': 26,
                'scene': 29, 'pattern': 30, 'mixer': 52, 'plug_in': 53,
                'nav_prev': 59, 'nav_next': 50,
                'screen_5': 68, 'screen_6': 69,
                # Molette 4D poussee dans une direction / cliquee (octet 1, pilote r22yb3m)
                'nav_click': 0, 'nav_up': 2, 'nav_right': 3, 'nav_down': 4, 'nav_left': 5,
                'step': 27}       # STEP : bit suppose (apres CHORDS 26), a confirmer   # bouton 5 au-dessus des ecrans (pilote r22yb3m)

def pad_number_for_id(pad_id: int) -> int:
    return (3 - pad_id // 4) * 4 + pad_id % 4 + 1

def pad_slot(pad_number: int) -> int:
    return 37 - 4 * ((pad_number - 1) // 4) + ((pad_number - 1) % 4)

def pad_led_frame(colors: dict[int, int]) -> bytes:
    frame = bytearray(42); frame[0] = 0x81
    for pad, color in colors.items():
        frame[1 + pad_slot(pad)] = color
    return bytes(frame)

CURVES = {'linear': 1.0, 'soft': 0.5, 'softer': 0.35, 'hard': 1.6}

def velocity(raw: int, exponent: float) -> int:
    return max(1, min(127, round(127 * (min(raw, 4095) / 4095) ** exponent)))

class Decoder:
    def __init__(self, exponent: float = 0.5):
        self.exponent = exponent
        self.bits = 0
        self.unknown = 0
        self.jog = None
        self.down: set[int] = set()
        self.knobs: dict[int, int] = {}

    def parse(self, r: bytes) -> list[tuple]:
        if not r:
            return []
        if r[0] == 0x01 and len(r) >= 7:
            return self._buttons(r) + self._knobs(r) + self._jog(r)
        if r[0] == 0x02 and len(r) >= 4:
            return self._pads(r)
        return []

    def _buttons(self, r):
        out = []
        for name, bit in CONTROL_BITS.items():
            idx = 1 + bit // 8
            if idx >= len(r):
                continue
            pressed = bool(r[idx] & (1 << (bit % 8)))
            mask = 1 << bit
            if pressed != bool(self.bits & mask):
                self.bits ^= mask
                out.append(('button', name, pressed))
        # Boutons pas encore identifies : on signale le numero de bit (aide a la calibration)
        known = set(CONTROL_BITS.values())
        for bit in range(72):
            if bit in known or 1 + bit // 8 >= len(r):
                continue
            pressed = bool(r[1 + bit // 8] & (1 << (bit % 8)))
            mask = 1 << bit
            if pressed != bool(self.unknown & mask):
                self.unknown ^= mask
                out.append(('unknown_button', bit, pressed))
        return out

    def _jog(self, r):
        """Grosse molette 4D : compteur sur les 4 bits faibles de l'octet 11 (pilote r22yb3m)."""
        if len(r) < 12:
            return []
        v = r[11] & 0x0F
        before, self.jog = self.jog, v
        if before is None:
            return []
        d = ((v - before + 8) % 16) - 8
        return [('jog', d)] if d and -3 <= d <= 3 else []

    def _knobs(self, r):
        out = []
        if len(r) < 28:
            return out
        for i in range(8):
            v = int.from_bytes(r[12+2*i:14+2*i], 'little')
            if not 0 <= v <= 999:
                continue
            before = self.knobs.get(i); self.knobs[i] = v
            if before is None:
                continue
            d = (v - before) % 1000
            if d > 500: d -= 1000
            if d and abs(d) <= 200:   # 200 max : ignore seulement les sauts aberrants
                out.append(('knob', i + 1, d))
        return out

    def _pads(self, r):
        out = []
        for pos in range(1, len(r) - 2, 3):
            pid, hi, lo = r[pos:pos+3]
            if pid >= 16 or (pid == hi == lo == 0):
                continue
            status, raw = hi >> 4, ((hi & 0x0F) << 8) | lo
            num = pad_number_for_id(pid)
            if status in (0, 1) and raw > 0:
                # Un pad tenu envoie sa pression en continu : on ne declenche
                # qu'a la premiere frappe, pas a chaque mise a jour.
                if num not in self.down:
                    self.down.add(num)
                    out.append(('pad_on', num, velocity(raw, self.exponent)))
            elif (status in (2, 3) or raw == 0) and num in self.down:
                self.down.discard(num)
                out.append(('pad_off', num, 0))
        return out

# --- Transport hidapi --------------------------------------------------------
def find_interface(hid):
    found = [d for d in hid.enumerate(VID, 0) if d['product_id'] in (PID_MK3, PID_PLUS)]
    return found, [d for d in found if d.get('interface_number') == HID_INTERFACE] or found

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--no-leds', action='store_true')
    ap.add_argument('--list', action='store_true')
    ap.add_argument('--curve', choices=CURVES, default='soft',
                    help='courbe de velocite (defaut : soft)')
    args = ap.parse_args()
    try:
        import hid
    except ImportError:
        print('Module hidapi absent : pip install hidapi'); return 1
    found, candidates = find_interface(hid)
    if args.list or not candidates:
        for d in found:
            print(f"{d['product_string']!r} pid={d['product_id']:#06x} "
                  f"interface={d.get('interface_number')} path={d['path']!r}")
        if not candidates:
            print('Aucune Maschine trouvee. Branchee ? Logiciels NI quittes ?')
            return 1
        if args.list:
            return 0
    dev = hid.device(); dev.open_path(candidates[0]['path']); dev.set_nonblocking(True)
    print(f"MK3 ouverte ({candidates[0].get('product_string')}). Ctrl+C pour quitter.")
    DIM, BRIGHT = 29, 30          # couleurs palette NI (30 = vert valide sur pad 1)
    leds = not args.no_leds
    dec = Decoder(CURVES[args.curve])
    def refresh():
        if leds:
            dev.write(pad_led_frame({p: (BRIGHT if p in dec.down else DIM) for p in range(1, 17)}))
    try:
        refresh()
        while True:
            data = bytes(dev.read(64))
            if not data:
                time.sleep(0.002); continue
            events = dec.parse(data)
            for ev in events:
                kind = ev[0]
                if kind == 'pad_on':   print(f'PAD {ev[1]:2d}  ON   velocite {ev[2]:3d}  ' + '#' * (ev[2] // 4))
                elif kind == 'pad_off': print(f'PAD {ev[1]:2d}  OFF')
                elif kind == 'button':  print(f"BOUTON {ev[1]} {'appuye' if ev[2] else 'relache'}")
                elif kind == 'knob':    print(f'ENCODEUR {ev[1]} {ev[2]:+d}')
                elif kind == 'unknown_button': print(f"BOUTON INCONNU bit {ev[1]} {'appuye' if ev[2] else 'relache'}")
            if any(e[0] in ('pad_on', 'pad_off') for e in events):
                refresh()
    except KeyboardInterrupt:
        pass
    finally:
        if leds:
            try: dev.write(pad_led_frame({}))
            except Exception: pass
        dev.close()
        print('\nLED eteintes, Maschine liberee.')
    return 0

if __name__ == '__main__':
    sys.exit(main())
