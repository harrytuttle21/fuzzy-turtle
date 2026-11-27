// Vue SCENES : voir scene_view.h
#include "scene_view.h"

#include <QMenu>
#include <QMimeData>
#include <QDrag>
#include <QPixmap>
#include <QDragEnterEvent>
#include <QDropEvent>
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
    setAcceptDrops (true);                           // carres de la palette
    setMouseTracking (true);                         // survol : groupe surligne
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

int SceneView::visibleGroups() const
{
    return std::clamp ((int) ((height() - rowH() - 6) / groupH()), 1, kGroups);
}

QRectF SceneView::lockBox (int k) const
{
    const QRectF r = sceneNameCell (k);
    return QRectF (r.right() - 18, r.center().y() - 8, 16, 16);
}

QRectF SceneView::groupCell (int g) const
{
    if (rotated)
        return QRectF (48, rowH() + 4 + (g - firstGroup) * groupH() + 1, nameW() - 52, groupH() - 3);
    return QRectF (nameW() + g * cellW() + 1, 2, cellW() - 2, rowH() - 3);
}

QRectF SceneView::muteBox (int g) const
{
    return QRectF (2, rowH() + 4 + (g - firstGroup) * groupH() + 1, 20, groupH() - 3);
}

QRectF SceneView::soloBox (int g) const
{
    return QRectF (24, rowH() + 4 + (g - firstGroup) * groupH() + 1, 20, groupH() - 3);
}

