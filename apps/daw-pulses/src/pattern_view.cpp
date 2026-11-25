// Fuzzy John DAW Pulses — vue PATTERN. Licence : GPLv3.
#include "pattern_view.h"
#include "pad_grid.h"

#include <QApplication>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QContextMenuEvent>
#include <QMouseEvent>

#include <QMouseEvent>
#include <QPainter>
#include <algorithm>
#include <cmath>
#include <QWheelEvent>

using namespace session;

namespace {
constexpr double kHeader = 18, kRowLabel = 26;
}

int PatternView::visibleRows() const
{
    return std::clamp (int ((height() - kHeader) / kMinRowPx), 4, int (kMaxVisible));
}

PatternView::PatternView (const seq::Sequencer& s, const session::State& st, QWidget* parent)
    : QWidget (parent), sequencer (s), state (st)
{
    setMinimumHeight (150);
    setAcceptDrops (true);                           // pads glisses depuis la vue des pads
}

int PatternView::firstRow (int length) const
{
    if (length <= visibleRows())
        return 0;
    if (playing)                                     // en lecture : la page suit la tete de lecture
        return (playRow % length) / visibleRows() * visibleRows();
    return std::clamp (scroll, 0, length - visibleRows());
}

void PatternView::paintEvent (QPaintEvent*)
{
    QPainter g (this);
    const seq::Pattern p = sequencer.pattern (state.group);
    const QColor color = groupColor (state.group);
    const int first = firstRow (p.length);
    const int rows = std::min (visibleRows(), p.length);
    const double cw = (width() - kRowLabel) / double (seq::kTracks);
    const double rh = (height() - kHeader) / double (visibleRows());
    QFont f = font();
    f.setPixelSize (std::clamp ((int) (rh * 0.62), 9, 12));
    g.setFont (f);

    // En-tete : numeros des pistes ; la piste du son choisi en couleur
    for (int t = 0; t < seq::kTracks; ++t)
    {
        const QRectF r (kRowLabel + t * cw, 0, cw, kHeader);
        const char tag = state.soundTag (state.group, t + 1);
        if (tag == 'M' || tag == 'S')                // piste coupee / en solo : M ou S
        {
            g.setPen (tag == 'S' ? QColor (232, 192, 64) : QColor (208, 72, 72));
            g.drawText (r, Qt::AlignCenter, QString ("%1 %2").arg (t + 1, 2, 10, QChar ('0')).arg (QChar (tag)));
            continue;
        }
        g.setPen (tag == 'm' ? QColor (70, 70, 78) : t + 1 == state.sound ? color : QColor (120, 120, 128));
        g.drawText (r, Qt::AlignCenter, QString ("%1").arg (t + 1, 2, 10, QChar ('0')));
    }
    for (int i = 0; i < rows; ++i)
    {
        const int row = first + i;
        // Cases calees sur des pixels entiers (sinon traits parasites sur ecran Retina)
        const double y = std::round (kHeader + i * rh), yEnd = std::round (kHeader + (i + 1) * rh);
        const bool isPlay = playing && row == playRow % p.length;
        if (isPlay)                                  // tete de lecture
            g.fillRect (QRectF (0, y, width(), yEnd - y), color.darker (300));
        else if (row % 4 == 0)                       // debut de chaque temps
            g.fillRect (QRectF (0, y, width(), yEnd - y), QColor (24, 24, 30));
        g.setPen (row % 4 == 0 ? QColor (170, 170, 176) : QColor (90, 90, 98));
        g.drawText (QRectF (0, y, kRowLabel - 4, rh), Qt::AlignRight | Qt::AlignVCenter,
                    QString ("%1").arg (row, 2, 16, QChar ('0')).toUpper());
        for (int t = 0; t < seq::kTracks; ++t)
        {
            const double x = std::round (kRowLabel + t * cw), xEnd = std::round (kRowLabel + (t + 1) * cw);
            const QRectF r (x + 1, y + 1, xEnd - x - 2, yEnd - y - 2);
            const seq::Cell& c = p.rows[(size_t) row][(size_t) t];
            // Duree : une note qui couvre cette ligne (depuis une ligne precedente)
            for (int back = 1; back < seq::kMaxRows && row - back >= 0 && c.empty(); ++back)
            {
                const seq::Cell& prev = p.rows[(size_t) (row - back)][(size_t) t];
                if (! prev.empty())
                {
                    if (prev.length > back)
                        g.fillRect (r.adjusted (cw * 0.3, 0, -cw * 0.3, 0), color.darker (260));
                    break;
                }
            }
            if (! c.empty())
            {
                QColor fill = color;
                fill.setAlphaF (0.35 + 0.65 * c.velocity / 127.0);   // intensite = velocite
                if (! state.soundAudible (state.group, t + 1))       // piste muette : grisee
                    fill = QColor (70, 70, 78);
                g.fillRect (r, fill);
                if (c.offset != 0)                    // timing fin : petit triangle, a gauche = en avance
                {
                    const double s = std::min (7.0, r.height() * 0.4);
                    const double xs = c.offset < 0 ? r.left() : r.right();
                    const double dir = c.offset < 0 ? 1 : -1;
                    const QPointF tri[3] { { xs, r.top() }, { xs + dir * s, r.top() }, { xs, r.top() + s } };
                    g.setPen (Qt::NoPen);
                    g.setBrush (QColor (255, 255, 255, 190));
                    g.drawPolygon (tri, 3);
                }
                g.setPen (QColor (15, 15, 15));
                g.drawText (r, Qt::AlignCenter, c.chordSize() > 1 ? noteName (c.note) + QString ("+%1").arg (c.chordSize() - 1)
                                                                : noteName (c.note));
            }
            else
                g.fillRect (r, QColor (20, 20, 25));
            // Ou les notes vont arriver : pad depose, ou selection deplacee
            bool ghost = ! moving && ! banding && row == targetRow && t == targetTrack;   // pad
            bool blocked = false;
            if (moving && ! banding && targetRow >= 0)
            {
                const int dr = targetRow - dragRow, dt = targetTrack - dragTrack;
                ghost = selected.contains (key (row - dr, t - dt)) && (dr || dt);
                blocked = ! canShift (dr, dt);
            }
            if (ghost)
            {
                QColor shade = blocked ? QColor (200, 40, 40) : color;
                shade.setAlphaF (0.45);
                g.fillRect (r, shade);
                g.setPen (QPen (Qt::white, 1.5, Qt::DashLine));
                g.setBrush (Qt::NoBrush);
                g.drawRect (r);
            }
            if (selected.contains (key (row, t)))      // note choisie : cadre clair
            {
                g.setPen (QPen (QColor (255, 255, 255, 200), 2));
                g.setBrush (Qt::NoBrush);
                g.drawRect (r.adjusted (1, 1, -1, -1));
            }
            if (row == selRow && t == selTrack)       // case choisie
            {
                g.setPen (QPen (Qt::white, 1.5));
                g.setBrush (Qt::NoBrush);
                g.drawRect (r);
            }
        }
    }
    if (! frameRect.isNull() && ! (banding && moving))  // zone tracee : reste visible (remplissage)
    {
        const int r0 = frameRect.top() - first, r1 = frameRect.bottom() - first;
        g.setPen (QPen (QColor (255, 255, 255, 110), 1, Qt::DotLine));
        g.setBrush (QColor (255, 255, 255, 10));
        g.drawRect (QRectF (kRowLabel + frameRect.left() * cw, kHeader + r0 * rh, frameRect.width() * cw, (r1 - r0 + 1) * rh));
    }
    if (banding && moving && targetRow >= 0)             // cadre de selection
    {
        const int r0 = std::min (bandRow, targetRow) - first, r1 = std::max (bandRow, targetRow) - first;
        const int t0 = std::min (bandTrack, targetTrack), t1 = std::max (bandTrack, targetTrack);
        g.setPen (QPen (QColor (255, 255, 255, 160), 1, Qt::DashLine));
        g.setBrush (QColor (255, 255, 255, 18));
        g.drawRect (QRectF (kRowLabel + t0 * cw, kHeader + r0 * rh, (t1 - t0 + 1) * cw, (r1 - r0 + 1) * rh));
    }
    if (p.length > visibleRows())                          // page affichee / longueur
    {
        g.setPen (QColor (120, 120, 128));
        g.drawText (QRectF (0, 0, kRowLabel + 4, kHeader), Qt::AlignCenter,
                    QString ("%1/%2").arg (first / visibleRows() + 1).arg ((p.length + visibleRows() - 1) / visibleRows()));
    }
}

