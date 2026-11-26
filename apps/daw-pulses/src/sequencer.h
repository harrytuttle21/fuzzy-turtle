// Fuzzy John DAW Pulses — sequenceur : plusieurs patterns par groupe (64, 4 banques de 16) et
// des scenes (un pattern par groupe), comme la Maschine et le pilote.
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
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace seq {

constexpr int kMaxRows = 256, kTracks = 16, kTiming = 24, kChordExtra = 7;   // 256 lignes = 16 mesures
constexpr int kPatternSlots = 64, kSlotsPerBank = 16;   // patterns par groupe ; scenes
constexpr int kOff = -1;            // aucun pattern : le groupe se tait (comme OFF du pilote)
constexpr int kFree = -2;           // scene : ce groupe garde le pattern qu'il joue (« libre »)
constexpr int kNoQueue = -3;        // pas de changement de pattern en attente

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

// Une scene : le pattern de chaque groupe (kOff = silence, kFree = ne change pas)
struct Scene
{
    bool used = false;
    std::string name;
    std::array<int, session::kGroups> pattern {};
    Scene() { pattern.fill (kFree); }
};

// Tous les patterns d'un groupe (copier / deplacer / exporter un groupe, annuler)
struct GroupPatterns
{
    std::vector<std::pair<int, Pattern>> patterns;   // emplacement -> pattern
    std::vector<std::pair<int, std::string>> names;
    int current = 0;
    bool off = true;
};

// Une note a jouer (lue par l'horloge)
struct Hit { int group, track, note, velocity, gateMs; double delayRows; };

class Sequencer
{
public:
    // Lecture : notes de la ligne `row` (heure prevue) et notes en avance de la ligne suivante.
    // delayRows = decalage par rapport a l'heure de la ligne (0 a 1).
    // Un pattern demande pendant la lecture commence a la fin de celui qui joue.
    std::vector<Hit> hitsForRow (int row, double rowMs);

    // Enregistrement : `rowPos` = position de lecture en lignes (avec la fraction).
    // Renvoie l'adresse de la case ecrite (pour lui donner sa duree au relachement).
    struct Written { int group = -1, row = -1, track = -1; };
    Written record (int group, int track, int note, int velocity, double rowPos, bool merge = false);   // merge : ajouter a l'accord
    void setLength (const Written& w, double rows);       // duree, au relachement

    Sequencer();
    // Les fonctions ci-dessous (pattern, setCell...) agissent sur le pattern CHOISI du groupe.
    Pattern pattern (int group) const;                    // copie, pour l'affichage
    void setPattern (int group, const Pattern& p);        // remet un pattern entier (annuler)
    void setCell (int group, int row, int track, const Cell& c);
    void setPatternLength (int group, int length);
    void clear (int group);
    int patternLength (int group) const;
    void setGroupOff (int group, bool off);               // groupe coupe (« off » du prototype)
    bool groupOff (int group) const;

    // --- Patterns d'un groupe (emplacements 0-63)
    int currentPattern (int group) const;
    bool hasPattern (int group, int index) const;
    Pattern patternAt (int group, int index) const;       // vide s'il n'existe pas
    void setPatternAt (int group, int index, const Pattern& p);   // cree ou remplace
    void removePattern (int group, int index);           // le pattern choisi devient vide
    std::string patternName (int group, int index) const;
    void setPatternName (int group, int index, const std::string& name);
    // Choisir un pattern (ou kOff) : tout de suite a l'arret, a la fin du pattern en lecture
    void selectPattern (int group, int index, bool playing);
    int queuedPattern (int group) const;                 // kNoQueue s'il n'y a rien en attente
    int localRow (int group, int row) const;             // ligne dans le pattern du groupe
    void resetPlayback();                                // retour au debut (STOP)
    void reset();                                        // projet vide : un pattern 1 vide par groupe, groupes eteints, aucune scene

    GroupPatterns groupPatterns (int group) const;
    void setGroupPatterns (int group, const GroupPatterns& gp);   // remplace tous les patterns du groupe
    void clearGroupPatterns (int group, int length = 16);         // un pattern 1 vide, groupe eteint

    // --- Scenes (0-63)
    Scene scene (int index) const;
    void setScene (int index, const Scene& s);
    std::array<int, session::kGroups> currentCombo() const;   // ce qui joue (ou jouera) par groupe
    void launchScene (int index, bool playing);
    int currentScene() const;
    void setCurrentScene (int index);                    // -1 = aucune

private:
    Pattern& cur (int group) { return *store[(size_t) group][(size_t) current[(size_t) group]]; }
    const Pattern& cur (int group) const { return *store[(size_t) group][(size_t) current[(size_t) group]]; }
    void choose (int group, int index, bool playing);    // sans verrou

    mutable std::mutex lock;
    std::array<std::array<std::unique_ptr<Pattern>, kPatternSlots>, session::kGroups> store;
    std::array<std::array<std::string, kPatternSlots>, session::kGroups> names;
    std::array<int, session::kGroups> current {};        // pattern choisi
    std::array<int, session::kGroups> queue {};          // pattern suivant (fin du pattern)
    std::array<int, session::kGroups> start {};          // ligne ou le pattern en cours a commence
    std::array<bool, session::kGroups> off {};
    std::array<Scene, kPatternSlots> scenes;
    int sceneNow = -1;
};

} // namespace seq
