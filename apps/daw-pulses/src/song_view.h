#pragma once
#include "sequencer.h"

#include <QList>
#include <QSet>
#include <QWidget>

#include <functional>

// Vue Song (morceau) : comme l'arrangeur de Maschine 3. En haut un apercu de tout le morceau
// (cadre = partie visible), la regle des mesures (et le temps), une ligne SONG avec un bloc
// par entree (scene x repetitions, ou longueur fixe), et une ligne par groupe qui montre le
// pattern joue dans chaque bloc (M / S a gauche). Meme modele que le mode ARRANGER du pilote.
// La vue ne modifie rien elle-meme : elle demande (signaux), la fenetre fait (et Cmd+Z).
class SongView : public QWidget
{
    Q_OBJECT
public:
    SongView (const seq::Sequencer& sequencer, const session::State& state, QWidget* parent = nullptr);
    int selected = 0;                                // entree choisie (la derniere cliquee)
    QSet<int> chosen;                                // blocs choisis (Shift / Cmd + clic)
    QList<int> chosenList() const;                   // dans l'ordre (au moins le bloc choisi)
    void selectOnly (int entry);
    double step = 1;                                 // longueur au bord : 1, 1/2, 1/3 ou 1/4 de mesure
    bool follow = false;                             // la vue suit la tete de lecture
    void followPlayhead();                           // a chaque ligne jouee
    void zoom (int dir);                             // ZOOM H : mesures plus larges / plus etroites
    void zoomV (int dir);                            // ZOOM V : lignes des groupes plus hautes / plus basses
    void setBarWidth (int px) { barPx = std::clamp (px, 16, 320); update(); }   // curseurs Zoom H / V
    void setLaneHeight (int px) { laneH = std::clamp (px, 16, 64); update(); }
    int barWidth() const { return (int) barPx; }
    int laneHeight() const { return laneH; }
    std::function<bool()> isPlaying;
    std::function<double()> bpm;                     // pour le temps (min:s)

signals:
    void selectedChanged (int entry);
    void moveEntries (const QList<int>& entries, int before, bool copy);   // glisser des blocs (Option = copie)
    void lengthChanged (int entry, double bars);     // tirer le bord droit
    void entryMenu (int entry, const QPoint& globalPos);
    void emptyMenu (const QPoint& globalPos);        // clic droit dans le vide : ajouter
    void chooseScene (int entry, const QPoint& globalPos);   // double-clic : changer la scene
    void startFrom (int entry);                      // clic sur la regle
    void insertScene (int scene, int before);        // scene lachee (liste des scenes, vue Scenes)
    void cellClicked (int entry, int group, const QPoint& globalPos);   // case groupe x bloc : son pattern
    void loopRegion (int from, int to);              // zone de boucle glissee sur la regle (-1 : effacer)
    void rulerMenu (const QPoint& globalPos);        // clic droit sur la regle (effacer la zone)
    void laneMuteSolo (int group, bool solo);        // M / S d'une ligne de groupe
    void zoomChanged();                              // Cmd + molette : les curseurs suivent

protected:
    bool event (QEvent*) override;                   // infobulle d'un bloc
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
    static constexpr int kLabelW = 112, kTop = 18, kRulerH = 18 + 24, kSongH = 40;   // kRulerH : apercu + regle
    int laneH = 24;                                  // hauteur des lignes de groupes (ZOOM V)
    int hoverLane = -1;                              // groupe sous la souris (surligne)
    enum class Edge { None, Left, Right } edge = Edge::None;   // bord de la zone de boucle tire
    double xOfRow (int row) const { return kLabelW + 4 + row / double (seq::kRowsPerBar) * barPx - scrollPx; }
    QRectF entryRect (int i) const;                  // le bloc de l'entree i (ligne SONG)
    int entryAt (const QPointF& p) const;            // -1 : aucune
    int insertIndexAt (double x) const;              // ou inserer / deplacer, d'apres x
    std::vector<int> startRows() const;              // debut de chaque entree (lignes), + la fin
    std::vector<int> lanes() const;                  // groupes montres
    int entryAtX (double x) const;                   // bloc sous cette position (regle)
    QRectF muteBox (int lane, bool solo) const;      // M / S a gauche d'une ligne de groupe
    QRectF overview() const { return QRectF (kLabelW + 4, 2, width() - kLabelW - 8, kTop - 4); }
    void scrollFromOverview (double x);              // clic / glisser dans l'apercu
    QString timeOf (int rows) const;                 // lignes -> « m:ss »

    const seq::Sequencer& sequencer;
    const session::State& state;
    double barPx = 64, scrollPx = 0;
    enum class Drag { None, Pending, Move, Resize, Ruler, Region, Overview } drag = Drag::None;
    int regionA = -1, regionB = -1;                  // zone en cours de glisser
    QPointF pressPos;
    int pressEntry = -1, dropIndex = -1;
    double resizeBars = 0;
    bool copyDrag = false;                           // Option tenu pendant le glisser
};
