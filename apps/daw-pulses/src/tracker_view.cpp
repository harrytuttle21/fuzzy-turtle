// Vue TRACKER : voir tracker_view.h
#include "tracker_view.h"
#include "pattern_view.h"

#include <QContextMenuEvent>
#include <QFontDatabase>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <algorithm>

using namespace session;

TrackerView::TrackerView (const seq::Sequencer& seqr, const session::State& st, PatternView& cur, QWidget* parent)
    : QWidget (parent), sequencer (seqr), state (st), cursor (cur)
{
    setMouseTracking (false);
    setFocusPolicy (Qt::NoFocus);                     // les touches passent par la fenetre
    setAttribute (Qt::WA_OpaquePaintEvent);
}

QString TrackerView::noteText (int note)
{
    static const char* names[] { "C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-" };
    if (note < 0)
        return "---";
    return QString ("%1%2").arg (names[note % 12]).arg (note / 12 - 1);
}

QFont TrackerView::mono() const
{
    QFont f = QFontDatabase::systemFont (QFontDatabase::FixedFont);
    f.setPointSizeF (std::max (9.0, font().pointSizeF()) * (single ? 1.3 : 1.0));   // une piste : plus gros
    return f;
}

int TrackerView::charW() const { return std::max (6, QFontMetrics (mono()).horizontalAdvance ('0')); }
int TrackerView::rowH() const { return QFontMetrics (mono()).height() + 3; }
int TrackerView::visibleRows() const { return std::max (1, (height() - headerH()) / rowH()); }
int TrackerView::visibleTracks() const { return single ? 1 : std::max (1, (width() - numW()) / trackW()); }

int TrackerView::firstRow() const
{
    // Comme les trackers : la ligne active reste au milieu de la vue
    const int length = sequencer.pattern (state.group).length;
    const int active = playing && follow ? playRow % std::max (1, length) : cursor.selectedRow();   // chaque tour
    const int vis = visibleRows();
    if (length <= vis)
        return 0;
    return std::clamp (active - vis / 2, 0, length - vis);
}

void TrackerView::moveCursor (int rows, int fields)
{
    const int length = sequencer.pattern (state.group).length;
    int track = single ? soundTrack() : cursor.selectedTrack(), row = cursor.selectedRow();
    if (fields)
    {
        int pos = track * 3 + fieldIndex + fields;
        pos = single ? std::clamp (pos, track * 3, track * 3 + 2) : std::clamp (pos, 0, seq::kTracks * 3 - 1);
        track = pos / 3;
        fieldIndex = pos % 3;
    }
    row = ((row + rows) % length + length) % length;  // le haut et le bas se rejoignent
    cursor.setCurrentCell (row, track);
    update();
}

bool TrackerView::hit (const QPointF& p, int& row, int& track, int& f) const
{
    if (p.x() < numW())
        return false;
    const int first = single ? soundTrack() : firstTrack;
    const int t = first + int ((p.x() - numW()) / trackW());
    if (t >= seq::kTracks || (single && t != first))
        return false;
    const int inTrack = int ((p.x() - numW()) - (t - first) * trackW()) / charW();
    track = t;
    f = inTrack < colOf (1) ? Note : inTrack < colOf (2) ? Velocity : Length;
    if (p.y() < headerH())
    {
        row = -1;                                     // en-tete
        return true;
    }
    row = firstRow() + int ((p.y() - headerH()) / rowH());
    return row < sequencer.pattern (state.group).length;
}

