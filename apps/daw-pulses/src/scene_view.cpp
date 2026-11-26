// Vue SCENES : voir scene_view.h
#include "scene_view.h"

#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <algorithm>

using namespace session;

namespace {
constexpr int kNothing = -9;
}

SceneView::SceneView (seq::Sequencer& seqr, const session::State& st, QWidget* parent)
    : QWidget (parent), sequencer (seqr), state (st)
{
    setFocusPolicy (Qt::NoFocus);
}

QString SceneView::patternText (int g, int index) const
{
    if (index == seq::kOff)
        return {};                                   // eteint : case vide (moins charge)
    if (index == seq::kFree)
        return QString::fromUtf8 ("·");
    const QString name = QString::fromStdString (sequencer.patternName (g, index));
    return name.isEmpty() ? QString ("Pat%1").arg (index + 1) : name;      // sans nom : Pat1, Pat2...
}

int SceneView::visibleScenes() const
{
    if (rotated)
        return std::clamp (std::max (96, width() - nameW() - nowW() - 8) / 96, 1, 64);
    return std::max (1, (height() - 2 * rowH() - 8) / rowH());
}

QRectF SceneView::groupCell (int g) const
{
    if (rotated)
        return QRectF (2, rowH() + 4 + g * groupH() + 1, nameW() - 4, groupH() - 3);
    return QRectF (nameW() + g * cellW() + 1, 2, cellW() - 2, rowH() - 3);
}

QRectF SceneView::nowCell (int g) const
{
    if (rotated)
        return QRectF (nameW() + 2, rowH() + 4 + g * groupH() + 1, nowW() - 4, groupH() - 3);
    return QRectF (nameW() + g * cellW() + 1, rowH() + 2, cellW() - 2, rowH() - 3);
}

QRectF SceneView::sceneNameCell (int k) const
{
    if (rotated)
        return QRectF (nameW() + nowW() + 8 + k * sceneW() + 1, 2, sceneW() - 3, rowH() - 3);
    return QRectF (4, 2 * rowH() + 6 + k * rowH() + 2, nameW() - 8, rowH() - 3);
}

QRectF SceneView::sceneCell (int k, int g) const
{
    if (rotated)
        return QRectF (nameW() + nowW() + 8 + k * sceneW() + 1, rowH() + 4 + g * groupH() + 1, sceneW() - 3, groupH() - 3);
    return QRectF (nameW() + g * cellW() + 1, 2 * rowH() + 6 + k * rowH() + 2, cellW() - 2, rowH() - 3);
}

