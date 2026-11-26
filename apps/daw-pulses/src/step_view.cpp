// Vue STEPS : voir step_view.h
#include "step_view.h"
#include "pattern_view.h"
#include "tracker_view.h"

#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <algorithm>

using namespace session;

StepView::StepView (const seq::Sequencer& seqr, const session::State& st, PatternView& cur, QWidget* parent)
    : QWidget (parent), sequencer (seqr), state (st), cursor (cur)
{
    setFocusPolicy (Qt::NoFocus);
}

int StepView::track() const { return std::clamp (state.sound - 1, 0, seq::kTracks - 1); }

QRectF StepView::cellRect (int row) const
{
    const int length = std::max (1, sequencer.pattern (state.group).length);
    const int lines = (length + kPerLine - 1) / kPerLine;
    const double numW = 34;
    const double w = (width() - numW - 4) / kPerLine;
    const double h = std::min (w * 0.8, double (height() - 4) / lines);
    return QRectF (numW + (row % kPerLine) * w + 1, 2 + (row / kPerLine) * h + 1, w - 3, h - 3);
}

int StepView::rowAt (const QPointF& p) const
{
    const int length = sequencer.pattern (state.group).length;
    for (int r = 0; r < length; ++r)
        if (cellRect (r).adjusted (-1, -1, 2, 2).contains (p))
            return r;
    return -1;
}

void StepView::paintEvent (QPaintEvent*)
{
    QPainter g (this);
    g.setRenderHint (QPainter::Antialiasing);
    g.fillRect (rect(), QColor (14, 14, 18));
    const seq::Pattern pat = sequencer.pattern (state.group);
    const QColor color = groupColor (state.group);
    const int t = track();
    QSet<int> chosen;
    for (const QPoint& c : cursor.selection())
        if (c.x() == t)
            chosen.insert (c.y());
    QFont f = font();
    for (int r = 0; r < pat.length; ++r)
    {
        const QRectF box = cellRect (r);
        if (r % kPerLine == 0)                        // numero de mesure a gauche
        {
            g.setPen (QColor (150, 150, 160));
            g.drawText (QRectF (0, box.top(), 30, box.height()), Qt::AlignCenter, QString::number (r / kPerLine + 1));
        }
        const seq::Cell& c = pat.rows[(size_t) r][(size_t) t];
        QColor back = (r / 4) % 2 ? QColor (30, 30, 38) : QColor (40, 40, 50);   // temps alternes
        if (! c.empty())
            back = color.darker (100 + (127 - c.velocity));   // plus fort = plus vif
        if (! c.empty() && chosen.contains (r))     // note choisie : remplie d'orange
            back = QColor (252, 140, 40);
        g.setPen (Qt::NoPen);
        g.setBrush (back);
        g.drawRoundedRect (box, 4, 4);
        if (playing && r == playRow % std::max (1, pat.length))
        {
            g.setBrush (QColor (255, 255, 255, 60));
            g.drawRoundedRect (box, 4, 4);
        }
        if (! c.empty())
        {
            f.setPointSizeF (std::clamp (box.height() / 3.2, 7.0, 12.0));
            g.setFont (f);
            g.setPen (QColor (15, 15, 18));
            QString text = TrackerView::noteText (c.note).remove ('-');
            if (c.chordSize() > 1)
                text += QString (" +%1").arg (c.chordSize() - 1);
            g.drawText (box, Qt::AlignCenter, text);
        }
        g.setBrush (Qt::NoBrush);
        if (chosen.contains (r))                     // note choisie : remplie d'orange
        {
            g.setPen (QPen (QColor (255, 220, 160), 2));
            g.drawRoundedRect (box.adjusted (1, 1, -1, -1), 4, 4);
        }
        if (drag == Drag::Move && targetRow >= 0 && chosen.contains (r) && targetRow != pressRow)
        {
            const int to = r + targetRow - pressRow;    // place d'arrivee
            if (to >= 0 && to < pat.length)
            {
                g.setPen (QPen (Qt::white, 1.5, Qt::DashLine));
                g.drawRoundedRect (cellRect (to).adjusted (2, 2, -2, -2), 4, 4);
            }
        }
        if (r == cursor.selectedRow() && cursor.selectedTrack() == t)
        {
            g.setPen (QPen (Qt::white, 1.5));
            g.drawRoundedRect (box.adjusted (-1, -1, 1, 1), 5, 5);
        }
    }
    if (! bandRect.isNull())                         // le cadre de selection
    {
        g.setPen (QPen (QColor (252, 140, 40), 1, Qt::DashLine));
        g.setBrush (QColor (252, 140, 40, 40));
        g.drawRect (bandRect);
    }
}

void StepView::publish()
{
    int n = 0;
    for (const QPoint& c : cursor.selection())
        if (c.x() == track())
            ++n;
    emit selectionShown (n);
    update();
}

