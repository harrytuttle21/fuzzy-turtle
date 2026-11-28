// Fuzzy John DAW Pulses — vue Scene Grid (facon Maschine). Licence : GPLv3.
#include "scene_grid_view.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

#include <algorithm>

using namespace session;

SceneGridView::SceneGridView (seq::Sequencer& seqr, const session::State& st, QWidget* parent)
    : QWidget (parent), sequencer (seqr), state (st)
{
    setMouseTracking (true);
}

void SceneGridView::setScene (int index)
{
    chosen = std::clamp (index, 0, seq::kPatternSlots - 1);
    // La scene choisie reste visible (defilement)
    const int visible = std::max (1, width() / kSceneW);
    if (chosen < firstScene)
        firstScene = chosen;
    else if (chosen >= firstScene + visible)
        firstScene = chosen - visible + 1;
    update();
}

int SceneGridView::sceneCount() const
{
    int last = -1;
    for (int i = 0; i < seq::kPatternSlots; ++i)
        if (sequencer.scene (i).used)
            last = i;
    return std::clamp (std::max ({ last + 2, chosen + 1, 8 }), 1, seq::kPatternSlots);
}

std::vector<int> SceneGridView::patternsOf (int group) const
{
    std::vector<int> list;
    for (int i = 0; i < seq::kPatternSlots; ++i)
        if (sequencer.hasPattern (group, i))
            list.push_back (i);
    return list;
}

QRectF SceneGridView::sceneRect (int scene) const
{
    return { (scene - firstScene) * (double) kSceneW + 1, 1, kSceneW - 3.0, kHeadH - 4.0 };
}

QRectF SceneGridView::cellRect (int column, int row) const
{
    return { column * colW() + 2, kHeadH + 4.0 + (row - firstRow) * kRowH, colW() - 4, kRowH - 3.0 };
}

QRectF SceneGridView::footRect (int column) const
{
    return { column * colW() + 1, height() - kFootH + 2.0, colW() - 2, kFootH - 3.0 };
}

bool SceneGridView::hit (const QPointF& p, int& scene, int& group, int& pattern, bool& foot) const
{
    scene = group = pattern = -1;
    foot = false;
    if (p.y() < kHeadH)
    {
        const int i = firstScene + (int) (p.x() / kSceneW);
        if (i >= 0 && i < sceneCount())
            scene = i;
        return scene >= 0;
    }
    const int column = std::clamp ((int) (p.x() / colW()), 0, 7);
    group = bank() + column;
    if (p.y() >= height() - kFootH)
    {
        foot = true;
        return true;
    }
    const int row = firstRow + (int) ((p.y() - kHeadH - 4) / kRowH);
    const auto list = patternsOf (group);
    if (row >= 0 && row < (int) list.size() && cellRect (column, row).contains (p))
        pattern = list[(size_t) row];
    return pattern >= 0;
}