void SceneView::paintEvent (QPaintEvent*)
{
    QPainter p (this);
    p.setRenderHint (QPainter::Antialiasing);
    p.fillRect (rect(), QColor (14, 14, 18));
    const auto combo = sequencer.currentCombo();
    const int now = sequencer.currentScene();
    QFont f = font();
    f.setPixelSize (12);
    p.setFont (f);

    // Titres des lignes / colonnes
    p.setPen (QColor (150, 150, 160));
    if (! rotated)
    {
        p.drawText (QRectF (6, 0, nameW() - 8, rowH()), Qt::AlignVCenter | Qt::AlignLeft, tr ("GROUPS"));
        p.setPen (QColor (230, 230, 240));
        p.drawText (QRectF (6, rowH(), nameW() - 8, rowH()), Qt::AlignVCenter | Qt::AlignLeft, tr ("NOW PLAYING"));
        p.setPen (QColor (60, 60, 70));
        p.drawLine (QPointF (0, 2 * rowH() + 3), QPointF (width(), 2 * rowH() + 3));
    }
    else
    {
        p.setPen (QColor (230, 230, 240));
        p.drawText (QRectF (nameW() + 2, 0, nowW() - 4, rowH()), Qt::AlignCenter, tr ("NOW"));
        p.setPen (QColor (60, 60, 70));
        p.drawLine (QPointF (nameW() + nowW() + 4, 0), QPointF (nameW() + nowW() + 4, height()));
    }
    // Groupes (le groupe choisi en plein)
    for (int g = 0; g < kGroups; ++g)
    {
        const QColor c = groupColor (g);
        const QRectF r = groupCell (g);
        p.setPen (Qt::NoPen);
        p.setBrush (g == state.group ? c : c.darker (260));
        p.drawRoundedRect (r, 4, 4);
        p.setPen (g == state.group ? QColor (15, 15, 18) : c.lighter (130));
        p.drawText (r, Qt::AlignCenter, p.fontMetrics().elidedText (session::groupTitle (state, g), Qt::ElideRight, (int) r.width() - 4));
    }
    // Ce qui joue (cadre orange : en attente de la fin du pattern)
    for (int g = 0; g < kGroups; ++g)
    {
        const QRectF r = nowCell (g);
        const int queued = sequencer.queuedPattern (g);
        const bool silent = sequencer.groupOff (g) && queued == seq::kNoQueue;
        p.setPen (queued != seq::kNoQueue ? QPen (QColor (252, 140, 40), 2, Qt::DashLine) : Qt::NoPen);
        p.setBrush (silent ? QColor (30, 30, 36) : groupColor (g).darker (150));
        p.drawRoundedRect (r, 4, 4);
        p.setPen (silent ? QColor (110, 110, 120) : QColor (240, 240, 245));
        p.drawText (r, Qt::AlignCenter, silent ? QString() : patternText (g, combo[(size_t) g]));
    }
    // Scenes
    const int vis = visibleScenes();
    firstScene = std::clamp (firstScene, 0, std::max (0, seq::kPatternSlots - vis));
    for (int k = 0; k < vis && firstScene + k < seq::kPatternSlots; ++k)
    {
        const int i = firstScene + k;
        const seq::Scene sc = sequencer.scene (i);
        bool matches = sc.used;                      // ce qui joue = cette scene ?
        for (int g = 0; sc.used && g < kGroups; ++g)
            matches &= sc.pattern[(size_t) g] == seq::kFree || sc.pattern[(size_t) g] == combo[(size_t) g];
        const QRectF nameBox = sceneNameCell (k);
        p.setPen (i == now ? QPen (QColor (232, 232, 240), 2) : Qt::NoPen);
        p.setBrush (i == now && matches ? QColor (232, 232, 240) : sc.used ? QColor (44, 44, 54) : QColor (22, 22, 27));
        p.drawRoundedRect (nameBox, 4, 4);
        p.setPen (i == now && matches ? QColor (15, 15, 18) : sc.used ? QColor (235, 235, 242) : QColor (80, 80, 90));
        const QString label = QString ("S%1").arg (i + 1) + (sc.name.empty() ? QString() : " " + QString::fromStdString (sc.name));
        p.drawText (nameBox.adjusted (6, 0, -4, 0), Qt::AlignVCenter | (rotated ? Qt::AlignHCenter : Qt::AlignLeft),
                    sc.used ? QString::fromUtf8 ("▶ ") + label : label);
        if (! sc.used)
            continue;
        for (int g = 0; g < kGroups; ++g)
        {
            const QRectF r = sceneCell (k, g);
            const int v = sc.pattern[(size_t) g];
            p.setPen (Qt::NoPen);
            p.setBrush (v == seq::kFree ? QColor (22, 22, 27) : v == seq::kOff ? QColor (30, 30, 36) : groupColor (g).darker (220));
            p.drawRoundedRect (r, 4, 4);
            p.setPen (v == seq::kFree ? QColor (90, 90, 100) : v == seq::kOff ? QColor (230, 110, 110) : QColor (230, 230, 238));
            p.drawText (r, Qt::AlignCenter, patternText (g, v));
        }
    }
    // D'autres scenes plus loin : la molette
    if (firstScene + vis < seq::kPatternSlots)
    {
        p.setPen (QColor (120, 120, 130));
        p.drawText (rotated ? QRectF (width() - 80, height() - 14, 80, 14) : QRectF (0, height() - 14, nameW(), 14), Qt::AlignCenter,
                    rotated ? QString::fromUtf8 ("wheel  ▶") : QString::fromUtf8 ("▼  wheel"));
    }
}

