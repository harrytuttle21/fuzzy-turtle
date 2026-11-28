// Vue Song (morceau) : voir song_view.h
#include "song_view.h"

#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QHelpEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QToolTip>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

using namespace session;

namespace {
QString shortPattern (int g, int index)              // AP1
{
    return groupName (g) + QString ("P%1").arg (index + 1);
}

int sceneFromMime (const QMimeData* m)
{
    return m && m->hasFormat ("application/x-fuzzy-scene") ? m->data ("application/x-fuzzy-scene").toInt() : -1;
}
}

SongView::SongView (const seq::Sequencer& seqr, const session::State& st, QWidget* parent)
    : QWidget (parent), sequencer (seqr), state (st)
{
    setMouseTracking (true);
    setAcceptDrops (true);                           // scenes glissees (liste, vue Scenes)
    setFocusPolicy (Qt::NoFocus);
    setMinimumHeight (kRulerH + kSongH + 3 * laneH);
}

QList<int> SongView::chosenList() const
{
    QList<int> out = chosen.values();
    const int n = (int) sequencer.song().size();
    out.erase (std::remove_if (out.begin(), out.end(), [n] (int i) { return i < 0 || i >= n; }), out.end());
    if (out.isEmpty() && selected >= 0 && selected < n)
        out << selected;
    std::sort (out.begin(), out.end());
    return out;
}

void SongView::selectOnly (int entry)
{
    selected = entry;
    chosen = { entry };
    update();
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
    emit zoomChanged();
}

