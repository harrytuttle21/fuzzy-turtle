#pragma once
#include "sequencer.h"

#include <QWidget>

#include <functional>

// Vue SCENES (onglet) : un grand tableau, une colonne par groupe.
//  - ligne NOW : le pattern que joue chaque groupe (clic = en choisir un autre, OFF...)
//  - une ligne par scene : son nom a gauche (clic = la lancer ; vide = y ranger ce qui joue),
//    puis le pattern de chaque groupe dans cette scene (clic = le changer, libre, silence).
class SceneView : public QWidget
{
    Q_OBJECT
public:
    SceneView (seq::Sequencer& sequencer, const session::State& state, QWidget* parent = nullptr);
    std::function<bool()> isPlaying;
    void setRotated (bool on) { rotated = on; update(); }   // scenes en colonnes, groupes en lignes
    bool isRotated() const { return rotated; }
    QSize minimumSizeHint() const override { return { 400, 160 }; }

signals:
    void sceneClicked (int scene);                   // lancer / ranger (comme la barre)
    void sceneMenu (int scene, const QPoint& globalPos);
    void changed();                                  // un pattern ou une scene a change
    void groupChosen (int group);                    // clic dans la colonne d'un groupe
    void renameScene (int scene);                    // double-clic sur le nom
    void renamePattern (int group, int pattern);

protected:
    void paintEvent (QPaintEvent*) override;
    void mousePressEvent (QMouseEvent*) override;
    void wheelEvent (QWheelEvent*) override;
    void mouseDoubleClickEvent (QMouseEvent*) override;

private:
    int nameW() const { return rotated ? 64 : 120; }
    int rowH() const { return 28; }
    double cellW() const { return (width() - nameW() - 4) / double (session::kGroups); }
    double groupH() const { return std::max (18.0, (height() - rowH() - 6) / double (session::kGroups)); }   // tourne
    int sceneW() const { return 96; }                                                                       // tourne
    int visibleScenes() const;
    // Cases (dans les deux sens) : nom d'un groupe, ce qui joue, nom d'une scene, case scene x groupe
    QRectF groupCell (int g) const;
    QRectF nowCell (int g) const;
    QRectF sceneNameCell (int k) const;              // k = rang visible
    QRectF sceneCell (int k, int g) const;
    QString patternText (int group, int index) const;
    int pickPattern (int group, const QPoint& at, bool allowFree, int currentValue);   // menu : -9 = rien

    seq::Sequencer& sequencer;
    const session::State& state;
    int firstScene = 0;
    bool rotated = false;
};
