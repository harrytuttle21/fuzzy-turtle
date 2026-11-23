#!/usr/bin/env python3
"""Fuzzy Turtle - prototype 2 : ecrans de la Maschine MK3 (macOS / Linux).

Dessine sur les deux ecrans couleur 480x272 de la MK3 :
  demo     : ecran gauche = accueil Fuzzy, ecran droit = maquette de la vue Tracker
  image    : affiche une image (ex. le logo) sur un ecran
  fps      : mesure combien d'images par seconde on peut envoyer
  clear    : efface les deux ecrans

Protocole repris du pilote "Maschine Zynthian Prototype" r22yb3m (licence MIT,
Copyright (c) 2026 Contributors to Maschine Zynthian Prototype) :
interface USB 5, endpoint bulk OUT 0x04, pixels RGB565 big-endian.
Transport : libusb (via ctypes) au lieu de sysfs.

Usage :  python3 mk3_screens.py                  (demo)
         python3 mk3_screens.py image logo.png --screen 0
         python3 mk3_screens.py fps
         python3 mk3_screens.py clear
         python3 mk3_screens.py demo --preview   (sans Maschine : enregistre des PNG)
         sudo python3 mk3_screens.py --detach    (si l'interface ecran est occupee)
Prerequis : pip install pillow ; libusb (MacPorts : sudo port install libusb) ;
quitter les logiciels Native Instruments (Maschine, NI Hardware Agent).
"""
from __future__ import annotations
import argparse, ctypes as C, ctypes.util, os, struct, sys, time

from PIL import Image, ImageChops, ImageDraw, ImageFont

VID, PID_MK3, PID_PLUS = 0x17CC, 0x1600, 0x1820     # Maschine MK3, Maschine+ (mode controleur)
DISPLAY_INTERFACE, DISPLAY_ENDPOINT = 5, 0x04          # MK3 ; Maschine+ : interface 6
DISPLAY_INTERFACES = {0x1600: 5, 0x1820: 6}
W, H = 480, 272
USB_TIMEOUT_MS = 2500
ORANGE = (252, 95, 0)           # couleur d'accent Fuzzy #FC5F00 (celle du logo)
# Correction de couleur propre aux ecrans Maschine (plus pales / jaunes qu'un ecran PC) :
# on assombrit le vert avec un gamma. Choisir la valeur avec l'action 'couleurs'.
GREEN_GAMMAS = (1.0, 1.3, 1.6, 2.0, 2.4, 2.8, 3.2, 3.6)
GREEN_GAMMA = GREEN_GAMMAS[2]     # case 3, choisie sur la vraie Maschine (18/11)


class MK3Error(RuntimeError):
    pass