void SceneGridView::paintEvent (QPaintEvent*)
{
    QPainter g (this);
    g.setRenderHint (QPainter::Antialiasing);
    g.fillRect (rect(), QColor (22, 22, 26));
    QFont f = font();
    f.setPixelSize (12);
    QFont bold = f;
    bold.setBold (true);
    const seq::Scene sc = sequencer.scene (chosen);
    const int playing = sequencer.currentScene();

    // --- En haut : les scenes
    g.fillRect (QRectF (0, 0, width(), kHeadH), QColor (16, 16, 20));
    for (int i = firstScene; i < sceneCount() && (i - firstScene) * kSceneW < width(); ++i)
    {
        const seq::Scene s = sequencer.scene (i);
        const QRectF r = sceneRect (i);
        const QColor c = seq::sceneColor (i, s.color);
        const bool isChosen = i == chosen;
        g.setPen (Qt::NoPen);
        g.setBrush (isChosen ? QColor (52, 52, 62) : QColor (30, 30, 36));
        g.drawRoundedRect (r, 3, 3);
        if (isChosen)                                // la scene choisie : soulignee
        {
            g.setBrush (c.lighter (120));
            g.drawRect (QRectF (r.left(), r.top(), r.width(), 3));
        }
        g.setFont (bold);
        g.setPen (s.used ? c.lighter (isChosen ? 140 : 110) : QColor (90, 90, 100));
        QString label = (i == playing ? QStringLiteral ("▶ ") : QString()) + QString ("S%1").arg (i + 1);
        if (s.used && ! s.name.empty())
            label += " " + QString::fromStdString (s.name);
        else if (! s.used)
            label += " +";
        g.drawText (r.adjusted (6, 2, s.used ? -20 : -4, 0), Qt::AlignVCenter | Qt::AlignLeft,
                    g.fontMetrics().elidedText (label, Qt::ElideRight, (int) r.width() - (s.used ? 26 : 10)));
        if (s.used)                                  // cadenas : ferme en couleur, ouvert en gris (clic = changer)
        {
            const QRectF lr = lockRect (i);
            const bool shut = s.locked || (globalLock && globalLock());   // Lock : tout est ferme
            const QColor lc = shut ? c.lighter (130) : QColor (110, 110, 120);
            const QRectF b (lr.center().x() - 4.5, lr.center().y() - 1, 9, 7);
            g.setPen (QPen (lc, 1.5));
            g.setBrush (Qt::NoBrush);
            const QRectF arc (b.left() + 1.5, b.top() - 6, 6, 9);
            if (shut)
                g.drawArc (arc, 0, 180 * 16);
            else
                g.drawArc (arc.translated (3.5, -1.5), 30 * 16, 150 * 16);   // anse ouverte
            g.setPen (Qt::NoPen);
            g.setBrush (lc);
            g.drawRect (b);
        }
    }

    // --- Au milieu : une colonne par groupe, tous ses patterns
    g.setFont (f);
    for (int k = 0; k < 8; ++k)
    {
        const int grp = bank() + k;
        const QColor gc = groupColor (grp);
        const int inScene = sc.used ? sc.pattern[(size_t) grp] : seq::kFree;
        if (k % 2 == 1)
            g.fillRect (QRectF (k * colW(), kHeadH, colW(), height() - kHeadH - kFootH), QColor (26, 26, 31));
        const auto list = patternsOf (grp);
        for (int row = firstRow; row < (int) list.size() && row < firstRow + visibleRows(); ++row)
        {
            const int idx = list[(size_t) row];
            const QRectF r = cellRect (k, row);
            const bool member = sc.used && inScene == idx;
            const bool hover = grp == hoverGroup && idx == hoverPattern;
            g.setPen (hover ? QPen (QColor (230, 230, 235), 1) : Qt::NoPen);
            g.setBrush (member ? gc : hover ? QColor (58, 58, 66) : QColor (44, 44, 50));
            g.drawRoundedRect (r, 3, 3);
            QString name = QString::fromStdString (sequencer.patternName (grp, idx));
            if (name.isEmpty())
                name = QString ("Pattern %1").arg (idx + 1);
            // Ce qui joue vraiment (essai en live compris) : petit triangle ; en attente : pointilles orange
            const bool nowOn = ! sequencer.groupOff (grp) && sequencer.currentPattern (grp) == idx;
            const bool queued = sequencer.queuedPattern (grp) == idx;
            if (queued)
            {
                g.setPen (QPen (QColor (252, 140, 40), 1.5, Qt::DashLine));
                g.setBrush (Qt::NoBrush);
                g.drawRoundedRect (r.adjusted (1, 1, -1, -1), 3, 3);
            }
            if (nowOn)
            {
                QPainterPath tri;
                tri.moveTo (r.left() + 5, r.top() + 6);
                tri.lineTo (r.left() + 11, r.center().y());
                tri.lineTo (r.left() + 5, r.bottom() - 6);
                tri.closeSubpath();
                g.fillPath (tri, member ? QColor (17, 17, 17) : QColor (235, 235, 240));
            }
            g.setPen (member ? QColor (17, 17, 17) : nowOn ? QColor (235, 235, 240) : QColor (138, 138, 148));
            g.drawText (r.adjusted (nowOn ? 15 : 6, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft,
                        g.fontMetrics().elidedText (name, Qt::ElideRight, (int) r.width() - (nowOn ? 19 : 10)));
        }
        if (sc.used && inScene == seq::kFree)        // groupe libre dans cette scene : pointilles
        {
            g.setPen (QPen (gc, 1.2, Qt::DashLine));
            g.setBrush (Qt::NoBrush);
            g.drawRoundedRect (QRectF (k * colW() + 1, kHeadH + 2, colW() - 2, height() - kHeadH - kFootH - 4), 4, 4);
        }
    }
    // Plus de patterns que de place : indication
    int most = 0;
    for (int k = 0; k < 8; ++k)
        most = std::max (most, (int) patternsOf (bank() + k).size());
    if (most > firstRow + visibleRows() || firstRow > 0)
    {
        g.setPen (QColor (140, 140, 150));
        g.drawText (QRectF (0, height() - kFootH - 18, width() - 6, 16), Qt::AlignRight | Qt::AlignVCenter,
                    QString ("%1-%2 / %3  (wheel)").arg (firstRow + 1).arg (std::min (most, firstRow + visibleRows())).arg (most));
    }

    // --- En bas : les groupes
    for (int k = 0; k < 8; ++k)
    {
        const int grp = bank() + k;
        const QRectF r = footRect (k);
        const QColor gc = state.groupAudible (grp) ? groupColor (grp) : QColor (90, 90, 98);
        const bool isGroup = grp == state.group;
        g.setPen (Qt::NoPen);
        g.setBrush (isGroup ? gc : QColor (30, 30, 36));
        g.drawRect (r);
        g.setFont (bold);
        g.setPen (isGroup ? QColor (17, 17, 17) : gc);
        g.drawText (r.adjusted (6, 3, -4, 0), Qt::AlignTop | Qt::AlignLeft, groupName (grp));
        g.setFont (f);
        g.setPen (isGroup ? QColor (17, 17, 17) : QColor (210, 210, 216));
        g.drawText (r.adjusted (6, 0, -4, -4), Qt::AlignBottom | Qt::AlignLeft,
                    g.fontMetrics().elidedText (state.groupLabel[(size_t) grp], Qt::ElideRight, (int) r.width() - 10));
    }
    if (! sc.used)                                   // scene vide : comment la remplir
    {
        g.setPen (QColor (150, 150, 160));
        g.drawText (QRectF (0, kHeadH + 4, width(), height() - kHeadH - kFootH - 8), Qt::AlignCenter,
                    tr ("S%1 is empty: click a pattern (Edit on) to put it in the scene").arg (chosen + 1));
    }
}

void SceneGridView::mousePressEvent (QMouseEvent* e)
{
    int scene, group, pattern;
    bool foot;
    hit (e->position(), scene, group, pattern, foot);
    const bool right = e->button() == Qt::RightButton;
    const QPoint at = e->globalPosition().toPoint();
    if (scene >= 0)
    {
        if (! right && sequencer.scene (scene).used && lockRect (scene).adjusted (-3, -4, 3, 4).contains (e->position()))
        {
            emit lockClicked (scene);
            return;
        }
        if (right)
            emit sceneMenu (scene, at);
        else
            emit sceneClicked (scene);
        return;
    }
    if (foot && group >= 0)
    {
        right ? emit groupMenu (group, at) : emit groupClicked (group);
        return;
    }
    if (pattern >= 0)
        right ? emit patternMenu (group, pattern, at) : emit patternToggled (group, pattern);
}

void SceneGridView::mouseDoubleClickEvent (QMouseEvent* e)
{
    int scene, group, pattern;
    bool foot;
    hit (e->position(), scene, group, pattern, foot);
    if (scene >= 0 && sequencer.scene (scene).used)
        emit renameScene (scene);
}

void SceneGridView::wheelEvent (QWheelEvent* e)
{
    const int step = e->angleDelta().y() > 0 ? -1 : e->angleDelta().y() < 0 ? 1 : (e->angleDelta().x() > 0 ? -1 : 1);
    if (e->position().y() < kHeadH || (e->modifiers() & Qt::ShiftModifier) || e->angleDelta().x() != 0)
    {
        const int visible = std::max (1, width() / kSceneW);
        firstScene = std::clamp (firstScene + step, 0, std::max (0, sceneCount() - visible));
    }
    else
    {
        int most = 0;
        for (int k = 0; k < 8; ++k)
            most = std::max (most, (int) patternsOf (bank() + k).size());
        firstRow = std::clamp (firstRow + step, 0, std::max (0, most - visibleRows()));
    }
    update();
}

void SceneGridView::mouseMoveEvent (QMouseEvent* e)
{
    int scene, group, pattern;
    bool foot;
    hit (e->position(), scene, group, pattern, foot);
    if (group != hoverGroup || pattern != hoverPattern)
    {
        hoverGroup = pattern >= 0 ? group : -1;
        hoverPattern = pattern;
        update();
    }
}

void SceneGridView::leaveEvent (QEvent*)
{
    hoverGroup = hoverPattern = -1;
    update();
}