bool PatternView::cellAt (const QPointF& pos, int& row, int& track) const
{
    const int length = sequencer.patternLength (state.group);
    const double cw = (width() - kRowLabel) / double (seq::kTracks);
    const double rh = (height() - kHeader) / double (visibleRows());
    const int t = (int) std::floor ((pos.x() - kRowLabel) / cw);
    const int i = (int) std::floor ((pos.y() - kHeader) / rh);
    if (t < 0 || t >= seq::kTracks || i < 0 || i >= std::min (visibleRows(), length))
        return false;
    row = firstRow (length) + i;
    track = t;
    return true;
}

QString PatternView::copyKey()
{
#ifdef Q_OS_MACOS
    return QStringLiteral ("\u2325");                // touche Option
#else
    return QStringLiteral ("Alt");
#endif
}

QList<QPoint> PatternView::selection() const
{
    QList<QPoint> out;
    for (int k : selected)
        out.append ({ k % seq::kTracks, k / seq::kTracks });
    return out;
}

void PatternView::setSelection (const QList<QPoint>& cells)
{
    selected.clear();
    for (const QPoint& c : cells)
        selected.insert (key (c.y(), c.x()));
    update();
}

bool PatternView::canShift (int rows, int tracks) const
{
    const int length = sequencer.patternLength (state.group);
    for (int k : selected)
    {
        const int r = k / seq::kTracks + rows, t = k % seq::kTracks + tracks;
        if (r < 0 || r >= length || t < 0 || t >= seq::kTracks)
            return false;
    }
    return true;
}

