#pragma once
#include "sequencer.h"

#include <QWidget>

// Palette (vue Scenes, bouton « Palette ») : les 16 patterns de la banque du groupe choisi,
// en carres places comme les pads (13 en haut a gauche). Glisser un carre dans une case de
// scene l'y met (mode Edit) ; un clic le joue (mode Play).
class PaletteView : public QWidget
{
    Q_OBJECT
public:
    PaletteView (const seq::Sequencer& sequencer, const session::State& state, QWidget* parent = nullptr);
    QSize sizeHint() const override { return { 4 * kCell + 8, 30 + 4 * kCell + 4 }; }
    QSize minimumSizeHint() const override { return sizeHint(); }
    int bank = 0;

signals:
    void playRequested (int group, int index);       // clic sur un carre
    void squareMenu (int index, const QPoint& globalPos);   // clic droit (Edit) : Copy, Paste, Duplicate, Delete

protected:
    void paintEvent (QPaintEvent*) override;
    void mousePressEvent (QMouseEvent*) override;
    void mouseMoveEvent (QMouseEvent*) override;
    void mouseReleaseEvent (QMouseEvent*) override;
    void contextMenuEvent (QContextMenuEvent*) override;

private:
    static constexpr int kCell = 66;
    QRectF square (int k) const;                     // k = 0..15 a l'ecran
    int indexAt (const QPointF& p) const;            // pattern sous la souris, -1 sinon
    QRectF bankLeft() const { return QRectF (width() - 70, 3, 22, 22); }
    QRectF bankRight() const { return QRectF (width() - 26, 3, 22, 22); }

    const seq::Sequencer& sequencer;
    const session::State& state;
    QPointF pressPos;
    int pressIndex = -1;
};
