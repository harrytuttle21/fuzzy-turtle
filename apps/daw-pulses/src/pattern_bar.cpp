// Barre PATTERNS / SCENES : voir pattern_bar.h
#include "pattern_bar.h"

#include <QAbstractItemView>
#include <algorithm>
#include <QComboBox>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QPushButton>

using namespace session;

namespace {
QString groupKey (int g) { return g < 8 ? QString (QChar ('A' + g)) : QString (QChar ('A' + g - 8)) + "2"; }
QString css (const QColor& c) { return c.name(); }
}

PatternBar::PatternBar (seq::Sequencer& seqr, const session::State& st, QWidget* parent)
    : QWidget (parent), sequencer (seqr), state (st)
{
    auto* grid = new QGridLayout (this);
    grid->setContentsMargins (6, 2, 6, 2);
    grid->setHorizontalSpacing (3);
    grid->setVerticalSpacing (3);
    patTitle = new QLabel;
    sceneTitle = new QLabel (tr ("SCENES"));
    for (auto* l : { patTitle, sceneTitle })
    {
        l->setObjectName ("section");
    }
    // Largeur fixe (le nom de groupe le plus long) : le reste de la barre ne bouge plus
    {
        QFont f = font();
        f.setBold (true);
        const int w = QFontMetrics (f).horizontalAdvance ("PATTERNS H2 WWWWWWWW") + 12;
        for (auto* l : { patTitle, sceneTitle })
            l->setFixedWidth (std::max (160, w));
    }
    grid->addWidget (sceneTitle, 0, 0);              // scenes en haut, patterns dessous
    grid->addWidget (patTitle, 1, 0);
    sceneTitle->setToolTip (tr ("SCENES: click = launch \u00B7 empty = new empty scene \u00B7 right-click = more (store what plays, rename...)"));
    patTitle->setToolTip (tr ("PATTERNS of the chosen group: click = play, click again = off \u00B7 empty = new \u00B7 right-click = more"));
    auto slotButton = [this] (bool scene, int k)
    {
        auto* b = new QPushButton;
        b->setFocusPolicy (Qt::NoFocus);
        b->setMinimumWidth (34);
        b->setFixedHeight (24);
        b->setSizePolicy (QSizePolicy::Expanding, QSizePolicy::Fixed);
        b->setContextMenuPolicy (Qt::CustomContextMenu);
        connect (b, &QPushButton::clicked, this, [this, scene, k]
        {
            scene ? chooseScene (sceneBank * seq::kSlotsPerBank + k) : choosePattern (patternBank * seq::kSlotsPerBank + k);
        });
        connect (b, &QPushButton::customContextMenuRequested, this, [this, b, scene, k] (const QPoint& p)
        {
            scene ? sceneMenu (sceneBank * seq::kSlotsPerBank + k, b->mapToGlobal (p))
                  : patternMenu (patternBank * seq::kSlotsPerBank + k, b->mapToGlobal (p));
        });
        return b;
    };
    for (int k = 0; k < seq::kSlotsPerBank; ++k)
    {
        patButtons << slotButton (false, k);
        sceneButtons << slotButton (true, k);
        grid->addWidget (sceneButtons.last(), 0, 1 + k);
        grid->addWidget (patButtons.last(), 1, 1 + k);
    }
    // Banques (4 x 16) et OFF (le groupe se tait)
    patBankButton = new QPushButton;
    sceneBankButton = new QPushButton;
    for (auto* b : { patBankButton, sceneBankButton })
    {
        b->setFocusPolicy (Qt::NoFocus);
        b->setFixedHeight (24);
        b->setFixedWidth (80);
        b->setToolTip (tr ("Bank: 16 slots each, 4 banks"));
    }
    connect (patBankButton, &QPushButton::clicked, this, [this] { patternBank = (patternBank + 1) % 4; refresh (true); });
    connect (sceneBankButton, &QPushButton::clicked, this, [this] { sceneBank = (sceneBank + 1) % 4; refresh (true); });
    grid->addWidget (sceneBankButton, 0, 17);       // scenes en haut, patterns dessous
    grid->addWidget (patBankButton, 1, 17);
    // Listes deroulantes : toutes les scenes / tous les patterns du groupe, et renommer
    sceneList = new QComboBox;
    patList = new QComboBox;
    for (auto* c : { sceneList, patList })
    {
        c->setFocusPolicy (Qt::NoFocus);
        c->setFixedHeight (24);
        c->setFixedWidth (150);                      // largeur fixe : la barre ne bouge pas
        c->setSizeAdjustPolicy (QComboBox::AdjustToMinimumContentsLengthWithIcon);
        c->view()->setMinimumWidth (220);             // la liste ouverte, elle, est large
    }
    sceneList->setToolTip (tr ("All the scenes: choose one to launch it"));
    patList->setToolTip (tr ("All the patterns of this group: choose one to play it"));
    connect (sceneList, &QComboBox::activated, this, [this] (int k)
    {
        const int i = sceneList->itemData (k).toInt();
        if (i >= 0)
        {
            sceneBank = i / seq::kSlotsPerBank;
            chooseScene (i);
        }
    });
    connect (patList, &QComboBox::activated, this, [this] (int k)
    {
        const int i = patList->itemData (k).toInt();
        if (i >= 0)
        {
            patternBank = i / seq::kSlotsPerBank;
            if (i != sequencer.currentPattern (state.group) || sequencer.groupOff (state.group))
                choosePattern (i);
        }
    });
    auto renameButton = [this] (const QString& tip)
    {
        auto* b = new QPushButton (tr ("Name"));
        b->setFocusPolicy (Qt::NoFocus);
        b->setFixedHeight (24);
        b->setToolTip (tip);
        return b;
    };
    auto* sceneRename = renameButton (tr ("Rename the current scene"));
    auto* patRename = renameButton (tr ("Rename the chosen pattern"));
    connect (sceneRename, &QPushButton::clicked, this, [this]
    {
        if (sequencer.currentScene() >= 0)
            renameScene (sequencer.currentScene());
        else
            emit message ("Launch a scene first");
    });
    connect (patRename, &QPushButton::clicked, this, [this] { renamePattern (state.group, sequencer.currentPattern (state.group)); });
    grid->addWidget (sceneList, 0, 18);
    grid->addWidget (sceneRename, 0, 19);
    grid->addWidget (patList, 1, 18);
    grid->addWidget (patRename, 1, 19);
    refresh (true);
}

