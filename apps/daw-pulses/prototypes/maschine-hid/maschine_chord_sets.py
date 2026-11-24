"""Community-transcribed Maschine Chord Set chord *symbols* (16 x 12).

Source: forum Native Instruments, user Blindeddie, "KK-Maschine Chord Sets.pdf".
https://community.native-instruments.com/discussion/1307/where-to-find-a-list-of-chords-used-in-chord-set-mode
The table documents chord names in tonic C, NOT MIDI voicings or octave registers.
This original lightweight parser supplies a musically conventional interpretation,
not a claim of bit-identical Native Instruments voicings.
No I/O, hardware, MIDI emission, or Zynthian dependencies.
"""
from __future__ import annotations
from dataclasses import dataclass
import re

# Eight MAJ columns (pads 1..12 top-to-bottom), then eight MIN columns.
# The notes preserve the sheet's enharmonic spelling at C (Bb, Ab, C#, D#).
_REFERENCE_ROWS = {
    'MAJ': (
        ('C','C','C','C','C','C','C','CMaj9'),
        ('Em','C/B','Am','G','D','G','G','C#dim'),
        ('F','C/A','FMaj7','Am','F','Dm','Am','Dm9'),
        ('G','C/G','Gsus4','F6','C','Am','Em','D#dim7'),
        ('Am','Fadd9','Amadd9','F','Ab','F','F','Em9'),
        ('Esus4','Fm','F6','Am/E','Eb','G/F','C/E','C9#5'),
        ('Gadd9','Csus4','Csus2','Dm7','Bb','C/E','Dm','FMaj7add13'),
        ('Dm','Am7','G','Am/C','F','Am7','Dm/C','Bb9'),
        ('Fadd9','Em','Dsus2','Gadd9','G7sus4','Dm7','G7/B','Em7'),
        ('F6','F','Bbadd9','Am7','Bbadd9','Em7','G','A9'),
        ('Gsus4','Gsus4','Gsus4','G/B','F6','C/F','F/A','Dm11'),
        ('G','G6','Fadd9','Dm/G','Cadd9','Gsus4','G7/B','G7(b9b13)'),
    ),
    'MIN': (
        ('Cm','Cm','Cm','Cm','Cm','Cm','Cm9','Cm6/9'),
        ('Cm/Eb','G+/B','Ab','Eb','Bb','Fm','Ab9','Dm7b5'),
        ('Fm','Cm/Bb','Eb','Bb','Ab6','Bb','Cm11','Cm11/G'),
        ('G','Cm/A','Bb','F','Bbadd9','Eb','C7(#9,b13)','Cm9'),
        ('AbMaj7','AbMaj7','F','Ab','Cm','Ab','Fm9','Fm9'),
        ('Eb','EbMaj7','Fm','AbMaj7','Dm7','Dm7b5','EbMaj7/F','Abm7'),
        ('Gm','Fm','Cm/G','Abm7','Cm/Eb','Gaddb9','C11','Ebmi7'),
        ('Bb','Bb7','Gsus4','EbMaj7','Fm','G','Am11','Bbm7b5'),
        ('F','Cm','Cm','Dsus4','G','Ab/G','Ab7#11','am11'),
        ('Fm/Ab','Bbadd9','Cm#5','D','AbMaj7','G7','G7#9','AbMaj7#5'),
        ('Cm/G','Abadd9','Cm6','Fm/G','B6/9','Cm/G','Cmadd9','G7(b9,b13)'),
        ('G','G7sus4','Cm7','G+','Csus4','G7b9','G7(b9,b13)','Cm9Maj7'),
    ),
}

# Common chord qualities, included compound extensions and alterations.
# Interval integers above 12 denote extensions in the upper octave.
QUALITY_INTERVALS = {
    '': (0,4,7),
    'm': (0,3,7), 'mi': (0,3,7),
    '6': (0,4,7,9), 'm6': (0,3,7,9), 'm6/9': (0,3,7,9,14), '6/9': (0,4,7,9,14),
    '7': (0,4,7,10), 'Maj7': (0,4,7,11), 'm7': (0,3,7,10), 'mi7': (0,3,7,10),
    '9': (0,4,7,10,14), 'Maj9': (0,4,7,11,14), 'm9': (0,3,7,10,14),
    '11': (0,4,7,10,14,17), 'm11': (0,3,7,10,14,17),
    'sus2': (0,2,7), 'sus4': (0,5,7), '7sus4': (0,5,7,10),
    'add9': (0,4,7,14), 'madd9': (0,3,7,14),
    'addb9': (0,4,7,13),
    'dim': (0,3,6), 'dim7': (0,3,6,9), 'm7b5': (0,3,6,10),
    '+': (0,4,8), 'm#5': (0,3,8), '9#5': (0,4,8,10,14),
    'Maj7#5': (0,4,8,11), 'Maj7add13': (0,4,7,11,21),
    '7b9': (0,4,7,10,13), '7#9': (0,4,7,10,15), '7#11': (0,4,7,10,18),
    '7(#9,b13)': (0,4,7,10,15,20),
    '7(b9,b13)': (0,4,7,10,13,20),
    '7(b9b13)': (0,4,7,10,13,20),
    'm9Maj7': (0,3,7,11,14),
}
_PITCHES = {'C':0,'D':2,'E':4,'F':5,'G':7,'A':9,'B':11}
_SHARPS = ('C','C#','D','D#','E','F','F#','G','G#','A','A#','B')
_FLATS = ('C','Db','D','Eb','E','F','Gb','G','Ab','A','Bb','B')
_ROOT_RE = re.compile(r'^([A-Ga-g])([#b]?)(.*)$')
_SLASH_RE = re.compile(r'/([A-Ga-g][#b]?)$')


