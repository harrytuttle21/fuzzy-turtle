// Fuzzy John DAW Pulses — vue PATTERN : le pattern du groupe choisi, 16 pistes (une par son).
// Une case = note (et sa duree, en bande plus claire) ; la velocite donne l'intensite.
// Clic = choisir une case ; Suppr = l'effacer (geree par la fenetre).
// Selection : clic sur une note, Shift + clic = ajouter / retirer, cadre trace dans une zone vide.
// Glisser la selection = la deplacer (Option / Alt = la copier) ; deposer un pad = ajouter sa note.
// Licence : GPLv3.
#pragma once
#include "sequencer.h"

#include <QList>
#include <QPoint>
#include <QSet>
#include <QWidget>

class PatternView : public QWidget
{
    Q_OBJECT
public:
    PatternView (const seq::Sequencer& sequencer, const session::State& state, QWidget* parent = nullptr);

    void setPlayRow (int row) { playRow = row; update(); }
    void setPlaying (bool p) { playing = p; update(); }
    int selectedRow() const { return selRow; }
    int selectedTrack() const { return selTrack; }
    QList<QPoint> selection() const;                 // notes choisies (x = piste, y = ligne)
    void clearSelection() { selected.clear(); update(); }
    void setSelection (const QList<QPoint>& cells);  // x = piste, y = ligne
    void setCurrentCell (int row, int track) { selRow = row; selTrack = track; update(); }

    static QString copyKey();                        // « ⌥ » sur Mac, « Alt » ailleurs

    QSize minimumSizeHint() const override { return { 300, 150 }; }

signals:
    void cellsMoved (const QList<QPoint>& cells, int rows, int tracks, bool copy);   // decalage
    void padDropped (int pad, int row);               // la note va dans la piste du pad
    void trackClicked (int track, bool solo);         // en-tete : clic = mute, Shift + clic = solo

protected:
    void paintEvent (QPaintEvent*) override;
    void mousePressEvent (QMouseEvent*) override;
    void wheelEvent (QWheelEvent*) override;
    void mouseMoveEvent (QMouseEvent*) override;
    void mouseReleaseEvent (QMouseEvent*) override;
    void dragEnterEvent (QDragEnterEvent*) override;
    void dragMoveEvent (QDragMoveEvent*) override;
    void dragLeaveEvent (QDragLeaveEvent*) override;
    void dropEvent (QDropEvent*) override;

private:
    static constexpr int kMaxVisible = 16;           // lignes visibles au plus
    static constexpr int kMinRowPx = 16;             // hauteur mini d'une ligne
    int visibleRows() const;                         // selon la hauteur de la vue
    int firstRow (int length) const;
    bool cellAt (const QPointF& pos, int& row, int& track) const;   // case sous la souris

    static int key (int row, int track) { return row * seq::kTracks + track; }
    bool canShift (int rows, int tracks) const;      // toute la selection reste dans le pattern ?

    QSet<int> selected;                              // notes choisies
    // Glisser en cours : selection deplacee, cadre de selection, ou pad depose
    int dragRow = -1, dragTrack = -1, targetRow = -1, targetTrack = -1;
    int bandRow = -1, bandTrack = -1;                // coin de depart du cadre
    bool moving = false, banding = false;
    QSet<int> bandBase;                              // selection avant le cadre (Shift)
    QPointF pressPos;

    const seq::Sequencer& sequencer;
    const session::State& state;
    int playRow = 0, selRow = 0, selTrack = 0, scroll = 0;
    bool playing = false;
};