QString PatternBar::patternLabel (int g, int index) const
{
    const QString name = QString::fromStdString (sequencer.patternName (g, index));
    return name.isEmpty() ? QString::number (index + 1) : name.left (6);
}

QString PatternBar::patternName (const seq::Sequencer& s, int g, int index)
{
    const QString name = QString::fromStdString (s.patternName (g, index));
    return name.isEmpty() ? QString ("Pat%1").arg (index + 1) : name;
}

QString PatternBar::sceneName (const seq::Sequencer& s, int index)
{
    const QString name = QString::fromStdString (s.scene (index).name);
    return name.isEmpty() ? QString ("S%1").arg (index + 1) : name;
}

void PatternBar::storeScene (int index)
{
    seq::Scene sc = sequencer.scene (index);
    sc.used = true;
    sc.pattern = sequencer.currentCombo();
    sequencer.setScene (index, sc);
    sequencer.setCurrentScene (index);
    emit message (QString ("SCENE %1 stored").arg (index + 1));
    refresh (true);
    emit changed();
}

void PatternBar::renamePattern (int g, int index)
{
    if (! sequencer.hasPattern (g, index))
        return;
    bool ok = false;
    const QString name = QInputDialog::getText (this, tr ("Pattern name"), tr ("Name of pattern %1 (8 letters):").arg (index + 1),
                                                QLineEdit::Normal, QString::fromStdString (sequencer.patternName (g, index)), &ok);
    if (! ok)
        return;
    sequencer.setPatternName (g, index, name.trimmed().left (8).toStdString());
    refresh (true);
    emit changed();
}

