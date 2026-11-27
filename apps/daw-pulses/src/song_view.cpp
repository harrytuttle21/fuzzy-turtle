// Vue Song (morceau) : voir song_view.h
#include "song_view.h"

#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

using namespace session;

namespace {
QColor sceneColor (int scene)                        // une couleur douce par scene
{
    return QColor::fromHsv ((scene * 47) % 360, 120, 200);
}

QString shortPattern (int g, int index)              // AP1
{
    return groupName (g) + QString ("P%1").arg (index + 1);
}
}

SongView::SongView (const seq::Sequencer& seqr, const session::State& st, QWidget* parent)
    : QWidget (parent), sequencer (seqr), state (st)
{
    setMouseTracking (true);
    setAcceptDrops (true);                           // scenes glissees depuis la vue Scenes
    setFocusPolicy (Qt::NoFocus);
    setMinimumHeight (kRulerH + kSongH + 3 * laneH);
}

void SongView::zoomV (int dir)
{
    laneH = std::clamp (laneH + (dir > 0 ? 4 : -4), 16, 64);
    update();
}

void SongView::leaveEvent (QEvent*)
{
    hoverLane = -1;
    update();
}

void SongView::zoom (int dir)
{
    barPx = std::clamp (barPx * (dir > 0 ? 1.25 : 0.8), 16.0, 320.0);
    update();
}

std::vector<int> SongView::startRows() const
{
    // Pendant qu'on tire le bord d'un bloc : sa nouvelle longueur, les suivants se decalent
    std::vector<int> rows { 0 };
    const auto song = sequencer.song();
    for (int i = 0; i < (int) song.size(); ++i)
        rows.push_back (rows.back() + (drag == Drag::Resize && i == pressEntry
                                           ? std::max (1, (int) std::lround (resizeBars * seq::kRowsPerBar))
                                           : sequencer.entryRows (song[(size_t) i])));
    return rows;
}

int SongView::entryAtX (double x) const
{
    const auto rows = startRows();
    const int n = (int) rows.size() - 1;
    for (int k = 0; k < n; ++k)
        if (x < xOfRow (rows[(size_t) k + 1]))
            return k;
    return n - 1;
}

void SongView::followPlayhead()
{
    if (! follow || ! (isPlaying && isPlaying()) || ! sequencer.songMode())
        return;
    int e = -1, r = 0;
    sequencer.songPosition (e, r);
    const auto rows = startRows();
    if (e < 0 || e >= (int) rows.size() - 1)
        return;
    const double x = xOfRow (rows[(size_t) e] + r);
    if (x > width() - 60 || x < kLabelW + 4)         // hors de vue : la page suit
    {
        scrollPx = std::max (0.0, scrollPx + x - (kLabelW + 40));
        update();
    }
}

std::vector<int> SongView::lanes() const
{
    // Les groupes qui jouent quelque chose (ou restent libres) dans au moins une scene du morceau
    // Les groupes A-H toujours (pour y mettre un pattern), plus ceux de A2-H2 qui jouent dans le morceau
    std::vector<int> out;
    const auto song = sequencer.song();
    for (int g = 0; g < kGroups; ++g)
    {
        bool show = g < 8;
        for (const auto& e : song)
            show |= sequencer.scene (e.scene).used && sequencer.scene (e.scene).pattern[(size_t) g] >= 0;
        if (show)
            out.push_back (g);
    }
    return out;
}

QRectF SongView::entryRect (int i) const
{
    const auto rows = startRows();
    if (i < 0 || i + 1 >= (int) rows.size())
        return {};
    const double x0 = xOfRow (rows[(size_t) i]), x1 = xOfRow (rows[(size_t) i + 1]);
    return QRectF (x0 + 1, kRulerH + 3, std::max (6.0, x1 - x0 - 2), kSongH - 6);
}

int SongView::entryAt (const QPointF& p) const
{
    const int n = (int) sequencer.song().size();
    for (int i = 0; i < n; ++i)
    {
        const QRectF r = entryRect (i);
        if (p.x() >= r.left() - 1 && p.x() <= r.right() + 1 && p.y() >= kRulerH)
            return i;
    }
    return -1;
}

