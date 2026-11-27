#pragma once
#include "sequencer.h"

#include <QWidget>

#include <functional>
#include <vector>

// Vue SCENE GRID (bouton Grid de l'onglet Scenes), facon Maschine :
//  - en haut : les scenes S1, S2... (clic = la choisir ; avec Play, elle se lance) ;
//  - au milieu : une colonne par groupe (8 a la fois, la banque du groupe choisi) avec tous ses
//    patterns ; ceux de la scene choisie en couleur, les autres grises (clic = entre / sort) ;
//  - en bas : les groupes et leur nom (clic = choisir le groupe).
// Licence : GPLv3.
class SceneGridView : public QWidget
{
    Q_OBJECT
public:
    SceneGridView (seq::Sequencer& sequencer, const session::State& state, QWidget* parent = nullptr);
    void setScene (int index);                       // la scene choisie (celle que l'on modifie)
    int scene() const { return chosen; }
    QSize minimumSizeHint() const override { return { 400, 200 }; }

signals:
    void sceneClicked (int scene);                   // clic sur une scene (en haut)
    void sceneMenu (int scene, const QPoint& globalPos);
    void renameScene (int scene);                    // double-clic sur une scene
    void patternToggled (int group, int pattern);    // clic sur un pattern : entre / sort de la scene
    void patternMenu (int group, int pattern, const QPoint& globalPos);
    void groupClicked (int group);                   // clic sur un groupe (en bas)
    void groupMenu (int group, const QPoint& globalPos);

protected:
    void paintEvent (QPaintEvent*) override;
    void mousePressEvent (QMouseEvent*) override;
    void mouseDoubleClickEvent (QMouseEvent*) override;
    void wheelEvent (QWheelEvent*) override;
    void mouseMoveEvent (QMouseEvent*) override;
    void leaveEvent (QEvent*) override;

private:
    static constexpr int kHeadH = 30, kFootH = 44, kRowH = 24, kSceneW = 104;
    int bank() const { return state.group >= 8 ? 8 : 0; }
    int sceneCount() const;                          // scenes montrees : les pleines + 1 vide (8 au moins)
    double colW() const { return width() / 8.0; }
    int visibleRows() const { return std::max (1, (height() - kHeadH - kFootH - 8) / kRowH); }
    std::vector<int> patternsOf (int group) const;   // les patterns qui existent, dans l'ordre
    QRectF sceneRect (int scene) const;
    QRectF cellRect (int column, int row) const;     // row = rang dans la liste (avant defilement)
    QRectF footRect (int column) const;
    bool hit (const QPointF& p, int& scene, int& group, int& pattern, bool& foot) const;

    seq::Sequencer& sequencer;
    const session::State& state;
    int chosen = 0;
    int firstScene = 0, firstRow = 0;
    int hoverGroup = -1, hoverPattern = -1;
};