QString SongView::timeOf (int rows) const
{
    const double tempo = bpm ? std::max (1.0, bpm()) : 120.0;
    const int secs = (int) std::lround (rows / 4.0 * 60.0 / tempo);   // 4 lignes = 1 temps
    return QString ("%1:%2").arg (secs / 60).arg (secs % 60, 2, 10, QChar ('0'));
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

QRectF SongView::muteBox (int lane, bool solo) const
{
    const double y = kRulerH + kSongH + lane * laneH;
    return QRectF (solo ? 26 : 4, y + (laneH - 16) / 2.0, 19, 16);   // a gauche, comme la colonne des groupes
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

void SongView::scrollFromOverview (double x)
{
    // L'apercu montre tout le morceau : on y centre la partie visible
    const auto rows = startRows();
    const double totalPx = std::max (1.0, rows.back() / double (seq::kRowsPerBar) * barPx);
    const QRectF ov = overview();
    const double visible = width() - kLabelW - 8;
    const double target = (x - ov.left()) / ov.width() * std::max (totalPx, visible) - visible / 2;
    scrollPx = std::clamp (target, 0.0, std::max (0.0, totalPx - visible / 2));
    update();
}

bool SongView::event (QEvent* e)
{
    if (e->type() == QEvent::ToolTip)                // infobulle : scene, debut, longueur, repetitions
    {
        auto* h = static_cast<QHelpEvent*> (e);
        const int i = entryAt (h->pos());
        const auto song = sequencer.song();
        if (i >= 0 && h->pos().y() < kRulerH + kSongH)
        {
            const auto rows = startRows();
            const seq::SongEntry& en = song[(size_t) i];
            const seq::Scene sc = sequencer.scene (en.scene);
            const int len = rows[(size_t) i + 1] - rows[(size_t) i];
            QToolTip::showText (h->globalPos(),
                QString ("<b>Block %1: S%2 %3</b><br>Starts bar %4 (%5)<br>Length %6 bars (%7)<br>%8")
                    .arg (i + 1).arg (en.scene + 1).arg (QString::fromStdString (sc.name).toHtmlEscaped())
                    .arg (rows[(size_t) i] / double (seq::kRowsPerBar) + 1).arg (timeOf (rows[(size_t) i]))
                    .arg (len / double (seq::kRowsPerBar)).arg (timeOf (len))
                    .arg (en.bars > 0 ? QString ("Fixed length") : QString ("Repeats x%1").arg (en.repeats)), this);
        }
        else
            QToolTip::hideText();
        return true;
    }
    return QWidget::event (e);
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

    // Apercu de tout le morceau (en haut) : les blocs en petit, le cadre = la partie visible
    {
        const QRectF ov = overview();
        p.fillRect (QRectF (0, 0, width(), kTop), QColor (18, 18, 23));
        const double totalPx = std::max (1.0, totalRows / double (seq::kRowsPerBar) * barPx);
        const double visible = width() - kLabelW - 8;
        const double span = std::max (totalPx, visible);
        for (int i = 0; i < (int) song.size(); ++i)
        {
            const double a = rows[(size_t) i] / double (seq::kRowsPerBar) * barPx, b = rows[(size_t) i + 1] / double (seq::kRowsPerBar) * barPx;
            const seq::Scene sc = sequencer.scene (song[(size_t) i].scene);
            p.fillRect (QRectF (ov.left() + a / span * ov.width(), ov.top() + 2, std::max (1.0, (b - a) / span * ov.width() - 1), ov.height() - 4),
                        sc.used ? seq::sceneColor (song[(size_t) i].scene, sc.color).darker (130) : QColor (70, 70, 78));
        }
        p.setPen (QPen (QColor (235, 235, 242), 1));
        p.setBrush (Qt::NoBrush);
        p.drawRect (QRectF (ov.left() + scrollPx / span * ov.width(), ov.top(), std::min (1.0, visible / span) * ov.width(), ov.height()));
        // Duree totale, a gauche
        p.setPen (QColor (200, 200, 210));
        p.drawText (QRectF (6, 0, kLabelW - 8, kTop), Qt::AlignVCenter | Qt::AlignLeft,
                    QString ("%1 bars · %2").arg (totalRows / double (seq::kRowsPerBar)).arg (timeOf (totalRows)));
    }

    // Regle des mesures (un numero toutes les n mesures si elles sont etroites, + le temps)
    p.fillRect (QRectF (0, kTop, width(), kRulerH - kTop), QColor (24, 24, 30));
    {
        int la = -1, lb = -1;                        // zone de boucle : orange pale
        sequencer.songLoopRange (la, lb);
        if (drag == Drag::Region)
        {
            la = std::min (regionA, regionB);
            lb = std::max (regionA, regionB);
        }
        if (la >= 0 && lb + 1 < (int) rows.size())
        {
            const double x0 = std::max<double> (kLabelW, xOfRow (rows[(size_t) la])), x1 = xOfRow (rows[(size_t) lb + 1]);
            p.fillRect (QRectF (x0, kTop + 2, x1 - x0, kRulerH - kTop - 4), QColor (245, 201, 160, sequencer.songLoop() ? 150 : 70));
            p.fillRect (QRectF (x0, kRulerH, x1 - x0, height() - kRulerH), QColor (245, 201, 160, 18));
        }
    }
    const int every = barPx >= 40 ? 1 : barPx >= 20 ? 2 : 4;
    const bool showTime = barPx * every >= 64;
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
            p.drawText (QRectF (x + 3, kTop, 30, kRulerH - kTop), Qt::AlignVCenter | Qt::AlignLeft, QString::number (b + 1));
            if (showTime)
            {
                p.setPen (QColor (130, 130, 142));
                p.drawText (QRectF (x + 22, kTop, 50, kRulerH - kTop), Qt::AlignVCenter | Qt::AlignLeft, timeOf (b * seq::kRowsPerBar));
            }
        }
    }
    if (! song.empty())                              // depart de la lecture (choisi par la regle)
    {
        const double x = xOfRow (std::min (sequencer.songCursorRow(), rows.back()));   // le curseur (libre)
        if (x >= kLabelW)
        {
            p.setPen (Qt::NoPen);
            p.setBrush (QColor (252, 140, 40));
            p.drawPolygon (QPolygonF ({ QPointF (x, kRulerH - 2), QPointF (x - 6, kRulerH - 12), QPointF (x + 6, kRulerH - 12) }));
            p.setPen (QPen (QColor (252, 140, 40, 150), 1, Qt::DashLine));   // le curseur, sur toute la hauteur
            p.drawLine (QPointF (x, kRulerH), QPointF (x, height()));
        }
    }

    // Colonne des noms (M / S a droite de chaque groupe)
    p.fillRect (QRectF (0, kRulerH, kLabelW, height()), QColor (20, 20, 25));
    QFont bold = f;
    bold.setBold (true);
    p.setFont (bold);
    p.setPen (QColor (230, 230, 240));
    p.drawText (QRectF (8, kRulerH, kLabelW - 10, kSongH), Qt::AlignVCenter | Qt::AlignLeft, sequencer.songMode() ? tr ("SONG ▶") : tr ("SONG"));
    for (size_t k = 0; k < groups.size(); ++k)
    {
        const int g = groups[k];
        if (g == hoverLane)
            p.fillRect (QRectF (0, laneTop + k * laneH, kLabelW, laneH), QColor (255, 255, 255, 30));
        // Nom dans une case de la couleur du groupe (comme la colonne des groupes)
        const QColor c = state.groupAudible (g) ? groupColor (g) : QColor (90, 90, 98);
        const double nh = std::min (18.0, laneH - 2.0);
        const QRectF nameBox (48, laneTop + k * laneH + (laneH - nh) / 2.0, kLabelW - 52, nh);
        const bool chosen = g == state.group;            // groupe choisi : case pleine, cadre blanc
        p.setPen (chosen ? QPen (Qt::white, 2) : QPen (c.darker (200), 1));
        p.setBrush (chosen ? c : c.darker (330));
        p.drawRoundedRect (nameBox, 3, 3);
        p.setPen (chosen ? QColor (17, 17, 17) : c.lighter (130));
        p.setFont (bold);
        p.drawText (nameBox.adjusted (5, 0, -2, 0), Qt::AlignVCenter | Qt::AlignLeft,
                    p.fontMetrics().elidedText (groupTitle (state, g), Qt::ElideRight, (int) nameBox.width() - 8));
        p.setFont (f);
        for (const bool solo : { false, true })      // M rouge = muet, S jaune = solo (norme de l'app)
        {
            const bool on = solo ? state.groupSolo[(size_t) g].load() : state.groupMute[(size_t) g].load();
            const QRectF b = muteBox ((int) k, solo);
            p.setPen (on ? Qt::NoPen : QPen (QColor (70, 70, 80)));
            p.setBrush (on ? (solo ? QColor (232, 192, 64) : QColor (224, 80, 80)) : QColor (22, 22, 27));
            p.drawRoundedRect (b, 3, 3);
            p.setPen (on ? QColor (15, 15, 18) : QColor (110, 110, 120));
            p.drawText (b, Qt::AlignCenter, solo ? "S" : "M");
        }
    }
    p.setFont (f);
    p.save();
    p.setClipRect (QRectF (kLabelW, kRulerH, width() - kLabelW, height() - kRulerH));

    // Pendant un glisser de blocs : ils s'ecartent pour montrer ou ils vont tomber
    std::vector<double> shift (song.size() + 1, 0.0);
    double gapPx = 0;
    const QList<int> moving = drag == Drag::Move ? chosenList() : QList<int>();
    if (drag == Drag::Move && dropIndex >= 0)
    {
        for (int i : moving)
            gapPx += (rows[(size_t) i + 1] - rows[(size_t) i]) / double (seq::kRowsPerBar) * barPx;
        for (int i = 0; i < (int) song.size(); ++i)
        {
            double removed = 0;
            if (! copyDrag)
                for (int m : moving)
                    if (m < i)
                        removed += (rows[(size_t) m + 1] - rows[(size_t) m]) / double (seq::kRowsPerBar) * barPx;
            shift[(size_t) i] = (i >= dropIndex ? gapPx : 0) - removed;
        }
    }

    // Ligne SONG : un bloc par entree
    int playEntry = -1, playRow = 0;
    const bool playing = isPlaying && isPlaying() && sequencer.songMode();
    if (playing)
        sequencer.songPosition (playEntry, playRow);
    auto drawBlock = [&] (int i, QRectF r, bool ghost)
    {
        const auto& e = song[(size_t) i];
        const seq::Scene sc = sequencer.scene (e.scene);
        QColor c = sc.used ? seq::sceneColor (e.scene, sc.color) : QColor (70, 70, 78);
        const bool now = playing && i == playEntry;
        const bool picked = chosen.contains (i) || i == selected;
        p.setOpacity (ghost ? 0.6 : 1.0);
        p.setPen (picked ? QPen (Qt::white, 2) : QPen (c.darker (160), 1));
        p.setBrush (now ? c : c.darker (170));
        p.drawRoundedRect (r, 5, 5);
        p.setPen (now ? QColor (15, 15, 18) : QColor (240, 240, 245));
        const QString name = QString ("S%1 %2").arg (e.scene + 1).arg (QString::fromStdString (sc.name)).trimmed();
        const QString len = e.bars > 0 ? QString ("%1 bars").arg (e.bars) : QString::fromUtf8 ("×%1").arg (e.repeats);
        p.drawText (r.adjusted (6, 2, -4, -r.height() / 2), Qt::AlignLeft | Qt::AlignVCenter,
                    p.fontMetrics().elidedText (sc.used ? name : name + tr (" (empty)"), Qt::ElideRight, (int) r.width() - 10));
        p.drawText (r.adjusted (6, r.height() / 2, -4, -2), Qt::AlignLeft | Qt::AlignVCenter,
                    p.fontMetrics().elidedText (len, Qt::ElideRight, (int) r.width() - 10));
        p.setOpacity (1.0);
    };
    for (int i = 0; i < (int) song.size(); ++i)
    {
        if (drag == Drag::Move && dropIndex >= 0 && ! copyDrag && moving.contains (i))
            continue;                                // deplaces : dessines dans l'ecart
        drawBlock (i, entryRect (i).translated (shift[(size_t) i], 0), false);
    }
    if (drag == Drag::Move && dropIndex >= 0)        // les blocs deplaces, dans l'ecart
    {
        double x = xOfRow (rows[(size_t) std::min (dropIndex, (int) song.size())]) + shift[(size_t) std::min (dropIndex, (int) song.size())] - gapPx;
        if (dropIndex >= (int) song.size())
        {
            double removed = 0;
            if (! copyDrag)
                for (int m : moving)
                    removed += (rows[(size_t) m + 1] - rows[(size_t) m]) / double (seq::kRowsPerBar) * barPx;
            x = xOfRow (rows.back()) - removed;
        }
        for (int i : moving)
        {
            const double w = (rows[(size_t) i + 1] - rows[(size_t) i]) / double (seq::kRowsPerBar) * barPx;
            drawBlock (i, QRectF (x + 1, kRulerH + 3, std::max (6.0, w - 2), kSongH - 6), true);
            x += w;
        }
    }

    // Une ligne par groupe : le pattern de chaque bloc, repete sur sa longueur
    for (size_t k = 0; k < groups.size(); ++k)
    {
        const int g = groups[k];
        const double y = laneTop + k * laneH;
        if (g == hoverLane)                          // groupe sous la souris : ligne eclaircie
            p.fillRect (QRectF (kLabelW, y, width() - kLabelW, laneH), QColor (255, 255, 255, 30));
        p.setPen (QColor (30, 30, 36));
        p.drawLine (QPointF (kLabelW, y + laneH), QPointF (width(), y + laneH));
        if (drag == Drag::Move)
            p.setOpacity (0.35);                     // pendant un glisser : les lignes s'effacent
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
                // En lecture : le pattern qui joue (cette repetition) en vif avec un cadre blanc,
                // le reste du bloc en cours un peu moins, les autres blocs estompes
                const int at = a + playRow;
                const bool nowRep = playing && i == playEntry && at >= s && at < s + len;
                const bool nowBlock = playing && i == playEntry;
                p.setPen (nowRep ? QPen (Qt::white, 2) : QPen (Qt::NoPen));
                p.setBrush (nowRep ? groupColor (g) : groupColor (g).darker (playing ? (nowBlock ? 160 : 260)
                                                                                    : (s == a ? 140 : 200)));
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
        p.setOpacity (1.0);
    }

    // Tete de lecture
    if (playing && playEntry >= 0 && playEntry < (int) song.size())
    {
        const double x = xOfRow (rows[(size_t) playEntry] + playRow);
        p.setPen (QPen (QColor (252, 140, 40), 2));
        p.drawLine (QPointF (x, kTop), QPointF (x, height()));
    }
    // Une scene glissee depuis la liste : le trait d'insertion
    if (dropIndex >= 0 && drag != Drag::Move)
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
                    tr ("Empty song: drag a scene from the list on the left, or \"+ Scene\", or right-click here"));
    }
}