void StepView::mousePressEvent (QMouseEvent* e)
{
    const int r = rowAt (e->position());
    if (e->button() != Qt::LeftButton)
        return;
    const int t = track();
    pressPos = e->position();
    const Qt::KeyboardModifiers mods = e->modifiers();
    const bool cmd = mods & Qt::ControlModifier;      // Cmd sur Mac
    const bool shift = mods & Qt::ShiftModifier;
    if (r < 0)                                       // hors des cases : cadre seulement
    {
        before = (cmd || shift) ? cursor.selection() : QList<QPoint> {};
        if (! cmd && ! shift)
            cursor.clearSelection();
        pressRow = targetRow = -1;
        drag = Drag::PendingAdd;
        publish();
        return;
    }
    const seq::Pattern pat = sequencer.pattern (state.group);
    const bool empty = pat.rows[(size_t) r][(size_t) t].empty();
    pressRow = targetRow = r;
    cursor.setCurrentCell (r, t);
    if (cmd || shift)                                // choisir des notes (et glisser = cadre en plus)
    {
        QList<QPoint> sel = cursor.selection();
        const QPoint p (t, r);
        if (shift && anchorRow >= 0 && ! empty)      // Shift : toute la plage depuis la derniere note
        {
            for (int k = std::min (anchorRow, r); k <= std::max (anchorRow, r) && k < pat.length; ++k)
                if (! pat.rows[(size_t) k][(size_t) t].empty() && ! sel.contains (QPoint (t, k)))
                    sel << QPoint (t, k);
        }
        else if (! empty)                            // Cmd : ajouter / enlever cette note
            sel.contains (p) ? (void) sel.removeAll (p) : (void) sel.append (p);
        if (! empty)
            anchorRow = r;
        cursor.setSelection (sel);
        before = sel;
        drag = Drag::PendingAdd;
        publish();
        return;
    }
    if (empty)                                       // clic = note ; glisser = cadre de selection
    {
        drag = Drag::Pending;
        before = {};
        update();
        return;
    }
    anchorRow = r;
    if (! cursor.selection().contains (QPoint (t, r)))   // autre note : la selection repart d'elle
        cursor.setSelection ({ QPoint (t, r) });
    drag = Drag::Move;                               // glisser : deplacer (Option = copier)
    publish();
}

void StepView::mouseMoveEvent (QMouseEvent* e)
{
    if (drag == Drag::None)
        return;
    const QPointF p = e->position();
    if ((drag == Drag::Pending || drag == Drag::PendingAdd) && (p - pressPos).manhattanLength() < 6)
        return;
    if (drag == Drag::Pending || drag == Drag::PendingAdd)
        drag = Drag::Band;
    if (drag == Drag::Band)                          // toutes les notes dont la case touche le cadre
    {
        bandRect = QRectF (pressPos, p).normalized().adjusted (-1, -1, 1, 1);   // jamais plat
        const seq::Pattern pat = sequencer.pattern (state.group);
        QList<QPoint> sel = before;
        for (int r = 0; r < pat.length; ++r)
            if (! pat.rows[(size_t) r][(size_t) track()].empty() && cellRect (r).intersects (bandRect) && ! sel.contains (QPoint (track(), r)))
                sel << QPoint (track(), r);
        cursor.setSelection (sel);
        publish();
        return;
    }
    const int r = rowAt (p);
    if (r >= 0)
        targetRow = r;
    update();
}

void StepView::mouseReleaseEvent (QMouseEvent* e)
{
    const Drag was = drag;
    drag = Drag::None;
    bandRect = {};
    if (was == Drag::Pending)                        // simple clic dans le vide : une note
    {
        cursor.clearSelection();
        emit addRequested (pressRow);
        publish();
    }
    else if (was == Drag::Move && targetRow >= 0 && targetRow != pressRow)
        emit cellsMoved (cursor.selection(), targetRow - pressRow, 0, e->modifiers() & Qt::AltModifier);
    targetRow = -1;
    update();
}

void StepView::mouseDoubleClickEvent (QMouseEvent* e)
{
    const int r = rowAt (e->position());
    if (r >= 0 && ! sequencer.pattern (state.group).rows[(size_t) r][(size_t) track()].empty())
        emit cellDoubleClicked (r, track());
}

void StepView::contextMenuEvent (QContextMenuEvent* e)
{
    const int r = rowAt (e->pos());
    if (r < 0)
        return;
    cursor.setCurrentCell (r, track());
    emit cellMenu (r, track(), e->globalPos());
}

void StepView::wheelEvent (QWheelEvent* e)
{
    const int r = rowAt (e->position());
    const int steps = (e->angleDelta().y() ? e->angleDelta().y() : e->angleDelta().x()) / 120;
    if (r >= 0 && steps && ! sequencer.pattern (state.group).rows[(size_t) r][(size_t) track()].empty())
        emit valueWheel (r, steps);
}