void PatternView::mousePressEvent (QMouseEvent* e)
{
    if (e->button() == Qt::RightButton || (e->modifiers() & Qt::MetaModifier))
        return;                                      // clic secondaire : contextMenuEvent
    if (e->position().y() < kHeader && e->position().x() > kRowLabel)   // en-tete : mute / solo
    {
        const double cw = (width() - kRowLabel) / double (seq::kTracks);
        const int t = (int) ((e->position().x() - kRowLabel) / cw);
        if (t >= 0 && t < seq::kTracks)
            emit trackClicked (t, e->modifiers() & Qt::ShiftModifier);
        return;
    }
    int row = 0, track = 0;
    if (! cellAt (e->position(), row, track))
        return;
    selRow = row;
    selTrack = track;
    pressPos = e->position();
    moving = banding = false;
    if (! (e->modifiers() & Qt::ShiftModifier))
        frameRect = {};
    emit cellClicked (track);
    dragRow = dragTrack = -1;
    const bool full = ! sequencer.pattern (state.group).rows[(size_t) row][(size_t) track].empty();
    const bool shift = e->modifiers() & Qt::ShiftModifier;
    if (full && shift)                                   // Shift + clic : ajouter / retirer la note
    {
        if (selected.contains (key (row, track)))
            selected.remove (key (row, track));
        else
            selected.insert (key (row, track));
    }
    else if (full)                                       // note : on pourra glisser la selection
    {
        if (! selected.contains (key (row, track)))
            selected = { key (row, track) };
        dragRow = row;
        dragTrack = track;
    }
    else                                                 // case vide : cadre de selection
    {
        bandBase = shift ? selected : QSet<int>();
        selected = bandBase;
        bandRow = row;
        bandTrack = track;
        banding = true;
    }
    update();
}

void PatternView::mouseMoveEvent (QMouseEvent* e)
{
    if (dragRow < 0 && ! banding)
        return;
    if (! moving && (e->position() - pressPos).manhattanLength() < QApplication::startDragDistance())
        return;
    moving = true;
    int row = 0, track = 0;
    if (banding)
    {
        // Cadre : toutes les notes entre le coin de depart et la souris
        const QPointF p (std::clamp (e->position().x(), kRowLabel + 1, width() - 1.0),
                         std::clamp (e->position().y(), kHeader + 1, height() - 1.0));
        if (! cellAt (p, row, track))
            return;
        const seq::Pattern pat = sequencer.pattern (state.group);
        selected = bandBase;
        for (int r = std::min (row, bandRow); r <= std::max (row, bandRow); ++r)
            for (int t = std::min (track, bandTrack); t <= std::max (track, bandTrack); ++t)
                if (! pat.rows[(size_t) r][(size_t) t].empty())
                    selected.insert (key (r, t));
        targetRow = row;
        targetTrack = track;
        frameRect = QRect (QPoint (std::min (track, bandTrack), std::min (row, bandRow)),
                           QPoint (std::max (track, bandTrack), std::max (row, bandRow)));
    }
    else
    {
        if (! cellAt (e->position(), targetRow, targetTrack))
            targetRow = targetTrack = -1;
        setCursor (e->modifiers() & Qt::AltModifier ? Qt::DragCopyCursor : Qt::ClosedHandCursor);
    }
    update();
}