int SceneView::pickPattern (int g, const QPoint& at, bool allowFree, int currentValue)
{
    // Menu : les patterns du groupe, un nouveau, silence (et « libre » dans une scene)
    QMenu menu (this);
    auto* title = menu.addAction (tr ("Group %1").arg (session::groupTitle (state, g)));
    title->setEnabled (false);
    menu.addSeparator();
    QList<QPair<QAction*, int>> choices;
    for (int i = 0; i < seq::kPatternSlots; ++i)
        if (sequencer.hasPattern (g, i))
        {
            auto* a = menu.addAction (QString ("%1  %2").arg (i + 1, 2).arg (patternText (g, i)));
            a->setCheckable (true);
            a->setChecked (i == currentValue);
            choices << qMakePair (a, i);
        }
    int fresh = -1;
    for (int i = 0; i < seq::kPatternSlots && fresh < 0; ++i)
        if (! sequencer.hasPattern (g, i))
            fresh = i;
    if (fresh >= 0)
        choices << qMakePair (menu.addAction (tr ("New pattern (%1)").arg (fresh + 1)), fresh);
    menu.addSeparator();
    auto* off = menu.addAction (tr ("Off (silent)"));
    off->setCheckable (true);
    off->setChecked (currentValue == seq::kOff);
    choices << qMakePair (off, (int) seq::kOff);
    if (allowFree)
    {
        auto* fr = menu.addAction (tr ("Free (keeps its pattern)"));
        fr->setCheckable (true);
        fr->setChecked (currentValue == seq::kFree);
        choices << qMakePair (fr, (int) seq::kFree);
    }
    QAction* rename = nullptr;
    if (currentValue >= 0)
    {
        menu.addSeparator();
        rename = menu.addAction (tr ("Rename %1...").arg (patternText (g, currentValue)));
    }
    auto* picked = menu.exec (at);
    if (picked && picked == rename)
    {
        emit renamePattern (g, currentValue);
        return kNothing;
    }
    for (const auto& c : choices)
        if (c.first == picked)
        {
            if (c.second == fresh)                   // nouveau pattern : meme longueur que celui qui joue
                sequencer.setPatternAt (g, fresh, [&] { seq::Pattern p; p.length = sequencer.patternLength (g); return p; }());
            return c.second;
        }
    return kNothing;
}

void SceneView::mousePressEvent (QMouseEvent* e)
{
    const QPointF pos = e->position();
    const bool right = e->button() == Qt::RightButton || (e->modifiers() & Qt::MetaModifier);
    const QPoint at = e->globalPosition().toPoint();
    for (int g = 0; g < kGroups; ++g)                // nom d'un groupe : le choisir
        if (groupCell (g).contains (pos))
        {
            emit groupChosen (g);
            return;
        }
    for (int g = 0; g < kGroups; ++g)                // ce qui joue : clic = joue / se tait, clic droit = choisir
        if (nowCell (g).contains (pos))
        {
            emit groupChosen (g);
            const int nowValue = sequencer.currentCombo()[(size_t) g];
            const int v = right ? pickPattern (g, at, false, nowValue)
                                : (nowValue == seq::kOff ? sequencer.currentPattern (g) : (int) seq::kOff);
            if (v != kNothing)
            {
                sequencer.selectPattern (g, v, isPlaying && isPlaying());
                sequencer.setCurrentScene (-1);
                emit changed();
            }
            update();
            return;
        }
    const int vis = visibleScenes();
    for (int k = 0; k < vis && firstScene + k < seq::kPatternSlots; ++k)
    {
        const int i = firstScene + k;
        if (sceneNameCell (k).contains (pos))        // nom de la scene : lancer (clic droit : menu)
        {
            right ? emit sceneMenu (i, at) : emit sceneClicked (i);
            update();
            return;
        }
        seq::Scene sc = sequencer.scene (i);
        if (! sc.used)
            continue;
        for (int g = 0; g < kGroups; ++g)
            if (sceneCell (k, g).contains (pos))
            {
                // clic : le pattern du groupe actif / eteint dans la scene ; clic droit : choisir
                const int v = right ? pickPattern (g, at, true, sc.pattern[(size_t) g])
                                    : (sc.pattern[(size_t) g] >= 0 ? (int) seq::kOff : sequencer.currentPattern (g));
                if (v != kNothing)
                {
                    sc.pattern[(size_t) g] = v;
                    sequencer.setScene (i, sc);
                    emit changed();
                }
                update();
                return;
            }
    }
}

void SceneView::mouseDoubleClickEvent (QMouseEvent* e)
{
    // Double-clic sur le nom d'une scene : la renommer
    const QPointF pos = e->position();
    for (int k = 0; k < visibleScenes() && firstScene + k < seq::kPatternSlots; ++k)
        if (sceneNameCell (k).contains (pos) && sequencer.scene (firstScene + k).used)
            emit renameScene (firstScene + k);
}

void SceneView::wheelEvent (QWheelEvent* e)
{
    const int steps = (e->angleDelta().y() ? e->angleDelta().y() : e->angleDelta().x()) / 120;
    firstScene = std::clamp (firstScene - steps * 2, 0, std::max (0, seq::kPatternSlots - visibleScenes()));
    update();
}