void PatternBar::renameScene (int index)
{
    seq::Scene sc = sequencer.scene (index);
    if (! sc.used)
        return;
    if (sc.locked)
    {
        emit message (QString ("SCENE %1 is locked").arg (index + 1));
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getText (this, tr ("Scene name"), tr ("Name of scene %1 (8 letters):").arg (index + 1),
                                                QLineEdit::Normal, QString::fromStdString (sc.name), &ok);
    if (! ok)
        return;
    sc.name = name.trimmed().left (8).toStdString();
    sequencer.setScene (index, sc);
    refresh (true);
    emit changed();
}

int PatternBar::freeSlot (int from) const
{
    for (int k = 1; k <= seq::kPatternSlots; ++k)
    {
        const int i = (from + k) % seq::kPatternSlots;
        if (! sequencer.hasPattern (state.group, i))
            return i;
    }
    return -1;
}

void PatternBar::refresh (bool force)
{
    const int g = state.group;
    const auto combo = sequencer.currentCombo();
    const int cur = sequencer.currentPattern (g), queued = sequencer.queuedPattern (g);
    const bool silent = sequencer.groupOff (g);
    const int sceneNow = sequencer.currentScene();
    // Signature de ce qui est affiche : on ne redessine que si elle change
    QString keyText = QString ("%1|%2|%3|%4|%5|%6|%7").arg (g).arg (cur).arg (queued).arg (silent).arg (sceneNow)
                          .arg (patternBank).arg (sceneBank);
    for (int k = 0; k < seq::kSlotsPerBank; ++k)
    {
        const int i = patternBank * seq::kSlotsPerBank + k;
        keyText += sequencer.hasPattern (g, i) ? QString::fromStdString (sequencer.patternName (g, i)) + "+" : "-";
        const seq::Scene sc = sequencer.scene (sceneBank * seq::kSlotsPerBank + k);
        keyText += sc.used ? QString::fromStdString (sc.name) + "*" : ".";
    }
    for (int v : combo)
        keyText += QString::number (v) + ",";
    if (! force && keyText == lastKey)
        return;
    lastKey = keyText;

    const QColor color = groupColor (g);
    patTitle->setText (tr ("PATTERNS %1").arg (session::groupTitle (state, g)));
    patTitle->setStyleSheet (QString ("QLabel { color: %1; } QToolTip { color: #f0f0f5; background: #202028; border: 1px solid #555; }").arg (css (color)));
    for (int k = 0; k < seq::kSlotsPerBank; ++k)
    {
        const int i = patternBank * seq::kSlotsPerBank + k;
        auto* b = patButtons[k];
        const bool exists = sequencer.hasPattern (g, i);
        const bool playingNow = exists && i == cur && ! silent;
        const bool waiting = i == queued;
        b->setText (exists ? patternLabel (g, i) : QString::number (i + 1));
        // Choisi mais eteint (silence) : seulement un cadre de la couleur du groupe
        const bool chosenOff = exists && i == cur && silent;
        QString style = playingNow ? QString ("QPushButton { background: %1; color: #ffffff; font-weight: bold; border: 2px solid %1; }").arg (css (color.darker (115)))
                      : chosenOff ? QString ("QPushButton { background: #1c1c22; color: %1; font-weight: bold; border: 2px solid %1; }").arg (css (color))
                      : exists ? QString ("QPushButton { background: #24242c; color: %1; border: 2px solid %2; }").arg (css (color.lighter (120)), css (color.darker (180)))
                               : QString ("QPushButton { background: #16161b; color: #555; border: 1px solid #2a2a32; }");
        style += "QPushButton { padding: 0px 2px; }"
                 "QToolTip { color: #f0f0f5; background: #202028; border: 1px solid #555; }";   // infobulle lisible
        if (waiting)                                 // en attente de la fin du pattern
            style += "QPushButton { border: 2px dashed #fc8c28; }";
        b->setStyleSheet (style);
        b->setToolTip (exists ? tr ("Pattern %1 %2 (%3 lines) \u00B7 click = play, again = off \u00B7 right-click = more")
                                    .arg (i + 1).arg (QString::fromStdString (sequencer.patternName (g, i)))
                                    .arg (sequencer.patternAt (g, i).length)
                              : tr ("Empty: click = new pattern"));
    }
    patBankButton->setText (tr ("Bank %1/4").arg (patternBank + 1));
    {
        // Listes : les patterns qui existent / les scenes rangees
        const QSignalBlocker b1 (patList), b2 (sceneList);
        patList->clear();
        for (int i = 0; i < seq::kPatternSlots; ++i)
            if (sequencer.hasPattern (g, i))
            {
                patList->addItem (QString ("%1  %2").arg (i + 1, 2).arg (patternName (sequencer, g, i)), i);
                if (i == cur)
                    patList->setCurrentIndex (patList->count() - 1);
            }
        sceneList->clear();
        sceneList->addItem (tr ("Scenes..."), -1);
        for (int i = 0; i < seq::kPatternSlots; ++i)
            if (sequencer.scene (i).used)
            {
                sceneList->addItem (QString ("%1  %2").arg (i + 1, 2).arg (sceneName (sequencer, i)), i);
                if (i == sceneNow)
                    sceneList->setCurrentIndex (sceneList->count() - 1);
            }
    }
    sceneBankButton->setText (tr ("Bank %1/4").arg (sceneBank + 1));
    for (int k = 0; k < seq::kSlotsPerBank; ++k)
    {
        const int i = sceneBank * seq::kSlotsPerBank + k;
        auto* b = sceneButtons[k];
        const seq::Scene sc = sequencer.scene (i);
        b->setText (sc.used ? (sc.name.empty() ? QString ("S%1").arg (i + 1) : QString::fromStdString (sc.name).left (6))
                            : QString ("S%1").arg (i + 1));
        // Scene en cours : tant que ce qui joue lui correspond (sinon : cadre seulement)
        bool matches = sc.used;
        for (int gg = 0; sc.used && gg < session::kGroups; ++gg)
            matches &= sc.pattern[(size_t) gg] == seq::kFree || sc.pattern[(size_t) gg] == combo[(size_t) gg];
        b->setStyleSheet (QString (i == sceneNow && matches ? "QPushButton { background: #e8e8f0; color: #111; font-weight: bold; }"
                                 : i == sceneNow ? "QPushButton { background: #2c2c36; color: #e8e8f0; border: 2px solid #e8e8f0; }"
                                 : sc.used ? "QPushButton { background: #2c2c36; color: #e8e8f0; border: 1px solid #555; }"
                                           : "QPushButton { background: #16161b; color: #555; border: 1px solid #2a2a32; }")
                          + "QPushButton { padding: 0px 2px; }"
                            "QToolTip { color: #f0f0f5; background: #202028; border: 1px solid #555; }");
        QString tip;
        if (sc.used)
            for (int gg = 0; gg < session::kGroups; ++gg)
            {
                const int p = sc.pattern[(size_t) gg];
                if (p == seq::kFree)
                    continue;
                tip += QString ("%1:%2 ").arg (groupKey (gg), p == seq::kOff ? QString ("off") : QString::number (p + 1));
            }
        b->setToolTip (sc.used ? tr ("Scene %1%3: %2").arg (i + 1).arg (tip.trimmed()).arg (sc.locked ? QString (" (locked)") : QString()) : tr ("Empty: click = new empty scene"));
    }
}

void PatternBar::choosePattern (int index)
{
    const int g = state.group;
    const bool playing = isPlaying && isPlaying();
    const bool existed = sequencer.hasPattern (g, index);
    // Le pattern choisi : un clic l'active, un 2e clic l'eteint (le groupe se tait)
    const int now = sequencer.currentCombo()[(size_t) g];
    if (existed && index == sequencer.currentPattern (g) && (now == index || now == seq::kOff))
    {
        const bool turnOff = now == index;
        sequencer.selectPattern (g, turnOff ? seq::kOff : index, playing);
        sequencer.setCurrentScene (-1);
        refresh (true);
        emit changed();
        emit message (QString ("%1 PATTERN %2 %3").arg (groupKey (g)).arg (index + 1).arg (turnOff ? "OFF" : "ON"));
        return;
    }
    sequencer.selectPattern (g, index, playing);     // vide : nouveau pattern (meme longueur)
    sequencer.setCurrentScene (-1);
    refresh (true);
    emit changed();
    emit message (QString ("%1 PATTERN %2%3%4").arg (groupKey (g)).arg (index + 1)
                      .arg (existed ? QString() : QString (" (new)"))
                      .arg (playing && sequencer.queuedPattern (g) == index ? QString (" : next") : QString()));
}

void PatternBar::chooseScene (int index)
{
    const bool playing = isPlaying && isPlaying();
    seq::Scene sc = sequencer.scene (index);
    if (! sc.used)                                   // vide : nouvelle scene, aucun pattern actif
    {
        sc.used = true;
        sc.pattern.fill (seq::kOff);
        sequencer.setScene (index, sc);
        sequencer.setCurrentScene (index);
        emit message (QString ("SCENE %1 new (empty): click its cells in the Scenes view").arg (index + 1));
    }
    else
    {
        sequencer.launchScene (index, playing);
        emit message (QString ("SCENE %1%2").arg (index + 1).arg (playing ? QString (" : next") : QString()));
    }
    refresh (true);
    emit changed();
}

void PatternBar::patternMenu (int index, const QPoint& at)
{
    const int g = state.group;
    const bool exists = sequencer.hasPattern (g, index);
    QMenu menu (this);
    auto* title = menu.addAction (tr ("Group %1, pattern %2").arg (groupKey (g)).arg (index + 1));
    title->setEnabled (false);
    menu.addSeparator();
    auto* play = menu.addAction (exists ? tr ("Play") : tr ("New Pattern"));
    const int cur = sequencer.currentPattern (g);
    auto* dupHere = menu.addAction (tr ("Duplicate pattern %1 here").arg (cur + 1));
    dupHere->setEnabled (index != cur);
    auto* dupNext = menu.addAction (tr ("Duplicate to the next free slot"));
    dupNext->setEnabled (exists);
    auto* rename = menu.addAction (tr ("Rename..."));
    rename->setEnabled (exists);
    menu.addSeparator();
    auto* erase = menu.addAction (index == cur ? tr ("Clear (stays chosen)") : tr ("Delete"));
    erase->setEnabled (exists);
    auto* picked = menu.exec (at);
    if (! picked)
        return;
    if (picked == play)
        choosePattern (index);
    else if (picked == dupHere || picked == dupNext)
    {
        const int from = picked == dupHere ? cur : index;
        const int to = picked == dupHere ? index : freeSlot (index);
        if (to < 0)
        {
            emit message ("No free pattern slot");
            return;
        }
        sequencer.setPatternAt (g, to, sequencer.patternAt (g, from));
        sequencer.setPatternName (g, to, sequencer.patternName (g, from));
        choosePattern (to);
        emit message (QString ("PATTERN %1 > %2").arg (from + 1).arg (to + 1));
    }
    else if (picked == rename)
        renamePattern (g, index);
    else if (picked == erase)
    {
        if (index == cur)
            emit aboutToChange (g);                  // le pattern choisi : annulable
        sequencer.removePattern (g, index);
        emit changed();
    }
    refresh (true);
}

void PatternBar::sceneMenu (int index, const QPoint& at)
{
    const seq::Scene sc = sequencer.scene (index);
    QMenu menu (this);
    auto* title = menu.addAction (tr ("Scene %1").arg (index + 1));
    title->setEnabled (false);
    menu.addSeparator();
    auto* launch = menu.addAction (sc.used ? tr ("Launch") : tr ("New empty scene"));
    auto* store = menu.addAction (sc.used ? tr ("Replace with the patterns playing now") : tr ("Store the patterns playing now"));
    store->setEnabled (! sc.locked);
    // Groupe choisi : libre (garde son pattern quand on lance la scene) ou fixe
    const int g = state.group;
    auto* freeGroup = menu.addAction (sc.used && sc.pattern[(size_t) g] == seq::kFree
                                          ? tr ("Group %1: fixed (pattern playing now)").arg (groupKey (g))
                                          : tr ("Group %1: free (keeps its pattern)").arg (groupKey (g)));
    freeGroup->setEnabled (sc.used && ! sc.locked);
    auto* dupNext = menu.addAction (tr ("Duplicate to the next free slot"));
    dupNext->setEnabled (sc.used);
    auto* rename = menu.addAction (tr ("Rename..."));
    rename->setEnabled (sc.used && ! sc.locked);
    auto* lock = menu.addAction (sc.locked ? tr ("Unlock") : tr ("Lock (no more changes)"));
    lock->setEnabled (sc.used);
    menu.addSeparator();
    auto* erase = menu.addAction (tr ("Delete"));
    erase->setEnabled (sc.used && ! sc.locked);
    auto* picked = menu.exec (at);
    if (! picked)
        return;
    seq::Scene edit = sc;
    if (picked == launch)
    {
        chooseScene (index);
        return;
    }
    if (picked == store)
    {
        const auto combo = sequencer.currentCombo();
        edit.used = true;
        for (int gg = 0; gg < session::kGroups; ++gg)       // les groupes libres le restent
            edit.pattern[(size_t) gg] = sc.used && sc.pattern[(size_t) gg] == seq::kFree ? seq::kFree : combo[(size_t) gg];
        sequencer.setScene (index, edit);
        sequencer.setCurrentScene (index);
    }
    else if (picked == freeGroup)
    {
        edit.pattern[(size_t) g] = sc.pattern[(size_t) g] == seq::kFree ? sequencer.currentCombo()[(size_t) g] : seq::kFree;
        sequencer.setScene (index, edit);
    }
    else if (picked == dupNext)
    {
        for (int k = 1; k < seq::kPatternSlots; ++k)
        {
            const int to = (index + k) % seq::kPatternSlots;
            if (! sequencer.scene (to).used)
            {
                seq::Scene copy = sc;
                copy.locked = false;                 // la copie se modifie
                sequencer.setScene (to, copy);
                emit message (QString ("SCENE %1 > %2").arg (index + 1).arg (to + 1));
                break;
            }
        }
    }
    else if (picked == rename)
    {
        renameScene (index);
        return;
    }
    else if (picked == lock)
    {
        edit.locked = ! sc.locked;
        sequencer.setScene (index, edit);
        emit message (QString ("SCENE %1 %2").arg (index + 1).arg (edit.locked ? "LOCKED" : "UNLOCKED"));
    }
    else if (picked == erase)
        sequencer.setScene (index, seq::Scene());
    refresh (true);
    emit changed();
}
