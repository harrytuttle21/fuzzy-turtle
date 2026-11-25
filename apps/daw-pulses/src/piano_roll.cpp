// Fuzzy John DAW Pulses — vue Piano Roll. Licence : GPLv3.
#include "piano_roll.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {
bool isBlack (int pitch)
{
    const int n = pitch % 12;
    return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
}
}

PianoRollView::PianoRollView (seq::Sequencer& s, const session::State& st, QWidget* parent)
    : QWidget (parent), sequencer (s), state (st)
{
    setMouseTracking (true);
    setAcceptDrops (true);                           // fichiers MIDI
    repaint.setInterval (60);
    connect (&repaint, &QTimer::timeout, this, [this] { if (isVisible()) update(); });
    repaint.start();
}

QList<int> PianoRollView::notesOf (const seq::Cell& c)
{
    QList<int> out;
    if (c.empty())
        return out;
    out << c.note;
    for (int n : c.extra)
        if (n >= 0)
            out << n;
    return out;
}

seq::Cell PianoRollView::withNotes (seq::Cell base, const QList<int>& notes)
{
    if (notes.isEmpty())
        return {};
    base.note = notes[0];
    base.extra.fill (-1);
    for (int i = 1; i < notes.size() && i <= seq::kChordExtra; ++i)
        base.extra[(size_t) i - 1] = notes[i];
    return base;
}

void PianoRollView::centerOn (int pitch)
{
    setTopPitch (pitch + visiblePitches() / 2);
}

int PianoRollView::visibleRows() const
{
    const int length = std::max (1, sequencer.patternLength (state.group));
    return std::clamp ((int) std::lround (length * 100.0 / zoomPercent), 1, length);
}

int PianoRollView::visiblePitches() const
{
    return std::max (1, (height() - kHeader) / rowPx);
}

void PianoRollView::setZoomX (int percent)
{
    zoomPercent = std::clamp (percent, 100, 1600);
    setScrollX (firstRowShown);
}

void PianoRollView::setRowHeight (int px)
{
    const int middle = topPitch - visiblePitches() / 2;     // garder le milieu en place
    rowPx = std::clamp (px, 6, 30);
    setTopPitch (middle + visiblePitches() / 2);
}

void PianoRollView::setScrollX (int first)
{
    const int length = std::max (1, sequencer.patternLength (state.group));
    firstRowShown = std::clamp (first, 0, std::max (0, length - visibleRows()));
    update();
    emit viewChanged();
}

void PianoRollView::setTopPitch (int pitch)
{
    topPitch = std::clamp (pitch, std::min (127, visiblePitches() - 1), 127);
    update();
    emit viewChanged();
}

double PianoRollView::colWidth() const
{
    return (width() - kKeys) / double (visibleRows());
}

double PianoRollView::xOfRow (double row) const { return kKeys + (row - firstRowShown) * colWidth(); }
double PianoRollView::rowAt (double x) const { return (x - kKeys) / colWidth() + firstRowShown; }
double PianoRollView::yOfPitch (int pitch) const { return kHeader + (topPitch - pitch) * rowPx; }
int PianoRollView::pitchAt (double y) const { return std::clamp (topPitch - (int) std::floor ((y - kHeader) / rowPx), 0, 127); }

bool PianoRollView::noteAt (const QPointF& p, int& row, int& pitch, bool& edge) const
{
    // La note la plus recente qui couvre ce point (une note peut durer plusieurs lignes)
    const seq::Pattern pat = sequencer.pattern (state.group);
    const int y = pitchAt (p.y());
    for (int r = pat.length - 1; r >= 0; --r)
    {
        const seq::Cell& c = pat.rows[(size_t) r][(size_t) track()];
        if (! notesOf (c).contains (y))
            continue;
        const double x0 = xOfRow (r + c.offset / double (seq::kTiming)), x1 = xOfRow (r + c.length);
        if (p.x() >= x0 && p.x() <= x1)
        {
            row = r;
            pitch = y;
            edge = x1 - p.x() < std::min (7.0, (x1 - x0) / 3);
            return true;
        }
    }
    return false;
}

void PianoRollView::removeNote (int row, int pitch)
{
    seq::Cell c = sequencer.pattern (state.group).rows[(size_t) row][(size_t) track()];
    QList<int> notes = notesOf (c);
    notes.removeAll (pitch);
    sequencer.setCell (state.group, row, track(), withNotes (c, notes));
}

