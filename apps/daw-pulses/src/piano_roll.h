// Fuzzy John DAW Pulses — vue Piano Roll : les notes du son choisi (sa piste dans le pattern
// du groupe), le temps en largeur et la hauteur en hauteur, avec un petit clavier a gauche.
//
// Souris : clic dans le vide = une note (tirer = sa duree) ; tirer une note = la deplacer
// (Option / Alt = la copier) ; tirer son bord droit = sa duree ; double-clic ou clic droit =
// l'effacer ; molette sur une note = sa velocite, ailleurs = monter / descendre ; clic sur le
// clavier = ecouter. Une case du pattern peut tenir un accord (meme duree, meme velocite).
// Licence : GPLv3.
#pragma once
#include "sequencer.h"
#include "session.h"

#include <QColor>
#include <QTimer>
#include <QWidget>

class PianoRollView : public QWidget
{
    Q_OBJECT
public:
    PianoRollView (seq::Sequencer& sequencer, const session::State& state, QWidget* parent = nullptr);
    void setPlayRow (int row) { playRow = row; }
    void setPlaying (bool on) { playing = on; }
    void centerOn (int pitch);                       // la note au milieu de la vue
    // Zoom et defilement : temps (lignes visibles, premiere ligne) et hauteur (note du haut, taille)
    void setZoomX (int percent);                     // 100 = tout le pattern dans la largeur
    void setRowHeight (int px);                      // 6 a 30 px par note
    void setScrollX (int firstRow);
    void setTopPitch (int pitch);
    int zoomX() const { return zoomPercent; }
    int rowHeight() const { return rowPx; }
    int scrollX() const { return firstRowShown; }
    int topPitchShown() const { return topPitch; }
    int visibleRows() const;                         // lignes du pattern visibles en largeur
    int visiblePitches() const;
    QSize sizeHint() const override { return { 700, 360 }; }
    QSize minimumSizeHint() const override { return { 320, 160 }; }

    // Notes d'une case (note + accord) et case refaite a partir d'une liste de notes
    static QList<int> notesOf (const seq::Cell& c);
    static seq::Cell withNotes (seq::Cell base, const QList<int>& notes);

signals:
    void aboutToEdit();                              // juste avant une modification (annuler)
    void edited();                                   // apres une modification
    void viewChanged();                              // zoom ou defilement (barres de la fenetre)
    void keyPressed (int pitch);                     // clavier de gauche : ecouter
    void keyReleased (int pitch);
    void midiDropped (const QString& path, int sound);   // fichier .mid depose

protected:
    void paintEvent (QPaintEvent*) override;
    void mousePressEvent (QMouseEvent*) override;
    void mouseMoveEvent (QMouseEvent*) override;
    void mouseReleaseEvent (QMouseEvent*) override;
    void mouseDoubleClickEvent (QMouseEvent*) override;
    void contextMenuEvent (QContextMenuEvent*) override;
    void wheelEvent (QWheelEvent*) override;
    void resizeEvent (QResizeEvent*) override { emit viewChanged(); }
    void dragEnterEvent (QDragEnterEvent*) override;
    void dropEvent (QDropEvent*) override;

private:
    static constexpr int kKeys = 44, kHeader = 18;   // clavier, en-tete
    int rowPx = 12;                                  // hauteur d'une note (zoom vertical)
    int zoomPercent = 100;                           // zoom horizontal
    int firstRowShown = 0;
    int lastLength = -1;                             // longueur changee : les barres suivent
    int track() const { return state.sound - 1; }
    double colWidth() const;
    double xOfRow (double row) const;
    double rowAt (double x) const;
    int pitchAt (double y) const;
    double yOfPitch (int pitch) const;
    bool noteAt (const QPointF& p, int& row, int& pitch, bool& edge) const;   // note sous la souris
    void removeNote (int row, int pitch);

    seq::Sequencer& sequencer;
    const session::State& state;
    QTimer repaint;                                  // suit le pattern (enregistrement, autres vues)
    int topPitch = 84;                               // note du haut de la vue
    int shownSound = -1;
    int playRow = -1;
    bool playing = false;
    // Glisser en cours
    enum class Drag { None, Move, Resize } drag = Drag::None;
    int dragRow = 0, dragPitch = 0, targetRow = 0, targetPitch = 0, newLength = 1, keyDown = -1;
    QPointF pressPos;
    bool moved = false;
};
