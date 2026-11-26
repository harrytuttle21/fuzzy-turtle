// Fuzzy John DAW Pulses — onglet Keyboard : grand clavier visuel comme l'ecran droit du pilote
// (notes jouees allumees, gamme en couleur, nom de l'accord), gammes et accords (Harmonizer,
// Chord Sets) identiques au pilote. Licence : GPLv3.
#pragma once
#include "session.h"

#include <QHash>
#include <QMap>
#include <QList>
#include <QTimer>
#include <QWidget>

namespace music {

// Reglages du clavier (gamme et accords), partages par la fenetre et la vue
struct KeySettings
{
    int root = 0;                                    // tonique : 0 = C ... 11 = B
    int scale = 0;                                   // >= 0 : music::scales() (0 = Chromatic) ; < 0 : personnalisee (-1 = la 1re)
    int chordMode = 0;                               // 0 aucun, 1 Harmonizer, 2 Chord Set, 3 accord personnalise, 4 User
    int harmony = 2;                                 // index dans music::harmonies() (1-3-5)
    int chordSet = 0;                                // index dans music::chordSets()
    int customChord = 0;                             // index dans customChords()
    int userBank = 0;                                // User 1-10 (accords perso, comme le pilote)
};

// Gammes et accords personnalises (enregistres dans les reglages de l'app)
struct Custom { QString name; std::vector<int> notes; };   // gamme : degres 0-11 ; accord : intervalles
std::vector<Custom>& customScales();
std::vector<Custom>& customChords();
void loadCustom();
void saveCustom();
// Accords USER : 10 banques de 12 accords (intervalles depuis la tonique de l'octave), le meme
// fichier que le pilote Maschine (user_chords.json) : ce qui est cree d'un cote se retrouve de l'autre
constexpr int kUserBanks = 10;
std::vector<std::vector<std::vector<int>>>& userChords();
QString userChordsFile();                        // le fichier utilise (a cote du pilote, sinon les reglages)
void loadUserChords();
bool saveUserChords();
int userCount (int bank);                        // accords remplis dans une banque
const std::vector<int>& scaleSteps (int scale);        // gamme fournie (>= 0) ou personnalisee (< 0)
int customScaleId (int index);                          // index personnalise -> valeur de « scale »

bool inScale (int note, const KeySettings& k);
QList<int> chordFor (int note, const KeySettings& k);   // notes jouees pour une touche (accord compris)
QString chordName (QList<int> notes);                    // « Cm7 », sinon les notes
QString scaleLabel (int index);                          // « Major : Lydian », « Custom : Ma gamme »
QString chordLabel (const KeySettings& k);              // « Harmonizer 1-3-5 », « Custom : Jazz 9 »...

} // namespace music

class KeyboardView : public QWidget
{
    Q_OBJECT
public:
    KeyboardView (const session::State& state, const music::KeySettings& keys, QWidget* parent = nullptr);
    void setActive (int note, bool on);              // note tenue (clavier, souris, Maschine)
    void flash (int note, int ms);                   // note jouee par le pattern
    void setKeyLabels (const QMap<int, QString>& labels) { keyLabels = labels; update(); }   // lettres de l'ordinateur (note -> lettre)
    void setColorScale (bool on) { colorScale = on; update(); }   // gamme en couleur, ou touches classiques
    QSize sizeHint() const override { return { 900, 260 }; }
    QSize minimumSizeHint() const override { return { 420, 140 }; }

signals:
    void keyPressed (int note);
    void keyReleased (int note);

protected:
    void paintEvent (QPaintEvent*) override;
    void mousePressEvent (QMouseEvent*) override;
    void mouseReleaseEvent (QMouseEvent*) override;

private:
    int firstNote() const;                           // do grave affiche (suit l'octave)
    QRectF keyRect (int note) const;
    int noteAt (const QPointF& p) const;
    QList<int> activeNotes() const;

    const session::State& state;
    const music::KeySettings& keys;
    QHash<int, int> held;                            // note -> nombre de raisons de l'allumer
    QHash<int, qint64> flashes;                      // note -> fin d'allumage (ms)
    QTimer tick;
    int mouseNote = -1;
    QMap<int, QString> keyLabels;
    bool colorScale = true;
    static constexpr int kOctaves = 4, kTop = 46;
};
