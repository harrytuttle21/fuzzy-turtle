#!/usr/bin/env python3
"""Fabrique le kit de test « Fuzzy Test Kit » : 16 sons de batterie synthetises
(aucun enregistrement : libres de droits, GPLv3 comme le reste du projet).
Usage : python3 make_test_kit.py [dossier]   (par defaut : le dossier Fuzzy Test Kit a cote du script)"""
import sys, wave
from pathlib import Path
import numpy as np

SR = 44100
rng = np.random.default_rng(7)


def t(sec):
    return np.arange(int(SR * sec)) / SR


def env(x, decay, attack=0.001):
    tt = np.arange(len(x)) / SR
    return x * np.exp(-tt / decay) * np.minimum(1, tt / attack)


def noise(sec):
    return rng.uniform(-1, 1, int(SR * sec))


def highpass(x, k=0.95):
    y = np.zeros_like(x)
    for i in range(1, len(x)):
        y[i] = k * (y[i - 1] + x[i] - x[i - 1])
    return y


def lowpass(x, k=0.2):
    y = np.zeros_like(x)
    for i in range(1, len(x)):
        y[i] = y[i - 1] + k * (x[i] - y[i - 1])
    return y


def biquad(x, kind, freq, q=0.707):
    """Filtre du 2e ordre (formules RBJ) : 'hp' passe-haut, 'bp' passe-bande."""
    w = 2 * np.pi * freq / SR
    alpha, cw = np.sin(w) / (2 * q), np.cos(w)
    if kind == 'hp':
        b = [(1 + cw) / 2, -(1 + cw), (1 + cw) / 2]
    else:
        b = [alpha, 0, -alpha]
    a = [1 + alpha, -2 * cw, 1 - alpha]
    b0, b1, b2 = (v / a[0] for v in b)
    a1, a2 = a[1] / a[0], a[2] / a[0]
    y = np.zeros_like(x)
    x1 = x2 = y1 = y2 = 0.0
    for i, v in enumerate(x):
        out = b0 * v + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1, y2, y1 = x1, v, y1, out
        y[i] = out
    return y


def hihat(sec, decay):
    """Charleston facon TR-808 : 6 carres « metalliques », filtres vers 7-10 kHz, + un peu de souffle."""
    raw = metal(sec) + noise(sec) * 0.25
    x = biquad(biquad(raw, 'hp', 7000), 'hp', 7000)
    x = x * 0.6 + biquad(raw, 'bp', 10000, 1.2) * 0.4
    tt = np.arange(len(x)) / SR
    return x * (np.exp(-tt / decay) * 0.85 + np.exp(-tt / 0.01) * 0.15) * np.minimum(1, tt / 0.0008)


def sweep(f0, f1, fall, sec):
    tt = t(sec)
    f = f1 + (f0 - f1) * np.exp(-tt / fall)
    return np.sin(2 * np.pi * np.cumsum(f) / SR)


def metal(sec, freqs=(205, 304, 369, 522, 540, 800)):
    tt = t(sec)
    return sum(np.sign(np.sin(2 * np.pi * f * tt)) for f in freqs) / len(freqs)


def kick():       return env(sweep(180, 48, 0.035, 1.2), 0.30) + env(noise(1.2), 0.004) * 0.3
def snare():      return env(sweep(240, 180, 0.02, 0.4), 0.07) * 0.7 + env(highpass(noise(0.4), 0.9), 0.12) * 0.8
def closed_hh():  return hihat(0.2, 0.045)
def open_hh():    return hihat(1.0, 0.32)
def rim():        return env(sweep(1700, 900, 0.005, 0.1), 0.012) + env(highpass(noise(0.1)), 0.006) * 0.5
def low_tom():    return env(sweep(140, 90, 0.08, 0.6), 0.25)
def mid_tom():    return env(sweep(200, 130, 0.07, 0.5), 0.20)
def high_tom():   return env(sweep(280, 190, 0.06, 0.45), 0.17)
def crash():      return env(highpass(metal(2.0, (310, 437, 581, 732, 911, 1219)) + noise(2.0), 0.95), 0.8, 0.002)
def ride():       return env(highpass(metal(1.6, (420, 611, 853, 1102)) * 0.8 + noise(1.6) * 0.3, 0.96), 0.6)
def cowbell():    return env(lowpass(metal(0.4, (540, 800)), 0.5), 0.12)
def shaker():     return env(highpass(noise(0.18), 0.98), 0.05, 0.03)
def conga():      return env(sweep(420, 330, 0.02, 0.4), 0.13)
def zap():        return env(sweep(2400, 80, 0.06, 0.4), 0.15)
def bass():
    tt = t(0.8)
    f = 65.41                                           # C2
    return env(np.tanh(2.5 * np.sin(2 * np.pi * f * tt)), 0.45, 0.003)


def clap():
    x = np.zeros(int(SR * 0.5))
    burst = highpass(noise(0.5), 0.9)
    for k, delay in enumerate((0, 0.011, 0.022)):         # trois claquements rapproches
        start = int(SR * delay)
        x[start:] += env(burst[: len(x) - start], 0.010 if k < 2 else 0.18) * 0.7
    return x


SOUNDS = [('Kick', kick), ('Snare', snare), ('Closed HH', closed_hh), ('Open HH', open_hh),
          ('Clap', clap), ('Rim', rim), ('Low Tom', low_tom), ('Mid Tom', mid_tom),
          ('High Tom', high_tom), ('Crash', crash), ('Ride', ride), ('Cowbell', cowbell),
          ('Shaker', shaker), ('Conga', conga), ('Bass C2', bass), ('Zap', zap)]


def write(path: Path, x: np.ndarray) -> None:
    x = x / max(1e-9, np.max(np.abs(x))) * 0.89          # -1 dB
    fade = min(len(x), int(SR * 0.03))                   # fin en fondu de 30 ms : pas de clic
    x[-fade:] *= np.linspace(1, 0, fade)
    data = (x * 32767).astype('<i2').tobytes()
    with wave.open(str(path), 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(data)


def main() -> int:
    folder = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).parent / 'Fuzzy Test Kit'
    folder.mkdir(parents=True, exist_ok=True)
    for i, (name, fn) in enumerate(SOUNDS, 1):
        path = folder / f'{i:02d} {name}.wav'
        write(path, fn())
        print(path)
    return 0


if __name__ == '__main__':
    sys.exit(main())
