// Fuzzy John DAW Pulses — etat partage de la session (groupe, son, octave...).
// Les valeurs lues par la tache des pads sont atomiques : pas de verrou sur le chemin du son.
// Licence : GPLv3.
#pragma once
#include <QColor>
#include <QString>

#include <array>
#include <atomic>

namespace session {

constexpr int kGroups = 16;           // A-H et A2-H2
constexpr int kSounds = 16;

// Couleurs des groupes A-H (comme les boutons de la Maschine+) ; A2-H2 = ordre inverse.
constexpr std::array<int, 8> kGroupHues { 1, 2, 5, 6, 9, 12, 13, 15 };     // teintes NI
inline const std::array<QColor, 8> kGroupColors {
    QColor (235, 35, 25), QColor (252, 95, 0), QColor (235, 200, 40), QColor (0, 200, 165),
    QColor (25, 90, 240), QColor (105, 55, 240), QColor (230, 40, 225), QColor (240, 20, 70) };
inline const QColor kAccent (252, 95, 0);                                    // orange Fuzzy

inline QString groupName (int g) { return QString (QChar ('A' + g % 8)) + (g >= 8 ? "2" : ""); }
inline int groupHue (int g) { return g < 8 ? kGroupHues[(size_t) g] : kGroupHues[(size_t) (15 - g)]; }
inline QColor groupColor (int g) { return g < 8 ? kGroupColors[(size_t) g] : kGroupColors[(size_t) (15 - g)]; }

// Hauteur par defaut d'un son : 1-4 batterie (C4), 5-16 C mineur pentatonique des C3
// (comme le moteur et le prototype).
inline int defaultNote (int sound)
{
    if (sound <= 4)
        return 60;
    static constexpr int scale[5] { 0, 3, 5, 7, 10 };
    const int deg = sound - 5;
    return 48 + scale[deg % 5] + 12 * (deg / 5);
}

inline QString noteName (int note)
{
    static const char* names[12] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return QString ("%1%2").arg (names[note % 12]).arg (note / 12 - 1);
}

inline QString soundName (int sound)
{
    static const char* drums[4] { "Kick", "Snare", "Closed HH", "Open HH" };
    return sound <= 4 ? QString (drums[sound - 1]) : "Note " + noteName (defaultNote (sound));
}

// Sons 1-4 : ONE-SHOT (le son joue jusqu'au bout) ; 5-16 : ADSR (la note tient).
inline bool adsr (int sound) { return sound > 4; }

struct State
{
    std::atomic<int> group { 0 };     // 0-15
    std::atomic<int> sound { 5 };     // son joue au clavier (1-16)
    std::atomic<int> octave { 3 };    // clavier : C3 en bas
    std::atomic<int> velocity { 100 };
};

} // namespace session
