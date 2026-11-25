#!/usr/bin/env python3
"""Fuzzy Turtle - prototype 3 : pads + encodeurs + ecrans ensemble (mini DAW Pulses).

Modes, comme sur Maschine :
  PAD MODE      chaque pad joue un son du groupe (mode par defaut)
                ecran gauche : les 16 sons et 4 reglages ; ecran droit : la piste du son choisi
  Bouton 3      (au-dessus de l'ecran gauche, tous les modes sauf MIXER) : vue TRACKER 16 pistes
                sur les deux ecrans ; un 2e appui revient a la vue precedente
  TEMPO         taper plusieurs fois en rythme = tap tempo ; TEMPO tenu + molette 4D = +/- 1 BPM
  Molette 4D    tourner : ligne du curseur ; pousser haut/bas : ligne ; gauche/droite : piste
  Encodeur 8    longueur du pattern (1 a 256 lignes), dans tous les modes
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
  DUPLICATE     tenu + pad + pad = copier le son (sample, synthe, reglages) ; tenu + pad, groupe, pad =
                vers un autre groupe ; tenu + groupe + groupe = copier le groupe (sons, patterns, volume).
  GROUPES       16 groupes : A-H et A2-H2 ; Shift + groupe = ce groupe dans l'autre banque (A <-> A2),
                les 8 boutons suivent la banque ; MIXER page 2 = A2-H2 ; MIDI IN canaux 9-16 = A2-H2.
  PATTERN       pads = 16 patterns du groupe (64 : 4 banques, < > ou encodeur 2) ; vide eteint,
                plein doux, choisi vif, suivant clignote ; en lecture, le changement se fait a la
                fin du pattern ; DUPLICATE + pad = copier le pattern choisi ici ; ERASE + pad =
                effacer ; Shift + pad (serigraphie) sur le pattern choisi : 1-4 annuler / refaire,
                9 Clear, 11 Copy, 12 Paste, 13-16 transposer (pistes 5-16) ; Shift + DUPLICATE =
                DOUBLE (longueur x 2, contenu recopie, partout) ; re-taper le pattern choisi = aucun pattern (le groupe se tait ; enregistre
                ainsi dans une scene) ; bouton 8 = NAME (nom court : encodeur 1 position, 2 lettre, 3 noms
                tout prets) ; encodeur 1 = banque, encodeur 8 = longueur (comme en vue TRACKER ;
                affichee en bas a droite de l'ecran droit).
  TRACKER       a l'arret, Shift + pad (serigraphie MK3) sur la piste choisie : 9 Clear, 11 Copy,
                12 Paste, 13 / 14 demi-ton - / +, 15 / 16 octave - / + : 1er appui = la note du
                curseur, 2e appui rapide = toute la piste (12 colle ce qui a ete copie) ; (5 / 6
                Quantize : notes deja sur la grille) ; Shift + pad 1 / 3 annuler, 2 / 4 refaire. En Keyboard / Chords, le tracker
                de l'ecran gauche s'edite comme en vue complete (encodeurs 1-4, molette, 4D).
  SCENE         pads = 16 scenes (64 : 4 banques, < > ou encodeur 1) ; une scene = un pattern par
                groupe. Pad vide = y ranger les patterns en cours ; pad = lancer (en lecture :
                chaque groupe change a la fin de son pattern) ; SELECT + pad = remplacer ;
                Shift + pad 9 / 11 / 12 = effacer / copier / coller la scene choisie ;
                DUPLICATE + pad = copier la scene en cours ; ERASE + pad = effacer.
                SELECT + groupe = ce groupe est LIBRE dans la scene en cours (il garde son
                pattern quand on la lance, pour jouer en live) ; 2e fois = fixe.
  FILE          projets (dossier projects/NOM.fuzzy/project.json) : encodeur 1 / molette = choisir,
                5 SAVE, 6 LOAD, 7 NEW, 8 NAME, Shift + 6 COPY, Shift + 7 DELETE ; 2e appui pour
                confirmer : SAVE sur un autre projet du meme nom (overwrite), LOAD / NEW non
                enregistre, DELETE. Shift + FILE = enregistrer tout de suite. En quittant, le projet est
                enregistre (l'ancienne version reste en project.bak.json) ; au demarrage, le
                dernier projet est rouvert.
  ARRANGER      le morceau : liste de scenes jouees dans l'ordre. Pads = ajouter la scene apres
                l'entree choisie (< > banque) ; encodeur 1 / molette = entree, 2 = scene,
                3 = repetitions, 4 = longueur en mesures (sinon : plus long pattern x repetitions),
                par pas de 1 ; bouton 3 (HALF) = par pas de 0,5 ; ERASE = enlever, DUPLICATE = doubler l'entree ; bouton 5 SONG (la
                lecture suit le morceau), 6 GO (jouer depuis l'entree choisie), 7 LOOP. Chaque
                entree dure son plus long pattern x repetitions ; tous les groupes changent
                ensemble. Enregistre dans le projet.
  SELECT        SELECT + pad = choisir le son sans le jouer (Pad mode, Keyboard, Chords, STEP) ;
                les pads montrent les 16 sons, le choisi en blanc (et sa case a l'ecran).
  NOMS          bouton NAME (pattern, scene, projet) ou GROUP (mode PATTERN, bouton 7) ;
                encodeurs 5 position, 6 lettre, 7 noms tout prets (INTRO... DRUMS, BASS, VOX...).
  EVENTS        piano roll de la piste choisie : notes en barres (duree), velocite = luminosite.
                Encodeurs 1 ligne, 2 zoom (16 / 32 / 64), 3 hauteur de la vue, 4 piste ;
                5 deplacer la note, 6 hauteur, 7 velocite, 8 duree ; FIXED VEL + molette =
                velocite ; ERASE = effacer la note. Zoom 16 / 32 / 48 / 64 / 128 / 256 lignes.
                En REC pendant la lecture, la duree d'une note = le temps d'appui sur le pad.
  PLUG-IN       enveloppe des sons du groupe : ONE-SHOT (le son va au bout) ou ADSR (le son tient
                la duree de la note puis relache). Pad = choisir le son, SELECT + pads = plusieurs ;
                encodeur 1 = mode, 2 = relachement. Defaut : sons 1-4 ONE-SHOT, 5-16 ADSR.
  SETTINGS      4 onglets, boutons 1-4 : AUDIO / MIDI / COLOR / STATUS (MIDI ouvre directement
                l'onglet MIDI). SETTINGS a nouveau = fermer : bouton 5 YES / bouton 6 NO.
                COLOR : encodeur 1 cible GROUP / PADS ; GROUP : pad n = couleur n ;
                PADS : choisir des pads puis encodeur 2 = leur couleur, Shift + pad = couleur du
                groupe.
                STATUS : etat (controleur, ecrans, agent NI, carte son, moteur, calibration,
                autres logiciels) ; bouton 5 = verifier ; Shift + 8 = quitter et rendre a NI.
                AUDIO : 1 sortie, 2 carte son Maschine ON/OFF, 3 frequence, 4 tampon, 5 entree
                (pour le sampling, plus tard) ; applique apres 1 s ; les pads jouent pour tester.
                MIDI : 1 port IN, 2 mode IN (OMNI / CH>GROUP), 3 port OUT, 4 canal OUT du groupe
                (Internal = moteur interne), 5 horloge OFF/SEND/RECEIVE, 6 Start/Stop, 7 Thru : bouton 5 YES (sauvegarde dans
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

Son : le moteur C++ (prototypes/pad-sound) est demarre automatiquement, sur la sortie choisie
      dans SETTINGS > AUDIO (la Maschine elle-meme est possible : sa carte son reste
      disponible ; --output "nom" pour l'imposer, --no-sound pour s'en passer).
      Lancement en une commande : prototypes/start.sh
      Ancienne methode, moteur a la main dans un autre terminal :
         prototypes/pad-sound/build/fuzzy_pad_sound_artefacts/Release/fuzzy_pad_sound --osc
      mk3_live.py lui envoie chaque note par OSC (port 57390). Sans moteur : interface muette.

Usage :  python3 mk3_live.py              (sans sudo depuis le 20/11/2026)
         python3 mk3_live.py --preview     (sans Maschine : enregistre des PNG)
Acces a la Maschine (mode partage) : pads / boutons / lumieres a travers le pilote HID du
systeme (hidapi), ecrans seuls par libusb (interface 5). Rien n'est detache. Sur macOS, l'agent
NI (NIHostIntegrationAgent) tient les ecrans : il est mis en pause au demarrage et relance en
quittant (la Maschine revient au logiciel NI sans debrancher).
Necessite mk3_probe.py et mk3_screens.py dans le meme dossier ; pip install hidapi pillow.
"""
from __future__ import annotations
import argparse, errno, atexit, json, os, queue, random, signal, socket, struct, subprocess, sys, threading, time
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw

from mk3_probe import Decoder, CURVES, pad_led_frame, VID, PID_MK3, PID_PLUS
from maschine_chord_sets import MACHINE_CHORD_SET_SOURCES   # 16 Chord Sets (pilote r22yb3m, MIT)
from mk3_screens import Screens, Preview, MK3Error, W, H, font, header

ROWS, VISIBLE = 256, 11         # ROWS = longueur maximale d'un pattern (16 mesures, comme le XM)
PAT_SLOTS, PAT_BANKS = 64, 4    # 64 patterns par groupe : 4 banques de 16 (mode PATTERN)
NAME_LEN = 8                    # noms courts des patterns et des scenes
NAME_CHARS = ' ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-'
NAME_PRESETS = ('', 'INTRO', 'VERSE', 'CHORUS', 'BRIDGE', 'BUILD', 'DROP', 'BREAK', 'FILL',
                'OUTRO', 'MAIN', 'VAR', 'DRUMS', 'PERC', 'BASS', 'GUIT', 'VOX', 'SYNTH', 'KEYS',
                'PIANO', 'PAD', 'LEAD', 'STRINGS', 'BRASS', 'FX', 'SAMPLE', 'LOOP')
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
        self.lost = 0
        try:   # macOS : tampon d'envoi UDP de 9 Ko par defaut, trop petit pour les rafales
            self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 1 << 20)
        except OSError:
            pass

    def _sendto(self, data: bytes) -> None:
        """Envoi fiable en local : si macOS refuse un instant (tampon plein, ENOBUFS), on
        reessaie au lieu de perdre le message (une fin de note perdue = note bloquee)."""
        for _ in range(40):
            try:
                self.sock.sendto(data, self.addr)
                return
            except OSError as e:
                if e.errno not in (errno.ENOBUFS, errno.EAGAIN, errno.EWOULDBLOCK):
                    return                     # moteur absent... : rien a faire
                time.sleep(0.0002)
        self.lost += 1
        print(f'[moteur] message perdu ({self.lost} en tout) : envois trop nombreux')

    @staticmethod
    def _pad4(b: bytes) -> bytes:
        return b + b'\0' * (4 - len(b) % 4)

    def hit(self, sound: int, velocity: int, note: int, tag: int = -1, mode: int = 0,
            release_ms: int = 150, gate_ms: int = 0, synth: int = 0, pan: int = 64) -> None:
        """Joue un son. tag = groupe (0-15) ; mode 0 ONE-SHOT, 1 ADSR (la note tient gate_ms,
        ou jusqu'a /off si 0, puis relache en release_ms)."""
        msg = (self._pad4(b'/fuzzy/proto/hit') + self._pad4(b',iiiiiiiii')
               + struct.pack('>iiiiiiiii', sound, velocity, note, tag, mode, release_ms, gate_ms, synth, pan))
        self._sendto(msg)


    def send(self, address: str, *args) -> None:
        """Message OSC quelconque vers le moteur (entiers, decimaux, textes)."""
        tags, body = ',', b''
        for a in args:
            if isinstance(a, (bool, int)):
                tags, body = tags + 'i', body + struct.pack('>i', int(a))
            elif isinstance(a, float):
                tags, body = tags + 'f', body + struct.pack('>f', a)
            else:
                tags, body = tags + 's', body + self._pad4(str(a).encode())
        self._sendto(self._pad4(address.encode()) + self._pad4(tags.encode()) + body)

    def panic(self) -> None:
        """Coupe tous les sons du moteur (Shift + MUTE)."""
        self._sendto(self._pad4(b'/fuzzy/proto/panic') + self._pad4(b','))


SOUND = Sound()
PARAMS = ['Volume', 'Pan', 'Pitch', 'Cutoff']
GROUPS = tuple('ABCDEFGH') + tuple(f'{g}2' for g in 'ABCDEFGH')   # 16 groupes : A-H et A2-H2


def bank_groups(st) -> tuple:
    """Les 8 groupes de la banque du groupe choisi (A-H ou A2-H2) : ceux des 8 boutons."""
    return GROUPS[8:] if st.group.endswith('2') else GROUPS[:8]
SET_TABS = ('AUDIO', 'MIDI', 'COLORS', 'STATUS')     # boutons 1-4 dans SETTINGS (20/11)
# Bouton 3 de SETTINGS : COLORS, 2e appui = KEYS (clavier de l'ordinateur)
DEFAULT_PC = {'keys': False, 'layout': 'AZERTY', 'space': True, 'length': 300}
PC_LENGTHS = (100, 150, 200, 300, 500, 800, 1200)       # duree d'une note jouee au clavier (ms)
# Touches = notes (decalage en demi-tons depuis le C de l'octave), comme le « clavier
# musical » des logiciels : rangee du milieu = touches blanches, rangee du haut = noires.
PC_LAYOUTS = {
    'AZERTY': {'notes': dict(zip('qzsedftgyhujkolpm', range(17))), 'oct': 'wx', 'vel': 'cv',
               'white': 'QSDFGHJKLM', 'black': 'ZE TYU OP'},
    'QWERTY': {'notes': dict(zip('awsedftgyhujkolp;', range(17))), 'oct': 'zx', 'vel': 'cv',
               'white': 'ASDFGHJKL;', 'black': 'WE TYU OP'}}
DEFAULT_AUDIO = {'output': '', 'maschine_audio': True, 'rate': 0, 'buffer': 96, 'input': ''}
DEFAULT_MIDI = {'in': '', 'in_mode': 'OMNI', 'out': '', 'clock': 0, 'transport': True, 'thru': False}
CLOCK_MODES = ('OFF', 'SEND', 'RECEIVE')
BUFFERS = (32, 64, 96, 128, 192, 256, 512)
ENGINE_REPLY_PORT = 57391                             # le moteur renvoie son etat ici
ENV_MODES = ('ONE-SHOT', 'ADSR')                      # enveloppe d'un son (comme Maschine)
RELEASES = (5, 10, 20, 50, 80, 120, 150, 200, 300, 500, 800, 1200, 2000, 3000)   # ms


# Kits de batterie : un dossier = un kit (fichiers audio ranges par nom -> pads 1 a 16)
KIT_DIRS = (Path(__file__).resolve().parent.parent / 'kits',        # prototypes/kits (kit de test)
            Path.home() / 'Music' / 'Fuzzy Kits')                    # tes kits
AUDIO_EXT = {'.wav', '.aif', '.aiff', '.flac', '.ogg', '.mp3', '.m4a'}


def kit_files(folder: Path) -> list:
    """Les 16 premiers fichiers audio du dossier, par ordre de nom (01 Kick, 02 Snare...)."""
    try:
        files = [f for f in folder.iterdir() if f.is_file() and f.suffix.lower() in AUDIO_EXT
                 and not f.name.startswith('.')]
    except OSError:
        return []
    return sorted(files, key=lambda f: f.name.lower())[:16]


def list_kits() -> list:
    """[(nom, dossier)] : les dossiers de KIT_DIRS qui contiennent des fichiers audio."""
    kits = []
    for base in KIT_DIRS:
        if base.is_dir():
            for d in sorted(base.iterdir(), key=lambda p: p.name.lower()):
                if d.is_dir() and kit_files(d):
                    kits.append((d.name, d))
    return kits


SYNTHS = ('Kick', 'Snare', 'Closed HH', 'Open HH', 'Tone')   # synthes du moteur (1-5)


def default_synth(sound: int) -> int:
    """Synthe par defaut d'un son : 1-4 batterie, 5-16 Tone."""
    return sound if sound <= 4 else 5


def sample_label(path: str) -> str:
    """Nom court d'un sample : '01 Kick.wav' -> 'Kick'."""
    stem = Path(path).stem.lstrip('0123456789 -_.') or Path(path).stem
    return stem[:11]


def default_env(sound: int) -> dict:
    """Batterie (sons 1-4) en ONE-SHOT ; notes (5-16) en ADSR : la note tient sa duree."""
    return {'mode': 'ONE-SHOT' if sound <= 4 else 'ADSR', 'release': 150}
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
        old = data.get('hues_v', 1) < 2                # anciens reglages : A2 = A (on garde le nouvel ordre)
        st.group_hue.update({g: int(h) for g, h in data.get('group_hue', {}).items()
                             if g in GROUPS and not (old and g.endswith('2'))})
        for g, k in data.get('keys', {}).items():
            if g in GROUPS:
                st.keys[g].update({n: int(v) for n, v in k.items() if n in st.keys[g]})
        st.linked = set(data.get('linked', [])) & set(GROUPS)
        for key, h in data.get('pad_hue', {}).items():
            g, p = key.split(':')
            st.pad_hue[(g, int(p))] = int(h)
        st.audio.update({k: v for k, v in data.get('audio', {}).items() if k in DEFAULT_AUDIO})
        st.project_name = data.get('project', st.project_name)
        midi = data.get('midi', {})
        st.midi.update({k: v for k, v in midi.items() if k in DEFAULT_MIDI})
        st.midi['out_ch'].update({g: int(c) for g, c in midi.get('out_ch', {}).items() if g in GROUPS})
        st.pc.update({k: v for k, v in data.get('pc', {}).items() if k in DEFAULT_PC})
        st.metro_vol = int(data.get('metro_vol', st.metro_vol))
    except (OSError, ValueError, KeyError, TypeError, AttributeError):
        pass


def write_text(path: Path, text: str) -> None:
    """Ecrit un fichier. S'il appartient a root (cree un jour avec sudo) et refuse l'ecriture,
    on le remplace : le dossier est a toi, donc l'ancien fichier peut etre supprime."""
    try:
        path.write_text(text)
    except PermissionError:
        path.unlink()
        path.write_text(text)


def save_settings(st) -> None:
    try:
        write_text(SETTINGS_FILE, json.dumps({
            'group_hue': st.group_hue, 'hues_v': 2,
            'pad_hue': {f'{g}:{p}': h for (g, p), h in st.pad_hue.items() if h is not None},
            'keys': st.keys, 'linked': sorted(st.linked),
            'audio': st.audio, 'midi': st.midi, 'project': st.project_name, 'pc': st.pc,
            'metro_vol': st.metro_vol},
            indent=1))
    except OSError as e:
        print('Sauvegarde des reglages impossible :', e)