int SongView::insertIndexAt (double x) const
{
    const auto rows = startRows();
    const int n = (int) rows.size() - 1;
    for (int i = 0; i < n; ++i)                      // avant le milieu d'un bloc : avant lui
        if (x < (xOfRow (rows[(size_t) i]) + xOfRow (rows[(size_t) i + 1])) / 2)
            return i;
    return n;
}

void SongView::paintEvent (QPaintEvent*)
{
    QPainter p (this);
    p.setRenderHint (QPainter::Antialiasing);
    p.fillRect (rect(), QColor (14, 14, 18));
    QFont f = font();
    f.setPixelSize (11);
    p.setFont (f);
    const auto song = sequencer.song();
    const auto rows = startRows();
    const auto groups = lanes();
    const int totalRows = rows.back();
    const double laneTop = kRulerH + kSongH;

    // Regle des mesures (un numero toutes les n mesures si elles sont etroites)
    p.fillRect (QRectF (0, 0, width(), kRulerH), QColor (24, 24, 30));
    // Zone de boucle (glissee sur la regle) : orange pale
    {
        int la = -1, lb = -1;
        sequencer.songLoopRange (la, lb);
        if (drag == Drag::Region)
        {
            la = std::min (regionA, regionB);
            lb = std::max (regionA, regionB);
        }
        if (la >= 0 && lb + 1 < (int) rows.size())
        {
            const double x0 = std::max<double> (kLabelW, xOfRow (rows[(size_t) la])), x1 = xOfRow (rows[(size_t) lb + 1]);
            p.fillRect (QRectF (x0, 2, x1 - x0, kRulerH - 4), QColor (245, 201, 160, sequencer.songLoop() ? 150 : 70));
            p.fillRect (QRectF (x0, kRulerH, x1 - x0, height() - kRulerH), QColor (245, 201, 160, 18));
        }
    }
    const int every = barPx >= 40 ? 1 : barPx >= 20 ? 2 : 4;
    const int bars = std::max (8, (int) std::ceil (totalRows / double (seq::kRowsPerBar)) + 4);
    for (int b = 0; b <= bars; ++b)
    {
        const double x = xOfRow (b * seq::kRowsPerBar);
        if (x < kLabelW || x > width())
            continue;
        p.setPen (QColor (44, 44, 54));
        p.drawLine (QPointF (x, kRulerH), QPointF (x, height()));
        if (b % every == 0)
        {
            p.setPen (QColor (225, 225, 232));
            p.drawText (QRectF (x + 3, 0, 40, kRulerH), Qt::AlignVCenter | Qt::AlignLeft, QString::number (b + 1));
        }
    }
    // Depart de la lecture (entree choisie par la regle)
    if (! song.empty())
    {
        const int from = std::clamp (sequencer.songFrom(), 0, (int) song.size() - 1);
        const double x = xOfRow (rows[(size_t) from]);
        if (x >= kLabelW)
        {
            p.setPen (Qt::NoPen);
            p.setBrush (QColor (252, 140, 40));
            p.drawPolygon (QPolygonF ({ QPointF (x, kRulerH - 2), QPointF (x - 6, kRulerH - 12), QPointF (x + 6, kRulerH - 12) }));
        }
    }

    // Colonne des noms
    p.fillRect (QRectF (0, kRulerH, kLabelW, height()), QColor (20, 20, 25));
    QFont bold = f;
    bold.setBold (true);
    p.setFont (bold);
    p.setPen (QColor (230, 230, 240));
    p.drawText (QRectF (8, kRulerH, kLabelW - 10, kSongH), Qt::AlignVCenter | Qt::AlignLeft, sequencer.songMode() ? tr ("SONG ▶") : tr ("SONG"));
    for (size_t k = 0; k < groups.size(); ++k)
    {
        const int g = groups[k];
        p.setPen (groupColor (g));
        p.drawText (QRectF (8, laneTop + k * laneH, kLabelW - 10, laneH), Qt::AlignVCenter | Qt::AlignLeft,
                    p.fontMetrics().elidedText (groupTitle (state, g), Qt::ElideRight, kLabelW - 12));
    }
    p.setFont (f);
    p.save();
    p.setClipRect (QRectF (kLabelW, kRulerH, width() - kLabelW, height() - kRulerH));

    // Ligne SONG : un bloc par entree
    int playEntry = -1, playRow = 0;
    const bool playing = isPlaying && isPlaying() && sequencer.songMode();
    if (playing)
        sequencer.songPosition (playEntry, playRow);
    for (int i = 0; i < (int) song.size(); ++i)
    {
        const auto& e = song[(size_t) i];
        const QRectF r = entryRect (i);              // (pendant un etirement : les suivants decales)
        const seq::Scene sc = sequencer.scene (e.scene);
        QColor c = sc.used ? sceneColor (e.scene) : QColor (70, 70, 78);
        const bool now = playing && i == playEntry;
        p.setPen (i == selected ? QPen (Qt::white, 2) : QPen (c.darker (160), 1));
        p.setBrush (now ? c : c.darker (170));
        p.drawRoundedRect (r, 5, 5);
        p.setPen (now ? QColor (15, 15, 18) : QColor (240, 240, 245));
        const QString name = QString ("S%1 %2").arg (e.scene + 1).arg (QString::fromStdString (sc.name)).trimmed();
        const QString len = e.bars > 0 ? QString ("%1 bars").arg (e.bars) : QString::fromUtf8 ("×%1").arg (e.repeats);
        p.drawText (r.adjusted (6, 2, -4, -r.height() / 2), Qt::AlignLeft | Qt::AlignVCenter,
                    p.fontMetrics().elidedText (sc.used ? name : name + tr (" (empty)"), Qt::ElideRight, (int) r.width() - 10));
        p.drawText (r.adjusted (6, r.height() / 2, -4, -2), Qt::AlignLeft | Qt::AlignVCenter,
                    p.fontMetrics().elidedText (len, Qt::ElideRight, (int) r.width() - 10));
    }

    // Une ligne par groupe : le pattern de chaque bloc, repete sur sa longueur
    for (size_t k = 0; k < groups.size(); ++k)
    {
        const int g = groups[k];
        const double y = laneTop + k * laneH;
        if (g == hoverLane)                          // groupe sous la souris : ligne eclaircie
            p.fillRect (QRectF (kLabelW, y, width() - kLabelW, laneH), QColor (255, 255, 255, 16));
        p.setPen (QColor (30, 30, 36));
        p.drawLine (QPointF (kLabelW, y + laneH), QPointF (width(), y + laneH));
        for (int i = 0; i < (int) song.size(); ++i)
        {
            const seq::Scene sc = sequencer.scene (song[(size_t) i].scene);
            if (! sc.used)
                continue;
            const int v = sc.pattern[(size_t) g];
            const int a = rows[(size_t) i], b = rows[(size_t) i + 1];
            if (v == seq::kFree)                     // libre : garde le pattern d'avant
            {
                p.setPen (QPen (groupColor (g).darker (200), 1, Qt::DotLine));
                p.setBrush (Qt::NoBrush);
                p.drawRect (QRectF (xOfRow (a) + 1, y + 4, xOfRow (b) - xOfRow (a) - 2, laneH - 8));
                continue;
            }
            if (v < 0)
                continue;                            // eteint : rien
            const int len = std::max (1, sequencer.patternAt (g, v).length);
            for (int s = a; s < b; s += len)         // une case par repetition du pattern
            {
                const double x0 = xOfRow (s), x1 = xOfRow (std::min (b, s + len));
                const QRectF r (x0 + 1, y + 3, std::max (2.0, x1 - x0 - 2), laneH - 6);
                p.setPen (Qt::NoPen);
                p.setBrush (groupColor (g).darker (s == a ? 140 : 200));
                p.drawRoundedRect (r, 3, 3);
                if (s == a)
                {
                    QString label = QString::fromStdString (sequencer.patternName (g, v));
                    label = label.isEmpty() ? shortPattern (g, v) : shortPattern (g, v) + " " + label;
                    p.setPen (QColor (240, 240, 245));
                    p.drawText (r.adjusted (4, 0, -2, 0), Qt::AlignVCenter | Qt::AlignLeft,
                                p.fontMetrics().elidedText (label, Qt::ElideRight, (int) r.width() - 6));
                }
            }
        }
    }

    // Tete de lecture
    if (playing && playEntry >= 0 && playEntry < (int) song.size())
    {
        const double x = xOfRow (rows[(size_t) playEntry] + playRow);
        p.setPen (QPen (QColor (252, 140, 40), 2));
        p.drawLine (QPointF (x, 0), QPointF (x, height()));
    }
    // Ou le bloc / la scene va arriver
    if (dropIndex >= 0)
    {
        const double x = xOfRow (rows[(size_t) std::min (dropIndex, (int) rows.size() - 1)]);
        p.setPen (QPen (Qt::white, 3));
        p.drawLine (QPointF (x, kRulerH), QPointF (x, kRulerH + kSongH));
    }
    p.restore();
    // Hauteur : toutes les lignes de groupes (la fenetre defile verticalement)
    const int need = kRulerH + kSongH + (int) groups.size() * laneH + 4;
    if (minimumHeight() != need)
        QTimer::singleShot (0, this, [this, need] { setMinimumHeight (need); });
    if (song.empty())
    {
        p.setPen (QColor (130, 130, 140));
        p.drawText (QRectF (kLabelW + 10, kRulerH, width() - kLabelW - 20, kSongH + 2 * laneH), Qt::AlignCenter,
                    tr ("Empty song: \"+ Scene\", or drag a scene name from the Scenes view, or right-click here"));
    }
}