void PatternView::mouseReleaseEvent (QMouseEvent* e)
{
    if (moving && ! banding && targetRow >= 0)
    {
        const int dr = targetRow - dragRow, dt = targetTrack - dragTrack;
        if ((dr || dt) && canShift (dr, dt))
        {
            emit cellsMoved (selection(), dr, dt, e->modifiers() & Qt::AltModifier);
            QSet<int> moved;                             // la selection suit les notes
            for (int k : selected)
                moved.insert (key (k / seq::kTracks + dr, k % seq::kTracks + dt));
            selected = moved;
            selRow = targetRow;
            selTrack = targetTrack;
        }
    }
    dragRow = dragTrack = targetRow = targetTrack = -1;
    moving = banding = false;
    unsetCursor();
    update();
}

void PatternView::dragEnterEvent (QDragEnterEvent* e)
{
    if (! session::midiFileIn (e->mimeData()->urls()).isEmpty())   // fichier MIDI
    {
        e->acceptProposedAction();
        return;
    }
    if (e->mimeData()->hasFormat (kPadMime))
        e->acceptProposedAction();
}

void PatternView::dragMoveEvent (QDragMoveEvent* e)
{
    if (! session::midiFileIn (e->mimeData()->urls()).isEmpty())
    {
        e->acceptProposedAction();
        return;
    }
    int row = 0, track = 0;
    if (! e->mimeData()->hasFormat (kPadMime) || ! cellAt (e->position(), row, track))
    {
        targetRow = -1;
        e->ignore();
        update();
        return;
    }
    targetRow = row;                                     // la ligne visee, dans la piste du pad
    targetTrack = e->mimeData()->data (kPadMime).toInt() - 1;
    e->acceptProposedAction();
    update();
}

void PatternView::dragLeaveEvent (QDragLeaveEvent*)
{
    targetRow = targetTrack = -1;
    update();
}

void PatternView::dropEvent (QDropEvent* e)
{
    const QString midiPath = session::midiFileIn (e->mimeData()->urls());
    if (! midiPath.isEmpty())                        // fichier MIDI : sur le son de la colonne visee
    {
        const double cw = (width() - kRowLabel) / double (seq::kTracks);
        const int t = std::clamp ((int) ((e->position().x() - kRowLabel) / cw), 0, seq::kTracks - 1);
        e->acceptProposedAction();
        emit midiDropped (midiPath, t + 1);
        return;
    }
    int row = 0, track = 0;
    if (e->mimeData()->hasFormat (kPadMime) && cellAt (e->position(), row, track))
    {
        const int pad = e->mimeData()->data (kPadMime).toInt();
        emit padDropped (pad, row);
        selRow = row;
        selTrack = pad - 1;
        e->acceptProposedAction();
    }
    targetRow = targetTrack = -1;
    update();
}

void PatternView::wheelEvent (QWheelEvent* e)
{
    const int length = sequencer.patternLength (state.group);
    if (length <= visibleRows() || playing)
        return;
    scroll = std::clamp (scroll + (e->angleDelta().y() < 0 ? visibleRows() : -visibleRows()), 0, length - visibleRows());
    update();
}

void PatternView::contextMenuEvent (QContextMenuEvent* e)
{
    int row = 0, track = 0;
    if (e->pos().y() >= kHeader && cellAt (e->pos(), row, track))   // grille : menu de la case
    {
        if (! selected.contains (key (row, track)))
        {
            selRow = row;
            selTrack = track;
            update();
        }
        emit cellMenu (row, track, e->globalPos());
        return;
    }
    // Clic droit sur le numero d'une piste : solo (jaune)
    if (e->pos().y() >= kHeader || e->pos().x() <= kRowLabel)
        return;
    const double cw = (width() - kRowLabel) / double (seq::kTracks);
    const int t = (int) ((e->pos().x() - kRowLabel) / cw);
    if (t >= 0 && t < seq::kTracks)
        emit trackClicked (t, true);
}

void PatternView::moveCursor (int rows, int tracks)
{
    const int length = sequencer.patternLength (state.group);
    selRow = std::clamp (selRow + rows, 0, length - 1);
    selTrack = std::clamp (selTrack + tracks, 0, seq::kTracks - 1);
    if (length > visibleRows())                      // la page suit le curseur
    {
        const int first = firstRow (length);
        if (selRow < first)
            scroll = selRow;
        else if (selRow >= first + visibleRows())
            scroll = selRow - visibleRows() + 1;
    }
    update();
}

void PatternView::mouseDoubleClickEvent (QMouseEvent* e)
{
    int row = 0, track = 0;
    if (e->button() == Qt::LeftButton && cellAt (e->position(), row, track))
        emit cellDoubleClicked (row, track);
}