void SongView::mousePressEvent (QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton)
        return;
    pressPos = e->position();
    if (pressPos.y() < kTop)                         // apercu : aller a cet endroit du morceau
    {
        drag = Drag::Overview;
        scrollFromOverview (pressPos.x());
        return;
    }
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
    if (pressPos.y() >= kRulerH + kSongH)            // lignes des groupes
    {
        const auto groups = lanes();
        const int lane = (int) ((pressPos.y() - kRulerH - kSongH) / laneH);
        if (lane < 0 || lane >= (int) groups.size())
            return;
        if (pressPos.x() < kLabelW)                  // M / S du groupe
        {
            for (const bool solo : { false, true })
                if (muteBox (lane, solo).contains (pressPos))
                {
                    emit laneMuteSolo (groups[(size_t) lane], solo);
                    update();
                    return;
                }
            emit laneSelected (groups[(size_t) lane]);   // clic sur le nom : choisir le groupe
            return;
        }
        const int i = entryAtX (pressPos.x());       // une case groupe x bloc : son pattern dans la scene
        if (i >= 0 && pressPos.x() < xOfRow (startRows().back()))
        {
            selectOnly (i);
            emit selectedChanged (i);
            emit cellClicked (i, groups[(size_t) lane], e->globalPosition().toPoint());
        }
        return;
    }
    pressEntry = entryAt (pressPos);
    if (pressEntry < 0)
        return;
    const auto mods = e->modifiers();
    if (mods & Qt::ControlModifier)                  // Cmd + clic : ajouter / enlever ce bloc
    {
        if (chosen.isEmpty())
            chosen.insert (selected);
        chosen.contains (pressEntry) ? (void) chosen.remove (pressEntry) : (void) chosen.insert (pressEntry);
        selected = pressEntry;
    }
    else if (mods & Qt::ShiftModifier)               // Shift + clic : tous les blocs depuis le dernier
    {
        for (int k = std::min (selected, pressEntry); k <= std::max (selected, pressEntry); ++k)
            chosen.insert (k);
        selected = pressEntry;
    }
    else if (! chosen.contains (pressEntry))          // clic sur un autre bloc : lui seul
        selectOnly (pressEntry);
    else
        selected = pressEntry;
    emit selectedChanged (selected);
    const QRectF r = entryRect (pressEntry);
    const bool atEdge = pressPos.y() < kRulerH + kSongH && std::abs (pressPos.x() - r.right()) <= 6
                        && ! (mods & (Qt::ControlModifier | Qt::ShiftModifier));
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
    if (drag == Drag::Overview)
    {
        scrollFromOverview (pos.x());
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
        copyDrag = e->modifiers() & Qt::AltModifier;  // Option : copie
        dropIndex = insertIndexAt (pos.x());
        update();
    }
}