# --- Protocole ecran (extrait du pilote r22yb3m, MIT) ----------------------
def screen_packet(index: int, x: int, y: int, w: int, h: int, body: bytes) -> bytes:
    """Un rectangle de pixels RGB565 big-endian pour l'ecran 0 (gauche) ou 1 (droit)."""
    count = w * h
    if count % 2 or len(body) != count * 2:
        raise ValueError('Taille de rectangle ou de pixels invalide')
    return (bytes((0x84, 0x00, index, 0x60, 0, 0, 0, 0))
            + struct.pack('>HHHH', x, y, w, h)
            + bytes((0x02, 0, 0, 0))
            + struct.pack('>I', count // 2)
            + body
            + bytes((0x02, 0, 0, 0, 0x03, 0, 0, 0, 0x40, 0, 0, 0)))


def to_rgb565_be(img: Image.Image) -> bytes:
    """Image Pillow -> octets RGB565 big-endian (calcul fait par Pillow, rapide)."""
    r, g, b = img.convert('RGB').split()
    hi = ImageChops.add(r.point(lambda v: v & 0xF8), g.point(lambda v: v >> 5))
    lo = ImageChops.add(g.point(lambda v: (v << 3) & 0xE0), b.point(lambda v: v >> 3))
    return Image.merge('LA', (hi, lo)).tobytes()


def correct(img: Image.Image, gamma: float | None = None) -> Image.Image:
    """Applique la correction de couleur des ecrans Maschine (gamma sur le vert)."""
    g = GREEN_GAMMA if gamma is None else gamma
    if g == 1.0:
        return img
    lut = list(range(256)) + [round(255 * (v / 255) ** g) for v in range(256)] + list(range(256))
    return img.convert('RGB').point(lut)


# --- Transport libusb ------------------------------------------------------
def load_libusb():
    candidates = [ctypes.util.find_library('usb-1.0'),
                  '/opt/local/lib/libusb-1.0.dylib',      # MacPorts
                  '/usr/local/lib/libusb-1.0.dylib',      # Homebrew Intel
                  '/opt/homebrew/lib/libusb-1.0.dylib',   # Homebrew Apple Silicon
                  'libusb-1.0.so.0']                      # Linux
    for name in candidates:
        if not name:
            continue
        try:
            lib = C.CDLL(name)
            break
        except OSError:
            continue
    else:
        raise MK3Error('libusb introuvable (MacPorts : sudo port install libusb)')
    lib.libusb_init.argtypes = [C.POINTER(C.c_void_p)]
    lib.libusb_init.restype = C.c_int
    lib.libusb_exit.argtypes = [C.c_void_p]
    lib.libusb_exit.restype = None
    lib.libusb_open_device_with_vid_pid.argtypes = [C.c_void_p, C.c_uint16, C.c_uint16]
    lib.libusb_open_device_with_vid_pid.restype = C.c_void_p
    lib.libusb_close.argtypes = [C.c_void_p]
    lib.libusb_close.restype = None
    for fn in ('kernel_driver_active', 'claim_interface', 'release_interface',
               'detach_kernel_driver', 'attach_kernel_driver'):
        f = getattr(lib, 'libusb_' + fn)
        f.argtypes = [C.c_void_p, C.c_int]
        f.restype = C.c_int
    lib.libusb_bulk_transfer.argtypes = [C.c_void_p, C.c_ubyte, C.POINTER(C.c_ubyte),
                                         C.c_int, C.POINTER(C.c_int), C.c_uint]
    lib.libusb_bulk_transfer.restype = C.c_int
    return lib


class Screens:
    """Ouvre l'interface ecran une fois, envoie autant d'images que voulu."""

    def __init__(self, detach: bool = False):
        self.lib = load_libusb()
        self.ctx = C.c_void_p()
        if self.lib.libusb_init(C.byref(self.ctx)) < 0:
            raise MK3Error('libusb_init a echoue')
        self.handle, self.product = None, ''
        for pid, name in ((PID_MK3, 'Maschine MK3'), (PID_PLUS, 'Maschine+')):
            self.handle = self.lib.libusb_open_device_with_vid_pid(self.ctx, VID, pid)
            if self.handle:
                self.product, self.iface = name, DISPLAY_INTERFACES[pid]
                break
        if not self.handle:
            self.lib.libusb_exit(self.ctx)
            raise MK3Error('Maschine MK3 (17cc:1600) ou Maschine+ (17cc:1820) introuvable : '
                           'branchee, allumee (Maschine+ en mode controleur) ?')
        print(f'[diag] ecrans : {self.product}')
        self.claimed = self.detached = False
        bound = self.lib.libusb_kernel_driver_active(self.handle, self.iface)
        print(f'[diag] pilote systeme sur l\'interface {self.iface} : code {bound} '
              '(0 = libre, 1 = occupee, -12 = question non geree)')
        rc = self.lib.libusb_claim_interface(self.handle, self.iface)
        print(f'[diag] reservation de l\'interface {self.iface} : code {rc} (0 = OK)')
        if rc != 0 and bound == 1 and detach:
            d = self.lib.libusb_detach_kernel_driver(self.handle, self.iface)
            print(f'[diag] detachement du pilote systeme : code {d} (0 = OK)')
            if d == 0:
                self.detached = True
                rc = self.lib.libusb_claim_interface(self.handle, self.iface)
                print(f'[diag] nouvelle reservation : code {rc} (0 = OK)')
        if rc != 0:
            self.close()
            hint = (' Essaie avec --detach (voir les explications).' if bound == 1 and not detach
                    else ' Quitte Maschine et NI Hardware Agent, puis reessaie.')
            raise MK3Error(f'Interface ecran indisponible (code {rc}).' + hint)
        self.claimed = True

    def send(self, index: int, img: Image.Image, x: int = 0, y: int = 0) -> None:
        img = correct(img)
        w, h = img.size
        payload = screen_packet(index, x, y, w, h, to_rgb565_be(img))
        data = (C.c_ubyte * len(payload)).from_buffer_copy(payload)
        done = C.c_int(0)
        rc = self.lib.libusb_bulk_transfer(self.handle, DISPLAY_ENDPOINT, data,
                                           len(payload), C.byref(done), USB_TIMEOUT_MS)
        if rc != 0 or done.value != len(payload):
            raise MK3Error(f'Envoi ecran {index} : code {rc}, {done.value}/{len(payload)} octets')

    def close(self) -> None:
        if getattr(self, 'handle', None):
            if getattr(self, 'claimed', False):
                self.lib.libusb_release_interface(self.handle, self.iface)
            if getattr(self, 'detached', False):
                self.lib.libusb_attach_kernel_driver(self.handle, self.iface)
            self.lib.libusb_close(self.handle)
            self.handle = None
        self.lib.libusb_exit(self.ctx)


class Preview:
    """Remplace la Maschine : enregistre ce qui serait affiche dans des PNG."""

    def send(self, index, img, x=0, y=0):
        path = f'mk3_ecran_{"gauche" if index == 0 else "droit"}.png'
        img.save(path)
        print(f'Apercu enregistre : {path}')

    def close(self):
        pass


# --- Dessin ------------------------------------------------------------------
def font(size: int):
    for name in ('Menlo.ttc', '/System/Library/Fonts/Menlo.ttc',
                 'DejaVuSansMono.ttf', '/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf'):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    try:
        return ImageFont.load_default(size=size)
    except TypeError:
        return ImageFont.load_default()


def header(d: ImageDraw.ImageDraw, title: str, right: str, color=ORANGE) -> None:
    d.rectangle((0, 0, W, 26), fill=color)
    r, g, b = color                                # texte noir sur fond clair, blanc sur fond fonce
    ink = 'black' if 0.299 * r + 0.587 * g + 0.114 * b > 110 else 'white'
    d.text((8, 4), title, font=font(16), fill=ink)
    f = font(14)
    d.text((W - 8 - d.textlength(right, font=f), 5), right, font=f, fill=ink)


def left_demo() -> Image.Image:
    img = Image.new('RGB', (W, H), 'black')
    d = ImageDraw.Draw(img)
    header(d, 'FUZZY TURTLE', 'DAW Pulses')
    d.text((16, 50), 'Groupe A  -  Batterie', font=font(22), fill='white')
    f = font(14)
    sounds = ['Kick', 'Snare', 'Clap', 'HH fer', 'HH ouv', 'Tom', 'Perc', 'Crash']
    for i, s in enumerate(sounds):
        x, y = 16 + (i % 4) * 116, 96 + (i // 4) * 44
        d.rectangle((x, y, x + 106, y + 36), outline=ORANGE if i == 0 else (90, 90, 90), width=2)
        d.text((x + 8, y + 10), f'{i + 1:02d} {s}', font=f, fill='white')
    # 8 parametres au-dessus des 8 encodeurs
    params = ['Volume', 'Pan', 'Pitch', 'Attack', 'Decay', 'Cutoff', 'Reso', 'Send']
    for i, p in enumerate(params[:4]):
        x = i * 120
        d.text((x + 10, 200), p, font=f, fill=(180, 180, 180))
        d.rectangle((x + 10, 224, x + 110, 234), outline=(90, 90, 90))
        d.rectangle((x + 10, 224, x + 10 + 25 * (i + 1), 234), fill=ORANGE)
    d.text((16, 250), 'CPU 12%   RAM 340 Mo   120 BPM', font=font(12), fill=(140, 140, 140))
    return img


def right_tracker() -> Image.Image:
    img = Image.new('RGB', (W, H), (12, 12, 16))
    d = ImageDraw.Draw(img)
    header(d, 'VUE TRACKER', 'Pattern 01')
    f = font(14)
    rows = [('C-4', '01', '7F', '---'), ('---', '--', '--', '---'),
            ('D-4', '02', '60', 'R04'), ('---', '--', '--', '---'),
            ('C-4', '01', '7F', 'P20'), ('---', '--', '--', '---'),
            ('D-4', '02', '60', '---'), ('C-4', '01', '40', 'B40'),
            ('C-4', '01', '7F', '---'), ('---', '--', '--', '---'),
            ('D-4', '02', '60', 'G08'), ('---', '--', '--', '---')]
    d.text((8, 32), 'Lig  Note Ins Vol FX', font=f, fill=ORANGE)
    cursor = 4
    for n, (note, ins, vol, fx) in enumerate(rows):
        y = 52 + n * 18
        if n == cursor:
            d.rectangle((0, y - 1, W, y + 16), fill=(70, 35, 0))
        elif n % 4 == 0:
            d.rectangle((0, y - 1, W, y + 16), fill=(30, 30, 38))
        col = 'white' if note != '---' else (110, 110, 110)
        d.text((8, y), f'{n:02X}   {note}  {ins}  {vol}  {fx}', font=f, fill=col)
    return img


def fit(path: str) -> Image.Image:
    src = Image.open(path).convert('RGBA')
    src.thumbnail((W, H))
    img = Image.new('RGB', (W, H), 'black')
    img.paste(src, ((W - src.width) // 2, (H - src.height) // 2), src)
    return img


# --- Actions -------------------------------------------------------------------
def hold(dev, redraw) -> None:
    """Garde l'image affichee (renvoyee chaque seconde) jusqu'a Ctrl+C.
    A la sortie, le pilote NI reprend la main et peut redessiner par-dessus."""
    if isinstance(dev, Preview):
        return
    print('Image maintenue. Ctrl+C pour quitter.')
    try:
        while True:
            time.sleep(1.0)
            redraw()
    except KeyboardInterrupt:
        print()

def run_fps(dev, seconds: float = 5.0) -> None:
    """Barre orange qui traverse l'ecran ; on envoie l'ecran entier a chaque image."""
    frames, t0 = 0, time.perf_counter()
    while time.perf_counter() - t0 < seconds:
        img = Image.new('RGB', (W, H), 'black')
        x = (frames * 8) % W
        ImageDraw.Draw(img).rectangle((x, 0, x + 40, H), fill=ORANGE)
        dev.send(0, img)
        frames += 1
    full = frames / (time.perf_counter() - t0)
    # Petit rectangle (une ligne de tracker) : c'est ce qu'on enverra le plus souvent
    strip, n, t0 = Image.new('RGB', (W, 18), ORANGE), 0, time.perf_counter()
    while time.perf_counter() - t0 < 2.0:
        dev.send(1, strip, 0, 126)
        n += 1
    part = n / (time.perf_counter() - t0)
    print(f'Ecran entier : {full:.1f} images/s   |   une ligne (480x18) : {part:.1f} /s')


def main() -> int:
    global GREEN_GAMMA
    p = argparse.ArgumentParser(description='Fuzzy Turtle - ecrans Maschine MK3')
    p.add_argument('action', nargs='?', default='demo', choices=('demo', 'image', 'mire', 'couleurs', 'fps', 'clear'))
    p.add_argument('file', nargs='?', help="image a afficher (action 'image')")
    p.add_argument('--screen', type=int, choices=(0, 1), default=0, help='0 = gauche, 1 = droit')
    p.add_argument('--preview', action='store_true', help='sans Maschine : enregistrer des PNG')
    p.add_argument('--detach', action='store_true',
                   help="liberer l'interface ecran du pilote systeme (avec sudo sur macOS)")
    p.add_argument('--vert', type=int, choices=range(1, 9), metavar='1-8',
                   help="correction de couleur choisie avec l'action 'couleurs'")
    a = p.parse_args()
    if a.vert:
        GREEN_GAMMA = GREEN_GAMMAS[a.vert - 1]

    try:
        dev = Preview() if a.preview else Screens(detach=a.detach)
    except MK3Error as e:
        print('Erreur :', e)
        return 1
    try:
        if a.action == 'demo':
            dev.send(0, left_demo())
            dev.send(1, right_tracker())
            print('Demo affichee sur les deux ecrans.')
            hold(dev, lambda: (dev.send(0, left_demo()), dev.send(1, right_tracker())))
        elif a.action == 'image':
            if not a.file or not os.path.exists(a.file):
                print('Indique une image existante, ex. : image logo.png')
                return 1
            dev.send(a.screen, fit(a.file))
            print(f'Image affichee sur l\'ecran {"gauche" if a.screen == 0 else "droit"}.')
            img = fit(a.file)
            hold(dev, lambda: dev.send(a.screen, img))
        elif a.action == 'mire':
            # Petit rectangle 240x80 au centre : format valide sur Zynthian
            box = Image.new('RGB', (240, 80), ORANGE)
            ImageDraw.Draw(box).text((20, 28), 'FUZZY TEST', font=font(20), fill='black')
            send = lambda: (dev.send(0, box, 120, 96), dev.send(1, box, 120, 96))
            send()
            print('Mire 240x80 envoyee au centre des deux ecrans.')
            hold(dev, send)
        elif a.action == 'couleurs':
            # 8 cases du meme orange, chacune avec une correction differente.
            # La correction globale est desactivee pour cette mire.
            GREEN_GAMMA = 1.0
            img = Image.new('RGB', (W, H), 'black')
            d = ImageDraw.Draw(img)
            d.text((10, 6), 'Quelle case ressemble le plus a l\'orange du PC ?', font=font(14), fill='white')
            for i, g in enumerate(GREEN_GAMMAS):
                x, y = 10 + (i % 4) * 117, 34 + (i // 4) * 118
                sw = correct(Image.new('RGB', (106, 90), ORANGE), g)
                img.paste(sw, (x, y))
                d.text((x + 44, y + 94), str(i + 1), font=font(16), fill='white')
            logo = None
            if a.file and os.path.exists(a.file):
                logo = fit(a.file)
            send = lambda: (dev.send(0, img), dev.send(1, logo or img))
            send()
            print('Mire de couleurs affichee : note le numero de la case la plus proche.')
            hold(dev, send)
        elif a.action == 'fps':
            if a.preview:
                print("Le test 'fps' a besoin de la Maschine.")
                return 1
            run_fps(dev)
        elif a.action == 'clear':
            black = Image.new('RGB', (W, H), 'black')
            dev.send(0, black)
            dev.send(1, black)
            print('Ecrans effaces.')
    except KeyboardInterrupt:
        pass
    except MK3Error as e:
        print('Erreur :', e)
        return 1
    finally:
        dev.close()
    return 0


if __name__ == '__main__':
    sys.exit(main())