void PianoRollView::paintEvent (QPaintEvent*)
{
    QPainter g (this);
    const seq::Pattern pat = sequencer.pattern (state.group);
    const QColor color = session::groupColor (state.group);
    if (shownSound != state.sound)                    // autre son : on se place sur ses notes
    {
        shownSound = state.sound;
        int sum = 0, n = 0;
        for (int r = 0; r < pat.length; ++r)
            for (int p : notesOf (pat.rows[(size_t) r][(size_t) track()]))
                sum += p, ++n;
        centerOn (n ? sum / n : session::defaultNote (state.sound));
    }
    if (pat.length != lastLength)
    {
        lastLength = pat.length;
        firstRowShown = std::clamp (firstRowShown, 0, std::max (0, pat.length - visibleRows()));
        QMetaObject::invokeMethod (this, [this] { emit viewChanged(); }, Qt::QueuedConnection);
    }
    g.fillRect (rect(), QColor (18, 18, 22));
    const double cw = colWidth();

    // Lignes de notes (touches noires plus sombres) et clavier
    for (int pitch = topPitch; yOfPitch (pitch) < height(); --pitch)
    {
        if (pitch < 0)
            break;
        const double y = yOfPitch (pitch);
        g.fillRect (QRectF (kKeys, y, width() - kKeys, rowPx), isBlack (pitch) ? QColor (22, 22, 27) : QColor (28, 28, 34));
        g.fillRect (QRectF (0, y, kKeys - 2, rowPx - 1), pitch == keyDown ? color
                                                           : isBlack (pitch) ? QColor (40, 40, 46) : QColor (210, 210, 214));
        if (pitch % 12 == 0)                          // les do : nom et trait
        {
            g.setPen (QColor (90, 90, 100));
            g.drawLine (QPointF (kKeys, y + rowPx), QPointF (width(), y + rowPx));
            QFont f = font();
            f.setPixelSize (9);
            g.setFont (f);
            g.setPen (QColor (40, 40, 46));
            g.drawText (QRectF (2, y, kKeys - 6, rowPx), Qt::AlignVCenter | Qt::AlignRight, session::noteName (pitch));
        }
    }
    // Colonnes : temps (traits plus marques) et fin du pattern
    for (int r = firstRowShown; r <= std::min (pat.length, firstRowShown + visibleRows()); ++r)
    {
        g.setPen (r % 4 == 0 ? QColor (60, 60, 70) : QColor (34, 34, 40));
        g.drawLine (QPointF (xOfRow (r), kHeader), QPointF (xOfRow (r), height()));
    }
    // En-tete : numero des lignes a chaque temps
    g.fillRect (QRectF (0, 0, width(), kHeader), QColor (14, 14, 17));
    QFont f = font();
    f.setPixelSize (10);
    g.setFont (f);
    g.setPen (QColor (140, 140, 150));
    g.setClipRect (QRectF (kKeys, 0, width() - kKeys, kHeader));
    const int every = cw * 4 >= 26 ? 4 : cw * 8 >= 26 ? 8 : 16;   // numeros lisibles meme dezoome
    for (int r = 0; r < pat.length; r += every)
        g.drawText (QRectF (xOfRow (r) + 3, 0, cw * every, kHeader), Qt::AlignVCenter | Qt::AlignLeft,
                    QString ("%1").arg (r, 2, 16, QChar ('0')).toUpper());
    g.setClipping (false);
    g.setPen (color);
    g.drawText (QRectF (0, 0, kKeys, kHeader), Qt::AlignCenter, QString ("%1").arg (state.sound.load(), 2, 10, QChar ('0')));

    // Notes du son choisi
    g.setClipRect (QRectF (kKeys, kHeader, width() - kKeys, height() - kHeader));
    for (int r = 0; r < pat.length; ++r)
    {
        const seq::Cell& c = pat.rows[(size_t) r][(size_t) track()];
        for (int pitch : notesOf (c))
        {
            const bool dragged = drag != Drag::None && moved && r == dragRow && pitch == dragPitch;
            const int length = dragged && drag == Drag::Resize ? newLength : c.length;
            QRectF box (xOfRow (r + c.offset / double (seq::kTiming)) + 1, yOfPitch (pitch) + 1,
                        std::max (4.0, length * cw - c.offset / double (seq::kTiming) * cw - 2), rowPx - 2);
            QColor fill = color;
            fill.setAlphaF (0.4 + 0.6 * c.velocity / 127.0);   // plus vif = plus fort
            if (dragged && drag == Drag::Move)
                fill.setAlphaF (0.25);                     // sa place d'origine pendant le deplacement
            g.fillRect (box, fill);
            g.setPen (QColor (255, 255, 255, 90));
            g.drawRect (box);
            if (box.width() > 34)
            {
                QFont nf = font();
                nf.setPixelSize (9);
                g.setFont (nf);
                g.setPen (QColor (15, 15, 15));
                g.drawText (box.adjusted (3, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, session::noteName (pitch));
            }
        }
    }
    // Note deplacee : sa nouvelle place
    if (drag == Drag::Move && moved)
    {
        const seq::Cell& c = pat.rows[(size_t) dragRow][(size_t) track()];
        QRectF box (xOfRow (targetRow) + 1, yOfPitch (targetPitch) + 1, std::max (4.0, c.length * cw - 2), rowPx - 2);
        g.setPen (QPen (Qt::white, 1.5, Qt::DashLine));
        g.setBrush (QColor (255, 255, 255, 40));
        g.drawRect (box);
    }
    // Tete de lecture
    if (playing && playRow >= 0)
    {
        const double x = xOfRow (playRow % std::max (1, pat.length));
        g.fillRect (QRectF (x, kHeader, std::max (2.0, cw), height()), QColor (255, 255, 255, 30));
    }
}

void PianoRollView::mousePressEvent (QMouseEvent* e)
{
    if (e->button() == Qt::RightButton || (e->modifiers() & Qt::MetaModifier))
        return;                                      // clic secondaire : contextMenuEvent
    const QPointF p = e->position();
    if (p.y() < kHeader)
        return;
    if (p.x() < kKeys)                               // clavier : ecouter
    {
        keyDown = pitchAt (p.y());
        emit keyPressed (keyDown);
        update();
        return;
    }
    pressPos = p;
    moved = false;
    int row = 0, pitch = 0;
    bool edge = false;
    if (noteAt (p, row, pitch, edge))
    {
        dragRow = targetRow = row;
        dragPitch = targetPitch = pitch;
        newLength = sequencer.pattern (state.group).rows[(size_t) row][(size_t) track()].length;
        drag = edge ? Drag::Resize : Drag::Move;
        return;
    }
    // Case vide (ou accord a completer) : nouvelle note ; tirer = sa duree
    const int r = std::clamp ((int) rowAt (p.x()), 0, sequencer.patternLength (state.group) - 1);
    const int y = pitchAt (p.y());
    seq::Cell c = sequencer.pattern (state.group).rows[(size_t) r][(size_t) track()];
    QList<int> notes = notesOf (c);
    if (notes.size() > seq::kChordExtra)
        return;                                      // accord complet (8 notes)
    emit aboutToEdit();
    if (c.empty())
        c = { y, state.velocity.load(), 1, 0 };
    notes << y;
    sequencer.setCell (state.group, r, track(), withNotes (c, notes));
    emit keyPressed (y);                              // on entend la note posee
    keyDown = y;
    dragRow = r;
    dragPitch = y;
    newLength = c.length;
    drag = Drag::Resize;
    moved = true;
    emit edited();
    update();
}

void PianoRollView::mouseMoveEvent (QMouseEvent* e)
{
    const QPointF p = e->position();
    if (drag == Drag::None)
    {
        int row = 0, pitch = 0;
        bool edge = false;
        setCursor (p.x() < kKeys ? Qt::PointingHandCursor
                   : noteAt (p, row, pitch, edge) ? (edge ? Qt::SizeHorCursor : Qt::OpenHandCursor) : Qt::ArrowCursor);
        return;
    }
    if (! moved && (p - pressPos).manhattanLength() < QApplication::startDragDistance())
        return;
    moved = true;
    const int length = sequencer.patternLength (state.group);
    if (drag == Drag::Resize)
        newLength = std::clamp ((int) std::ceil (rowAt (p.x()) - dragRow), 1, length - dragRow);
    else
    {
        const int dr = (int) std::floor (rowAt (p.x())) - (int) std::floor (rowAt (pressPos.x()));
        targetRow = std::clamp (dragRow + dr, 0, length - 1);
        targetPitch = pitchAt (p.y());
        setCursor (e->modifiers() & Qt::AltModifier ? Qt::DragCopyCursor : Qt::ClosedHandCursor);
    }
    update();
}

void PianoRollView::mouseReleaseEvent (QMouseEvent* e)
{
    if (keyDown >= 0)
    {
        emit keyReleased (keyDown);
        keyDown = -1;
    }
    if (drag == Drag::Resize && moved)
    {
        // La duree vaut pour toute la case (les notes d'un accord durent pareil)
        seq::Cell c = sequencer.pattern (state.group).rows[(size_t) dragRow][(size_t) track()];
        if (! c.empty() && c.length != newLength)
        {
            emit aboutToEdit();
            c.length = newLength;
            sequencer.setCell (state.group, dragRow, track(), c);
            emit edited();
        }
    }
    else if (drag == Drag::Move && moved && (targetRow != dragRow || targetPitch != dragPitch))
    {
        const seq::Cell from = sequencer.pattern (state.group).rows[(size_t) dragRow][(size_t) track()];
        emit aboutToEdit();
        if (! (e->modifiers() & Qt::AltModifier))     // Option / Alt : copie
            removeNote (dragRow, dragPitch);
        seq::Cell to = sequencer.pattern (state.group).rows[(size_t) targetRow][(size_t) track()];
        QList<int> notes = notesOf (to);
        if (to.empty())
            to = { targetPitch, from.velocity, from.length, targetRow == dragRow ? from.offset : 0 };
        if (! notes.contains (targetPitch) && notes.size() <= seq::kChordExtra)
            notes << targetPitch;
        sequencer.setCell (state.group, targetRow, track(), withNotes (to, notes));
        emit edited();
    }
    drag = Drag::None;
    moved = false;
    unsetCursor();
    update();
}

void PianoRollView::mouseDoubleClickEvent (QMouseEvent* e)
{
    int row = 0, pitch = 0;
    bool edge = false;
    if (e->button() == Qt::LeftButton && noteAt (e->position(), row, pitch, edge))
    {
        emit aboutToEdit();
        removeNote (row, pitch);
        emit edited();
        drag = Drag::None;
        update();
    }
}

void PianoRollView::contextMenuEvent (QContextMenuEvent* e)
{
    int row = 0, pitch = 0;
    bool edge = false;
    if (noteAt (e->pos(), row, pitch, edge))         // clic droit sur une note : l'effacer
    {
        emit aboutToEdit();
        removeNote (row, pitch);
        emit edited();
        update();
    }
}

void PianoRollView::wheelEvent (QWheelEvent* e)
{
    int row = 0, pitch = 0;
    bool edge = false;
    const int dy = e->angleDelta().y() != 0 ? e->angleDelta().y() : e->angleDelta().x();
    const int step = dy > 0 ? 1 : dy < 0 ? -1 : 0;
    if (! step)
        return;
    if (! (e->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier))
        && noteAt (e->position(), row, pitch, edge))   // sur une note : velocite
    {
        seq::Cell c = sequencer.pattern (state.group).rows[(size_t) row][(size_t) track()];
        emit aboutToEdit();
        c.velocity = std::clamp (c.velocity + step * 5, 1, 127);
        sequencer.setCell (state.group, row, track(), c);
        emit edited();
        update();
        return;
    }
    if (e->modifiers() & Qt::ControlModifier)        // Cmd (Ctrl) + molette : zoom horizontal
        setZoomX (zoomPercent * (step > 0 ? 5 : 4) / (step > 0 ? 4 : 5));
    else if (e->modifiers() & Qt::AltModifier)       // Option (Alt) + molette : zoom vertical
        setRowHeight (rowPx + step * 2);
    else if (e->modifiers() & Qt::ShiftModifier)     // Shift + molette : temps
        setScrollX (firstRowShown - step * std::max (1, visibleRows() / 8));
    else
        setTopPitch (topPitch + step * 2);
}

void PianoRollView::dragEnterEvent (QDragEnterEvent* e)
{
    if (! session::midiFileIn (e->mimeData()->urls()).isEmpty())
        e->acceptProposedAction();
}

void PianoRollView::dropEvent (QDropEvent* e)
{
    const QString path = session::midiFileIn (e->mimeData()->urls());
    if (! path.isEmpty())
    {
        e->acceptProposedAction();
        emit midiDropped (path, state.sound);
    }
}