void TrackerView::paintEvent (QPaintEvent*)
{
    QPainter g (this);
    g.fillRect (rect(), QColor (14, 14, 18));
    g.setFont (mono());
    const seq::Pattern pat = sequencer.pattern (state.group);
    const QColor color = groupColor (state.group);
    const int cw = charW(), rh = rowH(), tw = trackW(), nw = numW(), hh = headerH();
    const int curRow = cursor.selectedRow(), curTrack = single ? soundTrack() : cursor.selectedTrack();
    if (single)                                       // une seule piste : celle du son choisi
        firstTrack = soundTrack();

    // La piste du curseur reste visible (defilement horizontal)
    const int vt = visibleTracks();
    if (! single)
    {
        if (curTrack < firstTrack)
            firstTrack = curTrack;
        else if (curTrack >= firstTrack + vt)
            firstTrack = curTrack - vt + 1;
        firstTrack = std::clamp (firstTrack, 0, std::max (0, seq::kTracks - vt));
    }
    const int lastTrack = single ? firstTrack + 1 : std::min (seq::kTracks, firstTrack + vt + 1);

    QSet<int> chosen;
    for (const QPoint& c : cursor.selection())
        chosen.insert (c.y() * seq::kTracks + c.x());

    // En-tete : numero et nom du son ; Mute en rouge, Solo en jaune, inaudible en gris
    g.fillRect (QRect (0, 0, width(), hh), QColor (24, 24, 30));
    for (int t = firstTrack; t < lastTrack; ++t)
    {
        const int x = nw + (t - firstTrack) * tw;
        const char tag = state.soundTag (state.group, t + 1);
        QColor c = tag == 'M' ? QColor (224, 80, 80) : tag == 'S' ? QColor (232, 192, 64)
                 : tag == 'm' ? QColor (100, 100, 110) : color;
        if (t + 1 == state.sound)
            g.fillRect (QRect (x + 1, 1, tw - 2, hh - 2), color.darker (300));
        g.setPen (c);
        const QString name = t < names.size() ? names[t] : QString();
        g.drawText (QRect (x + cw / 2, 0, tw - cw, hh), Qt::AlignVCenter | Qt::AlignLeft,
                    QString ("%1 %2").arg (t + 1, 2, 10, QChar ('0')).arg (name).left (12));
    }

    // Lignes
    const int first = firstRow();
    const int vis = visibleRows();
    for (int i = 0; i <= vis; ++i)
    {
        const int r = first + i;
        if (r >= pat.length)
            break;
        const int y = hh + i * rh;
        QColor back = r % 16 == 0 ? QColor (34, 34, 44) : r % 4 == 0 ? QColor (26, 26, 34) : QColor (18, 18, 23);
        if (playing && r == playRow % std::max (1, pat.length))
            back = QColor (60, 52, 30);
        if (r == curRow)
            back = editOn ? QColor (70, 34, 30) : QColor (40, 44, 60);   // Edit : ligne rouge, comme les trackers
        g.fillRect (QRect (0, y, width(), rh), back);
        g.setPen (r % 4 == 0 ? QColor (220, 220, 230) : QColor (130, 130, 145));
        g.drawText (QRect (cw / 2, y, nw - cw, rh), Qt::AlignVCenter | Qt::AlignLeft, QString ("%1").arg (r, 3, 10, QChar ('0')));

        for (int t = firstTrack; t < lastTrack; ++t)
        {
            const int x = nw + (t - firstTrack) * tw;
            const seq::Cell& c = pat.rows[(size_t) r][(size_t) t];
            if (chosen.contains (r * seq::kTracks + t))
                g.fillRect (QRect (x + 1, y, tw - 2, rh), QColor (252, 140, 40, 70));
            // Champ du curseur : encadre (les chiffres en cours de saisie s'y affichent)
            if (r == curRow && t == curTrack)
            {
                const int fx = x + cw / 2 + colOf (fieldIndex) * cw;
                const int fw = (fieldIndex == Note ? 3 : fieldIndex == Velocity ? 3 : 2) * cw;
                g.fillRect (QRect (fx - 2, y + 1, fw + 4, rh - 2), editOn ? QColor (252, 140, 40) : QColor (110, 140, 220));
            }
            const bool on = ! c.empty();
            const bool curField = r == curRow && t == curTrack;
            auto put = [&] (int col, const QString& text, const QColor& ink, int f)
            {
                g.setPen (curField && f == fieldIndex ? QColor (15, 15, 18) : ink);
                g.drawText (QRect (x + cw / 2 + col * cw, y, text.size() * cw + 2, rh), Qt::AlignVCenter | Qt::AlignLeft, text);
            };
            const QString vel = on ? QString ("%1").arg (c.velocity, 3, 10, QChar (' ')) : QString ("...");
            const QString len = on ? QString ("%1").arg (std::min (c.length, 99), 2, 10, QChar ('0')) : QString ("..");
            QString noteStr = noteText (c.note);
            put (colOf (0), noteStr, on ? QColor (240, 240, 245) : QColor (70, 70, 82), Note);
            if (on && c.chordSize() > 1)               // accord : petit chiffre (nombre de notes)
            {
                g.setPen (color.lighter (130));
                g.drawText (QRect (x + cw / 2 + (colOf (0) + 3) * cw, y - 3, cw, rh), Qt::AlignTop | Qt::AlignLeft, QString::number (c.chordSize()));
            }
            put (colOf (1), curField && fieldIndex == Velocity && ! typed.isEmpty() ? typed.rightJustified (3, ' ') : vel,
                 on ? QColor (120, 200, 140) : QColor (60, 60, 72), Velocity);
            put (colOf (2), curField && fieldIndex == Length && ! typed.isEmpty() ? typed.rightJustified (2, ' ') : len,
                 on ? QColor (140, 170, 230) : QColor (60, 60, 72), Length);
            g.setPen (QColor (40, 40, 50));
            g.drawLine (x + tw - 1, y, x + tw - 1, y + rh);
        }
    }
    g.setPen (QColor (40, 40, 50));
    g.drawLine (nw - 1, 0, nw - 1, height());
}

