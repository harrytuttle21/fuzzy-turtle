#pragma once
#include "sequencer.h"

#include <QWidget>

#include <algorithm>
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
    void setRotated (bool on) { rotated = on; firstGroup = 0; update(); }   // scenes en colonnes, groupes en lignes
    // Modes : Edit = modifier les scenes ; Play = jouer en cliquant ; aucun = tout est protege
    void setModes (bool edit, bool play) { editMode = edit; playMode = play; update(); }
    bool isRotated() const { return rotated; }
    QSize minimumSizeHint() const override { return { 400, 160 }; }

signals:
    void sceneClicked (int scene);                   // lancer / ranger (comme la barre)
    void sceneMenu (int scene, const QPoint& globalPos);
    void changed();                                  // un pattern ou une scene a change
    void groupChosen (int group);                    // clic dans la colonne d'un groupe
    void renameScene (int scene);                    // double-clic sur le nom
    void renamePattern (int group, int pattern);
    void groupMenu (int group, const QPoint& globalPos);   // clic droit sur un groupe : Mute / Solo...
    void muteSoloClicked (int group, bool solo);     // boutons M / S d'un groupe (Rotate)
    void allOff (bool solo);                         // A/M : tout dé-muter ; A/S : tout dé-soloer
    void beforeEdit (const QString& label);          // avant de modifier une scene (Cmd+Z)
    void message (const QString& text);

protected:
    void paintEvent (QPaintEvent*) override;
    void mousePressEvent (QMouseEvent*) override;
    void wheelEvent (QWheelEvent*) override;
    void mouseDoubleClickEvent (QMouseEvent*) override;
    void dragEnterEvent (class QDragEnterEvent*) override;   // un carre de la palette
    void dragMoveEvent (class QDragMoveEvent*) override;
    void dragLeaveEvent (class QDragLeaveEvent*) override;
    void dropEvent (class QDropEvent*) override;

private:
    int nameW() const { return rotated ? 196 : 120; }                    // tourne : M, S et noms lisibles
    int nowW() const { return 110; }                                       // tourne : colonne NOW
    int rowH() const { return 28; }
    double cellW() const { return (width() - nameW() - 4) / double (session::kGroups); }
    double groupH() const { return 24; }             // tourne : hauteur fixe, lisible (on fait defiler)
    int visibleGroups() const;                       // tourne : combien de groupes tiennent
    bool groupShown (int g) const { return ! rotated || (g >= firstGroup && g < firstGroup + visibleGroups()); }
    QRectF upArrow() const { return QRectF (nameW() - 44, 2, 20, rowH() - 4); }
    QRectF muteBox (int g) const;                    // Rotate : M et S a gauche du groupe
    QRectF soloBox (int g) const;
    QRectF allMuteBox() const { return QRectF (2, 3, 34, rowH() - 6); }   // A/M au-dessus
    QRectF allSoloBox() const { return QRectF (38, 3, 34, rowH() - 6); }  // A/S
    bool dropTarget (const QPointF& pos, int group, int& scene, int& cellGroup) const;   // ou lacher un pattern
    int dropScene = -1, dropGroup = -1;              // case survolee pendant le glisser
    QRectF downArrow() const { return QRectF (nameW() - 22, 2, 20, rowH() - 4); }
    QRectF lockBox (int k) const;                    // le cadenas d'une scene (rang visible)
    bool sceneLocked (int i) const { return sequencer.scene (i).locked; }
    double sceneW() const                            // tourne : les scenes remplissent toute la largeur
    {
        const int avail = std::max (96, width() - nameW() - nowW() - 8);
        return avail / double (std::clamp (avail / 96, 1, 64));
    }
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
    int firstScene = 0, firstGroup = 0;
    bool rotated = false;
    bool editMode = false, playMode = true;
    int editScene = -1;                              // Edit seul : la scene choisie (cadre)
};