void SongView::mousePressEvent (QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton)
        return;
    pressPos = e->position();
    if (pressPos.y() < kRulerH && pressPos.x() > kLabelW)   // regle : clic = jouer d'ici ; glisser = zone de boucle
    {
        if (sequencer.song().empty())
            return;
        int la = -1, lb = -1;                        // un bord de la zone : on le tire
        sequencer.songLoopRange (la, lb);
        const auto rows = startRows();
        edge = Edge::None;
        if (la >= 0 && lb + 1 < (int) rows.size())
        {
            if (std::abs (pressPos.x() - xOfRow (rows[(size_t) la])) <= 6)
                edge = Edge::Left;
            else if (std::abs (pressPos.x() - xOfRow (rows[(size_t) lb + 1])) <= 6)
                edge = Edge::Right;
        }
        if (edge != Edge::None)
        {
            drag = Drag::Region;
            regionA = la;
            regionB = lb;
            return;
        }
        drag = Drag::Ruler;
        regionA = regionB = entryAtX (pressPos.x());
        return;
    }
    if (pressPos.y() >= kRulerH + kSongH)            // une case groupe x bloc : son pattern dans la scene
    {
        const int i = entryAtX (pressPos.x());
        const auto groups = lanes();
        const int lane = (int) ((pressPos.y() - kRulerH - kSongH) / laneH);
        if (i >= 0 && pressPos.x() > kLabelW && lane >= 0 && lane < (int) groups.size()
            && pressPos.x() < xOfRow (startRows().back()))
        {
            selected = i;
            emit selectedChanged (i);
            update();
            emit cellClicked (i, groups[(size_t) lane], e->globalPosition().toPoint());
        }
        return;
    }
    pressEntry = entryAt (pressPos);
    if (pressEntry < 0)
        return;
    selected = pressEntry;
    emit selectedChanged (selected);
    const QRectF r = entryRect (pressEntry);
    const bool atEdge = pressPos.y() < kRulerH + kSongH && std::abs (pressPos.x() - r.right()) <= 6;
    drag = atEdge ? Drag::Resize : Drag::Pending;
    resizeBars = r.width() / barPx;
    update();
}