void TrackerView::mousePressEvent (QMouseEvent* e)
{
    int row = 0, track = 0, f = 0;
    if (! hit (e->position(), row, track, f))
        return;
    if (row < 0)                                      // en-tete : mute (Shift = solo)
    {
        if (e->button() == Qt::LeftButton)
            emit trackClicked (track, e->modifiers() & Qt::ShiftModifier);
        return;
    }
    if (e->button() != Qt::LeftButton)
        return;
    fieldIndex = f;
    if (e->modifiers() & Qt::ShiftModifier)           // Shift + clic : etendre la selection
    {
        bandRow = cursor.selectedRow();
        bandTrack = cursor.selectedTrack();
    }
    else
    {
        bandRow = row;
        bandTrack = track;
        cursor.clearSelection();
    }
    banding = true;
    cursor.setCurrentCell (row, track);
    if (e->modifiers() & Qt::ShiftModifier)
        mouseMoveEvent (e);
    emit cellClicked (track);
    update();
}

void TrackerView::mouseMoveEvent (QMouseEvent* e)
{
    if (! banding || ! (e->buttons() & Qt::LeftButton))
        return;
    int row = 0, track = 0, f = 0;
    if (! hit (e->position(), row, track, f) || row < 0)
        return;
    // Cadre : toutes les notes entre la case de depart et celle-ci
    const seq::Pattern pat = sequencer.pattern (state.group);
    QList<QPoint> cells;
    for (int r = std::min (row, bandRow); r <= std::max (row, bandRow); ++r)
        for (int t = std::min (track, bandTrack); t <= std::max (track, bandTrack); ++t)
            if (! pat.rows[(size_t) r][(size_t) t].empty())
                cells << QPoint (t, r);
    if (row != bandRow || track != bandTrack)
        cursor.setSelection (cells);
    update();
}

void TrackerView::mouseReleaseEvent (QMouseEvent*)
{
    banding = false;
}

void TrackerView::mouseDoubleClickEvent (QMouseEvent* e)
{
    int row = 0, track = 0, f = 0;
    if (e->button() == Qt::LeftButton && hit (e->position(), row, track, f) && row >= 0)
        emit cellDoubleClicked (row, track, f);
}

void TrackerView::contextMenuEvent (QContextMenuEvent* e)
{
    int row = 0, track = 0, f = 0;
    if (! hit (e->pos(), row, track, f))
        return;
    if (row < 0)
    {
        emit trackClicked (track, true);               // en-tete : clic droit = solo
        return;
    }
    cursor.setCurrentCell (row, track);
    emit cellMenu (row, track, e->globalPos());
}

void TrackerView::wheelEvent (QWheelEvent* e)
{
    // (Option + molette : certains systemes envoient la molette en horizontal)
    const int steps = (e->angleDelta().y() ? e->angleDelta().y() : e->angleDelta().x()) / 120;
    if (! steps)
        return;
    int row = 0, track = 0, f = 0;
    if ((e->modifiers() & Qt::AltModifier) && hit (e->position(), row, track, f) && row >= 0)
    {
        emit valueWheel (row, track, f, steps);       // Option + molette : changer la valeur
        return;
    }
    moveCursor (-steps, 0);                           // molette : le curseur descend / monte
}
