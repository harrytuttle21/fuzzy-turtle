// Fuzzy John DAW Pulses — etat partage de la session (groupe, son, octave...).
// Les valeurs lues par la tache des pads sont atomiques : pas de verrou sur le chemin du son.
// Licence : GPLv3.
#pragma once
#include <QColor>
#include <QString>
#include <QUrl>
#include <QList>

#include <algorithm>
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
// Le premier fichier MIDI (.mid, .midi) parmi des URL glissees (vide sinon)
inline QString midiFileIn (const QList<QUrl>& urls)
{
    for (const QUrl& u : urls)
        if (u.isLocalFile() && (u.toLocalFile().endsWith (".mid", Qt::CaseInsensitive)
                                || u.toLocalFile().endsWith (".midi", Qt::CaseInsensitive)))
            return u.toLocalFile();
    return {};
}

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
    std::array<std::atomic<int>, kGroups> groupVolume;   // mixeur, 0-127 (100 = normal)
    std::atomic<int> masterVolume { 100 };

    // Mute / solo, comme le prototype : un groupe joue s'il n'est pas coupe et, si un solo
    // existe, s'il en fait partie ; un son pareil, mais le solo d'un son vaut dans son groupe.
    std::array<std::atomic<bool>, kGroups> groupMute {}, groupSolo {};
    std::array<std::atomic<bool>, kGroups * 16> soundMute {}, soundSolo {};   // groupe * 16 + son - 1

    bool groupAudible (int g) const
    {
        if (groupMute[(size_t) g])
            return false;
        for (const auto& s : groupSolo)
            if (s)
                return groupSolo[(size_t) g];
        return true;
    }
    bool soundAudible (int g, int sound) const       // son 1-16
    {
        const size_t i = (size_t) (g * 16 + sound - 1);
        if (soundMute[i])
            return false;
        for (int s = 0; s < 16; ++s)
            if (soundSolo[(size_t) (g * 16 + s)])
                return soundSolo[i];
        return true;
    }
    bool audible (int g, int sound) const { return groupAudible (g) && soundAudible (g, sound); }
    // 'M' coupe, 'S' solo, 'm' muet a cause d'un solo ailleurs, 0 sinon
    char soundTag (int g, int sound) const
    {
        const size_t i = (size_t) (g * 16 + sound - 1);
        return soundMute[i] ? 'M' : soundSolo[i] ? 'S' : soundAudible (g, sound) ? 0 : 'm';
    }

    // Sample de chaque son (chemin du fichier ; vide = son de synthese). Interface seulement.
    std::array<QString, kGroups * 16> samples;

    // Comment joue chaque son : One-shot (va au bout), Gate (s'arrete au relachement),
    // Loop (boucle de loopStart a loopEnd, fractions du fichier, tant que la note tient)
    enum PlayMode { OneShot = 0, Gate = 1, Loop = 2 };
    std::array<std::atomic<int>, kGroups * 16> playMode {};
    std::array<std::atomic<float>, kGroups * 16> loopStart {}, loopEnd {};
    std::array<std::atomic<int>, kGroups * 16> releaseMs {};

    // Synthe de chaque son, comme le pilote : -1 = celui du pad (A-H : batterie 1-4, Tone 5-16 ;
    // A2-H2 : aucun), 0 = aucun (« None » : son vide), 1-5 = Kick, Snare, Closed HH, Open HH, Tone
    std::array<std::atomic<int>, kGroups * 16> synth {};
    // Le son a un instrument (sample ou synthe) ? Sinon il ne joue pas et son pad est eteint.
    // Tenu a jour par l'interface (lu aussi par la lecture du pattern).
    std::array<std::atomic<bool>, kGroups * 16> hasInstrument {};
    static int defaultSynth (int sound) { return sound <= 4 ? sound : 5; }
    int effSynth (int g, int sound) const
    {
        const int v = synth[(size_t) index (g, sound)];
        return v >= 0 ? v : g >= 8 ? 0 : defaultSynth (sound);
    }

    static int index (int g, int sound) { return g * 16 + sound - 1; }
    int mode (int g, int sound) const { return playMode[(size_t) index (g, sound)]; }
    bool held (int g, int sound) const { return mode (g, sound) != OneShot; }   // note tenue (ADSR)
    static int defaultMode (int sound) { return sound <= 4 ? OneShot : Gate; }  // batterie / notes

    void resetSounds()                               // nouveau projet : sons par defaut
    {
        for (int i = 0; i < kGroups * 16; ++i)
        {
            samples[(size_t) i].clear();
            playMode[(size_t) i] = defaultMode (i % 16 + 1);
            loopStart[(size_t) i] = 0.0f;
            loopEnd[(size_t) i] = 1.0f;
            releaseMs[(size_t) i] = 150;
            synth[(size_t) i] = -1;
            hasInstrument[(size_t) i] = i < 8 * 16;   // A-H : synthes du pad ; A2-H2 : vides
        }
    }

    State()
    {
        for (auto& v : groupVolume) v = 100;
        for (int i = 0; i < kGroups * 16; ++i)
        {
            playMode[(size_t) i] = defaultMode (i % 16 + 1);
            loopEnd[(size_t) i] = 1.0f;
            releaseMs[(size_t) i] = 150;
            synth[(size_t) i] = -1;
            hasInstrument[(size_t) i] = i < 8 * 16;
        }
    }

    // Velocite envoyee au moteur : velocite jouee x volume du groupe x master (0 = muet)
    int outVelocity (int g, int velocity) const
    {
        const double v = velocity * groupVolume[(size_t) g].load() / 100.0 * masterVolume.load() / 100.0;
        return v < 0.5 ? 0 : std::min (127, (int) (v + 0.5));
    }
};

} // namespace session
