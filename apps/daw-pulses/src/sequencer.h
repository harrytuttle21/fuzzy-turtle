// Fuzzy John DAW Pulses — sequenceur : un pattern par groupe (premiere etape).
//
// Un pattern = 1 a 64 lignes (4 lignes par temps) x 16 pistes (une par son du groupe).
// Une case : note, velocite, duree en lignes, et decalage fin (1/24 de ligne, -12 a +12),
// comme dans le prototype (cellule [note, vel, duree, decalage] du format .fuzzy).
// Les donnees sont partagees entre l'interface, la tache des pads (enregistrement) et la
// tache de l'horloge (lecture) : un verrou court les protege. Licence : GPLv3.
#pragma once
#include "session.h"

#include <array>
#include <chrono>
#include <mutex>
#include <vector>

namespace seq {

constexpr int kMaxRows = 64, kTracks = 16, kTiming = 24, kChordExtra = 7;

struct Cell
{
    int note = -1;          // -1 = vide
    int velocity = 100;
    int length = 1;         // duree en lignes
    int offset = 0;         // decalage fin, en 1/24 de ligne
    // Accord : notes en plus de `note` (-1 = aucune), comme [n1, n2...] dans le .fuzzy
    std::array<int, kChordExtra> extra { -1, -1, -1, -1, -1, -1, -1 };
    bool empty() const { return note < 0; }
    int chordSize() const { int n = empty() ? 0 : 1; for (int e : extra) n += e >= 0; return n; }
};

struct Pattern
{
    int length = 16;
    std::array<std::array<Cell, kTracks>, kMaxRows> rows {};
};

// Une note a jouer (lue par l'horloge)
struct Hit { int group, track, note, velocity, gateMs; double delayRows; };

class Sequencer
{
public:
    // Lecture : notes de la ligne `row` (heure prevue) et notes en avance de la ligne suivante.
    // delayRows = decalage par rapport a l'heure de la ligne (0 a 1).
    std::vector<Hit> hitsForRow (int row, double rowMs) const;

    // Enregistrement : `rowPos` = position de lecture en lignes (avec la fraction).
    // Renvoie l'adresse de la case ecrite (pour lui donner sa duree au relachement).
    struct Written { int group = -1, row = -1, track = -1; };
    Written record (int group, int track, int note, int velocity, double rowPos);
    void setLength (const Written& w, double rows);       // duree, au relachement

    Pattern pattern (int group) const;                    // copie, pour l'affichage
    void setPattern (int group, const Pattern& p);        // remet un pattern entier (annuler)
    void setCell (int group, int row, int track, const Cell& c);
    void setPatternLength (int group, int length);
    void clear (int group);
    int patternLength (int group) const;
    void setGroupOff (int group, bool off);               // groupe coupe (« off » du prototype)
    bool groupOff (int group) const;

private:
    mutable std::mutex lock;
    std::array<Pattern, session::kGroups> patterns {};
    std::array<bool, session::kGroups> off {};
};

} // namespace seq