def save_user_chords(st) -> None:
    try:
        write_text(USER_FILE, json.dumps({'banks': [[list(c) for c in b] for b in st.user_chords]},
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
        # Un pattern par groupe : 256 lignes x 16 pistes ; case = (note ou accord, volume)
        self.grids = {g: [[None] * 16 for _ in range(ROWS)] for g in GROUPS}
        demo = self.grids['A']
        for r in (0, 8, 10):
            demo[r][0] = (60, 0x7F)                            # Kick
        for r in (4, 12):
            demo[r][1] = (60, 0x70)                            # Snare
        for r in (2, 6, 14):
            demo[r][2] = (60, 0x50)                            # Closed HH
        # Patterns (bouton PATTERN) : 64 par groupe ; st.grids[g] = le pattern choisi du groupe
        self.patterns = {g: [None] * PAT_SLOTS for g in GROUPS}
        for g in GROUPS:
            self.patterns[g][0] = {'grid': self.grids[g], 'length': 32, 'name': ''}
        self.pat_index = {g: 0 for g in GROUPS}                # pattern choisi par groupe
        self.pat_queue: dict[str, int] = {}                    # changement a la fin du pattern
        self.pat_off: set[str] = set()                         # groupes sans pattern (silence)
        self.env = {(g, s): default_env(s) for g in GROUPS for s in range(1, 17)}
        self.rec_open: dict = {}               # pad tenu en REC : (ligne, piste, compteur, grille)
        self.plugin_mode = False               # PLUG-IN : enveloppe des sons (plusieurs a la fois)
        self.plugin_sel: set[int] = set()      # sons choisis avec SELECT + pad
        self.plugin_knobs = {0: Knob(40), 1: Knob(25), 4: Knob(40)}
        self.samples: dict = {}                # (groupe, son) -> fichier audio (sinon : synthese)
        self.kit_names = {g: '' for g in GROUPS}   # kit charge sur chaque groupe
        self.sent_samples: dict = {}           # ce que le moteur a deja recu
        self.loops: dict = {}                  # (groupe, son) -> boucle {'on', 'start', 'end'} (reglee dans DAW Pulses)
        self.reverse: set = set()              # (groupe, son) joues a l'envers (regle dans DAW Pulses)
        self.sent_reverse: dict = {}
        self.sent_loops: dict = {}             # boucles deja envoyees au moteur
        self.browse = None                     # PLUG-IN : liste ouverte ('kit' / 'synth' / None)
        self.kit_sel = 0
        self.synth_sel = 0
        self.synth: dict = {}                  # (groupe, son) -> synthe choisi (1-5, sinon : celui du pad)
        self.events_mode = False               # EVENTS : piano roll de la piste choisie
        self.pr_zoom = 32
        self.pr_low = 48
        self.pr_knobs = {0: Knob(20), 1: Knob(40), 2: Knob(25), 3: Knob(40),
                         4: Knob(30), 5: Knob(25), 6: Knob(10), 7: Knob(25)}
        self.pat_start = {g: 0 for g in GROUPS}                # debut du pattern (compteur)
        self.pattern_mode = False
        self.pat_bank = 0
        self.pat_knobs = {0: Knob(40), 7: Knob(12)}           # 1 banque, 8 longueur
        self.len_flash = 0.0                                   # affichage LENGTH (encodeur 8)
        # Scenes (bouton SCENE) : un pattern par groupe ; 64 scenes (4 banques de 16)
        self.scenes: list = [None] * PAT_SLOTS
        self.scene_current: int | None = None
        self.scene_mode = False
        self.scene_bank = 0
        self.scene_knobs = {0: Knob(40)}
        self.scene_names = [''] * PAT_SLOTS
        self.name_edit = None                    # nom en cours d'edition : ('pattern', g, i) / ('scene', i)
        self.name_pos = 0
        self.name_knobs = {4: Knob(40), 5: Knob(25), 6: Knob(40), 7: Knob(40)}   # sous l'ecran droit
        self.group_names = {g: '' for g in GROUPS}       # noms courts des groupes
        self.trk_left = False                    # Keyboard / Chords : tracker sur l'ecran gauche
        self.trk_page = 0                        # 0 = pistes 1-8, 1 = pistes 9-16
        self.clipboard = None                    # Shift + pad 11 / 12 : ('note', case) ou ('track', cases)
        self.last_fn = None                      # 2e Shift + pad rapide : toute la piste
        self.drag = None                         # molette 4D : appui long + rotation (deplacer / copier)
        self.last_click = None                   # molette 4D : (heure, ligne, piste) du dernier clic court
        self.clip_pattern = None                 # Shift + pad 11 / 12 en mode PATTERN
        self.dup_src = None                      # DUPLICATE tenu : ('sound', groupe, pad) ou ('group', groupe)
        self.clip_scene = None                   # Shift + pad 11 / 12 en mode SCENE
        self.file_mode = False                   # FILE : projets (enregistrer, ouvrir...)
        # ARRANGER : liste de scenes jouees dans l'ordre (mode SONG)
        self.arrangement: list = []              # [{'scene': index, 'repeats': n}, ...]
        self.arranger_mode = False
        self.arr_on = False                      # SONG : la lecture suit l'arrangement
        self.arr_loop = True                     # a la fin : recommencer (sinon : stop)
        self.arr_sel = 0                         # entree choisie dans la liste
        self.arr_pos = 0                         # entree qui joue
        self.arr_rows = 0                        # lignes deja jouees dans cette entree
        self.arr_len = 0                         # lignes de cette entree (repetitions comprises)
        self.arr_knobs = {0: Knob(40), 1: Knob(40), 2: Knob(70), 3: Knob(70)}   # 3-4 moins sensibles
        self.arr_half = False                    # bouton 3 HALF : repetitions / longueur par 0,5
        self.file_sel = 0
        self.file_ask = None                     # question YES / NO en cours (action, projet)
        self.file_moved = False                  # la liste a ete parcourue : SAVE vise ce projet
        self.project_name = 'SONG01'
        self.saved_text = ''                     # projet tel qu'enregistre (pour 'not saved')
        self.project_file = None                 # dossier du projet ouvert / enregistre
        self.toast = ('', 0.0)                   # court message a l'ecran (texte, fin)
        self.muted: set[str] = set()                           # MUTE + groupe
        self.solo: set[str] = set()                            # SOLO + groupe(s)
        self.sound_muted: set = set()                          # MUTE + pad : (groupe, son)
        self.sound_solo: set = set()                           # SOLO + pad(s) : (groupe, son)
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
        self.group_vol = {g: 100 for g in GROUPS}
        self.group_pan = {g: 64 for g in GROUPS}       # stereo des groupes (0 G, 64 centre, 127 D)
        self.mix_pan = False                           # MIXER : bouton 4 = vue PAN
        self.sound_vol = {(g, s): 100 for g in GROUPS for s in range(1, 17)}
        self.mix_knobs = [Knob(4) for _ in range(8)]
        # Settings (Shift + MIDI) : couleurs des groupes et des pads
        # 2e banque : couleurs de la 1re dans l'ordre inverse (A2 = couleur de H ... H2 = A)
        self.group_hue = dict(zip(GROUPS, GROUP_HUES + GROUP_HUES[::-1]))
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
        # SETTINGS : onglets COLOR / MASCHINE / AUDIO / MIDI (boutons 1-4 au-dessus des ecrans)
        self.set_tab = 'AUDIO'
        self.pc = dict(DEFAULT_PC)                     # SETTINGS > KEYS : clavier de l'ordinateur
        self.pc_knobs = {i: Knob(40) for i in range(4)}
        self.pc_oct = 0                                # octave du clavier (touches W / X ou Z / X)
        self.pc_vel = 100                              # velocite du clavier (touches C / V)
        self.device_info = ('Maschine MK3', '?')     # modele, firmware
        self.ni_paused = False
        self.exclusive = False
        self.status: list = []                       # onglet MASCHINE : (libelle, valeur, ok)
        self.status_time = 0.0
        self.quit_request = False                    # Shift + bouton 8 (MASCHINE) : rendre a NI
        self.audio = dict(DEFAULT_AUDIO)             # reglages AUDIO (enregistres)
        self.midi = {**DEFAULT_MIDI, 'out_ch': {g: 0 for g in GROUPS}}
        self.audio_state: dict = {}                  # etat renvoye par le moteur
        self.midi_state: dict = {}
        self.audio_pending = self.midi_pending = 0.0 # reglage a envoyer au moteur (heure du changement)
        self.engine_synced = False                   # reglages enregistres envoyes au demarrage
        self.audio_knobs = {i: Knob(40) for i in range(5)}
        self.midi_knobs = {i: Knob(40) for i in range(7)}
        self.midi_in_flash = self.midi_out_flash = 0.0
        self.card_types: dict[str, str] = {}         # carte son -> USB / Built-in / ... (macOS)
        self.audio_fallback = ''                     # sortie de secours si la carte choisie manque
        self.audio_notice = ''                       # message sur l'ecran AUDIO
        self.audio_action = 0.0                      # dernier changement automatique de sortie
        self.keep_output = False                     # --output impose
        self.known_cards: tuple = ()
        self.held_play: dict = {}                    # pad -> (frappe, velo, notes, groupe)
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
        self.pending: list = []                        # notes decalees a jouer (heure, groupe, piste, case)
        self.midi_held: set = set()                    # notes MIDI IN tenues (groupe, note)
        self.metro = False                             # TAP : metronome (lecture et enregistrement)
        self.metro_vol = 90                            # TAP tenu + molette 4D : volume du metronome
        self.tap_turned = False
        self.pad_shift = 0                             # Pad mode en lecture : Shift + 13-16 = transposer
        self.countin = None                            # REC tenu + PLAY : (debut, temps deja battus)
        self.pan_knobs = [Knob(10) for _ in range(8)]  # MIXER > PAN : plus doux, cran au centre
        self.pan_hold = [0] * 8
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

    @property
    def length(self) -> int:
        """Longueur (lignes) du pattern choisi du groupe choisi."""
        return self.patterns[self.group][self.pat_index[self.group]]['length']

    @length.setter
    def length(self, value: int) -> None:
        self.patterns[self.group][self.pat_index[self.group]]['length'] = value

    def sound_audible(self, g: str, s: int) -> bool:
        """Un son joue s'il n'est pas coupe et, si un Solo existe dans son groupe, s'il en fait partie."""
        if (g, s) in self.sound_muted:
            return False
        return (g, s) in self.sound_solo or not any(k[0] == g for k in self.sound_solo)

    def sound_tag(self, s: int, g: str | None = None) -> str:
        """'M' son coupe, 'S' son en solo, '' sinon."""
        key = (g or self.group, s)
        return 'M' if key in self.sound_muted else 'S' if key in self.sound_solo else ''

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

    def melodic_sound(self) -> int:
        """Son joue au clavier : le son choisi ; un son 1-4 de synthese de batterie -> 5."""
        if self.sound <= 4 and (self.group, self.sound) not in self.samples \
                and self.synth.get((self.group, self.sound), self.sound) <= 4:
            return 5
        return self.sound

    def eff_synth(self, g: str, s: int) -> int:
        """Synthe d'un son : choisi, sinon celui du pad (banque A-H) ; 0 = aucun instrument
        (banque A2-H2 vide au depart, ou 'None' choisi dans SYNTH)."""
        syn = self.synth.get((g, s))
        if syn is not None:
            return syn
        return 0 if g.endswith('2') else default_synth(s)

    def instrument(self, g: str, s: int) -> bool:
        """Le son a un instrument (sample, kit ou synthe) ? Sinon : muet, pad eteint."""
        return (g, s) in self.samples or self.eff_synth(g, s) != 0

    def group_filled(self, g: str) -> bool:
        return any(self.instrument(g, s) for s in range(1, 17))

    def sound_name(self, s: int, group: str | None = None) -> str:
        g = group or self.group
        path = self.samples.get((g, s))
        if path:                                     # sample d'un kit
            return sample_label(path)
        synth = self.eff_synth(g, s)
        if synth == 0:
            return 'Empty'
        return SYNTHS[synth - 1] if synth <= 4 else f'Note {note_name(default_note(s))}'


# --- Pads / boutons / LED par hidapi (mode partage, sans sudo) ----------------------
class HidDev:
    """Pads / boutons / LED a travers le pilote HID du systeme : rien n'est detache,
    la carte son de la Maschine reste disponible, pas besoin de sudo."""

    def __init__(self):
        try:
            import hid
        except ImportError:
            raise MK3Error('Module hidapi absent : python3 -m pip install hidapi')
        found = [d for d in hid.enumerate(VID, 0) if d['product_id'] in (PID_MK3, PID_PLUS)]
        cands = [d for d in found if d.get('interface_number') == HID_INTERFACE] or found
        if not cands:
            raise MK3Error('Maschine introuvable (branchee et allumee ?)')
        info = cands[0]
        self.product = 'Maschine+' if info['product_id'] == PID_PLUS else 'Maschine MK3'
        rel = info.get('release_number', 0) or 0
        self.firmware = f'{rel >> 8:x}.{rel & 0xFF:02x}' if rel else '?'
        self.dev = hid.device()
        try:
            self.dev.open_path(info['path'])
        except OSError as e:
            raise MK3Error(f'Pads indisponibles ({e}) : un autre programme a-t-il pris la Maschine ?')

    def read(self, timeout_ms: int = 50) -> bytes:
        data = self.dev.read(64, timeout_ms)
        return bytes(data) if data else b''

    def write(self, report: bytes) -> None:
        self.dev.write(report)

    def close(self) -> None:
        self.dev.close()


# --- Agent Native Instruments (macOS) : il tient les ecrans ---------------------------
NI_AGENT_APP = '/Library/Application Support/Native Instruments/Hardware/NIHostIntegrationAgent.app'


def ni_agent_running() -> bool:
    if sys.platform != 'darwin':
        return False
    return subprocess.run(['pgrep', '-x', 'NIHostIntegrationAgent'],
                          capture_output=True).returncode == 0


def ni_agent_pause() -> bool:
    """Arrete l'agent NI (il repart avec ni_agent_resume, ou au prochain demarrage du Mac)."""
    if not ni_agent_running():
        return False
    subprocess.run(['killall', 'NIHostIntegrationAgent'], capture_output=True)
    for _ in range(20):
        if not ni_agent_running():
            break
        time.sleep(0.1)
    time.sleep(0.3)                                    # le temps que l'interface se libere
    return True


def ni_agent_resume() -> None:
    subprocess.run(['open', '-g', NI_AGENT_APP], capture_output=True)
    print('Agent Native Instruments relance : la Maschine revient au logiciel NI.')


# --- Ancien acces exclusif par libusb (--exclusive, sudo) ---------------------------------
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
    groups = GROUPS[8 * st.mixer_page: 8 * st.mixer_page + 8]   # page 1 : A-H, page 2 : A2-H2
    return [(f'Group {g}', ('group', g)) for g in groups]


def mix_get(st: State, key) -> int:
    if st.mix_pan:                                   # vue PAN : stereo (Pan du son = PLUG-IN / Pad mode)
        return st.params[key[1:]][1] if key[0] == 'sound' else st.group_pan[key[1]]
    return st.sound_vol[key[1:]] if key[0] == 'sound' else st.group_vol[key[1]]


def mix_set(st: State, key, v: int) -> None:
    v = max(0, min(127, v))
    if st.mix_pan:
        if key[0] == 'sound':
            st.params[key[1:]][1] = v
        else:
            st.group_pan[key[1]] = v
    elif key[0] == 'sound':
        st.sound_vol[key[1:]] = v
    else:
        st.group_vol[key[1]] = v


def draw_mixer(st: State, half: int) -> Image.Image:
    """Mixeur sur les 2 ecrans : 4 voies par ecran, chacune sous son encodeur."""
    acc = group_rgb(st)
    img = Image.new('RGB', (W, H), (10, 10, 14))
    d = ImageDraw.Draw(img)
    what = f'{st.group} sounds' if not st.melodic() else 'Groups'
    # bandeau : couleur du groupe pour ses sons (Pad mode) ; neutre pour les groupes (chaque voie
    # a sa couleur, les pads montrent le groupe)
    neutral = acc if not st.melodic() else (58, 58, 66)
    if half == 0:                                    # boutons 3 TRACKER, 4 PAN
        header(d, f"{'PAN' if st.mix_pan else 'MIXER'}  {what}", '', neutral)
        keys = None if not st.melodic() else (34, 34, 40)
        softkey(d, st, 2, 'TRACKER', False, keys)
        softkey(d, st, 3, 'PAN', st.mix_pan, keys)
    else:
        header(d, f'MASTER {st.master}', f'page {st.mixer_page + 1}/2   < >', neutral)
    chans = mixer_channels(st)[half * 4: half * 4 + 4]
    for k, (name, key) in enumerate(chans):
        x = k * 120
        v = mix_get(st, key)
        # voie de groupe : la couleur de CE groupe (comme son bouton) ; le bandeau garde
        # la couleur du groupe choisi
        col = hue_rgb(st.group_hue[key[1]]) if key[0] == 'group' else acc
        if key[0] == 'sound':                         # son coupe / hors solo dans son groupe
            off = not st.sound_audible(key[1], key[2])
            tag = st.sound_tag(key[2], key[1])
        else:
            off = key[1] in GROUPS and not st.audible(key[1])
            tag = 'M' if off else ''
        d.text((x + 60 - d.textlength(name, font=font(13)) / 2, 34), name, font=font(13),
               fill=(120, 120, 120) if off else 'white')        # centre au-dessus de l'encodeur
        rounded(d, (x + 40, 58, x + 80, 230), outline=(70, 70, 80), r=6)
        if st.mix_pan:                               # PAN : gauche en haut, droite en bas
            mid, pos = 144, 60 + round(166 * v / 127)
            d.rectangle((x + 43, min(mid, pos), x + 77, max(mid, pos)), fill=(90, 90, 90) if off else col)
            d.line((x + 36, mid, x + 84, mid), fill=(220, 220, 220), width=1)   # centre
            d.text((x + 28, 60), 'L', font=font(12), fill=(170, 170, 170))
            d.text((x + 28, 214), 'R', font=font(12), fill=(170, 170, 170))
            val = 'C' if v == 64 else f'L{64 - v}' if v < 64 else f'R{v - 64}'
            d.text((x + 60 - d.textlength(val, font=font(14)) / 2, 238), val, font=font(14), fill='white')
            if tag:
                d.text((x + 86, 60), tag, font=font(14), fill=(200, 60, 60) if tag == 'M' else (240, 200, 40))
            continue
        hgt = round(168 * v / 127)
        if hgt:
            d.rectangle((x + 43, 228 - hgt, x + 77, 227), fill=(90, 90, 90) if off else col)
        d.line((x + 36, 228 - round(168 * 100 / 127), x + 84, 228 - round(168 * 100 / 127)),
               fill=(220, 220, 220), width=1)            # repere du niveau normal (100)
        val = str(v)                                 # centre sous la voie
        d.text((x + 60 - d.textlength(val, font=font(14)) / 2, 238), val, font=font(14), fill='white')
        if tag:
            d.text((x + 86, 60), tag, font=font(14), fill=(200, 60, 60) if tag == 'M' else (240, 200, 40))
    return img


# --- SETTINGS : 4 onglets (boutons 1-4) ---------------------------------------------
def tab_bar(d: ImageDraw.ImageDraw, st: State) -> None:
    """Onglets COLOR / MASCHINE / AUDIO / MIDI sous les boutons 1-4."""
    for k, tab in enumerate(SET_TABS):
        if tab == 'COLORS' and st.set_tab == 'KEYS':
            tab = 'KEYS'                               # bouton 3 : COLORS, 2e appui KEYS
        softkey(d, st, k, tab, st.set_tab == tab)


def param_box(d: ImageDraw.ImageDraw, slot: int, name: str, value: str, acc, y: int = 150,
              dim: bool = False) -> None:
    """Case d'un reglage, sous l'encodeur `slot` (0-3) de cet ecran."""
    x = slot * 120
    rounded(d, (x + 6, y, x + 114, y + 86), fill=(24, 24, 32), outline=shade(acc, 0.6))
    d.text((x + 14, y + 8), name, font=font(13), fill=(160, 160, 160))
    f = font(16)
    while value and d.textlength(value, font=f) > 96:   # couper pour rester dans la case
        value = value[:-2] + '.'
    d.text((x + 14, y + 40), value, font=f, fill=(90, 90, 90) if dim else 'white')


def dot(d: ImageDraw.ImageDraw, x: int, y: int, state) -> None:
    """Temoin : vert = OK, rouge = probleme, orange = a surveiller / information."""
    color = {True: (40, 200, 90), False: (220, 50, 50)}.get(state, (240, 160, 30))
    d.ellipse((x, y, x + 12, y + 12), fill=color)


def short(name: str, n: int = 34) -> str:
    return name if len(name) <= n else name[:n - 1] + '.'


def audio_outputs(st: State) -> list:
    outs = st.audio_state.get('outputs', [])
    if not st.audio['maschine_audio']:
        outs = [o for o in outs if 'maschine' not in o.lower()]
    return outs


def draw_settings(st: State, half: int) -> Image.Image:
    acc = group_rgb(st)
    img = Image.new('RGB', (W, H), (10, 10, 14))
    d = ImageDraw.Draw(img)
    tab = st.set_tab
    if half == 0:
        tab_bar(d, st)
    if tab == 'COLORS':
        draw_set_color(st, d, half, acc)
    elif tab == 'KEYS':
        draw_set_keys(st, d, half, acc)
    elif tab == 'STATUS':
        draw_set_maschine(st, d, half, acc)
    elif tab == 'AUDIO':
        draw_set_audio(st, d, half, acc)
    else:
        draw_set_midi(st, d, half, acc)
    if half == 1 and st.set_confirm:                   # fermer : sauvegarder ?
        d.rectangle((0, 0, W, 26), fill=(10, 10, 14))
        softkey(d, st, 0, 'YES', False)
        softkey(d, st, 1, 'NO', False)
        rounded(d, (70, 90, 410, 180), fill=(14, 14, 20), outline=acc, width=2)
        d.text((100, 105), 'Save the settings?', font=font(20), fill='white')
        d.text((100, 140), 'Button 5 = YES    Button 6 = NO', font=font(13), fill=(180, 180, 180))
    return img


def draw_set_keys(st: State, d, half: int, acc) -> None:
    """SETTINGS > KEYS : clavier de l'ordinateur (jouer, barre d'espace)."""
    pc, lay = st.pc, PC_LAYOUTS[st.pc['layout']]
    if half == 0:
        d.text((10, 36), 'COMPUTER KEYBOARD', font=font(14), fill=acc)
        lines = ['Keys of the computer play the chosen sound',
                 '(the terminal window must be in front).',
                 'Same switch: button 6 in KEYBOARD.',
                 'Button 3 again: COLORS.']
        for k, line in enumerate(lines):
            d.text((10, 60 + k * 20), line, font=font(13), fill=(190, 190, 190))
        param_box(d, 0, 'Play keys', 'ON' if pc['keys'] else 'OFF', acc, y=150)
        param_box(d, 1, 'Layout', pc['layout'], acc, y=150)
        param_box(d, 2, 'Space bar', 'PLAY/STOP' if pc['space'] else 'OFF', acc, y=150)
        param_box(d, 3, 'Note length', f"{pc['length']} ms", acc, y=150)
        return
    header(d, 'KEY MAP', pc['layout'], acc)
    white, black = lay['white'], lay['black']
    names = ['C', 'D', 'E', 'F', 'G', 'A', 'B', 'C', 'D', 'E']
    for k, key in enumerate(white):                    # touches blanches
        x = 10 + k * 46
        rounded(d, (x, 96, x + 42, 160), fill=(225, 225, 225), outline=(90, 90, 90))
        d.text((x + 21 - d.textlength(key, font=font(16)) / 2, 108), key, font=font(16), fill='black')
        d.text((x + 21 - d.textlength(names[k], font=font(12)) / 2, 138), names[k], font=font(12),
               fill=(90, 90, 90))
    for k, key in enumerate(black):                    # touches noires, entre les blanches
        if key == ' ':
            continue
        x = 10 + k * 46 + 25
        rounded(d, (x, 40, x + 38, 90), fill=(30, 30, 36), outline=acc)
        d.text((x + 19 - d.textlength(key, font=font(16)) / 2, 54), key, font=font(16), fill='white')
    o, v = lay['oct'].upper(), lay['vel'].upper()
    d.text((10, 176), f'{o[0]} / {o[1]} : octave - / +      {v[0]} / {v[1]} : velocity - / +',
           font=font(13), fill=(190, 190, 190))
    d.text((10, 198), 'Space bar : PLAY / STOP' if pc['space'] else 'Space bar : off',
           font=font(13), fill=(190, 190, 190))
    d.text((10, 222), f'Octave {st.octave + st.pc_oct}   Velocity {st.pc_vel}   Sound {st.sound:02d}',
           font=font(13), fill=acc)


def draw_set_color(st: State, d, half: int, acc) -> None:
    if half == 0:
        d.text((10, 36), f'COLORS  Group {st.group}', font=font(14), fill=acc)
        d.text((W - 170, 36), 'Button 3 again: KEYS', font=font(12), fill=(140, 140, 140))
        d.text((10, 58), f'Target : {st.set_target}', font=font(20), fill='white')
        if st.set_target == 'GROUP':
            lines = ['Pads = the 16 colors.', 'Tap a pad: color of group ' + st.group + '.',
                     'Group buttons: choose the group.']
        else:
            lines = ['Tap pads to select them (several OK).', 'Encoder 2: color of the selection.',
                     'Shift + pad: back to the group color.']
            sel = ', '.join(str(p) for p in sorted(st.set_selected)) or '-'
            d.text((10, 170), f'Selected : {sel}', font=font(14), fill=acc)
        for k, line in enumerate(lines):
            d.text((10, 94 + k * 22), line, font=font(14), fill=(190, 190, 190))
        for k, t in enumerate(['Target', 'Color', 'SETTINGS = close', '']):
            d.text((k * 120 + 60 - d.textlength(t, font=font(12)) / 2, 254), t, font=font(12), fill=(160, 160, 160))
        return
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


def draw_set_maschine(st: State, d, half: int, acc) -> None:
    if half == 0:
        d.text((10, 32), 'MASCHINE STATUS', font=font(14), fill=acc)
        if not st.status:
            d.text((10, 70), 'Checking...', font=font(16), fill=(180, 180, 180))
        for k, (label, value, ok) in enumerate(st.status[:9]):
            y = 54 + k * 22
            dot(d, 10, y + 3, ok)
            d.text((30, y), label, font=font(13), fill=(170, 170, 170))
            d.text((200, y), short(value, 34), font=font(13), fill='white')
        return
    header(d, 'STATUS', time.strftime('%H:%M:%S', time.localtime(st.status_time))
           if st.status_time else '', acc)
    softkey(d, st, 0, 'REFRESH', False)
    softkey(d, st, 3, 'TO NI', 'shift' in st.held)
    lines = ['Button 5 : check again.',
             'Shift + button 8 : give the Maschine',
             'back to NI (no unplugging needed).']
    for k, line in enumerate(lines):
        d.text((12, 36 + k * 20), line, font=font(13), fill=(190, 190, 190))
    # Ordinateur : processeur, memoire, charge audio
    c, a = getattr(st, 'computer', {}), st.audio_state
    d.text((12, 104), 'COMPUTER', font=font(14), fill=acc)
    rows = []
    if 'cpu' in c:
        rows.append(('CPU', f"{c['cpu']:.0f} %", c['cpu'] < 70 or (None if c['cpu'] < 90 else False)))
    if 'mem_used' in c:
        free = c['mem_total'] - c['mem_used']
        rows.append(('Memory', f"{c['mem_used']:.1f} / {c['mem_total']:.0f} GB",
                     True if free > 2 else (None if free > 0.5 else False)))
    if a.get('cpu') is not None:
        rows.append(('Audio engine', f"{a['cpu']:.0f} % (sound calculation)",
                     a['cpu'] < 50 or (None if a['cpu'] < 80 else False)))
    if 'rss_mb' in c:
        rows.append(('This program', f"{c['rss_mb']:.0f} MB", True))
    if not rows:
        rows.append(('CPU / memory', 'not available', None))
    for k, (label, value, ok) in enumerate(rows):
        y = 128 + k * 22
        dot(d, 12, y + 3, ok)
        d.text((32, y), label, font=font(13), fill=(170, 170, 170))
        d.text((150, y), value, font=font(13), fill='white')
    d.text((12, 250), 'Green = OK   Orange = info   Red = problem', font=font(11), fill=(140, 140, 140))


def draw_set_audio(st: State, d, half: int, acc) -> None:
    a, s = st.audio, st.audio_state
    if half == 0:
        d.text((10, 32), 'AUDIO', font=font(14), fill=acc)
        if not s:
            d.text((10, 60), 'Audio engine not running', font=font(16), fill=(220, 80, 80))
            d.text((10, 86), '(started without sound, or not compiled)', font=font(13), fill=(160, 160, 160))
        else:
            d.text((10, 58), short('Output : ' + (s.get('device') or '-'), 44), font=font(15), fill='white')
            xr = s.get('xruns', -1)
            d.text((10, 82), f"{s.get('rate', 0):.0f} Hz   buffer {s.get('buffer', 0)}   "
                             f"latency {s.get('latency', 0):.1f} ms" + (f'   drops {xr}' if xr >= 0 else ''),
                   font=font(13), fill=(190, 190, 190))
            pending = st.audio_pending and time.monotonic() - st.audio_pending < 2
            if pending:
                d.text((10, 106), 'Applying...', font=font(12), fill=acc)
            elif st.audio_notice:
                d.text((10, 106), short(st.audio_notice, 52), font=font(12), fill=(240, 160, 30))
            else:
                d.text((10, 106), 'Turn an encoder: applied after 1 s', font=font(12), fill=(140, 140, 140))
        out = (st.audio_fallback or a['output'] or s.get('device', '')) if s else (a['output'] or 'default')
        param_box(d, 0, 'Output', out or 'default', acc)
        param_box(d, 1, 'Maschine', 'ON' if a['maschine_audio'] else 'OFF', acc)
        param_box(d, 2, 'Rate', f"{a['rate'] / 1000:g} kHz" if a['rate'] else 'auto', acc)
        param_box(d, 3, 'Buffer', f"{a['buffer']}", acc)
        return
    header(d, 'SOUND CARDS', 'internal Maschine: ' + ('ON' if a['maschine_audio'] else 'OFF'), acc)
    cards = sound_cards(st)
    if not cards:
        d.text((12, 40), 'No list yet (audio engine not running?)', font=font(13), fill=(160, 160, 160))
    current = s.get('device', '')
    missing = a['output'] if a['output'] and cards and a['output'] not in cards else ''
    if missing:
        cards = cards + [missing]                      # carte choisie mais debranchee
    for k, name in enumerate(cards[:5] if not missing else cards[:4] + [missing]):
        y = 34 + k * 22
        used = name == current
        kind = 'absent' if name == missing else st.card_types.get(name, '')
        if used:
            d.rectangle((6, y - 1, W - 6, y + 19), fill=shade(acc, 0.45))
        d.text((12, y), ('> ' if used else '  ') + short(name, 30), font=font(14),
               fill='white' if used else (200, 200, 200))
        if kind:
            d.text((372, y), kind, font=font(13), fill='white' if used else (150, 150, 150))
        if name == missing:
            d.text((372, y), 'absent', font=font(13), fill=(240, 160, 30))
    if len(cards) > 5:
        d.text((12, 34 + 5 * 22), f'+ {len(cards) - 5} more', font=font(12), fill=(140, 140, 140))
    param_box(d, 0, 'Input', a['input'] or 'None', acc, y=160)
    d.text((132, 172), 'Input: saved now, used later', font=font(12), fill=(150, 150, 150))
    d.text((132, 190), 'for sampling.', font=font(12), fill=(150, 150, 150))
    d.text((132, 214), 'Plug a card: listed in 2 s.', font=font(12), fill=(150, 150, 150))
    d.text((132, 232), 'Tap pads to test the sound.', font=font(12), fill=(150, 150, 150))


def draw_set_midi(st: State, d, half: int, acc) -> None:
    m, s = st.midi, st.midi_state
    now = time.monotonic()
    if half == 0:
        d.text((10, 32), 'MIDI', font=font(14), fill=acc)
        if not s:
            d.text((10, 60), 'Audio engine not running (it handles MIDI)', font=font(14), fill=(220, 80, 80))
        rows = [('IN', s.get('cur_in') or 'none', now - st.midi_in_flash < 0.6),
                ('OUT', s.get('cur_out') or 'none', now - st.midi_out_flash < 0.6)]
        for k, (name, port, active) in enumerate(rows if s else []):
            y = 58 + k * 26
            d.ellipse((10, y + 3, 22, y + 15), fill=(40, 220, 90) if active else (50, 50, 60))
            d.text((30, y), f'{name:4s}{short(port, 40)}', font=font(15), fill='white')
        d.text((10, 116), 'IN mode OMNI: notes play the selected group.' if m['in_mode'] == 'OMNI'
               else 'IN mode CH>GROUP: ch 1-8 = A-H, 9-16 = A2-H2.', font=font(12), fill=(160, 160, 160))
        ch = m['out_ch'][st.group]
        param_box(d, 0, 'IN port', m['in'] or 'None', acc)
        param_box(d, 1, 'IN mode', m['in_mode'], acc)
        param_box(d, 2, 'OUT port', m['out'] or 'None', acc)
        param_box(d, 3, f'OUT grp {st.group}', 'Internal' if not ch else f'Ch {ch}', acc)
        return
    clock = CLOCK_MODES[m['clock']]
    info = (f"receiving {s.get('bpm', 0):.1f} BPM" if clock == 'RECEIVE' and s.get('bpm')
            else f'sending {st.bpm} BPM' if clock == 'SEND' else 'clock off')
    header(d, 'MIDI SYNC', info, acc)
    d.text((12, 34), 'Groups sent to MIDI OUT (encoder 4, group buttons):', font=font(12), fill=(160, 160, 160))
    for k, g in enumerate(GROUPS):                     # 16 groupes : 4 lignes de 4
        x, y = 12 + (k % 4) * 116, 52 + (k // 4) * 25
        ch = m['out_ch'][g]
        rounded(d, (x, y, x + 108, y + 22), fill=acc if g == st.group else (24, 24, 32),
                outline=shade(acc, 0.6), r=6)
        d.text((x + 8, y + 4), f"{g:3s}{'int' if not ch else 'ch ' + str(ch)}", font=font(12),
               fill='black' if g == st.group else 'white')
    param_box(d, 0, 'Clock', clock, acc)
    param_box(d, 1, 'Start/Stop', 'ON' if m['transport'] else 'OFF', acc)
    param_box(d, 2, 'Thru', 'ON' if m['thru'] else 'OFF', acc)


CARD_TYPES = {'usb': 'USB', 'builtin': 'Built-in', 'bluetooth': 'Bluetooth', 'virtual': 'Virtual',
              'hdmi': 'HDMI', 'displayport': 'DisplayPort', 'thunderbolt': 'Thunderbolt',
              'aggregate': 'Aggregate', 'airplay': 'AirPlay', 'pci': 'PCI'}


def scan_card_types(st: State) -> None:
    """Type de chaque carte son (USB, integree...) : macOS seulement (system_profiler)."""
    if sys.platform != 'darwin':
        return
    try:
        out = subprocess.run(['system_profiler', 'SPAudioDataType', '-json'], capture_output=True,
                             text=True, timeout=10).stdout
        types = {}
        for group in json.loads(out).get('SPAudioDataType', []):
            for dev in group.get('_items', []):
                t = str(dev.get('coreaudio_device_transport', ''))
                key = t.replace('coreaudio_device_type_', '')
                types[dev.get('_name', '')] = CARD_TYPES.get(key, key.capitalize() or '?')
        st.card_types = types
    except (OSError, subprocess.SubprocessError, ValueError):
        pass


def sound_cards(st: State) -> list:
    """Toutes les cartes son vues par le moteur (sorties et entrees), sans doublon."""
    s = st.audio_state
    return list(dict.fromkeys(s.get('outputs', []) + s.get('inputs', [])))


def fallback_output(st: State, outs: list) -> str:
    """Carte choisie absente : la Maschine (si ON), sinon le Mac, sinon la premiere."""
    if st.audio['maschine_audio']:
        for o in outs:
            if 'maschine' in o.lower():
                return o
    for o in outs:
        if st.card_types.get(o) == 'Built-in':
            return o
    for o in outs:
        if 'maschine' not in o.lower():
            return o
    return outs[0] if outs else ''


def ensure_output(st: State) -> None:
    """A chaque etat du moteur : carte choisie absente -> secours ; revenue -> on la reprend."""
    s, a = st.audio_state, st.audio
    outs, current = s.get('outputs', []), s.get('device', '')
    now = time.monotonic()
    if not outs or st.audio_pending or now - st.audio_action < 3:
        return
    wanted = a['output']
    blocked = not a['maschine_audio'] and 'maschine' in current.lower()
    if wanted and wanted in outs:
        if current != wanted:                          # la carte est (re)venue
            if st.audio_fallback:
                st.audio_notice = f'{wanted} back'
                print(f'Carte son : {wanted} de retour.')
            st.audio_fallback = ''
            st.audio_action = now
            send_audio(st)
        return
    if (wanted and wanted not in outs) or blocked or not current:
        fb = fallback_output(st, outs)
        if not fb:
            return
        notice = f'{wanted} absent -> {fb}' if wanted and wanted not in outs else f'Output -> {fb}'
        if (st.audio_fallback, st.audio_notice) != (fb, notice):
            st.audio_fallback, st.audio_notice = fb, notice
            print(f'Carte son : {notice}')
        if fb != current:
            st.audio_action = now
            send_audio(st, override=fb)


def maschine_status(st: State) -> None:
    """Onglet MASCHINE : etat de la Maschine et de son environnement (dans une tache a part :
    ioreg prend un instant)."""
    product, firmware = st.device_info
    outs = st.audio_state.get('outputs', [])
    items = [('Controller', f'{product}  firmware {firmware}', True),
             ('Pads, buttons, lights', 'OK (exclusive, sudo)' if st.exclusive else 'OK (shared)', True),
             ('Screens', 'OK', True)]
    if sys.platform == 'darwin':
        running = ni_agent_running()
        items.append(('NI agent', 'paused by Fuzzy' if st.ni_paused and not running
                      else 'running' if running else 'not running', True if not running else None))
    cards = sound_cards(st)
    if st.card_types:
        usb = [c for c in cards if st.card_types.get(c) == 'USB']
        items.append(('USB sound cards', ', '.join(usb) or 'none', True if usb else None))
    elif cards:
        items.append(('Sound cards', ', '.join(cards), True))
    card = any('maschine' in o.lower() for o in outs)
    items.append(('Maschine sound card', 'present' if card else ('absent' if outs else 'unknown'),
                  True if card else (False if outs else None)))
    s = st.audio_state
    items.append(('Audio engine', short(f"{s.get('device', '')}  {s.get('rate', 0):.0f} Hz", 30)
                  if s else 'not running', bool(s)))
    cal = Path.home() / '.config' / 'fuzzy-turtle' / ('maschine-plus.conf' if 'Maschine+' in product
                                                       else 'maschine-mk3.conf')
    items.append(('Calibration file', 'found' if cal.exists() else 'default values',
                  True if cal.exists() else None))
    if sys.platform == 'darwin':
        others = set()
        try:
            out = subprocess.run(['ioreg', '-w0', '-r', '-n', product], capture_output=True,
                                 text=True, timeout=3).stdout
            for line in out.splitlines():
                if 'UserClient' in line and '+-o ' in line:
                    name = line.split('+-o ', 1)[1].split('  <class', 1)[0].strip()
                    if name not in ('MIDIServer', 'NIHostIntegratio', 'Python', 'python3',
                                    'Python3', 'fuzzy_pad_sound', 'Fuzzy Pad Sound',
                                    'IOHIDLibUserClient', 'IOAudioEngineUserClient'):
                        others.add(name)
        except (OSError, subprocess.SubprocessError):
            pass
        items.append(('Other apps on it', ', '.join(sorted(others)) or 'none', None if others else True))
    st.computer = computer_stats()
    st.status, st.status_time = items, time.time()


def computer_stats() -> dict:
    """CPU et memoire de l'ordinateur (macOS : ps, vm_stat, sysctl ; Linux : /proc)."""
    out = {}
    try:
        import resource
        rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        out['rss_mb'] = rss / (1024 * 1024) if sys.platform == 'darwin' else rss / 1024
    except (ImportError, OSError):
        pass
    try:
        if sys.platform == 'darwin':
            ps = subprocess.run(['ps', '-A', '-o', '%cpu='], capture_output=True, text=True, timeout=2).stdout
            out['cpu'] = sum(float(v) for v in ps.split() if v.replace('.', '', 1).isdigit()) / (os.cpu_count() or 1)
            total = int(subprocess.run(['sysctl', '-n', 'hw.memsize'], capture_output=True, text=True,
                                       timeout=2).stdout.strip())
            vm = subprocess.run(['vm_stat'], capture_output=True, text=True, timeout=2).stdout
            page = int(vm.split('page size of ')[1].split()[0]) if 'page size of ' in vm else 4096
            pages = {l.split(':')[0].strip(): int(l.split(':')[1].strip().rstrip('.'))
                     for l in vm.splitlines()[1:] if ':' in l and l.split(':')[1].strip().rstrip('.').isdigit()}
            used = sum(pages.get(k, 0) for k in ('Pages active', 'Pages wired down',
                                                  'Pages occupied by compressor')) * page
            out['mem_used'], out['mem_total'] = used / 2**30, total / 2**30
        else:
            load = os.getloadavg()[0]
            out['cpu'] = 100 * load / (os.cpu_count() or 1)
            info = {l.split(':')[0]: int(l.split()[1]) for l in Path('/proc/meminfo').read_text().splitlines()}
            out['mem_total'] = info['MemTotal'] / 2**20
            out['mem_used'] = (info['MemTotal'] - info.get('MemAvailable', 0)) / 2**20
    except (OSError, ValueError, IndexError, KeyError, subprocess.SubprocessError):
        pass
    return out


def refresh_status(st: State) -> None:
    threading.Thread(target=maschine_status, args=(st,), daemon=True).start()


def open_settings(st: State) -> None:
    import copy
    st.settings, st.set_confirm = True, False
    st.set_backup = (dict(st.group_hue), dict(st.pad_hue), copy.deepcopy(st.audio), copy.deepcopy(st.midi),
                     dict(st.pc))
    st.set_selected.clear()
    if st.set_tab == 'STATUS':
        refresh_status(st)


def settings_button(st: State, name: str) -> bool:
    """Boutons dans SETTINGS (hors YES / NO). Renvoie True si le bouton est utilise ici."""
    if name == 'screen_3' and st.set_tab in ('COLORS', 'KEYS'):
        st.set_tab = 'KEYS' if st.set_tab == 'COLORS' else 'COLORS'   # 2e appui : l'autre page
        return True
    if name in ('screen_1', 'screen_2', 'screen_3', 'screen_4'):
        st.set_tab = SET_TABS[int(name[-1]) - 1]
        if st.set_tab == 'STATUS':
            refresh_status(st)
        return True
    if st.set_tab == 'STATUS' and name == 'screen_5':
        refresh_status(st)
        return True
    if st.set_tab == 'STATUS' and name == 'screen_8':
        if 'shift' in st.held:
            st.quit_request = True                 # quitter et rendre la Maschine a NI
        return True
    return name.startswith('screen_')              # les autres boutons d'ecran : rien ici


def cycle(options: list, current, n: int):
    if not options:
        return current
    i = options.index(current) if current in options else -1
    return options[(i + n) % len(options)] if i >= 0 or n else options[0]


def settings_knob(st: State, i: int, delta: int) -> None:
    """Encodeurs des onglets AUDIO (1-5), MIDI (1-7) et KEYS (1-4)."""
    if st.set_tab == 'KEYS' and i in st.pc_knobs:
        n = st.pc_knobs[i].steps(delta)
        if not n:
            return
        pc = st.pc
        if i == 0:
            pc['keys'] = not pc['keys']
            if pc['keys']:
                print_pc_keys(st)
        elif i == 1:
            pc['layout'] = 'QWERTY' if pc['layout'] == 'AZERTY' else 'AZERTY'
        elif i == 2:
            pc['space'] = not pc['space']
        else:
            k = PC_LENGTHS.index(pc['length']) if pc['length'] in PC_LENGTHS else 3
            pc['length'] = PC_LENGTHS[max(0, min(len(PC_LENGTHS) - 1, k + n))]
        return
    if st.set_tab == 'AUDIO' and i in st.audio_knobs:
        n = st.audio_knobs[i].steps(delta)
        if not n:
            return
        a, s = st.audio, st.audio_state
        if i == 0:
            outs = audio_outputs(st)
            a['output'] = cycle(outs, s.get('device', '') if st.audio_fallback else
                                (a['output'] or s.get('device', '')), n)
            st.audio_fallback, st.audio_notice = '', ''
        elif i == 1:
            a['maschine_audio'] = not a['maschine_audio']
            if not a['maschine_audio'] and 'maschine' in (a['output'] or s.get('device', '')).lower():
                a['output'] = next(iter(audio_outputs(st)), '')
        elif i == 2:
            rates = [r for r in s.get('rates', []) if r in (44100, 48000, 88200, 96000)] or [44100, 48000]
            a['rate'] = cycle(rates, a['rate'] or int(s.get('rate', 0)), n)
        elif i == 3:
            avail = s.get('buffers', [])
            sizes = [b for b in BUFFERS if not avail or b in avail] or list(BUFFERS)
            a['buffer'] = cycle(sizes, a['buffer'], n)
        elif i == 4:
            a['input'] = cycle([''] + s.get('inputs', []), a['input'], n)
            return                                     # entree : enregistree, pas encore utilisee
        st.audio_pending = time.monotonic()
    elif st.set_tab == 'MIDI' and i in st.midi_knobs:
        n = st.midi_knobs[i].steps(delta)
        if not n:
            return
        m, s = st.midi, st.midi_state
        if i == 0:
            m['in'] = cycle([''] + s.get('ins', []), m['in'], n)
        elif i == 1:
            m['in_mode'] = 'CH>GROUP' if m['in_mode'] == 'OMNI' else 'OMNI'
        elif i == 2:
            m['out'] = cycle([''] + s.get('outs', []), m['out'], n)
        elif i == 3:
            m['out_ch'][st.group] = (m['out_ch'][st.group] + n) % 17      # 0 = interne, 1-16
        elif i == 4:
            m['clock'] = (m['clock'] + n) % len(CLOCK_MODES)
        elif i == 5:
            m['transport'] = not m['transport']
        elif i == 6:
            m['thru'] = not m['thru']
        st.midi_pending = time.monotonic()


def send_audio(st: State, override: str = '') -> None:
    """Envoie les reglages audio (seulement les valeurs que la carte connait)."""
    a, s = st.audio, st.audio_state
    output = override or (a['output'] if a['output'] in s.get('outputs', []) else '')
    rate = a['rate'] if a['rate'] in s.get('rates', []) else 0
    SOUND.send('/fuzzy/proto/audio/set', output, a['input'], int(rate), int(a['buffer']))


def send_midi(st: State) -> None:
    m = st.midi
    SOUND.send('/fuzzy/proto/midi/set', m['in'], m['out'], int(m['clock']), int(m['transport']),
               int(m['thru']))


def sync_samples(st: State) -> None:
    """Envoie au moteur les samples qui ont change (fichier, ou '' = retour a la synthese)."""
    for key in set(st.samples) | set(st.sent_samples):
        path = st.samples.get(key, '')
        if st.sent_samples.get(key, '') != path:
            g, snd = key
            SOUND.send('/fuzzy/proto/sample', GROUPS.index(g), snd, path, default_note(snd))
            if path:
                st.sent_samples[key] = path
            else:
                st.sent_samples.pop(key, None)
    # A l'envers (Reverse, regle dans DAW Pulses)
    for key in set(st.reverse) | set(st.sent_reverse):
        on = key in st.reverse
        if st.sent_reverse.get(key) != on:
            SOUND.send('/fuzzy/proto/sample/reverse', GROUPS.index(key[0]), key[1], int(on))
            st.sent_reverse[key] = on
    # Boucles (apres les samples : le moteur garde l'ordre des messages)
    for key in set(st.loops) | set(st.sent_loops):
        lp = st.loops.get(key, {'on': False, 'start': 0.0, 'end': 1.0})
        val = (bool(lp['on']), float(lp['start']), float(lp['end']))
        if st.sent_loops.get(key) != val:
            g, snd = key
            SOUND.send('/fuzzy/proto/sample/loop', GROUPS.index(g), snd, int(val[0]), val[1], val[2])
            st.sent_loops[key] = val


def load_kit(st: State, folder: Path) -> None:
    """Charge un kit sur les 16 pads du groupe : fichiers par ordre de nom, en ONE-SHOT."""
    files = kit_files(folder)
    for snd in range(1, 17):
        if snd <= len(files):
            st.samples[(st.group, snd)] = str(files[snd - 1])
            st.env[(st.group, snd)]['mode'] = 'ONE-SHOT'     # une batterie joue jusqu'au bout
        else:
            st.samples.pop((st.group, snd), None)
    st.kit_names[st.group] = folder.name
    sync_samples(st)
    st.toast = (f'Kit {folder.name}: {len(files)} sounds on group {st.group}', time.monotonic() + 2)


def print_pc_keys(st: State) -> None:
    """Affiche le clavier musical dans le terminal (ecran de l'ordinateur)."""
    lay = PC_LAYOUTS[st.pc['layout']]
    o, v = lay['oct'].upper(), lay['vel'].upper()
    print('\nClavier de l\'ordinateur ACTIF (' + st.pc['layout'] + ') - la fenetre du terminal doit etre devant')
    top = [' '] * 40
    for k, key in enumerate(lay['black']):            # touches noires entre les blanches
        top[3 + 4 * k] = key
    print(''.join(top).rstrip())
    print(' ' + '   '.join(lay['white']))
    print(' C   D   E   F   G   A   B   C   D   E')
    print(f' {o[0]} / {o[1]} : octave - / +    {v[0]} / {v[1]} : velocite - / +    espace : PLAY / STOP\n')


def pc_key(st: State, ch: str) -> None:
    """Une touche du clavier de l'ordinateur (lue dans le terminal)."""
    if ch == ' ' and st.pc['space']:                  # barre d'espace : PLAY / STOP
        st.playing = not st.playing
        st.step_manual = None
        st.next_row_time = time.monotonic()
        return
    if not st.pc['keys']:
        return
    lay, c = PC_LAYOUTS[st.pc['layout']], ch.lower()
    now = time.monotonic()
    if c in lay['oct']:                               # octave - / +
        st.pc_oct = max(-st.octave, min(8 - st.octave, st.pc_oct + (1 if c == lay['oct'][1] else -1)))
        st.toast = (f'Keys: octave {st.octave + st.pc_oct}', now + 0.8)
    elif c in lay['vel']:                             # velocite - / +
        st.pc_vel = max(1, min(127, st.pc_vel + (16 if c == lay['vel'][1] else -16)))
        st.toast = (f'Keys: velocity {st.pc_vel}', now + 2)
    elif c in lay['notes']:
        note = max(0, min(127, 12 * (st.octave + st.pc_oct + 1) + lay['notes'][c]))
        snd = st.sound
        v = out_vel(st, st.group, snd, st.pc_vel)
        if v and st.audible(st.group):
            play_note(st, st.group, snd, v, note, st.pc['length'] / 1000)
        st.note_flash[note] = (now + 0.15, st.pc_vel)
        if snd <= 16:
            st.flash[snd] = now + 0.1
        if st.recording:
            record(st, snd, note, st.pc_vel)


def pc_keys_listener(events: queue.Queue, running: threading.Event) -> None:
    """Lit le clavier de l'ordinateur dans le terminal (touche par touche, sans Entree)."""
    if not sys.stdin.isatty():
        return
    import termios, tty, select
    fd = sys.stdin.fileno()
    try:
        old = termios.tcgetattr(fd)
    except termios.error:
        return
    atexit.register(lambda: termios.tcsetattr(fd, termios.TCSADRAIN, old))   # meme en cas d'erreur
    try:
        tty.setcbreak(fd)                             # Ctrl+C fonctionne toujours
        while running.is_set():
            ready, _, _ = select.select([fd], [], [], 0.2)
            if ready:
                ch = os.read(fd, 1).decode(errors='ignore')
                if ch:
                    events.put(('pc_key', ch))
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old)


def engine_sync(st: State, keep_output: bool) -> None:
    """Premier etat du moteur recu : lui envoyer les reglages enregistres."""
    a = st.audio
    st.keep_output = keep_output
    if keep_output:                                    # --output impose : on garde sa sortie
        a['output'] = st.audio_state.get('device', '')
    if a['output'] in st.audio_state.get('outputs', []) or a['rate'] or a['buffer'] != 96:
        send_audio(st)
        st.audio_action = time.monotonic()
    send_midi(st)
    st.sent_samples = {}                               # moteur (re)demarre : tout renvoyer
    st.sent_loops = {}
    st.sent_reverse = {}
    sync_samples(st)
    st.engine_synced = True
    threading.Thread(target=scan_card_types, args=(st,), daemon=True).start()


def play_note(st: State, group: str, snd: int, vel: int, note: int, gate: float = 0.0) -> None:
    """Joue une note : moteur interne, ou MIDI OUT si le groupe est envoye dehors (gate en s ;
    0 = note-off envoye au relachement)."""
    if not st.sound_audible(group, snd) or not st.audible(group):   # MUTE / SOLO (son ou groupe)
        return
    ch = st.midi['out_ch'].get(group, 0) if st.midi['out'] else 0
    if ch:
        SOUND.send('/fuzzy/proto/midi/note', ch, note, vel, int(gate * 1000))
        st.midi_out_flash = time.monotonic()
    elif st.instrument(group, snd):                  # sans instrument : rien a jouer
        env = st.env[(group, snd)]
        SOUND.hit(snd, vel, note, GROUPS.index(group), ENV_MODES.index(env['mode']),
                  int(env['release']), int(gate * 1000), st.synth.get((group, snd), 0),
                  max(0, min(127, st.params[(group, snd)][1] + st.group_pan[group] - 64)))   # Pan son + groupe


def release_notes(st: State, group: str, notes: list) -> None:
    """Pad relache : fin des notes (MIDI OUT, et sons internes). La fin de note est envoyee
    meme en ONE-SHOT : sans effet sur un son ONE-SHOT, elle evite qu'une note reste tenue si
    le mode a change entre la frappe et le relachement."""
    ch = st.midi['out_ch'].get(group, 0) if st.midi['out'] else 0
    for snd, n in notes:
        if ch:
            SOUND.send('/fuzzy/proto/midi/off', ch, n)
        else:
            SOUND.send('/fuzzy/proto/off', snd, n, GROUPS.index(group))


def osc_parse(data: bytes):
    """Message OSC minimal : adresse + arguments i / f / s."""
    def string(i):
        j = data.index(b'\0', i)
        return data[i:j].decode('utf-8', 'replace'), (j + 4) & ~3
    addr, i = string(0)
    tags, i = string(i)
    args = []
    for t in tags[1:]:
        if t == 'i':
            args.append(struct.unpack('>i', data[i:i + 4])[0]); i += 4
        elif t == 'f':
            args.append(struct.unpack('>f', data[i:i + 4])[0]); i += 4
        elif t == 's':
            v, i = string(i); args.append(v)
        else:
            break
    return addr, args


def engine_listener(st: State, events: queue.Queue, running: threading.Event) -> None:
    """Ecoute le moteur (port 57391) : etat audio / MIDI, notes MIDI IN, horloge recue."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.bind(('127.0.0.1', ENGINE_REPLY_PORT))
    except OSError:
        print(f'Port {ENGINE_REPLY_PORT} occupe : pas de retour du moteur (AUDIO / MIDI).')
        return
    sock.settimeout(0.2)
    while running.is_set():
        try:
            data = sock.recv(65536)
            addr, args = osc_parse(data)
        except (socket.timeout, ValueError, struct.error):
            continue
        except OSError:
            break
        if addr == '/fuzzy/proto/audio/state' and len(args) >= 9:
            first = not st.audio_state
            st.audio_state = {
                'device': args[0], 'rate': args[1], 'buffer': args[2], 'latency': args[3],
                'xruns': args[4], 'outputs': [o for o in args[5].split('\n') if o],
                'rates': [int(r) for r in args[6].split(',') if r],
                'buffers': [int(b) for b in args[7].split(',') if b],
                'inputs': [x for x in args[8].split('\n') if x],
                'cpu': args[9] if len(args) > 9 else None}
            events.put(('engine', 'first' if first else 'state'))
        elif addr == '/fuzzy/proto/midi/state' and len(args) >= 7:
            old = st.midi_state
            now = time.monotonic()
            if old and args[4] != old.get('in_count'):
                st.midi_in_flash = now
            if old and args[5] != old.get('out_count'):
                st.midi_out_flash = now
            st.midi_state = {'ins': [x for x in args[0].split('\n') if x],
                             'outs': [x for x in args[1].split('\n') if x],
                             'cur_in': args[2], 'cur_out': args[3], 'in_count': args[4],
                             'out_count': args[5], 'bpm': args[6]}
            if st.settings and st.set_tab == 'MIDI':
                events.put(('redraw',))
        elif addr == '/fuzzy/proto/sample/error' and args:
            st.toast = (f'Unreadable sample: {Path(args[0]).name}', time.monotonic() + 3)
            events.put(('redraw',))
        elif addr == '/fuzzy/proto/midi/in' and len(args) >= 3:
            st.midi_in_flash = time.monotonic()
            ch, note, vel = args
            m = st.midi
            if m['in_mode'] == 'OMNI':
                group = st.group
            elif 1 <= ch <= 16:
                group = GROUPS[ch - 1]
            else:
                continue
            (st.midi_held.add if vel > 0 else st.midi_held.discard)((group, note))
            if vel > 0:                                # jouer tout de suite (comme un pad)
                snd = st.melodic_sound()
                v = out_vel(st, group, snd, vel)
                if v and st.audible(group):
                    play_note(st, group, snd, v, note)
            else:
                release_notes(st, group, [(st.melodic_sound(), note)])
            events.put(('midi_note', group, note, vel))
        elif addr == '/fuzzy/proto/clock/in' and args:
            if st.midi['clock'] == 2:
                events.put(('clock_bpm', args[0]))
        elif addr == '/fuzzy/proto/transport/in' and args:
            if st.midi['clock'] == 2 and st.midi['transport']:
                events.put(('transport', bool(args[0])))
    sock.close()



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
    if st.file_mode:
        return draw_file(st, 0)
    if st.plugin_mode:
        return draw_plugin(st, 0)
    if st.arranger_mode:
        return draw_arranger(st, 0)
    if st.pattern_mode:
        return draw_patterns(st, 0)
    if st.scene_mode:
        return draw_scenes(st, 0)
    if st.events_mode:
        return draw_events(st, 0)
    if st.tracker16:
        return draw_tracks(st, 0)
    if kb_tracker(st):                               # Keyboard / Chords : tracker a gauche
        return draw_tracks(st, st.trk_page * 8, left=True)
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
        chosen = pad == st.sound and 'select' in st.held   # SELECT tenu : le son choisi en blanc
        d.rectangle((x, y, x + 110, y + 28), fill='white' if chosen else shade(acc, 0.36) if lit else None,
                    outline=acc if pad == st.sound else (70, 70, 70), width=2)
        silent = not st.sound_audible(st.group, pad) or not st.instrument(st.group, pad)
        tag = ' M' if (st.group, pad) in st.sound_muted else ' S' if (st.group, pad) in st.sound_solo else ''
        label = f'{pad:02d} {st.sound_name(pad)[:10 - len(tag)]}{tag}'
        d.text((x + 55 - d.textlength(label, font=f) / 2, y + 7), label, font=f,   # centre dans la case
               fill='black' if chosen else (110, 110, 110) if silent else 'white')
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
    toast_box(st, d, acc)                            # courts messages (octave, Shift + pad...)
    tempo_overlay(st, d)
    return img


def length_overlay(st: State, d: ImageDraw.ImageDraw) -> None:
    """Encodeur 8 tourne : grand affichage de la longueur du pattern."""
    if time.monotonic() > st.len_flash:
        return
    acc = group_rgb(st)
    d.rectangle((90, 70, 390, 200), fill=(15, 15, 20), outline=acc, width=3)
    d.text((110, 80), f'LENGTH  Pattern {st.pat_index[st.group] + 1:02d}', font=font(16), fill=acc)
    txt = f'{st.length} lines'
    d.text((240 - d.textlength(txt, font=font(36)) / 2, 112), txt, font=font(36), fill='white')
    d.text((110, 172), f'= {st.length / 16:g} bar' + ('s' if st.length > 16 else '') + '  (encoder 8)',
           font=font(13), fill=(170, 170, 170))


def length_label(st: State, d: ImageDraw.ImageDraw) -> None:
    """En bas a droite de l'ecran droit, sous l'encodeur 8 : longueur du pattern."""
    txt = f'Length {st.length}'
    d.rectangle((362, 254, W, H), fill=(12, 12, 16))
    d.text((W - 10 - d.textlength(txt, font=font(13)), 255), txt, font=font(13), fill=group_rgb(st))


def draw_patterns(st: State, half: int) -> Image.Image:
    """Mode PATTERN : a gauche les 16 patterns de la banque, a droite l'apercu du pattern."""
    acc = group_rgb(st)
    img = Image.new('RGB', (W, H), (10, 10, 14))
    d = ImageDraw.Draw(img)
    g = st.group
    cur, queued = st.pat_index[g], st.pat_queue.get(g)
    if half == 0:
        header(d, f'PATTERNS  Group {g} {st.group_names[g]}', f'Bank {st.pat_bank + 1}/{PAT_BANKS}', acc)
        for i in range(16):
            pad = 13 + i % 4 - 4 * (i // 4)            # pad 13 en haut a gauche
            idx = st.pat_bank * 16 + pad - 1
            pat = st.patterns[g][idx]
            x, y = 8 + (i % 4) * 117, 32 + (i // 4) * 52
            off = g in st.pat_off
            if idx == cur and not off:
                rounded(d, (x, y, x + 110, y + 46), fill=acc)
            elif pat:
                rounded(d, (x, y, x + 110, y + 46), fill=shade(acc, 0.3),
                        outline=acc if idx == cur else None)
            else:
                rounded(d, (x, y, x + 110, y + 46), outline=(50, 50, 60))
            if idx == queued:
                rounded(d, (x - 2, y - 2, x + 112, y + 48), outline='white', width=2)
            ink = 'black' if idx == cur and not off else ('white' if pat else (90, 90, 90))

            def centered(txt, ty, f):                  # texte centre dans la case
                d.text((x + 55 - d.textlength(txt, font=f) / 2, ty), txt, font=f, fill=ink)
            centered(f'P{idx + 1:02d}', y + 5, font(15))
            if pat and pat.get('name'):
                centered(pat['name'], y + 26, font(12))       # nom court
            elif pat:
                notes = sum(1 for r in range(pat['length']) for c in pat['grid'][r] if c)
                centered(f"{pat['length']} lin  {notes} n", y + 26, font(11))
            else:
                centered('empty', y + 26, font(11))
        labels = [f'Bank {st.pat_bank + 1}', '', '', '']
        for k, t in enumerate(labels):
            d.text((k * 120 + 60 - d.textlength(t, font=font(12)) / 2, 252), t, font=font(12), fill=(170, 170, 170))
        return img
    pat = st.patterns[g][cur]
    state = ('next: none' if queued == OFF else f'next: P{queued + 1:02d}' if queued is not None
             else 'NO PATTERN' if g in st.pat_off else 'playing' if st.playing else 'stopped')
    header(d, f"Pattern {cur + 1:02d} {pat.get('name', '')}  {g}", state, acc)
    softkey(d, st, 2, 'GROUP', st.name_edit is not None and st.name_edit[0] == 'group')
    softkey(d, st, 3, 'NAME', st.name_edit is not None and st.name_edit[0] == 'pattern')
    length = pat['length']
    cw = max(3, min(28, 456 // max(1, length)))      # largeur d'une ligne dans l'apercu
    playing = play_row(st)
    for t in range(16):
        y = 34 + t * 8
        for r in range(length):
            x = 14 + r * cw
            cell = pat['grid'][r][t]
            if r == playing:
                d.rectangle((x, y, x + cw - 2, y + 6), fill=(0, 90, 60) if not cell else 'white')
            elif cell:
                d.rectangle((x, y, x + cw - 2, y + 6), fill=acc if t == st.sound - 1 else shade(acc, 0.7))
            elif r % 4 == 0:
                d.rectangle((x, y + 2, x + 1, y + 4), fill=(60, 60, 70))
    lines = ['Pad: choose (while playing: at the end of', 'the pattern).  Chosen pad again: no pattern.',
             'DUPLICATE + pad: copy here.  ERASE + pad: erase.', '< > : bank.  NAME / GROUP: names.']
    if g in st.pat_off or queued == OFF:             # le bandeau est cache par NAME
        d.text((12, 146), f'Group {g}: no pattern (silent)' if g in st.pat_off
               else f'Group {g}: no pattern at the end', font=font(13), fill=(240, 160, 30))
    for k, line in enumerate(lines):
        d.text((12, 166 + k * 18), line, font=font(13), fill=(180, 180, 180))
    d.text((12, 240), 'PATTERN again: back.', font=font(13), fill=acc)
    length_label(st, d)
    toast_box(st, d, acc)
    name_overlay(st, d)
    return img


def draw_scenes(st: State, half: int) -> Image.Image:
    """Mode SCENE : a gauche les 16 scenes de la banque, a droite le detail de la scene."""
    acc = group_rgb(st)
    img = Image.new('RGB', (W, H), (10, 10, 14))
    d = ImageDraw.Draw(img)
    cur, waiting = st.scene_current, bool(st.pat_queue)
    if half == 0:
        header(d, 'SCENES', f'Bank {st.scene_bank + 1}/{PAT_BANKS}', acc)
        for i in range(16):
            pad = 13 + i % 4 - 4 * (i // 4)            # pad 13 en haut a gauche
            idx = st.scene_bank * 16 + pad - 1
            sc = st.scenes[idx]
            x, y = 8 + (i % 4) * 117, 32 + (i // 4) * 52
            if idx == cur:
                rounded(d, (x, y, x + 110, y + 46), fill=acc)
                if waiting:
                    rounded(d, (x - 2, y - 2, x + 112, y + 48), outline='white', width=2)
            elif sc:
                rounded(d, (x, y, x + 110, y + 46), fill=shade(acc, 0.3))
            else:
                rounded(d, (x, y, x + 110, y + 46), outline=(50, 50, 60))
            ink = 'black' if idx == cur else ('white' if sc else (90, 90, 90))

            def centered(txt, ty, f):
                d.text((x + 55 - d.textlength(txt, font=f) / 2, ty), txt, font=f, fill=ink)
            centered(f'S{idx + 1:02d}', y + 5, font(15))
            if sc and st.scene_names[idx]:
                centered(st.scene_names[idx], y + 26, font(12))   # nom court
            elif sc:                                   # groupes dont le pattern n'est pas le 1er
                used = [f"{g}{':' if len(g) > 1 else ''}{p + 1}" for g, p in sc.items()
                        if p is not None and p != OFF and (p or (
                    st.patterns[g][p] and any(
                        c for r in st.patterns[g][p]['grid'][:st.patterns[g][p]['length']] for c in r)))]
                used += [f'{g}~' for g, p in sc.items() if p is None]      # ~ = libre
                used += [f'{g}-' for g, p in sc.items() if p == OFF]       # - = aucun pattern
                centered(' '.join(used[:4]) + (' ..' if len(used) > 4 else '') or '-', y + 26, font(11))
            else:
                centered('empty', y + 26, font(11))
        d.text((10, 252), f'Bank {st.scene_bank + 1}', font=font(12), fill=(170, 170, 170))
        return img
    state = 'switching...' if waiting else ('playing' if st.playing else 'stopped')
    header(d, f'Scene {cur + 1:02d} {st.scene_names[cur]}' if cur is not None else 'No scene', state, acc)
    softkey(d, st, 3, 'NAME', st.name_edit is not None)
    combo = st.scenes[cur] if cur is not None and st.scenes[cur] else current_combo(st)
    for k, g in enumerate(GROUPS):                     # 16 groupes : 4 lignes de 4
        x, y = 12 + (k % 4) * 116, 32 + (k // 4) * 27
        p = combo.get(g, 0)
        color = hue_rgb(st.group_hue[g])
        if p is None:                                  # groupe libre : garde son pattern
            rounded(d, (x, y, x + 108, y + 24), outline=color, r=6)
            txt, ink = f'{g:3s}FREE', color
        elif p == OFF:                                 # aucun pattern dans cette scene
            rounded(d, (x, y, x + 108, y + 24), fill=(24, 24, 32), outline=(70, 70, 80), r=6)
            txt, ink = f'{g:3s}--', 'white'
        else:
            live = st.pat_index[g] == p and g not in st.pat_queue and g not in st.pat_off
            rounded(d, (x, y, x + 108, y + 24), fill=color if live else (24, 24, 32),
                    outline=color, width=2, r=6)
            txt, ink = f'{g:3s}P{p + 1:02d}', 'black' if live else 'white'
        d.text((x + 54 - d.textlength(txt, font=font(14)) / 2, y + 4), txt, font=font(14), fill=ink)
    d.text((12, 146), 'Choose the patterns in the PATTERN view,', font=font(13), fill=acc)
    d.text((12, 164), 'then save them here on an empty pad.', font=font(13), fill=acc)
    lines = ['Pad: launch.  SELECT + pad: replace.  DUPLICATE + pad: copy.',
             'ERASE + pad: erase.  < > : bank.  SCENE again: back.',
             'SELECT + group: FREE (keeps its pattern) / fixed.',
             'NAME: scene name.']
    for k, line in enumerate(lines):
        d.text((12, 188 + k * 20), line, font=font(12), fill=(180, 180, 180))
    toast_box(st, d, acc)
    name_overlay(st, d)
    return img


def toast_box(st: State, d: ImageDraw.ImageDraw, acc) -> None:
    """Court message (Shift + pad, DOUBLE...)."""
    if time.monotonic() < st.toast[1]:
        d.rectangle((30, 110, 450, 150), fill=(15, 15, 20), outline=acc, width=2)
        d.text((240 - d.textlength(st.toast[0], font=font(15)) / 2, 120), st.toast[0],
               font=font(15), fill='white')


def tempo_overlay(st: State, d: ImageDraw.ImageDraw) -> None:
    """TEMPO (ou VOLUME, ou TAP) tenu : grand affichage du tempo (ou d'un volume)."""
    if 'tap' in st.held:
        acc = group_rgb(st)
        d.rectangle((90, 70, 390, 200), fill=(15, 15, 20), outline=acc, width=3)
        d.text((110, 80), 'METRONOME VOLUME', font=font(16), fill=acc)
        txt = f'{st.metro_vol}'
        d.text((240 - d.textlength(txt, font=font(40)) / 2, 108), txt, font=font(40), fill='white')
        d.text((110, 172), 'hold TAP + 4D.  TAP alone: on / off', font=font(12), fill=(170, 170, 170))
        return
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
    return (st.playhead - 1 - st.pat_start[st.group]) % st.length if st.playing else -1


def step_page(st: State) -> int:
    """Page STEP affichee : suit la lecture en Play, sinon la page du curseur."""
    if st.step_manual is not None:
        return min(st.step_manual, (st.length - 1) // 16)
    return (play_row(st) if st.playing and st.follow else st.cursor) // 16


def view_top(st: State, visible: int) -> int:
    """Premiere ligne affichee : suit la ligne qui joue pendant la lecture, sinon le curseur."""
    center = play_row(st) if st.playing and st.follow else st.cursor
    return max(0, min(center - visible // 2, st.length - visible))


def softkey(d: ImageDraw.ImageDraw, st: State, slot: int, text: str, active: bool,
            color=None) -> None:
    """Etiquette d'un bouton au-dessus de l'ecran (slot 0-3 de cet ecran), centree :
    sombre = disponible, blanche = fonction active."""
    x0 = 4 + slot * 120
    tx = x0 + 54 - d.textlength(text, font=font(16)) / 2
    if active:
        d.rectangle((x0, 3, x0 + 108, 23), fill='white')
        d.text((tx, 4), text, font=font(16), fill='black')
    else:
        d.rectangle((x0, 3, x0 + 108, 23), fill=color or shade(group_rgb(st), 0.35), outline=(20, 20, 20))
        d.text((tx, 4), text, font=font(16), fill=(235, 235, 235))


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
                   + (f'Pads {shift_label(st.pad_shift)}  ' if st.pad_shift else '') + f'Trk {st.sound:02d} {st.group}')
    f = font(14)
    t = st.sound - 1
    if 'select' in st.held:                          # SELECT tenu : le son choisi en blanc
        d.rectangle((0, 29, W, 48), fill='white')
        d.text((8, 31), f'Lin  Note Vol   {st.sound:02d} {st.sound_name(st.sound)}', font=f, fill='black')
    else:
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
        if cell and cell_off(cell):                  # timing fin : < en avance, > en retard
            d.text((8 + d.textlength(txt, font=f) + 6, y), '<' if cell_off(cell) < 0 else '>',
                   font=f, fill=(150, 150, 150))
        others = sum(1 for i, c in enumerate(st.grid[r]) if c and i != t)
        if others:
            d.text((200, y), f'+{others} other track' + ('s' if others > 1 else ''),
                   font=font(12), fill=(130, 130, 130))
    labels = ['Line', 'Note', 'Volume', 'Length']
    cell = st.grid[st.cursor][t]
    values = [f'{st.cursor:02X}', cell_label(cell[0]) if cell else '---',
              f'{cell[1]:02X}' if cell else '--', f'{st.length}']
    for i in range(4):
        d.text((i * 120 + 60 - d.textlength(f'{labels[i]} {values[i]}', font=font(12)) / 2, 250), f'{labels[i]} {values[i]}', font=font(12), fill=(180, 180, 180))
    length_overlay(st, d)
    return img


TRK_ROWS, TRK_X0, TRK_W = 15, 24, 57
SHORT = ['Kick', 'Snare', 'CHH', 'OHH']


def kb_tracker(st: State) -> bool:
    """Keyboard / Chords avec TRACKER : tracker sur l'ecran gauche seulement."""
    return st.trk_left and st.right_view in ('keyboard', 'chords') and not st.tracker16 \
        and not st.mixer and not st.settings and not st.pattern_mode and not st.scene_mode


def pattern_title(st: State) -> str:
    pat = st.patterns[st.group][st.pat_index[st.group]]
    name = pat.get('name', '') if pat else ''
    return f'P{st.pat_index[st.group] + 1:02d} {name}  {st.group}' if name else \
        f'Pattern {st.pat_index[st.group] + 1:02d}  {st.group}'


def draw_tracks(st: State, first: int, left: bool | None = None) -> Image.Image:
    """Vue Tracker 16 pistes : pistes first+1 a first+8 (gauche 1-8, droite 9-16).
    left=True : dessin pour l'ecran gauche quelle que soit la page (Keyboard / Chords)."""
    left = first == 0 if left is None else left
    acc = group_rgb(st)
    img = Image.new('RGB', (W, H), (12, 12, 16))
    d = ImageDraw.Draw(img)
    rec = step_label(st) + ('REC  ' if st.recording else '')
    if left:
        pages = '  < >' if first == 8 or st.trk_left and not st.tracker16 else ''
        right = f'{rec}{first + 1}-{first + 8} < >' if pages else f'{rec}L{st.length}  {first + 1}-{first + 8}'
        left_header(d, st, pattern_title(st), right)
    else:
        softkey_header(d, st, f'{rec}9-16')
    f = font(11)
    for t in range(8):
        x = TRK_X0 + t * TRK_W
        tr = first + t
        name = SHORT[tr] if tr < 4 else note_name(default_note(tr + 1))
        sel = tr == st.sound - 1
        inv = sel and 'select' in st.held                          # SELECT tenu : en blanc
        silent = not st.sound_audible(st.group, tr + 1)            # coupee / hors solo : grisee
        fill = 'white' if inv else acc if sel else (45, 45, 50) if silent else shade(acc, 0.35)
        d.rectangle((x, 29, x + TRK_W - 3, 41), fill=fill)
        tag = st.sound_tag(tr + 1)
        d.text((x + 3, 29), f'{tr + 1:02d}' + ('' if tag else f' {name}'), font=f,
               fill='black' if sel else (120, 120, 120) if silent else 'white')
        if tag:                                      # piste coupee / en solo : M ou S a la place du nom
            d.text((x + 24, 29), 'MUTE' if tag == 'M' else 'SOLO', font=f, fill='black' if sel else
                   (230, 70, 70) if tag == 'M' else (240, 200, 40))
    # En lecture : la ligne d'infos du bas laisse la place a une 16e ligne du pattern
    rows = TRK_ROWS + 1 if st.playing else TRK_ROWS
    top = view_top(st, rows)
    playing_row = play_row(st)
    for k in range(min(rows, st.length)):
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
            silent = not st.sound_audible(st.group, tr + 1)
            d.text((x + 3, y), f'{cell_label(cell[0])} {cell[1]:02X}' if cell else '--- --',
                   font=f, fill=((95, 95, 95) if silent else 'white') if cell else (80, 80, 80))
            if cell and cell_off(cell):              # timing fin : petit trait a gauche / a droite
                mx = x + 1 if cell_off(cell) < 0 else x + TRK_W - 5
                d.rectangle((mx, y + 3, mx + 1, y + 11), fill=(170, 170, 170))
    if not left and not st.playing:
        length_label(st, d)
    if left and not st.playing:
        cell = st.grid[st.cursor][st.sound - 1]
        info = (f'Line {st.cursor:02X}   Track {st.sound:02d}   '
                f'Note {cell_label(cell[0]) if cell else "---"}   Vol {cell[1]:02X}' if cell else
                f'Line {st.cursor:02X}   Track {st.sound:02d}   Note ---   Vol --')
        d.text((6, 256), info, font=font(11), fill=(170, 170, 170))
    if left:
        tempo_overlay(st, d)
        length_overlay(st, d)
        toast_box(st, d, acc)                        # message court (Shift + pad)
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
        softkey_header(d, st, step_label(st) + f'Oct {st.octave}  {KEY_NAMES[st.root]} {scale_name}'[:24])
        softkey(d, st, 1, 'PC KEYS', st.pc['keys'])     # bouton 6 : clavier de l'ordinateur
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
        d.text((i * 120 + 60 - d.textlength(t, font=font(12)) / 2, 254), t, font=font(12), fill=(180, 180, 180))
    scale_dropdown(st, d)
    return img




def draw_right(st: State) -> Image.Image:
    img = _draw_right(st)
    if st.countin is not None:                         # decompte : 4 3 2 1
        d = ImageDraw.Draw(img)
        acc = group_rgb(st)
        n = str(max(1, 5 - st.countin[1]))
        d.rectangle((140, 50, 340, 230), fill=(15, 15, 20), outline=acc, width=3)
        d.text((240 - d.textlength(n, font=font(110)) / 2, 62), n, font=font(110), fill='white')
        d.text((240 - d.textlength('REC', font=font(14)) / 2, 206), 'REC', font=font(14), fill=(230, 60, 60))
        return img
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
    if st.file_mode:
        return draw_file(st, 1)
    if st.plugin_mode:
        return draw_plugin(st, 1)
    if st.arranger_mode:
        return draw_arranger(st, 1)
    if st.pattern_mode:
        return draw_patterns(st, 1)
    if st.scene_mode:
        return draw_scenes(st, 1)
    if st.events_mode:
        return draw_events(st, 1)
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
        d.text((k * 120 + 60 - d.textlength(t, font=font(12)) / 2, 254), t, font=font(12), fill=(180, 180, 180))
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
    for i, g in enumerate(bank_groups(st)):
        x, y = 60 + (i % 4) * 92, 82 + (i // 4) * 46
        on = g in st.linked or (g == st.group and st.linked)
        rounded(d, (x, y, x + 80, y + 36), fill=hue_rgb(st.group_hue[g]) if on else (30, 30, 38),
                outline=hue_rgb(st.group_hue[g]))
        d.text((x + 40 - d.textlength(g, font=font(16)) / 2, y + 9), g, font=font(16),
               fill='black' if on else 'white')
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
    if ('shift' in st.held or st.pattern_mode or st.scene_mode or st.arranger_mode
            or ({'mute', 'solo'} & st.held)                      # MUTE / SOLO + pad : rien ne joue
            or (st.plugin_mode and 'select' in st.held)
            or st.settings and st.set_tab in ('COLORS', 'STATUS')):
        return []                          # AUDIO / MIDI : les pads jouent (tester la sortie)
    if 'select' in st.held and not (st.pattern_mode or st.scene_mode or st.file_mode
                                    or st.arranger_mode or st.plugin_mode):
        return []                          # SELECT + pad : on choisit le son, rien ne joue
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
    return [(pad, pad_mode_note(st, pad))]


def shift_label(n: int) -> str:
    """Transposition lisible : 12 -> 'Oct +1', 14 -> 'Oct +1 +2', -3 -> '-3'."""
    octs = int(n / 12)
    rest = n - 12 * octs
    return ' '.join(([f'Oct {octs:+d}'] if octs else []) + ([f'{rest:+d}'] if rest else [])) or '0'


def pad_mode_note(st: State, pad: int) -> int:
    """Note d'un pad en Pad mode, avec la transposition en direct (Shift + 13-16 en lecture)."""
    return max(0, min(127, default_note(pad) + st.pad_shift))


def snapshot(st: State, coalesce: bool = False) -> None:
    """Memorise le pattern avant une modification (pour Undo). coalesce : les reglages
    a l'encodeur tres rapproches ne comptent que pour une seule etape."""
    now = time.monotonic()
    if coalesce and now - st.last_undo_push < 0.8:
        st.last_undo_push = now
        return
    st.last_undo_push = now
    st.undo_stack.append((st.group, st.pat_index[st.group], [row[:] for row in st.grid], st.length))
    del st.undo_stack[:-100]                     # 100 etapes maximum
    st.redo_stack.clear()


def undo(st: State, redo: bool = False) -> None:
    src, dst = (st.redo_stack, st.undo_stack) if redo else (st.undo_stack, st.redo_stack)
    if not src:
        return
    group, idx, grid, length = src.pop()
    pattern = st.patterns[group][idx]
    if pattern is None:                              # pattern efface depuis : on le recree
        pattern = st.patterns[group][idx] = {'grid': grid, 'length': length, 'name': ''}
    dst.append((group, idx, [row[:] for row in pattern['grid']], pattern['length']))
    pattern['grid'], pattern['length'] = grid, length     # la longueur aussi (DOUBLE, Paste)
    if st.pat_index[group] == idx:
        st.grids[group] = grid


def quantize(grid, rows, tracks, amount: float) -> int:
    """Rapproche les notes de leur ligne : 1.0 = pile sur la ligne, 0.5 = a mi-chemin.
    Renvoie le nombre de notes deplacees."""
    moved = 0
    for r in rows:
        for t in tracks:
            cell = grid[r][t]
            if cell and cell_off(cell):
                off = 0 if amount >= 1 else int(cell_off(cell) * (1 - amount))
                grid[r][t] = with_off(cell, off)
                moved += 1
    return moved


def track_function(st: State, pad: int) -> None:
    """Vue TRACKER, a l'arret : Shift + pad = fonction ecrite sous le pad (MK3).
    1er appui : la note du curseur ; 2e appui rapide (moins d'1 s) : toute la piste."""
    t, rows = st.sound - 1, st.length
    name = f'Track {st.sound:02d}'
    now = time.monotonic()
    if pad in (5, 6):                                # Quantize 100 % / 50 % : vers la ligne
        last = st.last_fn
        whole = bool(last and last[0] == pad and now - last[1] < 1.0 and last[3] == t)
        rows_q = range(rows) if whole else [st.cursor]
        if not whole:
            snapshot(st)                             # Shift + pad 1 : annuler
        moved = quantize(st.grid, rows_q, [t], 1.0 if pad == 5 else 0.5)
        what = name if whole else f'Note line {st.cursor:02X}'
        st.toast = (f"{what} quantized {'100' if pad == 5 else '50'}%"
                    + ('' if moved else ' (already on the grid)'), now + 2)
        st.last_fn = None if whole else (pad, now, None, t, None)
        return
    if pad in (7, 8):                                # Nudge < / > : d'une ligne
        d = -1 if pad == 7 else 1
        last = st.last_fn
        if last and last[0] == pad and now - last[1] < 1.0 and last[3] == t:
            if last[4] is not None:                  # 2e appui : on defait le 1er deplacement
                back = st.cursor - d
                st.grid[back][t], st.grid[st.cursor][t] = st.grid[st.cursor][t], None
                st.cursor = back
            col = [st.grid[r][t] for r in range(rows)]   # puis toute la piste tourne
            for r in range(rows):
                st.grid[r][t] = col[(r - d) % rows]
            st.last_fn = None
            st.toast = (f"{name} nudged {'<' if d < 0 else '>'}", now + 2)
            return
        snapshot(st)                                 # Shift + pad 1 : annuler
        to = st.cursor + d
        moved = 0 <= to < rows and st.grid[st.cursor][t] and not st.grid[to][t]
        if moved:
            st.grid[to][t], st.grid[st.cursor][t] = st.grid[st.cursor][t], None
            st.cursor = to
        st.last_fn = (pad, now, st.cursor, t, True if moved else None)
        st.toast = ((f"Note nudged {'<' if d < 0 else '>'}" if moved else 'Nudge: no note here, or no room')
                    + '  - again: whole track', now + 2)
        return
    if pad == 12:                                    # Paste : colle ce qui a ete copie
        if not st.clipboard:
            st.toast = ('Nothing to paste (Shift + pad 11 = copy)', now + 2)
            return
        snapshot(st)
        if st.clipboard[0] == 'note':                # la note, a la ligne du curseur
            st.grid[st.cursor][t] = st.clipboard[1]
            st.toast = (f'Note pasted line {st.cursor:02X}', now + 2)
            st.cursor = min(rows - 1, st.cursor + 1) # pret pour coller a la ligne suivante
        else:                                        # toute la piste
            cells = st.clipboard[1]
            for r in range(min(rows, len(cells))):
                st.grid[r][t] = cells[r]
            st.toast = (f'Track pasted into {name}', now + 2)
        return
    last = st.last_fn                                # (pad, heure, ligne, piste, case d'avant)
    whole = last is not None and last[0] == pad and now - last[1] < 1.0 \
        and last[2] == st.cursor and last[3] == t
    if whole:                                        # 2e appui : on annule l'effet sur la note
        st.grid[st.cursor][t] = last[4]              # puis on l'applique a toute la piste
        st.last_fn = None
    else:
        st.last_fn = (pad, now, st.cursor, t, st.grid[st.cursor][t])
    rows_hit = range(rows) if whole else (st.cursor,)
    hint = '' if whole else '  - again: whole track'
    if pad == 11:                                    # Copy
        if whole:
            st.clipboard = ('track', [st.grid[r][t] for r in range(rows)])
            st.toast = (f'{name} copied ({rows} lines)', now + 2)
        else:
            cell = st.grid[st.cursor][t]
            st.clipboard = ('note', cell)
            st.toast = (f'Note copied ({cell_label(cell[0]).strip() if cell else "empty"}){hint}', now + 2)
        return
    if not whole:
        snapshot(st)                                 # Shift + pad 1 : annuler
    if pad == 9:                                     # Clear
        for r in rows_hit:
            st.grid[r][t] = None
        msg = f'{name} cleared' if whole else f'Note cleared{hint}'
    else:                                            # 13 / 14 demi-ton, 15 / 16 octave
        n = {13: -1, 14: 1, 15: -12, 16: 12}[pad]
        for r in rows_hit:
            cell = st.grid[r][t]
            if cell:
                notes = tuple(max(0, min(127, x + n)) for x in cell_notes(cell[0]))
                st.grid[r][t] = (notes if len(notes) > 1 else notes[0],) + tuple(cell[1:])
        amount = (f"Oct {'+' if n > 0 else '-'}1" if abs(n) == 12
                  else f"{'+' if n > 0 else ''}{n} semitone")
        msg = f'{name} {amount}' if whole else f'Note {amount}{hint}'
    st.toast = (msg, now + 2)


DRAG_HOLD = 0.4                                  # appui long sur la molette 4D (secondes)
DOUBLE_CLICK = 0.4                               # double appui rapide (secondes)


def edit4d_ok(st: State) -> bool:
    """Gestes d'edition a la molette 4D : TRACKER / EVENTS, a l'arret, hors menus."""
    return (not st.playing and not st.settings and not st.mixer and not st.file_mode
            and not st.arranger_mode and not st.pattern_mode and not st.scene_mode
            and not st.plugin_mode and (st.tracker16 or kb_tracker(st) or st.events_mode))


def drag_turn(st: State, delta: int) -> bool:
    """Molette tournee pendant l'appui. Retourne True si le geste glisser est en cours."""
    d = st.drag
    if not d:
        return False
    if not d['active']:
        if time.monotonic() - d['t0'] < DRAG_HOLD or not d['cell']:
            return False                             # appui court : la molette bouge le curseur
        d['active'] = True                           # appui long sur une note : on la prend
        d['copy'] = d['copy'] or 'shift' in st.held
        d['col'] = [st.grid[r][d['track']] for r in range(st.length)]
        snapshot(st)                                 # Shift + pad 1 : annuler
    t, col = d['track'], d['col']
    st.cursor = max(0, min(st.length - 1, st.cursor + delta))
    for r in range(st.length):                       # apercu : la colonne d'origine...
        st.grid[r][t] = col[r]
    if not d['copy']:
        st.grid[d['row']][t] = None                  # ...sans la note (deplacer)
    st.grid[st.cursor][t] = d['cell']                # ...et la note sous le curseur
    if st.events_mode:
        pr_follow_pitch(st)
    st.toast = (f"{'Copy' if d['copy'] else 'Move'} to line {st.cursor:02X}  - release to drop",
                time.monotonic() + 2)
    return True


def drag_release(st: State) -> None:
    """Molette relachee : deposer la note, ou compter un clic (double appui = couper / coller)."""
    d, st.drag = st.drag, None
    now = time.monotonic()
    if not d:
        return
    if d['active']:                                  # fin du glisser : la note reste ou elle est
        t = d['track']
        if st.cursor == d['row'] and not d['copy']:
            st.toast = ('Note not moved', now + 2)
        else:
            replaced = d['col'][st.cursor] is not None and st.cursor != d['row']
            st.toast = (f"Note {'copied' if d['copy'] else 'moved'} to line {st.cursor:02X}"
                        + ('  (replaced a note)' if replaced else ''), now + 2)
        st.last_click = None
        return
    if now - d['t0'] >= DRAG_HOLD:                   # appui long sans rotation : rien
        st.last_click = None
        return
    t, row = st.sound - 1, st.cursor
    last = st.last_click
    if not (last and now - last[0] < DOUBLE_CLICK and last[1] == row and last[2] == t):
        st.last_click = (now, row, t)                # 1er clic : on attend le 2e
        return
    st.last_click = None                             # double appui
    cell = st.grid[row][t]
    if cell:                                         # sur une note : couper
        snapshot(st)
        st.clipboard = ('note', cell)
        st.grid[row][t] = None
        st.toast = ('Note cut  - double press on an empty line: paste', now + 2)
    elif st.clipboard and st.clipboard[0] == 'note' and st.clipboard[1]:
        snapshot(st)                                 # sur une ligne vide : coller
        st.grid[row][t] = st.clipboard[1]
        st.toast = (f'Note pasted line {row:02X}', now + 2)
    else:
        st.toast = ('Nothing to paste (double press on a note = cut)', now + 2)


TIMING = 24                                      # decalage fin : 24 crans par ligne


def cell_len(cell) -> int:
    """Duree d'une note en lignes (case : (note, velocite[, duree[, decalage]]))."""
    return cell[2] if cell and len(cell) > 2 else 1


def cell_off(cell) -> int:
    """Decalage fin de la note par rapport a sa ligne, en 1/24 de ligne (-12 a +12)."""
    return cell[3] if cell and len(cell) > 3 else 0


def make_cell(note, vel: int, length: int = 1, off: int = 0):
    """Case la plus courte possible : (note, vel), + duree si > 1, + decalage s'il existe."""
    if off:
        return (note, vel, max(1, length), off)
    return (note, vel) if length <= 1 else (note, vel, length)


def with_len(cell, length: int):
    return make_cell(cell[0], cell[1], length, cell_off(cell))


def with_off(cell, off: int):
    return make_cell(cell[0], cell[1], cell_len(cell), max(-TIMING // 2, min(TIMING // 2, off)))


def record(st: State, track: int, note, vel: int, pad: int | None = None) -> None:
    """Ecrit une note sur une piste : a la ligne qui joue (lecture) ou a la ligne du curseur.
    En lecture, avec le pad : sa duree sera celle de l'appui (fixee au relachement)."""
    snapshot(st, coalesce=st.playing)
    if st.playing:
        row = play_row(st)
        dur = 60 / st.bpm / 4
        frac = (time.monotonic() - (st.next_row_time - dur)) / dur   # position dans la ligne (0-1)
        off = round(max(0.0, min(1.0, frac)) * TIMING)
        if off > TIMING // 2:                        # 2e moitie de la ligne : ligne suivante, en avance
            row, off = (row + 1) % st.length, off - TIMING
        st.grid[row][track - 1] = make_cell(note, vel, 1, off)
        if pad is not None:
            st.rec_open[pad] = (row, track - 1, st.playhead, st.grid)
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
            if st.set_confirm or st.set_tab != 'COLORS':
                return
            if st.set_target == 'GROUP':
                st.group_hue[st.group] = pad - 1     # pad n = teinte n
            elif 'shift' in st.held:
                st.pad_hue.pop((st.group, pad), None)   # retour a la couleur du groupe
            else:
                st.set_selected ^= {pad}
            return
        if ({'mute', 'solo'} & st.held) and not (st.pattern_mode or st.scene_mode or st.arranger_mode):
            key = (st.group, pad)                    # MUTE / SOLO + pad : le son du pad dans le groupe
            now = time.monotonic()
            shown = (st.plugin_mode or st.mixer or st.tracker16 or kb_tracker(st)
                     or (not st.melodic() and not st.step_mode))     # l'ecran montre deja l'etat
            if 'mute' in st.held:
                st.sound_muted ^= {key}
                msg = f"Sound {pad:02d} {st.sound_name(pad)} " + ('muted' if key in st.sound_muted else 'on')
            else:
                st.sound_solo ^= {key}
                solo = sorted(k[1] for k in st.sound_solo if k[0] == st.group)
                msg = ('Solo: ' + ', '.join(f'{k:02d}' for k in solo)) if solo else 'Solo off'
            if not shown:
                st.toast = (msg, now + 2)
            return
        if ('duplicate' in st.held and 'shift' not in st.held
                and not (st.pattern_mode or st.scene_mode or st.arranger_mode or st.file_mode)):
            duplicate_pad(st, pad)                   # DUPLICATE + pad (source) + pad (cible) : copier le son
            return
        if st.file_mode:                             # FILE : les pads jouent, sans rien modifier
            if st.right_view == 'chords' and not st.step_mode:
                notes = st.chord_notes(pad)
                if notes:
                    st.held_notes[pad] = [(n, vel) for n in notes]
            elif st.right_view == 'keyboard' and not st.step_mode:
                st.held_notes[pad] = [(st.pad_note(pad), vel)]
            return
        if st.arranger_mode:                         # ARRANGER : pad = ajouter la scene
            arranger_pad(st, pad)
            return
        if st.plugin_mode:                           # PLUG-IN : choisir le son / la selection
            if 'select' in st.held:
                if not st.plugin_sel:
                    st.plugin_sel.add(st.sound)
                st.plugin_sel ^= {pad}
            else:
                st.plugin_sel.clear()
            st.sound = pad
            return
        if st.pattern_mode:                          # PATTERN : pads = patterns
            pattern_pad(st, pad, vel)
            return
        if st.scene_mode:                            # SCENE : pads = scenes
            scene_pad(st, pad)
            return
        if st.right_view == 'chords' and st.user_edit:   # EDIT USER : pads = 16 demi-tons
            chord = set(st.user_chords[st.user_bank][st.user_slot - 1])
            chord ^= {pad - 1}
            st.user_chords[st.user_bank][st.user_slot - 1] = tuple(sorted(chord))
            return
        if 'select' in st.held and st.right_view in ('tracker', 'keyboard', 'chords') and not (
                st.pattern_mode or st.scene_mode or st.file_mode or st.arranger_mode or st.plugin_mode):
            st.sound = pad                           # SELECT + pad : choisir le son (sans jouer)
            st.trk_page = (pad - 1) // 8
            return
        if st.step_mode and 'select' in st.held:     # STEP : SELECT + pad = choisir le son du pad
            st.sound = pad
            return
        if 'shift' in st.held and pad in (1, 2, 3, 4):   # Shift + pad 1 / 3 : Undo ; 2 / 4 : Redo
            undo(st, redo=pad in (2, 4))
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
        if ('shift' in st.held and st.playing and pad >= 13 and not keyboard and not st.step_mode
                and not st.events_mode):             # Pad mode en lecture : transposer les pads
            st.pad_shift = max(-24, min(24, st.pad_shift + {13: -1, 14: 1, 15: -12, 16: 12}[pad]))
            st.toast = (f'Pads {shift_label(st.pad_shift)}' if st.pad_shift else 'Pads: original pitch',
                        time.monotonic() + 0.8)
            return
        if ('shift' in st.held and not st.playing and pad in (5, 6, 7, 8, 9, 11, 12, 13, 14, 15, 16)
                and (st.tracker16 or kb_tracker(st) or st.events_mode or not keyboard)):
            track_function(st, pad)                  # serigraphie MK3, sur la piste choisie
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
            if pad >= 15:
                st.toast = (f'Octave {st.octave}', time.monotonic() + 0.8)
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
                    record(st, st.sound, notes, vel, pad)
            return
        if keyboard:                                 # pads = notes de la gamme
            note = st.pad_note(pad)
            st.held_notes[pad] = [(note, vel)]
            if st.recording:
                record(st, st.sound, note, vel, pad)
            return
        st.sound = pad                               # pads = sons du groupe
        if st.recording:
            record(st, pad, pad_mode_note(st, pad), vel, pad)
    elif kind == 'pad_off':
        st.down.discard(ev[1])
        st.held_notes.pop(ev[1], None)
        opened = st.rec_open.pop(ev[1], None)        # REC en lecture : duree = temps d'appui
        if opened:
            row, t, start, grid = opened
            cell = grid[row][t]
            if cell:
                grid[row][t] = with_len(cell, max(1, min(st.length, st.playhead - start)))
        if not st.down and st.advance_on_release:
            st.cursor = min(st.length - 1, st.cursor + 1)
            st.advance_on_release = False
    elif kind == 'pc_key':                           # clavier de l'ordinateur
        pc_key(st, ev[1])
    elif kind == 'jog':                              # grosse molette 4D
        if 'nav_click' in st.held and edit4d_ok(st) and drag_turn(st, ev[1]):
            pass                                     # appui long + rotation : la note suit
        elif st.file_mode and not st.settings:         # FILE : choisir le projet
            st.file_sel = max(0, min(len(list_projects()) - 1, st.file_sel + ev[1]))
            st.file_moved = True
        elif st.plugin_mode and st.browse and not st.settings:   # PLUG-IN > KIT / SYNTH : choisir
            browse_move(st, ev[1])
        elif st.arranger_mode and not st.settings:   # ARRANGER : choisir l'entree
            st.arr_sel = max(0, min(len(st.arrangement) - 1, st.arr_sel + ev[1]))
        elif st.mixer or 'volume' in st.held:          # MIXER, ou VOLUME tenu : volume master
            st.master = max(0, min(127, st.master + 2 * ev[1]))
        elif 'tap' in st.held:                       # TAP tenu + molette : volume du metronome
            st.metro_vol = max(0, min(127, st.metro_vol + 3 * ev[1]))
            st.tap_turned = True
        elif 'tempo' in st.held:                     # TEMPO tenu + molette : tempo au BPM pres
            st.bpm = max(40, min(240, st.bpm + ev[1]))
            st.taps.clear()
        elif st.events_mode and 'fixed_vel' in st.held:   # EVENTS : FIXED VEL + molette = velocite
            edit_cell(st, False, ev[1] * 2)
        elif not st.melodic() or st.tracker16 or kb_tracker(st) or st.events_mode:
            st.cursor = max(0, min(st.length - 1, st.cursor + ev[1]))
            if st.events_mode:
                pr_follow_pitch(st)
    elif kind == 'midi_note':                        # note recue en MIDI IN
        group, note, vel = ev[1], ev[2], ev[3]
        if vel and group == st.group:
            st.note_flash[note] = (time.monotonic() + 0.15, vel)
            if st.recording:
                record(st, st.melodic_sound(), note, vel)
    elif kind == 'clock_bpm':                        # horloge MIDI recue : tempo
        st.bpm = max(40, min(240, round(ev[1])))
    elif kind == 'transport':                        # Start / Stop recus en MIDI
        if ev[1] and not st.playing:
            reset_playback(st)
            st.playing = True
            st.next_row_time = time.monotonic()
        elif not ev[1]:
            st.playing = False
    elif kind == 'engine':                           # etat du moteur recu
        if st.settings and st.set_tab == 'STATUS' and ev[1] == 'first':
            refresh_status(st)
        cards = tuple(sound_cards(st))
        if cards != st.known_cards:                  # carte branchee / debranchee
            if st.known_cards:
                threading.Thread(target=scan_card_types, args=(st,), daemon=True).start()
            st.known_cards = cards
        if st.engine_synced and not st.keep_output:
            ensure_output(st)
    elif kind == 'unknown_button':
        print(f'[diag] bouton inconnu : bit {ev[1]} ({"appuye" if ev[2] else "relache"})')
    elif kind == 'button':
        name, pressed = ev[1], ev[2]
        (st.held.add if pressed else st.held.discard)(name)
        if name == 'tap' and not pressed and not st.tap_turned:   # TAP relache : metronome
            st.metro = not st.metro
            st.toast = ('Metronome ON' if st.metro else 'Metronome OFF', time.monotonic() + 1.5)
        if name == 'nav_click' and not pressed:      # molette 4D relachee
            drag_release(st)
        if name == 'duplicate':                      # DUPLICATE appuye / relache : nouvelle copie
            st.dup_src = None
        if not pressed:
            return
        if st.settings and not st.set_confirm and settings_button(st, name):
            return
        if name == 'midi_channel':                   # MIDI : ouvre SETTINGS > MIDI
            if st.settings and st.set_tab == 'MIDI' and not st.set_confirm:
                st.set_confirm = True                # 2e appui : fermer (sauvegarder ? YES / NO)
                return
            if not st.settings:
                open_settings(st)
            st.set_tab = 'MIDI'
            return
        if st.mixer and not st.settings and name in ('screen_3', 'screen_4'):
            if name == 'screen_4':                   # 4 : PAN (stereo) / volumes
                st.mix_pan = not st.mix_pan
            else:                                    # 3 : retour au TRACKER
                st.mixer = st.mix_pan = st.events_mode = False
                if st.right_view in ('keyboard', 'chords'):
                    st.trk_left = True
                else:
                    st.tracker16 = True
            return
        # SETTINGS ouvert par-dessus un mode : ses boutons (onglets, YES / NO) passent en premier
        if st.arranger_mode and not st.settings and arranger_button(st, name):   # SONG / GO / LOOP...
            return
        if name == 'events' and not st.settings:    # EVENTS : piano roll
            st.events_mode = not st.events_mode or st.mixer
            st.mixer = st.nr_menu = False            # le menu Note Repeat se range, l'arpege continue                         # le mixeur laisse la place
            st.pattern_mode = st.scene_mode = st.file_mode = st.arranger_mode = st.plugin_mode = False
            if st.events_mode:
                pr_follow_pitch(st)
            return
        if st.events_mode and name == 'erase' and not st.settings:   # EVENTS : ERASE = effacer
            if st.grid[st.cursor][st.sound - 1]:
                snapshot(st)
                st.grid[st.cursor][st.sound - 1] = None
            return
        if name == 'plug_in' and not st.settings:   # PLUG-IN : enveloppe des sons du groupe
            st.plugin_mode = not st.plugin_mode or st.mixer
            st.mixer = st.nr_menu = False
            st.pattern_mode = st.scene_mode = st.file_mode = st.arranger_mode = False
            st.plugin_sel.clear()
            st.browse = None
            return
        if st.plugin_mode and not st.settings and name in ('screen_6', 'screen_7', 'screen_8'):
            plugin_button(st, name)                  # 6 KIT / LOAD, 7 CANCEL, 8 SYNTH
            return
        if name == 'arranger':                       # ARRANGER : l'ordre des scenes (SONG)
            st.arranger_mode = not st.arranger_mode or st.mixer
            st.mixer = st.nr_menu = False
            st.pattern_mode = st.scene_mode = st.file_mode = st.plugin_mode = False
            st.name_edit = None
            return
        if st.file_mode and not st.settings and name in ('screen_5', 'screen_6', 'screen_7', 'screen_8'):
            file_button(st, name)                    # FILE : SAVE / LOAD / NEW / NAME
            return
        if name == 'file':
            if 'shift' in st.held:                   # Shift + FILE : enregistrer tout de suite
                if would_overwrite(st):
                    st.toast = (f'{st.project_name} exists: use FILE > SAVE', time.monotonic() + 2)
                else:
                    save_project(st)
                    st.toast = (f'Saved: {st.project_name}', time.monotonic() + 2)
            else:
                st.file_mode = not st.file_mode or st.mixer
                st.mixer = st.nr_menu = False
                st.pattern_mode = st.scene_mode = st.arranger_mode = st.plugin_mode = False
                st.name_edit = None
                names = list_projects()
                cur = project_folder(st.project_name).stem
                st.file_sel = names.index(cur) if cur in names else 0
                st.file_moved = False
            return
        if st.pattern_mode and not st.settings and name == 'screen_7':   # 7 : nom du GROUPE
            if st.name_edit and st.name_edit[0] == 'group':
                st.name_edit = None
            else:
                st.name_edit, st.name_pos = ('group', st.group), 0
            return
        if (st.pattern_mode or st.scene_mode) and not st.settings and name == 'screen_8':   # 8 : NAME
            if st.name_edit:
                st.name_edit = None
            elif st.pattern_mode:
                st.name_edit, st.name_pos = ('pattern', st.group, st.pat_index[st.group]), 0
            elif st.scene_current is not None and st.scenes[st.scene_current]:
                st.name_edit, st.name_pos = ('scene', st.scene_current), 0
            return
        if name in ('pad_mode', 'keyboard', 'chords'):  # quitter PATTERN / SCENE / FILE vers ce mode
            st.pattern_mode = st.scene_mode = st.file_mode = st.arranger_mode = st.plugin_mode = False
            st.name_edit = None
        if name.startswith('group_'):
            letter = name[-1].upper()
            g = letter + ('2' if st.group.endswith('2') else '')    # meme banque que le groupe choisi
            if st.scene_mode and 'select' in st.held:   # SCENE : SELECT + groupe = libre / fixe
                scene_free_toggle(st, g)
            elif 'shift' in st.held and not ({'mute', 'solo'} & st.held) and not st.link_edit:
                st.group = letter if g.endswith('2') else letter + '2'   # Shift + groupe : autre banque
            elif 'duplicate' in st.held and not (st.pattern_mode or st.arranger_mode):
                duplicate_group(st, g)               # DUPLICATE + groupe + groupe : copier le groupe
            elif 'mute' in st.held:                    # MUTE + groupe : couper / reactiver
                st.muted ^= {g}
            elif 'solo' in st.held:                  # SOLO + groupe(s) : n'ecouter que ceux-la
                st.solo ^= {g}
            elif st.link_edit:                       # LINK : lier / delier ce groupe
                st.link_toggle(g)
            else:
                st.group = g
                st.plugin_sel.clear()
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
        elif name == 'settings':                     # SETTINGS : menus de configuration
            if not st.settings:
                open_settings(st)
            else:
                st.set_confirm = True                # fermer : sauvegarder ? YES / NO
        elif name in ('screen_5', 'screen_6') and st.set_confirm:
            if name == 'screen_5':                   # YES : sauvegarder
                save_settings(st)
            else:                                    # NO : revenir aux reglages d'avant
                hues, pads, audio, midi, pc = st.set_backup
                st.group_hue, st.pad_hue, st.pc = hues, pads, pc
                if audio != st.audio:
                    st.audio = audio
                    send_audio(st)
                if midi != st.midi:
                    st.midi = midi
                    send_midi(st)
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
        elif name == 'play' and 'rec' in st.held and not st.playing:   # REC tenu + PLAY : decompte
            st.recording = True
            st.countin = (time.monotonic(), 0)
        elif name == 'play' and st.countin is not None:   # PLAY pendant le decompte : annuler
            st.countin = None
        elif name == 'play':
            st.playing = not st.playing
            st.step_manual = None                    # en lecture, les pages suivent a nouveau
            st.next_row_time = time.monotonic()
        elif name == 'tap':                          # TAP : marche / arret au relachement
            st.tap_turned = False                    # (TAP tenu + molette : volume)
        elif name == 'restart':                      # RESTART : retour au debut, et lecture
            reset_playback(st)
            st.playing = True
            st.step_manual = None
            st.next_row_time = time.monotonic()
        elif name == 'stop':
            st.playing = False
            st.countin = None
            reset_playback(st)
        elif name == 'rec':
            st.recording = not st.recording
        elif name == 'keyboard':
            st.mixer = False
            st.sound = st.melodic_sound()            # batterie de synthese : un son melodique
            st.right_view, st.tracker16, st.step_mode = 'keyboard', False, False
            st.held_notes.clear()
        elif name == 'chords' and 'shift' in st.held:   # Shift + CHORDS : clavier des accords
            st.chord_keyboard = not st.chord_keyboard
        elif name == 'chords':
            st.mixer = False
            st.sound = st.melodic_sound()            # batterie de synthese : un son melodique
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
        elif name == 'nav_click' and edit4d_ok(st):  # TRACKER / EVENTS : debut d'un geste
            t = st.sound - 1
            st.drag = {'t0': time.monotonic(), 'row': st.cursor, 'track': t,
                       'cell': st.grid[st.cursor][t], 'copy': 'shift' in st.held, 'active': False}
        elif name == 'screen_6' and st.right_view == 'keyboard' and not st.mixer and not st.tracker16:
            st.pc['keys'] = not st.pc['keys']        # 6 : jouer avec le clavier de l'ordinateur
            if st.pc['keys']:
                print_pc_keys(st)
            st.toast = ('Computer keys ON (see the terminal)' if st.pc['keys'] else 'Computer keys OFF',
                        time.monotonic() + 2)
            save_settings(st)
        elif name == 'mixer':                        # MIXER : ouvrir / fermer le mixeur
            st.mixer = not st.mixer
            st.mixer_page = 0
            st.mix_pan = False
            if st.mixer:                             # les encodeurs vont au mixeur
                st.plugin_mode = st.arranger_mode = st.file_mode = False
                st.nr_menu = False
                st.browse = None
        elif name in ('nav_left', 'nav_right', 'nav_prev', 'nav_next') and st.mixer:   # page 1 / 2
            st.mixer_page = 1 if name in ('nav_right', 'nav_next') else 0
        elif name == 'screen_8' and st.right_view == 'chords' and st.chord_mode == 'USER':
            st.user_edit = not st.user_edit          # bouton 8 : EDIT ; 2e appui = sauvegarde
            if not st.user_edit:
                save_user_chords(st)
        elif name == 'duplicate' and 'shift' in st.held:   # Shift + DUPLICATE : DOUBLE
            double_pattern(st)
        elif name == 'pattern':                      # PATTERN : pads = patterns du groupe
            st.mixer = st.nr_menu = False            # le mixeur et le menu Note Repeat laissent la place
            st.pattern_mode, st.scene_mode, st.file_mode = not st.pattern_mode, False, False
            st.arranger_mode = st.plugin_mode = False
            st.name_edit = None
            st.pat_bank = st.pat_index[st.group] // 16
        elif name == 'scene':                        # SCENE : pads = scenes
            st.mixer = st.nr_menu = False            # le mixeur et le menu Note Repeat laissent la place
            st.scene_mode, st.pattern_mode, st.file_mode = not st.scene_mode, False, False
            st.arranger_mode = st.plugin_mode = False
            st.name_edit = None
            if st.scene_current is not None:
                st.scene_bank = st.scene_current // 16
        elif name in ('nav_prev', 'nav_next') and st.scene_mode:     # < > : banque de scenes
            st.scene_bank = max(0, min(PAT_BANKS - 1, st.scene_bank + (1 if name == 'nav_next' else -1)))
        elif name in ('nav_prev', 'nav_next') and st.pattern_mode:   # < > : banque de patterns
            st.pat_bank = max(0, min(PAT_BANKS - 1, st.pat_bank + (1 if name == 'nav_next' else -1)))
        elif name in ('nav_prev', 'nav_next') and kb_tracker(st) and not st.step_mode:
            st.trk_page = 1 if name == 'nav_next' else 0    # < > : pistes 1-8 / 9-16
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
        elif name in ('nav_up', 'nav_down') and st.melodic() and not st.tracker16 and not kb_tracker(st):
            st.octave = max(0, min(7, st.octave + (1 if name == 'nav_up' else -1)))  # octave
            st.toast = (f'Octave {st.octave}', time.monotonic() + 0.8)
        elif name in ('nav_up', 'nav_down'):          # 4D pousse haut / bas : ligne
            st.cursor = max(0, min(st.length - 1, st.cursor + (1 if name == 'nav_down' else -1)))
        elif name in ('nav_left', 'nav_right'):       # 4D pousse gauche / droite : piste
            st.sound = max(1, min(16, st.sound + (1 if name == 'nav_right' else -1)))
            st.trk_page = (st.sound - 1) // 8
        elif name == 'screen_3' and not st.mixer and st.events_mode:   # EVENTS -> TRACKER
            st.events_mode = False
            if st.right_view in ('keyboard', 'chords'):
                st.trk_left = True
            else:
                st.tracker16 = True
        elif name == 'screen_3' and not st.mixer and st.right_view in ('keyboard', 'chords'):
            st.trk_left = not st.trk_left            # Keyboard / Chords : tracker a gauche seulement
        elif name == 'screen_3' and not st.mixer:    # TRACKER (bouton 3), dans tous les modes
            st.tracker16 = not st.tracker16          # 2e appui : retour a la vue precedente
        elif name == 'tempo':                        # tap tempo (TEMPO tenu + molette : jog)
            tap_tempo(st)
    elif kind == 'knob':
        i = ev[1] - 1
        if st.settings and st.set_tab in ('AUDIO', 'MIDI', 'KEYS'):
            settings_knob(st, i, ev[2])
            return
        if st.name_edit and not st.settings:         # editeur de nom
            name_knob(st, i, ev[2])
            return
        if (st.events_mode and not st.settings and not st.nr_menu and not st.mixer
                and not (st.pattern_mode or st.scene_mode or st.file_mode or st.arranger_mode
                         or st.plugin_mode)):        # EVENTS : piano roll
            events_knob(st, i, ev[2])
            return
        if st.plugin_mode and not st.settings and not st.nr_menu:     # PLUG-IN : 1 mode, 2 relachement
            plugin_knob(st, i, ev[2])
            return
        if st.arranger_mode and not st.settings and not st.nr_menu:   # 1 entree, 2 repet., 3 scene
            arranger_knob(st, i, ev[2])
            return
        if st.file_mode and not st.settings:         # FILE : encodeur 1 = projet choisi
            if i == 0:
                n = st.scene_knobs[0].steps(ev[2])
                st.file_sel = max(0, min(len(list_projects()) - 1, st.file_sel + n))
                st.file_moved = st.file_moved or bool(n)
            return
        if st.scene_mode and not st.settings and not st.mixer and not st.nr_menu:     # 1 banque
            if i in st.scene_knobs:
                n = st.scene_knobs[i].steps(ev[2])
                if n:
                    st.scene_bank = (st.scene_bank + n) % PAT_BANKS
            return
        if st.pattern_mode and not st.settings and not st.mixer and not st.nr_menu:   # 1 banque, 8 longueur
            if i in st.pat_knobs:
                n = st.pat_knobs[i].steps(ev[2])
                if n and i == 7:
                    st.length = max(1, min(ROWS, st.length + n))
                    st.cursor = min(st.cursor, st.length - 1)
                elif n:
                    st.pat_bank = (st.pat_bank + n) % PAT_BANKS
            return
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
            n = (st.pan_knobs if st.mix_pan else st.mix_knobs)[i].steps(ev[2])
            if n:
                key = mixer_channels(st)[i][1]
                old = mix_get(st, key)
                new = old + n
                if st.mix_pan and old == 64:         # PAN au centre : un cran a franchir
                    st.pan_hold[i] += n
                    if abs(st.pan_hold[i]) < 3:
                        return
                    new = 64 + (1 if st.pan_hold[i] > 0 else -1)
                elif st.mix_pan and (old - 64) * (new - 64) <= 0:
                    new = 64                         # PAN : on s'arrete au centre en le traversant
                    st.pan_knobs[i].acc = 0
                st.pan_hold[i] = 0
                mix_set(st, key, new)
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
        if (st.tracker16 or (kb_tracker(st) and i < 4)) and i in st.trk_knobs:
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
            st.toast = (f'Octave {st.octave}', time.monotonic() + 0.8)
        elif i == 7:                                 # encodeur 8 : longueur du pattern
            st.length = max(1, min(ROWS, st.length + n))
            st.cursor = min(st.cursor, st.length - 1)
            st.len_flash = time.monotonic() + 1.2    # grand affichage LENGTH
        elif st.tracker16 or (kb_tracker(st) and i < 4):   # 1 ligne, 2 piste, 3 note, 4 volume
            if i == 0:
                st.cursor = max(0, min(st.length - 1, st.cursor + n))
            elif i == 1:
                st.sound = max(1, min(16, st.sound + n))
                st.trk_page = (st.sound - 1) // 8    # tracker a gauche : page de la piste
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
    try:
        held = [(h[0], h[1], h[2]) for h in list(st.held_play.values())]
    except RuntimeError:                             # pad frappe pendant la lecture : au prochain tour
        return
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
            play_note(st, st.group, snd, v, n, nr_interval(st) * 0.8)
        if st.recording and st.playing:              # REC : les notes generees s'enregistrent
            record(st, snd, n, vel)
        return
    for _, vel, lst in held:
        by_sound: dict[int, list] = {}
        for snd, n in lst:
            v = out_vel(st, st.group, snd, vel)
            if v:
                play_note(st, st.group, snd, v, n, nr_interval(st) * 0.8)
            by_sound.setdefault(snd, []).append(n)
        if st.recording and st.playing:              # un accord reste un accord dans la case
            for snd, ns in by_sound.items():
                record(st, snd, tuple(ns) if len(ns) > 1 else ns[0], vel)


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
    note, vol = cell[0], cell[1]
    if is_note and isinstance(note, tuple):           # accord : tout l'accord est transpose
        if all(0 <= x + n <= 119 for x in note):
            note = tuple(x + n for x in note)
    elif is_note:
        note = max(0, min(119, note + n))
    else:
        vol = max(1, min(127, vol + n))
    st.grid[st.cursor][st.sound - 1] = (note, vol) + tuple(cell[2:])     # la duree est gardee


OFF = -1                                             # "aucun pattern" (groupe silencieux)


def pattern_length(st: State, g: str) -> int:
    return st.patterns[g][st.pat_index[g]]['length']


def new_pattern(length: int = 16) -> dict:
    return {'grid': [[None] * 16 for _ in range(ROWS)], 'length': length, 'name': ''}


def apply_pattern(st: State, g: str, idx: int) -> None:
    """Le groupe g passe au pattern idx (il repart de sa premiere ligne)."""
    if st.patterns[g][idx] is None:
        st.patterns[g][idx] = new_pattern()
    st.pat_index[g] = idx
    st.pat_off.discard(g)
    st.grids[g] = st.patterns[g][idx]['grid']
    st.pat_start[g] = st.playhead
    if g == st.group:
        st.cursor = min(st.cursor, st.length - 1)
        st.step_manual = None


def select_pattern(st: State, g: str, idx: int) -> None:
    """Choisir un pattern : tout de suite a l'arret, a la fin du pattern en cours en lecture."""
    if idx == OFF:                                   # aucun pattern : le groupe se tait
        set_off(st, g)
        return
    if st.patterns[g][idx] is None:
        st.patterns[g][idx] = new_pattern(pattern_length(st, g))
    if st.playing and (idx != st.pat_index[g] or g in st.pat_off):
        st.pat_queue[g] = idx
    else:
        st.pat_queue.pop(g, None)
        apply_pattern(st, g, idx)


def set_off(st: State, g: str) -> None:
    """Le groupe n'a plus de pattern (silence) : tout de suite a l'arret, a la fin du pattern
    en lecture. Son pattern reste en memoire (on le reprend en le choisissant)."""
    if st.playing and g not in st.pat_off:
        st.pat_queue[g] = OFF
    else:
        st.pat_queue.pop(g, None)
        st.pat_off.add(g)


def reset_playback(st: State) -> None:
    """Retour au debut : tous les patterns repartent de leur premiere ligne."""
    st.playhead = 0
    st.pending = []                                   # notes decalees en attente : oubliees
    st.arr_pos = st.arr_rows = 0                      # SONG : depuis le debut
    for g, idx in list(st.pat_queue.items()):
        if idx == OFF:
            st.pat_off.add(g)
        else:
            apply_pattern(st, g, idx)
    st.pat_queue.clear()
    for g in GROUPS:
        st.pat_start[g] = 0


def pattern_pad(st: State, pad: int, vel: int) -> None:
    """Mode PATTERN : pad = pattern de la banque ; DUPLICATE + pad = copier le pattern choisi
    ici ; ERASE + pad = effacer."""
    g, idx = st.group, st.pat_bank * 16 + pad - 1
    if 'shift' in st.held:                           # Shift + pad : serigraphie, pattern choisi
        pattern_function(st, pad)
        return
    if 'erase' in st.held:
        if idx == st.pat_index[g]:                   # pattern en cours : il devient vide
            st.patterns[g][idx] = new_pattern(st.length)
            apply_pattern(st, g, idx)
        else:
            st.patterns[g][idx] = None
            if st.pat_queue.get(g) == idx:
                st.pat_queue.pop(g)
    elif (idx == st.pat_index[g] and g not in st.pat_off and 'duplicate' not in st.held
          and st.pat_queue.get(g) is None):
        set_off(st, g)                               # re-taper le pattern choisi : aucun pattern
    elif 'duplicate' in st.held:
        src = st.patterns[g][st.pat_index[g]]
        st.patterns[g][idx] = {'grid': [row[:] for row in src['grid']], 'length': src['length'],
                               'name': src.get('name', '')}
        select_pattern(st, g, idx)
    else:
        select_pattern(st, g, idx)


def copy_sound(st: State, src: tuple, dst: tuple) -> None:
    """Copie un son (sample, boucle, sens, synthe, enveloppe, volume, reglages) ; comme DAW Pulses."""
    if src == dst:
        return
    for table in (st.samples, st.loops):
        if src in table:
            table[dst] = json.loads(json.dumps(table[src]))
        else:
            table.pop(dst, None)
    st.reverse.discard(dst)
    if src in st.reverse:
        st.reverse.add(dst)
    eff = st.eff_synth(*src)                         # le synthe reel (celui du pad depend du pad)
    st.synth.pop(dst, None)
    if st.eff_synth(*dst) != eff:
        st.synth[dst] = eff
    st.env[dst] = dict(st.env[src])
    st.sound_vol[dst] = st.sound_vol[src]
    st.params[dst] = list(st.params[src])


def duplicate_pad(st: State, pad: int) -> None:
    """DUPLICATE tenu : 1er pad = la source, pad suivant = la cible (aussi dans un autre groupe :
    DUPLICATE + pad, groupe, pad)."""
    now = time.monotonic()
    src = st.dup_src
    if not src or src[0] != 'sound':
        st.dup_src = ('sound', st.group, pad)
        st.toast = (f'Copy {st.group} {pad:02d} {st.sound_name(pad)} to ? (pad)', now + 3)
        return
    if (src[1], src[2]) == (st.group, pad):
        return
    copy_sound(st, (src[1], src[2]), (st.group, pad))
    sync_samples(st)
    st.dup_src = None
    st.toast = (f'Sound {src[1]} {src[2]:02d} copied to {st.group} {pad:02d}', now + 2)


def duplicate_group(st: State, g: str) -> None:
    """DUPLICATE tenu : 1er groupe = la source, groupe suivant = la cible (16 sons, patterns,
    volume). Avec un son en attente (DUPLICATE + pad) : le groupe s'ouvre pour choisir le pad."""
    now = time.monotonic()
    src = st.dup_src
    if src and src[0] == 'sound':                    # son en attente : on va dans ce groupe
        st.group = g
        st.toast = (f'Copy {src[1]} {src[2]:02d} to group {g}: pad ?', now + 3)
        return
    if not src:
        st.dup_src = ('group', g)
        st.toast = (f'Copy group {g} to ? (group)', now + 3)
        return
    if src[1] == g:
        return
    snapshot(st)
    for s in range(1, 17):
        copy_sound(st, (src[1], s), (g, s))
    st.patterns[g] = [None if p is None else json.loads(json.dumps(p)) for p in st.patterns[src[1]]]
    st.pat_index[g] = st.pat_index[src[1]]
    st.group_vol[g] = st.group_vol[src[1]]
    st.kit_names[g] = st.kit_names[src[1]]
    sync_samples(st)
    st.dup_src = None
    st.toast = (f'Group {src[1]} copied to {g}', now + 2)


def pattern_function(st: State, pad: int) -> None:
    """Mode PATTERN, Shift + pad (serigraphie MK3) sur le pattern choisi du groupe : 1 / 3
    annuler, 2 / 4 refaire, 9 Clear, 11 Copy, 12 Paste, 13 / 14 demi-ton, 15 / 16 octave
    (pistes melodiques 5-16), 5 / 6 Quantize."""
    g, now = st.group, time.monotonic()
    idx = st.pat_index[g]
    pat = st.patterns[g][idx]
    label = f'P{idx + 1:02d} {g}'
    if pad in (1, 2, 3, 4):
        undo(st, redo=pad in (2, 4))
        return
    if pad in (5, 6):                                # Quantize 100 % / 50 % : tout le pattern
        snapshot(st)
        moved = quantize(pat['grid'], range(pat['length']), range(16), 1.0 if pad == 5 else 0.5)
        st.toast = (f"{label} quantized {'100' if pad == 5 else '50'}%"
                    + ('' if moved else ' (already on the grid)'), now + 2)
        return
    if pad == 11:
        st.clip_pattern = {'grid': [row[:] for row in pat['grid']], 'length': pat['length'],
                           'name': pat.get('name', '')}
        st.toast = (f'{label} copied', now + 2)
        return
    if pad == 12 and not st.clip_pattern:
        st.toast = ('Nothing to paste (Shift + pad 11 = copy)', now + 2)
        return
    if pad not in (9, 12, 13, 14, 15, 16):
        return
    snapshot(st)                                     # Shift + pad 1 : annuler
    if pad == 9:                                     # Clear : pattern vide (meme longueur)
        for r in range(ROWS):
            pat['grid'][r] = [None] * 16
        msg = f'{label} cleared'
    elif pad == 12:                                  # Paste : contenu, longueur et nom
        clip = st.clip_pattern
        for r in range(ROWS):
            pat['grid'][r] = clip['grid'][r][:]
        pat['length'], pat['name'] = clip['length'], clip['name']
        st.cursor = min(st.cursor, pat['length'] - 1)
        msg = f'Pasted into {label}'
    else:                                            # transposer les pistes melodiques
        n = {13: -1, 14: 1, 15: -12, 16: 12}[pad]
        for r in range(ROWS):
            for t in range(4, 16):
                cell = pat['grid'][r][t]
                if cell:
                    notes = tuple(max(0, min(127, x + n)) for x in cell_notes(cell[0]))
                    pat['grid'][r][t] = (notes if len(notes) > 1 else notes[0],) + tuple(cell[1:])
        amount = f"Oct {'+' if n > 0 else '-'}1" if abs(n) == 12 else f"{'+' if n > 0 else ''}{n} semitone"
        msg = f'{label} {amount} (tracks 5-16)'
    st.toast = (msg, now + 2)


def scene_function(st: State, pad: int) -> None:
    """Mode SCENE, Shift + pad : 9 Clear, 11 Copy, 12 Paste, sur la scene choisie."""
    now, cur = time.monotonic(), st.scene_current
    if cur is None:
        st.toast = ('Choose a scene first', now + 2)
        return
    label = f'S{cur + 1:02d}'
    if pad == 11 and st.scenes[cur]:
        st.clip_scene = (dict(st.scenes[cur]), st.scene_names[cur])
        st.toast = (f'{label} copied', now + 2)
    elif pad == 12:
        if not st.clip_scene:
            st.toast = ('Nothing to paste (Shift + pad 11 = copy)', now + 2)
            return
        st.scenes[cur], st.scene_names[cur] = dict(st.clip_scene[0]), st.clip_scene[1]
        st.toast = (f'Pasted into {label}', now + 2)
    elif pad == 9:
        st.scenes[cur], st.scene_names[cur] = None, ''
        st.scene_current = None
        st.toast = (f'{label} cleared', now + 2)


def double_pattern(st: State) -> None:
    """Shift + DUPLICATE (DOUBLE) : le pattern choisi double de longueur, contenu recopie."""
    now = time.monotonic()
    if st.length * 2 > ROWS:
        st.toast = (f'Double: {ROWS} lines maximum', now + 2)
        return
    snapshot(st)
    n = st.length
    for r in range(n):
        st.grid[n + r] = st.grid[r][:]
    st.length = n * 2
    st.toast = (f'Doubled: {n} -> {n * 2} lines', now + 2)
    st.len_flash = now + 1.2


# --- Projets (bouton FILE) : patterns, scenes, tempo, sons... dans un dossier .fuzzy -----
PROJECTS_DIR = Path(__file__).with_name('projects')
PROJECT_VERSION = 1


def project_folder(name: str) -> Path:
    safe = ''.join(c if c.isalnum() or c in '-_' else '_' for c in name.strip()) or 'UNTITLED'
    return PROJECTS_DIR / f'{safe}.fuzzy'


def list_projects() -> list:
    if not PROJECTS_DIR.is_dir():
        return []
    return sorted(p.stem for p in PROJECTS_DIR.glob('*.fuzzy') if (p / 'project.json').exists())


def project_data(st: State) -> dict:
    """Tout ce qui fait le morceau (pas les reglages de l'appareil : ceux-la restent dans
    fuzzy_settings.json)."""
    groups = {}
    for g in GROUPS:
        pats = []
        for idx, pat in enumerate(st.patterns[g]):
            if pat is None:
                continue
            cells = [[r, t, list(c[0]) if isinstance(c[0], tuple) else c[0], c[1]]
                     + ([c[2]] if len(c) > 2 else [])          # duree (lignes) si > 1
                     + ([c[3]] if len(c) > 3 else [])          # decalage fin (1/24 de ligne)
                     for r in range(ROWS) for t, c in enumerate(pat['grid'][r]) if c]
            pats.append({'index': idx, 'name': pat.get('name', ''), 'length': pat['length'],
                         'cells': cells})
        groups[g] = {'patterns': pats, 'current': st.pat_index[g], 'off': g in st.pat_off,
                     'name': st.group_names[g],
                     'volume': st.group_vol[g], 'pan': st.group_pan[g], 'key': st.keys[g],
                     'kit': st.kit_names[g],
                     'sounds': {str(s): {'volume': st.sound_vol[(g, s)], 'params': st.params[(g, s)],
                                         'envelope': st.env[(g, s)],
                                         **({'sample': st.samples[(g, s)]} if (g, s) in st.samples else {}),
                                         **({'loop': st.loops[(g, s)]} if (g, s) in st.loops else {}),
                                         **({'reverse': True} if (g, s) in st.reverse else {}),
                                         **({'synth': SYNTHS[st.synth[(g, s)] - 1] if st.synth[(g, s)] else 'None'}
                                            if (g, s) in st.synth else {})}
                                for s in range(1, 17)}}
    scenes = [{'index': i, 'name': st.scene_names[i], 'groups': sc}
              for i, sc in enumerate(st.scenes) if sc]
    return {'format': 'fuzzy-proto', 'version': PROJECT_VERSION, 'name': st.project_name,
            'bpm': st.bpm, 'master': st.master, 'linked': sorted(st.linked),
            'groups': groups, 'scenes': scenes,
            'arrangement': st.arrangement, 'arr_loop': st.arr_loop, 'song_mode': st.arr_on}


def project_text(st: State) -> str:
    return json.dumps(project_data(st), indent=1, sort_keys=True)


def project_modified(st: State) -> bool:
    return project_text(st) != st.saved_text


def save_project(st: State) -> str:
    """Ecrit projects/<NOM>.fuzzy/project.json (l'ancienne version devient project.bak.json)."""
    folder = project_folder(st.project_name)
    folder.mkdir(parents=True, exist_ok=True)
    path = folder / 'project.json'
    text = project_text(st)
    if path.exists():
        path.replace(folder / 'project.bak.json')
    path.write_text(text)
    st.saved_text = text
    st.project_file = folder.stem                     # le fichier de ce projet
    return str(path)


def would_overwrite(st: State) -> bool:
    """Enregistrer ecraserait un AUTRE projet (meme nom) que celui qu'on a ouvert ?"""
    stem = project_folder(st.project_name).stem
    return stem in list_projects() and stem != st.project_file


def unique_name(base: str) -> str:
    names = set(list_projects())
    if project_folder(base).stem not in names:
        return base
    n = 2
    while project_folder(f'{base}-{n}').stem in names:
        n += 1
    return f'{base}-{n}'


def copy_project(name: str) -> str:
    """Copie le projet `name` sous un nouveau nom (NOM-2, NOM-3...)."""
    import shutil
    new = unique_name(name)
    shutil.copytree(PROJECTS_DIR / f'{name}.fuzzy', project_folder(new))
    path = project_folder(new) / 'project.json'
    data = json.loads(path.read_text())
    data['name'] = new
    path.write_text(json.dumps(data, indent=1, sort_keys=True))
    return new


def delete_project(name: str) -> None:
    """Supprime projects/<name>.fuzzy (et rien en dehors du dossier projects)."""
    import shutil
    folder = (PROJECTS_DIR / f'{name}.fuzzy').resolve()
    if folder.parent == PROJECTS_DIR.resolve() and folder.suffix == '.fuzzy' and folder.is_dir():
        shutil.rmtree(folder)


def load_project(st: State, name: str) -> None:
    """Ouvre projects/<name>.fuzzy : la lecture s'arrete, tout le morceau est remplace."""
    data = json.loads((PROJECTS_DIR / f'{name}.fuzzy' / 'project.json').read_text())
    st.playing = False
    st.pat_queue.clear()
    st.undo_stack.clear()
    st.redo_stack.clear()
    reset_playback(st)
    st.bpm = int(data.get('bpm', 120))
    st.master = int(data.get('master', st.master))
    st.linked = set(data.get('linked', [])) & set(GROUPS)
    st.project_name = data.get('name', name)
    for g in GROUPS:
        gd = data.get('groups', {}).get(g, {})
        st.patterns[g] = [None] * PAT_SLOTS
        for pd in gd.get('patterns', []):
            grid = [[None] * 16 for _ in range(ROWS)]
            for cell in pd.get('cells', []):
                r, t, note, vel = cell[:4]
                if 0 <= r < ROWS and 0 <= t < 16:
                    grid[r][t] = make_cell(tuple(note) if isinstance(note, list) else note, vel,
                                           int(cell[4]) if len(cell) > 4 else 1,
                                           int(cell[5]) if len(cell) > 5 else 0)
            st.patterns[g][pd['index']] = {'grid': grid, 'length': pd.get('length', 16),
                                           'name': pd.get('name', '')}
        cur = gd.get('current', 0)
        if st.patterns[g][cur] is None:
            st.patterns[g][cur] = new_pattern(16)
        st.pat_index[g] = cur
        st.grids[g] = st.patterns[g][cur]['grid']
        (st.pat_off.add if gd.get('off') else st.pat_off.discard)(g)
        st.group_vol[g] = gd.get('volume', 100)
        st.group_pan[g] = gd.get('pan', 64)
        st.group_names[g] = gd.get('name', '')
        st.kit_names[g] = gd.get('kit', '')
        for snd in range(1, 17):
            st.samples.pop((g, snd), None)
            st.synth.pop((g, snd), None)
            st.loops.pop((g, snd), None)
            st.reverse.discard((g, snd))
        if gd.get('key'):
            st.keys[g].update(gd['key'])
        for s, sd in gd.get('sounds', {}).items():
            st.sound_vol[(g, int(s))] = sd.get('volume', 100)
            st.params[(g, int(s))] = list(sd.get('params', st.params[(g, int(s))]))
            e = sd.get('envelope', {})
            st.env[(g, int(s))] = {'mode': e.get('mode', default_env(int(s))['mode']),
                                   'release': int(e.get('release', 150))}
            if sd.get('sample'):
                st.samples[(g, int(s))] = sd['sample']
            if sd.get('reverse'):                         # a l'envers (DAW Pulses)
                st.reverse.add((g, int(s)))
            if isinstance(sd.get('loop'), dict):         # boucle reglee dans DAW Pulses
                st.loops[(g, int(s))] = {'on': bool(sd['loop'].get('on')),
                                         'start': float(sd['loop'].get('start', 0)),
                                         'end': float(sd['loop'].get('end', 1))}
            if sd.get('synth') in SYNTHS:
                st.synth[(g, int(s))] = SYNTHS.index(sd['synth']) + 1
            elif sd.get('synth') == 'None':
                st.synth[(g, int(s))] = 0
    st.scenes = [None] * PAT_SLOTS
    st.scene_names = [''] * PAT_SLOTS
    for sd in data.get('scenes', []):
        st.scenes[sd['index']] = dict(sd.get('groups', {}))
        st.scene_names[sd['index']] = sd.get('name', '')
    st.scene_current = None
    st.arrangement = [{'scene': int(e['scene']), 'repeats': float(e.get('repeats', 1)),
                       **({'bars': float(e['bars'])} if e.get('bars') else {})}
                      for e in data.get('arrangement', []) if 0 <= int(e['scene']) < PAT_SLOTS]
    st.arr_loop = bool(data.get('arr_loop', True))
    st.arr_on = bool(data.get('song_mode', False))
    st.arr_sel = 0
    st.cursor = 0
    sync_samples(st)
    st.saved_text = project_text(st)
    st.project_file = name


def new_project(st: State) -> None:
    """Projet vide : patterns, scenes et noms effaces (sons et reglages gardes)."""
    st.playing = False
    st.pat_queue.clear()
    st.undo_stack.clear()
    st.redo_stack.clear()
    reset_playback(st)
    for g in GROUPS:
        st.patterns[g] = [None] * PAT_SLOTS
        st.patterns[g][0] = new_pattern(16)
        st.pat_index[g] = 0
        st.grids[g] = st.patterns[g][0]['grid']
    st.pat_off.clear()
    st.scenes = [None] * PAT_SLOTS
    st.scene_names = [''] * PAT_SLOTS
    st.scene_current = None
    st.arrangement, st.arr_on, st.arr_sel = [], False, 0
    existing = set(list_projects())
    n = 1
    while project_folder(f'SONG{n:02d}').stem in existing:
        n += 1
    st.project_name = f'SONG{n:02d}'
    st.cursor = 0
    st.saved_text = ''                                  # jamais enregistre
    st.project_file = None


def file_button(st: State, name: str) -> None:
    """Mode FILE : 5 SAVE, 6 LOAD, 7 NEW, 8 NAME ; Shift + 6 COPY, Shift + 7 DELETE (projet
    choisi). Question YES (bouton 5) / NO (bouton 6) : SAVE sur un projet qui existe deja,
    LOAD / NEW avec des changements non enregistres, DELETE."""
    now = time.monotonic()
    names = list_projects()
    selected = names[st.file_sel % len(names)] if names else None
    if st.file_ask:                                  # une question est posee : YES / NO
        action, target = st.file_ask
        st.file_ask = None
        if name == 'screen_5':
            file_do(st, action, target)
        else:
            st.toast = ('Cancelled', now + 2)
        return
    shift = 'shift' in st.held
    action = {'screen_5': 'SAVE', 'screen_6': 'COPY' if shift else 'LOAD',
              'screen_7': 'DELETE' if shift else 'NEW', 'screen_8': 'NAME'}[name]
    if action == 'NAME':
        st.name_edit, st.name_pos = (None, 0) if st.name_edit else (('project',), 0)
        st.file_moved = False                        # SAVE : sous le nouveau nom
        return
    if action in ('LOAD', 'COPY', 'DELETE') and selected is None:
        st.toast = ('No project yet: SAVE first', now + 2)
        return
    if action == 'SAVE' and st.file_moved and selected and selected != project_folder(st.project_name).stem:
        st.file_ask = ('SAVE', selected)             # projet choisi dans la liste : on ecrase lui
    elif action == 'SAVE' and project_folder(st.project_name).stem in names:
        st.file_ask = ('SAVE', None)                 # le fichier existe : on demande
    elif action in ('LOAD', 'NEW') and project_modified(st):
        st.file_ask = (action, selected)
    elif action == 'DELETE':
        st.file_ask = ('DELETE', selected)
    else:
        file_do(st, action, None if action == 'SAVE' else selected)


def file_question(st: State) -> str:
    action, target = st.file_ask
    return {'SAVE': f'Overwrite {target or st.project_name}?',
            'LOAD': 'Not saved! Load anyway?', 'NEW': 'Not saved! New project anyway?',
            'DELETE': f'Delete {target}?'}[action]


def file_do(st: State, action: str, selected) -> None:
    now = time.monotonic()
    if action == 'SAVE':
        if selected:                                 # enregistrer sous le nom choisi dans la liste
            st.project_name = selected
        path = save_project(st)
        st.toast = (f'Saved: {st.project_name}', now + 2)
        print(f'Projet enregistre : {path}')
    elif action == 'LOAD':
        load_project(st, selected)
        st.toast = (f'Loaded: {st.project_name}', now + 2)
    elif action == 'NEW':
        new_project(st)
        st.toast = (f'New project: {st.project_name}', now + 2)
    elif action == 'COPY':
        new = copy_project(selected)
        st.toast = (f'Copy: {new}', now + 2)
        print(f'Projet copie : {selected} -> {new}')
        selected = new
    elif action == 'DELETE':
        delete_project(selected)
        if selected == st.project_file:              # le projet ouvert n'a plus de fichier
            st.project_file, st.saved_text = None, ''
        st.toast = (f'Deleted: {selected}', now + 2)
        print(f'Projet supprime : {selected}')
        selected = None
    names = list_projects()
    cur = selected if selected in names else project_folder(st.project_name).stem
    st.file_sel = names.index(cur) if cur in names else min(st.file_sel, max(0, len(names) - 1))
    st.file_moved = False


def draw_file(st: State, half: int) -> Image.Image:
    acc = group_rgb(st)
    img = Image.new('RGB', (W, H), (10, 10, 14))
    d = ImageDraw.Draw(img)
    names = list_projects()
    if half == 0:
        header(d, 'PROJECTS', f'{len(names)} saved', acc)
        if not names:
            d.text((12, 50), 'No project yet.', font=font(16), fill='white')
            d.text((12, 76), 'Button 5 (SAVE) saves this one.', font=font(13), fill=(170, 170, 170))
        sel = st.file_sel % len(names) if names else 0
        top = max(0, min(sel - 4, len(names) - 9))
        current = project_folder(st.project_name).stem
        for k, n in enumerate(names[top:top + 9]):
            y = 34 + k * 24
            i = top + k
            if i == sel:
                d.rectangle((4, y - 2, W - 4, y + 20), fill=shade(acc, 0.5))
            d.text((14, y), ('> ' if n == current else '  ') + n, font=font(15),
                   fill='white' if i == sel else (190, 190, 190))
        d.text((10, 252), 'Encoder 1 / 4D wheel: choose', font=font(12), fill=(170, 170, 170))
        return img
    modified = project_modified(st)
    header(d, '', '', acc)
    shift = 'shift' in st.held
    labels = (('YES', 'NO', '', '') if st.file_ask else
              ('SAVE', 'COPY', 'DELETE', 'NAME') if shift else ('SAVE', 'LOAD', 'NEW', 'NAME'))
    for k, label in enumerate(labels):
        if label:
            softkey(d, st, k, label, label == 'NAME' and st.name_edit is not None)
    pats = sum(1 for g in GROUPS for p in st.patterns[g]
               if p and any(c for r in p['grid'][:p['length']] for c in r))
    scenes = sum(1 for sc in st.scenes if sc)
    d.text((12, 34), st.project_name, font=font(22), fill='white')
    d.text((12, 64), 'not saved' if modified else 'everything is saved', font=font(14),
           fill=(240, 160, 30) if modified else (40, 200, 90))
    lines = [f'{pats} patterns with notes, {scenes} scenes, {len(st.arrangement)} song entries',
             f'{st.bpm} BPM', '',
             'SAVE: under the name chosen in the list (if moved).',
             'SHIFT: copy or delete the selected project.',
             'SHIFT + FILE: quick save from any mode.',
             'Quitting saves automatically (old version',
             'kept as project.bak.json).']
    for k, line in enumerate(lines):
        d.text((12, 92 + k * 20), line, font=font(13), fill=(190, 190, 190))
    if st.file_ask:                                  # question YES / NO
        rounded(d, (40, 100, 440, 190), fill=(14, 14, 20), outline=acc, width=2)
        q = file_question(st)
        d.text((240 - d.textlength(q, font=font(18)) / 2, 115), q, font=font(18), fill='white')
        d.text((240 - d.textlength('YES  /  NO', font=font(14)) / 2, 152), 'YES  /  NO', font=font(14), fill=acc)
    else:
        toast_box(st, d, acc)
    name_overlay(st, d)
    return img


ARR_MAX = 64                                          # entrees dans l'arrangement


def scene_rows(st: State, idx: int) -> int:
    """Longueur d'une scene : le plus long de ses patterns (16 lignes si aucun)."""
    sc = st.scenes[idx] if 0 <= idx < PAT_SLOTS else None
    lengths = [st.patterns[g][p]['length'] for g, p in (sc or {}).items()
               if p is not None and p != OFF and st.patterns[g][p]]
    return max(lengths, default=16)


def entry_rows(st: State, e: dict) -> int:
    """Duree d'une entree : longueur choisie (encodeur 4, en mesures), sinon scene x repetitions."""
    return round(e['bars'] * 16) if e.get('bars') else round(scene_rows(st, e['scene']) * e['repeats'])


def arr_start_entry(st: State) -> None:
    """Debut d'une entree : tous les groupes passent ensemble aux patterns de sa scene."""
    entry = st.arrangement[st.arr_pos]
    sc = st.scenes[entry['scene']]
    st.pat_queue.clear()
    for g, p in (sc or {}).items():
        if p is None:                                # FREE : le groupe garde son pattern
            continue
        if p == OFF:
            st.pat_off.add(g)
            st.pat_start[g] = st.playhead
        else:
            apply_pattern(st, g, p)
    if sc:
        st.scene_current = entry['scene']
    st.arr_len = entry_rows(st, entry)
    st.arr_rows = 0


def arr_step(st: State) -> bool:
    """Appele a chaque ligne en mode SONG, avant de la jouer. False : fin du morceau."""
    if not st.arrangement:
        return True
    if st.arr_rows == 0 or st.arr_pos >= len(st.arrangement):
        if st.arr_pos >= len(st.arrangement):
            if not st.arr_loop:
                return False
            st.arr_pos = 0
        arr_start_entry(st)
    return True


def arr_after_row(st: State) -> None:
    st.arr_rows += 1
    if st.arr_rows >= st.arr_len:                    # entree finie : la suivante
        st.arr_pos += 1
        st.arr_rows = 0


def arranger_pad(st: State, pad: int) -> None:
    """Mode ARRANGER : pad = ajouter cette scene apres l'entree choisie."""
    idx = st.scene_bank * 16 + pad - 1
    now = time.monotonic()
    if st.scenes[idx] is None:
        st.toast = (f'S{idx + 1:02d} is empty (make it in SCENE)', now + 2)
        return
    if len(st.arrangement) >= ARR_MAX:
        st.toast = (f'{ARR_MAX} entries maximum', now + 2)
        return
    pos = st.arr_sel + 1 if st.arrangement else 0
    st.arrangement.insert(pos, {'scene': idx, 'repeats': 1})
    st.arr_sel = pos


def arranger_button(st: State, name: str) -> bool:
    """Boutons du mode ARRANGER. True si le bouton est utilise ici."""
    now = time.monotonic()
    if name == 'screen_5':                           # SONG : la lecture suit l'arrangement
        st.arr_on = not st.arr_on
        st.arr_pos, st.arr_rows = 0, 0
        st.toast = ('SONG mode ON' if st.arr_on else 'SONG mode OFF', now + 2)
    elif name == 'screen_6' and st.arrangement:      # GO : jouer a partir de l'entree choisie
        st.arr_on = True
        st.arr_pos, st.arr_rows = st.arr_sel, 0
        if not st.playing:
            st.playing = True
            st.next_row_time = now
    elif name == 'screen_7':                         # LOOP
        st.arr_loop = not st.arr_loop
    elif name == 'screen_3':                         # HALF : repetitions et longueur par demi
        st.arr_half = not st.arr_half
    elif name == 'erase' and st.arrangement:         # ERASE : enlever l'entree choisie
        del st.arrangement[st.arr_sel]
        st.arr_sel = max(0, min(st.arr_sel, len(st.arrangement) - 1))
    elif name == 'duplicate' and st.arrangement and 'shift' not in st.held:
        if len(st.arrangement) < ARR_MAX:            # DUPLICATE : doubler l'entree choisie
            st.arrangement.insert(st.arr_sel + 1, dict(st.arrangement[st.arr_sel]))
            st.arr_sel += 1
    elif name in ('nav_prev', 'nav_next'):           # < > : banque de scenes des pads
        st.scene_bank = max(0, min(PAT_BANKS - 1, st.scene_bank + (1 if name == 'nav_next' else -1)))
    else:
        return False
    return True


def arranger_knob(st: State, i: int, delta: int) -> None:
    """1 entree choisie, 2 scene de l'entree, 3 repetitions, 4 longueur en mesures."""
    if i not in st.arr_knobs:
        return
    n = st.arr_knobs[i].steps(delta)
    if not n or not st.arrangement:
        return
    if i == 0:
        st.arr_sel = max(0, min(len(st.arrangement) - 1, st.arr_sel + n))
        return
    e = st.arrangement[st.arr_sel]
    step = 0.5 if st.arr_half else 1                # bouton 3 (HALF) : par demi
    if i == 2:                                       # repetitions : longueur auto
        e['repeats'] = max(0.5, min(16, e['repeats'] + n * step))
        e.pop('bars', None)
    elif i == 3:                                     # longueur choisie, en mesures
        auto = scene_rows(st, e['scene']) * e['repeats'] / 16
        bars = (e.get('bars') or auto) + n * step
        bars = round(bars * 2) / 2                   # toujours un multiple de 0,5
        if bars < 0.5 or bars == auto:
            e.pop('bars', None)                      # retour a la longueur automatique
        else:
            e['bars'] = min(64, bars)
    else:
        used = [k for k, sc in enumerate(st.scenes) if sc]
        if used:
            k = used.index(e['scene']) if e['scene'] in used else 0
            e['scene'] = used[(k + n) % len(used)]


def draw_arranger(st: State, half: int) -> Image.Image:
    acc = group_rgb(st)
    img = Image.new('RGB', (W, H), (10, 10, 14))
    d = ImageDraw.Draw(img)
    arr = st.arrangement
    playing_entry = st.arr_pos if st.arr_on and st.playing else None
    if half == 0:
        total = sum(entry_rows(st, e) for e in arr)
        header(d, f'ARRANGER  {total / 16:g} bars', '', acc)      # a droite : bouton HALF
        softkey(d, st, 2, 'HALF', st.arr_half)       # bouton 3 (au-dessus de Repeats) : par demi
        if not arr:
            d.text((12, 50), 'Empty song.', font=font(16), fill='white')
            d.text((12, 76), 'Tap pads (scenes) to add them in order.', font=font(13), fill=(170, 170, 170))
        top = max(0, min(st.arr_sel - 4, len(arr) - 9))
        for k, e in enumerate(arr[top:top + 9]):
            i, y = top + k, 32 + k * 24
            if i == st.arr_sel:
                d.rectangle((4, y - 2, W - 4, y + 20), fill=shade(acc, 0.5))
            if i == playing_entry:
                d.text((0, y), '>', font=font(15), fill=(40, 220, 90))
            name = st.scene_names[e['scene']] if st.scenes[e['scene']] else '(empty)'
            bars = entry_rows(st, e) / 16
            ink = 'white' if i == st.arr_sel else (190, 190, 190)
            # colonnes alignees sur les encodeurs 1-4 (Entry / Scene / Repeats / duree)
            d.text((14, y), f'{i + 1:2d}', font=font(15), fill=ink)
            d.text((130, y), f"S{e['scene'] + 1:02d} {name}", font=font(15), fill=ink)
            d.text((250, y), f"x{e['repeats']:g}", font=font(15), fill=ink)
            d.text((370, y), f'{bars:g} bar' + ('s' if bars != 1 else ''), font=font(15),
                   fill=ink if e.get('bars') else (150, 150, 150))   # blanc : longueur choisie
        for k, t in enumerate(['Entry', 'Scene', 'Repeats', 'Length']):
            d.text((k * 120 + 60 - d.textlength(t, font=font(12)) / 2, 252), t, font=font(12), fill=(170, 170, 170))
        return img
    header(d, '', '', acc)
    softkey(d, st, 0, 'SONG', st.arr_on)
    softkey(d, st, 1, 'GO', False)
    softkey(d, st, 2, 'LOOP', st.arr_loop)
    if playing_entry is not None and playing_entry < len(arr):
        e = arr[playing_entry]
        rows = scene_rows(st, e['scene'])
        rep_now = st.arr_rows // max(1, rows) + 1
        d.text((12, 34), f"Playing {playing_entry + 1}/{len(arr)}: S{e['scene'] + 1:02d} "
                         f"{st.scene_names[e['scene']]}", font=font(16), fill='white')
        d.text((12, 58), f"repeat {min(rep_now, max(1, round(e['repeats'] + 0.49)))}/{e['repeats']:g}",
               font=font(14), fill=acc)
        frac = st.arr_rows / max(1, st.arr_len)
        d.rectangle((12, 80, 468, 90), outline=(70, 70, 80))
        d.rectangle((12, 80, 12 + round(456 * frac), 90), fill=acc)
    else:
        d.text((12, 34), 'SONG mode ON: PLAY follows the song' if st.arr_on
               else 'SONG mode OFF: scenes are played by hand', font=font(14), fill='white')
    lines = ['Pads: add a scene after the selected entry',
             '(< > : scene bank).  ERASE: remove the entry.',
             'DUPLICATE: double the entry.',
             'SONG: on / off.  GO: play from the selected entry.',
             'LOOP: start again at the end (otherwise stop).']
    for k, line in enumerate(lines):
        d.text((12, 104 + k * 20), line, font=font(13), fill=(180, 180, 180))
    d.text((12, 210), 'Entry length: longest pattern x repeats, or set', font=font(12), fill=(140, 140, 140))
    d.text((12, 226), 'with encoder 4 (Length, in bars).', font=font(12), fill=(140, 140, 140))
    toast_box(st, d, acc)
    return img


def plugin_targets(st: State) -> list:
    """Sons modifies en PLUG-IN : la selection (SELECT + pads), sinon le son choisi."""
    return sorted(st.plugin_sel) or [st.sound]


def browse_move(st: State, n: int) -> None:
    """Liste ouverte de PLUG-IN : kit ou synthe suivant / precedent (molette 4D, encodeur 5)."""
    if st.browse == 'kit':
        st.kit_sel = max(0, min(len(list_kits()) - 1, st.kit_sel + n))
    elif st.browse == 'synth':
        st.synth_sel = max(0, min(len(SYNTHS), st.synth_sel + n))      # + None


def plugin_button(st: State, name: str) -> None:
    """PLUG-IN : 6 KIT (liste des kits ; 2e appui = LOAD), 8 SYNTH (liste des synthes ;
    2e appui = LOAD), 7 CANCEL."""
    now = time.monotonic()
    targets = plugin_targets(st)
    if name == 'screen_7':                           # CANCEL
        st.browse = None
    elif name == 'screen_6' and st.browse is None:   # ouvrir la liste des kits
        kits = list_kits()
        if not kits:
            st.toast = ('No kit: folders of .wav in prototypes/kits or ~/Music/Fuzzy Kits', now + 3)
            return
        names = [k[0] for k in kits]
        cur = st.kit_names[st.group]
        st.kit_sel = names.index(cur) if cur in names else min(st.kit_sel, len(kits) - 1)
        st.browse = 'kit'
    elif name == 'screen_6' and st.browse == 'kit':  # LOAD : le kit choisi sur le groupe
        kits = list_kits()
        if kits:
            load_kit(st, kits[min(st.kit_sel, len(kits) - 1)][1])
        st.browse = None
    elif name == 'screen_8' and st.browse is None:   # ouvrir la liste des synthes
        eff = st.eff_synth(st.group, targets[0])
        st.synth_sel = eff - 1 if eff else len(SYNTHS)     # dernier de la liste : None
        st.browse = 'synth'
    elif name == 'screen_8' and st.browse == 'synth':   # LOAD : ce synthe sur les sons choisis
        synth = st.synth_sel + 1 if st.synth_sel < len(SYNTHS) else 0     # 0 = None (aucun)
        for snd in targets:
            st.samples.pop((st.group, snd), None)
            if synth == default_synth(snd) and not st.group.endswith('2'):
                st.synth.pop((st.group, snd), None)
            else:
                st.synth[(st.group, snd)] = synth
            if synth:
                st.env[(st.group, snd)]['mode'] = 'ONE-SHOT' if synth <= 4 else 'ADSR'
        if not any((st.group, snd) in st.samples for snd in range(1, 17)):
            st.kit_names[st.group] = ''
        sync_samples(st)
        st.browse = None
        st.toast = (f"{SYNTHS[synth - 1] if synth else 'No instrument'} on sound{'s' if len(targets) > 1 else ''} "
                    + ', '.join(f'{snd:02d}' for snd in targets), now + 2)


def plugin_knob(st: State, i: int, delta: int) -> None:
    """PLUG-IN : 1 = mode d'enveloppe, 2 = relachement, pour tous les sons choisis ;
    5 = kit ou synthe dans la liste ouverte (KIT / SYNTH)."""
    if i not in st.plugin_knobs:
        return
    n = st.plugin_knobs[i].steps(delta)
    if not n:
        return
    if i == 4:
        browse_move(st, n)
        return
    targets = plugin_targets(st)
    first = st.env[(st.group, targets[0])]
    if i == 0:
        mode = ENV_MODES[(ENV_MODES.index(first['mode']) + n) % len(ENV_MODES)]
        for s in targets:
            st.env[(st.group, s)]['mode'] = mode
    else:
        k = min(range(len(RELEASES)), key=lambda j: abs(RELEASES[j] - first['release']))
        rel = RELEASES[max(0, min(len(RELEASES) - 1, k + n))]
        for s in targets:
            st.env[(st.group, s)]['release'] = rel


def draw_plugin(st: State, half: int) -> Image.Image:
    """PLUG-IN : enveloppe des sons du groupe (ONE-SHOT / ADSR, relachement)."""
    acc = group_rgb(st)
    img = Image.new('RGB', (W, H), (10, 10, 14))
    d = ImageDraw.Draw(img)
    targets = plugin_targets(st)
    if half == 0:
        header(d, f'PLUG-IN  Group {st.group}', f'{len(targets)} sound' + ('s' if len(targets) > 1 else ''), acc)
        for i in range(16):
            pad = 13 + i % 4 - 4 * (i // 4)            # pad 13 en haut a gauche
            x, y = 8 + (i % 4) * 117, 32 + (i // 4) * 52
            env = st.env[(st.group, pad)]
            chosen = pad in targets
            rounded(d, (x, y, x + 110, y + 46), fill=acc if chosen else (24, 24, 32),
                    outline=shade(acc, 0.6))
            silent = not st.sound_audible(st.group, pad) or not st.instrument(st.group, pad)
            tag = st.sound_tag(pad)
            ink = 'black' if chosen else (110, 110, 110) if silent else 'white'
            name = f'{pad:02d} {st.sound_name(pad)}'
            d.text((x + 55 - d.textlength(name, font=font(12)) / 2, y + 5), name, font=font(12), fill=ink)
            mode = {'M': 'MUTE', 'S': 'SOLO'}.get(tag) or ('ADSR' if env['mode'] == 'ADSR' else 'ONE-SHOT')
            color = ((230, 70, 70) if tag == 'M' else (240, 200, 40) if tag == 'S'
                     else (90, 90, 90) if silent else (170, 170, 170))
            d.text((x + 55 - d.textlength(mode, font=font(13)) / 2, y + 25), mode, font=font(13),
                   fill=ink if chosen else color)
        for k, t in enumerate(['Mode', 'Release', '', '']):
            d.text((k * 120 + 60 - d.textlength(t, font=font(12)) / 2, 252), t, font=font(12), fill=(170, 170, 170))
        return img
    def keys(labels, title):                         # etiquettes des boutons 5-8, en haut
        header(d, title, '', acc)
        for k, t in enumerate(labels):
            if t:
                softkey(d, st, k, t, False)

    if st.browse:                                      # liste des kits / des synthes
        if st.browse == 'kit':
            items = [(kname, f'{len(kit_files(folder))} sounds') for kname, folder in list_kits()]
            sel, labels = st.kit_sel, ['', 'LOAD', 'CANCEL', '']
            hint = '4D wheel / encoder 5: choose.  Own kits: ~/Music/Fuzzy Kits'
        else:
            items = ([(name, 'drum' if k < 4 else 'notes') for k, name in enumerate(SYNTHS)]
                     + [('None', 'empty')])
            sel, labels = st.synth_sel, ['', '', 'CANCEL', 'LOAD']
            hint = '4D wheel / encoder 5: choose.  LOAD: on the chosen sounds'
        keys(labels, 'KIT' if st.browse == 'kit' else 'SYNTH')
        top = max(0, min(sel - 4, len(items) - 9))
        for k, (iname, info) in enumerate(items[top:top + 9]):
            y = 34 + k * 21
            chosen = top + k == sel
            if chosen:
                d.rectangle((6, y - 1, W - 6, y + 18), fill=acc)
            ink = 'black' if chosen else 'white'
            d.text((12, y), short(iname, 40), font=font(14), fill=ink)
            d.text((W - 80, y), info, font=font(12), fill=ink if chosen else (150, 150, 150))
        if not items:
            d.text((12, 40), 'Nothing found.', font=font(14), fill='white')
        d.text((12, 250), hint, font=font(11), fill=(140, 140, 140))
        return img
    keys(['', 'KIT', '', 'SYNTH'], 'ENVELOPE')
    modes = {st.env[(st.group, s)]['mode'] for s in targets}
    rels = {st.env[(st.group, s)]['release'] for s in targets}
    names = ', '.join(f'{s:02d}' for s in targets)
    d.text((12, 34), short('Sounds: ' + names, 52), font=font(14), fill='white')
    param_box(d, 0, 'Mode', modes.pop() if len(modes) == 1 else 'mixed', acc, y=60)
    param_box(d, 1, 'Release', f'{rels.pop()} ms' if len(rels) == 1 else 'mixed', acc, y=60,
              dim=all(st.env[(st.group, s)]['mode'] == 'ONE-SHOT' for s in targets))
    src = st.samples.get((st.group, targets[0]))
    eff = st.eff_synth(st.group, targets[0])
    synth = SYNTHS[eff - 1] if eff else 'none (empty sound)'
    d.text((12, 150), short(('Sample: ' + Path(src).name) if src else f'Synth: {synth}', 52),
           font=font(13), fill='white')
    if st.kit_names[st.group]:
        d.text((12, 168), short(f'Kit on group {st.group}: {st.kit_names[st.group]}', 52),
               font=font(12), fill=(170, 170, 170))
    lines = ['ONE-SHOT: plays to its end. ADSR: holds the note,',
             'then fades out with the release.',
             'SELECT + pads: several sounds at once.',
             'KIT: 16 samples on the pads.  SYNTH: a synth sound.']
    for k, line in enumerate(lines):
        d.text((12, 192 + k * 15), line, font=font(12), fill=(170, 170, 170))
    return img


# --- EVENTS : vue piano roll de la piste choisie ---------------------------------------
PR_ZOOMS = (16, 32, 48, 64, 128, 256)                 # lignes visibles
PR_ROWS = 18                                          # hauteurs visibles (demi-tons)


def pr_window(st: State) -> tuple:
    cols = st.pr_zoom                                # meme plus long que le pattern (zone grisee)
    start = (st.cursor // cols) * cols
    return start, cols


def pr_follow_pitch(st: State) -> None:
    """La vue suit la note choisie si elle sort de l'ecran."""
    cell = st.grid[st.cursor][st.sound - 1]
    if cell:
        notes = cell_notes(cell[0])
        if min(notes) < st.pr_low:
            st.pr_low = max(0, min(notes) - 2)
        elif max(notes) >= st.pr_low + PR_ROWS:
            st.pr_low = min(127 - PR_ROWS, max(notes) - PR_ROWS + 3)


def events_knob(st: State, i: int, delta: int) -> None:
    """Gauche : 1 ligne, 2 zoom, 3 hauteur de la vue, 4 piste.
    Droite : 5 deplacer la note, 6 hauteur, 7 velocite, 8 duree."""
    if i not in st.pr_knobs:
        return
    n = st.pr_knobs[i].steps(delta)
    if not n:
        return
    t = st.sound - 1
    if i == 0:
        st.cursor = max(0, min(st.length - 1, st.cursor + n))
        pr_follow_pitch(st)
    elif i == 1:
        k = PR_ZOOMS.index(st.pr_zoom) if st.pr_zoom in PR_ZOOMS else 1
        st.pr_zoom = PR_ZOOMS[max(0, min(len(PR_ZOOMS) - 1, k + n))]
    elif i == 2:
        st.pr_low = max(0, min(127 - PR_ROWS, st.pr_low + n))
    elif i == 3:
        st.sound = max(1, min(16, st.sound + n))
        pr_follow_pitch(st)
    elif i == 4 and 'shift' in st.held:              # Shift : decalage fin (1/24 de ligne)
        cell = st.grid[st.cursor][t]
        if cell:
            snapshot(st, coalesce=True)
            st.grid[st.cursor][t] = with_off(cell, cell_off(cell) + n)
    elif i == 4:                                     # deplacer la note dans le temps
        cell = st.grid[st.cursor][t]
        to = st.cursor + n
        if cell and 0 <= to < st.length and not st.grid[to][t]:
            snapshot(st, coalesce=True)
            st.grid[to][t], st.grid[st.cursor][t] = cell, None
            st.cursor = to
    elif i in (5, 6):                                # hauteur / velocite
        edit_cell(st, i == 5, n)
        pr_follow_pitch(st)
    elif i == 7:                                     # duree (lignes)
        cell = st.grid[st.cursor][t]
        if cell:
            snapshot(st, coalesce=True)
            st.grid[st.cursor][t] = with_len(cell, max(1, min(st.length, cell_len(cell) + n)))


def draw_events(st: State, half: int) -> Image.Image:
    acc = group_rgb(st)
    img = Image.new('RGB', (W, H), (10, 10, 14))
    d = ImageDraw.Draw(img)
    t = st.sound - 1
    cell = st.grid[st.cursor][t]
    if half == 0:
        rec = step_label(st) + ('REC  ' if st.recording else '')
        header(d, f'{pattern_title(st)}  T{st.sound:02d}', f'{rec}', acc)
        softkey(d, st, 2, 'TRACKER', False)         # bouton 3 : passer au tracker
        start, cols = pr_window(st)
        x0, y0, gw, rh = 34, 30, 440, 12
        cw = gw / cols
        top = st.pr_low + PR_ROWS - 1                # hauteur du haut de l'ecran
        end = x0 + max(0, min(cols, st.length - start)) * cw   # fin du pattern dans la vue
        for k in range(PR_ROWS):                     # lignes de hauteur (touches noires sombres)
            pitch = top - k
            y = y0 + k * rh
            black = pitch % 12 in (1, 3, 6, 8, 10)
            d.rectangle((x0, y, x0 + gw, y + rh - 1), fill=(16, 16, 22) if black else (24, 24, 32))
            if end < x0 + gw:                        # au-dela de la longueur du pattern
                d.rectangle((end, y, x0 + gw, y + rh - 1), fill=(6, 6, 8))
            if pitch % 12 == 0 or k == PR_ROWS - 1:
                d.text((2, y - 1), note_name(pitch), font=font(10), fill=(150, 150, 150))
        for c in range(cols + 1):                    # temps, mesures
            r = start + c
            if r % 4 == 0:
                x = x0 + c * cw
                d.line((x, y0, x, y0 + PR_ROWS * rh), fill=(70, 70, 85) if r % 16 == 0 else (40, 40, 50))
        playing = play_row(st)
        if start <= playing < start + cols:
            x = x0 + (playing - start) * cw
            d.rectangle((x, y0, x + max(1, cw - 1), y0 + PR_ROWS * rh), fill=(0, 70, 45))
        x = x0 + (st.cursor - start) * cw            # curseur
        d.rectangle((x, y0, x + max(2, cw - 1), y0 + PR_ROWS * rh), outline=(120, 120, 140))
        for r in range(st.length):                   # notes (barres), meme celles qui debordent
            c = st.grid[r][t]
            if not c:
                continue
            length = cell_len(c)
            off = cell_off(c) / TIMING                   # decalage fin : la barre glisse
            if r + off + length <= start or r + off >= start + cols:
                continue
            a = max(r + off, start) - start
            b = min(r + off + length, start + cols) - start
            for n in cell_notes(c[0]):
                if not st.pr_low <= n <= top:
                    continue
                y = y0 + (top - n) * rh
                bright = 0.45 + 0.55 * c[1] / 127    # velocite = luminosite
                sel = r == st.cursor
                d.rectangle((x0 + a * cw + 1, y + 1, x0 + b * cw - 1, y + rh - 2),
                            fill='white' if sel else shade(acc, bright), outline=None)
        for k, label in enumerate(['Line', f'Zoom {cols}', 'View', 'Track']):
            d.text((k * 120 + 60 - d.textlength(label, font=font(12)) / 2, 252), label, font=font(12), fill=(170, 170, 170))
        return img
    header(d, 'NOTE', f'Line {st.cursor:02X}', acc)
    if cell:
        param_box(d, 0, 'Position', f'{st.cursor:02X}', acc, y=40)
        param_box(d, 1, 'Note', cell_label(cell[0]).strip(), acc, y=40)
        param_box(d, 2, 'Velocity', f'{cell[1]}', acc, y=40)
        n = cell_len(cell)
        param_box(d, 3, 'Length', f'{n} line' + ('s' if n > 1 else ''), acc, y=40)
        o = cell_off(cell)
        d.text((W - 180, 128), 'Timing: ' + ('on the line' if not o else f'{o:+d}/24 line'),
               font=font(12), fill=acc if o else (150, 150, 150))
    else:
        d.text((12, 60), 'No note on this line.', font=font(16), fill='white')
        d.text((12, 86), 'Turn encoder 1 (Line) or the 4D wheel.', font=font(13), fill=(170, 170, 170))
    env = st.env[(st.group, st.sound)]['mode']
    lines = [f'Sound {st.sound:02d} envelope: {env} (PLUG-IN to change).',
             'Pads: play, REC: record (held = length).',
             'FIXED VEL + 4D wheel: velocity.  ERASE: delete.',
             'Shift + encoder 5: fine timing. Stopped:',
             'Shift + pads = Quantize / Clear / Copy...', 'EVENTS again: back.']
    for k, line in enumerate(lines):
        d.text((12, 140 + k * 20), line, font=font(12), fill=(180, 180, 180))
    toast_box(st, d, acc)
    return img


def current_combo(st: State) -> dict:
    """Les patterns qui jouent (ou joueront) maintenant, groupe par groupe."""
    return {g: st.pat_queue.get(g, OFF if g in st.pat_off else st.pat_index[g]) for g in GROUPS}


def launch_scene(st: State, idx: int) -> None:
    """Chaque groupe passe au pattern de la scene (en lecture : a la fin de son pattern).
    Un groupe LIBRE (None) garde le pattern qu'il joue : pratique en live."""
    for g, p in st.scenes[idx].items():
        if p is not None:
            select_pattern(st, g, p)
    st.scene_current = idx


def scene_free_toggle(st: State, g: str) -> None:
    """Shift + groupe (mode SCENE) : dans la scene en cours, ce groupe devient libre (il garde
    son pattern quand on lance la scene) ou redevient fixe (pattern qu'il joue maintenant)."""
    if st.scene_current is None or not st.scenes[st.scene_current]:
        return
    sc = st.scenes[st.scene_current]
    sc[g] = current_combo(st)[g] if sc.get(g) is None else None


def scene_pad(st: State, pad: int) -> None:
    """Mode SCENE : pad vide = y ranger les patterns en cours ; pad plein = lancer la scene ;
    SELECT + pad = remplacer par les patterns en cours ; DUPLICATE + pad = copier la scene
    choisie ici ; ERASE + pad = effacer ; Shift + pad = fonctions de la serigraphie."""
    idx = st.scene_bank * 16 + pad - 1
    if 'shift' in st.held:                           # Shift + pad : serigraphie, scene choisie
        scene_function(st, pad)
        return
    if 'erase' in st.held:
        st.scenes[idx] = None
        st.scene_names[idx] = ''
        if st.scene_current == idx:
            st.scene_current = None
    elif 'duplicate' in st.held:
        if st.scene_current is not None and st.scenes[st.scene_current]:
            st.scenes[idx] = dict(st.scenes[st.scene_current])
            st.scene_names[idx] = st.scene_names[st.scene_current]
            st.scene_current = idx
    elif st.scenes[idx] is None or 'select' in st.held:   # vide, ou SELECT + pad : remplacer
        old = st.scenes[idx] or {}
        combo = current_combo(st)                    # les groupes libres le restent
        st.scenes[idx] = {g: (None if g in old and old[g] is None else p) for g, p in combo.items()}
        st.scene_current = idx
    else:
        launch_scene(st, idx)


def name_target(st: State):
    """Nom modifie par l'editeur (bouton 8 en mode PATTERN / SCENE) : (texte, enregistrer)."""
    t = st.name_edit
    if t is None:
        return None
    if t[0] == 'project':
        return st.project_name, lambda v: setattr(st, 'project_name', v or 'SONG01')
    if t[0] == 'group':
        return st.group_names[t[1]], lambda v: st.group_names.__setitem__(t[1], v)
    if t[0] == 'pattern':
        pat = st.patterns[t[1]][t[2]]
        if pat is None:
            return None
        return pat.get('name', ''), lambda v: pat.__setitem__('name', v)
    return st.scene_names[t[1]], lambda v: st.scene_names.__setitem__(t[1], v)


def name_knob(st: State, i: int, delta: int) -> None:
    """Editeur de nom (encodeurs sous l'ecran droit) : 5 position, 6 lettre, 7 noms tout prets."""
    target = name_target(st)
    if target is None or i not in st.name_knobs:
        return
    n = st.name_knobs[i].steps(delta)
    if not n:
        return
    text, save = target
    chars = list(text.ljust(NAME_LEN))
    names_knob = 7 if st.name_edit[0] == 'project' else 6     # projet : noms sur l'encodeur 8
    if i in (6, 7) and i != names_knob:
        return
    if i == 4:
        st.name_pos = max(0, min(NAME_LEN - 1, st.name_pos + n))
    elif i == 5:
        k = NAME_CHARS.find(chars[st.name_pos])
        chars[st.name_pos] = NAME_CHARS[(max(k, 0) + n) % len(NAME_CHARS)]
        save(''.join(chars).rstrip())
    else:
        k = NAME_PRESETS.index(text) if text in NAME_PRESETS else 0
        save(NAME_PRESETS[(k + n) % len(NAME_PRESETS)])
        st.name_pos = 0


def name_overlay(st: State, d: ImageDraw.ImageDraw) -> None:
    """Cadre de l'editeur de nom (ecran droit)."""
    target = name_target(st)
    if target is None:
        return
    acc = group_rgb(st)
    t = st.name_edit
    what = (f'Pattern {t[2] + 1:02d} {t[1]}' if t[0] == 'pattern' else 'Project' if t[0] == 'project'
            else f'Group {t[1]}' if t[0] == 'group' else f'Scene {t[1] + 1:02d}')
    d.rectangle((20, 40, 460, 230), fill=(15, 15, 20), outline=acc, width=3)
    d.text((40, 50), f'NAME  {what}', font=font(16), fill=acc)
    text = target[0].ljust(NAME_LEN)
    for k, ch in enumerate(text):
        x = 60 + k * 46
        d.rectangle((x, 90, x + 40, 140), fill=acc if k == st.name_pos else (30, 30, 40))
        d.text((x + 20 - d.textlength(ch, font=font(30)) / 2, 96), ch, font=font(30),
               fill='black' if k == st.name_pos else 'white')
    d.text((40, 160), 'Encoders below: position / letter / ready names', font=font(13), fill=(180, 180, 180))
    d.text((40, 184), 'Press the same button again: done', font=font(13), fill=acc)
    d.rectangle((0, 248, W, H), fill=(15, 15, 20))
    labels = (('Position', 'Letter', '', 'Names') if st.name_edit[0] == 'project'
              else ('Position', 'Letter', 'Names'))
    for k, label in enumerate(labels):                  # sous les encodeurs 5-7 (projet : 5, 6, 8)
        d.text((k * 120 + 60 - d.textlength(label, font=font(12)) / 2, 252), label, font=font(12), fill=acc)


def schedule_cell(st: State, g: str, t: int, cell, when: float, now: float) -> None:
    """Joue une case maintenant, ou plus tard si elle est decalee (timing fin)."""
    if when <= now + 0.0005:
        play_cell(st, g, t, cell, now)
    else:
        st.pending.append((when, g, t, cell))


def play_cell(st: State, g: str, t: int, cell, now: float) -> None:
    note, vol = cell[0], cell[1]
    current = g == st.group
    if current:
        st.flash[t + 1] = now + 0.08
    v = out_vel(st, g, t + 1, vol)
    gate = 60 / st.bpm / 4 * (cell_len(cell) - 0.1)    # duree de la note (lignes)
    for n in cell_notes(note):                         # une note ou un accord
        if v:
            play_note(st, g, t + 1, v, n, gate)
        if current and t + 1 == st.sound:              # le clavier montre le son choisi
            st.note_flash[n] = (now + 0.15, vol)


def pending_tick(st: State) -> None:
    """Joue les notes decalees dont l'heure est venue."""
    if not st.pending:
        return
    if not st.playing:
        st.pending.clear()
        return
    now = time.monotonic()
    due = [p for p in st.pending if p[0] <= now]
    if due:
        st.pending = [p for p in st.pending if p[0] > now]
        for _, g, t, cell in due:
            play_cell(st, g, t, cell, now)


def click(st: State, accent: bool) -> None:
    """Un coup de metronome (synthe Click du moteur ; ni Mute ni Solo ne le coupent)."""
    vel = round((127 if accent else 95) * st.metro_vol / 127 * st.master / 127)
    if vel < 1:
        return
    SOUND.hit(1, vel, 96 if accent else 84, -1, 0, 50, 0, 6, 64)


def countin_tick(st: State) -> bool:
    """Decompte avant l'enregistrement : 4 temps (4 3 2 1), puis la lecture demarre."""
    start, done = st.countin
    beat = 60 / st.bpm
    now = time.monotonic()
    if done < 4 and now >= start + done * beat:
        click(st, done == 0)
        st.countin = (start, done + 1)
        return True
    if done >= 4 and now >= start + 4 * beat:          # fin du decompte : on enregistre
        st.countin = None
        st.recording = st.playing = True
        st.step_manual = None
        st.next_row_time = start + 4 * beat
        return True
    return False


def tick(st: State) -> bool:
    """Fait avancer la lecture : 4 lignes par temps. Renvoie True si une ligne a joue."""
    pending_tick(st)
    if st.countin is not None:
        return countin_tick(st)
    if not st.playing:
        return False
    now = time.monotonic()
    moved = False
    while now >= st.next_row_time:
        if st.arr_on and not arr_step(st):                  # SONG : fin du morceau (sans LOOP)
            st.playing = False
            reset_playback(st)
            return True
        for g in GROUPS:                                    # tous les groupes qui sonnent
            row = (st.playhead - st.pat_start[g]) % pattern_length(st, g)
            if row == 0 and g in st.pat_queue:              # pattern suivant : a la fin du pattern
                nxt = st.pat_queue.pop(g)
                if nxt == OFF:
                    st.pat_off.add(g)
                    st.pat_start[g] = st.playhead
                else:
                    apply_pattern(st, g, nxt)
                row = 0
            if not st.audible(g) or g in st.pat_off:    # groupe sans pattern : silence
                continue
            dur = 60 / st.bpm / 4
            at = st.next_row_time                           # heure prevue de cette ligne
            nxt = (row + 1) % pattern_length(st, g)
            for t in range(16):                             # toutes les pistes
                cell = st.grids[g][row][t]
                if cell and cell_off(cell) >= 0:            # a l'heure ou en retard
                    schedule_cell(st, g, t, cell, at + cell_off(cell) / TIMING * dur, now)
                early = st.grids[g][nxt][t]                 # ligne suivante, en avance
                if early and cell_off(early) < 0:
                    schedule_cell(st, g, t, early, at + (1 + cell_off(early) / TIMING) * dur, now)
        if st.metro and st.playhead % 4 == 0:              # TAP : un coup par temps, accent sur la mesure
            click(st, st.playhead % 16 == 0)
        st.playhead += 1
        if st.arr_on and st.arrangement:
            arr_after_row(st)
        st.next_row_time += 60 / st.bpm / 4
        moved = True
    return moved


# Retro-eclairage des boutons (rapport 0x80, 63 octets) - emplacements et
# niveaux repris du pilote r22yb3m (MIT). Boutons mono : 4 = doux, 7 = vif.
# Groupes A-H : couleurs de la palette NI, (douce, vive) pour chaque groupe.
BUTTON_SLOTS = {'fixed_vel': 45, 'plug_in': 1, 'events': 52, 'arranger': 2, 'file': 8, 'select': 55, 'restart': 37, 'pattern': 51, 'scene': 50, 'duplicate': 54, 'erase': 38, 'tempo': 23, 'play': 41, 'rec': 42, 'stop': 43, 'shift': 44, 'pad_mode': 46, 'keyboard': 47,
                'chords': 48, 'step': 49, 'nav_prev': 6, 'nav_next': 7,
                'mute': 57, 'solo': 56,
                'mixer': 3, 'volume': 20,  # MIXER / VOLUME : emplacements supposes (21 = SWING)
                'screen_3': 14, 'screen_5': 16, 'screen_6': 17, 'screen_7': 18, 'screen_8': 19,
                'midi_channel': 0, 'settings': 9, 'tap': 39, 'screen_1': 12, 'screen_2': 13, 'screen_4': 15, 'follow': 40, 'note_repeat': 22, 'lock': 24}
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
STEP_SOUND = 70                                        # blanc vif : son choisi (STEP + SELECT)
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
    on.add('restart')                                  # RESTART : toujours possible
    on.add('tap')                                      # TAP : metronome
    on.add('file')                                     # FILE : projets ; Shift + FILE : enregistrer
    on.add('arranger')                                 # ARRANGER : l'ordre des scenes
    on.add('plug_in')                                  # PLUG-IN : enveloppe des sons
    on.add('events')                                   # EVENTS : piano roll
    if st.events_mode:
        on |= {'erase', 'fixed_vel'}                   # FIXED VEL + molette : velocite
    on.add('settings')                                 # SETTINGS : couleurs, Maschine, audio, MIDI
    on.add('midi_channel')                             # MIDI : SETTINGS > MIDI
    if st.settings and not st.set_confirm:
        on |= {'screen_1', 'screen_2', 'screen_3', 'screen_4'}     # onglets
        if st.set_tab == 'STATUS':
            on |= {'screen_5', 'screen_8'}             # REFRESH, Shift + 8 : rendre a NI
        return on
    if st.nr_on:
        on.add('lock')                                 # LOCK : utile quand Note Repeat tourne
    if st.set_confirm:
        return on | {'screen_5', 'screen_6'}           # YES / NO
    if st.nr_menu:
        return on                                      # menu Note Repeat : tout aux encodeurs
    on |= {'pattern', 'scene'}                         # PATTERN / SCENE
    if st.file_mode:
        return on | {'screen_5', 'screen_6', 'screen_7', 'screen_8'}
    if st.plugin_mode:
        return on | {None: {'select', 'screen_6', 'screen_8'}, 'kit': {'screen_6', 'screen_7'},
                     'synth': {'screen_7', 'screen_8'}}[st.browse]
    if st.arranger_mode:
        return on | {'screen_3', 'screen_5', 'screen_6', 'screen_7', 'nav_prev', 'nav_next', 'erase',
                     'duplicate'}
    if st.pattern_mode or st.scene_mode:
        return on | {'nav_prev', 'nav_next', 'duplicate', 'erase', 'screen_8'} \
            | ({'screen_7'} if st.pattern_mode else set()) \
            | ({'select'} if st.scene_mode else set())     # 8 : NAME ; SELECT + groupe : FREE
    on.add('duplicate')                                # Shift + DUPLICATE : DOUBLE (pattern x 2)
    if kb_tracker(st):
        on |= {'nav_prev', 'nav_next'}                 # < > : pistes 1-8 / 9-16
    on.add('select')                                   # SELECT + pad : choisir le son
    if st.step_mode or st.mixer:
        on |= {'nav_prev', 'nav_next'}                 # < > : pages STEP ou pages du mixeur
    if st.mixer:
        on |= {'screen_3', 'screen_4'}                 # 3 TRACKER, 4 PAN
    if not st.mixer:
        on.add('screen_3')                             # TRACKER
        if st.right_view == 'keyboard' and not st.tracker16:
            on.add('screen_6')                         # PC KEYS
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
              'pattern': st.pattern_mode, 'scene': st.scene_mode, 'select': False, 'file': st.file_mode,
              'arranger': st.arranger_mode or st.arr_on, 'plug_in': st.plugin_mode, 'events': st.events_mode, 'fixed_vel': False, 'restart': False, 'duplicate': False, 'erase': False,
              'mute': bool(st.muted), 'solo': bool(st.solo),
              'mixer': st.mixer, 'volume': False,
              'screen_3': (st.set_tab in ('COLORS', 'KEYS')) if st.settings else
                          (not st.mixer and (st.tracker16 or kb_tracker(st))),
              'screen_8': (st.name_edit is not None) if (st.pattern_mode or st.scene_mode)
                          else (st.user_edit and not st.settings), 'screen_7': st.link_edit,
              'settings': st.settings, 'follow': st.follow,
              'midi_channel': st.settings and st.set_tab == 'MIDI',
              'screen_1': st.settings and st.set_tab == SET_TABS[0],
              'screen_2': st.settings and st.set_tab == SET_TABS[1],
              'screen_4': (st.set_tab == SET_TABS[3]) if st.settings else (st.mixer and st.mix_pan),
              'note_repeat': st.nr_active, 'lock': st.nr_lock,
              'tap': st.metro or st.countin is not None,
              'screen_5': st.arranger_mode and st.arr_on,
              'screen_6': st.right_view == 'keyboard' and st.pc['keys'] and not st.settings}
    if st.arranger_mode:
        active['screen_7'] = st.arr_loop
        active['screen_3'] = st.arr_half
    for name in available_buttons(st):
        slot = BUTTON_SLOTS[name]
        lit = active[name] or name in st.held
        if name.startswith('screen_'):
            f[1 + slot] = SCREEN_ACTIVE if lit else SCREEN_IDLE
        else:
            f[1 + slot] = BTN_BRIGHT if lit else BTN_DIM
    for i, g in enumerate(bank_groups(st)):           # 8 boutons = banque du groupe choisi
        dim, bright = hue_pair(st.group_hue[g])
        if st.link_edit:                               # LINK : groupes lies vifs, autres doux
            f[1 + GROUP_SLOT + i] = bright if (g in st.linked or g == st.group) else dim
            continue
        if not st.group_filled(g):                     # groupe sans instrument : eteint
            f[1 + GROUP_SLOT + i] = dim if g == st.group else 0      # (doux s'il est choisi)
        elif not st.audible(g):
            f[1 + GROUP_SLOT + i] = 0                  # groupe coupe (Mute, ou hors Solo) : eteint
        else:                                          # choisi ou en Solo : vif
            f[1 + GROUP_SLOT + i] = bright if (g == st.group or g in st.solo) else dim
    return bytes(f)


STEP_PLAYHEAD = 69          # blanc doux (teinte 16 de la palette NI) : pas en cours de lecture


def led_frame(st: State) -> bytes:
    """Pads dans la couleur du groupe : douce au repos, vive si frappe, joue ou choisi.
    Mode STEP : pas avec note = vif, pas vide = eteint, pas en lecture = blanc."""
    now = time.monotonic()
    dim, bright = hue_pair(st.group_hue[st.group])
    if st.settings and st.set_tab != 'COLORS':         # AUDIO / MIDI / KEYS : pads pour tester le son
        return pad_led_frame({p: bright if p in st.down else dim for p in range(1, 17)})
    if st.settings:
        if st.set_target == 'GROUP':                   # pad n = teinte n (vive)
            return pad_led_frame({p: hue_pair(p - 1)[1] for p in range(1, 17)})
        return pad_led_frame({p: pad_pair(st, p)[1 if p in st.set_selected else 0]
                              for p in range(1, 17)})
    if ({'mute', 'solo'} & st.held) and not (st.pattern_mode or st.scene_mode or st.arranger_mode
                                              or st.settings):
        # MUTE / SOLO tenu : coupe = eteint, en solo = vif, les autres = doux
        return pad_led_frame({p: 0 if (st.group, p) in st.sound_muted or not st.instrument(st.group, p)
                              else pad_pair(st, p)[1] if (st.group, p) in st.sound_solo
                              else pad_pair(st, p)[0] for p in range(1, 17)})
    if st.plugin_mode:                                 # PLUG-IN : sons choisis vifs, coupes eteints
        targets = plugin_targets(st)
        return pad_led_frame({p: pad_pair(st, p)[1] if (p in targets or p in st.down)
                              else 0 if not (st.sound_audible(st.group, p) and st.instrument(st.group, p))
                              else pad_pair(st, p)[0] for p in range(1, 17)})
    if st.arranger_mode:                               # ARRANGER : scenes pleines douces,
        colors = {}                                    # celle de l'entree choisie vive
        sel = st.arrangement[st.arr_sel]['scene'] if st.arrangement else None
        for p in range(1, 17):
            idx = st.scene_bank * 16 + p - 1
            colors[p] = bright if idx == sel else (dim if st.scenes[idx] else 0)
        return pad_led_frame(colors)
    if st.scene_mode:                                  # SCENE : vide eteint, pleine douce,
        colors = {}                                    # en cours vive (clignote en attente)
        blink = int(now * 4) % 2 == 0
        waiting = bool(st.pat_queue)
        for p in range(1, 17):
            idx = st.scene_bank * 16 + p - 1
            if idx == st.scene_current:
                colors[p] = (bright if blink else dim) if waiting else bright
            else:
                colors[p] = dim if st.scenes[idx] else 0
        return pad_led_frame(colors)
    if st.pattern_mode:                                # PATTERN : vide eteint, plein doux,
        colors = {}                                    # choisi vif, en attente clignote
        blink = int(now * 4) % 2 == 0
        for p in range(1, 17):
            idx = st.pat_bank * 16 + p - 1
            if st.pat_queue.get(st.group) == idx:
                colors[p] = bright if blink else dim
            elif idx == st.pat_index[st.group] and st.pat_queue.get(st.group) == OFF:
                colors[p] = bright if blink else 0     # va s'arreter
            elif idx == st.pat_index[st.group] and st.group not in st.pat_off:
                colors[p] = bright
            else:
                colors[p] = dim if st.patterns[st.group][idx] else 0
        return pad_led_frame(colors)
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
            if (not st.sound_audible(st.group, st.sound) or not st.instrument(st.group, st.melodic_sound())) \
                    and p not in st.down and not shift:
                colors[p] = 0                          # son joue coupe (MUTE / SOLO) : eteint
        return pad_led_frame(colors)
    if 'select' in st.held and not (st.pattern_mode or st.scene_mode or st.file_mode
                                    or st.arranger_mode or st.plugin_mode or st.settings):
        # SELECT tenu (Pad mode, Keyboard, Chords, STEP) : les 16 sons, le choisi en blanc
        return pad_led_frame({p: STEP_SOUND if p == st.sound else pad_pair(st, p)[0]   # en blanc
                              for p in range(1, 17)})
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
    return pad_led_frame({p: (pad_pair(st, p)[0] if p == st.sound else 0)     # son vide : eteint
                          if not st.instrument(st.group, p) and p not in st.down
                          else pad_pair(st, p)[1] if (p in st.down or st.flash.get(p, 0) > now
                                                   or p == st.sound or (st.group, p) in st.sound_solo)
                          else 0 if not (st.sound_audible(st.group, p) and st.instrument(st.group, p))
                          else pad_pair(st, p)[0]
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
    """Demarre le moteur audio (avec ton compte, meme si l'interface a ete lancee sous sudo)."""
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
    ap.add_argument('--exclusive', action='store_true',
                    help='ancien acces exclusif (sudo ; la carte son de la Maschine disparait)')
    ap.add_argument('--palette', action='store_true',
                    help='nuancier : les 16 pads montrent les 16 teintes de la Maschine')
    a = ap.parse_args()
    if a.vert:
        import mk3_screens
        mk3_screens.GREEN_GAMMA = mk3_screens.GREEN_GAMMAS[a.vert - 1]
    st = State()
    if project_folder(st.project_name).stem in list_projects():   # rouvrir le dernier projet
        try:
            load_project(st, project_folder(st.project_name).stem)
            print(f'Projet ouvert : {st.project_name}')
        except (OSError, ValueError, KeyError, TypeError) as e:
            print(f'Projet {st.project_name} illisible ({e}) : projet vide')

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

    st.ni_paused, st.exclusive = False, a.exclusive
    try:
        if a.exclusive:                                # ancien mode : sudo, detache tout
            screens = Screens(detach=True)
            dev = UsbHid(screens)
        else:
            dev = HidDev()
            st.device_info = (dev.product, dev.firmware)
            try:
                screens = Screens()
            except MK3Error:
                if not ni_agent_running():
                    raise
                print("Ecrans tenus par l'agent Native Instruments : mise en pause (relance en quittant).")
                st.ni_paused = ni_agent_pause()
                if st.ni_paused:
                    atexit.register(ni_agent_resume)   # meme en cas d'erreur
                screens = Screens()
    except MK3Error as e:
        print('Erreur :', e)
        print('Quitte les logiciels qui utilisent la Maschine (Maschine, Mixxx...) et reessaie.')
        if 'dev' in locals():
            dev.close()
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
            evs = dec.parse(data) if data else []
            for ev in dec.expire():                  # relachements perdus (jeu tres rapide)
                print(f'[maschine] pad {ev[1]} : relachement perdu, note arretee '
                      f'(plus grand ecart normal entre deux pressions : {dec.max_gap * 1000:.0f} ms)')
                evs.append(ev)
            for ev in evs:
                if ev[0] == 'pad_on':
                    # Lecture simple de l'etat, sans attendre le verrou (pris pendant le dessin)
                    notes = sound_for(st, ev[1])
                    old = st.held_play.get(ev[1])
                    if old and not st.arp:           # frappe sans relachement recu : on libere d'abord
                        release_notes(st, old[3], old[2])
                    # le groupe est garde : la fin de note ira au bon groupe meme s'il a change
                    st.held_play[ev[1]] = (time.monotonic(), ev[2], notes, st.group)
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
                            play_note(st, st.group, snd, v, note)   # moteur, ou MIDI OUT
                elif ev[0] == 'pad_off':
                    held = st.held_play.pop(ev[1], None)
                    if held and not st.arp:
                        release_notes(st, held[3], held[2])
                    if not dec.down and not st.held_play and not st.midi_held:
                        # plus aucun pad tenu : filet de securite, aucune note jouee en
                        # direct ne doit rester tenue (fins de note perdues quand on joue vite)
                        SOUND.send('/fuzzy/proto/off_live')
                events.put(ev)

    def player():
        while running.is_set():
            with lock:
                if tick(st):
                    events.put(('redraw',))
                repeat_tick(st)
            time.sleep(0.001)

    threads = [threading.Thread(target=reader, daemon=True), threading.Thread(target=player, daemon=True),
               threading.Thread(target=engine_listener, args=(st, events, running), daemon=True),
               threading.Thread(target=pc_keys_listener, args=(events, running), daemon=True)]
    for t in threads:
        t.start()

    dirty, last_draw, last_leds, last_buttons = True, 0.0, b'', b''
    last_imgs: list = [None, None]                    # dernieres images envoyees aux ecrans
    last_transport = None
    try:
        while not st.quit_request:
            try:
                ev = events.get(timeout=0.01)
                while True:                               # vider tout ce qui attend
                    with lock:
                        if ev[0] == 'engine' and ev[1] == 'first' and not st.engine_synced:
                            engine_sync(st, keep_output=bool(a.output))
                        if ev[0] != 'redraw':
                            handle(st, ev)
                    # l'etat du moteur (2 fois par seconde) ne redessine que s'il est affiche :
                    # chaque envoi aux ecrans fait passer beaucoup de donnees sur l'USB
                    if ev[0] != 'engine' or (st.settings and st.set_tab in ('STATUS', 'AUDIO', 'MIDI')):
                        dirty = True
                    ev = events.get_nowait()
            except queue.Empty:
                pass
            now = time.monotonic()
            # SETTINGS > AUDIO / MIDI : reglages envoyes au moteur apres un court repos
            if st.audio_pending and now - st.audio_pending > 1.0:
                st.audio_pending = 0.0
                send_audio(st)
                dirty = True
            if st.midi_pending and now - st.midi_pending > 0.3:
                st.midi_pending = 0.0
                send_midi(st)
            transport = (st.playing, st.bpm)               # horloge MIDI / Start-Stop envoyes
            if transport != last_transport:
                SOUND.send('/fuzzy/proto/transport', int(st.playing), float(st.bpm))
                last_transport = transport
            if st.toast[1] and now > st.toast[1]:        # fin d'un message : l'effacer de l'ecran
                st.toast = ('', 0.0)
                dirty = True
            if st.len_flash and now > st.len_flash:        # fin du grand cadre LENGTH
                st.len_flash = 0.0
                dirty = True
            if st.settings and st.set_tab in ('STATUS', 'AUDIO', 'MIDI') and now - last_draw > 0.5:
                dirty = True                               # etat qui change tout seul
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
                for k in (0, 1):                          # seulement les lignes qui ont change
                    box = (ImageChops.difference(images[k], last_imgs[k]).getbbox()
                           if last_imgs[k] is not None else (0, 0, W, H))
                    if box:
                        top, bottom = box[1], box[3]
                        screens.send(k, images[k].crop((0, top, W, bottom)), 0, top)
                last_imgs = list(images)
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
        if project_modified(st):                       # le morceau est enregistre en quittant
            if would_overwrite(st):                    # jamais par-dessus un autre projet
                st.project_name = unique_name(st.project_name)
            print('Projet enregistre :', save_project(st))
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
        if st.quit_request and not st.ni_paused and sys.platform == 'darwin' and not ni_agent_running():
            ni_agent_resume()                          # Shift + 8 : rendre la Maschine a NI
        print('\nArrete.' + ('' if st.ni_paused or not a.exclusive else
                               ' Debranche / rebranche la Maschine pour la rendre au pilote NI.'))
    return 0


if __name__ == '__main__':
    sys.exit(main())
