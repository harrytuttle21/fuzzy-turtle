#pragma once
#include "sequencer.h"

#include <QWidget>

class PatternView;

// Vue STEPS du Piano Roll : la piste du son choisi seule, en pas (16 par ligne = une mesure),
// comme le mode Step de la Maschine. Curseur et selection partages avec la vue Pattern.
class StepView : public QWidget
{
    Q_OBJECT
public:
    StepView (const seq::Sequencer& sequencer, const session::State& state, PatternView& cursor, QWidget* parent = nullptr);

    void setPlayRow (int row) { playRow = row; update(); }
    void setPlaying (bool p) { playing = p; update(); }
    QSize minimumSizeHint() const override { return { 300, 120 }; }

signals:
    void addRequested (int row);                      // clic sur un pas vide : une note
    void cellMenu (int row, int track, const QPoint& globalPos);
    void cellDoubleClicked (int row, int track);      // double-clic sur une note : ses options
    void valueWheel (int row, int steps);             // molette sur une note : velocite
    void cellsMoved (const QList<QPoint>& cells, int rows, int tracks, bool copy);   // glisser la selection

protected:
    void paintEvent (QPaintEvent*) override;
    void mousePressEvent (QMouseEvent*) override;
    void mouseDoubleClickEvent (QMouseEvent*) override;
    void mouseMoveEvent (QMouseEvent*) override;
    void mouseReleaseEvent (QMouseEvent*) override;
    void contextMenuEvent (QContextMenuEvent*) override;
    void wheelEvent (QWheelEvent*) override;

private:
    static constexpr int kPerLine = 16;
    QRectF cellRect (int row) const;
    int rowAt (const QPointF& p) const;
    int track() const;

    const seq::Sequencer& sequencer;
    const session::State& state;
    PatternView& cursor;
    int playRow = 0;
    // Souris : clic dans le vide = note ; glisser dans le vide = cadre ; glisser une note = deplacer
    enum class Drag { None, Pending, Band, Move } drag = Drag::None;
    QPointF pressPos;
    int pressRow = -1, targetRow = -1;
    QList<QPoint> before;                            // selection avant un cadre (Shift)
    bool playing = false;
};