void SongView::mouseMoveEvent (QMouseEvent* e)
{
    const QPointF pos = e->position();
    if (drag == Drag::None)                          // le bord droit d'un bloc : curseur de taille
    {
        const int lane = pos.y() >= kRulerH + kSongH ? (int) ((pos.y() - kRulerH - kSongH) / laneH) : -1;
        const auto groups = lanes();
        const int over = lane >= 0 && lane < (int) groups.size() ? groups[(size_t) lane] : -1;
        if (over != hoverLane)
        {
            hoverLane = over;
            update();
        }
        const int i = entryAt (pos);
        const bool atEdge = i >= 0 && pos.y() >= kRulerH && pos.y() < kRulerH + kSongH && std::abs (pos.x() - entryRect (i).right()) <= 6;
        setCursor (atEdge ? Qt::SizeHorCursor : Qt::ArrowCursor);
        return;
    }
    if (drag == Drag::Ruler && std::abs (pos.x() - pressPos.x()) >= 6)
        drag = Drag::Region;
    if (drag == Drag::Region)
    {
        if (edge == Edge::Left)                      // bord gauche tire : le debut change
            regionA = std::min (entryAtX (pos.x() + 1), regionB);
        else if (edge == Edge::Right)                // bord droit : la fin
            regionB = std::max (entryAtX (pos.x() - 1), regionA);
        else
            regionB = entryAtX (pos.x());
        update();
        return;
    }
    if (drag == Drag::Resize)
    {
        const double raw = (pos.x() - entryRect (pressEntry).left()) / barPx;
        resizeBars = std::max (step, std::round (raw / step) * step);
        update();
        return;
    }
    if (drag == Drag::Pending && (pos - pressPos).manhattanLength() >= 8)
        drag = Drag::Move;
    if (drag == Drag::Move)
    {
        dropIndex = insertIndexAt (pos.x());
        update();
    }
}

