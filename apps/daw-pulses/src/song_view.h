#pragma once
#include "sequencer.h"

#include <QWidget>

#include <functional>

// Vue Song (morceau) : comme l'arrangeur de Maschine 3. Une regle de mesures, une ligne SONG
// avec un bloc par entree (scene x repetitions, ou longueur fixe), et une ligne par groupe
// qui montre le pattern joue dans chaque bloc. Meme modele que le mode ARRANGER du pilote.
// La vue ne modifie rien elle-meme : elle demande (signaux), la fenetre fait (et Cmd+Z).
class SongView : public QWidget
{
    Q_OBJECT
public:
    SongView (const seq::Sequencer& sequencer, const session::State& state, QWidget* parent = nullptr);
    int selected = 0;                                // entree choisie
    double step = 1;                                 // longueur au bord : 1, 1/2, 1/3 ou 1/4 de mesure
    bool follow = false;                             // la vue suit la tete de lecture
    void followPlayhead();                           // a chaque ligne jouee
    void zoom (int dir);                             // ZOOM H : mesures plus larges / plus etroites
    void zoomV (int dir);                            // ZOOM V : lignes des groupes plus hautes / plus basses
    std::function<bool()> isPlaying;

signals:
    void selectedChanged (int entry);
    void moveEntry (int from, int to);               // glisser un bloc
    void lengthChanged (int entry, double bars);     // tirer le bord droit
    void entryMenu (int entry, const QPoint& globalPos);
    void emptyMenu (const QPoint& globalPos);        // clic droit dans le vide : ajouter
    void chooseScene (int entry, const QPoint& globalPos);   // double-clic : changer la scene
    void startFrom (int entry);                      // clic sur la regle
    void insertScene (int scene, int before);        // scene lachee (glisser depuis Scenes)
    void cellClicked (int entry, int group, const QPoint& globalPos);   // case groupe x bloc : son pattern
    void loopRegion (int from, int to);              // zone de boucle glissee sur la regle (-1 : effacer)
    void rulerMenu (const QPoint& globalPos);        // clic droit sur la regle (effacer la zone)

protected:
    void paintEvent (QPaintEvent*) override;
    void mousePressEvent (QMouseEvent*) override;
    void mouseMoveEvent (QMouseEvent*) override;
    void mouseReleaseEvent (QMouseEvent*) override;
    void mouseDoubleClickEvent (QMouseEvent*) override;
    void contextMenuEvent (QContextMenuEvent*) override;
    void wheelEvent (QWheelEvent*) override;
    void leaveEvent (QEvent*) override;
    void dragEnterEvent (QDragEnterEvent*) override;
    void dragMoveEvent (QDragMoveEvent*) override;
    void dragLeaveEvent (QDragLeaveEvent*) override;
    void dropEvent (QDropEvent*) override;

private:
    static constexpr int kLabelW = 86, kRulerH = 24, kSongH = 40;
    int laneH = 24;                                  // hauteur des lignes de groupes (ZOOM V)
    int hoverLane = -1;                              // groupe sous la souris (surligne)
    enum class Edge { None, Left, Right } edge = Edge::None;   // bord de la zone de boucle tire
    double xOfRow (int row) const { return kLabelW + 4 + row / double (seq::kRowsPerBar) * barPx - scrollPx; }
    QRectF entryRect (int i) const;                  // le bloc de l'entree i (ligne SONG)
    int entryAt (const QPointF& p) const;            // -1 : aucune
    int insertIndexAt (double x) const;              // ou inserer / deplacer, d'apres x
    std::vector<int> startRows() const;              // debut de chaque entree (lignes), + la fin
    std::vector<int> lanes() const;                  // groupes montres (ceux qui jouent dans le morceau)

    const seq::Sequencer& sequencer;
    const session::State& state;
    double barPx = 64, scrollPx = 0;
    enum class Drag { None, Pending, Move, Resize, Ruler, Region } drag = Drag::None;
    int entryAtX (double x) const;                   // bloc sous cette position (regle)
    int regionA = -1, regionB = -1;                  // zone en cours de glisser
    QPointF pressPos;
    int pressEntry = -1, dropIndex = -1;
    double resizeBars = 0;
};