void SongView::mouseReleaseEvent (QMouseEvent* e)
{
    if (drag == Drag::Ruler)                         // simple clic : le curseur ici (au pas choisi ; la zone reste)
    {
        // Au temps pres (1/4 de mesure) ; Shift + clic : a la ligne pres
        const double snap = (e->modifiers() & Qt::ShiftModifier) ? 1.0 : seq::kRowsPerBar / 4.0;
        const auto rows = startRows();
        const int row = (int) std::clamp (std::floor (rowOfX (pressPos.x()) / snap) * snap, 0.0, (double) std::max (0, rows.back() - 1));
        emit cursorAt (row);
    }
    else if (drag == Drag::Region)
        emit loopRegion (std::min (regionA, regionB), std::max (regionA, regionB));
    else if (drag == Drag::Resize && pressEntry >= 0)
        emit lengthChanged (pressEntry, resizeBars);
    else if (drag == Drag::Move && pressEntry >= 0 && dropIndex >= 0)
        emit moveEntries (chosenList(), dropIndex, e->modifiers() & Qt::AltModifier);
    drag = Drag::None;
    edge = Edge::None;
    dropIndex = -1;
    pressEntry = -1;
    copyDrag = false;
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
        if (! chosen.contains (i))
            selectOnly (i);
        emit selectedChanged (i);
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