void SongView::mouseReleaseEvent (QMouseEvent*)
{
    if (drag == Drag::Ruler)                         // simple clic : jouer d'ici (la zone reste)
        emit startFrom (regionA);
    else if (drag == Drag::Region)
        emit loopRegion (std::min (regionA, regionB), std::max (regionA, regionB));
    else if (drag == Drag::Resize && pressEntry >= 0)
        emit lengthChanged (pressEntry, resizeBars);
    else if (drag == Drag::Move && pressEntry >= 0 && dropIndex >= 0)
    {
        const int to = dropIndex > pressEntry ? dropIndex - 1 : dropIndex;   // place apres retrait
        if (to != pressEntry)
            emit moveEntry (pressEntry, to);
    }
    drag = Drag::None;
    edge = Edge::None;
    dropIndex = -1;
    pressEntry = -1;
    update();
}

void SongView::mouseDoubleClickEvent (QMouseEvent* e)
{
    const int i = entryAt (e->position());
    if (i >= 0 && e->position().y() < kRulerH + kSongH)
        emit chooseScene (i, e->globalPosition().toPoint());
}

void SongView::contextMenuEvent (QContextMenuEvent* e)
{
    if (e->pos().y() < kRulerH)                      // regle : effacer la zone de boucle
    {
        emit rulerMenu (e->globalPos());
        return;
    }
    const int i = entryAt (e->pos());
    if (i >= 0)
    {
        selected = i;
        emit selectedChanged (i);
        update();
        emit entryMenu (i, e->globalPos());
    }
    else
        emit emptyMenu (e->globalPos());
}

void SongView::wheelEvent (QWheelEvent* e)
{
    // Molette : avancer / reculer dans le morceau ; Cmd + molette : zoom
    const int d = e->angleDelta().y() ? e->angleDelta().y() : e->angleDelta().x();
    if (e->modifiers() & Qt::ControlModifier)
        zoom (d > 0 ? 1 : -1);
    else
        scrollPx = std::max (0.0, scrollPx - d / 120.0 * barPx);
    update();
}

namespace {
int sceneFromMime (const QMimeData* m)
{
    return m && m->hasFormat ("application/x-fuzzy-scene") ? m->data ("application/x-fuzzy-scene").toInt() : -1;
}
}

void SongView::dragEnterEvent (QDragEnterEvent* e)
{
    if (sceneFromMime (e->mimeData()) >= 0)
        e->acceptProposedAction();
}

void SongView::dragMoveEvent (QDragMoveEvent* e)
{
    dropIndex = insertIndexAt (e->position().x());
    e->acceptProposedAction();
    update();
}

void SongView::dragLeaveEvent (QDragLeaveEvent*)
{
    dropIndex = -1;
    update();
}

void SongView::dropEvent (QDropEvent* e)
{
    const int scene = sceneFromMime (e->mimeData());
    const int before = insertIndexAt (e->position().x());
    dropIndex = -1;
    update();
    if (scene >= 0)
    {
        e->acceptProposedAction();
        emit insertScene (scene, before);
    }
}