QRectF SceneView::nowCell (int g) const
{
    if (rotated)
        return QRectF (nameW() + 2, rowH() + 4 + (g - firstGroup) * groupH() + 1, nowW() - 4, groupH() - 3);
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
        return QRectF (nameW() + nowW() + 8 + k * sceneW() + 1, rowH() + 4 + (g - firstGroup) * groupH() + 1, sceneW() - 3, groupH() - 3);
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
        firstGroup = std::clamp (firstGroup, 0, kGroups - visibleGroups());
        p.drawText (QRectF (80, 0, nameW() - 128, rowH()), Qt::AlignVCenter | Qt::AlignLeft, tr ("GROUPS"));
        // A/M et A/S : tout reactiver (allumes s'il y a quelque chose a reactiver)
        bool anyMute = false, anySolo = false;
        for (int g = 0; g < kGroups; ++g)
        {
            anyMute |= state.groupMute[(size_t) g].load();
            anySolo |= state.groupSolo[(size_t) g].load();
        }
        for (const bool solo : { false, true })
        {
            const bool on = solo ? anySolo : anyMute;
            const QRectF b = solo ? allSoloBox() : allMuteBox();
            p.setPen (on ? Qt::NoPen : QPen (QColor (70, 70, 80)));
            p.setBrush (on ? (solo ? QColor (232, 192, 64) : QColor (224, 80, 80)) : QColor (22, 22, 27));
            p.drawRoundedRect (b, 3, 3);
            p.setPen (on ? QColor (15, 15, 18) : QColor (110, 110, 120));
            p.drawText (b, Qt::AlignCenter, solo ? "A/S" : "A/M");
        }
        if (visibleGroups() < kGroups)               // pas la place pour 16 : triangles haut / bas
            for (const bool up : { true, false })
            {
                const bool can = up ? firstGroup > 0 : firstGroup + visibleGroups() < kGroups;
                const QRectF a = up ? upArrow() : downArrow();
                p.setPen (Qt::NoPen);
                p.setBrush (can ? QColor (60, 60, 72) : QColor (28, 28, 34));
                p.drawRoundedRect (a, 3, 3);
                p.setBrush (can ? QColor (235, 235, 242) : QColor (80, 80, 90));
                const QPointF c = a.center();
                QPolygonF tri;
                tri << QPointF (c.x() - 5, c.y() + (up ? 3 : -3)) << QPointF (c.x() + 5, c.y() + (up ? 3 : -3)) << QPointF (c.x(), c.y() + (up ? -4 : 4));
                p.drawPolygon (tri);
            }
        p.setPen (QColor (230, 230, 240));
        p.drawText (QRectF (nameW() + 2, 0, nowW() - 4, rowH()), Qt::AlignCenter, tr ("NOW"));
        p.setPen (QColor (60, 60, 70));
        p.drawLine (QPointF (nameW() + nowW() + 4, 0), QPointF (nameW() + nowW() + 4, height()));
    }
    // Groupes (le groupe choisi en plein)
    for (int g = 0; g < kGroups; ++g)
    {
        if (! groupShown (g))
            continue;
        const QColor c = state.groupAudible (g) ? groupColor (g) : QColor (90, 90, 98);   // muet : gris
        const QRectF r = groupCell (g);
        p.setPen (Qt::NoPen);
        p.setBrush (g == state.group ? c : c.darker (260));
        p.drawRoundedRect (r, 4, 4);
        p.setPen (g == state.group ? QColor (15, 15, 18) : c.lighter (130));
        const QRectF tr = rotated ? r.adjusted (8, 0, -4, 0) : r;   // tourne : nom a gauche
        p.drawText (tr, Qt::AlignVCenter | (rotated ? Qt::AlignLeft : Qt::AlignHCenter),
                    p.fontMetrics().elidedText (session::groupTitle (state, g)
                        + (rotated ? "" : state.groupMute[(size_t) g] ? " M" : state.groupSolo[(size_t) g] ? " S" : ""), Qt::ElideRight, (int) tr.width() - 2));
        if (rotated)                                 // M (rouge = muet) et S (jaune = solo)
            for (const bool solo : { false, true })
            {
                const bool on = solo ? state.groupSolo[(size_t) g].load() : state.groupMute[(size_t) g].load();
                const QRectF b = solo ? soloBox (g) : muteBox (g);
                p.setPen (on ? Qt::NoPen : QPen (QColor (70, 70, 80)));
                p.setBrush (on ? (solo ? QColor (232, 192, 64) : QColor (224, 80, 80)) : QColor (22, 22, 27));
                p.drawRoundedRect (b, 3, 3);
                p.setPen (on ? QColor (15, 15, 18) : QColor (110, 110, 120));
                p.drawText (b, Qt::AlignCenter, solo ? "S" : "M");
            }
    }
    // Ce qui joue (cadre orange : en attente de la fin du pattern)
    for (int g = 0; g < kGroups; ++g)
    {
        if (! groupShown (g))
            continue;
        const QRectF r = nowCell (g);
        const int queued = sequencer.queuedPattern (g);
        const bool silent = sequencer.groupOff (g) && queued == seq::kNoQueue;
        p.setPen (queued != seq::kNoQueue ? QPen (QColor (252, 140, 40), 2, Qt::DashLine) : Qt::NoPen);
        p.setBrush (silent ? QColor (30, 30, 36) : groupColor (g).darker (150));
        p.drawRoundedRect (r, 4, 4);
        p.setPen (silent ? QColor (110, 110, 120) : QColor (240, 240, 245));
        p.drawText (r, Qt::AlignCenter, silent ? QString() : patternText (g, combo[(size_t) g]));
    }
    if (hoverGroup >= 0 && groupShown (hoverGroup))  // groupe sous la souris : bande eclaircie
    {
        const QRectF c = groupCell (hoverGroup);
        p.fillRect (rotated ? QRectF (0, c.top() - 1, width(), groupH()) : QRectF (c.left() - 1, 0, cellW(), height()),
                    QColor (255, 255, 255, 16));
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
        p.setPen (i == now ? QPen (QColor (232, 232, 240), 2) : i == editScene && editMode ? QPen (QColor (252, 140, 40), 2) : QPen (Qt::NoPen));
        p.setBrush (i == now && matches ? QColor (232, 232, 240) : sc.used ? QColor (44, 44, 54) : QColor (22, 22, 27));
        p.drawRoundedRect (nameBox, 4, 4);
        p.setPen (i == now && matches ? QColor (15, 15, 18) : sc.used ? QColor (235, 235, 242) : QColor (80, 80, 90));
        const QString label = QString ("S%1").arg (i + 1) + (sc.name.empty() ? QString() : " " + QString::fromStdString (sc.name));
        p.drawText (nameBox.adjusted (6, 0, sc.used ? -22 : -4, 0), Qt::AlignVCenter | (rotated ? Qt::AlignHCenter : Qt::AlignLeft),
                    p.fontMetrics().elidedText (sc.used ? QString::fromUtf8 ("▶ ") + label : label, Qt::ElideRight, (int) nameBox.width() - (sc.used ? 28 : 10)));
        if (! sc.used)
            continue;
        {
            // Cadenas : ferme (orange) = scene verrouillee ; ouvert (gris) = modifiable
            const QRectF lb = lockBox (k);
            // Edit eteint : tout est ferme (gris) ; Edit : ouvert, sauf les scenes verrouillees (orange)
            const bool closed = sc.locked || ! editMode;
            const QColor ink = sc.locked ? QColor (252, 140, 40) : ! editMode ? QColor (90, 90, 100) : QColor (150, 150, 162);
            p.setBrush (Qt::NoBrush);
            p.setPen (QPen (ink, 1.6));
            const QRectF body (lb.left() + 2, lb.top() + 7, 12, 8);
            const QRectF arc (lb.left() + 4, lb.top() + 1, 8, 10);
            if (closed)
                p.drawArc (arc, 0, 180 * 16);
            else
                p.drawArc (arc.translated (4, -1), 0, 180 * 16);   // anse ouverte, decalee
            p.setBrush (ink);
            p.setPen (Qt::NoPen);
            p.drawRoundedRect (body, 2, 2);
        }
        for (int g = 0; g < kGroups; ++g)
        {
            if (! groupShown (g))
                continue;
            const QRectF r = sceneCell (k, g);
            const int v = sc.pattern[(size_t) g];
            p.setPen (Qt::NoPen);
            p.setBrush (v == seq::kFree ? QColor (22, 22, 27) : v == seq::kOff ? QColor (30, 30, 36) : groupColor (g).darker (220));
            p.drawRoundedRect (r, 4, 4);
            if (i == dropScene && g == dropGroup)    // palette : ici en lachant
            {
                p.setPen (QPen (QColor (252, 140, 40), 2, Qt::DashLine));
                p.setBrush (Qt::NoBrush);
                p.drawRoundedRect (r.adjusted (1, 1, -1, -1), 4, 4);
            }
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
    if (currentValue >= 0 && editMode)
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
    // Modes : Play = cliquer joue ; Edit = cliquer modifie les scenes ; aucun = tout est protege
    const QPointF pos = e->position();
    const bool right = e->button() == Qt::RightButton || (e->modifiers() & Qt::MetaModifier);
    const QPoint at = e->globalPosition().toPoint();
    const bool playing = isPlaying && isPlaying();
    if (rotated && ! right)                          // M / S d'un groupe, A/M et A/S : toujours permis
    {
        if (allMuteBox().contains (pos) || allSoloBox().contains (pos))
        {
            emit allOff (allSoloBox().contains (pos));
            update();
            return;
        }
        for (int g = 0; g < kGroups; ++g)
            if (groupShown (g) && (muteBox (g).contains (pos) || soloBox (g).contains (pos)))
            {
                emit muteSoloClicked (g, soloBox (g).contains (pos));
                update();
                return;
            }
    }
    if (rotated && visibleGroups() < kGroups)        // triangles : faire defiler les groupes
    {
        if (upArrow().contains (pos) || downArrow().contains (pos))
        {
            firstGroup = std::clamp (firstGroup + (upArrow().contains (pos) ? -1 : 1), 0, kGroups - visibleGroups());
            update();
            return;
        }
    }
    for (int g = 0; g < kGroups; ++g)                // nom d'un groupe : le choisir ; clic droit : Mute / Solo
        if (groupShown (g) && groupCell (g).contains (pos))
        {
            right ? emit groupMenu (g, at) : emit groupChosen (g);
            update();
            return;
        }
    for (int g = 0; g < kGroups; ++g)                // ce qui joue (Play) : clic = joue / se tait, clic droit = choisir
        if (groupShown (g) && nowCell (g).contains (pos))
        {
            emit groupChosen (g);
            if (! playMode)
            {
                emit message (tr ("Turn Play on to change what plays"));
                return;
            }
            const int nowValue = sequencer.currentCombo()[(size_t) g];
            const int v = right ? pickPattern (g, at, false, nowValue)
                                : (nowValue == seq::kOff ? sequencer.currentPattern (g) : (int) seq::kOff);
            if (v != kNothing)
            {
                sequencer.selectPattern (g, v, playing);
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
        seq::Scene sc = sequencer.scene (i);
        if (sc.used && lockBox (k).contains (pos))  // cadenas (Edit)
        {
            if (! editMode)
            {
                emit message (tr ("Turn Edit on to lock / unlock a scene"));
                return;
            }
            emit beforeEdit (QString ("Lock scene %1").arg (i + 1));
            sc.locked = ! sc.locked;
            sequencer.setScene (i, sc);
            emit message (QString ("SCENE %1 %2").arg (i + 1).arg (sc.locked ? "LOCKED" : "UNLOCKED"));
            emit changed();
            update();
            return;
        }
        if (sceneNameCell (k).contains (pos))
        {
            if (! right && sc.used)                  // clic : au relachement (glisser = vers la vue Song)
            {
                pendingName = i;
                pendingPos = pos;
                return;
            }
            nameClick (i, right, at);
            return;
        }
        if (! sc.used)
            continue;
        for (int g = 0; g < kGroups; ++g)
            if (groupShown (g) && sceneCell (k, g).contains (pos))
            {
                if (! editMode && ! playMode)
                {
                    emit message (tr ("Scenes protected: turn Edit or Play on"));
                    return;
                }
                int value = sc.pattern[(size_t) g];
                if (editMode)
                {
                    if (sc.locked)
                    {
                        emit message (QString ("SCENE %1 is locked").arg (i + 1));
                        if (! playMode)
                            return;
                    }
                    else
                    {
                        // clic : le pattern du groupe actif / eteint dans la scene ; clic droit : choisir
                        const int v = right ? pickPattern (g, at, true, value)
                                            : (value >= 0 ? (int) seq::kOff : sequencer.currentPattern (g));
                        if (v != kNothing)
                        {
                            emit beforeEdit (QString ("Scene %1").arg (i + 1));
                            sc.pattern[(size_t) g] = value = v;
                            sequencer.setScene (i, sc);
                            editScene = i;
                        }
                    }
                }
                if (playMode && value >= 0 && ! right)   // Play : jouer ce pattern pour ce groupe
                {
                    sequencer.selectPattern (g, value, playing);
                    sequencer.setCurrentScene (-1);
                    emit message (QString ("%1 : %2%3").arg (session::groupTitle (state, g), patternText (g, value),
                                                             playing ? QString (" (next)") : QString()));
                }
                emit changed();
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
        if (sceneNameCell (k).contains (pos) && sequencer.scene (firstScene + k).used && ! lockBox (k).contains (pos))
        {
            if (! editMode)
                emit message (tr ("Turn Edit on to rename a scene"));
            else if (sceneLocked (firstScene + k))
                emit message (QString ("SCENE %1 is locked").arg (firstScene + k + 1));
            else
                emit renameScene (firstScene + k);
        }
}

void SceneView::wheelEvent (QWheelEvent* e)
{
    const int steps = (e->angleDelta().y() ? e->angleDelta().y() : e->angleDelta().x()) / 120;
    if (rotated && e->position().x() < nameW() + nowW() + 4)   // colonne des groupes : les faire defiler
    {
        firstGroup = std::clamp (firstGroup - steps, 0, kGroups - visibleGroups());
        update();
        return;
    }
    firstScene = std::clamp (firstScene - steps * 2, 0, std::max (0, seq::kPatternSlots - visibleScenes()));
    update();
}

// ------------------------------------------------------------ palette : glisser un pattern dans une scene

namespace {
// « fuzzy/pattern » : groupe, index
bool readDrag (const QMimeData* m, int& group, int& index)
{
    if (! m || ! m->hasFormat ("application/x-fuzzy-pattern"))
        return false;
    const QList<QByteArray> parts = m->data ("application/x-fuzzy-pattern").split (',');
    if (parts.size() != 2)
        return false;
    group = parts[0].toInt();
    index = parts[1].toInt();
    return true;
}
}

bool SceneView::dropTarget (const QPointF& pos, int group, int& scene, int& cellGroup) const
{
    // Une case de scene (le groupe doit etre celui du pattern), ou le nom d'une scene (ligne du groupe)
    scene = cellGroup = -1;
    for (int k = 0; k < visibleScenes() && firstScene + k < seq::kPatternSlots; ++k)
    {
        if (sceneNameCell (k).contains (pos))
        {
            scene = firstScene + k;
            cellGroup = group;
            return true;
        }
        for (int g = 0; g < kGroups; ++g)
            if (groupShown (g) && sceneCell (k, g).contains (pos))
            {
                scene = firstScene + k;
                cellGroup = g;
                return g == group;
            }
    }
    return false;
}

void SceneView::dragEnterEvent (QDragEnterEvent* e)
{
    int g = 0, i = 0;
    if (readDrag (e->mimeData(), g, i))
        e->acceptProposedAction();
}

void SceneView::dragMoveEvent (QDragMoveEvent* e)
{
    int g = 0, i = 0, sc = -1, cg = -1;
    readDrag (e->mimeData(), g, i);
    const bool ok = dropTarget (e->position(), g, sc, cg) && editMode && ! (sc >= 0 && sequencer.scene (sc).locked);
    dropScene = ok ? sc : -1;
    dropGroup = ok ? cg : -1;
    ok ? e->acceptProposedAction() : e->ignore();
    update();
}

void SceneView::dragLeaveEvent (QDragLeaveEvent*)
{
    dropScene = dropGroup = -1;
    update();
}

void SceneView::dropEvent (QDropEvent* e)
{
    int g = 0, i = 0, sc = -1, cg = -1;
    dropScene = dropGroup = -1;
    update();
    if (! readDrag (e->mimeData(), g, i))
        return;
    if (! editMode)
    {
        emit message (tr ("Turn Edit on to put patterns into scenes"));
        return;
    }
    if (! dropTarget (e->position(), g, sc, cg))
    {
        if (sc >= 0)
            emit message (QString ("Pattern of group %1: drop it in the row of group %1").arg (session::groupName (g)));
        return;
    }
    seq::Scene scene = sequencer.scene (sc);
    if (scene.locked)
    {
        emit message (QString ("SCENE %1 is locked").arg (sc + 1));
        return;
    }
    emit beforeEdit (QString ("Scene %1").arg (sc + 1));
    if (! scene.used)                                // scene vide : nouvelle, rien d'autre actif
    {
        scene.used = true;
        scene.pattern.fill (seq::kOff);
    }
    scene.pattern[(size_t) g] = i;
    sequencer.setScene (sc, scene);
    e->acceptProposedAction();
    emit message (QString ("S%1 %2 = %3").arg (sc + 1).arg (session::groupName (g)).arg (patternText (g, i)));
    emit changed();
}

void SceneView::nameClick (int i, bool right, const QPoint& at)
{
    // Clic sur le nom d'une scene (selon les modes Edit / Play)
    const seq::Scene sc = sequencer.scene (i);
    if (right)                                       // menu de la scene : Edit seulement
        editMode ? emit sceneMenu (i, at) : emit message (tr ("Turn Edit on to change scenes"));
    else if (playMode && (sc.used || editMode))
        emit sceneClicked (i);                       // lancer (vide + Edit : nouvelle scene)
    else if (editMode)
    {
        editScene = i;                               // Edit seul : choisir la scene sans la lancer
        if (! sc.used)
            emit sceneClicked (i);                   // vide : nouvelle scene
    }
    else if (! editMode && ! playMode)
        emit message (tr ("Scenes protected: turn Edit or Play on"));
    update();
}

int SceneView::groupAtPos (const QPointF& p) const
{
    for (int g = 0; g < kGroups; ++g)
    {
        if (! groupShown (g))
            continue;
        const QRectF c = groupCell (g);
        if (rotated ? (p.y() >= c.top() - 1 && p.y() < c.top() - 1 + groupH())
                    : (p.x() >= c.left() - 1 && p.x() < c.left() - 1 + cellW()))
            return g;
    }
    return -1;
}

void SceneView::leaveEvent (QEvent*)
{
    hoverGroup = -1;
    update();
}

void SceneView::mouseMoveEvent (QMouseEvent* e)
{
    if (! (e->buttons() & Qt::LeftButton))           // survol : la ligne / colonne du groupe eclaircie
    {
        const int g = groupAtPos (e->position());
        if (g != hoverGroup)
        {
            hoverGroup = g;
            update();
        }
        return;
    }
    // Nom d'une scene glisse : vers la vue Song (ajouter au morceau)
    if (pendingName < 0 || ! (e->buttons() & Qt::LeftButton) || (e->position() - pendingPos).manhattanLength() < 8)
        return;
    const int i = pendingName;
    pendingName = -1;
    auto* mime = new QMimeData;
    mime->setData ("application/x-fuzzy-scene", QByteArray::number (i));
    auto* drag = new QDrag (this);
    drag->setMimeData (mime);
    QPixmap pix (110, 26);
    pix.fill (QColor (252, 140, 40));
    {
        QPainter q (&pix);
        q.setPen (QColor (15, 15, 18));
        q.drawText (pix.rect(), Qt::AlignCenter, QString ("S%1 %2").arg (i + 1).arg (QString::fromStdString (sequencer.scene (i).name)));
    }
    drag->setPixmap (pix);
    drag->setHotSpot (QPoint (55, 13));
    drag->exec (Qt::CopyAction);
}

void SceneView::mouseReleaseEvent (QMouseEvent* e)
{
    if (pendingName >= 0)                            // simple clic sur le nom
    {
        const int i = pendingName;
        pendingName = -1;
        nameClick (i, false, e->globalPosition().toPoint());
    }
}
