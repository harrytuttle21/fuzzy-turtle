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
    int scale = 0;                                   // index dans music::scales() (0 = Chromatic)
    int chordMode = 0;                               // 0 aucun, 1 Harmonizer, 2 Chord Set
    int harmony = 2;                                 // index dans music::harmonies() (1-3-5)
    int chordSet = 0;                                // index dans music::chordSets()
};

bool inScale (int note, const KeySettings& k);
QList<int> chordFor (int note, const KeySettings& k);   // notes jouees pour une touche (accord compris)
QString chordName (QList<int> notes);                    // « Cm7 », sinon les notes
QString scaleLabel (int index);                          // « Major : Lydian »

} // namespace music

class KeyboardView : public QWidget
{
    Q_OBJECT
public:
    KeyboardView (const session::State& state, const music::KeySettings& keys, QWidget* parent = nullptr);
    void setActive (int note, bool on);              // note tenue (clavier, souris, Maschine)
    void flash (int note, int ms);                   // note jouee par le pattern
    void setKeyLabels (const QMap<int, QString>& labels) { keyLabels = labels; update(); }   // lettres de l'ordinateur (note -> lettre)
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
    static constexpr int kOctaves = 4, kTop = 46;
};