def pitch_class(note):
    """A pitch class from a chord letter + optional accidental."""
    note = note[0].upper()+note[1:]
    if note[0] not in _PITCHES:
        raise ValueError(f'Bad pitch {note}')
    return (_PITCHES[note[0]] + (1 if note.endswith('#') else -1 if note.endswith('b') else 0)) % 12


def note_spelling(pc, original, semitones):
    """Retain source spelling in C. Else prefer source's sharp/flat style."""
    if semitones % 12 == 0:
        return original[0].upper()+original[1:]
    return (_SHARPS if '#' in original else _FLATS)[pc % 12]


def visible_suffix(suffix):
    """Concise MK3 naming: C major, Cmi minor, Bb ma7, Bb add9."""
    if suffix in ('m','mi'):
        return 'mi'
    if suffix.startswith('mi'):
        return suffix
    if suffix == 'm9Maj7':
        return 'mi9 ma7'
    if suffix.startswith('m') and suffix not in ('Maj7','Maj9','Maj7#5','Maj7add13'):
        return 'mi'+suffix[1:]
    if suffix.startswith('Maj'):
        return ' ma' + suffix[3:].replace('add13',' add13')
    if suffix.startswith('add'):
        return ' '+suffix
    if suffix == '7sus4':
        return '7 sus4'
    return suffix


@dataclass(frozen=True)
class ChordSymbol:
    source: str
    root_name: str
    root_pc: int
    quality: str
    intervals: tuple[int, ...]
    bass_name: str | None
    bass_pc: int | None

    def display(self, shift=0):
        root = note_spelling(self.root_pc+shift, self.root_name, shift)
        name = root + visible_suffix(self.quality)
        if self.bass_name is not None:
            bass = note_spelling(self.bass_pc+shift, self.bass_name, shift)
            name += '/' + bass
        return name

    def midi_notes(self, tonic=60):
        """Deterministic chord interpretation; not NI's unreleased MIDI voicing.

        A chord-tone slash bass selects an inversion if feasible (Cmi/Eb =>
        Eb4 G4 C5), while a nonchord-tone slash bass adds a lower voice
        (Cmi/A => A3 C4 Eb4 G4). Keep exact notes for note-off matching.
        """
        root = tonic + self.root_pc
        pitches = sorted(root + i for i in self.intervals)
        if self.bass_pc is not None:
            bass_pc = (self.bass_pc + tonic) % 12
            # If requested bass already occurs in the chord, invert upward
            # rather than adding a duplicate voice.
            for _ in range(len(pitches)):
                if pitches and pitches[0] % 12 == bass_pc:
                    break
                pitches.append(pitches.pop(0) + 12)
                pitches.sort()
            else:
                # Nonchord bass placed below the sounding root/tonic.
                bass = root - 1
                while bass % 12 != bass_pc:
                    bass -= 1
                pitches = [bass, *sorted(root+i for i in self.intervals)]
        return tuple(n for n in pitches if 0 <= n <= 127)


def parse_chord_symbol(symbol):
    """Strictly parse chart names; never silently substitute a triad."""
    match = _ROOT_RE.match(symbol)
    if match is None:
        raise ValueError(f'Invalid chord symbol: {symbol!r}')
    letter, accidental, rest = match.groups()
    tonic = letter.upper()+accidental
    bass_match = _SLASH_RE.search(rest)
    bass = bass_match.group(1) if bass_match else None
    suffix = rest[:bass_match.start()] if bass_match else rest
    # PDF contains one entry spelled Ebmi7 and one lower-case am11.
    if suffix not in QUALITY_INTERVALS:
        raise ValueError(f'Unknown chord quality in {symbol!r}: {suffix!r}')
    return ChordSymbol(symbol,tonic,pitch_class(tonic),suffix,QUALITY_INTERVALS[suffix],
                       bass[0].upper()+bass[1:] if bass else None,
                       pitch_class(bass) if bass else None)


def _transpose_rows(rows):
    assert len(rows)==12 and all(len(r)==8 for r in rows)
    return tuple(tuple(parse_chord_symbol(rows[pad][bank]) for pad in range(12))
                 for bank in range(8))

MACHINE_CHORD_SET_SOURCES = {family:_transpose_rows(rows)
                             for family, rows in _REFERENCE_ROWS.items()}
assert len(MACHINE_CHORD_SET_SOURCES['MAJ']) == len(MACHINE_CHORD_SET_SOURCES['MIN']) == 8
assert all(len(bank)==12 for banks in MACHINE_CHORD_SET_SOURCES.values() for bank in banks)
