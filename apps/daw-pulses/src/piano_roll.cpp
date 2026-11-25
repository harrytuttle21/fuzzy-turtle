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
    return std::max (1, (notesBottom() - kHeader) / rowPx);
}

void PianoRollView::setZoomX (int percent)
{
    zoomPercent = std::clamp (percent, 100, 1600);
    setScrollX (firstRowShown);
}

void PianoRollView::setRowHeight (int px)
{
    const int middle = topPitch - visiblePitches() / 2;     // garder le milieu en place
    rowPx = std::clamp (px, 3, 30);
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

QRectF PianoRollView::noteBox (int r, const seq::Cell& c, int pitch) const
{
    const double cw = colWidth();
    return QRectF (xOfRow (r + c.offset / double (seq::kTiming)) + 1, yOfPitch (pitch) + 1,
                   std::max (4.0, c.length * cw - c.offset / double (seq::kTiming) * cw - 2), rowPx - 2);
}

QList<QPoint> PianoRollView::selection() const
{
    QList<QPoint> out;
    for (int k : sel)
        out << QPoint (k / 128, k % 128);
    return out;
}

void PianoRollView::selectAll()
{
    const seq::Pattern pat = sequencer.pattern (state.group);
    sel.clear();
    for (int r = 0; r < pat.length; ++r)
        for (int p : notesOf (pat.rows[(size_t) r][(size_t) track()]))
            sel.insert (key (r, p));
    update();
}

int PianoRollView::deleteSelection()
{
    if (sel.isEmpty())
        return 0;
    emit aboutToEdit();
    const int n = (int) sel.size();
    for (int k : sel)
        removeNote (k / 128, k % 128);
    sel.clear();
    emit edited();
    update();
    return n;
}

int PianoRollView::transpose (int step)
{
    QList<int> keys = sel.values();
    if (keys.isEmpty() && lastNote >= 0)
        keys << lastNote;
    // la note doit encore exister
    const seq::Pattern pat = sequencer.pattern (state.group);
    keys.erase (std::remove_if (keys.begin(), keys.end(), [&] (int k)
    {
        return k / 128 >= pat.length || ! notesOf (pat.rows[(size_t) (k / 128)][(size_t) track()]).contains (k % 128);
    }), keys.end());
    if (keys.isEmpty())
        return 0;
    emit aboutToEdit();
    const bool hadSel = ! sel.isEmpty();
    if (! moveNotes (keys, 0, step, false))
        return 0;
    if (! hadSel)
    {
        lastNote = keys[0] + step;
        sel.clear();
    }
    emit edited();
    if (keys.size() == 1)
        emit keyPressed (keys[0] % 128 + step), emit keyReleased (keys[0] % 128 + step);
    return (int) keys.size();
}

bool PianoRollView::moveNotes (const QList<int>& keys, int dr, int dp, bool copy)
{
    // Toutes les notes bougent ensemble ; si une sortait du pattern ou du clavier : rien ne bouge
    seq::Pattern pat = sequencer.pattern (state.group);
    const int t = track();
    struct N { int row, pitch; seq::Cell from; };
    QList<N> list;
    for (int k : keys)
    {
        const int r = k / 128, p = k % 128;
        if (r + dr < 0 || r + dr >= pat.length || p + dp < 0 || p + dp > 127)
            return false;
        list << N { r, p, pat.rows[(size_t) r][(size_t) t] };
    }
    const seq::Pattern before = pat;
    if (! copy)
        for (const N& n : list)
        {
            seq::Cell& c = pat.rows[(size_t) n.row][(size_t) t];
            QList<int> notes = notesOf (c);
            notes.removeAll (n.pitch);
            c = withNotes (c, notes);
        }
    QSet<int> placed;
    for (const N& n : list)
    {
        seq::Cell& to = pat.rows[(size_t) (n.row + dr)][(size_t) t];
        QList<int> notes = notesOf (to);
        if (to.empty())
            to = { n.pitch + dp, n.from.velocity, n.from.length, dr == 0 ? n.from.offset : 0 };
        if (! notes.contains (n.pitch + dp) && notes.size() <= seq::kChordExtra)
            notes << n.pitch + dp;
        to = withNotes (to, notes);
        placed.insert (key (n.row + dr, n.pitch + dp));
    }
    auto same = [] (const seq::Cell& a, const seq::Cell& b)
    {
        return a.note == b.note && a.extra == b.extra && a.velocity == b.velocity && a.length == b.length && a.offset == b.offset;
    };
    for (int r = 0; r < pat.length; ++r)
        if (! same (pat.rows[(size_t) r][(size_t) t], before.rows[(size_t) r][(size_t) t]))
            sequencer.setCell (state.group, r, t, pat.rows[(size_t) r][(size_t) t]);
    if (! sel.isEmpty())
        sel = placed;                                // la selection suit les notes
    update();
    return true;
}

void PianoRollView::paintEvent (QPaintEvent*)
{
    QPainter g (this);
    const seq::Pattern pat = sequencer.pattern (state.group);
    const QColor color = session::groupColor (state.group);
    if (shownSound != state.sound || shownGroup != state.group)   // autre son : on se place sur ses notes
    {
        shownSound = state.sound;
        shownGroup = state.group;
        sel.clear();
        lastNote = -1;
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
    for (int pitch = topPitch; yOfPitch (pitch) < notesBottom(); --pitch)
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
        g.drawLine (QPointF (xOfRow (r), kHeader), QPointF (xOfRow (r), notesBottom()));
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
    g.setClipRect (QRectF (kKeys, kHeader, width() - kKeys, notesBottom() - kHeader));
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
            const bool chosen = sel.contains (key (r, pitch));
            g.setPen (chosen ? QPen (Qt::white, 2) : QPen (QColor (255, 255, 255, 90)));
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
    // Notes deplacees : leur nouvelle place
    if (drag == Drag::Move && moved)
    {
        const int dr = targetRow - dragRow, dp = targetPitch - dragPitch;
        g.setPen (QPen (Qt::white, 1.5, Qt::DashLine));
        g.setBrush (QColor (255, 255, 255, 40));
        const QSet<int> keys = sel.contains (key (dragRow, dragPitch)) ? sel : QSet<int> { key (dragRow, dragPitch) };
        for (int k : keys)
        {
            const seq::Cell& c = pat.rows[(size_t) (k / 128)][(size_t) track()];
            g.drawRect (QRectF (xOfRow (k / 128 + dr) + 1, yOfPitch (k % 128 + dp) + 1, std::max (4.0, c.length * cw - 2), rowPx - 2));
        }
    }
    if (banding && ! bandRect.isNull())               // cadre de selection
    {
        g.setPen (QPen (QColor (255, 255, 255, 160), 1, Qt::DashLine));
        g.setBrush (QColor (255, 255, 255, 18));
        g.drawRect (bandRect);
    }
    // Tete de lecture
    if (playing && playRow >= 0)
    {
        const double x = xOfRow (playRow % std::max (1, pat.length));
        g.fillRect (QRectF (x, kHeader, std::max (2.0, cw), height()), QColor (255, 255, 255, 30));
    }
    // Barres de velocite (une par case : un accord a une seule velocite) ; tirer = regler
    if (laneH() > 0)
    {
        g.setClipping (false);
        const double top = notesBottom();
        g.fillRect (QRectF (0, top, width(), laneH()), QColor (14, 14, 17));
        g.setPen (QColor (60, 60, 70));
        g.drawLine (QPointF (0, top), QPointF (width(), top));
        g.setPen (QColor (140, 140, 150));
        QFont lf = font();
        lf.setPixelSize (10);
        g.setFont (lf);
        g.drawText (QRectF (0, top, kKeys - 4, laneH()), Qt::AlignCenter, "VEL");
        g.setClipRect (QRectF (kKeys, top, width() - kKeys, laneH()));
        for (int r = 0; r < pat.length; ++r)
        {
            const seq::Cell& c = pat.rows[(size_t) r][(size_t) track()];
            if (c.empty())
                continue;
            bool chosen = false;
            for (int pitch : notesOf (c))
                chosen |= sel.contains (key (r, pitch));
            const double x = xOfRow (r + c.offset / double (seq::kTiming));
            const double h = (laneH() - 8) * c.velocity / 127.0;
            const double w = std::clamp (cw * 0.5, 3.0, 10.0);
            g.fillRect (QRectF (x + 1, top + laneH() - 3 - h, w, h), chosen ? QColor (252, 140, 40) : color);
        }
        g.setClipping (false);
    }
}

void PianoRollView::setVelocityAt (const QPointF& p)
{
    // La case dont la barre est la plus proche de la souris (sur la meme ligne de temps)
    const seq::Pattern pat = sequencer.pattern (state.group);
    const int r = (int) std::floor (rowAt (p.x()));
    int best = -1;
    for (int d : { 0, -1, 1 })
        if (r + d >= 0 && r + d < pat.length && ! pat.rows[(size_t) (r + d)][(size_t) track()].empty())
        {
            best = r + d;
            break;
        }
    if (best < 0)
        return;
    seq::Cell c = pat.rows[(size_t) best][(size_t) track()];
    const int v = std::clamp ((int) std::lround ((height() - 3 - p.y()) / (laneH() - 8) * 127.0), 1, 127);
    // Barre d'une note choisie : toutes les notes choisies prennent cette velocite
    bool inSel = false;
    for (int pitch : notesOf (c))
        inSel |= sel.contains (key (best, pitch));
    if (inSel)
    {
        QSet<int> rows;
        for (int k : sel)
            rows.insert (k / 128);
        for (int row : rows)
        {
            seq::Cell o = pat.rows[(size_t) row][(size_t) track()];
            if (o.empty() || o.velocity == v)
                continue;
            o.velocity = v;
            sequencer.setCell (state.group, row, track(), o);
        }
    }
    else if (c.velocity != v)
    {
        c.velocity = v;
        sequencer.setCell (state.group, best, track(), c);
    }
    update();
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
    if (laneH() > 0 && p.y() >= notesBottom())       // barres de velocite
    {
        emit aboutToEdit();
        laneDrag = true;
        setVelocityAt (p);
        return;
    }
    int row = 0, pitch = 0;
    bool edge = false;
    const bool shift = e->modifiers() & Qt::ShiftModifier;
    if (noteAt (p, row, pitch, edge) && shift && selectOn)   // Shift + clic : ajouter / retirer de la selection
    {
        const int k = key (row, pitch);
        sel.contains (k) ? (void) sel.remove (k) : (void) sel.insert (k);
        lastNote = k;
        update();
        return;
    }
    if (selectOn && ! noteAt (p, row, pitch, edge))   // dans le vide : cadre si on glisse
    {
        banding = true;
        bandBase = shift ? sel : QSet<int> {};
        if (! shift)
            sel.clear();
        bandRect = {};
        pendingNote = editOn && ! shift;             // Edit : un simple clic posera une note
        update();
        return;
    }
    if (noteAt (p, row, pitch, edge))
    {
        if (! sel.contains (key (row, pitch)))       // autre note : la selection repart d'elle
        {
            sel.clear();
            if (selectOn)
                sel.insert (key (row, pitch));
        }
        lastNote = key (row, pitch);
        dragRow = targetRow = row;
        dragPitch = targetPitch = pitch;
        newLength = sequencer.pattern (state.group).rows[(size_t) row][(size_t) track()].length;
        drag = edge ? Drag::Resize : Drag::Move;
        return;
    }
    // Case vide (ou accord a completer) : nouvelle note ; tirer = sa duree (mode Edit)
    sel.clear();
    if (! editOn)
    {
        update();
        return;
    }
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
    lastNote = key (r, y);
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
    if (laneDrag)
    {
        setVelocityAt (p);
        return;
    }
    if (banding && pendingNote && (p - pressPos).manhattanLength() < 5)
        return;                                      // pas encore un glisser
    pendingNote = false;
    if (banding)
    {
        bandRect = QRectF (pressPos, p).normalized();
        const seq::Pattern pat = sequencer.pattern (state.group);
        sel = bandBase;
        for (int r = 0; r < pat.length; ++r)
        {
            const seq::Cell& c = pat.rows[(size_t) r][(size_t) track()];
            for (int pitch : notesOf (c))
                if (noteBox (r, c, pitch).intersects (bandRect))
                    sel.insert (key (r, pitch));
        }
        update();
        return;
    }
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
    if (laneDrag)
    {
        laneDrag = false;
        emit edited();
        return;
    }
    if (pendingNote)                                 // clic simple dans le vide (Edit) : une note
    {
        pendingNote = false;
        banding = false;
        bandRect = {};
        const int r = std::clamp ((int) rowAt (pressPos.x()), 0, sequencer.patternLength (state.group) - 1);
        const int y = pitchAt (pressPos.y());
        seq::Cell c = sequencer.pattern (state.group).rows[(size_t) r][(size_t) track()];
        QList<int> notes = notesOf (c);
        if (notes.size() <= seq::kChordExtra && ! notes.contains (y))
        {
            emit aboutToEdit();
            if (c.empty())
                c = { y, state.velocity.load(), 1, 0 };
            notes << y;
            sequencer.setCell (state.group, r, track(), withNotes (c, notes));
            lastNote = key (r, y);
            emit keyPressed (y);                     // on l'entend, brievement
            QTimer::singleShot (150, this, [this, y] { emit keyReleased (y); });
            emit edited();
        }
        update();
        return;
    }
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
    else if (drag == Drag::Move && moved && (targetRow != dragRow || targetPitch != dragPitch)
             && sel.size() > 1 && sel.contains (key (dragRow, dragPitch)))
    {
        emit aboutToEdit();                          // toute la selection bouge
        moveNotes (sel.values(), targetRow - dragRow, targetPitch - dragPitch, e->modifiers() & Qt::AltModifier);
        emit edited();
    }
    else if (drag == Drag::Move && moved && (targetRow != dragRow || targetPitch != dragPitch))
    {
        const seq::Cell from = sequencer.pattern (state.group).rows[(size_t) dragRow][(size_t) track()];
        lastNote = key (targetRow, targetPitch);
        if (sel.size() == 1)
            sel = { lastNote };
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
    banding = false;
    bandRect = {};
    unsetCursor();
    update();
}

void PianoRollView::mouseDoubleClickEvent (QMouseEvent* e)
{
    int row = 0, pitch = 0;
    bool edge = false;
    if (editOn && e->button() == Qt::LeftButton && noteAt (e->position(), row, pitch, edge))
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
    if (editOn && noteAt (e->pos(), row, pitch, edge))   // clic droit sur une note : l'effacer (mode Edit)
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
