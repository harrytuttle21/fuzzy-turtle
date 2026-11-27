// Fuzzy John DAW Pulses — fenetre principale (squelette Qt). Licence : GPLv3.
#include "main_window.h"

#include "engine_link.h"
#include "maschine_link.h"
#include "mixer_view.h"
#include "pattern_view.h"
#include "tracker_view.h"
#include "step_view.h"
#include "pattern_bar.h"
#include "scene_view.h"
#include "palette_view.h"
#include "song_view.h"
#include "loop_dialog.h"
#include "piano_roll.h"
#include "generate_dialog.h"
#include "midi_import.h"
#include "keyboard_view.h"
#include "music_tables.h"
#include "note_dialog.h"
#include "scale_menu.h"
#include "settings_dialog.h"
#include "pad_grid.h"
#include "piano_keys.h"
#include "transport.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QDockWidget>
#include <QToolBar>
#include <QMenu>
#include <QCheckBox>
#include <QClipboard>
#include <QPixmap>
#include <QUrl>
#include <QDesktopServices>
#include <QScrollBar>
#include <QScreen>
#include <QGuiApplication>
#include <QRandomGenerator>
#include <QDateTime>
#include <QActionGroup>
#include <QWidgetAction>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QFileInfo>
#include <QMessageBox>
#include <QDir>
#include <QDrag>
#include <QShortcut>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QButtonGroup>
#include <QTabBar>
#include <QAbstractButton>
#include <QMimeData>
#include <QDialog>
#include <QScrollArea>
#include <QTableWidget>
#include <QListWidget>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QVBoxLayout>

#include <cstring>

using namespace session;
namespace fc = fuzzy::control;

namespace {
constexpr int kLayoutVersion = 8;                  // disposition des panneaux (2 : Pattern et Mixer en panneaux)

// Clavier musical de l'ordinateur : rangee du milieu = touches blanches, rangee du haut = noires.
struct KeyLayout { const char* notes; char octDown, octUp, velDown, velUp; };
const KeyLayout kAzerty { "qzsedftgyhujkolpm", 'w', 'x', 'c', 'v' };
const KeyLayout kQwerty { "awsedftgyhujkolp;", 'z', 'x', 'c', 'v' };

QString css (const QColor& c) { return c.name(); }

fc::Rgb rgb (const QColor& c) { return { (uint8_t) c.red(), (uint8_t) c.green(), (uint8_t) c.blue() }; }

} // namespace

MainWindow::MainWindow (const Options& options, QWidget* parent) : QMainWindow (parent), opts (options)
{
    setWindowTitle ("Fuzzy John DAW Pulses");
    sentReverse.fill (-1);                           // rien encore envoye au moteur
    for (auto& g : padGroup)
        g = 0;

    QSettings settings;
    engine = new EngineLink (this);
    transport = new Transport (*engine, sequencer, state, this);
    transport->setBpm (settings.value ("bpm", 120).toDouble());
    transport->setMetronome (settings.value ("metronome", true).toBool());
    transport->setMetronomeVolume (settings.value ("metronomeVolume", 90).toInt());
    state.octave = settings.value ("octave", 3).toInt();
    for (int g = 0; g < kGroups; ++g)
        state.groupVolume[(size_t) g] = settings.value (QString ("volume/%1").arg (groupName (g)), 100).toInt();
    state.masterVolume = settings.value ("volume/master", 100).toInt();

    // --- mise en page : au centre PATTERN / MIXER ; autour, des panneaux qu'on deplace,
    //     detache, masque ou redimensionne (vue par defaut proche de la Maschine :
    //     pads a droite, groupes en bas a gauche, clavier en bas, transport tout en bas)
    auto* top = new QToolBar (tr ("Top bar"));
    top->setObjectName ("topBarTool");
    top->setMovable (false);
    top->toggleViewAction()->setVisible (false);
    top->addWidget (buildTopBar());
    addToolBar (Qt::TopToolBarArea, top);
    // Barre PATTERNS / SCENES, sous la barre du haut (View > Patterns / Scenes pour la masquer)
    addToolBarBreak (Qt::TopToolBarArea);
    patternsToolBar = new QToolBar (tr ("Patterns / Scenes"));
    patternsToolBar->setObjectName ("patternsBar");
    patternsToolBar->setMovable (false);
    patternBar = new PatternBar (sequencer, state);
    patternBar->isPlaying = [this] { return transport && transport->playing(); };
    patternsToolBar->addWidget (patternBar);
    addToolBar (Qt::TopToolBarArea, patternsToolBar);
    connect (patternBar, &PatternBar::message, this, [this] (const QString& t) { toast (t); });
    connect (patternBar, &PatternBar::aboutToChange, this, [this] (int g) { remember (g); });
    connect (patternBar, &PatternBar::beforeEdit, this, [this] (int g, const QString& label) { rememberPatterns (g, label); });
    patternBar->startAudition = [this] (int g)       // Play d'un pattern : ce groupe seul
    {
        state.audition = g;
        if (! transport->playing())
            transport->play();
        if (patternManagerRefresh)
            patternManagerRefresh();
    };
    patternBar->stopPlayback = [this] { transport->stop(); };
    patternBar->endAudition = [this] { state.audition = -1; };
    connect (patternBar, &PatternBar::changed, this, &MainWindow::patternsChanged);

    // Tout est en panneaux (pas de zone centrale fixe) : Pattern et Mixer en onglets
    // Sans animation des panneaux : sur macOS, les separateurs animes laissaient un trait fin noir
    setDockOptions (AllowNestedDocks | AllowTabbedDocks);
    setTabPosition (Qt::AllDockWidgetAreas, QTabWidget::North);
    // Un seul clavier : l'onglet Keyboard. L'ancien panneau garde ses reglages (PC KEYS,
    // AZERTY / QWERTY), invisible ; il est cree avant l'onglet qui les reprend.
    auto* oldKeys = buildKeyboardPanel();
    oldKeys->setParent (this);
    oldKeys->hide();
    // Vue par defaut : en haut, Pattern / Piano Roll / Mixer en onglets sur toute la largeur ;
    // en bas, Groups | Keyboard | Pads cote a cote
    patternDock = makeDock (tr ("Pattern"), "patternDock", buildLeftPanel());
    mixerDock = makeDock (tr ("Mixer"), "mixerDock", buildMixer());
    trackerDock = makeDock (tr ("Tracker"), "trackerDock", buildTracker());   // avant le Piano Roll (Edit / Step partages)
    rollDock = makeDock (tr ("Piano Roll"), "pianoRollDock", buildPianoRoll());
    addDockWidget (Qt::TopDockWidgetArea, patternDock);
    tabifyDockWidget (patternDock, rollDock);       // onglets : Pattern, Piano Roll, Tracker, Mixer
    tabifyDockWidget (rollDock, trackerDock);
    tabifyDockWidget (trackerDock, mixerDock);
    sceneDock = makeDock (tr ("Scenes"), "scenesDock", buildScenes());
    tabifyDockWidget (mixerDock, sceneDock);        // onglet Scenes, apres le Mixer
    songDock = makeDock (tr ("Song"), "songDock", buildSong());
    tabifyDockWidget (sceneDock, songDock);         // onglet Song, apres Scenes
    groupsDock = makeDock (tr ("Groups"), "groupsDock", buildGroupBlock());
    keysTabDock = makeDock (tr ("Keyboard"), "keyboardTabDock", buildKeyboardTab());
    soundDock = makeDock (tr ("Sound"), "soundDock", buildSoundTab());
    pads = new PadGrid;
    auto* padsPanel = new QWidget;                   // pads, et dessous : tout reactiver
    auto* padsCol = new QVBoxLayout (padsPanel);
    padsCol->setContentsMargins (4, 2, 4, 2);
    padsCol->addWidget (pads, 1);
    {
        // En bas des pads : modes des pads, puis Unmute all / Unsolo all
        auto* row = new QHBoxLayout;
        row->setContentsMargins (0, 0, 0, 0);
        // Trois boutons, comme PAD MODE / KEYBOARD / CHORDS de la Maschine : le mode actif est orange
        padsOnlyButton = new QPushButton (tr ("Pad mode"));
        padModeButton = new QPushButton (tr ("Keyboard mode"));
        chordsModeButton = new QPushButton (tr ("Chords mode"));
        padsOnlyButton->setToolTip (tr ("Pads play their 16 sounds (Maschine: PAD MODE)"));
        padModeButton->setToolTip (tr ("Pads play the chosen sound on 16 notes of the scale (Maschine: KEYBOARD)"));
        chordsModeButton->setToolTip (tr ("Each pad plays a chord of the Chords menu (Maschine: CHORDS)"));
        connect (padsOnlyButton, &QPushButton::clicked, this, [this] { setPadKeyboard (false); });
        connect (padModeButton, &QPushButton::clicked, this, [this] { setPadKeyboard (true); });
        connect (chordsModeButton, &QPushButton::clicked, this, [this] { setPadChords (true); });
        int widest = 0;
        for (auto* b : { padsOnlyButton, padModeButton, chordsModeButton })
        {
            b->setFocusPolicy (Qt::NoFocus);
            b->setObjectName ("keys");
            b->setCheckable (true);
            b->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed);
            widest = std::max (widest, b->fontMetrics().horizontalAdvance (b->text()));
            row->addWidget (b);
        }
        Q_UNUSED (widest);
        QTimer::singleShot (0, this, [this]           // meme taille, mesuree avec le style de l'app
        {
            int w = 0;
            for (auto* b : { padsOnlyButton, padModeButton, chordsModeButton })
            {
                b->ensurePolished();
                w = std::max (w, b->sizeHint().width());
            }
            for (auto* b : { padsOnlyButton, padModeButton, chordsModeButton })
                b->setFixedWidth (w + 4);
        });
        padsOnlyButton->setChecked (true);
        row->insertStretch (0, 1);                   // les 3 boutons au centre
        row->addStretch (1);
        row->addWidget (muteSoloRow (padsUnmute, padsUnsolo, false, true));   // A/M sur A/S, a droite
        padsCol->addLayout (row);
    }
    padsDock = makeDock (tr ("Pads"), "padsDock", padsPanel);
    statusDock = makeDock (tr ("Status"), "statusDock", buildScreens());
    addDockWidget (Qt::BottomDockWidgetArea, groupsDock);
    addDockWidget (Qt::BottomDockWidgetArea, keysTabDock);
    splitDockWidget (groupsDock, keysTabDock, Qt::Horizontal);
    // Ligne d'aide, tout en bas : l'aide de la vue sous la souris (les barres gardent leur place)
    {
        auto* helpBar = new QToolBar (tr ("Help line"));
        helpBar->setObjectName ("helpLine");
        helpBar->setMovable (false);
        helpBar->setFloatable (false);
        // (taille demandee petite : un long texte n'elargit jamais la fenetre)
        struct HelpLabel : QLabel
        {
            using QLabel::QLabel;
            QSize sizeHint() const override { return { 200, QLabel::sizeHint().height() }; }
            QSize minimumSizeHint() const override { return { 0, QLabel::sizeHint().height() }; }
        };
        helpLine = new HelpLabel (tr ("<span style='color:#b8b8c4'>Move the mouse over a view to see its help here</span>"));
        helpLine->setTextFormat (Qt::RichText);
        helpLine->setStyleSheet ("QLabel { color: #b8b8c4; background: #1c1c22; padding: 3px 10px; font-size: 12px; }");
        helpLine->setSizePolicy (QSizePolicy::Expanding, QSizePolicy::Preferred);
        helpLine->setMinimumWidth (0);
        helpBar->addWidget (helpLine);
        helpBar->setStyleSheet ("QToolBar { background: #1c1c22; border: none; border-top: 1px solid #6a6a76; spacing: 0; padding: 0; }");   // trait au-dessus
        addToolBar (Qt::BottomToolBarArea, helpBar);
    }
    // Transport : sa propre fenetre, sous les groupes (detachable a part)
    transportDock = makeDock (tr ("Transport"), "transportDock", buildTransportBar());
    splitDockWidget (groupsDock, transportDock, Qt::Vertical);
    addDockWidget (Qt::BottomDockWidgetArea, padsDock);
    splitDockWidget (keysTabDock, padsDock, Qt::Horizontal);
    tabifyDockWidget (keysTabDock, soundDock);       // Keyboard et Sound en onglets (apres les pads)
    keysTabDock->raise();

    addDockWidget (Qt::RightDockWidgetArea, statusDock);
    statusDock->hide();                              // cachee AVANT de la detacher : sinon elle
    statusDock->setFloating (true);                  // s'affichait un instant au demarrage
    statusDock->resize (720, 190);
    statusDock->hide();

    // Transport : sous les groupes (panneau Groups), plus de barre en bas de la fenetre
    applyTheme();
    if (auto* screen = QGuiApplication::primaryScreen())   // grande fenetre au premier lancement
        resize (screen->availableGeometry().size() * 0.92);
    else
        resize (1280, 900);
    patternDock->raise();

    // --- menu (Mac : Fuzzy DAW Pulses > Settings..., Cmd + ,)
    auto* settingsAction = new QAction (tr ("Settings..."), this);
    settingsAction->setMenuRole (QAction::PreferencesRole);
    settingsAction->setShortcut (QKeySequence::Preferences);
    connect (settingsAction, &QAction::triggered, this, [this] { openSettings (0); });
    auto* fileMenu = menuBar()->addMenu (tr ("&File"));
    auto addFile = [this, fileMenu] (const QString& text, QKeySequence key, auto slot)
    {
        auto* a = fileMenu->addAction (text);
        a->setShortcut (key);
        connect (a, &QAction::triggered, this, slot);
    };
    addFile (tr ("New Project"), QKeySequence::New, [this] { newProject(); });
    addFile (tr ("Open Project..."), QKeySequence::Open, [this] { openProject(); });
    addFile (tr ("Save"), QKeySequence::Save, [this] { saveProject(); });
    addFile (tr ("Save As..."), QKeySequence::SaveAs, [this] { saveProjectAs(); });
    fileMenu->addAction (tr ("Save With Samples..."), this, [this] { saveProjectAs (true); });
    fileMenu->addSeparator();
    fileMenu->addAction (tr ("Export Group..."), this, [this] { exportGroup(); });   // un groupe entier dans un fichier
    fileMenu->addAction (tr ("Import Group..."), this, [this] { importGroup(); });
    fileMenu->addSeparator();
    fileMenu->addSeparator();
    fileMenu->addAction (tr ("Import MIDI..."), this, [this]
    {
        QSettings settings;
        const QString path = QFileDialog::getOpenFileName (this, tr ("Import MIDI"), settings.value ("midiDir").toString(),
                                                           tr ("MIDI files (*.mid *.midi)"));
        if (path.isEmpty())
            return;
        settings.setValue ("midiDir", QFileInfo (path).absolutePath());
        importMidi (path, state.sound);
    });
    fileMenu->addAction (settingsAction);            // Mac : range dans le menu de l'app (Cmd + ,)

    // Menu Help : a propos (lien GitHub), raccourcis
    auto* aboutMenu = menuBar()->addMenu (tr ("&Help"));
    auto* aboutAction = aboutMenu->addAction (tr ("About Fuzzy John DAW Pulses..."), this, [this] { showAbout(); });
    aboutMenu->addAction (tr ("Keyboard shortcuts..."), this, [this] { showShortcuts(); });
    aboutMenu->addAction (tr ("Fuzzy Turtle on GitHub"), this, []
    {
        QDesktopServices::openUrl (QUrl ("https://github.com/harrytuttle21/fuzzy-turtle"));
    });
    aboutAction->setMenuRole (QAction::NoRole);      // reste dans ce menu sur Mac
    auto* appAbout = new QAction (tr ("About Fuzzy John DAW Pulses"), this);   // et dans le menu de l'app (Mac)
    appAbout->setMenuRole (QAction::AboutRole);
    connect (appAbout, &QAction::triggered, this, [this] { showAbout(); });
    aboutMenu->addAction (appAbout);

    // Menu Edit : raccourcis habituels (Cmd sur Mac, Ctrl ailleurs ; Ctrl marche aussi sur Mac)
    auto* editMenu = menuBar()->addMenu (tr ("&Edit"));
    auto addEdit = [this, editMenu] (const QString& text, QKeySequence::StandardKey std, const char* macCtrl, auto slot)
    {
        auto* a = editMenu->addAction (text);
        QList<QKeySequence> keys { QKeySequence (std) };
#ifdef Q_OS_MACOS
        if (macCtrl)
            keys << QKeySequence (macCtrl);           // touche Ctrl physique du Mac
#else
        Q_UNUSED (macCtrl);
#endif
        a->setShortcuts (keys);
        connect (a, &QAction::triggered, this, slot);
    };
    addEdit (tr ("Undo"), QKeySequence::Undo, "Meta+Z", [this] { undo (false); });
    addEdit (tr ("Redo"), QKeySequence::Redo, "Meta+Shift+Z", [this] { undo (true); });
    editMenu->addSeparator();
    addEdit (tr ("Cut"), QKeySequence::Cut, "Meta+X", [this] { copySelection (true); });
    addEdit (tr ("Copy"), QKeySequence::Copy, "Meta+C", [this] { copySelection (false); });
    addEdit (tr ("Paste"), QKeySequence::Paste, "Meta+V", [this] { paste(); });
    addEdit (tr ("Select All"), QKeySequence::SelectAll, "Meta+A", [this] { selectAll(); });
    editMenu->addSeparator();
    auto* dbl = editMenu->addAction (tr ("Double Pattern / Duplicate Song blocks"), this, [this]
    {
        if (lastArea == Area::Song && songView && ! songView->visibleRegion().isEmpty())
            songDuplicateBlocks();                   // Song : Cmd+D duplique les blocs choisis
        else
            doublePattern();
    });
    dbl->setShortcut (QKeySequence ("Ctrl+D"));
    auto* genAction = editMenu->addAction (tr ("Generate Notes..."), this, [this] { openGenerate(); });
    genAction->setShortcut (QKeySequence ("Ctrl+G"));

    // Menu View : panneaux a montrer / cacher, vues (Default, Live, Mix, et les siennes)
    auto* viewMenu = menuBar()->addMenu (tr ("&View"));
    connect (viewMenu, &QMenu::aboutToShow, this, [this, viewMenu] { buildViewMenu (viewMenu); });
    buildViewMenu (viewMenu);                        // raccourcis actifs des le depart
    if (settings.contains ("windowGeometry"))
        restoreGeometry (settings.value ("windowGeometry").toByteArray());
    // Tailles par defaut une fois la fenetre affichee (avant, Qt ne connait pas la place),
    // puis la disposition de la derniere fois si elle existe
    QTimer::singleShot (0, this, [this]
    {
        resizeDocks ({ groupsDock, keysTabDock, padsDock }, { width() * 25 / 100, width() * 50 / 100, width() * 25 / 100 }, Qt::Horizontal);
        resizeDocks ({ patternDock, padsDock }, { height() * 57 / 100, height() * 43 / 100 }, Qt::Vertical);   // vue d'usine (mesuree sur la capture de reference)
        resizeDocks ({ groupsDock, transportDock }, { height(), transportDock->widget()->sizeHint().height() }, Qt::Vertical);
        defaultLayout = factoryLayout = saveState (kLayoutVersion);   // vue Default (d'usine)
        if (QSettings().contains ("layout/default"))  // « Save as Default » : la sienne
            defaultLayout = QSettings().value ("layout/default").toByteArray();
        QSettings s;
        if (s.contains ("windowState"))
            restoreState (s.value ("windowState").toByteArray(), kLayoutVersion);
        placeTracker();                              // disposition d'avant le Tracker : on l'ajoute
        // et une 2e fois quand les onglets sont dessines (sur Mac, ils arrivent un peu apres)
        QTimer::singleShot (250, this, [this] { placeTracker(); });
    });

    // Menu Settings bien visible, une entree par page
    auto* settingsMenu = menuBar()->addMenu (tr ("&Settings"));
    const QStringList pages { tr ("Audio..."), tr ("MIDI..."), tr ("Colors..."), tr ("Keys...") };
    for (int i = 0; i < pages.size(); ++i)
    {
        auto* a = settingsMenu->addAction (pages[i]);
        a->setMenuRole (QAction::NoRole);            // reste dans ce menu sur Mac
        connect (a, &QAction::triggered, this, [this, i] { openSettings (i); });
    }

    // --- projet : le dernier ouvert revient au demarrage ; « * » dans le titre si modifie
    const QString last = settings.value ("lastProject").toString();
    if (last.isEmpty() || ! openProjectFile (last, true))
    {
        project.fromApp (sequencer, state, bpmBox->value());
        project.markSaved();
    }
    projectTimer.setInterval (500);
    connect (&projectTimer, &QTimer::timeout, this, [this]
    {
        const bool m = project.modified (sequencer, state, bpmBox->value());
        if (m != projectModified)
        {
            projectModified = m;
            updateTitle();
        }
    });
    projectTimer.start();
    updateTitle();

    // --- traits parasites (macOS) : nettoyage au demarrage, F12 = diagnostic
    QTimer::singleShot (300, this, [this] { fixSeparators(); update(); });
    QTimer::singleShot (1500, this, [this] { fixSeparators(); update(); });
    auto* diag = new QShortcut (QKeySequence (Qt::Key_F12), this);
    connect (diag, &QShortcut::activated, this, &MainWindow::showDisplayDiagnostics);

    // --- charge de l'ordinateur (barre du haut), une fois par seconde
    statsTimer.setInterval (1000);
    connect (&statsTimer, &QTimer::timeout, this, &MainWindow::refreshStats);
    statsTimer.start();
    refreshStats();

    // --- pads et clavier de l'ecran
    connect (pads, &PadGrid::pressed, this, [this] (int pad, int vel)
    {
        lastPad = pad;
        playPad (pad, vel, true);
        pads->flash (pad, true);
        if (! padKeyboard)                           // KEYBOARD / CHORDS : le son joue reste le meme
            selectSound (pad);
    });
    connect (pads, &PadGrid::released, this, [this] (int pad)
    {
        playPad (pad, 0, false);
        pads->flash (pad, false);
    });
    connect (pads, &PadGrid::menuRequested, this, [this] (int pad, const QPoint& at)
    {
        if (padChords)                               // mode Chords : le menu des accords (User)
        {
            chordPadMenu (pad, at);
            return;
        }
        lastPad = pad;
        lastArea = Area::Pads;
        // Clic droit sur un pad : Mute (rouge), Solo (jaune), puis le sample.
        // Pads choisis (Shift / Cmd + clic) et pad clique parmi eux : le menu vaut pour tous.
        const size_t i = (size_t) (state.group * 16 + pad - 1);
        const int g = state.group;
        QList<int> targets = pads->selection();
        if (! targets.contains (pad))
            targets = { pad };
        const bool many = targets.size() > 1;
        auto padList = [&targets]
        {
            QStringList l;
            for (int p : targets)
                l << QString ("%1").arg (p, 2, 10, QChar ('0'));
            return l.join (" ");
        };
        QMenu menu (this);
        if (many)
        {
            auto* title = menu.addAction (tr ("%1 pads: %2").arg (targets.size()).arg (padList()));
            title->setEnabled (false);
            menu.addSeparator();
        }
        auto item = [&menu] (const QString& text, const QString& color, bool on)
        {
            auto* button = new QPushButton ((on ? QStringLiteral ("✓ ") : QStringLiteral ("   ")) + text);
            button->setFlat (true);
            button->setStyleSheet (QString ("QPushButton { color: %1; text-align: left; padding: 5px 14px; border: none; font-weight: bold; }"
                                            "QPushButton:hover { background: #2c2c36; }").arg (color));
            auto* action = new QWidgetAction (&menu);
            action->setDefaultWidget (button);
            menu.addAction (action);
            connect (button, &QPushButton::clicked, &menu, [&menu, action] { menu.setActiveAction (action); action->trigger(); menu.close(); });
            return action;
        };
        const QString num = many ? tr ("%1 pads").arg (targets.size()) : QString ("%1").arg (pad, 2, 10, QChar ('0'));
        auto* mute = item ("Mute " + num, "#e05050", state.soundMute[i]);
        auto* solo = item ("Solo " + num, "#e8c040", state.soundSolo[i]);
        menu.addSeparator();
        // Lecture du son : One-shot / Gate / Loop (la boucle demande un sample)
        auto* playGroup = new QActionGroup (&menu);
        QList<QAction*> modeActions;
        const QStringList modeNames { tr ("One-shot"), tr ("Gate"), tr ("Loop") };
        for (int m = 0; m < 3; ++m)
        {
            auto* a = menu.addAction (modeNames[m]);
            a->setCheckable (true);
            a->setChecked (state.playMode[i] == m);
            a->setEnabled (m != session::State::Loop || ! state.samples[i].isEmpty());
            playGroup->addAction (a);
            modeActions << a;
        }
        auto* editLoopAction = menu.addAction (tr ("Edit Loop..."));
        editLoopAction->setEnabled (! state.samples[i].isEmpty() && ! many);
        menu.addSeparator();
        auto* load = menu.addAction (tr ("Load Sample..."));
        load->setEnabled (! many);
        // Copier / couper / coller les sons (aussi vers un autre groupe)
        menu.addSeparator();
        auto* copySound = menu.addAction (many ? tr ("Copy %1 Sounds").arg (targets.size()) : tr ("Copy Sound"));
        auto* cutSound = menu.addAction (many ? tr ("Cut %1 Sounds (move)").arg (targets.size()) : tr ("Cut Sound (move)"));
        const int clipFirst = soundClips.empty() ? 0 : soundClips.front().first;
        auto* pasteSound = menu.addAction (soundClips.empty() ? tr ("Paste Sound")
                                           : soundClips.size() > 1 ? tr ("Paste %1 Sounds (from pad %2)").arg (soundClips.size()).arg (pad, 2, 10, QChar ('0'))
                                           : tr ("Paste Sound (%1 %2)").arg (groupName (soundClipGroup)).arg (clipFirst, 2, 10, QChar ('0')));
        pasteSound->setEnabled (! soundClips.empty() && ! (soundClipGroup == g && soundClips.size() == 1 && clipFirst == pad));
        // Remise a zero : tout en bas, dans un sous-menu (loin de Load Sample : pas de clic par erreur)
        menu.addSeparator();
        auto* reset = menu.addMenu (tr ("Remove / Reset Pad"));
        bool anySample = false;
        for (int p : targets)
            anySample |= ! state.samples[(size_t) state.index (g, p)].isEmpty();
        QAction* remove = anySample ? reset->addAction (many ? tr ("Remove Samples") : tr ("Remove Sample (%1)").arg (padName (g, pad))) : nullptr;
        if (remove)
            reset->addSeparator();
        auto* emptyPad = reset->addAction (many ? tr ("Empty Pads (no sound)") : tr ("Empty Pad (no sound)"));
        auto* defaultSound = reset->addAction (many ? tr ("Default Sounds") : tr ("Default Sound (%1)").arg (g >= 8 ? tr ("empty in A2-H2") : soundName (pad)));
        if (! many)
        {
            emptyPad->setEnabled (! (state.samples[i].isEmpty() && state.effSynth (g, pad) == 0));
            defaultSound->setEnabled (state.synth[i] != -1 || ! state.samples[i].isEmpty());
        }
        QAction* chosen = nullptr;
        for (auto* a : { mute, solo })
            connect (a, &QAction::triggered, &menu, [&chosen, a] { chosen = a; });
        if (auto* picked = menu.exec (at))
            chosen = picked;
        std::vector<std::pair<int, int>> undoPads;
        for (int p : targets)
            undoPads.push_back ({ g, p });
        if (chosen == mute || chosen == solo)
        {
            // Tous comme le pad clique (pas une bascule pad par pad)
            const bool on = ! (chosen == solo ? state.soundSolo[i].load() : state.soundMute[i].load());
            for (int p : targets)
                (chosen == solo ? state.soundSolo : state.soundMute)[(size_t) state.index (g, p)] = on;
            refreshMuteSolo();
        }
        else if (chosen == copySound || chosen == cutSound)
            copySounds (targets, chosen == cutSound);
        else if (chosen == pasteSound)
            pasteSounds (pad);
        else if (modeActions.contains (chosen))
        {
            const int m = (int) modeActions.indexOf (chosen);
            for (int p : targets)                        // Loop : seulement les pads avec un sample
                if (m != session::State::Loop || ! state.samples[(size_t) state.index (g, p)].isEmpty())
                    state.playMode[(size_t) state.index (g, p)] = m;
            syncSamples();
            refreshAll();
        }
        else if (chosen == editLoopAction)
            editLoop (pad);
        else if (chosen == load)
            loadSampleDialog (pad);
        else if (chosen && chosen == remove)
        {
            rememberSounds (undoPads, {}, "Remove sample");
            for (int p : targets)
                state.samples[(size_t) state.index (g, p)].clear();
            syncSamples();
            refreshAll();
        }
        else if (chosen == emptyPad || chosen == defaultSound)
        {
            rememberSounds (undoPads, {}, chosen == emptyPad ? QString ("Empty pad") : QString ("Default sound"));
            for (int p : targets)
            {
                const size_t k = (size_t) state.index (g, p);
                state.synth[k] = chosen == emptyPad ? 0 : -1;   // 0 = aucun instrument (« None » du pilote)
                state.samples[k].clear();
            }
            syncSamples();
            refreshAll();
        }
    });
    connect (pads, &PadGrid::midiDropped, this, &MainWindow::importMidi);
    connect (pads, &PadGrid::soundDropped, this, &MainWindow::soundDropMenu);
    connect (pads, &PadGrid::filesDropped, this, [this] (int pad, const QStringList& files)
    {
        // Un fichier = ce pad ; plusieurs = ce pad et les suivants (par ordre de nom)
        for (int k = 0; k < files.size() && pad + k <= 16; ++k)
            setPadSample (state.group, pad + k, files[k]);
        selectSound (pad);
    });
    connect (piano, &PianoKeys::noteOn, this, [this] (int n, int v) { playKey (n, v, true); piano->setHeld (n, true); });
    connect (piano, &PianoKeys::noteOff, this, [this] (int n) { playKey (n, 0, false); piano->setHeld (n, false); });

    // --- transport
    connect (transport, &Transport::beat, this, [this] (int b)
    {
        beatShown = b;
        QString dots;
        for (int i = 0; i < 4; ++i)
            dots += i == b ? QStringLiteral ("● ") : QStringLiteral ("○ ");
        beatLabel->setText (dots.trimmed());
        if (maschine)
            maschine->updateScreens();
    });
    connect (transport, &Transport::played, this, [this] (int g, int snd, int note, int gateMs)
    {
        if (g == state.group && snd == state.sound)  // le son choisi : ses notes s'allument
            keyboardView->flash (note, gateMs);
    });
    connect (transport, &Transport::row, this, [this] (int r)
    {
        const int local = sequencer.localRow (state.group, r);   // ligne dans le pattern du groupe
        patternView->setPlayRow (local);
        pianoRoll->setPlayRow (local);
        tracker->setPlayRow (local);
        rollTracker->setPlayRow (local);
        stepView->setPlayRow (local);
        if (patternBar)
            patternBar->refresh();                   // pattern en attente : il vient de commencer ?
    });
    connect (transport, &Transport::playingChanged, this, [this] (bool on)
    {
        if (! on)
            state.audition = -1;                     // fin de l'ecoute : tous les groupes rejouent
        QTimer::singleShot (0, this, [this]          // Play / Stop des listes
        {
            if (patternManagerRefresh)
                patternManagerRefresh();
            if (sceneListRefresh)
                sceneListRefresh();
        });
        patternView->setPlaying (on);
        pianoRoll->setPlaying (on);
        tracker->setPlaying (on);
        rollTracker->setPlaying (on);
        stepView->setPlaying (on);
        for (auto& t : takeSaved)                     // nouvelle prise a chaque lecture
            t = false;
        playButton->setChecked (on);
        playButton->setText (on ? QStringLiteral ("\u275A\u275A") : QStringLiteral ("\u25B6"));   // pause / lecture
        if (! on)
            beatLabel->setText (QStringLiteral ("○ ○ ○ ○"));
        refreshMaschine();
    });

    // --- moteur audio
    connect (engine, &EngineLink::stateChanged, this, &MainWindow::refreshEngine);
    connect (engine, &EngineLink::stateChanged, this, [this] { syncSamples(); });   // moteur pret : ses samples
    connect (engine, &EngineLink::sampleError, this, [this] (const QString& path)
    {
        statusBar()->showMessage (tr ("Unreadable sample: %1").arg (QFileInfo (path).fileName()), 4000);
    });
    connect (engine, &EngineLink::meters, mixer, &MixerView::setLevels);
    connect (engine, &EngineLink::stopped, this, [this] (const QString& why)
    {
        engineLabel->setText (why);
        sentSamples = {};                              // un moteur relance ne connait plus rien
        sentLoops = {};
        sentReverse.fill (-1);
    });
    if (opts.engine && ! engine->start (settings.value ("output").toString()))
        engineLabel->setText (engine->error());

    // --- Maschine
    maschine = new MaschineLink (this);
    maschine->setPadHandler ([this] (int pad, int vel, bool down) { playPad (pad, vel, down); });
    connect (maschine, &MaschineLink::pad, this, [this] (int pad, int, bool down)
    {
        pads->flash (pad, down);
        if (down && ! padKeyboard)                   // KEYBOARD / CHORDS : le son joue reste le meme
            selectSound (pad);
        maschine->updateLights();
    });
    connect (maschine, &MaschineLink::button, this, &MainWindow::onMaschineButton);
    connect (maschine, &MaschineLink::knob, this, &MainWindow::soundKnob);
    connect (maschine, &MaschineLink::jog, this, [this] (int d)
    {
        if (maschine->property ("tempoHeld").toBool())
            bpmBox->setValue (bpmBox->value() + d);
        else if (maschine->property ("swingHeld").toBool())   // SWING tenu + molette : swing
        {
            swingBox->setValue (swingBox->value() + d);
            toast (QString ("SWING %1 %").arg (swingBox->value()));
        }
        else if (maschine->property ("tapHeld").toBool())
        {
            metroVolume->setValue (metroVolume->value() + 4 * d);
            tapTurned = true;
        }
        else
            selectSound (qBound (1, state.sound + d, 16));
    });
    connect (maschine, &MaschineLink::statusChanged, this, [this] { maschineLabel->setText (maschine->status()); });
    maschine->setLightPainter ([this] (fc::Mk3Lights& l)
    {
        const int g = state.group, hue = groupHue (g);
        for (int p = 1; p <= 16; ++p)                 // son coupe : eteint ; solo : vif
        {
            const char tag = state.soundTag (g, p);
            if (padChords)                            // mode CHORDS : pads avec accord doux, le 1er vif
            {
                l.setPad (p, hue, padChordKey (p) < 1 ? fc::Shade::Off : p == 1 ? fc::Shade::Bright : fc::Shade::Dim);
                continue;
            }
            if (padKeyboard)                          // mode KEYBOARD : toniques vives, autres notes douces
            {
                const bool playable = state.hasInstrument[(size_t) state.index (g, state.sound)];
                l.setPad (p, hue, ! playable ? fc::Shade::Off
                                  : (padNote (p) - keySettings.root) % 12 == 0 ? fc::Shade::Bright : fc::Shade::Dim);
                continue;
            }
            l.setPad (p, hue, tag == 'M' || tag == 'm' || ! state.hasInstrument[(size_t) state.index (g, p)] ? fc::Shade::Off
                              : p == state.sound || tag == 'S' ? fc::Shade::Bright : fc::Shade::Dim);
        }
        const int bank = g / 8 * 8;
        for (int i = 0; i < 8; ++i)
            l.setGroup (i, groupHue (bank + i), ! state.groupAudible (bank + i) ? fc::Shade::Off
                                               : bank + i == g || state.groupSolo[(size_t) (bank + i)] ? fc::Shade::Bright : fc::Shade::Dim);
        l.setButton ("mute", muteHeld ? fc::Shade::Bright : fc::Shade::Dim);
        l.setButton ("solo", soloHeld ? fc::Shade::Bright : fc::Shade::Dim);
        // Vues de l'app et nouveautes : vif = la vue affichee
        auto shown = [] (QWidget* w) { return w && ! w->visibleRegion().isEmpty(); };
        l.setButton ("pattern", patternHeld || shown (patternView) ? fc::Shade::Bright : fc::Shade::Dim);
        {
            bool anyScene = false;                   // SCENE : vif tenu ou scene en cours, doux s'il y en a
            for (int i = 0; i < seq::kPatternSlots && ! anyScene; ++i)
                anyScene = sequencer.scene (i).used;
            l.setButton ("scene", sceneHeld || shown (sceneView) || sequencer.currentScene() >= 0 ? fc::Shade::Bright
                                  : anyScene ? fc::Shade::Dim : fc::Shade::Off);
        }
        l.setButton ("swing", state.swing.load() > 0 ? fc::Shade::Bright : fc::Shade::Dim);
        l.setButton ("arranger", shown (songView) || sequencer.songMode() ? fc::Shade::Bright : fc::Shade::Dim);
        l.setButton ("events", shown (pianoRoll) ? fc::Shade::Bright : fc::Shade::Dim);
        l.setButton ("mixer", shown (mixer) ? fc::Shade::Bright : fc::Shade::Dim);
        l.setButton ("keyboard", padKeyboard && ! padChords ? fc::Shade::Bright : fc::Shade::Dim);
        l.setButton ("chords", padChords ? fc::Shade::Bright : fc::Shade::Dim);
        l.setButton ("pad_mode", padKeyboard ? fc::Shade::Dim : fc::Shade::Bright);
        l.setButton ("plug_in", shown (soundUi.title) ? fc::Shade::Bright : fc::Shade::Dim);
        l.setButton ("variation", fc::Shade::Dim);
        l.setButton ("sampling", fc::Shade::Dim);
        l.setButton ("play", transport->playing() ? fc::Shade::Bright : fc::Shade::Dim);
        l.setButton ("stop", fc::Shade::Dim);
        l.setButton ("restart", fc::Shade::Dim);
        l.setButton ("rec", recButton->isChecked() ? fc::Shade::Bright : fc::Shade::Dim);
        l.setButton ("tap", transport->metronome() ? fc::Shade::Bright : fc::Shade::Dim);
        l.setButton ("tempo", fc::Shade::Dim);
        l.setButton ("shift", fc::Shade::Dim);
    });
    maschine->setScreenPainter ([this] (fc::Canvas& left, fc::Canvas& right)
    {
        const QColor acc = groupColor (state.group);
        // Ecran gauche : les 16 sons du groupe, comme les pads
        left.fill (fc::kBlack);
        left.fillRect (0, 0, 480, 26, rgb (acc));
        left.text (8, 5, "FUZZY JOHN  DAW PULSES", fc::kBlack);
        const std::string groupText = ("Group " + groupTitle (state, state.group)).toStdString();
        left.text (480 - 8 - fc::Canvas::textWidth (groupText), 5, groupText, fc::kBlack);
        for (int p = 1; p <= 16; ++p)
        {
            const int x = 8 + (p - 1) % 4 * 118, y = 34 + (3 - (p - 1) / 4) * 58;
            const bool chosen = p == state.sound;
            left.fillRect (x, y, 110, 50, chosen ? rgb (acc) : fc::Rgb { 26, 26, 34 });
            left.frameRect (x, y, 110, 50, rgb (acc.darker (150)));
            // Numero du pad petit, en haut a gauche (gris) ; nom du sample centre, sur 2 lignes s'il le faut
            const fc::Rgb ink = chosen ? fc::kBlack : fc::kWhite;
            left.text (x + 3, y + 2, QString ("%1").arg (p, 2, 10, QChar ('0')).toStdString(),
                       chosen ? fc::Rgb { 40, 40, 40 } : fc::Rgb { 120, 120, 130 });
            const int perLine = 13;                      // 13 caracteres de 8 px dans 110 px
            QString name = padLabel (p).simplified();
            QStringList lines;
            if (name.size() <= perLine)
                lines << name;
            else
            {
                int cut = (int) name.left (perLine + 1).lastIndexOf (' ');   // couper a un espace si possible
                if (cut <= 0)
                    cut = perLine;
                lines << name.left (cut).trimmed() << name.mid (cut).trimmed().left (perLine);
            }
            const int top = lines.size() == 1 ? y + 21 : y + 15;
            for (int k = 0; k < lines.size(); ++k)
            {
                const std::string t = lines[k].toStdString();
                left.text (x + (110 - fc::Canvas::textWidth (t)) / 2, top + k * 14, t, ink);
            }
        }
        // Ecran droit, PATTERN ou SCENE tenu : les 16 noms de la banque, places comme les pads
        if (patternHeld || sceneHeld)
        {
            const bool scenes = sceneHeld;
            const int bank = scenes ? patternBar->sceneBank : patternBar->patternBank;
            const int g = state.group;
            right.fill (fc::kBlack);
            right.fillRect (0, 0, 480, 26, rgb (acc));
            const std::string head = scenes ? QString ("SCENES  bank %1/4").arg (bank + 1).toStdString()
                                            : QString ("PATTERNS %1  bank %2/4").arg (groupTitle (state, g)).arg (bank + 1).toStdString();
            right.text (8, 5, head, fc::kBlack);
            const int nowIndex = scenes ? sequencer.currentScene() : (sequencer.groupOff (g) ? -1 : sequencer.currentPattern (g));
            const int queued = scenes ? -1 : sequencer.queuedPattern (g);
            for (int k = 0; k < 16; ++k)
            {
                const int pad = 13 + k % 4 - 4 * (k / 4);   // pad 13 en haut a gauche
                const int i = bank * 16 + pad - 1;
                const int x = 4 + (k % 4) * 119, y = 32 + (k / 4) * 60;
                const bool used = scenes ? sequencer.scene (i).used : sequencer.hasPattern (g, i);
                const bool now = i == nowIndex;
                right.fillRect (x, y, 112, 54, now ? rgb (acc) : used ? fc::Rgb { 44, 44, 54 } : fc::Rgb { 18, 18, 22 });
                if (i == queued)                     // en attente : cadre blanc
                {
                    right.fillRect (x, y, 112, 2, fc::kWhite);
                    right.fillRect (x, y + 52, 112, 2, fc::kWhite);
                    right.fillRect (x, y, 2, 54, fc::kWhite);
                    right.fillRect (x + 110, y, 2, 54, fc::kWhite);
                }
                const fc::Rgb ink = now ? fc::kBlack : used ? fc::kWhite : fc::Rgb { 90, 90, 100 };
                const std::string num = (scenes ? QString ("S%1").arg (i + 1) : QString::number (i + 1)).toStdString();
                right.text (x + 4, y + 4, num, ink);
                if (used)
                {
                    QString name = scenes ? QString::fromStdString (sequencer.scene (i).name) : QString::fromStdString (sequencer.patternName (g, i));
                    if (name.isEmpty() && ! scenes)
                        name = QString ("Pat%1").arg (i + 1);
                    if (scenes && sequencer.scene (i).locked)
                        right.text (x + 112 - 4 - fc::Canvas::textWidth ("LOCK"), y + 4, "LOCK", ink);
                    const std::string t = name.left (13).toStdString();
                    right.text (x + (112 - fc::Canvas::textWidth (t)) / 2, y + 26, t, ink);
                }
            }
            return;
        }
        // Ecran droit, vue Song montree (ARRANGER) : le morceau, comme le pilote
        if (arrangerMode)
        {
            const auto song = sequencer.song();
            int total = 0;
            for (const auto& e : song)
                total += sequencer.entryRows (e);
            right.fill (fc::kBlack);
            right.fillRect (0, 0, 480, 26, rgb (acc));
            right.text (8, 5, QString ("ARRANGER  %1 bars").arg (total / double (seq::kRowsPerBar)).toStdString(), fc::kBlack);
            right.text (8, 34, ((sequencer.songMode() ? QString ("SONG ON   ") : QString ("SONG off  "))
                                   + (sequencer.songLoop() ? "LOOP" : "no loop") + (songHalf ? "   HALF" : "")).toStdString(), fc::kWhite);
            if (song.empty())
                right.text (8, 70, "Empty song: SELECT + pad = add a scene", fc::kWhite);
            int playEntry = -1, playRow = 0;
            if (transport->playing() && sequencer.songMode())
                sequencer.songPosition (playEntry, playRow);
            const int sel = std::clamp (songView->selected, 0, std::max (0, (int) song.size() - 1));
            const int first = std::clamp (sel - 3, 0, std::max (0, (int) song.size() - 8));
            for (int k = 0; k < 8 && first + k < (int) song.size(); ++k)
            {
                const int i = first + k;
                const auto& e = song[(size_t) i];
                const int y = 56 + k * 26;
                if (i == sel)
                    right.fillRect (0, y - 3, 480, 24, fc::Rgb { 60, 60, 72 });
                const QString name = QString::fromStdString (sequencer.scene (e.scene).name);
                const QString len = e.bars > 0 ? QString ("%1 bars").arg (e.bars) : QString ("x%1").arg (e.repeats);
                right.text (8, y, QString ("%1%2  S%3 %4   %5").arg (i == playEntry ? "> " : "  ").arg (i + 1, 2)
                                      .arg (e.scene + 1).arg (name).arg (len).toStdString(),
                            i == playEntry ? rgb (acc) : fc::kWhite);
            }
            return;
        }
        // Ecran droit : transport
        right.fill (fc::kBlack);
        right.fillRect (0, 0, 480, 26, rgb (acc));
        right.text (8, 5, "TRANSPORT", fc::kBlack);
        right.text (8, 50, QString ("%1 BPM").arg (transport->bpm(), 0, 'f', 0).toStdString(), fc::kWhite, 3);
        right.text (8, 120, transport->playing() ? "PLAY" : "STOP", transport->playing() ? rgb (acc) : fc::kWhite, 2);
        for (int i = 0; i < 4; ++i)
            right.fillRect (200 + i * 40, 122, 28, 28, transport->playing() && i == beatShown ? rgb (acc) : fc::Rgb { 40, 40, 48 });
        {
            // Scene en cours et pattern du groupe choisi (noms)
            const int sc = sequencer.currentScene();
            const QString sceneText = sc < 0 ? QString ("-")
                                    : QString ("S%1 %2").arg (sc + 1).arg (QString::fromStdString (sequencer.scene (sc).name));
            right.text (8, 92, ("SCENE   " + sceneText.trimmed()).toStdString(), fc::kWhite);
            const int g = state.group;
            const int pi = sequencer.currentPattern (g);
            QString pn = QString::fromStdString (sequencer.patternName (g, pi));
            if (pn.isEmpty())
                pn = QString ("Pat%1").arg (pi + 1);
            const QString patText = sequencer.groupOff (g) ? QString ("off") : QString ("%1 %2").arg (pi + 1).arg (pn);
            right.text (8, 160, QString ("PATTERN %1 : %2").arg (groupTitle (state, g), patText).toStdString(), fc::kWhite);
        }
        right.text (8, 180, QString ("Metronome %1  vol %2").arg (transport->metronome() ? "ON" : "OFF")
                                .arg (transport->metronomeVolume()).toStdString(), fc::kWhite);
        right.text (8, 204, QString ("Keyboard : sound %1, octave %2").arg (state.sound.load())
                                .arg (state.octave.load()).toStdString(), fc::Rgb { 170, 170, 170 });
        // Bas : message de 2 s, sinon le mode du son choisi
        const bool showToast = QDateTime::currentMSecsSinceEpoch() < toastUntil;
        const QString bottom = showToast ? toastText
                             : QString ("Sound %1 : %2").arg (state.sound.load(), 2, 10, QChar ('0'))
                                   .arg (QStringList { "ONE-SHOT", "GATE", "LOOP" }[state.mode (state.group, state.sound)]);
        right.text (8, 244, bottom.toStdString(), showToast ? rgb (acc) : fc::Rgb { 140, 140, 140 }, showToast ? 2 : 1);
    });
    if (opts.maschine)
        maschine->open();
    maschineLabel->setText (opts.maschine ? maschine->status() : tr ("Maschine: not opened (--no-maschine)"));
    maschineButton->setText (maschine->isOpen() ? tr ("Give back to NI") : tr ("Connect"));

    qApp->installEventFilter (this);                 // clavier de l'ordinateur, ou que soit le focus
    pads->setFocusPolicy (Qt::StrongFocus);
    pads->setFocus();
    selectGroup (settings.value ("group", 0).toInt());
    selectSound (state.sound);
    updateKeyLabels();
    refreshEngine();
}

MainWindow::~MainWindow()
{
    qApp->removeEventFilter (this);
    patternManagerRefresh = nullptr;
    sceneListRefresh = nullptr;
    delete patternManagerDialog;                     // avant nos membres (sa fermeture les touche)
    delete sceneListDialog;
}

// ---------------------------------------------------------------------------------------- UI

QWidget* MainWindow::buildTopBar()
{
    auto* bar = new QFrame;
    bar->setObjectName ("topBar");
    auto* row = new QHBoxLayout (bar);
    row->setContentsMargins (12, 6, 12, 6);
    auto* title = new QLabel ("<b>FUZZY JOHN</b> &nbsp;DAW Pulses");
    title->setObjectName ("title");
    statsLabel = new QLabel;
    statsLabel->setObjectName ("stats");
    statsLabel->setToolTip (tr ("Computer CPU and memory, audio engine load, memory of this app"));
    projectLabel = new QLabel;
    projectLabel->setObjectName ("projectName");
    projectLabel->setToolTip (tr ("Project (File > Save, Cmd+S)"));
    row->addWidget (title);
    row->addSpacing (18);
    row->addWidget (projectLabel);
    row->addStretch();
    row->addWidget (statsLabel);
    return bar;
}

QWidget* MainWindow::buildScreens()
{
    // Deux "ecrans" cote a cote, comme ceux de la Maschine
    auto* box = new QWidget;
    auto* row = new QHBoxLayout (box);
    row->setContentsMargins (0, 0, 0, 0);
    row->setSpacing (14);

    leftScreen = new QFrame;
    leftScreen->setObjectName ("screen");
    auto* l = new QVBoxLayout (leftScreen);
    auto* soundTitle = new QLabel (tr ("SOUND"));
    soundTitle->setObjectName ("section");
    soundLabel = new QLabel;
    soundLabel->setWordWrap (true);
    auto* maschineTitle = new QLabel (tr ("MASCHINE"));
    maschineTitle->setObjectName ("section");
    maschineLabel = new QLabel;
    maschineLabel->setWordWrap (true);
    maschineButton = new QPushButton;
    maschineButton->setFocusPolicy (Qt::NoFocus);
    connect (maschineButton, &QPushButton::clicked, this, [this]
    {
        if (maschine->isOpen())
            maschine->close();
        else
            maschine->open();
        maschineLabel->setText (maschine->isOpen() ? maschine->status() : tr ("Maschine given back to the NI software"));
        maschineButton->setText (maschine->isOpen() ? tr ("Give back to NI") : tr ("Connect"));
    });
    auto* mrow = new QHBoxLayout;
    mrow->addWidget (maschineLabel, 1);
    mrow->addWidget (maschineButton);
    l->addWidget (soundTitle);
    l->addWidget (soundLabel);
    l->addStretch();
    l->addWidget (maschineTitle);
    l->addLayout (mrow);

    auto* rightScreen = new QFrame;
    rightScreen->setObjectName ("screen");
    auto* r = new QVBoxLayout (rightScreen);
    auto* audioTitle = new QLabel (tr ("AUDIO"));
    audioTitle->setObjectName ("section");
    auto* audioSettings = new QPushButton (QStringLiteral ("\u2699  ") + tr ("Settings"));
    audioSettings->setFocusPolicy (Qt::NoFocus);
    audioSettings->setToolTip (tr ("Audio output, input, sample rate, buffer"));
    connect (audioSettings, &QPushButton::clicked, this, [this] { openSettings (0); });
    engineLabel = new QLabel (tr ("Audio engine: starting..."));
    engineLabel->setWordWrap (true);
    auto* audioHead = new QHBoxLayout;
    audioHead->addWidget (audioTitle);
    audioHead->addStretch();
    audioHead->addWidget (audioSettings);
    r->addLayout (audioHead);
    r->addWidget (engineLabel);
    r->addStretch();

    row->addWidget (leftScreen, 1);
    row->addWidget (rightScreen, 1);
    return box;
}

QWidget* MainWindow::buildGroupBlock()
{
    // 8 boutons de groupe en 2 rangees de 4 (comme la Maschine) + la banque A2-H2
    auto* panel = new QFrame;
    panel->setObjectName ("panel");
    auto* col = new QVBoxLayout (panel);
    auto* head = new QHBoxLayout;
    auto* title = new QLabel (tr ("GROUPS"));
    title->setObjectName ("section");
    bankButton = new QPushButton (tr ("A2 - H2"));
    bankButton->setCheckable (true);
    bankButton->setFocusPolicy (Qt::NoFocus);
    bankButton->setToolTip (tr ("Second bank of groups (Maschine: Shift + group)"));
    connect (bankButton, &QPushButton::clicked, this, [this] (bool bank2)
    {
        selectGroup (state.group % 8 + (bank2 ? 8 : 0));
    });
    head->addWidget (title);
    head->addStretch();
    col->addLayout (head);
    auto* grid = new QGridLayout;
    grid->setSpacing (8);
    for (int i = 0; i < 8; ++i)
    {
        auto* b = new QPushButton;
        b->setCheckable (true);
        b->setFocusPolicy (Qt::NoFocus);
        b->setFixedHeight (58);                      // toujours la place de 2 lignes (lettre + nom) : rien ne bouge
        b->setAcceptDrops (true);                    // pad ou groupe lache dessus
        b->installEventFilter (this);                // glisser le groupe (copier / deplacer)
        b->setProperty ("groupIndex", i);
        connect (b, &QPushButton::clicked, this, [this, i] { selectGroup (i + (state.group >= 8 ? 8 : 0)); });
        b->setContextMenuPolicy (Qt::CustomContextMenu);   // clic droit : Mute (rouge), Solo (jaune)
        connect (b, &QPushButton::customContextMenuRequested, this, [this, i, b] (const QPoint& p)
        {
            const int g = i + (state.group >= 8 ? 8 : 0);
            const int count = (int) groupTargets (g).size();
            const bool several = count > 1;              // groupes choisis : « Mute 3 groups »...
            QMenu menu (this);
            QAction* chosen = nullptr;
            QList<QAction*> items;
            for (const bool solo : { false, true })
            {
                const bool on = solo ? state.groupSolo[(size_t) g].load() : state.groupMute[(size_t) g].load();
                auto* button = new QPushButton ((on ? QStringLiteral ("\u2713 ") : QStringLiteral ("   "))
                                                + (solo ? "Solo " : "Mute ") + (several ? QString ("%1 groups").arg (count) : groupName (g)));
                button->setFlat (true);
                button->setStyleSheet (QString ("QPushButton { color: %1; text-align: left; padding: 5px 14px; border: none; font-weight: bold; }"
                                                "QPushButton:hover { background: #2c2c36; }").arg (solo ? "#e8c040" : "#e05050"));
                auto* action = new QWidgetAction (&menu);
                action->setDefaultWidget (button);
                menu.addAction (action);
                items << action;
                connect (button, &QPushButton::clicked, &menu, [&menu, &chosen, action] { chosen = action; menu.close(); });
            }
            menu.addSeparator();
            const QList<int> targets = groupTargets (g);
            QStringList names;
            for (int t : targets)
                names << groupName (t);
            auto* copyGroup = menu.addAction (several ? tr ("Copy %1 groups").arg (count) : tr ("Copy Group %1").arg (groupName (g)));
            auto* cutGroup = menu.addAction (several ? tr ("Cut %1 groups (move)").arg (count) : tr ("Cut Group %1 (move)").arg (groupName (g)));
            QStringList clipNames;
            for (int t : groupClips)
                clipNames << groupName (t);
            auto* pasteGroup = menu.addAction (groupClips.empty() ? tr ("Paste Group")
                                               : groupClips.size() > 1 ? tr ("Paste %1 groups here").arg (groupClips.size()) : tr ("Paste Group %1 here").arg (clipNames.join (" ")));
            pasteGroup->setEnabled (! groupClips.empty() && ! (groupClips.size() == 1 && groupClips[0] == g));
            menu.addSeparator();
            auto* renameAction = menu.addAction (tr ("Rename Group %1...").arg (groupName (g)));
            auto* exportAction = menu.addAction (tr ("Export Group %1...").arg (groupName (g)));
            auto* importAction = menu.addAction (tr ("Import Group into %1...").arg (groupName (g)));
            auto* picked = menu.exec (b->mapToGlobal (p));
            if (chosen)
            {
                // Mute / Solo : tous les groupes choisis comme celui-ci
                const bool solo = chosen == items[1];
                const bool on = ! (solo ? state.groupSolo[(size_t) g].load() : state.groupMute[(size_t) g].load());
                for (int t : targets)
                    (solo ? state.groupSolo : state.groupMute)[(size_t) t] = on;
                refreshMuteSolo();
            }
            else if (picked == copyGroup || picked == cutGroup)
                copyGroups (targets, picked == cutGroup);
            else if (picked == pasteGroup)
                pasteGroups (g);
            else if (picked == renameAction)
                renameGroup (g);
            else if (picked == exportAction || picked == importAction)
            {
                if (g != state.group)                    // le groupe du clic devient le groupe choisi
                    selectGroup (g);
                picked == exportAction ? exportGroup() : importGroup();
            }
        });
        grid->addWidget (b, i / 4, i % 4);
        groupButtons[(size_t) i] = b;
    }
    col->addLayout (grid);
    // Sous les groupes, au centre : tout reactiver et la 2e banque
    auto* tools = new QHBoxLayout;
    tools->addStretch();
    tools->addWidget (muteSoloRow (groupUnmute, groupUnsolo, true));
    tools->addWidget (bankButton);
    tools->addStretch();
    col->addLayout (tools);
    col->addStretch (1);                             // le bloc reste en haut, la barre colle aux groupes
    return panel;
}

QWidget* MainWindow::buildMixer()
{
    auto* panel = new QFrame;
    panel->setObjectName ("panel");
    auto* col = new QVBoxLayout (panel);
    auto* head = new QHBoxLayout;
    auto* hint = new QLabel (tr ("drag the white line = volume \u00B7 double-click = 100 \u00B7 M = mute \u00B7 S = solo \u00B7 drag the L / R bar = pan (sticks at the center) \u00B7 double-click or Option + click = center"));
    hint->setObjectName ("hint");
    registerHint (hint, "MIXER");
    hint->setSizePolicy (QSizePolicy::Ignored, QSizePolicy::Preferred);   // peut retrecir
    hint->setMinimumWidth (0);
    head->addWidget (muteSoloRow (mixUnmute, mixUnsolo, true));
    mixBankButton = new QPushButton (tr ("A2 - H2"));   // comme le bouton des groupes
    mixBankButton->setCheckable (true);
    mixBankButton->setFocusPolicy (Qt::NoFocus);
    mixBankButton->setToolTip (tr ("Second bank of groups (Cmd+B ; Maschine: Shift + group)"));
    connect (mixBankButton, &QPushButton::clicked, this, [this] { switchBank(); });
    head->addWidget (mixBankButton);
    head->addStretch();
    head->addWidget (hint);
    mixer = new MixerView (state);
    connect (mixer, &MixerView::muteSoloChanged, this, [this] { refreshMuteSolo(); });
    connect (mixer, &MixerView::volumeChanged, this, [this]
    {
        QSettings s;
        for (int g = 0; g < kGroups; ++g)
            s.setValue (QString ("volume/%1").arg (groupName (g)), state.groupVolume[(size_t) g].load());
        s.setValue ("volume/master", state.masterVolume.load());
    });
    col->addLayout (head);
    col->addWidget (mixer, 1);
    return panel;
}

QWidget* MainWindow::buildLeftPanel()
{
    // Panneau Pattern : commandes en haut, grille dessous
    auto* panel = new QFrame;
    panel->setObjectName ("panel");
    auto* col = new QVBoxLayout (panel);
    auto* head = new QHBoxLayout;

    // Commandes du pattern
    auto* patternTools = new QWidget;
    auto* tools = new QHBoxLayout (patternTools);
    tools->setContentsMargins (0, 0, 0, 0);
    auto* lenLabel = new QLabel (tr ("Length"));
    lenLabel->setObjectName ("hint");
    lengthBox = new QSpinBox;
    lengthBox->setRange (1, seq::kMaxRows);
    lengthBox->setFocusPolicy (Qt::ClickFocus);
    lengthBox->setToolTip (tr ("Pattern length in lines (4 lines = 1 beat)"));
    connect (lengthBox, &QSpinBox::editingFinished, lengthBox, [this] { lengthBox->clearFocus(); });
    connect (lengthBox, &QSpinBox::valueChanged, this, [this] (int v)
    {
        remember (state.group);
        sequencer.setPatternLength (state.group, v);
        patternView->update();
    });
    auto* clear = new QPushButton (tr ("Clear"));
    clear->setFocusPolicy (Qt::NoFocus);
    clear->setToolTip (tr ("Erase the whole pattern of this group"));
    connect (clear, &QPushButton::clicked, this, [this]
    {
        remember (state.group);
        sequencer.clear (state.group);
        patternView->clearSelection();
    });
    auto* twice = new QPushButton (QStringLiteral ("\u00D72"));
    twice->setFocusPolicy (Qt::NoFocus);
    twice->setToolTip (tr ("Double: length x 2, notes copied into the new half"));
    connect (twice, &QPushButton::clicked, this, [this] { doublePattern(); });
    auto* hint = new QLabel (tr ("\u25CF + \u25B6 rec  \u00B7  drag = move (%1 copy)  \u00B7  Shift + click / frame = select  \u00B7  Del = erase")
                                 .arg (PatternView::copyKey()));
    hint->setObjectName ("hint");
    registerHint (hint, "PATTERN");
    hint->setSizePolicy (QSizePolicy::Ignored, QSizePolicy::Preferred);   // peut retrecir
    hint->setMinimumWidth (0);
    hint->setAlignment (Qt::AlignRight | Qt::AlignVCenter);
    // Ordre : Edit by "Enter", Length, x2, Generate, Clear, transposition ; aide a droite
    editButton = new QPushButton (tr ("Edit by \"Enter\""));
    editButton->setCheckable (true);
    editButton->setFocusPolicy (Qt::NoFocus);
    editButton->setObjectName ("keys");              // orange quand actif
    editButton->setToolTip (tr ("Edit mode: Enter adds / erases the note of the chosen sound, Del erases"));
    editMode = QSettings().value ("pattern/edit", false).toBool();
    editButton->setChecked (editMode);
    connect (editButton, &QPushButton::toggled, this, [this] (bool on)
    {
        editMode = on;
        QSettings().setValue ("pattern/edit", on);
    });
    auto* gen = new QPushButton (QStringLiteral ("\U0001F3B2 ") + tr ("Generate"));
    gen->setFocusPolicy (Qt::NoFocus);
    gen->setToolTip (tr ("Random notes for the chosen sound (Cmd+G)"));
    connect (gen, &QPushButton::clicked, this, [this] { openGenerate(); });
    tools->addWidget (editButton);
    tools->addSpacing (8);
    tools->addWidget (lenLabel);
    tools->addWidget (lengthBox);
    tools->addWidget (twice);
    tools->addWidget (gen);
    auto* manager = new QPushButton (tr ("List"));    // en premier sur la ligne
    manager->setFocusPolicy (Qt::NoFocus);
    manager->setToolTip (tr ("List of the patterns of this group: play, rename, copy, paste, duplicate, delete, scenes"));
    connect (manager, &QPushButton::clicked, this, [this] { openPatternManager(); });
    tools->insertWidget (0, manager);
    tools->insertSpacing (1, 8);
    tools->addWidget (clear);
    tools->addSpacing (8);
    for (const int step : { -1, 1, -12, 12 })        // transposition : selection, sinon case du curseur
    {
        auto* t = new QPushButton (step == -12 ? tr ("OCT -") : step == -1 ? tr ("SEMI -") : step == 1 ? tr ("SEMI +") : tr ("OCT +"));
        t->setFocusPolicy (Qt::NoFocus);
        t->setToolTip (tr ("Transpose the selected notes (or the note at the cursor)"));
        connect (t, &QPushButton::clicked, this, [this, step] { transposePattern (step); });
        tools->addWidget (t);
    }
    tools->addSpacing (10);
    tools->addWidget (hint, 1);
    head->addWidget (patternTools);
    patternTools->setObjectName ("patternTools");

    patternView = new PatternView (sequencer, state);
    connect (patternView, &PatternView::midiDropped, this, &MainWindow::importMidi);
    connect (patternView, &PatternView::cellClicked, this, [this] (int track)
    {
        if (track + 1 != state.sound)               // la piste = le son : on le choisit
            selectSound (track + 1);
    });
    connect (patternView, &PatternView::cellMenu, this, &MainWindow::patternMenu);
    connect (patternView, &PatternView::cellDoubleClicked, this, &MainWindow::noteOptions);
    connect (patternView, &PatternView::trackClicked, this, [this] (int track, bool solo)
    {
        toggleSound (state.group, track + 1, solo);
    });
    // Glisser une note (Option = copier) ; deposer un pad = sa note, dans sa piste
    connect (patternView, &PatternView::cellsMoved, this, [this] (const QList<QPoint>& cells, int dr, int dt, bool copy)
    {
        // On lit tout, on efface les anciennes places (sauf copie), puis on ecrit les nouvelles
        const seq::Pattern p = sequencer.pattern (state.group);
        remember (state.group);
        if (! copy)
            for (const QPoint& c : cells)
                sequencer.setCell (state.group, c.y(), c.x(), {});
        for (const QPoint& c : cells)
            sequencer.setCell (state.group, c.y() + dr, c.x() + dt, p.rows[(size_t) c.y()][(size_t) c.x()]);
        patternView->update();
    });
    connect (patternView, &PatternView::padDropped, this, [this] (int pad, int row)
    {
        remember (state.group);
        sequencer.setCell (state.group, row, pad - 1, { defaultNote (pad), state.velocity.load(), 1, 0 });
        selectSound (pad);
        patternView->update();
    });
    col->addLayout (head);
    col->addWidget (patternView, 1);
    return panel;
}

void MainWindow::showLeftTab (int index)
{
    // 0 = Mixer, 1 = Pattern : le panneau s'affiche et passe devant
    QDockWidget* dock = index == 0 ? mixerDock : index == 2 ? rollDock : index == 3 ? keysTabDock
                      : index == 4 ? soundDock : index == 5 ? trackerDock : index == 6 ? sceneDock
                      : index == 7 ? songDock : patternDock;
    dock->show();
    dock->raise();
    QTimer::singleShot (0, this, [this] { refreshMaschine(); });
}

bool MainWindow::rollShown() const
{
    return rollDock->isVisible() && ! pianoRoll->visibleRegion().isEmpty();
}

bool MainWindow::trackerShown() const
{
    return trackerDock->isVisible() && ! tracker->visibleRegion().isEmpty();
}

void MainWindow::placeTracker()
{
    // Une disposition enregistree avant le Tracker ne le connait pas : il rejoint les onglets.
    // Ordre des onglets du haut : Pattern, Piano Roll, Tracker, Mixer.
    if (! trackerDock->isVisible() && ! trackerDock->isFloating())
    {
        tabifyDockWidget (rollDock, trackerDock);
        trackerDock->show();
    }
    if (! sceneDock->isVisible() && ! sceneDock->isFloating())   // disposition d'avant l'onglet Scenes
    {
        tabifyDockWidget (mixerDock, sceneDock);
        sceneDock->show();
    }
    if (! songDock->isVisible() && ! songDock->isFloating())     // disposition d'avant l'onglet Song
    {
        tabifyDockWidget (sceneDock, songDock);
        songDock->show();
    }
    for (auto* bar : findChildren<QTabBar*>())
    {
        QStringList titles;
        for (int k = 0; k < bar->count(); ++k)
            titles << bar->tabText (k);
        if (! titles.contains (patternDock->windowTitle()) || ! titles.contains (trackerDock->windowTitle())
            || ! titles.contains (rollDock->windowTitle()) || ! titles.contains (mixerDock->windowTitle()))
            continue;
        const int roll = (int) titles.indexOf (rollDock->windowTitle()), trk = (int) titles.indexOf (trackerDock->windowTitle()),
                  mix = (int) titles.indexOf (mixerDock->windowTitle());
        if (trk == roll + 1 && mix == trk + 1)
            break;                                   // deja dans le bon ordre
        const QString front = bar->tabText (bar->currentIndex());
        removeDockWidget (trackerDock);
        removeDockWidget (mixerDock);
        tabifyDockWidget (rollDock, trackerDock);
        tabifyDockWidget (trackerDock, mixerDock);
        trackerDock->show();
        mixerDock->show();
        for (auto* d : { patternDock, rollDock, trackerDock, mixerDock })
            if (d->windowTitle() == front)
                d->raise();
        break;
    }
}

bool MainWindow::patternShown() const
{
    return patternDock->isVisible() && ! patternView->visibleRegion().isEmpty();
}

QWidget* MainWindow::buildTransportBar()
{
    // Sous les groupes, compact sur 2 lignes : en haut les temps, METRO et son volume ;
    // en bas ⏮ ▶ ■ ● et le tempo
    auto* bar = new QFrame;
    bar->setObjectName ("transport");
    auto* rows = new QVBoxLayout (bar);
    rows->setContentsMargins (8, 4, 8, 4);
    rows->setSpacing (4);
    auto* top = new QHBoxLayout;
    auto* row = new QHBoxLayout;
    top->setSpacing (8);
    row->setSpacing (6);

    auto* restart = new QPushButton (QStringLiteral ("⏮"));                  // retour au debut
    restart->setToolTip (tr ("Restart from the beginning (Maschine: RESTART)"));
    connect (restart, &QPushButton::clicked, this, [this] { transport->restart(); });
    playButton = new QPushButton (QStringLiteral ("▶"));                    // lecture / pause
    playButton->setCheckable (true);
    playButton->setToolTip (tr ("Play / pause (space bar, Maschine: PLAY)"));
    connect (playButton, &QPushButton::clicked, this, [this] (bool on) { on ? transport->play() : transport->pause(); });
    auto* stop = new QPushButton (QStringLiteral ("■"));                     // arret
    stop->setToolTip (tr ("Stop and back to the beginning (Maschine: STOP)"));
    connect (stop, &QPushButton::clicked, this, [this] { transport->stop(); engine->panic(); });
    recButton = new QPushButton (QStringLiteral ("●"));                      // enregistrement
    recButton->setCheckable (true);
    recButton->setObjectName ("rec");
    recButton->setToolTip (tr ("Record (arms recording, used by the sequencer coming next)"));
    connect (recButton, &QPushButton::toggled, this, [this] (bool on)
    {
        recArmed = on;
        for (auto& t : takeSaved)                     // nouvelle prise
            t = false;
        refreshMaschine();
    });
    for (auto* b : { restart, playButton, stop, recButton })
    {
        b->setFocusPolicy (Qt::NoFocus);
        b->setFixedSize (40, 30);                    // compacts : la fenetre Transport reste etroite
    }
    row->addStretch();
    for (auto* b : { restart, playButton, stop, recButton })
        row->addWidget (b);
    row->addSpacing (8);

    bpmBox = new QSpinBox;
    bpmBox->setRange (30, 300);
    bpmBox->setValue ((int) transport->bpm());
    bpmBox->setSuffix (" BPM");
    bpmBox->setToolTip (tr ("Tempo (Maschine: hold TEMPO + turn the 4D wheel)"));
    bpmBox->setFocusPolicy (Qt::ClickFocus);         // le clavier reste au clavier musical
    connect (bpmBox, &QSpinBox::editingFinished, bpmBox, [this] { bpmBox->clearFocus(); });
    connect (bpmBox, &QSpinBox::valueChanged, this, [this] (int v)
    {
        transport->setBpm (v);
        QSettings().setValue ("bpm", v);
        refreshMaschine();
    });
    beatLabel = new QLabel (QStringLiteral ("○ ○ ○ ○"));
    beatLabel->setObjectName ("beats");
    row->addWidget (bpmBox);
    swingBox = new QSpinBox;                         // SWING : une double-croche sur deux plus tard
    swingBox->setRange (0, 100);
    swingBox->setPrefix (tr ("Swing "));
    swingBox->setSuffix (" %");
    swingBox->setValue (state.swing.load());
    swingBox->setMinimumWidth (100);
    swingBox->setToolTip (tr ("Swing for patterns and the song (0 % = straight). Maschine: SWING held + jog wheel"));
    connect (swingBox, &QSpinBox::valueChanged, this, [this] (int v)
    {
        state.swing = v;
        refreshMaschine();
    });
    row->addWidget (swingBox);
    row->addStretch();
    top->addStretch();
    top->addWidget (beatLabel);
    top->addSpacing (8);

    metroButton = new QPushButton (QStringLiteral ("♩  ") + tr ("METRO"));
    metroButton->setCheckable (true);
    metroButton->setChecked (transport->metronome());
    metroButton->setFocusPolicy (Qt::NoFocus);
    metroButton->setToolTip (tr ("Metronome (Maschine: TAP)"));
    metroButton->setFixedSize (104, 30);
    connect (metroButton, &QPushButton::toggled, this, [this] (bool on)
    {
        transport->setMetronome (on);
        QSettings().setValue ("metronome", on);
        refreshMaschine();
    });
    metroVolume = new QSlider (Qt::Horizontal);
    metroVolume->setRange (0, 127);
    metroVolume->setValue (transport->metronomeVolume());
    metroVolume->setFocusPolicy (Qt::NoFocus);
    metroVolume->setFixedWidth (80);
    metroVolume->setToolTip (tr ("Metronome volume (Maschine: hold TAP + turn the 4D wheel)"));
    connect (metroVolume, &QSlider::valueChanged, this, [this] (int v)
    {
        transport->setMetronomeVolume (v);
        QSettings().setValue ("metronomeVolume", v);
        refreshMaschine();
    });
    top->addWidget (metroButton);
    top->addWidget (metroVolume);
    songTransportButton = new QPushButton (tr ("SONG"));   // la lecture suit le morceau (vue Song)
    songTransportButton->setCheckable (true);
    songTransportButton->setObjectName ("keys");
    songTransportButton->setFocusPolicy (Qt::NoFocus);
    songTransportButton->setToolTip (tr ("Song mode: PLAY follows the song (Song view)"));
    songTransportButton->setChecked (sequencer.songMode());
    connect (songTransportButton, &QPushButton::clicked, this, [this] (bool on) { setSongMode (on); });
    top->addWidget (songTransportButton);
    top->addStretch();
    rows->addLayout (top);
    rows->addLayout (row);
    bar->setToolTip (tr ("Space bar: play / pause"));
    bar->setSizePolicy (QSizePolicy::Preferred, QSizePolicy::Maximum);   // juste sa hauteur
    return bar;
}

void MainWindow::openSettings (int page)
{
    SettingsDialog dialog (*engine, this);
    dialog.showPage (page);
    dialog.exec();
}

QWidget* MainWindow::buildKeyboardPanel()
{
    auto* panel = new QFrame;
    panel->setObjectName ("panel");
    auto* col = new QVBoxLayout (panel);
    auto* row = new QHBoxLayout;
    auto* title = new QLabel (tr ("KEYBOARD"));
    title->setObjectName ("section");
    layoutBox = new QComboBox;
    layoutBox->addItems ({ "AZERTY", "QWERTY" });
    layoutBox->setCurrentText (QSettings().value ("layout", "AZERTY").toString());
    connect (layoutBox, &QComboBox::currentTextChanged, this, [this] (const QString& t)
    {
        QSettings().setValue ("layout", t);
        updateKeyLabels();
    });
    keysButton = new QPushButton (QStringLiteral ("\u2328  PC KEYS"));
    keysButton->setCheckable (true);
    keysButton->setChecked (QSettings().value ("pcKeys", true).toBool());
    keysButton->setFocusPolicy (Qt::NoFocus);
    keysButton->setToolTip (tr ("Play with the computer keyboard (on / off)"));
    connect (keysButton, &QPushButton::toggled, this, [this] (bool on)
    {
        QSettings().setValue ("pcKeys", on);
        updateKeyLabels();                           // lettres sur les touches seulement si actif
        if (! on)                                    // couper : relacher les notes tenues
            for (auto it = keyNotes.begin(); it != keyNotes.end(); it = keyNotes.erase (it))
            {
                playKey (it.value(), 0, false);
                piano->setHeld (it.value(), false);
            }
        statusBar()->showMessage (on ? tr ("Computer keys ON") : tr ("Computer keys OFF"), 1500);
    });
    octaveLabel = new QLabel;
    auto* help = new QLabel;
    help->setObjectName ("hint");
    help->setWordWrap (false);                       // une seule ligne, selon AZERTY / QWERTY
    auto helpText = [this, help]
    {
        const QString oct = layoutBox->currentText() == "QWERTY" ? "Z / X" : "W / X";
        help->setText (tr ("Octave %1   Velocity C / V").arg (oct));
    };
    helpText();
    connect (layoutBox, &QComboBox::currentTextChanged, help, helpText);
    row->addWidget (title);
    row->addWidget (keysButton);
    row->addWidget (layoutBox);
    row->addWidget (octaveLabel);
    row->addWidget (help, 1);
    piano = new PianoKeys;
    col->addLayout (row);
    col->addWidget (piano);
    return panel;
}

void MainWindow::applyTheme()
{
    const QString accent = css (kAccent);
    qApp->setStyleSheet (QString (R"(
        QWidget { background: #101014; color: #e6e6e6; font-size: 13px; }
        QToolTip { color: #f0f0f5; background: #202028; border: 1px solid #555; padding: 3px; }
        #topBar, #panel { background: #17171c; border: 1px solid #26262e; border-radius: 8px; }
        QDockWidget { color: #8a8a92; font-size: 11px; }
        QDockWidget::title { background: #121216; padding: 3px 8px; text-align: left; }
        QToolBar { background: #0e0e11; border: none; spacing: 0; padding: 2px 6px; }
        QMainWindow::separator { background: #1c1c22; width: 5px; height: 5px; }
        QMainWindow::separator:hover { background: %1; }
        #screen { background: #0b0b0e; border: 1px solid #2a2a33; border-top: 3px solid %1; border-radius: 6px; }
        #stats { color: #9a9aa4; font-family: monospace; background: transparent; }
        #transport { background: #17171c; border: 1px solid #26262e; border-radius: 8px; }
        #transport QPushButton { font-size: 15px; padding: 0; }
        QPushButton#tab { background: transparent; border: none; color: #8a8a92; font-weight: bold; padding: 2px 8px; }
        QPushButton#tab:checked { color: %1; border-bottom: 2px solid %1; border-radius: 0; }
        QPushButton#rec { color: #e04040; }
        QPushButton#rec:checked { background: #e04040; color: #111; }
        #title { font-size: 18px; color: %1; background: transparent; }
        #projectName { font-size: 15px; font-weight: bold; color: #e8e8ec; background: transparent; }
        #section { color: %1; font-weight: bold; letter-spacing: 1px; background: transparent; }
        #hint { color: #8a8a92; background: transparent; }
        #beats { font-size: 18px; color: %1; background: transparent; }
        QLabel { background: transparent; }
        QPushButton { background: #22222a; border: 1px solid #34343e; border-radius: 6px; padding: 6px 12px; }
        QPushButton:hover { border-color: %1; }
        QPushButton:checked { background: %1; color: #111; }
        QComboBox, QSpinBox { background: #22222a; border: 1px solid #34343e; border-radius: 6px; padding: 4px 8px; }
        QScrollBar:horizontal { background: #15151a; height: 12px; margin: 0; border: none; }
        QScrollBar:vertical { background: #15151a; width: 12px; margin: 0; border: none; }
        QScrollBar::handle { background: #3a3a46; border-radius: 5px; min-width: 24px; min-height: 24px; }
        QScrollBar::handle:hover { background: #50505e; }
        QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; border: none; background: none; }
        QScrollBar::add-page, QScrollBar::sub-page { background: none; }
        QSpinBox { padding-right: 22px; }
        QSpinBox::up-button, QSpinBox::down-button { width: 20px; border: none; background: #30303a; }
        QSpinBox::up-button { subcontrol-origin: border; subcontrol-position: top right; border-top-right-radius: 6px; border-bottom: 1px solid #22222a; }
        QSpinBox::down-button { subcontrol-origin: border; subcontrol-position: bottom right; border-bottom-right-radius: 6px; }
        QSpinBox::up-button:hover, QSpinBox::down-button:hover { background: #44444f; }
        QSpinBox::up-arrow { image: url(:/arrow_up.png); width: 10px; height: 6px; }
        QSpinBox::down-arrow { image: url(:/arrow_down.png); width: 10px; height: 6px; }
        QPushButton#unmute, QPushButton#unsolo { font-size: 11px; font-weight: bold; padding: 2px 8px; min-width: 30px; }
        QPushButton#unmute:enabled { background: #e05050; color: #111; border-color: #e05050; }
        QPushButton#unsolo:enabled { background: #e8c040; color: #111; border-color: #e8c040; }
        QPushButton#unmute:disabled, QPushButton#unsolo:disabled { background: #17171c; color: #55555e; }
        QMenu { background: #1c1c22; border: 1px solid #34343e; padding: 4px; }
        QMenu::item { padding: 5px 24px 5px 24px; border-radius: 4px; }
        QMenu::item:selected { background: #f5c9a0; color: #111; }
        QMenu::item:checked { color: %1; font-weight: bold; }
        QMenu::item:checked:selected { background: #f5c9a0; color: #111; }
        QMenu::item:disabled { color: #66666e; }
        QMenu::separator { height: 1px; background: #34343e; margin: 4px 8px; }
        QComboBox QAbstractItemView { background: #1c1c22; border: 1px solid #34343e; outline: 0;
                                      selection-background-color: #f5c9a0; selection-color: #111; }
        QComboBox QAbstractItemView::item { padding: 4px 8px; }
        QComboBox QAbstractItemView::item:selected { background: #f5c9a0; color: #111; }
        QTableWidget, QListWidget { background: #15151a; alternate-background-color: #1a1a20;
                                    selection-background-color: #f5c9a0; selection-color: #111; gridline-color: #26262e; }
        QSlider::groove:horizontal { height: 4px; background: #34343e; border-radius: 2px; }
        QSlider::handle:horizontal { width: 12px; margin: -5px 0; background: %1; border-radius: 6px; }
        QStatusBar { color: #8a8a92; }
    )").arg (accent));
}

// ------------------------------------------------------------------------------------ jouer

void MainWindow::playPad (int pad, int velocity, bool down)
{
    if (pad < 1 || pad > 16)
        return;
    padHeld[(size_t) pad] = down;
    if (down)
    {
        const int g = state.group;
        padGroup[(size_t) pad] = g;                  // la fin de note ira a ce groupe
        if (maschineShift)                            // Shift + pad : fonctions de la serigraphie
        {
            QMetaObject::invokeMethod (this, [this, pad] { shiftPad (pad); }, Qt::QueuedConnection);
            return;
        }
        if (patternHeld || sceneHeld)                // PATTERN / SCENE + pad : pattern ou scene de la banque
        {
            holdUsed = true;
            const bool scene = sceneHeld;
            QMetaObject::invokeMethod (this, [this, pad, scene]
            {
                scene ? patternBar->chooseScene (patternBar->sceneBank * seq::kSlotsPerBank + pad - 1)
                      : patternBar->choosePattern (patternBar->patternBank * seq::kSlotsPerBank + pad - 1);
            }, Qt::QueuedConnection);
            return;
        }
        if (duplicateHeld)                           // DUPLICATE + pad + pad : copie du son
        {
            QMetaObject::invokeMethod (this, [this, g, pad]
            {
                if (dupPad == 0)
                {
                    dupGroup = g;
                    dupPad = pad;
                    toast (QString ("COPY %1 %2 TO ?").arg (groupName (g)).arg (pad, 2, 10, QChar ('0')));
                }
                else if (! (dupGroup == g && dupPad == pad))
                {
                    transferSound (dupGroup, dupPad, g, pad, Transfer::Copy);
                    dupPad = 0;
                }
            }, Qt::QueuedConnection);
            return;
        }
        if (arrangerMode && selectHeld)              // vue Song (ARRANGER) : SELECT + pad = ajouter la scene
        {
            QMetaObject::invokeMethod (this, [this, pad]
            {
                addSongScene (patternBar->sceneBank * seq::kSlotsPerBank + pad - 1, -1);
            }, Qt::QueuedConnection);
            return;
        }
        if (padKeyboard && ! muteHeld && ! soloHeld)  // KEYBOARD : une note ; CHORDS : un accord
        {
            const bool chords = padChords;
            const int note = chords ? padChordKey (pad) : padNote (pad);
            if (note < 1)                            // pad sans accord (Chord Set : pads 13-16)
                return;
            padKey[(size_t) pad] = note;
            QMetaObject::invokeMethod (this, [this, note, velocity, chords] { playKey (note, velocity, true, chords); }, Qt::QueuedConnection);
            return;
        }
        if (muteHeld || soloHeld)                   // MUTE / SOLO + pad : couper / solo ce son
        {
            toggleSound (g, pad, soloHeld);
            return;
        }
        const int v = state.outVelocity (g, pad, velocity);   // volume du son, du groupe et master
        padSent[(size_t) pad] = state.pitched (g, pad, defaultNote (pad));   // la fin de note coupera celle-ci
        if (v > 0 && state.audible (g, pad) && state.hasInstrument[(size_t) state.index (g, pad)])
            engine->hit (pad, v, padSent[(size_t) pad], g, state.held (g, pad), state.releaseMs[(size_t) state.index (g, pad)],
                         0, state.effSynth (g, pad), state.pan (g, pad), state.poly (g, pad));
        std::lock_guard<std::mutex> lk (recLock);
        recordStart (padRec[(size_t) pad], pad - 1, defaultNote (pad), velocity);
    }
    else
    {
        if (padKey[(size_t) pad] > 0)                 // mode KEYBOARD : fin de la note de ce pad
        {
            const int note = padKey[(size_t) pad].exchange (0);
            QMetaObject::invokeMethod (this, [this, note] { playKey (note, 0, false); }, Qt::QueuedConnection);   // puis liveCheck
            return;
        }
        if (state.held (padGroup[(size_t) pad], pad))
            engine->noteOff (pad, padSent[(size_t) pad], padGroup[(size_t) pad]);
        {
            std::lock_guard<std::mutex> lk (recLock);
            recordEnd (padRec[(size_t) pad]);
        }
        QMetaObject::invokeMethod (this, [this] { liveCheck(); }, Qt::QueuedConnection);
    }
}

void MainWindow::playNote (int note, int velocity, bool down)
{
    const int snd = state.sound;
    const int v = state.outVelocity (state.group, snd, velocity);
    if (down)
    {
        const LiveNote live { snd, state.pitched (state.group, snd, note), state.group };
        liveNotes.insert (note, live);                // le relachement coupera exactement cette note
        if (v > 0 && state.audible (state.group, snd) && state.hasInstrument[(size_t) state.index (state.group, snd)])
            engine->hit (snd, v, live.sent, state.group, state.held (state.group, snd), state.releaseMs[(size_t) state.index (state.group, snd)],
                         0, state.effSynth (state.group, snd), state.pan (state.group, snd), state.poly (state.group, snd));
        if (keyboardView)
            keyboardView->setActive (note, true);
        std::lock_guard<std::mutex> lk (recLock);
        recordStart (noteRec[note], snd - 1, note, velocity);
        return;
    }
    if (keyboardView)
        keyboardView->setActive (note, false);
    {
        std::lock_guard<std::mutex> lk (recLock);
        if (noteRec.contains (note))
            recordEnd (noteRec[note]);
    }
    const LiveNote live = liveNotes.contains (note) ? liveNotes.take (note) : LiveNote { snd, note, state.group };
    engine->noteOff (live.sound, live.sent, live.group);
}

void MainWindow::recordStart (RecNote& slot, int track, int note, int velocity)
{
    // REC arme et lecture en cours : la note va dans le pattern du groupe, a sa place exacte
    slot.active = false;
    if (! recArmed || ! transport->playing())
        return;
    if (! takeSaved[(size_t) state.group].exchange (true))
        remember (state.group);                       // avant la 1re note de la prise
    slot.where = sequencer.record (state.group, track, note, velocity, transport->position(), chordMerge);
    slot.start = std::chrono::steady_clock::now();
    slot.active = true;
    QMetaObject::invokeMethod (this, [this] { patternView->update(); }, Qt::QueuedConnection);
}

void MainWindow::recordEnd (RecNote& slot)
{
    if (! slot.active)
        return;
    slot.active = false;
    const double held = std::chrono::duration<double> (std::chrono::steady_clock::now() - slot.start).count();
    sequencer.setLength (slot.where, held / transport->rowSeconds());   // duree = temps d'appui
    QMetaObject::invokeMethod (this, [this] { patternView->update(); }, Qt::QueuedConnection);
}

// ------------------------------------------------------------------------------- selections

void MainWindow::selectGroup (int group)
{
    QTimer::singleShot (0, this, [this] { if (patternManagerRefresh) patternManagerRefresh(); if (palette) palette->update(); });   // la liste suit le groupe
    state.group = qBound (0, group, kGroups - 1);
    if (patternBar)
        patternBar->refresh();
    QSettings().setValue ("group", state.group.load());
    refreshGroups();
    refreshAll();
}

void MainWindow::selectSound (int sound)
{
    state.sound = qBound (1, sound, 16);
    refreshAll();
}

void MainWindow::setOctave (int octave)
{
    // Les notes tenues gardent leur hauteur : leur fin de note est deja memorisee.
    state.octave = qBound (0, octave, 7);
    QSettings().setValue ("octave", state.octave.load());
    refreshAll();
    statusBar()->showMessage (tr ("Octave %1").arg (state.octave.load()), 800);
}

void MainWindow::refreshAll()
{
    if (patternBar)
        patternBar->refresh();                       // patterns / scenes du groupe
    refreshSoundPanel();
    if (rollTitle)                                   // Piano Roll : le son choisi
        {
            const QString full = tr ("SOUND %1 — %2").arg (state.sound.load(), 2, 10, QChar ('0')).arg (padName (state.group, state.sound));
            rollTitle->setText (rollTitle->fontMetrics().elidedText (full, Qt::ElideRight, rollTitle->width() - 4));
            rollTitle->setToolTip (full);
        }
    const QColor c = groupColor (state.group);
    pads->setColor (c);
    pads->setGroup (state.group);
    if (tracker)                                     // en-tete du Tracker : noms des sons
    {
        QStringList names;
        for (int p = 1; p <= 16; ++p)
            names << padName (state.group, p);
        tracker->setTrackNames (names);
        if (rollTracker)
            rollTracker->setTrackNames (names);
    }
    pads->setSelected (state.sound);
    for (int p = 1; p <= 16; ++p)
        pads->setLabel (p, padLabel (p));
    {
        std::array<bool, 16> empty {};
        for (int p = 1; p <= 16; ++p)
            empty[(size_t) p - 1] = padChords ? padChordKey (p) < 1
                                              : ! state.hasInstrument[(size_t) state.index (state.group, p)];
        pads->setEmpty (empty);
    }
    piano->setColor (c);
    piano->setBaseNote (12 * (state.octave + 1));
    octaveLabel->setText (tr ("Octave %1   Velocity %2").arg (state.octave.load()).arg (state.velocity.load()));
    if (octaveSpin)
    {
        const QSignalBlocker b1 (octaveSpin), b2 (velocitySpin);
        octaveSpin->setValue (state.octave);
        velocitySpin->setValue (state.velocity);
    }
    updateKeyLabels();                               // les lettres suivent l'octave
    soundLabel->setText (tr ("Group <b>%1</b> — sound <b>%2 %3</b> (%4)<br>The keyboard plays this sound; click a pad to change it.")
                             .arg (groupName (state.group))
                             .arg (state.sound.load(), 2, 10, QChar ('0'))
                             .arg (soundName (state.sound))
                             .arg (QStringList { "ONE-SHOT", "GATE", "LOOP" }[state.mode (state.group, state.sound)]));
    refreshMaschine();
}

void MainWindow::refreshGroups()
{
    // Les 8 boutons montrent la banque du groupe choisi (A-H ou A2-H2), dans leurs couleurs
    const int bank = state.group >= 8 ? 8 : 0;
    bankButton->setChecked (bank == 8);
    if (mixBankButton)
        mixBankButton->setChecked (bank == 8);
    for (int i = 0; i < 8; ++i)
    {
        auto* b = groupButtons[(size_t) i];
        const int g = bank + i;
        const QString tag = state.groupMute[(size_t) g] ? " M" : state.groupSolo[(size_t) g] ? " S" : "";
        b->setText (state.groupLabel[(size_t) g].isEmpty() ? groupName (g) + tag : groupName (g) + tag + "\n" + state.groupLabel[(size_t) g]);
        b->setChecked (g == state.group);
        const QColor c = state.groupAudible (g) ? groupColor (g) : QColor (90, 90, 98);   // muet : gris
        b->setStyleSheet (QString ("QPushButton { color: %1; border: 2px solid %1; background: #17171c; font-size: 16px; font-weight: bold; }"
                                   "QPushButton:checked { color: #111; background: %1; }").arg (css (c)));
    }
    if (mixer)
        mixer->update();
    if (patternView)
    {
        patternView->clearSelection();               // autre groupe : autre pattern
        const QSignalBlocker block (lengthBox);
        lengthBox->setValue (sequencer.patternLength (state.group));
        patternView->update();
    }
    if (pads && patternView && mixer)
        refreshMuteSolo();                           // M / S du nouveau groupe
    // L'ecran de gauche reprend la couleur du groupe, comme sur la Maschine
    leftScreen->setStyleSheet (QString ("#screen { border-top: 3px solid %1; }").arg (css (groupColor (state.group))));
}

void MainWindow::refreshStats()
{
    stats.update();
    // Code couleur : vert < 80 %, orange de 80 a 95 %, rouge au-dela
    const auto colored = [] (const QString& text, double percent)
    {
        const char* c = percent > 95 ? "#ff4040" : percent >= 80 ? "#ffa020" : "#3ccf6e";
        return QString ("<span style='color:%1'>%2</span>").arg (c, text);
    };
    QStringList parts;
    if (stats.valid())
    {
        parts << colored (tr ("CPU %1 %").arg (stats.cpuPercent(), 0, 'f', 0), stats.cpuPercent())
              << colored (tr ("RAM %1 / %2 GB").arg (stats.memUsedGB(), 0, 'f', 1).arg (stats.memTotalGB(), 0, 'f', 0),
                          100.0 * stats.memUsedGB() / stats.memTotalGB());
    }
    if (engine->state().cpu >= 0)
        parts << colored (tr ("Audio %1 %").arg (engine->state().cpu, 0, 'f', 0), engine->state().cpu);
    parts << tr ("App %1 MB").arg (stats.appMB(), 0, 'f', 0);
    statsLabel->setText (parts.join ("<span style='color:#55555e'> &nbsp;|&nbsp; </span>"));
}

void MainWindow::refreshMaschine()
{
    arrangerMode = songView && ! songView->visibleRegion().isEmpty();   // la vue Song est devant
    if (! maschine)
        return;
    maschine->updateLights();
    maschine->updateScreens();
}

void MainWindow::refreshEngine()
{
    const AudioState& a = engine->state();
    if (! a.valid())
    {
        if (engine->running())
            engineLabel->setText (tr ("Audio engine: starting..."));
        else if (! opts.engine)
            engineLabel->setText (tr ("Audio engine: not started (--no-engine)"));
        return;
    }
    engineLabel->setText (tr ("%1<br>%2 Hz, buffer %3, latency %4 ms%5")
                              .arg (a.device).arg (a.rate, 0, 'f', 0).arg (a.buffer)
                              .arg (a.latencyMs, 0, 'f', 1)
                              .arg (a.xruns > 0 ? tr (", %1 dropouts").arg (a.xruns) : QString()));

}

// --------------------------------------------------------------------- Maschine : boutons

void MainWindow::onMaschineButton (const QString& name, bool down)
{
    if (name == "tempo")
        maschine->setProperty ("tempoHeld", down);
    if (name == "swing")                             // tenu + molette : swing
    {
        maschine->setProperty ("swingHeld", down);
        if (down)
            toast (QString ("SWING %1 % (turn the jog wheel)").arg (state.swing.load()));
        return;
    }
    if (name == "mute" && down && maschine->property ("shiftHeld").toBool())   // Shift + MUTE : panique
    {
        releaseAllKeys();
        engine->panic();                             // tous les sons se coupent
        toast ("ALL SOUNDS OFF");
        return;
    }
    if (name == "arranger")                          // ARRANGER : vue Song (2e appui : retour)
    {
        if (down)
            toggleView (7, "SONG");
        return;
    }
    if (down && arrangerMode && arrangerButton (name))   // vue Song : ERASE, DUPLICATE, ecrans 3/5/6/7...
        return;
    if (name == "mute" || name == "solo")           // tenus : + groupe ou + pad
    {
        (name == "mute" ? muteHeld : soloHeld) = down;
        refreshMaschine();
        return;
    }
    if (name == "pattern" || name == "scene")        // tenus + pad : choisir un pattern / une scene
    {
        std::atomic<bool>& held = name == "pattern" ? patternHeld : sceneHeld;
        held = down;
        if (down)
        {
            holdUsed = false;
            toast (name == "pattern" ? QString ("PATTERN + pad (bank %1)").arg (patternBar->patternBank + 1)
                                     : QString ("SCENE + pad (bank %1)").arg (patternBar->sceneBank + 1));
        }
        else if (! holdUsed && name == "pattern")    // PATTERN seul : vue Pattern (comme avant)
            toggleView (1, "PATTERN");
        else if (! holdUsed)                         // SCENE seul : vue Scenes (2e appui : retour)
            toggleView (6, "SCENES");
        refreshMaschine();                           // lumieres PATTERN / SCENE
        return;
    }
    if (name == "duplicate")                         // DUPLICATE tenu + source + cible (pad ou groupe)
    {
        duplicateHeld = down;
        dupGroup = -1;
        dupPad = 0;
        if (down)
            toast ("DUPLICATE: source, then target");
        return;
    }
    if (name == "tap")
    {
        maschine->setProperty ("tapHeld", down);
        if (down)
            tapTurned = false;
        else if (! tapTurned)                         // TAP seul : metronome marche / arret
            metroButton->toggle();
        return;
    }
    if (name == "select")                            // tenu : SELECT + pad (vue Song)
    {
        selectHeld = down;
        return;
    }
    if (name == "shift")                             // tenu / relache (avant : restait « tenu »)
    {
        maschine->setProperty ("shiftHeld", down);
        maschineShift = down;
        return;
    }
    if (! down)
        return;
    const bool shift = maschine->property ("shiftHeld").toBool();
    if (false)
        ;
    else if (name == "play")
        transport->playing() ? transport->pause() : transport->play();
    else if (name == "restart")
        transport->restart();
    else if (name == "rec")
        recButton->toggle();
    else if (name == "stop")
    {
        transport->stop();
        engine->panic();
    }
    else if (name.startsWith ("group_") && name.size() == 7)
    {
        const int i = name.at (6).toLatin1() - 'a';                // A-H ; Shift = A2-H2
        const bool bank2 = shift ? state.group < 8 : state.group >= 8;
        if (duplicateHeld)                           // DUPLICATE + groupe + groupe : copie du groupe
        {
            const int g = i + (bank2 ? 8 : 0);
            if (dupGroup < 0 || dupPad != 0)
            {
                dupGroup = g;
                dupPad = 0;
                toast (QString ("COPY GROUP %1 TO ?").arg (groupName (g)));
            }
            else if (dupGroup != g)
            {
                transferGroup (dupGroup, g, Transfer::Copy);
                dupGroup = -1;
            }
        }
        else if (muteHeld || soloHeld)                    // MUTE / SOLO + groupe
            toggleGroup (i + (state.group >= 8 ? 8 : 0), soloHeld);
        else
            selectGroup (i + (bank2 ? 8 : 0));
    }
    else if (name == "nav_up" || name == "nav_down")
        setOctave (state.octave + (name == "nav_up" ? 1 : -1));
    // Vues de l'app (comme PATTERN / EVENTS / MIXER du prototype)
    else if (name == "events")
        toggleView (2, "PIANO ROLL");
    else if (name == "keyboard")                     // pads : le son choisi sur 16 notes (bascule)
        setPadKeyboard (! (padKeyboard && ! padChords));
    else if (name == "chords")                       // pads : les accords du menu Chords (bascule) ; Shift = le menu
    {
        if (shift)
        {
            showLeftTab (3);
            if (chordMenuButton)
                chordMenuButton->showMenu();
        }
        else
        {
            setPadChords (! padChords);
            if (padChords)
                showLeftTab (3);                     // onglet Keyboard : le menu Chords est la
        }
    }
    else if (name == "pad_mode")                     // pads : les 16 sons
        setPadKeyboard (false);
    else if (name == "plug_in")                      // PLUG-IN : reglages du son choisi
        toggleView (4, "SOUND");
    else if (name == "mixer")
        toggleView (0, "MIXER");
    else if (name == "variation")                    // nouveau tirage ; Shift = la fenetre Generate
        shift ? openGenerate() : quickVariation();
    else if (name == "sampling")                     // mode du son ; Shift = Edit Loop
    {
        if (openLoopDialog)                          // fenetre Edit Loop ouverte : SAMPLING la ferme (reglages gardes)
        {
            openLoopDialog->accept();
            toast ("EDIT LOOP closed");
        }
        else
            shift ? editLoop (state.sound) : cyclePlayMode();
    }
    else if (name == "nav_left" || name == "nav_right")   // case du pattern : ligne (Shift = piste)
    {
        const int step = name == "nav_right" ? 1 : -1;
        showLeftTab (1);
        shift ? patternView->moveCursor (0, step) : patternView->moveCursor (step, 0);
    }
    else if (name == "nav_click")                    // appui molette : la note du son choisi (mode EDIT)
    {
        showLeftTab (1);
        if (editMode)
            addNoteAtCursor();
        else
            toast ("Edit by Enter is off");
    }
}

// ------------------------------------------------------------ clavier de l'ordinateur

bool MainWindow::eventFilter (QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Enter && helpLine && watched->isWidgetType())   // aide de la vue survolee
        showHelpFor (static_cast<QWidget*> (watched));
    if (event->type() == QEvent::MouseButtonPress)    // zone cliquee : la cible de Cmd+C / X / V
    {
        for (auto* w = qobject_cast<QWidget*> (watched); w; w = w->parentWidget())
        {
            if (w == pads)
            {
                lastArea = Area::Pads;
                break;
            }
            if (w == patternView || w == pianoRoll)
            {
                lastArea = Area::Pattern;
                break;
            }
            if (w == songView || w == songSceneList)
            {
                lastArea = Area::Song;
                break;
            }
        }
    }
    if (auto* b = qobject_cast<QPushButton*> (watched); b && b->property ("groupIndex").isValid())
    {
        // Bouton de groupe : le glisser (copier / deplacer le groupe) ; y lacher un pad ou un groupe
        const int i = b->property ("groupIndex").toInt();
        switch (event->type())
        {
            case QEvent::MouseButtonPress:
            {
                auto* m = static_cast<QMouseEvent*> (event);
                lastArea = Area::Groups;
                const int g = i + (state.group >= 8 ? 8 : 0);
                if (m->button() == Qt::LeftButton && (m->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier)))
                {
                    // Choisir des groupes sans changer de groupe : Cmd + clic = celui-ci, Shift + clic = de .. a
                    if (groupChosen.isEmpty())
                        groupChosen.insert (state.group);
                    if (m->modifiers() & Qt::ControlModifier)
                        groupChosen.contains (g) ? (void) groupChosen.remove (g) : (void) groupChosen.insert (g);
                    else
                    {
                        const int from = groupAnchor >= 0 ? groupAnchor : state.group.load();
                        for (int k = std::min (from, g); k <= std::max (from, g); ++k)
                            groupChosen.insert (k);
                    }
                    groupAnchor = g;
                    refreshMuteSolo();
                    return true;                     // pas de changement de groupe
                }
                if (m->button() == Qt::LeftButton)
                {
                    if (! groupChosen.isEmpty())     // clic simple : fin de la selection
                    {
                        groupChosen.clear();
                        refreshMuteSolo();
                    }
                    groupAnchor = g;
                    groupPress = m->position().toPoint();
                    groupPressIndex = i;
                }
                break;
            }
            case QEvent::MouseMove:
            {
                auto* m = static_cast<QMouseEvent*> (event);
                if (groupPressIndex == i && (m->buttons() & Qt::LeftButton)
                    && (m->position().toPoint() - groupPress).manhattanLength() >= QApplication::startDragDistance() * 2)
                {
                    groupPressIndex = -1;
                    const int g = i + (state.group >= 8 ? 8 : 0);
                    auto* mime = new QMimeData;
                    mime->setData (kGroupMime, QByteArray::number (g));
                    auto* drag = new QDrag (b);
                    drag->setMimeData (mime);
                    drag->setPixmap (b->grab().scaled (b->size() * 0.7, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    drag->exec (Qt::CopyAction | Qt::MoveAction);
                    b->setDown (false);
                    return true;
                }
                break;
            }
            case QEvent::MouseButtonRelease:
                groupPressIndex = -1;
                break;
            case QEvent::DragEnter:
            case QEvent::DragMove:
            {
                auto* d = static_cast<QDropEvent*> (event);
                const bool ok = d->mimeData()->hasFormat (kSoundMime) || d->mimeData()->hasFormat (kGroupMime);
                ok ? d->acceptProposedAction() : d->ignore();
                return true;
            }
            case QEvent::Drop:
            {
                auto* d = static_cast<QDropEvent*> (event);
                d->acceptProposedAction();
                // copie des donnees : la fenetre s'ouvre apres la fin du glisser
                auto* copy = new QMimeData;
                for (const char* f : { kSoundMime, kGroupMime })
                    if (d->mimeData()->hasFormat (f))
                        copy->setData (f, d->mimeData()->data (f));
                QMetaObject::invokeMethod (this, [this, i, copy] { groupDropped (i, copy); delete copy; }, Qt::QueuedConnection);
                return true;
            }
            default:
                break;
        }
    }
    if (event->type() == QEvent::KeyRelease && maschine && static_cast<QKeyEvent*> (event)->key() == Qt::Key_Shift)
        maschine->setProperty ("shiftHeld", false);
    if (event->type() == QEvent::KeyRelease)
    {
        // Une touche qui joue une note : son relachement passe toujours (Cmd, Alt, menu ouvert,
        // champ de saisie...), sinon la note resterait tenue jusqu'a la securite du moteur (30 s)
        auto* k = static_cast<QKeyEvent*> (event);
        if (! k->isAutoRepeat() && keyNotes.contains (k->key()))
        {
            const int note = keyNotes.take (k->key());
            playKey (note, 0, false);
            piano->setHeld (note, false);
            return true;
        }
    }
    if (event->type() == QEvent::ApplicationDeactivate)
        releaseAllKeys();                            // on change d'app en tenant une touche
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
    {
        // Pas de notes quand on tape dans un champ (tempo...)
        QWidget* focus = QApplication::focusWidget();
        const bool typing = qobject_cast<QLineEdit*> (focus) || qobject_cast<QAbstractSpinBox*> (focus)
                         || QApplication::activePopupWidget() || QApplication::activeModalWidget();   // menu, dialogue
        if (! typing && handleKey (static_cast<QKeyEvent*> (event), event->type() == QEvent::KeyPress))
            return true;
    }
    return QMainWindow::eventFilter (watched, event);
}

bool MainWindow::handleKey (QKeyEvent* e, bool down)
{
    if (e->isAutoRepeat())
        return keyNotes.contains (e->key()) || e->key() == Qt::Key_Space;   // touche tenue : on ignore
    if (e->modifiers() & (Qt::ControlModifier | Qt::MetaModifier | Qt::AltModifier))
        return false;                                // raccourcis du systeme
    if (! down)
    {
        if (! keyNotes.contains (e->key()))
            return false;
        const int note = keyNotes.take (e->key());   // la note jouee a l'appui, meme si l'octave a change
        playKey (note, 0, false);
        piano->setHeld (note, false);
        return true;
    }
    if (e->key() == Qt::Key_Space)
    {
        transport->playing() ? transport->pause() : transport->play();
        return true;
    }
    if (activeTracker() && trackerKey (e))
        return true;
    if (lastArea == Area::Song && songView && ! songView->visibleRegion().isEmpty() && songKey (e))
        return true;
    if (stepView && ! stepView->visibleRegion().isEmpty() && stepsKey (e))
        return true;
    if ((e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) && rollShown())
    {
        if (const int n = pianoRoll->deleteSelection())
            statusBar()->showMessage (tr ("%1 note(s) erased").arg (n), 1200);
        return true;
    }
    if ((e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) && patternShown() && ! editMode)
    {
        statusBar()->showMessage (tr ("Turn \"Edit by Enter\" on to erase notes with Del"), 1500);
        return true;
    }
    if ((e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) && patternShown())
    {
        const QList<QPoint> sel = patternView->selection();     // les notes choisies, sinon la case
        remember (state.group);
        if (sel.isEmpty())
            sequencer.setCell (state.group, patternView->selectedRow(), patternView->selectedTrack(), {});
        for (const QPoint& c : sel)
            sequencer.setCell (state.group, c.y(), c.x(), {});
        patternView->clearSelection();
        return true;
    }
    if (patternShown())
    {
        // Fleches : deplacer la case choisie ; Entree : la note du son choisi (vue Pads)
        const int dr = e->key() == Qt::Key_Up ? -1 : e->key() == Qt::Key_Down ? 1 : 0;
        const int dt = e->key() == Qt::Key_Left ? -1 : e->key() == Qt::Key_Right ? 1 : 0;
        if (dr || dt)
        {
            patternView->moveCursor (dr, dt);
            return true;
        }
        if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
        {
            if (editMode)
                addNoteAtCursor();
            else
                statusBar()->showMessage (tr ("Turn \"Edit by Enter\" on to add / erase notes with Enter"), 1500);
            return true;
        }
    }
    if (! keysButton->isChecked())                   // jeu au clavier coupe (bouton PC KEYS)
        return false;
    char c = 0;
    if (e->key() >= Qt::Key_A && e->key() <= Qt::Key_Z)
        c = char ('a' + (e->key() - Qt::Key_A));
    else if (e->key() == Qt::Key_Semicolon)
        c = ';';
    else if (e->text().size() == 1)
        c = e->text().at (0).toLower().toLatin1();
    if (! c)
        return false;
    const KeyLayout& lay = layoutBox->currentText() == "QWERTY" ? kQwerty : kAzerty;
    if (c == lay.octDown || c == lay.octUp)
    {
        setOctave (state.octave + (c == lay.octUp ? 1 : -1));
        return true;
    }
    if (c == lay.velDown || c == lay.velUp)
    {
        state.velocity = qBound (1, state.velocity + (c == lay.velUp ? 16 : -16), 127);
        refreshAll();
        return true;
    }
    const char* pos = c ? std::strchr (lay.notes, c) : nullptr;
    if (! pos)
        return false;
    const int note = qBound (0, 12 * (state.octave + 1) + int (pos - lay.notes), 127);
    keyNotes.insert (e->key(), note);
    playKey (note, state.velocity, true);
    piano->setHeld (note, true);
    return true;
}

void MainWindow::updateKeyLabels()
{
    const KeyLayout& lay = layoutBox->currentText() == "QWERTY" ? kQwerty : kAzerty;
    QMap<int, QString> labels;
    for (int i = 0; keysButton->isChecked() && lay.notes[i]; ++i)
        labels.insert (i, QString (QChar (lay.notes[i])).toUpper());
    piano->setKeyLabels (labels);
    if (keyboardView)                                // les lettres sur le grand clavier aussi
    {
        QMap<int, QString> byNote;
        for (auto it = labels.begin(); it != labels.end(); ++it)
            byNote.insert (12 * (state.octave + 1) + it.key(), it.value());
        keyboardView->setKeyLabels (byNote);
    }
}

void MainWindow::closeEvent (QCloseEvent* event)
{
    if (! maybeSave())
    {
        event->ignore();                              // Cancel : on reste
        return;
    }
    {
        QSettings s;                                  // disposition des panneaux, pour la prochaine fois
        s.setValue ("windowState", saveState (kLayoutVersion));
        s.setValue ("windowGeometry", saveGeometry());
    }
    transport->stop();
    maschine->close();                               // la Maschine revient au logiciel NI
    engine->stop();
    QMainWindow::closeEvent (event);
}

// ------------------------------------------------------------------------------- projets .fuzzy

QString MainWindow::projectsDir() const
{
    return QSettings().value ("projectsDir", Project::defaultDir()).toString();
}

void MainWindow::updateTitle()
{
    setWindowTitle (QString ("%1%2 — Fuzzy John DAW Pulses").arg (project.name, projectModified ? " *" : ""));
    if (projectLabel)
        projectLabel->setText (QString ("%1%2").arg (project.name, projectModified ? " *" : ""));
}

void MainWindow::projectChanged()
{
    // Tempo, volumes et patterns viennent d'etre remplaces : l'affichage suit
    if (mixer)
        mixer->update();
    {
        const QSignalBlocker block (lengthBox);
        lengthBox->setValue (sequencer.patternLength (state.group));
    }
    patternView->update();
    refreshGroups();
    refreshMaschine();
    projectModified = false;
    updateTitle();
}

void MainWindow::newProject()
{
    if (! maybeSave())
        return;
    transport->stop();
    project.reset();
    sequencer.reset();                               // patterns, scenes : tout repart de zero
    if (songsRefresh)
        songsRefresh();
    state.swing = 0;                                 // nouveau projet : sans swing
    if (swingBox)
    {
        const QSignalBlocker block (swingBox);
        swingBox->setValue (0);
    }
    for (auto* sb : { songButton, songTransportButton })   // SONG : celui du projet
        if (sb)
        {
            const QSignalBlocker block (sb);
            sb->setChecked (sequencer.songMode());
        }
    for (int g = 0; g < kGroups; ++g)
    {
        state.groupVolume[(size_t) g] = 100;
        state.groupPan[(size_t) g] = 64;
        state.groupLabel[(size_t) g].clear();
    }
    state.resetSounds();
    syncSamples();
    state.masterVolume = 100;
    bpmBox->setValue (120);
    project.fromApp (sequencer, state, bpmBox->value());
    project.markSaved();
    clearHistory();
    QSettings().remove ("lastProject");
    projectChanged();
}

void MainWindow::openProject()
{
    if (! maybeSave())
        return;
    // Un projet = un dossier NOM.fuzzy ; on choisit le project.json qu'il contient
    QDir().mkpath (projectsDir());
    const QString path = QFileDialog::getOpenFileName (this, tr ("Open Project (choose project.json in a .fuzzy folder)"),
                                                       projectsDir(), tr ("Fuzzy project (project.json)"));
    if (! path.isEmpty())
        openProjectFile (path);
}

bool MainWindow::openProjectFile (const QString& path, bool quiet)
{
    Project loaded;
    QString error;
    if (! loaded.load (path, error))
    {
        if (! quiet)
            QMessageBox::warning (this, tr ("Open Project"), error);
        return false;
    }
    transport->stop();
    project = loaded;
    int bpm = 120;
    project.toApp (sequencer, state, bpm);
    bpmBox->setValue (bpm);
    project.fromApp (sequencer, state, bpm);          // reference pour savoir s'il est modifie
    syncSamples();
    if (songsRefresh)
        songsRefresh();                              // la liste des songs du projet
    if (swingBox)                                    // SWING du projet
    {
        const QSignalBlocker block (swingBox);
        swingBox->setValue (state.swing.load());
    }
    for (auto* sb : { songButton, songTransportButton })   // SONG : celui du projet
        if (sb)
        {
            const QSignalBlocker block (sb);
            sb->setChecked (sequencer.songMode());
        }
    clearHistory();
    project.markSaved();
    QSettings().setValue ("lastProject", project.file);
    if (projectLabel)
        projectChanged();
    return true;
}

bool MainWindow::saveProject()
{
    if (project.file.isEmpty())
        return saveProjectAs();
    project.fromApp (sequencer, state, bpmBox->value());
    QString error;
    // Enregistre dans le dossier du projet ouvert (meme s'il n'est pas dans le dossier par defaut)
    const QString dir = QFileInfo (QFileInfo (project.file).absolutePath()).absolutePath();
    if (! project.save (dir, error))
    {
        QMessageBox::warning (this, tr ("Save Project"), error);
        return false;
    }
    QSettings().setValue ("lastProject", project.file);
    projectModified = false;
    updateTitle();
    statusBar()->showMessage (tr ("Saved %1").arg (project.file), 3000);
    return true;
}

bool MainWindow::saveProjectAs (bool withSamples)
{
    // Emplacement et nom au choix ; le projet est un dossier NOM.fuzzy
    const QString startDir = project.file.isEmpty() ? projectsDir()
                           : QFileInfo (QFileInfo (project.file).absolutePath()).absolutePath();
    QDir().mkpath (startDir);
    const QString path = QFileDialog::getSaveFileName (this, withSamples ? tr ("Save Project With Samples") : tr ("Save Project As"),
                                                       QDir (startDir).filePath (project.name + ".fuzzy"),
                                                       tr ("Fuzzy project (*.fuzzy)"), nullptr, QFileDialog::DontConfirmOverwrite);
    if (path.isEmpty())
        return false;
    QString base = QFileInfo (path).fileName();
    if (base.endsWith (".fuzzy", Qt::CaseInsensitive))
        base.chop (6);
    const QString name = Project::cleanName (base);
    const QString dir = QFileInfo (path).absolutePath();
    const QString folder = Project::folderFor (dir, name);
    const QString target = folder + "/project.json";
    if (QFile::exists (target) && QFileInfo (target).absoluteFilePath() != project.file
        && QMessageBox::question (this, tr ("Save Project As"),
                                  tr ("A project named %1 already exists here. Replace it?").arg (name))
               != QMessageBox::Yes)
        return false;
    QDir().mkpath (folder);

    if (withSamples)
    {
        // Les samples utilises sont copies dans NOM.fuzzy/samples, le projet pointe sur les copies
        QDir().mkpath (folder + "/samples");
        QHash<QString, QString> copied;               // fichier d'origine -> copie
        int count = 0, failed = 0;
        for (QString& sample : state.samples)
        {
            if (sample.isEmpty() || QFileInfo (sample).absolutePath() == QDir (folder + "/samples").absolutePath())
                continue;
            if (! copied.contains (sample))
            {
                const QFileInfo src (sample);
                QString dest = folder + "/samples/" + src.fileName();
                for (int k = 2; QFile::exists (dest); ++k)   // meme nom, autre fichier : numero
                    dest = folder + "/samples/" + src.completeBaseName() + QString (" (%1).").arg (k) + src.suffix();
                if (! QFile::copy (sample, dest))
                {
                    ++failed;
                    continue;
                }
                copied.insert (sample, dest);
                ++count;
            }
            sample = copied.value (sample);
        }
        syncSamples();
        refreshAll();
        if (failed)
            QMessageBox::warning (this, tr ("Save With Samples"), tr ("%1 sample(s) could not be copied (missing file?).").arg (failed));
        statusBar()->showMessage (tr ("%1 sample(s) copied into %2").arg (count).arg (folder + "/samples"), 4000);
    }
    const QString oldName = project.name;
    project.name = name;
    project.fromApp (sequencer, state, bpmBox->value());
    QString error;
    if (! project.save (dir, error))
    {
        project.name = oldName;
        QMessageBox::warning (this, tr ("Save Project"), error);
        return false;
    }
    QSettings().setValue ("projectsDir", dir);       // la prochaine fois : ici
    QSettings().setValue ("lastProject", project.file);
    projectModified = false;
    updateTitle();
    if (! withSamples)
        statusBar()->showMessage (tr ("Saved %1").arg (project.file), 3000);
    return true;
}

bool MainWindow::maybeSave()
{
    if (! project.modified (sequencer, state, bpmBox->value()))
        return true;
    // Don't Save / Cancel / Save : 1re lettre en gras soulignee = sa touche (D, C, S, avec ou sans Cmd).
    // Cancel est choisi par defaut (Entree ne perd rien).
    QDialog box (this);
    box.setWindowTitle (tr ("Unsaved changes"));
    auto* col = new QVBoxLayout (&box);
    auto* text = new QLabel (tr ("Save the changes to project <b>%1</b>?").arg (project.name.toHtmlEscaped()));
    text->setContentsMargins (6, 6, 6, 10);
    col->addWidget (text);
    auto* row = new QHBoxLayout;
    int answer = 0;                                  // 0 = Cancel, 1 = Don't Save, 2 = Save
    auto button = [&] (const QString& word, int value)
    {
        auto* b = new QPushButton;
        auto* inner = new QHBoxLayout (b);
        inner->setContentsMargins (14, 0, 14, 0);
        auto* l = new QLabel (QString ("<b><u>%1</u></b>%2").arg (word.left (1).toHtmlEscaped(), word.mid (1).toHtmlEscaped()));
        l->setAttribute (Qt::WA_TransparentForMouseEvents);
        l->setStyleSheet ("background: transparent;");
        l->setAlignment (Qt::AlignCenter);
        inner->addWidget (l);
        b->setMinimumSize (l->sizeHint().width() + 34, 30);
        connect (b, &QPushButton::clicked, &box, [&box, &answer, value] { answer = value; box.accept(); });
        for (const QString& key : { word.left (1), "Ctrl+" + word.left (1) })   // Ctrl = Cmd sur Mac
            connect (new QShortcut (QKeySequence (key), &box), &QShortcut::activated, b, &QPushButton::click);
        row->addWidget (b);
        return b;
    };
    button (tr ("Don't Save"), 1);
    row->addStretch();
    auto* cancel = button (tr ("Cancel"), 0);
    auto* save = button (tr ("Save"), 2);
    save->setObjectName ("accent");
    col->addLayout (row);
    cancel->setDefault (true);
    cancel->setFocus();
    box.exec();
    if (answer == 2)
        return saveProject();
    return answer == 1;
}

// ------------------------------------------------------------------------------- panneaux et vues

QDockWidget* MainWindow::makeDock (const QString& title, const QString& name, QWidget* content)
{
    auto* dock = new QDockWidget (title, this);
    dock->setObjectName (name);                      // necessaire pour retrouver la disposition
    dock->setWidget (content);
    dock->setFeatures (QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable
                       | QDockWidget::DockWidgetClosable);
    // Panneau deplace, detache ou redimensionne : toute la fenetre est repeinte (sans trait oublie)
    auto repaintAll = [this] { QTimer::singleShot (0, this, [this] { fixSeparators(); repaint(); for (auto* w : findChildren<QWidget*>()) w->update(); }); };
    connect (dock, &QDockWidget::dockLocationChanged, this, repaintAll);
    connect (dock, &QDockWidget::topLevelChanged, this, repaintAll);
    connect (dock, &QDockWidget::visibilityChanged, this, repaintAll);
    return dock;
}

namespace {
const QStringList kBuiltInViews { "Default", "Live", "Mix" };
}

void MainWindow::buildViewMenu (QMenu* menu)
{
    menu->clear();
    // Panneaux : coche = visible
    struct Item { QDockWidget* dock; QKeySequence key; };
    for (const auto& item : { Item { padsDock, QKeySequence ("Ctrl+1") }, Item { groupsDock, QKeySequence ("Ctrl+2") },
                              Item { transportDock, QKeySequence() }, Item { statusDock, QKeySequence ("Ctrl+I") } })
    {
        auto* a = item.dock->toggleViewAction();
        a->setShortcut (item.key);
        menu->addAction (a);
    }
    menu->addSeparator();
    menu->addAction (patternsToolBar->toggleViewAction());   // barre Patterns / Scenes
    menu->addAction (tr ("Display Diagnostics (F12)"), this, [this] { showDisplayDiagnostics(); });
    menu->addSeparator();
    auto* mixerAction = menu->addAction (tr ("Mixer"), this, [this] { showLeftTab (0); });
    mixerAction->setShortcut (QKeySequence ("Ctrl+M"));
    auto* patternAction = menu->addAction (tr ("Pattern"), this, [this] { showLeftTab (1); });
    patternAction->setShortcut (QKeySequence ("Ctrl+P"));
    auto* scenesAction = menu->addAction (tr ("Scenes"), this, [this] { showLeftTab (6); });
    scenesAction->setShortcut (QKeySequence ("Ctrl+Shift+S"));
    auto* trackerAction = menu->addAction (tr ("Tracker"), this, [this] { showLeftTab (5); });
    trackerAction->setShortcut (QKeySequence ("Ctrl+T"));
    auto* rollAction = menu->addAction (tr ("Piano Roll"), this, [this] { showLeftTab (2); });
    rollAction->setShortcut (QKeySequence ("Ctrl+R"));
    auto* keysAction = menu->addAction (tr ("Keyboard"), this, [this] { showLeftTab (3); });
    keysAction->setShortcut (QKeySequence ("Ctrl+K"));
    auto* soundAction = menu->addAction (tr ("Sound"), this, [this] { showLeftTab (4); });
    soundAction->setShortcut (QKeySequence ("Ctrl+U"));
    auto* bankAction = menu->addAction (tr ("Switch Group Bank (A-H / A2-H2)"), this, [this] { switchBank(); });
    bankAction->setShortcut (QKeySequence ("Ctrl+B"));

    // Vues : toutes faites, puis celles qu'on a enregistrees
    menu->addSeparator();
    auto* title = menu->addAction (tr ("Views"));
    title->setEnabled (false);
    int n = 0;
    QSettings s;
    s.beginGroup ("views");
    const QStringList own = s.childGroups();
    s.endGroup();
    for (const QString& name : kBuiltInViews + own)
    {
        auto* a = menu->addAction ("   " + name, this, [this, name] { applyView (name); });
        if (n < 9)
            a->setShortcut (QKeySequence (QString ("Ctrl+Alt+%1").arg (++n)));
    }
    menu->addAction (tr ("Save View As..."), this, [this] { saveViewAs(); });
    menu->addAction (tr ("Save as Default"), this, [this]  // la disposition actuelle devient « Default »
    {
        defaultLayout = saveState (kLayoutVersion);
        QSettings().setValue ("layout/default", defaultLayout);
        toast ("THIS LAYOUT IS NOW THE DEFAULT VIEW");
    });
    if (QSettings().contains ("layout/default"))
        menu->addAction (tr ("Reset Default (factory)"), this, [this]
        {
            QSettings().remove ("layout/default");
            defaultLayout = factoryLayout;
            applyView ("Default");
            toast ("DEFAULT VIEW: FACTORY LAYOUT");
        });
    if (! own.isEmpty())
    {
        auto* del = menu->addMenu (tr ("Delete View"));
        for (const QString& name : own)
            del->addAction (name, this, [name]
            {
                QSettings settings;
                settings.remove ("views/" + name);
            });
    }
}

void MainWindow::applyView (const QString& name)
{
    if (kBuiltInViews.contains (name))
    {
        restoreState (defaultLayout, kLayoutVersion);
        placeTracker();
        QTimer::singleShot (50, this, [this] { fixSeparators(); });
        statusDock->hide();
        if (name == "Live")                          // jouer : pattern, pads, groupes (sans clavier)
        {
            keysTabDock->hide();
            showLeftTab (1);
        }
        else if (name == "Mix")                      // mixer : mixeur en grand, groupes
        {
            padsDock->hide();
            keysTabDock->hide();
            showLeftTab (0);
        }
        else
            showLeftTab (1);
    }
    else
    {
        QSettings s;
        restoreState (s.value ("views/" + name + "/state").toByteArray(), kLayoutVersion);
        placeTracker();
        QTimer::singleShot (50, this, [this] { fixSeparators(); });
        showLeftTab (s.value ("views/" + name + "/tab", 1).toInt());
    }
    statusBar()->showMessage (tr ("View: %1").arg (name), 1500);
}

void MainWindow::saveViewAs()
{
    bool ok = false;
    const QString typed = QInputDialog::getText (this, tr ("Save View As"),
                                                 tr ("Name of this view (panels, places, sizes):"),
                                                 QLineEdit::Normal, tr ("My view"), &ok).trimmed();
    if (! ok || typed.isEmpty())
        return;
    QString name = typed;
    name.replace ('/', '-');
    if (kBuiltInViews.contains (name, Qt::CaseInsensitive))
        name += " 2";                                // les vues fournies ne s'ecrasent pas
    QSettings s;
    s.setValue ("views/" + name + "/state", saveState (kLayoutVersion));
    s.setValue ("views/" + name + "/tab", patternShown() ? 1 : 0);
    statusBar()->showMessage (tr ("View saved: %1").arg (name), 1500);
}

// ------------------------------------------------------------------------------- annuler, copier, coller

void MainWindow::remember (int group)
{
    std::lock_guard<std::mutex> lk (undoLock);
    UndoStep step;
    step.group = group;
    step.patternIndex = sequencer.currentPattern (group);
    step.pattern = sequencer.pattern (group);
    undoStack.push_back (std::move (step));
    if (undoStack.size() > 100)                       // 100 pas en arriere (patterns de 256 lignes)
        undoStack.erase (undoStack.begin());
    redoStack.clear();
}

MainWindow::UndoStep MainWindow::capture (const UndoStep& like) const
{
    UndoStep now;
    now.label = like.label;
    now.group = like.group;
    if (like.group >= 0)
        now.pattern = sequencer.patternAt (like.group, like.patternIndex);
    for (const auto& gs : like.groups)
        now.groups.push_back ({ gs.group, sequencer.groupPatterns (gs.group), state.groupVolume[(size_t) gs.group].load(), state.groupLabel[(size_t) gs.group] });
    for (const auto& entry : like.sounds)
        now.sounds.push_back ({ entry.first, takeSound (entry.first / 16, entry.first % 16 + 1) });
    if (like.hasScenes)
    {
        now.hasScenes = true;
        for (int i = 0; i < seq::kPatternSlots; ++i)
            now.scenes.push_back (sequencer.scene (i));
        now.songs = sequencer.allSongs();
        now.songIndex = sequencer.currentSong();
    }
    return now;
}

void MainWindow::rememberPatterns (int g, const QString& label)
{
    // Avant une action du gestionnaire de patterns : Cmd+Z remet les patterns du groupe et les scenes
    UndoStep step;
    step.label = label;
    if (g >= 0)                                      // -1 : les scenes seulement
        step.groups.push_back ({ g, sequencer.groupPatterns (g), state.groupVolume[(size_t) g].load(), state.groupLabel[(size_t) g] });
    step.hasScenes = true;
    for (int i = 0; i < seq::kPatternSlots; ++i)
        step.scenes.push_back (sequencer.scene (i));
    step.songs = sequencer.allSongs();
    step.songIndex = sequencer.currentSong();
    std::lock_guard<std::mutex> lk (undoLock);
    undoStack.push_back (std::move (step));
    if (undoStack.size() > 100)
        undoStack.erase (undoStack.begin());
    redoStack.clear();
}

void MainWindow::rememberSounds (const std::vector<std::pair<int, int>>& pads, const std::vector<int>& groups, const QString& label)
{
    // Avant de changer des sons (et des groupes entiers) : Cmd+Z les remet
    UndoStep step;
    step.label = label;
    for (const auto& [g, p] : pads)
        step.sounds.push_back ({ state.index (g, p), takeSound (g, p) });
    for (int g : groups)
    {
        step.groups.push_back ({ g, sequencer.groupPatterns (g), state.groupVolume[(size_t) g].load(), state.groupLabel[(size_t) g] });
        for (int p = 1; p <= 16; ++p)
            step.sounds.push_back ({ state.index (g, p), takeSound (g, p) });
    }
    std::lock_guard<std::mutex> lk (undoLock);
    undoStack.push_back (std::move (step));
    if (undoStack.size() > 100)
        undoStack.erase (undoStack.begin());
    redoStack.clear();
}

void MainWindow::clearHistory()
{
    std::lock_guard<std::mutex> lk (undoLock);
    undoStack.clear();
    redoStack.clear();
}

void MainWindow::undo (bool redoIt)
{
    UndoStep step;
    {
        std::lock_guard<std::mutex> lk (undoLock);
        auto& from = redoIt ? redoStack : undoStack;
        auto& to = redoIt ? undoStack : redoStack;
        if (from.empty())
        {
            statusBar()->showMessage (redoIt ? tr ("Nothing to redo") : tr ("Nothing to undo"), 1200);
            return;
        }
        step = from.back();
        from.pop_back();
        to.push_back (capture (step));
    }
    if (step.group >= 0)
    {
        // Le pattern ou la modification a eu lieu (il peut ne plus etre celui qui joue)
        sequencer.setPatternAt (step.group, step.patternIndex, step.pattern);
        if (sequencer.currentPattern (step.group) != step.patternIndex && ! transport->playing())
            sequencer.selectPattern (step.group, step.patternIndex, false);
    }
    for (const auto& gs : step.groups)
    {
        sequencer.setGroupPatterns (gs.group, gs.patterns);
        state.groupVolume[(size_t) gs.group] = gs.volume;
        state.groupLabel[(size_t) gs.group] = gs.label;
    }
    if (step.hasScenes)
    {
        for (int i = 0; i < (int) step.scenes.size(); ++i)
            sequencer.setScene (i, step.scenes[(size_t) i]);
        if (! step.songs.empty())
        {
            const bool mode = sequencer.songMode();
            sequencer.setAllSongs (step.songs, step.songIndex);
            sequencer.setSongMode (mode);
        }
        if (songView)
            songView->update();
        if (songsRefresh)
            songsRefresh();
    }
    for (const auto& [i, s] : step.sounds)
        putSound (i / 16, i % 16 + 1, s);
    if (! step.sounds.empty() || ! step.groups.empty())
    {
        syncSamples();
        refreshAll();
    }
    const int shown = step.group >= 0 ? step.group : ! step.groups.empty() ? step.groups.back().group
                    : ! step.sounds.empty() ? step.sounds.back().first / 16 : state.group.load();
    if (shown != state.group)                         // on montre le groupe concerne
        selectGroup (shown);
    showPatternState();
    patternBar->refresh (true);
    if (sceneView)
        sceneView->update();
    if (patternManagerRefresh)
        patternManagerRefresh();
    if (sceneListRefresh)
        sceneListRefresh();
    if (palette)
        palette->update();
    statusBar()->showMessage (QString ("%1 (%2)").arg (redoIt ? tr ("Redo") : tr ("Undo"),
                                                       step.label.isEmpty() ? groupName (shown) : step.label), 1500);
}

void MainWindow::showPatternState()
{
    {
        const QSignalBlocker block (lengthBox);
        lengthBox->setValue (sequencer.patternLength (state.group));
    }
    patternView->clearSelection();
}

void MainWindow::copySelection (bool cut)
{
    if (lastArea == Area::Song && songView && ! songView->visibleRegion().isEmpty())
    {
        songCopyBlocks (cut);
        return;
    }
    if (lastArea == Area::Pads)                      // pads choisis (sinon le dernier pad clique)
    {
        QList<int> sel = pads->selection();
        copySounds (sel.isEmpty() ? QList<int> { lastPad } : sel, cut);
        return;
    }
    if (lastArea == Area::Groups)
    {
        copyGroups (groupTargets (state.group), cut);
        return;
    }
    if (! patternShown())
        return;
    QList<QPoint> sel = patternView->selection();
    if (sel.isEmpty())                                // rien de choisi : la case du curseur
        sel = { QPoint (patternView->selectedTrack(), patternView->selectedRow()) };
    const seq::Pattern p = sequencer.pattern (state.group);
    int first = seq::kMaxRows;
    for (const QPoint& c : sel)
        first = std::min (first, c.y());
    std::vector<Clip> clip;
    for (const QPoint& c : sel)
        if (! p.rows[(size_t) c.y()][(size_t) c.x()].empty())
            clip.push_back ({ c.y() - first, c.x(), p.rows[(size_t) c.y()][(size_t) c.x()] });
    if (clip.empty())
        return;
    clipboard = clip;
    if (cut)
    {
        remember (state.group);
        for (const Clip& c : clip)
            sequencer.setCell (state.group, c.row + first, c.track, {});
        patternView->clearSelection();
    }
    statusBar()->showMessage (tr ("%1 note(s) %2").arg (clip.size()).arg (cut ? tr ("cut") : tr ("copied")), 1200);
}

void MainWindow::paste()
{
    if (lastArea == Area::Song && songView && ! songView->visibleRegion().isEmpty())
    {
        songPasteBlocks();
        return;
    }
    if (lastArea == Area::Pads)
    {
        const QList<int> sel = pads->selection();
        pasteSounds (sel.isEmpty() ? lastPad : sel.first());
        return;
    }
    if (lastArea == Area::Groups)
    {
        pasteGroups (state.group);
        return;
    }
    // Colle a partir de la ligne du curseur, chaque note dans sa piste
    if (! patternShown() || clipboard.empty())
        return;
    remember (state.group);
    const int base = patternView->selectedRow(), length = sequencer.patternLength (state.group);
    QList<QPoint> pasted;
    for (const Clip& c : clipboard)
        if (base + c.row < length)
        {
            sequencer.setCell (state.group, base + c.row, c.track, c.cell);
            pasted.append (QPoint (c.track, base + c.row));
        }
    patternView->setSelection (pasted);
    statusBar()->showMessage (tr ("%1 note(s) pasted").arg (pasted.size()), 1200);
}

void MainWindow::selectAll()
{
    if (rollShown())
    {
        pianoRoll->selectAll();
        return;
    }
    if (! patternShown())
        return;
    const seq::Pattern p = sequencer.pattern (state.group);
    QList<QPoint> all;
    for (int r = 0; r < p.length; ++r)
        for (int t = 0; t < seq::kTracks; ++t)
            if (! p.rows[(size_t) r][(size_t) t].empty())
                all.append (QPoint (t, r));
    patternView->setSelection (all);
}

void MainWindow::doublePattern()
{
    // Longueur x 2 et les notes recopiees dans la nouvelle moitie (DOUBLE de la Maschine)
    const seq::Pattern p = sequencer.pattern (state.group);
    if (p.length * 2 > seq::kMaxRows)
    {
        statusBar()->showMessage (tr ("Pattern already %1 lines (max %2)").arg (p.length).arg (seq::kMaxRows), 1500);
        return;
    }
    remember (state.group);
    seq::Pattern d = p;
    for (int r = 0; r < p.length; ++r)
        d.rows[(size_t) (r + p.length)] = p.rows[(size_t) r];
    d.length = p.length * 2;
    sequencer.setPattern (state.group, d);
    showPatternState();
    statusBar()->showMessage (tr ("Pattern doubled: %1 lines").arg (d.length), 1200);
}

// ------------------------------------------------------------------------------- mute / solo

void MainWindow::toggleGroup (int group, bool solo)
{
    auto& flag = solo ? state.groupSolo[(size_t) group] : state.groupMute[(size_t) group];
    flag = ! flag;
    QMetaObject::invokeMethod (this, [this] { refreshMuteSolo(); }, Qt::QueuedConnection);
}

void MainWindow::toggleSound (int group, int sound, bool solo)
{
    const size_t i = (size_t) (group * 16 + sound - 1);
    auto& flag = solo ? state.soundSolo[i] : state.soundMute[i];
    flag = ! flag;
    QMetaObject::invokeMethod (this, [this] { refreshMuteSolo(); }, Qt::QueuedConnection);
}

void MainWindow::refreshMuteSolo()
{
    if (sceneView)
        sceneView->update();                         // vue Scenes : groupes muets en gris
    // « Unmute all » / « Unsolo all » : actifs seulement s'il y a quelque chose a reactiver
    bool anyMute = false, anySolo = false, anyGMute = false, anyGSolo = false;
    for (int s = 0; s < 16; ++s)
    {
        anyMute |= state.soundMute[(size_t) (state.group * 16 + s)].load();
        anySolo |= state.soundSolo[(size_t) (state.group * 16 + s)].load();
    }
    for (int g = 0; g < kGroups; ++g)
    {
        anyGMute |= state.groupMute[(size_t) g].load();
        anyGSolo |= state.groupSolo[(size_t) g].load();
    }
    padsUnmute->setEnabled (anyMute);
    padsUnsolo->setEnabled (anySolo);
    mixUnmute->setEnabled (anyGMute);
    mixUnsolo->setEnabled (anyGSolo);
    groupUnmute->setEnabled (anyGMute);
    groupUnsolo->setEnabled (anyGSolo);
    std::array<char, 16> tags {};
    for (int p = 1; p <= 16; ++p)
        tags[(size_t) p - 1] = state.soundTag (state.group, p);
    pads->setTags (tags);
    patternView->update();
    mixer->update();
    // Boutons de groupes (sans toucher au reste de l'affichage du groupe)
    const int bank = state.group >= 8 ? 8 : 0;
    for (int i = 0; i < 8; ++i)
    {
        const int g = bank + i;
        const QString tag = state.groupMute[(size_t) g] ? " M" : state.groupSolo[(size_t) g] ? " S" : "";
        groupButtons[(size_t) i]->setText (state.groupLabel[(size_t) g].isEmpty() ? groupName (g) + tag : groupName (g) + tag + "\n" + state.groupLabel[(size_t) g]);
        const QColor c = state.groupAudible (g) ? groupColor (g) : QColor (90, 90, 98);
        const QString frame = groupChosen.contains (g) ? QString ("3px dashed #fc8c28") : QString ("2px solid %1").arg (css (c));
        groupButtons[(size_t) i]->setStyleSheet (QString ("QPushButton { color: %1; border: %2; background: #17171c; font-size: 16px; font-weight: bold; }"
                                                          "QPushButton:checked { color: #111; background: %1; }").arg (css (c), frame));
    }
    refreshMaschine();
}

QWidget* MainWindow::muteSoloRow (QPushButton*& unmute, QPushButton*& unsolo, bool groups, bool vertical)
{
    // Deux petits boutons : tout reactiver d'un coup (sons du groupe, ou groupes du mixeur) ;
    // vertical : l'un au-dessus de l'autre (panneau Pads, gagne une ligne)
    auto* row = new QWidget;
    QBoxLayout* h = vertical ? (QBoxLayout*) new QVBoxLayout (row) : (QBoxLayout*) new QHBoxLayout (row);
    h->setContentsMargins (0, 0, 0, 0);
    h->setSpacing (vertical ? 3 : 6);
    unmute = new QPushButton (tr ("A/M"));           // norme de l'app : A/M = tout dé-muter
    unsolo = new QPushButton (tr ("A/S"));           //                   A/S = tout dé-soloer
    unmute->setObjectName ("unmute");
    unsolo->setObjectName ("unsolo");
    for (auto* b : { unmute, unsolo })
    {
        b->setFocusPolicy (Qt::NoFocus);
        b->setEnabled (false);
    }
    unmute->setToolTip (groups ? tr ("A/M: turn every muted group back on (red = something is muted)")
                               : tr ("A/M: turn every muted sound of this group back on (red = something is muted)"));
    unsolo->setToolTip (groups ? tr ("A/S: clear the solo of every group (yellow = something is solo)")
                               : tr ("A/S: clear the solo of every sound of this group (yellow = something is solo)"));
    connect (unmute, &QPushButton::clicked, this, [this, groups]
    {
        for (int i = 0; i < (groups ? kGroups : 16); ++i)
            (groups ? state.groupMute[(size_t) i] : state.soundMute[(size_t) (state.group * 16 + i)]) = false;
        refreshMuteSolo();
    });
    connect (unsolo, &QPushButton::clicked, this, [this, groups]
    {
        for (int i = 0; i < (groups ? kGroups : 16); ++i)
            (groups ? state.groupSolo[(size_t) i] : state.soundSolo[(size_t) (state.group * 16 + i)]) = false;
        refreshMuteSolo();
    });
    if (! groups)
        h->addStretch();
    h->addWidget (unmute);
    h->addWidget (unsolo);
    return row;
}

// ------------------------------------------------------------------------------- samples des pads

QString MainWindow::padName (int group, int sound) const
{
    const QString& path = state.samples[(size_t) (group * 16 + sound - 1)];
    if (! path.isEmpty())
        return QFileInfo (path).completeBaseName();
    static const char* synths[] { "", "Kick", "Snare", "Closed HH", "Open HH", "" };
    const int syn = state.effSynth (group, sound);
    if (syn == 0)
        return tr ("(empty)");                        // son vide
    if (state.synth[(size_t) state.index (group, sound)] >= 1 && syn <= 4)
        return synths[syn];                          // synthe choisi
    return soundName (sound);
}

void MainWindow::setPadSample (int group, int sound, const QString& path, bool undoable)
{
    if (undoable)
        rememberSounds ({ { group, sound } }, {}, path.isEmpty() ? QString ("Remove sample") : QString ("Load sample"));
    state.samples[(size_t) (group * 16 + sound - 1)] = path;
    syncSamples();
    refreshAll();
    if (! path.isEmpty())
        statusBar()->showMessage (tr ("Pad %1: %2").arg (sound, 2, 10, QChar ('0')).arg (QFileInfo (path).fileName()), 2000);
}

void MainWindow::syncSamples (bool all)
{
    // Qui a un instrument (sample ou synthe) : la lecture et les pads s'en servent
    for (int i = 0; i < kGroups * 16; ++i)
        state.hasInstrument[(size_t) i] = ! state.samples[(size_t) i].isEmpty() || state.effSynth (i / 16, i % 16 + 1) != 0;
    // Envoie au moteur les samples qui ont change ("" = retour au son de synthese)
    if (! engine->running())
        return;
    for (int g = 0; g < kGroups; ++g)
        for (int s = 1; s <= 16; ++s)
        {
            const size_t i = (size_t) (g * 16 + s - 1);
            if (all || sentSamples[i] != state.samples[i])
            {
                engine->setSample (g, s, state.samples[i], defaultNote (s));
                sentSamples[i] = state.samples[i];
            }
            // A l'envers (Reverse)
            const int rev = state.soundReverse[i] ? 1 : 0;
            if (all || sentReverse[i] != rev)
            {
                engine->setReverse (g, s, rev != 0);
                sentReverse[i] = rev;
            }
            // Boucle (envoyee apres le sample : le moteur garde l'ordre)
            const bool loop = state.playMode[i] == session::State::Loop;
            const QString key = QString ("%1;%2;%3").arg (loop).arg (state.loopStart[i].load()).arg (state.loopEnd[i].load());
            if (all || sentLoops[i] != key)
            {
                engine->setLoop (g, s, loop, state.loopStart[i], state.loopEnd[i]);
                sentLoops[i] = key;
            }
        }
}

void MainWindow::loadSampleDialog (int pad)
{
    QSettings settings;
    const QString path = QFileDialog::getOpenFileName (this, tr ("Load Sample on pad %1").arg (pad, 2, 10, QChar ('0')),
                                                       settings.value ("sampleDir").toString(),
                                                       tr ("Audio files (*.wav *.aif *.aiff *.flac *.ogg *.mp3 *.m4a *.aac *.caf)"));
    if (path.isEmpty())
        return;
    settings.setValue ("sampleDir", QFileInfo (path).absolutePath());
    setPadSample (state.group, pad, path);
    selectSound (pad);
}

void MainWindow::editLoop (int pad)
{
    LoopDialog dialog (*engine, state, state.group, pad, padName (state.group, pad), groupColor (state.group), this);
    openLoopDialog = &dialog;                        // SAMPLING (Maschine) la ferme
    dialog.exec();
    openLoopDialog = nullptr;
    // Ce que la fenetre a envoye au moteur est maintenant la reference
    const size_t i = (size_t) session::State::index (state.group, pad);
    sentLoops[i] = QString ("%1;%2;%3").arg (state.playMode[i] == session::State::Loop)
                       .arg (state.loopStart[i].load()).arg (state.loopEnd[i].load());
    refreshAll();
}

// ------------------------------------------------------------------------------- piano roll

QWidget* MainWindow::buildPianoRoll()
{
    auto* panel = new QFrame;
    panel->setObjectName ("panel");
    auto* col = new QVBoxLayout (panel);
    auto* head = new QHBoxLayout;
    rollTitle = new QLabel;
    rollTitle->setObjectName ("section");
    rollTitle->setFixedWidth (180);                  // largeur fixe : la barre ne bouge plus (nom raccourci)
    auto* hint = new QLabel (tr ("click = note \u00B7 drag in empty space = select \u00B7 drag a note = move (%1 copy) \u00B7 double-click = erase \u00B7 bottom bars = velocity")
                                 .arg (PatternView::copyKey()));
    hint->setObjectName ("hint");
    registerHint (hint, "PIANO ROLL");
    hint->setSizePolicy (QSizePolicy::Ignored, QSizePolicy::Preferred);   // peut retrecir
    hint->setMinimumWidth (0);
    head->addWidget (rollTitle);
    head->addStretch();
    head->addWidget (hint);
    auto* gen = new QPushButton (QStringLiteral ("\U0001F3B2 ") + tr ("Generate"));
    gen->setFocusPolicy (Qt::NoFocus);
    gen->setToolTip (tr ("Random notes for this sound (Cmd+G)"));
    connect (gen, &QPushButton::clicked, this, [this] { openGenerate(); });
    // Edit : ajouter / effacer a la souris ; Select : choisir des notes (cadre, Shift + clic)
    rollEditButton = new QPushButton (tr ("Edit"));
    rollSelectButton = new QPushButton (tr ("Select"));
    for (auto* b : { rollEditButton, rollSelectButton })
    {
        b->setCheckable (true);
        b->setFocusPolicy (Qt::NoFocus);
        b->setObjectName ("keys");                   // orange quand actif
    }
    rollEditButton->setToolTip (tr ("Edit mode: click adds a note, double-click / right-click erases"));
    rollSelectButton->setToolTip (tr ("Select mode: drag a frame or Shift + click to choose notes"));
    head->addWidget (rollEditButton);
    head->addWidget (rollSelectButton);
    head->addWidget (gen);
    QList<QPushButton*> transposes;
    for (const int step : { -1, 1, -12, 12 })        // transposition : selection, sinon la derniere note
    {
        auto* t = new QPushButton (step == -12 ? tr ("OCT -") : step == -1 ? tr ("SEMI -") : step == 1 ? tr ("SEMI +") : tr ("OCT +"));
        t->setFocusPolicy (Qt::NoFocus);
        t->setToolTip (tr ("Transpose the selected notes (or the last note touched)"));
        connect (t, &QPushButton::clicked, this, [this, step] { transposeRoll (step); });
        head->addWidget (t);
        transposes << t;
    }
    col->addLayout (head);
    pianoRoll = new PianoRollView (sequencer, state);
    {
        QSettings s;
        rollEditButton->setChecked (s.value ("roll/edit", true).toBool());
        rollSelectButton->setChecked (true);         // Select : toujours actif au demarrage (avec Edit)
        pianoRoll->setEditMode (rollEditButton->isChecked());
        pianoRoll->setSelectMode (rollSelectButton->isChecked());
    }
    connect (rollEditButton, &QPushButton::toggled, this, [this] (bool on)
    {
        pianoRoll->setEditMode (on);
        QSettings().setValue ("roll/edit", on);
    });
    connect (rollSelectButton, &QPushButton::toggled, this, [this] (bool on)
    {
        pianoRoll->setSelectMode (on);
        QSettings().setValue ("roll/select", on);
    });
    connect (pianoRoll, &PianoRollView::midiDropped, this, &MainWindow::importMidi);
    connect (pianoRoll, &PianoRollView::aboutToEdit, this, [this] { remember (state.group); });
    connect (pianoRoll, &PianoRollView::edited, this, [this] { patternView->update(); });
    connect (pianoRoll, &PianoRollView::keyPressed, this, [this] (int pitch) { rollKeyHeld = true; playNote (pitch, state.velocity, true); });
    connect (pianoRoll, &PianoRollView::keyReleased, this, [this] (int pitch) { playNote (pitch, 0, false); rollKeyHeld = false; liveCheck(); });
    // Barres de defilement et zoom (H = temps, V = hauteur des notes)
    auto* hBar = new QScrollBar (Qt::Horizontal);
    auto* vBar = new QScrollBar (Qt::Vertical);
    auto* zoomH = new QSlider (Qt::Horizontal);
    auto* zoomV = new QSlider (Qt::Horizontal);
    zoomH->setRange (100, 1600);
    zoomV->setRange (3, 30);
    for (auto* z : { zoomH, zoomV })
    {
        z->setFixedWidth (110);
        z->setFocusPolicy (Qt::NoFocus);
    }
    zoomH->setToolTip (tr ("Horizontal zoom: time (Cmd + wheel)"));
    zoomV->setToolTip (tr ("Vertical zoom: note height (%1 + wheel)").arg (PatternView::copyKey()));
    auto* zoomLabel = new QLabel (tr ("Zoom  H"));
    zoomLabel->setObjectName ("hint");
    auto* vLabel = new QLabel (tr ("V"));
    vLabel->setObjectName ("hint");
    auto* zoomBox = new QWidget;                     // les 2 zooms ensemble, serres (rien ne s'etire entre eux)
    auto* zoomRow = new QHBoxLayout (zoomBox);
    zoomRow->setContentsMargins (0, 0, 0, 0);
    zoomRow->setSpacing (6);
    zoomRow->addWidget (zoomLabel);
    zoomRow->addWidget (zoomH);
    zoomRow->addSpacing (8);
    zoomRow->addWidget (vLabel);
    zoomRow->addWidget (zoomV);
    zoomBox->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Preferred);
    head->insertWidget (2, zoomBox);
    auto syncBars = [this, hBar, vBar, zoomH, zoomV]
    {
        const QSignalBlocker b1 (hBar), b2 (vBar), b3 (zoomH), b4 (zoomV);
        const int length = sequencer.patternLength (state.group);
        hBar->setRange (0, std::max (0, length - pianoRoll->visibleRows()));
        hBar->setPageStep (pianoRoll->visibleRows());
        hBar->setValue (pianoRoll->scrollX());
        const int visible = pianoRoll->visiblePitches();
        vBar->setRange (0, std::max (0, 128 - visible));
        vBar->setPageStep (visible);
        vBar->setValue (127 - pianoRoll->topPitchShown());
        zoomH->setValue (pianoRoll->zoomX());
        zoomV->setValue (pianoRoll->rowHeight());
    };
    connect (pianoRoll, &PianoRollView::viewChanged, this, syncBars);
    connect (hBar, &QScrollBar::valueChanged, pianoRoll, &PianoRollView::setScrollX);
    connect (vBar, &QScrollBar::valueChanged, this, [this] (int v) { pianoRoll->setTopPitch (127 - v); });
    connect (zoomH, &QSlider::valueChanged, pianoRoll, &PianoRollView::setZoomX);
    connect (zoomV, &QSlider::valueChanged, pianoRoll, &PianoRollView::setRowHeight);
    auto* rollPage = new QWidget;
    auto* grid = new QGridLayout (rollPage);
    grid->setContentsMargins (0, 0, 0, 0);
    grid->setSpacing (0);
    grid->addWidget (pianoRoll, 0, 0);
    grid->addWidget (vBar, 0, 1);
    grid->addWidget (hBar, 1, 0);
    syncBars();

    // Trois affichages de la piste du son choisi : Roll (notes), Steps (pas, comme la Maschine),
    // Tracker (colonnes NOTE / VEL / LONG). Le reste de la barre suit l'affichage.
    stepView = new StepView (sequencer, state, *patternView);
    rollTracker = new TrackerView (sequencer, state, *patternView);
    rollTracker->setSingleTrack (true);
    rollStack = new QStackedWidget;
    rollStack->addWidget (rollPage);
    rollStack->addWidget (stepView);
    rollStack->addWidget (rollTracker);
    col->addWidget (rollStack, 1);
    auto* modes = new QButtonGroup (panel);
    int k = 0;
    for (const QString& name : { tr ("Roll"), tr ("Steps"), tr ("Track") })
    {
        auto* b = new QPushButton (name);
        b->setCheckable (true);
        b->setFocusPolicy (Qt::NoFocus);
        b->setObjectName ("keys");
        b->setToolTip (k == 0 ? tr ("Piano roll of the chosen sound")
                     : k == 1 ? tr ("The chosen sound in steps, 16 per line (like the Maschine step mode)")
                              : tr ("The chosen sound as a tracker column (NOTE / VEL / LENGTH)"));
        modes->addButton (b, k);
        head->insertWidget (1 + k, b);
        ++k;
    }
    // Barre commune aux 3 affichages, toujours dans le meme ordre (comme la vue Pattern) :
    // titre, Roll / Steps / Track, Length, x2, Generate, Clear, SEMI / OCT, puis les reglages
    // propres a l'affichage, l'aide a droite.
    auto* lenLabel = new QLabel (tr ("Length"));
    lenLabel->setObjectName ("hint");
    rollLength = new QSpinBox;
    rollLength->setRange (1, seq::kMaxRows);
    rollLength->setFocusPolicy (Qt::ClickFocus);
    rollLength->setToolTip (tr ("Pattern length in lines (4 lines = 1 beat)"));
    rollLength->setValue (sequencer.patternLength (state.group));
    connect (rollLength, &QSpinBox::editingFinished, rollLength, [this] { rollLength->clearFocus(); });
    connect (rollLength, &QSpinBox::valueChanged, this, [this] (int v) { lengthBox->setValue (v); pianoRoll->update(); });
    auto* twice = new QPushButton (QStringLiteral ("\u00D72"));
    twice->setFocusPolicy (Qt::NoFocus);
    twice->setToolTip (tr ("Double: length x 2, notes copied into the new half"));
    connect (twice, &QPushButton::clicked, this, [this] { doublePattern(); });
    auto* clear = new QPushButton (tr ("Clear"));
    clear->setFocusPolicy (Qt::NoFocus);
    clear->setToolTip (tr ("Erase the notes of this sound"));
    connect (clear, &QPushButton::clicked, this, [this]
    {
        remember (state.group);                      // seulement la piste du son choisi
        for (int r = 0; r < sequencer.patternLength (state.group); ++r)
            sequencer.setCell (state.group, r, state.sound - 1, {});
        patternView->update();
        pianoRoll->update();
    });
    // Track : Edit et Step, les memes que l'onglet Tracker
    auto* trackEdit = new QPushButton (tr ("Edit"));
    trackEdit->setCheckable (true);
    trackEdit->setFocusPolicy (Qt::NoFocus);
    trackEdit->setObjectName ("keys");
    trackEdit->setChecked (trackerEditButton->isChecked());
    connect (trackEdit, &QPushButton::toggled, trackerEditButton, &QPushButton::setChecked);
    connect (trackerEditButton, &QPushButton::toggled, trackEdit, &QPushButton::setChecked);
    connect (trackerEditButton, &QPushButton::toggled, rollTracker, &TrackerView::setEditMode);
    rollTracker->setEditMode (trackerEditButton->isChecked());
    auto* stepLabel = new QLabel (tr ("Step"));
    stepLabel->setObjectName ("hint");
    auto* trackStep = new QSpinBox;
    trackStep->setRange (0, 16);
    trackStep->setFocusPolicy (Qt::ClickFocus);
    trackStep->setValue (trackerStep->value());
    connect (trackStep, &QSpinBox::valueChanged, trackerStep, &QSpinBox::setValue);
    connect (trackerStep, &QSpinBox::valueChanged, trackStep, &QSpinBox::setValue);
    while (auto* item = head->takeAt (0))            // on range tout dans le meme ordre
        delete item;                                 // (les boutons restent : seule la place part)
    head->addWidget (rollTitle);
    for (auto* b : modes->buttons())
        head->addWidget (b);
    head->addSpacing (8);
    head->addWidget (lenLabel);
    head->addWidget (rollLength);
    head->addWidget (twice);
    head->addWidget (gen);
    head->addWidget (clear);
    head->addSpacing (8);
    for (auto* t : transposes)
        head->addWidget (t);
    head->addSpacing (8);
    const QList<QWidget*> rollOnly { rollEditButton, rollSelectButton, zoomBox };
    const QList<QWidget*> trackOnly { trackEdit, stepLabel, trackStep };
    for (auto* w : rollOnly + trackOnly)
        head->addWidget (w);
    head->addWidget (hint, 1);
    hint->setAlignment (Qt::AlignRight | Qt::AlignVCenter);
    auto showMode = [this, rollOnly, trackOnly, hint] (int id)
    {
        rollStack->setCurrentIndex (id);
        for (auto* w : rollOnly)
            w->setVisible (id == 0);
        for (auto* w : trackOnly)
            w->setVisible (id == 2);
        hint->setText (id == 0 ? tr ("click = note \u00B7 drag in empty space = select \u00B7 drag a note = move (%1 copy) \u00B7 double-click = erase \u00B7 bottom bars = velocity").arg (PatternView::copyKey())
                     : id == 1 ? tr ("click = note \u00B7 drag = frame \u00B7 Cmd + click = add \u00B7 Shift + click = range \u00B7 drag a note = move (Option = copy) \u00B7 double-click = options \u00B7 wheel = velocity \u00B7 Del = erase")
                               : tr ("keys = notes \u00B7 digits = velocity / length \u00B7 Del = erase \u00B7 %1 + wheel = value").arg (PatternView::copyKey()));
    };
    connect (modes, &QButtonGroup::idClicked, this, [showMode] (int id)
    {
        showMode (id);
        QSettings().setValue ("roll/mode", id);
    });
    const int mode = std::clamp (QSettings().value ("roll/mode", 0).toInt(), 0, 2);
    modes->button (mode)->setChecked (true);
    showMode (mode);
    connect (stepView, &StepView::addRequested, this, [this] (int row)
    {
        const int snd = state.sound, note = defaultNote (snd);
        remember (state.group);
        sequencer.setCell (state.group, row, snd - 1, { note, state.velocity.load(), 1, 0 });
        patternView->update();
        stepView->update();
        playNote (note, state.velocity, true);       // on l'entend, brievement
        QTimer::singleShot (150, this, [this, note] { playNote (note, 0, false); });
    });
    connect (stepView, &StepView::cellMenu, this, &MainWindow::patternMenu);
    connect (stepView, &StepView::cellsMoved, this, [this] (const QList<QPoint>& cells, int dr, int dt, bool copy)
    {
        moveCells (cells, dr, dt, copy);             // comme dans la vue Pattern (annulable)
        stepView->update();
    });
    connect (stepView, &StepView::selectionShown, this, [this] (int n)
    {
        if (n > 1)
            statusBar()->showMessage (tr ("%1 notes selected").arg (n), 2000);
        else if (n == 1)
            statusBar()->showMessage (tr ("1 note selected"), 2000);
    });
    connect (stepView, &StepView::cellDoubleClicked, this, &MainWindow::noteOptions);
    connect (stepView, &StepView::valueWheel, this, [this] (int row, int steps)
    {
        seq::Cell c = sequencer.pattern (state.group).rows[(size_t) row][(size_t) (state.sound - 1)];
        remember (state.group);
        c.velocity = std::clamp (c.velocity + steps * 4, 1, 127);
        sequencer.setCell (state.group, row, state.sound - 1, c);
        stepView->update();
    });
    connect (rollTracker, &TrackerView::cellMenu, this, &MainWindow::patternMenu);
    connect (rollTracker, &TrackerView::cellDoubleClicked, this, [this] (int row, int track, int)
    {
        if (sequencer.pattern (state.group).rows[(size_t) row][(size_t) track].empty())
            trackerWrite (row, track, lastTrackerNote >= 0 ? lastTrackerNote : defaultNote (track + 1));
        else
            noteOptions (row, track);
    });
    return panel;
}

void MainWindow::openGenerate()
{
    remember (state.group);                          // un seul pas d'annulation pour tout le tirage
    GenerateDialog dialog (sequencer, state, state.group, state.sound, this);
    connect (&dialog, &GenerateDialog::playToggle, this, [this]
    {
        transport->playing() ? transport->pause() : transport->play();
    });
    if (dialog.exec() == QDialog::Accepted)
        lastGenerate = dialog.settings();             // VARIATION (Maschine) reprend ces reglages
    else
    {
        std::lock_guard<std::mutex> lk (undoLock);   // Cancel : rien n'a change, pas de pas inutile
        if (! undoStack.empty())
            undoStack.pop_back();
    }
    showPatternState();
    patternView->update();
}

void MainWindow::importMidi (const QString& path, int sound)
{
    const midi::File file = midi::read (path);
    if (! file.error.isEmpty())
    {
        QMessageBox::warning (this, tr ("Import MIDI"), tr ("%1: %2").arg (QFileInfo (path).fileName(), file.error));
        return;
    }
    MidiImportDialog dialog (file, QFileInfo (path).fileName(), sound, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    remember (state.group);                          // annulable en une fois
    seq::Pattern p = sequencer.pattern (state.group);
    const midi::Placed placed = midi::apply (file, p, dialog.target(), dialog.firstSound(), dialog.replace());
    sequencer.setPattern (state.group, p);
    if (dialog.useTempo())
        bpmBox->setValue ((int) std::lround (file.bpm));
    showPatternState();
    patternView->update();
    statusBar()->showMessage (placed.skipped ? tr ("MIDI: %1 note(s) imported, %2 left out (beyond 256 lines or 16 sounds)")
                                                   .arg (placed.notes).arg (placed.skipped)
                                             : tr ("MIDI: %1 note(s) imported on %2 lines").arg (placed.notes).arg (placed.length), 5000);
}

// ------------------------------------------------------------------------------- Maschine : nouveautes

void MainWindow::toast (const QString& text)
{
    toastText = text;
    toastUntil = QDateTime::currentMSecsSinceEpoch() + 2000;
    statusBar()->showMessage (text, 2000);
    refreshMaschine();
    QTimer::singleShot (2050, this, [this] { refreshMaschine(); });
}

void MainWindow::addNoteAtCursor()
{
    // La note du son choisi (vue Pads) dans la case du curseur, puis ligne suivante (tracker)
    // Case deja occupee : Entree l'efface
    const int row = patternView->selectedRow(), snd = state.sound;
    remember (state.group);
    const bool full = ! sequencer.pattern (state.group).rows[(size_t) row][(size_t) (snd - 1)].empty();
    sequencer.setCell (state.group, row, snd - 1, full ? seq::Cell {} : seq::Cell { defaultNote (snd), state.velocity.load(), 1, 0 });
    patternView->setCurrentCell (row, snd - 1);
    patternView->moveCursor (1, 0);
}

void MainWindow::quickVariation()
{
    const GenerateSettings s = lastGenerate ? *lastGenerate : GenerateDialog::defaults (state.sound);
    remember (state.group);
    const seq::Pattern p = GenerateDialog::generate (sequencer.pattern (state.group), state.sound - 1, state.sound,
                                                     s, QRandomGenerator::global()->generate());
    sequencer.setPattern (state.group, p);
    int count = 0;
    for (int r = 0; r < p.length; ++r)
        count += ! p.rows[(size_t) r][(size_t) (state.sound - 1)].empty();
    showPatternState();
    patternView->update();
    toast (QString ("VARIATION  sound %1 : %2 notes").arg (state.sound.load(), 2, 10, QChar ('0')).arg (count));
}

void MainWindow::cyclePlayMode()
{
    const size_t i = (size_t) session::State::index (state.group, state.sound);
    int m = (state.playMode[i] + 1) % 3;
    if (m == session::State::Loop && state.samples[i].isEmpty())   // boucle : samples seulement
        m = session::State::OneShot;
    state.playMode[i] = m;
    syncSamples();
    refreshAll();
    toast (QString ("Sound %1 : %2").arg (state.sound.load(), 2, 10, QChar ('0')).arg (QStringList { "ONE-SHOT", "GATE", "LOOP" }[m]));
}

// ------------------------------------------------------------------------------- vues, clavier

int MainWindow::currentView() const
{
    auto shown = [] (QWidget* w) { return w && ! w->visibleRegion().isEmpty(); };
    return shown (songView) ? 7 : shown (sceneView) ? 6 : shown (tracker) ? 5 : shown (pianoRoll) || shown (stepView) || shown (rollTracker) ? 2
         : shown (mixer) ? 0 : shown (patternView) ? 1 : shown (keyboardView) ? 3 : 1;
}

void MainWindow::toggleView (int index, const QString& name)
{
    // 1er appui : la vue ; 2e appui : retour a celle d'avant
    const int now = currentView();
    if (now == index && viewBefore >= 0 && viewBefore != index)
    {
        const int back = viewBefore;
        viewBefore = -1;
        showLeftTab (back);
        toast (QStringList { "MIXER", "PATTERN", "PIANO ROLL", "KEYBOARD", "SOUND", "TRACKER", "SCENES", "SONG" }.value (back));
        return;
    }
    if (now != index)
        viewBefore = now;
    showLeftTab (index);
    toast (name);
}

void MainWindow::releaseAllKeys()
{
    for (auto it = keyNotes.begin(); it != keyNotes.end(); ++it)
    {
        playKey (it.value(), 0, false);
        piano->setHeld (it.value(), false);
    }
    keyNotes.clear();
    const auto left = liveNotes;                     // et ce qui resterait (souris, Maschine...)
    for (auto it = left.begin(); it != left.end(); ++it)
        engine->noteOff (it->sound, it->sent, it->group);
    liveNotes.clear();
    keyChords.clear();
}

void MainWindow::playKey (int key, int velocity, bool down, bool chords)
{
    // Une touche joue une note, ou un accord (Harmonizer, Chord Set) ; on garde les notes
    // jouees pour les relacher toutes, meme si les reglages changent entre-temps
    if (down)
    {
        if (keyChords.contains (key))                 // la meme touche encore : son accord d'avant s'arrete
            playKey (key, 0, false);
        const QList<int> notes = chords ? music::chordFor (key, keySettings) : QList<int> { key };
        keyChords[key] = notes;
        for (int i = 0; i < notes.size(); ++i)
        {
            chordMerge = i > 0;                       // un accord s'enregistre dans une seule case
            playNote (notes[i], velocity, true);
        }
        chordMerge = false;
        return;
    }
    const QList<int> notes = keyChords.contains (key) ? keyChords.take (key) : QList<int> { key };
    for (int n : notes)
        playNote (n, 0, false);
    liveCheck();
}

void MainWindow::liveCheck()
{
    // Plus aucune touche, aucun pad, aucune note du Piano Roll tenus : le moteur relache toute
    // note jouee en direct encore tenue (filet de securite, comme le pilote : une fin de note
    // perdue ne laisse plus une note de 30 s). Les notes du pattern ont leur duree : pas touchees.
    for (int p = 1; p <= 16; ++p)
        if (padHeld[(size_t) p])
            return;
    if (! keyChords.isEmpty() || ! keyNotes.isEmpty() || rollKeyHeld)
        return;
    liveNotes.clear();
    engine->offLive();
}

QWidget* MainWindow::buildKeyboardTab()
{
    // Onglet Keyboard : reglages (octave, gamme, accords, clavier de l'ordinateur) + grand clavier
    QSettings settings;
    keySettings.root = settings.value ("keys/root", 0).toInt();
    music::loadCustom();                             // gammes et accords personnalises
    keySettings.scale = std::clamp (settings.value ("keys/scale", 0).toInt(), -(int) music::customScales().size(),
                                    (int) music::scales().size() - 1);
    music::loadUserChords();                         // accords User 1-10 (fichier du pilote Maschine)
    keySettings.chordMode = std::clamp (settings.value ("keys/chordMode", 0).toInt(), 0, 4);
    keySettings.userBank = std::clamp (settings.value ("keys/userBank", 0).toInt(), 0, music::kUserBanks - 1);
    keySettings.customChord = std::clamp (settings.value ("keys/customChord", 0).toInt(), 0, std::max (0, (int) music::customChords().size() - 1));
    if (keySettings.chordMode == 3 && music::customChords().empty())
        keySettings.chordMode = 0;
    keySettings.harmony = std::clamp (settings.value ("keys/harmony", 2).toInt(), 0, (int) music::harmonies().size() - 1);
    keySettings.chordSet = std::clamp (settings.value ("keys/chordSet", 0).toInt(), 0, (int) music::chordSets().size() - 1);

    auto* panel = new QFrame;
    panel->setObjectName ("panel");
    auto* col = new QVBoxLayout (panel);
    auto* row = new QHBoxLayout;
    auto button = [] (const QString& text, const QString& tip)
    {
        auto* b = new QPushButton (text);
        b->setFocusPolicy (Qt::NoFocus);
        b->setToolTip (tip);
        return b;
    };
    // Octave et velocite : chiffres et petites fleches, comme le BPM (touches W / X et C / V)
    auto spinBox = [] (int lo, int hi, int value, const QString& prefix, const QString& tip)
    {
        auto* b = new QSpinBox;
        b->setRange (lo, hi);
        b->setValue (value);
        b->setPrefix (prefix);
        b->setFocusPolicy (Qt::ClickFocus);
        b->setToolTip (tip);
        b->setKeyboardTracking (false);
        return b;
    };
    octaveSpin = spinBox (0, 7, state.octave, tr ("Octave "), tr ("Octave of the keyboard (computer keys W / X, QWERTY: Z / X)"));
    velocitySpin = spinBox (1, 127, state.velocity, tr ("Velocity "), tr ("Velocity of the notes played (computer keys C / V)"));
    connect (octaveSpin, &QSpinBox::valueChanged, this, [this] (int v) { if (v != state.octave) setOctave (v); keyboardView->update(); });
    connect (velocitySpin, &QSpinBox::valueChanged, this, [this] (int v) { state.velocity = v; refreshAll(); });
    for (auto* b : { octaveSpin, velocitySpin })
        connect (b, &QSpinBox::editingFinished, b, [b] { b->clearFocus(); });
    auto keyHint = [] (const QString& t)
    {
        auto* l = new QLabel (t);
        l->setObjectName ("hint");
        return l;
    };
    auto combo = [] (const QStringList& items, int current)
    {
        auto* c = new QComboBox;
        c->addItems (items);
        c->setCurrentIndex (current);
        c->setFocusPolicy (Qt::ClickFocus);
        return c;
    };
    auto* rootBox = combo ({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, keySettings.root);
    rootCombo = rootBox;
    rootBox->setToolTip (tr ("Root note of the scale"));
    // Gamme et accords : menus en arborescence (categorie, puis gamme), personnalises compris
    auto* scaleButton = new QPushButton;
    scaleButton->setFocusPolicy (Qt::NoFocus);
    scaleButton->setToolTip (tr ("Scale: categories of the Maschine driver, then Custom (your own scales)"));
    auto* scaleMenu = new QMenu (scaleButton);
    scaleButton->setMenu (scaleMenu);
    auto* chordButton = new QPushButton;
    chordMenuButton = chordButton;
    chordButton->setFocusPolicy (Qt::NoFocus);
    chordButton->setToolTip (tr ("Harmonizer: each key plays a chord built on the scale. Chord Set: the 12 keys of an octave "
                                 "play 12 chords. Custom: your own chords"));
    auto* chordMenu = new QMenu (chordButton);
    chordButton->setMenu (chordMenu);
    auto save = [this, scaleButton, chordButton]
    {
        QSettings s;
        s.setValue ("keys/root", keySettings.root);
        s.setValue ("keys/scale", keySettings.scale);
        s.setValue ("keys/chordMode", keySettings.chordMode);
        s.setValue ("keys/harmony", keySettings.harmony);
        s.setValue ("keys/chordSet", keySettings.chordSet);
        s.setValue ("keys/customChord", keySettings.customChord);
        s.setValue ("keys/userBank", keySettings.userBank);
        if (keySettings.chordMode != 0)
            s.setValue ("keys/lastChordMode", keySettings.chordMode);   // repris par le mode CHORDS
        scaleButton->setText (music::scaleLabel (keySettings.scale));
        chordButton->setText (music::chordLabel (keySettings));
        keyboardView->update();
        refreshAll();                                // pads en mode KEYBOARD : leurs notes
    };
    saveKeySettings = save;
    connect (scaleMenu, &QMenu::aboutToShow, this, [this, scaleMenu, save]
    {
        music::fillScaleMenu (scaleMenu, keySettings.scale, [this, save] (int sc) { keySettings.scale = sc; save(); }, this);
    });
    connect (chordMenu, &QMenu::aboutToShow, this, [this, chordMenu, save]
    {
        music::fillChordMenu (chordMenu, keySettings, [this, save] (int mode, int idx)
        {
            keySettings.chordMode = mode;
            if (mode == 1)
                keySettings.harmony = idx;
            else if (mode == 2)
                keySettings.chordSet = idx;
            else if (mode == 3)
                keySettings.customChord = idx;
            else if (mode == 4)
                keySettings.userBank = idx;
            save();
        }, this);
    });
    scaleButton->setText (music::scaleLabel (keySettings.scale));
    chordButton->setText (music::chordLabel (keySettings));
    connect (rootBox, &QComboBox::currentIndexChanged, this, [this, save] (int i) { keySettings.root = i; save(); });
    // Clavier de l'ordinateur : le meme reglage que le panneau PC Keyboard
    auto* pcKeys = button (QStringLiteral ("\u2328 ") + tr ("PC KEYS"), tr ("Play with the computer keyboard (on / off)"));
    pcKeys->setCheckable (true);
    pcKeys->setChecked (keysButton->isChecked());
    pcKeys->setObjectName ("keys");
    connect (pcKeys, &QPushButton::toggled, keysButton, &QPushButton::setChecked);
    connect (keysButton, &QPushButton::toggled, pcKeys, &QPushButton::setChecked);
    auto* layout2 = combo ({}, 0);
    for (int i = 0; i < layoutBox->count(); ++i)
        layout2->addItem (layoutBox->itemText (i));
    layout2->setCurrentIndex (layoutBox->currentIndex());
    connect (layout2, &QComboBox::currentIndexChanged, layoutBox, &QComboBox::setCurrentIndex);
    connect (layoutBox, &QComboBox::currentIndexChanged, layout2, &QComboBox::setCurrentIndex);

    // Deux lignes (le panneau reste etroit possible) : octave et clavier de l'ordinateur,
    // puis gamme et accords
    for (auto* b : { scaleButton, chordButton })
    {
        b->setSizePolicy (QSizePolicy::Ignored, QSizePolicy::Fixed);   // panneau etroit possible
        b->setMinimumWidth (90);
        b->setStyleSheet ("text-align: left; padding-left: 10px;");
    }
    row->addWidget (octaveSpin);
    row->addWidget (keyHint ("W / X"));
    row->addSpacing (10);
    row->addWidget (velocitySpin);
    row->addWidget (keyHint ("C / V"));
    row->addStretch (1);
    row->addWidget (pcKeys);
    row->addWidget (layout2);
    col->addLayout (row);
    auto* row2 = new QHBoxLayout;
    row2->addWidget (rootBox);
    // Color : les notes de la gamme en couleur ; sinon touches noires et blanches classiques
    auto* colorButton = new QPushButton (tr ("Color"));
    colorButton->setCheckable (true);
    colorButton->setFocusPolicy (Qt::NoFocus);
    colorButton->setObjectName ("keys");
    colorButton->setToolTip (tr ("On: the notes of the scale in color. Off: classic black and white keys (scale shown by dots)"));
    colorButton->setChecked (QSettings().value ("keys/color", true).toBool());
    connect (colorButton, &QPushButton::toggled, this, [this] (bool on)
    {
        QSettings().setValue ("keys/color", on);
        keyboardView->setColorScale (on);
    });
    row2->addWidget (scaleButton, 3);
    row2->addSpacing (12);
    row2->addWidget (chordButton, 2);
    row2->addWidget (colorButton);
    col->addLayout (row2);
    keyboardView = new KeyboardView (state, keySettings);
    keyboardView->setColorScale (QSettings().value ("keys/color", true).toBool());
    connect (keyboardView, &KeyboardView::keyPressed, this, [this] (int n) { playKey (n, state.velocity, true); });
    connect (keyboardView, &KeyboardView::keyReleased, this, [this] (int n) { playKey (n, 0, false); });
    col->addWidget (keyboardView, 1);
    QTimer::singleShot (0, this, [this] { updateKeyLabels(); });
    return panel;
}

// ------------------------------------------------------------------------------- pattern : souris, Maschine

void MainWindow::switchBank()
{
    selectGroup (state.group % 8 + (state.group >= 8 ? 0 : 8));
    toast (state.group >= 8 ? "GROUPS A2 - H2" : "GROUPS A - H");
}

void MainWindow::moveCells (const QList<QPoint>& cells, int dr, int dt, bool copy)
{
    const seq::Pattern p = sequencer.pattern (state.group);
    for (const QPoint& c : cells)                    // tout reste dans le pattern ?
        if (c.y() + dr < 0 || c.y() + dr >= p.length || c.x() + dt < 0 || c.x() + dt >= seq::kTracks)
            return;
    remember (state.group);
    if (! copy)
        for (const QPoint& c : cells)
            sequencer.setCell (state.group, c.y(), c.x(), {});
    QList<QPoint> moved;
    for (const QPoint& c : cells)
    {
        sequencer.setCell (state.group, c.y() + dr, c.x() + dt, p.rows[(size_t) c.y()][(size_t) c.x()]);
        moved << QPoint (c.x() + dt, c.y() + dr);
    }
    patternView->setSelection (moved);
}

void MainWindow::patternMenu (int row, int track, const QPoint& at)
{
    QMenu menu (this);
    const bool full = ! sequencer.pattern (state.group).rows[(size_t) row][(size_t) track].empty();
    auto* copy = menu.addAction (tr ("Copy"));
    auto* cut = menu.addAction (tr ("Cut"));
    auto* pasteAction = menu.addAction (tr ("Paste here"));
    pasteAction->setEnabled (! clipboard.empty());
    auto* erase = menu.addAction (tr ("Delete"));
    menu.addSeparator();
    // Zone tracee a la souris : y mettre le son choisi (celui de la vue Pads)
    const QRect f = patternView->frame();
    QList<QAction*> fills;
    const QString soundText = QString ("%1 %2").arg (state.sound.load(), 2, 10, QChar ('0')).arg (padName (state.group, state.sound));
    for (const int every : { 1, 2, 4 })
    {
        auto* a = menu.addAction (f.isNull() ? tr ("Fill frame with %1 (draw a frame first)").arg (soundText)
                                  : every == 1 ? tr ("Fill frame with %1, every line").arg (soundText)
                                               : tr ("Fill frame with %1, every %2 lines").arg (soundText).arg (every));
        a->setEnabled (! f.isNull());
        a->setData (every);
        fills << a;
        if (f.isNull())
            break;
    }
    menu.addSeparator();
    auto* options = menu.addAction (tr ("Note Options..."));
    options->setEnabled (full);
    QAction* chosen = menu.exec (at);
    if (! chosen)
        return;
    if (chosen == cut || chosen == copy)
        copySelection (chosen == cut);
    else if (chosen == pasteAction)
        paste();
    else if (chosen == erase)
    {
        QList<QPoint> sel = patternView->selection();
        if (sel.isEmpty())
            sel = { QPoint (track, row) };
        remember (state.group);
        for (const QPoint& c : sel)
            sequencer.setCell (state.group, c.y(), c.x(), {});
        patternView->clearSelection();
    }
    else if (fills.contains (chosen))
        fillFrame (chosen->data().toInt());
    else if (chosen == options)
        noteOptions (row, track);
}

void MainWindow::fillFrame (int every)
{
    // Lignes de la zone tracee, dans la piste du son choisi (une piste = un son)
    const QRect f = patternView->frame();
    if (f.isNull())
        return;
    const int snd = state.sound, length = sequencer.patternLength (state.group);
    remember (state.group);
    QList<QPoint> added;
    for (int r = f.top(); r <= f.bottom() && r < length; r += every)
    {
        sequencer.setCell (state.group, r, snd - 1, { defaultNote (snd), state.velocity.load(), 1, 0 });
        added << QPoint (snd - 1, r);
    }
    patternView->setSelection (added);
    toast (tr ("%1 note(s) of sound %2").arg (added.size()).arg (snd, 2, 10, QChar ('0')));
}

void MainWindow::noteOptions (int row, int track)
{
    const seq::Cell c = sequencer.pattern (state.group).rows[(size_t) row][(size_t) track];
    const int length = sequencer.patternLength (state.group);
    NoteDialog dialog (c.empty() ? seq::Cell { defaultNote (track + 1), state.velocity.load(), 1, 0 } : c,
                       row, track, length - row, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    remember (state.group);
    sequencer.setCell (state.group, row, track, dialog.cell());
    patternView->update();
}

void MainWindow::shiftPad (int pad)
{
    // Shift + pad, comme la serigraphie de la Maschine (et le pilote) :
    // 1/3 Undo, 2/4 Redo, 5 Quantize, 6 Quantize 50 %, 7/8 Nudge, 9 Clear, 11 Copy, 12 Paste,
    // 13/14 demi-ton, 15/16 octave. Cible : la selection, sinon la case du curseur ;
    // 2e appui rapide : toute la piste du curseur.
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const bool twice = pad == lastShiftPad && now - lastShiftPadMs < 500;
    lastShiftPad = pad;
    lastShiftPadMs = now;
    if (pad == 1 || pad == 3 || pad == 2 || pad == 4)
    {
        undo (pad == 2 || pad == 4);
        toast (pad == 2 || pad == 4 ? "REDO" : "UNDO");
        return;
    }
    if (pad == 10)
    {
        toast ("CLEAR AUTO : no automation yet");
        return;
    }
    if (padKeyboard && pad >= 13)
    {
        // KEYBOARD / CHORDS (comme le pilote) : 13 / 14 = tonalite -/+ 1 demi-ton, 15 / 16 = octave -/+.
        // Les notes et accords des pads sont recalcules (C -> C# : l'accord de base devient C#...).
        if (pad <= 14)
        {
            const int root = ((keySettings.root + (pad == 13 ? -1 : 1)) % 12 + 12) % 12;
            if (rootCombo)
                rootCombo->setCurrentIndex (root);   // met a jour l'onglet Keyboard, enregistre, recalcule
            else
            {
                keySettings.root = root;
                if (saveKeySettings)
                    saveKeySettings();
            }
            static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
            toast (QString ("ROOT %1").arg (names[root]));
        }
        else
        {
            const int octave = qBound (0, state.octave + (pad == 15 ? -1 : 1), 7);
            if (octaveSpin)
                octaveSpin->setValue (octave);
            else
                setOctave (octave);
            refreshAll();
            toast (QString ("OCTAVE %1").arg (octave));
        }
        refreshMaschine();
        return;
    }
    showLeftTab (1);
    const seq::Pattern p = sequencer.pattern (state.group);
    QList<QPoint> targets = patternView->selection();
    if (twice || targets.isEmpty())
    {
        targets.clear();
        const int t = patternView->selectedTrack();
        if (twice)
        {
            for (int r = 0; r < p.length; ++r)
                if (! p.rows[(size_t) r][(size_t) t].empty())
                    targets << QPoint (t, r);
        }
        else
            targets << QPoint (t, patternView->selectedRow());
    }
    const QString scope = twice ? " (track)" : "";
    switch (pad)
    {
        case 5: case 6:                               // Quantize : timing fin a 0 (ou a moitie)
        {
            remember (state.group);
            for (const QPoint& c : targets)
            {
                seq::Cell cell = p.rows[(size_t) c.y()][(size_t) c.x()];
                if (cell.empty())
                    continue;
                cell.offset = pad == 5 ? 0 : cell.offset / 2;
                sequencer.setCell (state.group, c.y(), c.x(), cell);
            }
            toast ((pad == 5 ? "QUANTIZE" : "QUANTIZE 50%") + scope);
            break;
        }
        case 7: case 8:
            moveCells (targets, pad == 7 ? -1 : 1, 0, false);
            toast ((pad == 7 ? "NUDGE <" : "NUDGE >") + scope);
            break;
        case 9:
            remember (state.group);
            for (const QPoint& c : targets)
                sequencer.setCell (state.group, c.y(), c.x(), {});
            patternView->clearSelection();
            toast ("CLEAR" + scope);
            break;
        case 11:
            patternView->setSelection (targets);
            copySelection (false);
            toast ("COPY" + scope);
            break;
        case 12:
            paste();
            toast ("PASTE");
            break;
        case 13: case 14: case 15: case 16:           // demi-ton / octave, accord compris
        {
            const int step = pad == 13 ? -1 : pad == 14 ? 1 : pad == 15 ? -12 : 12;
            remember (state.group);
            for (const QPoint& c : targets)
            {
                seq::Cell cell = p.rows[(size_t) c.y()][(size_t) c.x()];
                if (cell.empty())
                    continue;
                QList<int> notes = PianoRollView::notesOf (cell);
                bool fits = true;
                for (int& n : notes)
                    fits &= (n += step) >= 0 && n <= 127;
                if (fits)
                    sequencer.setCell (state.group, c.y(), c.x(), PianoRollView::withNotes (cell, notes));
            }
            toast (QString ("NOTE %1%2").arg (std::abs (step) == 12 ? (step > 0 ? "OCT +1" : "OCT -1")
                                                                   : (step > 0 ? "+1" : "-1")).arg (scope));
            break;
        }
        default:
            break;
    }
    patternView->update();
}

// ------------------------------------------------------------------------------- onglet Sound

QWidget* MainWindow::buildSoundTab()
{
    auto* panel = new QFrame;
    panel->setObjectName ("panel");
    auto* col = new QVBoxLayout (panel);
    auto button = [] (const QString& text, const QString& tip)
    {
        auto* b = new QPushButton (text);
        b->setFocusPolicy (Qt::NoFocus);
        b->setToolTip (tip);
        return b;
    };
    // Titre et ecoute
    auto* head = new QHBoxLayout;
    soundUi.title = new QLabel;
    soundUi.title->setObjectName ("section");
    auto* preview = button (tr ("Hold to preview"), tr ("Plays the chosen sound while the button is held"));
    connect (preview, &QPushButton::pressed, this, [this] { playPad (state.sound, 110, true); pads->flash (state.sound, true); });
    connect (preview, &QPushButton::released, this, [this] { playPad (state.sound, 0, false); pads->flash (state.sound, false); });
    head->addWidget (soundUi.title, 1);
    head->addWidget (preview);
    col->addLayout (head);

    auto* form = new QGridLayout;
    form->setHorizontalSpacing (10);
    int row = 0;
    auto label = [] (const QString& text)
    {
        auto* l = new QLabel (text);
        l->setObjectName ("hint");
        return l;
    };
    // Source : synthe (comme le pilote) ou sample
    soundUi.synth = new QComboBox;
    soundUi.synth->addItems ({ tr ("Default"), "Kick", "Snare", "Closed HH", "Open HH", "Tone", tr ("None (empty)") });
    soundUi.synth->setFocusPolicy (Qt::ClickFocus);
    soundUi.synth->setToolTip (tr ("Built-in sound (used when there is no sample)"));
    connect (soundUi.synth, &QComboBox::currentIndexChanged, this, [this] (int idx)
    {
        const size_t i = (size_t) state.index (state.group, state.sound);
        state.synth[i] = idx == 0 ? -1 : idx == 6 ? 0 : idx;   // Default / Kick..Tone / None
        syncSamples();
        refreshAll();
    });
    soundUi.sample = new QLabel;
    soundUi.sample->setSizePolicy (QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto* load = button (tr ("Load..."), tr ("Load a sample (WAV, AIFF, FLAC, Ogg, MP3...)"));
    soundUi.removeSample = button (tr ("Remove"), tr ("Remove the sample (back to the built-in sound)"));
    connect (load, &QPushButton::clicked, this, [this] { loadSampleDialog (state.sound); });
    connect (soundUi.removeSample, &QPushButton::clicked, this, [this] { setPadSample (state.group, state.sound, {}); });
    auto* source = new QHBoxLayout;
    source->addWidget (soundUi.synth);
    source->addWidget (soundUi.sample, 1);
    source->addWidget (load);
    source->addWidget (soundUi.removeSample);
    form->addWidget (label (tr ("Source")), row, 0);
    form->addLayout (source, row++, 1, 1, 2);

    // Lecture : One-shot / Gate / Loop, boucle, release
    soundUi.mode = new QComboBox;
    soundUi.mode->addItems ({ tr ("One-shot"), tr ("Gate"), tr ("Loop") });
    soundUi.mode->setFocusPolicy (Qt::ClickFocus);
    connect (soundUi.mode, &QComboBox::currentIndexChanged, this, [this] (int m)
    {
        const size_t i = (size_t) state.index (state.group, state.sound);
        if (m == session::State::Loop && state.samples[i].isEmpty())
            m = session::State::Gate;                // boucle : samples seulement
        state.playMode[i] = m;
        syncSamples();
        refreshAll();
    });
    soundUi.editLoop = button (tr ("Edit Loop..."), tr ("Loop start and end, on the waveform"));
    reverseBox = new QCheckBox (tr ("Reverse"));
    reverseBox->setFocusPolicy (Qt::NoFocus);
    reverseBox->setToolTip (tr ("Play the sample backwards"));
    connect (reverseBox, &QCheckBox::toggled, this, [this] (bool on)
    {
        state.soundReverse[(size_t) state.index (state.group, state.sound)] = on;
        syncSamples();
    });
    connect (soundUi.editLoop, &QPushButton::clicked, this, [this] { editLoop (state.sound); });
    soundUi.release = new QSpinBox;
    soundUi.release->setRange (5, 5000);
    soundUi.release->setSuffix (" ms");
    soundUi.release->setFocusPolicy (Qt::ClickFocus);
    soundUi.release->setToolTip (tr ("Fade-out after the release (Gate and Loop)"));
    connect (soundUi.release, &QSpinBox::valueChanged, this, [this] (int ms)
    {
        state.releaseMs[(size_t) state.index (state.group, state.sound)] = ms;
    });
    connect (soundUi.release, &QSpinBox::editingFinished, soundUi.release, [this] { soundUi.release->clearFocus(); });
    auto* play = new QHBoxLayout;
    play->addWidget (soundUi.mode);
    play->addWidget (soundUi.editLoop);
    play->addWidget (reverseBox);
    play->addSpacing (10);
    play->addWidget (label (tr ("Release")));
    play->addWidget (soundUi.release);
    play->addSpacing (10);
    // Polyphonie (comme Polyphony de Maschine) : le meme son joue plusieurs fois en meme temps
    polyBox = new QSpinBox;
    polyBox->setRange (1, 16);
    polyBox->setFocusPolicy (Qt::ClickFocus);
    polyBox->setToolTip (tr ("Polyphony: notes of this sound at the same time (1 = mono: a new note stops the previous one)"));
    connect (polyBox, &QSpinBox::valueChanged, this, [this] (int v)
    {
        state.soundPoly[(size_t) state.index (state.group, state.sound)] = v;
    });
    connect (polyBox, &QSpinBox::editingFinished, polyBox, [this] { polyBox->clearFocus(); });
    play->addWidget (label (tr ("Polyphony")));
    play->addWidget (polyBox);
    play->addStretch();
    form->addWidget (label (tr ("Play")), row, 0);
    form->addLayout (play, row++, 1, 1, 2);

    // Volume, Pan, Pitch, Cutoff (encodeurs 1 a 4 de la Maschine)
    const QStringList names { tr ("Volume"), tr ("Pan"), tr ("Pitch"), tr ("Cutoff") };
    for (int k = 0; k < 4; ++k)
    {
        auto* slider = new QSlider (Qt::Horizontal);
        slider->setRange (k == 2 ? 40 : 0, k == 2 ? 88 : 127);   // pitch : +/- 24 demi-tons
        slider->setFocusPolicy (Qt::NoFocus);
        auto* value = new QLabel;
        value->setMinimumWidth (56);
        soundUi.sliders[(size_t) k] = slider;
        soundUi.values[(size_t) k] = value;
        form->addWidget (label (names[k] + QString ("  (%1)").arg (k + 1)), row, 0);
        form->addWidget (slider, row, 1);
        form->addWidget (value, row++, 2);
        connect (slider, &QSlider::valueChanged, this, [this, k] (int v)
        {
            const size_t i = (size_t) state.index (state.group, state.sound);
            auto& target = k == 0 ? state.soundVolume[i] : k == 1 ? state.soundPan[i] : k == 2 ? state.soundPitch[i] : state.soundCutoff[i];
            target = v;
            refreshSoundPanel();
        });
        // double-clic sur la valeur : retour au reglage d'origine
    }
    soundUi.sliders[3]->setEnabled (false);
    soundUi.sliders[3]->setToolTip (tr ("Kept in the project; the engine filter comes later"));
    form->setColumnStretch (1, 1);
    col->addLayout (form);
    auto* reset = button (tr ("Reset Volume / Pan / Pitch"), tr ("Back to 100 / center / original pitch"));
    connect (reset, &QPushButton::clicked, this, [this]
    {
        const size_t i = (size_t) state.index (state.group, state.sound);
        state.soundVolume[i] = 100;
        state.soundPan[i] = 64;
        state.soundPitch[i] = 64;
        refreshSoundPanel();
    });
    auto* foot = new QHBoxLayout;
    auto* hint = label (tr ("Maschine: PLUG-IN = this tab, knobs 1-4 = Volume, Pan, Pitch, Cutoff"));
    hint->setSizePolicy (QSizePolicy::Ignored, QSizePolicy::Preferred);
    foot->addWidget (hint, 1);
    foot->addWidget (reset);
    col->addLayout (foot);
    col->addStretch();
    return panel;
}

void MainWindow::refreshSoundPanel()
{
    if (! soundUi.title)
        return;
    const int g = state.group, snd = state.sound;
    const size_t i = (size_t) state.index (g, snd);
    soundUi.title->setText (tr ("SOUND %1 — %2   (group %3)").arg (snd, 2, 10, QChar ('0')).arg (padName (g, snd), groupTitle (state, g)));
    {
        const QSignalBlocker b (soundUi.synth);
        const int syn = state.synth[i];
        soundUi.synth->setCurrentIndex (syn < 0 ? 0 : syn == 0 ? 6 : syn);
    }
    const QString path = state.samples[i];
    soundUi.sample->setText (path.isEmpty() ? tr ("no sample") : QFileInfo (path).fileName());
    soundUi.sample->setToolTip (path);
    soundUi.removeSample->setEnabled (! path.isEmpty());
    {
        const QSignalBlocker b (soundUi.mode);
        soundUi.mode->setCurrentIndex (state.playMode[i]);
    }
    soundUi.editLoop->setEnabled (! path.isEmpty());
    {
        const QSignalBlocker b (reverseBox);
        reverseBox->setChecked (state.soundReverse[i]);
        const QSignalBlocker bp (polyBox);
        polyBox->setValue (state.poly (state.group, state.sound));
        reverseBox->setEnabled (! path.isEmpty());   // a l'envers : samples seulement
    }
    {
        const QSignalBlocker b (soundUi.release);
        soundUi.release->setValue (state.releaseMs[i]);
    }
    const std::array<int, 4> vals { state.soundVolume[i].load(), state.soundPan[i].load(),
                                    state.soundPitch[i].load(), state.soundCutoff[i].load() };
    for (int k = 0; k < 4; ++k)
    {
        const QSignalBlocker b (soundUi.sliders[(size_t) k]);
        soundUi.sliders[(size_t) k]->setValue (vals[(size_t) k]);
    }
    const int pan = vals[1] - 64, semis = std::clamp (vals[2] - 64, -24, 24);
    soundUi.values[0]->setText (QString::number (vals[0]));
    soundUi.values[1]->setText (pan == 0 ? "C" : pan < 0 ? QString ("L%1").arg (-pan) : QString ("R%1").arg (pan));
    soundUi.values[2]->setText (semis == 0 ? "0 st" : QString ("%1%2 st").arg (semis > 0 ? "+" : "").arg (semis));
    soundUi.values[3]->setText (QString::number (vals[3]));
}

void MainWindow::soundKnob (int index, int delta)
{
    // Encodeurs de la Maschine, comme le pilote : 1-4 reglages du son choisi,
    // 5 ligne du curseur, 6 note de la case, 7 velocite de la case, 8 longueur du pattern
    // (Shift + 5 : gamme, Shift + 6 : tonique, pour le clavier)
    index -= 1;                                      // la bibliotheque numerote les encodeurs de 1 a 8
    if (arrangerMode && index >= 0 && index < 4)     // vue Song : entree, scene, repetitions, longueur
    {
        arrangerKnob (index, delta);
        return;
    }
    const int g = state.group, snd = state.sound;
    const size_t i = (size_t) state.index (g, snd);
    if (index < 0 || index > 7)
        return;
    if (index < 4)
    {
        static const char* names[] { "VOLUME", "PAN", "PITCH", "CUTOFF" };
        auto& target = index == 0 ? state.soundVolume[i] : index == 1 ? state.soundPan[i] : index == 2 ? state.soundPitch[i] : state.soundCutoff[i];
        target = std::clamp (target + delta, index == 2 ? 40 : 0, index == 2 ? 88 : 127);
        refreshSoundPanel();
        toast (QString ("%1  sound %2 : %3").arg (names[index]).arg (snd, 2, 10, QChar ('0')).arg (soundUi.values[(size_t) index]->text()));
        return;
    }
    if (maschineShift && (index == 4 || index == 5))
    {
        if (index == 4)
            keySettings.scale = std::clamp (std::max (0, keySettings.scale) + delta, 0, (int) music::scales().size() - 1);
        else
            keySettings.root = ((keySettings.root + delta) % 12 + 12) % 12;
        keyboardView->update();
        static const char* roots[12] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        toast (QString ("%1 %2").arg (roots[keySettings.root]).arg (music::scaleLabel (keySettings.scale)));
        return;
    }
    if (index == 4)
    {
        patternView->moveCursor (delta, 0);
        return;
    }
    if (index == 7)
    {
        lengthBox->setValue (std::clamp (lengthBox->value() + delta, 1, seq::kMaxRows));
        toast (QString ("LENGTH %1").arg (lengthBox->value()));
        return;
    }
    // 6 : note, 7 : velocite de la case du curseur (accord compris)
    const int row = patternView->selectedRow(), track = patternView->selectedTrack();
    seq::Cell c = sequencer.pattern (g).rows[(size_t) row][(size_t) track];
    if (c.empty())
        return;
    remember (g);
    if (index == 5)
    {
        QList<int> notes = PianoRollView::notesOf (c);
        for (int& n : notes)
            n = std::clamp (n + delta, 0, 127);
        c = PianoRollView::withNotes (c, notes);
        toast (QString ("NOTE %1").arg (session::noteName (c.note)));
    }
    else
    {
        c.velocity = std::clamp (c.velocity + delta, 1, 127);
        toast (QString ("VELOCITY %1").arg (c.velocity));
    }
    sequencer.setCell (g, row, track, c);
    patternView->update();
}

// ------------------------------------------------------------------------------- pads : mode KEYBOARD

int MainWindow::padNote (int pad) const
{
    // Pad 1 = la tonique de l'octave choisie ; gamme chromatique : demi-ton par demi-ton,
    // sinon les notes de la gamme (comme le mode KEYBOARD du pilote)
    const int base = 12 * (state.octave + 1) + keySettings.root;
    const std::vector<int>& steps = music::scaleSteps (keySettings.scale);
    if (keySettings.scale == 0 || steps.empty())
        return std::clamp (base + pad - 1, 0, 127);
    const int n = (int) steps.size(), k = pad - 1;
    return std::clamp (base + steps[(size_t) (k % n)] + 12 * (k / n), 0, 127);
}

void MainWindow::refreshPadModeButtons()
{
    // Chaque bouton propose le mode suivant ; meme largeur pour les deux
    if (! padModeButton || ! chordsModeButton || ! padsOnlyButton)
        return;
    padsOnlyButton->setChecked (! padKeyboard);
    padModeButton->setChecked (padKeyboard && ! padChords);
    chordsModeButton->setChecked (padChords);
}

void MainWindow::setPadChords (bool on)
{
    if (on && keySettings.chordMode == 0)
    {
        // Pas d'accord choisi : le dernier reglage d'accords, sinon Harmonizer « Oct +1 »
        QSettings s;
        const int last = s.value ("keys/lastChordMode", 0).toInt();
        if (last >= 1 && last <= 4 && ! (last == 3 && music::customChords().empty()))
            keySettings.chordMode = last;
        else
        {
            keySettings.chordMode = 1;
            for (int i = 0; i < (int) music::harmonies().size(); ++i)
                if (QString (music::harmonies()[(size_t) i].name) == "Oct +1")
                    keySettings.harmony = i;
        }
        if (saveKeySettings)
            saveKeySettings();
    }
    padChords = on;
    padKeyboard = on;
    if (on)
        useMelodicSound();
    refreshPadModeButtons();
    refreshAll();
    refreshMaschine();
    toast (on ? QString ("CHORDS : %1").arg (music::chordLabel (keySettings)) : QString ("PAD MODE"));
}

int MainWindow::padChordKey (int pad) const
{
    // Chord Set : les 12 accords sur les pads 1-12 (touches C a B de l'octave) ;
    // Harmonizer et Custom : un accord sur chaque note de la gamme (comme les notes du mode KEYBOARD)
    if (keySettings.chordMode == 2 || keySettings.chordMode == 4)   // Chord Set et User : pads 1-12
        return pad <= 12 ? std::clamp (12 * (state.octave + 1) + keySettings.root + pad - 1, 1, 127) : -1;
    return padNote (pad);
}

namespace {
// Nom d'accord ecrit en C (« Am/E ») transpose de `shift` demi-tons (« Bm/F# » pour D)
QString transposeChordName (const QString& name, int shift)
{
    static const QStringList sharps { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const QStringList flats { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    auto note = [&] (const QString& s, int& used)
    {
        static const QString letters = "C D EF G A B";
        used = 0;
        if (s.isEmpty() || letters.indexOf (s[0]) < 0 || s[0] == ' ')
            return QString();
        int pc = (int) letters.indexOf (s[0]);
        used = 1;
        bool flat = false;
        if (s.size() > 1 && (s[1] == '#' || s[1] == 'b'))
        {
            flat = s[1] == 'b';
            pc += flat ? -1 : 1;
            used = 2;
        }
        pc = ((pc + shift) % 12 + 12) % 12;
        return (flat ? flats : sharps)[pc];
    };
    if (shift % 12 == 0)
        return name;
    int used = 0;
    const QString root = note (name, used);
    if (root.isEmpty())
        return name;
    QString out = root + name.mid (used);
    const int slash = (int) out.lastIndexOf ('/');
    if (slash > 0)
    {
        int bassUsed = 0;
        const QString bass = note (out.mid (slash + 1), bassUsed);
        if (! bass.isEmpty())
            out = out.left (slash + 1) + bass + out.mid (slash + 1 + bassUsed);
    }
    return out;
}
}

QString MainWindow::padChordLabel (int pad) const
{
    const int key = padChordKey (pad);
    if (key < 1)
        return {};
    if (keySettings.chordMode == 2)                  // Chord Set : le nom du jeu d'accords, dans la tonalite
    {
        const auto& set = music::chordSets()[(size_t) std::clamp (keySettings.chordSet, 0, (int) music::chordSets().size() - 1)];
        return transposeChordName (set.chords[(size_t) pad - 1].name, keySettings.root);
    }
    const QList<int> notes = music::chordFor (key, keySettings);
    if (keySettings.chordMode == 4 && notes.isEmpty())
        return QStringLiteral ("-");                 // User : case vide
    if (keySettings.chordMode == 1)                  // Oct +1... : la note et l'octave ajoutee
    {
        const auto& h = music::harmonies()[(size_t) std::clamp (keySettings.harmony, 0, (int) music::harmonies().size() - 1)];
        if (h.degrees.empty())
            return session::noteName (notes.isEmpty() ? key : notes.first()) + " " + QString (h.name).remove (' ');
    }
    const QString name = music::chordName (notes);
    if (name.isEmpty() || name.contains (' '))       // accord sans nom connu : sa note et son nombre de notes
        return session::noteName (notes.isEmpty() ? key : *std::min_element (notes.begin(), notes.end()))
               + QString (" (%1)").arg (notes.size());
    return name;
}

QString MainWindow::padLabel (int pad) const
{
    if (padChords)
        return padChordLabel (pad);
    if (padKeyboard)
        return session::noteName (padNote (pad));
    return padName (state.group, pad);
}

void MainWindow::setPadKeyboard (bool on)
{
    padKeyboard = on;
    padChords = false;
    if (on)
        useMelodicSound();
    refreshPadModeButtons();
    refreshAll();
    refreshMaschine();
    toast (on ? QString ("KEYBOARD : sound %1 %2").arg (state.sound.load(), 2, 10, QChar ('0')).arg (padName (state.group, state.sound))
              : QString ("PAD MODE"));
}

// ------------------------------------------------------------------------------- About

void MainWindow::showAbout()
{
    QDialog dialog (this);
    dialog.setWindowTitle (tr ("About Fuzzy John DAW Pulses"));
    auto* col = new QVBoxLayout (&dialog);
    col->setSpacing (10);
    auto* logo = new QLabel;
    logo->setAlignment (Qt::AlignCenter);
#ifdef FUZZY_HAS_LOGO
    logo->setPixmap (QPixmap (":/logo.png").scaled (220, 220, Qt::KeepAspectRatio, Qt::SmoothTransformation));
#else
    logo->setText (QString ("<span style='font-size:28px; font-weight:bold; color:%1'>FUZZY JOHN</span>"
                            "<span style='font-size:28px; color:%1'> DAW Pulses</span>").arg (css (kAccent)));
#endif
    col->addWidget (logo);
    auto* text = new QLabel (tr ("<p align='center'><b>Fuzzy John DAW Pulses</b><br>Version %1</p>"
                                 "<p align='center'>Fuzzy Turtle Software<br>Free software, GPLv3 license</p>"
                                 "<p align='center' style='color:#8a8a92'>Qt %2 &middot; JUCE engine &middot; Maschine MK3 / Maschine+</p>")
                                 .arg (QString (FUZZY_VERSION), qVersion()));
    text->setTextFormat (Qt::RichText);
    col->addWidget (text);
    // Contact : l'adresse a lire (selectionnable a la souris ; pas de lien mailto)
    const QString address = "fuzzyturtlesoftware@gmail.com";
    auto* contact = new QLabel (tr ("<p align='center'>Contact<br><b>%1</b></p>").arg (address));
    contact->setTextFormat (Qt::RichText);
    contact->setTextInteractionFlags (Qt::TextSelectableByMouse);
    col->addWidget (contact);
    auto* github = new QLabel (QString ("<p align='center'>Source code<br><a style='color:%1' href='https://github.com/harrytuttle21/fuzzy-turtle'>"
                                        "github.com/harrytuttle21/fuzzy-turtle</a></p>").arg (css (kAccent)));
    github->setTextFormat (Qt::RichText);
    github->setOpenExternalLinks (true);             // clic : le navigateur s'ouvre
    col->addWidget (github);
    auto* close = new QPushButton (tr ("Close"));
    connect (close, &QPushButton::clicked, &dialog, &QDialog::accept);
    col->addWidget (close);
    dialog.exec();
}

void MainWindow::transposePattern (int step)
{
    // Demi-tons ou octaves, accords compris ; la selection, sinon la case du curseur
    const seq::Pattern p = sequencer.pattern (state.group);
    QList<QPoint> targets = patternView->selection();
    if (targets.isEmpty())
        targets << QPoint (patternView->selectedTrack(), patternView->selectedRow());
    remember (state.group);
    int moved = 0;
    for (const QPoint& c : targets)
    {
        seq::Cell cell = p.rows[(size_t) c.y()][(size_t) c.x()];
        if (cell.empty())
            continue;
        QList<int> notes = PianoRollView::notesOf (cell);
        bool fits = true;
        for (int& n : notes)
            fits &= (n += step) >= 0 && n <= 127;
        if (fits)
        {
            sequencer.setCell (state.group, c.y(), c.x(), PianoRollView::withNotes (cell, notes));
            ++moved;
        }
    }
    patternView->update();
    toast (QString ("%1 %2%3 : %4 note(s)").arg (std::abs (step) == 12 ? "OCT" : "SEMI").arg (step > 0 ? "+" : "-")
               .arg (std::abs (step) == 12 ? 1 : std::abs (step)).arg (moved));
}

void MainWindow::transposeRoll (int step)
{
    showLeftTab (2);
    if (rollStack && rollStack->currentIndex() != 0)   // Steps / Tracker : selection, sinon la case
    {
        transposePattern (step);
        return;
    }
    const int n = pianoRoll->transpose (step);
    toast (n ? QString ("%1 %2 : %3 note(s)").arg (std::abs (step) == 12 ? "OCT" : "SEMI").arg (step > 0 ? "+" : "-").arg (n)
             : QString ("Select notes first"));
}

// ------------------------------------------------------------ copier / deplacer sons et groupes

MainWindow::SoundCopy MainWindow::takeSound (int g, int p) const
{
    const size_t i = (size_t) state.index (g, p);
    SoundCopy s;
    s.valid = true;
    s.sample = state.samples[i];
    s.mode = state.playMode[i];
    s.release = state.releaseMs[i];
    // le synthe « du pad » depend du pad et du groupe : on garde le synthe reel
    s.synth = state.effSynth (g, p);
    s.volume = state.soundVolume[i];
    s.pan = state.soundPan[i];
    s.pitch = state.soundPitch[i];
    s.cutoff = state.soundCutoff[i];
    s.loopStart = state.loopStart[i];
    s.loopEnd = state.loopEnd[i];
    s.reverse = state.soundReverse[i];
    s.poly = state.poly (g, p);
    return s;
}

void MainWindow::putSound (int g, int p, const SoundCopy& s)
{
    const size_t i = (size_t) state.index (g, p);
    state.samples[i] = s.sample;
    state.playMode[i] = s.mode;
    state.releaseMs[i] = s.release;
    // meme synthe que le pad par defaut : on garde « -1 » (comme le pilote)
    const int padDefault = g >= 8 ? 0 : session::State::defaultSynth (p);
    state.synth[i] = s.synth == padDefault ? -1 : s.synth;
    state.soundVolume[i] = s.volume;
    state.soundPan[i] = s.pan;
    state.soundPitch[i] = s.pitch;
    state.soundCutoff[i] = s.cutoff;
    state.loopStart[i] = s.loopStart;
    state.loopEnd[i] = s.loopEnd;
    state.soundReverse[i] = s.reverse;
    state.soundPoly[i] = s.poly;
}

void MainWindow::emptySound (int g, int p)
{
    const size_t i = (size_t) state.index (g, p);
    state.samples[i].clear();
    state.synth[i] = g >= 8 ? -1 : 0;                // A2-H2 : vide par defaut ; A-H : « None »
    state.playMode[i] = session::State::defaultMode (p);
    state.loopStart[i] = 0.0f;
    state.loopEnd[i] = 1.0f;
    state.releaseMs[i] = 150;
    state.resetSoundSettings (i);
}

bool MainWindow::soundIsEmpty (int g, int p) const
{
    return state.samples[(size_t) state.index (g, p)].isEmpty() && state.effSynth (g, p) == 0;
}

void MainWindow::transferSound (int fg, int fp, int tg, int tp, Transfer how)
{
    if (fg == tg && fp == tp)
        return;
    rememberSounds ({ { fg, fp }, { tg, tp } }, {}, how == Transfer::Copy ? "Copy sound" : how == Transfer::Move ? "Move sound" : "Swap sounds");
    const SoundCopy from = takeSound (fg, fp), to = takeSound (tg, tp);
    putSound (tg, tp, from);
    if (how == Transfer::Move)
        emptySound (fg, fp);
    else if (how == Transfer::Swap)
        putSound (fg, fp, to);
    syncSamples();
    refreshAll();
    const QString a = QString ("%1 %2").arg (groupName (fg)).arg (fp, 2, 10, QChar ('0'));
    const QString b = QString ("%1 %2").arg (groupName (tg)).arg (tp, 2, 10, QChar ('0'));
    toast (QString ("%1 %2 %3 %4").arg (how == Transfer::Copy ? "COPY" : how == Transfer::Move ? "MOVE" : "SWAP")
               .arg (a).arg (how == Transfer::Swap ? "<>" : ">").arg (b));
}

void MainWindow::transferGroup (int from, int to, Transfer how)
{
    // Tout le groupe : ses 16 sons, tous ses patterns, son volume (mute / solo non)
    if (from == to || from < 0 || to < 0)
        return;
    std::array<SoundCopy, 16> a, b;
    for (int p = 1; p <= 16; ++p)
    {
        a[(size_t) p - 1] = takeSound (from, p);
        b[(size_t) p - 1] = takeSound (to, p);
    }
    const seq::GroupPatterns pa = sequencer.groupPatterns (from), pb = sequencer.groupPatterns (to);   // tous les patterns
    const int va = state.groupVolume[(size_t) from], vb = state.groupVolume[(size_t) to];
    const QString la = state.groupLabel[(size_t) from], lb = state.groupLabel[(size_t) to];   // noms des groupes
    rememberSounds ({}, how == Transfer::Copy ? std::vector<int> { to } : std::vector<int> { from, to },
                    how == Transfer::Copy ? "Copy group" : how == Transfer::Move ? "Move group" : "Swap groups");
    for (int p = 1; p <= 16; ++p)
        putSound (to, p, a[(size_t) p - 1]);
    sequencer.setGroupPatterns (to, pa);
    state.groupVolume[(size_t) to] = va;
    state.groupLabel[(size_t) to] = la;
    if (how != Transfer::Copy)
    {
        for (int p = 1; p <= 16; ++p)
            how == Transfer::Swap ? putSound (from, p, b[(size_t) p - 1]) : emptySound (from, p);
        if (how == Transfer::Swap)
            sequencer.setGroupPatterns (from, pb);
        else
            sequencer.clearGroupPatterns (from, sequencer.patternLength (from));
        state.groupVolume[(size_t) from] = how == Transfer::Swap ? vb : 100;
        state.groupLabel[(size_t) from] = how == Transfer::Swap ? lb : QString();
    }
    syncSamples();
    refreshAll();
    showPatternState();
    toast (QString ("%1 GROUP %2 > %3").arg (how == Transfer::Copy ? "COPY" : how == Transfer::Move ? "MOVE" : "SWAP")
               .arg (groupName (from)).arg (groupName (to)));
}

bool MainWindow::confirmGroup (int from, int to, Transfer how)
{
    QMessageBox box (QMessageBox::Question, tr ("Group %1 > %2").arg (groupName (from)).arg (groupName (to)),
                     tr ("%1 group %2 to group %3?\nIts 16 sounds, all its patterns and its volume replace those of group %3.%4")
                         .arg (how == Transfer::Move ? tr ("Move") : tr ("Copy"))
                         .arg (groupName (from)).arg (groupName (to))
                         .arg (how == Transfer::Move ? tr ("\nGroup %1 becomes empty.").arg (groupName (from)) : QString()),
                     QMessageBox::Yes | QMessageBox::Cancel, this);
    box.setDefaultButton (QMessageBox::Cancel);
    return box.exec() == QMessageBox::Yes;
}

void MainWindow::soundDropMenu (int fg, int fp, int tp, const QPoint& at)
{
    if (padKeyboard)                                 // Keyboard / Chords : les pads ne deplacent pas les sons
    {
        if (padChords && keySettings.chordMode == 4 && fg == state.group)
            chordDropMenu (fp, tp, at);              // set User : ranger les accords
        else
            toast (padChords ? "Chords mode: sounds don't move (User sets: drag = chords)" : "Keyboard mode: sounds don't move");
        return;
    }
    // Pad lache sur un autre pad : Copy / Move / Swap (le menu sert de confirmation)
    const int tg = state.group;
    QMenu menu (this);
    const QString from = QString ("%1 %2 (%3)").arg (groupName (fg)).arg (fp, 2, 10, QChar ('0')).arg (padName (fg, fp));
    const QString to = QString ("%1 %2").arg (groupName (tg)).arg (tp, 2, 10, QChar ('0'));
    auto* title = menu.addAction (from + "  >  " + to + (soundIsEmpty (tg, tp) ? QString() : tr ("  (replaces %1)").arg (padName (tg, tp))));
    title->setEnabled (false);
    menu.addSeparator();
    auto* copy = menu.addAction (tr ("Copy here"));
    auto* move = menu.addAction (tr ("Move here"));
    auto* swap = menu.addAction (tr ("Swap"));
    menu.addAction (tr ("Cancel"));
    auto* picked = menu.exec (at);
    if (picked == copy || picked == move || picked == swap)
        transferSound (fg, fp, tg, tp, picked == copy ? Transfer::Copy : picked == move ? Transfer::Move : Transfer::Swap);
}

void MainWindow::groupDropped (int index, const QMimeData* mime)
{
    const int tg = index + (state.group >= 8 ? 8 : 0);
    if (mime->hasFormat (kGroupMime))                // groupe lache sur un groupe
    {
        const int fg = mime->data (kGroupMime).toInt();
        if (fg == tg)
            return;
        QMenu menu (this);
        auto* title = menu.addAction (tr ("Group %1  >  group %2").arg (groupName (fg)).arg (groupName (tg)));
        title->setEnabled (false);
        menu.addSeparator();
        auto* copy = menu.addAction (tr ("Copy group here..."));
        auto* move = menu.addAction (tr ("Move group here..."));
        menu.addAction (tr ("Cancel"));
        auto* picked = menu.exec (QCursor::pos());
        if ((picked == copy || picked == move) && confirmGroup (fg, tg, picked == move ? Transfer::Move : Transfer::Copy))
            transferGroup (fg, tg, picked == move ? Transfer::Move : Transfer::Copy);
        return;
    }
    const QStringList from = QString::fromLatin1 (mime->data (kSoundMime)).split (':');
    if (from.size() == 2)                            // pad lache sur un groupe : choisir le pad
        pickPadDialog (from[0].toInt(), from[1].toInt(), tg);
}

void MainWindow::pickPadDialog (int fg, int fp, int tg, bool)
{
    // Les 16 pads du groupe vise (comme la Maschine : 1 en bas a gauche) ; choisir puis Copy / Move
    QDialog dialog (this);
    dialog.setWindowTitle (tr ("%1 %2  >  group %3").arg (groupName (fg)).arg (fp, 2, 10, QChar ('0')).arg (groupName (tg)));
    auto* col = new QVBoxLayout (&dialog);
    auto* info = new QLabel (tr ("Choose the pad of group %1 for %2:").arg (groupName (tg)).arg (padName (fg, fp)));
    col->addWidget (info);
    auto* grid = new QGridLayout;
    grid->setSpacing (6);
    const QColor c = groupColor (tg);
    int chosen = 0;
    QList<QPushButton*> buttons;
    auto* warn = new QLabel;
    warn->setObjectName ("hint");
    for (int p = 1; p <= 16; ++p)
    {
        auto* b = new QPushButton (QString ("%1\n%2").arg (p, 2, 10, QChar ('0')).arg (padName (tg, p).left (12)));
        b->setCheckable (true);
        b->setMinimumSize (92, 56);
        const bool empty = soundIsEmpty (tg, p);
        b->setStyleSheet (QString ("QPushButton { color: %1; border: 2px solid %2; background: #17171c; }"
                                   "QPushButton:checked { color: #111; background: %2; }")
                              .arg (empty ? "#777" : css (c)).arg (css (empty ? QColor (70, 70, 78) : c)));
        grid->addWidget (b, 3 - (p - 1) / 4, (p - 1) % 4);
        buttons << b;
        connect (b, &QPushButton::clicked, &dialog, [&, p]
        {
            chosen = p;
            for (int k = 0; k < 16; ++k)
                buttons[k]->setChecked (k == p - 1);
            warn->setText (soundIsEmpty (tg, p) ? tr ("Pad %1 is empty.").arg (p, 2, 10, QChar ('0'))
                                                 : tr ("Replaces %1 on pad %2.").arg (padName (tg, p)).arg (p, 2, 10, QChar ('0')));
        });
    }
    col->addLayout (grid);
    col->addWidget (warn);
    auto* box = new QDialogButtonBox;
    auto* copy = box->addButton (tr ("Copy"), QDialogButtonBox::AcceptRole);
    auto* move = box->addButton (tr ("Move"), QDialogButtonBox::AcceptRole);
    box->addButton (QDialogButtonBox::Cancel);
    copy->setEnabled (false);
    move->setEnabled (false);
    for (auto* b : buttons)
        connect (b, &QPushButton::clicked, &dialog, [&] { copy->setEnabled (! (tg == fg && chosen == fp)); move->setEnabled (copy->isEnabled()); });
    QPushButton* clicked = nullptr;
    connect (box, &QDialogButtonBox::clicked, &dialog, [&] (QAbstractButton* b)
    {
        clicked = qobject_cast<QPushButton*> (b);
        b == copy || b == move ? dialog.accept() : dialog.reject();
    });
    col->addWidget (box);
    if (dialog.exec() == QDialog::Accepted && chosen && (clicked == copy || clicked == move))
        transferSound (fg, fp, tg, chosen, clicked == copy ? Transfer::Copy : Transfer::Move);
}

// ------------------------------------------------------------ affichage : traits parasites (macOS)

void MainWindow::fixSeparators()
{
    // Sur macOS, les separateurs entre panneaux sont de petits widgets. Apres un changement de
    // disposition (onglets, vue Default), certains restent a une ancienne place, par-dessus un
    // panneau : c'est le trait fin noir. Un vrai separateur est ENTRE deux panneaux, jamais dedans.
    QList<QRect> inside;
    for (auto* d : findChildren<QDockWidget*> (QString(), Qt::FindDirectChildrenOnly))
        if (d->isVisible() && ! d->isFloating() && ! d->visibleRegion().isEmpty())
            inside << d->geometry().adjusted (3, 3, -3, -3);
    for (auto* w : findChildren<QWidget*> ("qt_qmainwindow_extended_splitter", Qt::FindDirectChildrenOnly))
    {
        bool stray = ! rect().contains (w->geometry());
        for (const QRect& r : inside)
            stray |= r.intersects (w->geometry());
        if (stray)
            w->hide();
    }
}

void MainWindow::showDisplayDiagnostics()
{
    // Pour trouver un trait parasite : ce qui est sous la souris, et les widgets tres fins
    QString text;
    if (auto* under = QApplication::widgetAt (QCursor::pos()))
    {
        text += "Under the mouse: ";
        for (QWidget* w = under; w && w != this; w = w->parentWidget())
            text += QString ("%1 '%2' > ").arg (w->metaObject()->className(), w->objectName());
        text += "\n\n";
    }
    text += "Thin widgets (6 px or less):\n";
    for (auto* w : findChildren<QWidget*>())
        if (w->isVisible() && w->width() > 0 && w->height() > 0 && (w->width() <= 6 || w->height() <= 6))
        {
            const QPoint p = w->mapTo (this, QPoint());
            text += QString ("%1 '%2'  x %3, y %4, %5 x %6  (in %7)\n").arg (w->metaObject()->className(), w->objectName())
                        .arg (p.x()).arg (p.y()).arg (w->width()).arg (w->height())
                        .arg (w->parentWidget() ? w->parentWidget()->metaObject()->className() : "-");
        }
    printf ("%s\n", qPrintable (text));
    fflush (stdout);
    QMessageBox box (QMessageBox::Information, tr ("Display diagnostics"),
                     tr ("Put the mouse on the thin line and press F12 (or fn + F12) to name it.\n"
                         "Copy this text (Show Details) and send it."), QMessageBox::Ok, this);
    box.setDetailedText (text);
    auto* hide = box.addButton (tr ("Hide thin widgets (test)"), QMessageBox::ActionRole);
    box.exec();
    if (box.clickedButton() == hide)
        for (auto* w : findChildren<QWidget*>())
            if (w->isVisible() && w->width() > 0 && w->height() > 0 && (w->width() <= 6 || w->height() <= 6)
                && ! qobject_cast<QAbstractButton*> (w))
                w->hide();
}

void MainWindow::useMelodicSound()
{
    // Comme le pilote : KEYBOARD / CHORDS jouent des notes ; un son de batterie de synthese (1-4
    // sans sample) ne sait pas faire de melodie : on prend le son 5
    const int g = state.group, s = state.sound;
    if (s <= 4 && state.samples[(size_t) state.index (g, s)].isEmpty() && state.effSynth (g, s) >= 1 && state.effSynth (g, s) <= 4)
        selectSound (5);
}

// ------------------------------------------------------------ copier / coller : sons et groupes

void MainWindow::copySounds (const QList<int>& list, bool cut)
{
    soundClips.clear();
    QStringList names;
    for (int p : list)
    {
        soundClips.push_back ({ p, takeSound (state.group, p) });
        names << QString ("%1").arg (p, 2, 10, QChar ('0'));
    }
    soundClipGroup = state.group;
    soundClipCut = cut;
    toast (QString ("%1 %2 %3").arg (cut ? "CUT" : "COPY").arg (groupName (state.group)).arg (names.join (" ")));
}

void MainWindow::pasteSounds (int pad)
{
    // Les sons gardent leur ecart : le 1er va sur ce pad, les autres a cote (hors 1-16 : ignores)
    if (soundClips.empty())
    {
        toast ("Nothing to paste (Copy Sound first)");
        return;
    }
    const int g = state.group, first = soundClips.front().first;
    std::vector<std::pair<int, SoundCopy>> place;
    QStringList replaced;
    for (const auto& [p, s] : soundClips)
    {
        const int to = pad + p - first;
        if (to < 1 || to > 16 || (g == soundClipGroup && to == p && ! soundClipCut))
            continue;
        place.push_back ({ to, s });
        if (! soundIsEmpty (g, to))
            replaced << QString ("%1 %2").arg (to, 2, 10, QChar ('0')).arg (padName (g, to));
    }
    const Transfer how = soundClipCut ? Transfer::Move : Transfer::Copy;
    if (place.empty() || (! replaced.isEmpty()
        && QMessageBox::question (this, tr ("Paste Sound"), tr ("%1 %2 sound(s) to group %3?\nIt replaces: %4.")
                                      .arg (how == Transfer::Move ? tr ("Move") : tr ("Copy")).arg (place.size())
                                      .arg (groupName (g)).arg (replaced.join (", "))) != QMessageBox::Yes))
        return;
    std::vector<std::pair<int, int>> touched;
    for (const auto& [to, s] : place)
        touched.push_back ({ g, to });
    if (how == Transfer::Move)
        for (const auto& [p, s] : soundClips)
            touched.push_back ({ soundClipGroup, p });
    rememberSounds (touched, {}, "Paste sound");
    if (how == Transfer::Move)                       // d'abord vider l'origine (les pads peuvent se chevaucher)
        for (const auto& [p, s] : soundClips)
            emptySound (soundClipGroup, p);
    for (const auto& [to, s] : place)
        putSound (g, to, s);
    if (how == Transfer::Move)
        soundClips.clear();
    syncSamples();
    refreshAll();
    toast (QString ("PASTE %1 sound(s) > %2 %3").arg (place.size()).arg (groupName (g)).arg (pad, 2, 10, QChar ('0')));
}

QList<int> MainWindow::groupTargets (int g) const
{
    if (! groupChosen.contains (g))
        return { g };
    QList<int> out (groupChosen.begin(), groupChosen.end());
    std::sort (out.begin(), out.end());
    return out;
}

void MainWindow::copyGroups (const QList<int>& list, bool cut)
{
    groupClips.assign (list.begin(), list.end());
    groupClipCut = cut;
    QStringList names;
    for (int g : list)
        names << groupName (g);
    toast (QString ("%1 GROUP %2").arg (cut ? "CUT" : "COPY").arg (names.join (" ")));
}

void MainWindow::pasteGroups (int target)
{
    // Les groupes gardent leur ecart (A, C colles sur E -> E, G) ; tout est lu avant d'ecrire
    if (groupClips.empty())
    {
        toast ("Nothing to paste (Copy Group first)");
        return;
    }
    const int first = groupClips.front();
    struct Save { int from, to; std::array<SoundCopy, 16> sounds; seq::GroupPatterns patterns; int volume; QString label; };
    std::vector<Save> saves;
    QStringList what;
    for (int from : groupClips)
    {
        const int to = target + from - first;
        if (to < 0 || to >= kGroups || (to == from && ! groupClipCut))
            continue;
        Save s { from, to, {}, sequencer.groupPatterns (from), state.groupVolume[(size_t) from].load(), state.groupLabel[(size_t) from] };
        for (int p = 1; p <= 16; ++p)
            s.sounds[(size_t) p - 1] = takeSound (from, p);
        saves.push_back (s);
        what << groupName (from) + " > " + groupName (to);
    }
    if (saves.empty())
        return;
    QMessageBox box (QMessageBox::Question, tr ("Paste Group"),
                     tr ("%1: %2?\nThe 16 sounds, all the patterns and the volume replace those of the target group(s).%3")
                         .arg (groupClipCut ? tr ("Move") : tr ("Copy")).arg (what.join (", "))
                         .arg (groupClipCut ? tr ("\nThe moved groups become empty.") : QString()),
                     QMessageBox::Yes | QMessageBox::Cancel, this);
    box.setDefaultButton (QMessageBox::Cancel);
    if (box.exec() != QMessageBox::Yes)
        return;
    std::vector<int> involved;
    for (const auto& s : saves)
    {
        involved.push_back (s.to);
        if (groupClipCut)
            involved.push_back (s.from);
    }
    std::sort (involved.begin(), involved.end());
    involved.erase (std::unique (involved.begin(), involved.end()), involved.end());
    rememberSounds ({}, involved, groupClipCut ? "Move group" : "Copy group");
    if (groupClipCut)
        for (const auto& s : saves)
        {
            for (int p = 1; p <= 16; ++p)
                emptySound (s.from, p);
            sequencer.clearGroupPatterns (s.from, sequencer.patternLength (s.from));
            state.groupVolume[(size_t) s.from] = 100;
            state.groupLabel[(size_t) s.from].clear();
        }
    for (const auto& s : saves)
    {
        for (int p = 1; p <= 16; ++p)
            putSound (s.to, p, s.sounds[(size_t) p - 1]);
        sequencer.setGroupPatterns (s.to, s.patterns);
        state.groupVolume[(size_t) s.to] = s.volume;
        state.groupLabel[(size_t) s.to] = s.label;
    }
    if (groupClipCut)
        groupClips.clear();
    groupChosen.clear();
    syncSamples();
    refreshAll();
    refreshMuteSolo();
    showPatternState();
    toast (QString ("PASTE GROUP %1").arg (what.join (", ")));
}

// ------------------------------------------------------------ vue TRACKER

QWidget* MainWindow::buildTracker()
{
    auto* panel = new QFrame;
    panel->setObjectName ("panel");
    auto* col = new QVBoxLayout (panel);
    auto* head = new QHBoxLayout;
    auto* title = new QLabel (tr ("TRACKER"));
    title->setObjectName ("section");
    head->addWidget (title);
    // Edit : le clavier ecrit (ligne rouge) ; sinon il joue seulement
    trackerEditButton = new QPushButton (tr ("Edit"));
    trackerEditButton->setCheckable (true);
    trackerEditButton->setFocusPolicy (Qt::NoFocus);
    trackerEditButton->setObjectName ("keys");
    trackerEditButton->setToolTip (tr ("Edit: the computer keys write notes at the cursor (red line)"));
    head->addWidget (trackerEditButton);
    head->addSpacing (8);
    // Memes outils que la vue Pattern : Length, x2, Generate, Clear
    auto* lenLabel = new QLabel (tr ("Length"));
    lenLabel->setObjectName ("hint");
    trackerLength = new QSpinBox;
    trackerLength->setRange (1, seq::kMaxRows);
    trackerLength->setFocusPolicy (Qt::ClickFocus);
    trackerLength->setToolTip (tr ("Pattern length in lines (4 lines = 1 beat)"));
    trackerLength->setValue (sequencer.patternLength (state.group));
    connect (trackerLength, &QSpinBox::editingFinished, trackerLength, [this] { trackerLength->clearFocus(); });
    connect (trackerLength, &QSpinBox::valueChanged, this, [this] (int v) { lengthBox->setValue (v); tracker->update(); });
    auto* twice = new QPushButton (QStringLiteral ("\u00D72"));
    twice->setFocusPolicy (Qt::NoFocus);
    twice->setToolTip (tr ("Double: length x 2, notes copied into the new half"));
    connect (twice, &QPushButton::clicked, this, [this] { doublePattern(); });
    auto* gen = new QPushButton (QStringLiteral ("\U0001F3B2 ") + tr ("Generate"));
    gen->setFocusPolicy (Qt::NoFocus);
    gen->setToolTip (tr ("Random notes for the chosen sound (Cmd+G)"));
    connect (gen, &QPushButton::clicked, this, [this] { openGenerate(); });
    auto* clear = new QPushButton (tr ("Clear"));
    clear->setFocusPolicy (Qt::NoFocus);
    clear->setToolTip (tr ("Erase the whole pattern of this group"));
    connect (clear, &QPushButton::clicked, this, [this]
    {
        remember (state.group);
        sequencer.clear (state.group);
        patternView->clearSelection();
        tracker->update();
    });
    head->addWidget (lenLabel);
    head->addWidget (trackerLength);
    head->addWidget (twice);
    head->addWidget (gen);
    head->addWidget (clear);
    head->addSpacing (8);
    for (const int step : { -1, 1, -12, 12 })        // transposition : selection, sinon la case
    {
        auto* t = new QPushButton (step == -12 ? tr ("OCT -") : step == -1 ? tr ("SEMI -") : step == 1 ? tr ("SEMI +") : tr ("OCT +"));
        t->setFocusPolicy (Qt::NoFocus);
        connect (t, &QPushButton::clicked, this, [this, step] { transposePattern (step); showLeftTab (5); });
        head->addWidget (t);
    }
    head->addSpacing (8);
    auto* stepLabel = new QLabel (tr ("Step"));
    trackerStep = new QSpinBox;
    trackerStep->setRange (0, 16);
    trackerStep->setFocusPolicy (Qt::ClickFocus);
    trackerStep->setToolTip (tr ("Lines the cursor goes down after each note (0 = stays)"));
    head->addWidget (stepLabel);
    head->addWidget (trackerStep);
    auto* followButton = new QPushButton (tr ("Follow"));
    followButton->setCheckable (true);
    followButton->setFocusPolicy (Qt::NoFocus);
    followButton->setObjectName ("keys");
    followButton->setToolTip (tr ("While playing, the view follows the play line"));
    head->addWidget (followButton);
    auto* hint = new QLabel (tr ("keys = notes · digits = velocity / length · Del = erase · arrows, Tab · double-click = note · %1 + wheel = value")
                                 .arg (PatternView::copyKey()));
    hint->setObjectName ("hint");
    registerHint (hint, "TRACKER");
    hint->setSizePolicy (QSizePolicy::Ignored, QSizePolicy::Preferred);
    hint->setMinimumWidth (0);
    hint->setAlignment (Qt::AlignRight | Qt::AlignVCenter);
    head->addWidget (hint, 1);
    col->addLayout (head);
    tracker = new TrackerView (sequencer, state, *patternView);
    col->addWidget (tracker, 1);

    QSettings s;
    trackerEditButton->setChecked (s.value ("tracker/edit", true).toBool());
    trackerStep->setValue (s.value ("tracker/step", 1).toInt());
    followButton->setChecked (s.value ("tracker/follow", true).toBool());
    tracker->setEditMode (trackerEditButton->isChecked());
    tracker->setFollow (followButton->isChecked());
    connect (trackerEditButton, &QPushButton::toggled, this, [this] (bool on)
    {
        tracker->setEditMode (on);
        QSettings().setValue ("tracker/edit", on);
    });
    connect (trackerStep, &QSpinBox::valueChanged, this, [] (int v) { QSettings().setValue ("tracker/step", v); });
    connect (followButton, &QPushButton::toggled, this, [this] (bool on)
    {
        tracker->setFollow (on);
        QSettings().setValue ("tracker/follow", on);
    });
    connect (tracker, &TrackerView::cellMenu, this, &MainWindow::patternMenu);
    connect (tracker, &TrackerView::trackClicked, this, [this] (int track, bool solo) { toggleSound (state.group, track + 1, solo); });
    connect (tracker, &TrackerView::cellClicked, this, [this] (int track)
    {
        typedDigits.clear();
        tracker->setTyping ({});
        if (track + 1 != state.sound)
            selectSound (track + 1);
    });
    connect (tracker, &TrackerView::cellDoubleClicked, this, [this] (int row, int track, int)
    {
        // Double-clic : case vide = une note (la derniere ecrite) ; note = ses options
        if (sequencer.pattern (state.group).rows[(size_t) row][(size_t) track].empty())
            trackerWrite (row, track, lastTrackerNote >= 0 ? lastTrackerNote : defaultNote (track + 1));
        else
            noteOptions (row, track);
    });
    connect (tracker, &TrackerView::valueWheel, this, [this] (int row, int track, int field, int steps)
    {
        // Option + molette : note (demi-tons), velocite (de 4 en 4), longueur (en lignes)
        seq::Cell c = sequencer.pattern (state.group).rows[(size_t) row][(size_t) track];
        if (c.empty())
            return;
        remember (state.group);
        if (field == TrackerView::Note)
        {
            QList<int> notes = PianoRollView::notesOf (c);
            bool fits = true;
            for (int& n : notes)
                fits &= (n += steps) >= 0 && n <= 127;
            if (! fits)
                return;
            c = PianoRollView::withNotes (c, notes);
        }
        else if (field == TrackerView::Velocity)
            c.velocity = std::clamp (c.velocity + steps * 4, 1, 127);
        else
            c.length = std::clamp (c.length + steps, 1, sequencer.pattern (state.group).length);
        sequencer.setCell (state.group, row, track, c);
        patternView->update();
        tracker->update();
    });
    // Le Tracker suit les changements (enregistrement, Maschine, autres vues)
    auto* timer = new QTimer (tracker);
    connect (timer, &QTimer::timeout, this, [this]
    {
        for (QWidget* v : { (QWidget*) rollTracker, (QWidget*) stepView, (QWidget*) sceneView })   // Piano Roll, Scenes
            if (v && ! v->visibleRegion().isEmpty())
                v->update();
        if (rollLength && ! rollLength->hasFocus() && rollLength->value() != sequencer.patternLength (state.group))
        {
            const QSignalBlocker block (rollLength);  // longueur changee ailleurs
            rollLength->setValue (sequencer.patternLength (state.group));
        }
        if (trackerShown())
        {
            tracker->update();
            if (! trackerLength->hasFocus() && trackerLength->value() != sequencer.patternLength (state.group))
            {
                const QSignalBlocker block (trackerLength);   // longueur changee ailleurs (Pattern, Maschine)
                trackerLength->setValue (sequencer.patternLength (state.group));
            }
        }
    });
    timer->start (66);
    return panel;
}

TrackerView* MainWindow::activeTracker() const
{
    if (trackerShown())
        return tracker;
    if (rollTracker && rollDock->isVisible() && ! rollTracker->visibleRegion().isEmpty())
        return rollTracker;
    return nullptr;
}

bool MainWindow::stepsKey (QKeyEvent* e)
{
    // Affichage Steps : fleches = pas (gauche / droite) et mesures (haut / bas), Suppr = effacer
    const int key = e->key(), t = state.sound - 1;
    const int length = sequencer.patternLength (state.group);
    int row = patternView->selectedRow();
    const int d = key == Qt::Key_Left ? -1 : key == Qt::Key_Right ? 1 : key == Qt::Key_Up ? -16 : key == Qt::Key_Down ? 16 : 0;
    if (d)
    {
        patternView->setCurrentCell (((row + d) % length + length) % length, t);
        stepView->update();
        return true;
    }
    if (key == Qt::Key_Delete || key == Qt::Key_Backspace)
    {
        remember (state.group);
        const QList<QPoint> sel = patternView->selection();
        if (sel.isEmpty())
            sequencer.setCell (state.group, row, t, {});
        for (const QPoint& c : sel)
            sequencer.setCell (state.group, c.y(), c.x(), {});
        patternView->clearSelection();
        stepView->update();
        return true;
    }
    return false;
}

void MainWindow::trackerWrite (int row, int track, int note)
{
    // Ecrit une note (ou l'accord du menu Chords) dans la case ; garde la longueur deja la
    const seq::Cell old = sequencer.pattern (state.group).rows[(size_t) row][(size_t) track];
    QList<int> notes = keySettings.chordMode ? music::chordFor (note, keySettings) : QList<int> { note };
    seq::Cell c { notes.isEmpty() ? note : notes.first(), state.velocity.load(), old.empty() ? 1 : old.length, 0 };
    c = PianoRollView::withNotes (c, notes);
    remember (state.group);
    sequencer.setCell (state.group, row, track, c);
    lastTrackerNote = note;
    patternView->update();
    tracker->update();
    if (rollTracker)
        rollTracker->update();
}

bool MainWindow::trackerKey (QKeyEvent* e)
{
    // Touches de la vue Tracker (sans Cmd / Ctrl / Option : ceux-la sont des raccourcis)
    TrackerView* tracker = activeTracker();          // l'onglet Tracker, ou celui du Piano Roll
    if (! tracker)
        return false;
    if (tracker->singleTrack() && patternView->selectedTrack() != state.sound - 1)
        patternView->setCurrentCell (patternView->selectedRow(), state.sound - 1);
    const int key = e->key();
    const int length = sequencer.pattern (state.group).length;
    auto follow = [this, tracker]                             // la piste du curseur = le son choisi
    {
        typedDigits.clear();
        tracker->setTyping ({});
        if (patternView->selectedTrack() + 1 != state.sound)
            selectSound (patternView->selectedTrack() + 1);
        tracker->update();
    };
    auto advance = [this, tracker]
    {
        if (trackerStep->value())
            tracker->moveCursor (trackerStep->value(), 0);
    };
    // Deplacements
    if (key == Qt::Key_Up || key == Qt::Key_Down)
    {
        tracker->moveCursor (key == Qt::Key_Up ? -1 : 1, 0);
        follow();
        return true;
    }
    if (key == Qt::Key_Left || key == Qt::Key_Right)
    {
        tracker->moveCursor (0, key == Qt::Key_Left ? -1 : 1);
        follow();
        return true;
    }
    if ((key == Qt::Key_Tab || key == Qt::Key_Backtab) && tracker->singleTrack())
        return true;                                 // une seule piste
    if (key == Qt::Key_Tab || key == Qt::Key_Backtab)  // piste suivante / precedente
    {
        const int t = std::clamp (patternView->selectedTrack() + (key == Qt::Key_Tab ? 1 : -1), 0, seq::kTracks - 1);
        patternView->setCurrentCell (patternView->selectedRow(), t);
        tracker->setField (TrackerView::Note);
        follow();
        return true;
    }
    if (key == Qt::Key_PageUp || key == Qt::Key_PageDown)
    {
        tracker->moveCursor (key == Qt::Key_PageUp ? -16 : 16, 0);
        follow();
        return true;
    }
    if (key == Qt::Key_Home || key == Qt::Key_End)
    {
        patternView->setCurrentCell (key == Qt::Key_Home ? 0 : length - 1, patternView->selectedTrack());
        follow();
        return true;
    }
    const int row = patternView->selectedRow(), track = patternView->selectedTrack();
    // Effacer : la selection, sinon la case (puis le curseur descend)
    if (key == Qt::Key_Delete || key == Qt::Key_Backspace)
    {
        if (! trackerEditButton->isChecked())
        {
            toast ("Tracker: turn Edit on to erase");
            return true;
        }
        remember (state.group);
        const QList<QPoint> sel = patternView->selection();
        if (sel.isEmpty())
        {
            sequencer.setCell (state.group, row, track, {});
            advance();
        }
        for (const QPoint& c : sel)
            sequencer.setCell (state.group, c.y(), c.x(), {});
        patternView->clearSelection();
        patternView->update();
        tracker->update();
        return true;
    }
    // Chiffres (aussi la rangee du haut d'un clavier AZERTY, sans Maj) : velocite ou longueur
    if (tracker->field() != TrackerView::Note)
    {
        static const QString azerty = QString::fromUtf8 ("à&é\"'(-è_ç");   // 0 a 9
        const QString t = e->text();
        int digit = -1;
        if (t.size() == 1 && t[0].isDigit())
            digit = t[0].digitValue();
        else if (t.size() == 1 && azerty.contains (t[0]))
            digit = (int) azerty.indexOf (t[0]);
        if (digit < 0)
            return false;
        if (! trackerEditButton->isChecked())
        {
            toast ("Tracker: turn Edit on to type values");
            return true;
        }
        seq::Cell c = sequencer.pattern (state.group).rows[(size_t) row][(size_t) track];
        if (c.empty())
        {
            toast ("No note here");
            return true;
        }
        const bool vel = tracker->field() == TrackerView::Velocity;
        if (typedDigits.isEmpty())
            remember (state.group);
        typedDigits += QString::number (digit);
        const int value = typedDigits.toInt();
        if (vel)
            c.velocity = std::clamp (value, 1, 127);
        else
            c.length = std::clamp (value, 1, length);
        sequencer.setCell (state.group, row, track, c);
        tracker->setTyping (typedDigits);
        if (typedDigits.size() >= (vel ? 3 : 2))       // valeur complete : ligne suivante
        {
            typedDigits.clear();
            tracker->setTyping ({});
            advance();
        }
        patternView->update();
        return true;
    }
    // Champ NOTE en mode Edit : les touches piano ecrivent la note (et la jouent)
    if (! trackerEditButton->isChecked())
        return false;                                // Edit coupe : jeu normal des touches
    char ch = 0;
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        ch = char ('a' + (key - Qt::Key_A));
    else if (key == Qt::Key_Semicolon)
        ch = ';';
    else if (e->text().size() == 1)
        ch = e->text().at (0).toLower().toLatin1();
    const KeyLayout& lay = layoutBox->currentText() == "QWERTY" ? kQwerty : kAzerty;
    const char* pos = ch ? std::strchr (lay.notes, ch) : nullptr;
    if (! pos || ch == lay.octDown || ch == lay.octUp || ch == lay.velDown || ch == lay.velUp)
        return false;                                // octave, velocite... : comme d'habitude
    const int note = qBound (0, 12 * (state.octave + 1) + int (pos - lay.notes), 127);
    trackerWrite (row, track, note);
    keyNotes.insert (key, note);                     // on l'entend tant que la touche est tenue
    playKey (note, state.velocity, true);
    piano->setHeld (note, true);
    advance();
    return true;
}

// ------------------------------------------------------------ vue SCENES

void MainWindow::patternsChanged()
{
    if (songSceneListRefresh)
        songSceneListRefresh();                      // les scenes de la vue Song
    if (palette)
        palette->update();
    if (sceneListRefresh)
        sceneListRefresh();
    if (patternManagerRefresh)
        patternManagerRefresh();                     // la liste des patterns suit
    // Un pattern ou une scene a change : toutes les vues suivent
    showPatternState();                              // longueur, selection
    refreshAll();
    patternView->update();
    pianoRoll->update();
    tracker->update();
    if (sceneView)
        sceneView->update();
}

QWidget* MainWindow::buildScenes()
{
    auto* panel = new QFrame;
    panel->setObjectName ("panel");
    auto* col = new QVBoxLayout (panel);
    auto* head = new QHBoxLayout;
    auto* title = new QLabel (tr ("SCENES"));
    title->setObjectName ("section");
    head->addWidget (title);
    auto* store = new QPushButton (tr ("Store playing as new scene"));
    store->setFocusPolicy (Qt::NoFocus);
    store->setToolTip (tr ("The patterns playing now go into the next empty scene"));
    connect (store, &QPushButton::clicked, this, [this]
    {
        for (int i = 0; i < seq::kPatternSlots; ++i)
            if (! sequencer.scene (i).used)
            {
                patternBar->storeScene (i);          // la prochaine scene vide : ce qui joue
                return;
            }
        toast ("No empty scene");
    });
    head->addWidget (store);
    auto* rotate = new QPushButton (tr ("Rotate"));
    rotate->setCheckable (true);
    rotate->setFocusPolicy (Qt::NoFocus);
    rotate->setObjectName ("keys");
    rotate->setToolTip (tr ("Scenes in columns, groups in rows (or back)"));
    head->addWidget (rotate);
    // Modes : Play (defaut) = cliquer joue ; Edit = cliquer modifie ; les deux ; aucun = protege
    auto* editBtn = new QPushButton (tr ("Edit"));
    auto* playBtn = new QPushButton (tr ("Play"));
    for (auto* b : { editBtn, playBtn })
    {
        b->setCheckable (true);
        b->setFocusPolicy (Qt::NoFocus);
        b->setObjectName ("keys");
        head->addWidget (b);
    }
    auto* listBtn = new QPushButton (tr ("List"));
    listBtn->setFocusPolicy (Qt::NoFocus);
    listBtn->setToolTip (tr ("List of the scenes and their patterns (S1 - AP1, BP3...)"));
    connect (listBtn, &QPushButton::clicked, this, [this] { openSceneList(); });
    head->insertWidget (1, listBtn);
    auto* paletteBtn = new QPushButton (tr ("Palette"));
    paletteBtn->setCheckable (true);
    paletteBtn->setFocusPolicy (Qt::NoFocus);
    paletteBtn->setObjectName ("keys");
    paletteBtn->setToolTip (tr ("16 pattern squares of the chosen group: drag one into a scene cell (Edit on)"));
    head->addWidget (paletteBtn);
    editBtn->setToolTip (tr ("Edit: click a cell = on / off in the scene, right-click = choose a pattern, lock, rename"));
    playBtn->setToolTip (tr ("Play: click a cell = play that pattern, click a scene = launch it (Edit and Play both off = protected)"));
    auto* hint = new QLabel;
    hint->setObjectName ("hint");
    registerHint (hint, "SCENES");
    hint->setSizePolicy (QSizePolicy::Ignored, QSizePolicy::Preferred);
    hint->setMinimumWidth (0);
    hint->setAlignment (Qt::AlignRight | Qt::AlignVCenter);
    head->addWidget (hint, 1);
    col->addLayout (head);
    sceneView = new SceneView (sequencer, state);
    sceneView->isPlaying = [this] { return transport && transport->playing(); };
    rotate->setChecked (QSettings().value ("scenes/rotated", false).toBool());
    editBtn->setChecked (QSettings().value ("scenes/edit", false).toBool());
    playBtn->setChecked (QSettings().value ("scenes/play", true).toBool());
    auto modes = [this, editBtn, playBtn, hint]
    {
        const bool e = editBtn->isChecked(), p = playBtn->isChecked();
        sceneView->setModes (e, p);
        QSettings().setValue ("scenes/edit", e);
        QSettings().setValue ("scenes/play", p);
        hint->setText (! e && ! p ? tr ("PROTECTED: Edit and Play are off, nothing changes on click \u00B7 right-click a group = mute / solo")
                       : QString (p ? tr ("cell: click = play it \u00B7 scene: click = launch") : QString())
                         + (e && p ? QStringLiteral (" \u00B7 ") : QString())
                         + (e ? tr ("cell: click = on / off, right-click = choose \u00B7 lock = no more changes \u00B7 double-click = rename") : QString())
                         + tr (" \u00B7 wheel = more"));
    };
    connect (editBtn, &QPushButton::toggled, this, modes);
    connect (playBtn, &QPushButton::toggled, this, modes);
    sceneView->setRotated (rotate->isChecked());
    connect (rotate, &QPushButton::toggled, this, [this] (bool on)
    {
        sceneView->setRotated (on);
        QSettings().setValue ("scenes/rotated", on);
    });
    palette = new PaletteView (sequencer, state);
    auto* area = new QBoxLayout (QBoxLayout::LeftToRight);   // la palette a gauche de la grille
    area->addWidget (palette, 0, Qt::AlignTop);
    area->addWidget (sceneView, 1);
    col->addLayout (area, 1);
    auto placePalette = [this, area, rotate, paletteBtn]
    {
        // Toujours a gauche : en bas, elle prenait trop de hauteur aux groupes (Rotate)
        area->setDirection (QBoxLayout::LeftToRight);
        area->setAlignment (palette, Qt::AlignTop);
        palette->setVisible (paletteBtn->isChecked());
        Q_UNUSED (rotate);
    };
    paletteBtn->setChecked (QSettings().value ("scenes/palette", false).toBool());
    connect (paletteBtn, &QPushButton::toggled, this, [placePalette] (bool on) { QSettings().setValue ("scenes/palette", on); placePalette(); });
    connect (rotate, &QPushButton::toggled, this, [placePalette] { placePalette(); });
    placePalette();
    connect (palette, &PaletteView::squareMenu, this, [this, editBtn] (int i, const QPoint& at)
    {
        // Edit : Copy, Paste here, Duplicate, Delete (confirmation)... le menu de la barre du haut
        if (! editBtn->isChecked())
        {
            toast ("Turn Edit on to copy, duplicate or delete patterns");
            return;
        }
        patternBar->patternMenu (i, at);
        palette->update();
    });
    connect (palette, &PaletteView::playRequested, this, [this, playBtn] (int g, int i)
    {
        if (! playBtn->isChecked())
        {
            toast ("Turn Play on to play a pattern");
            return;
        }
        patternBar->playPattern (g, i);
    });
    connect (sceneView, &SceneView::muteSoloClicked, this, [this] (int g, bool solo)
    {
        auto& flag = solo ? state.groupSolo[(size_t) g] : state.groupMute[(size_t) g];
        flag = ! flag;
        refreshMuteSolo();
    });
    connect (sceneView, &SceneView::allOff, this, [this] (bool solo)
    {
        for (int g = 0; g < kGroups; ++g)
            (solo ? state.groupSolo : state.groupMute)[(size_t) g] = false;
        refreshMuteSolo();
    });
    connect (sceneView, &SceneView::beforeEdit, this, [this] (const QString& label) { rememberPatterns (-1, label); });
    modes();
    connect (sceneView, &SceneView::message, this, [this] (const QString& t) { toast (t); });
    connect (sceneView, &SceneView::groupMenu, this, [this] (int g, const QPoint& at)
    {
        // Clic droit sur un groupe (vue Scenes) : Mute / Solo, renommer
        QMenu menu (this);
        auto* mute = menu.addAction (state.groupMute[(size_t) g] ? tr ("Unmute %1").arg (groupTitle (state, g)) : tr ("Mute %1").arg (groupTitle (state, g)));
        auto* solo = menu.addAction (state.groupSolo[(size_t) g] ? tr ("Unsolo %1").arg (groupTitle (state, g)) : tr ("Solo %1").arg (groupTitle (state, g)));
        menu.addSeparator();
        auto* rename = menu.addAction (tr ("Rename Group %1...").arg (groupName (g)));
        auto* picked = menu.exec (at);
        if (picked == mute)
            state.groupMute[(size_t) g] = ! state.groupMute[(size_t) g];
        else if (picked == solo)
            state.groupSolo[(size_t) g] = ! state.groupSolo[(size_t) g];
        else if (picked == rename)
            renameGroup (g);
        if (picked == mute || picked == solo)
        {
            refreshMuteSolo();
            sceneView->update();
        }
    });
    connect (sceneView, &SceneView::sceneClicked, this, [this] (int i) { patternBar->chooseScene (i); });
    connect (sceneView, &SceneView::sceneMenu, this, [this] (int i, const QPoint& at) { patternBar->sceneMenu (i, at); });
    connect (sceneView, &SceneView::changed, this, [this] { patternBar->refresh (true); patternsChanged(); });
    connect (sceneView, &SceneView::groupChosen, this, [this] (int g) { if (g != state.group) selectGroup (g); });
    connect (sceneView, &SceneView::renameScene, this, [this] (int i) { patternBar->renameScene (i); });
    connect (sceneView, &SceneView::renamePattern, this, [this] (int g, int i) { patternBar->renamePattern (g, i); });
    return panel;
}

// ------------------------------------------------------------ groupe entier : exporter / importer

void MainWindow::renameGroup (int g)
{
    // La lettre reste (A, B... A2) ; on ajoute un nom court, lisible sur l'ecran de la Maschine
    bool ok = false;
    const QString text = QInputDialog::getText (this, tr ("Group name"),
                                                tr ("Name of group %1 (8 characters: A-Z, 0-9, -, space; empty = letter only):").arg (groupName (g)),
                                                QLineEdit::Normal, state.groupLabel[(size_t) g], &ok);
    if (! ok)
        return;
    QString clean;
    for (const QChar c : text.toUpper())             // les memes lettres que l'editeur de la Maschine
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == ' ')
            clean += c;
    clean = clean.simplified().left (8).trimmed();
    if (clean == state.groupLabel[(size_t) g])
        return;
    rememberSounds ({}, { g }, tr ("Rename group"));
    state.groupLabel[(size_t) g] = clean;
    refreshAll();
    refreshMuteSolo();
    if (patternBar)
        patternBar->refresh (true);
    if (sceneView)
        sceneView->update();
    maschine->updateScreens();
    toast (QString ("GROUP %1").arg (groupTitle (state, g)));
}

void MainWindow::exportGroup()
{
    // Fichier .fzgroup (JSON) : les 16 sons (samples, synthes, reglages), tous les patterns, le volume
    const int g = state.group;
    QSettings settings;
    const QString start = settings.value ("groupDir", Project::defaultDir()).toString() + "/Group_" + groupName (g) + "_.fzgroup";
    QString path = QFileDialog::getSaveFileName (this, tr ("Export Group %1").arg (groupName (g)), start, tr ("Fuzzy group (*.fzgroup)"));
    if (path.isEmpty())
        return;
    if (! path.endsWith (".fzgroup"))
        path += ".fzgroup";
    settings.setValue ("groupDir", QFileInfo (path).absolutePath());
    bool hasSamples = false;
    for (int p = 1; p <= 16; ++p)
        hasSamples |= ! state.samples[(size_t) state.index (g, p)].isEmpty();
    // Samples copies a cote du fichier (dossier « NOM samples ») : le groupe voyage complet
    const bool copy = hasSamples && QMessageBox::question (this, tr ("Export Group"),
        tr ("Copy the samples next to the file (folder \"%1 samples\")?\nThen the group can go to another computer.")
            .arg (QFileInfo (path).completeBaseName())) == QMessageBox::Yes;
    exportGroupTo (path, copy);
}

bool MainWindow::exportGroupTo (const QString& path, bool copy)
{
    const int g = state.group;
    const QString sampleDir = QFileInfo (path).completeBaseName() + " samples";
    QJsonObject sounds;
    int failed = 0;
    for (int p = 1; p <= 16; ++p)
    {
        const SoundCopy s = takeSound (g, p);
        QJsonObject sd { { "mode", s.mode }, { "release", s.release }, { "synth", s.synth }, { "volume", s.volume },
                         { "pan", s.pan }, { "pitch", s.pitch }, { "cutoff", s.cutoff }, { "reverse", s.reverse }, { "polyphony", s.poly },
                         { "loop", QJsonObject { { "start", s.loopStart }, { "end", s.loopEnd } } },
                         { "name", padName (g, p) } };
        if (! s.sample.isEmpty())
        {
            QString where = s.sample;
            if (copy)
            {
                const QString dir = QFileInfo (path).absolutePath() + "/" + sampleDir;
                QDir().mkpath (dir);
                const QString target = dir + "/" + QString ("%1-").arg (p, 2, 10, QChar ('0')) + QFileInfo (s.sample).fileName();
                QFile::remove (target);
                if (QFile::copy (s.sample, target))
                    where = sampleDir + "/" + QFileInfo (target).fileName();   // chemin relatif au fichier
                else
                    ++failed;
            }
            sd["sample"] = where;
        }
        sounds[QString::number (p)] = sd;
    }
    QJsonArray patterns;
    for (int idx = 0; idx < seq::kPatternSlots; ++idx)
    {
        if (! sequencer.hasPattern (g, idx))
            continue;
        const seq::Pattern pat = sequencer.patternAt (g, idx);
        QJsonArray cells;
        for (int r = 0; r < seq::kMaxRows; ++r)
            for (int t = 0; t < seq::kTracks; ++t)
                if (! pat.rows[(size_t) r][(size_t) t].empty())
                    cells.append (Project::cellToJson (r, t, pat.rows[(size_t) r][(size_t) t]));
        patterns.append (QJsonObject { { "index", idx }, { "name", QString::fromStdString (sequencer.patternName (g, idx)) },
                                       { "length", pat.length }, { "cells", cells } });
    }
    const QJsonObject doc { { "format", "fuzzy-group" }, { "version", 1 }, { "group", groupName (g) }, { "name", state.groupLabel[(size_t) g] },
                            { "volume", state.groupVolume[(size_t) g].load() }, { "current", sequencer.currentPattern (g) },
                            { "sounds", sounds }, { "patterns", patterns } };
    QSaveFile out (path);
    if (! out.open (QIODevice::WriteOnly) || out.write (QJsonDocument (doc).toJson (QJsonDocument::Indented)) < 0 || ! out.commit())
    {
        QMessageBox::warning (this, tr ("Export Group"), tr ("Cannot write %1").arg (path));
        return false;
    }
    if (failed)
        QMessageBox::warning (this, tr ("Export Group"), tr ("%1 sample(s) could not be copied (missing file?).").arg (failed));
    toast (QString ("GROUP %1 exported").arg (groupName (g)));
    return true;
}

void MainWindow::importGroup()
{
    // Remplace le groupe choisi par celui du fichier (sons, patterns, volume) ; Cmd+Z annule les sons
    const int g = state.group;
    QSettings settings;
    const QString path = QFileDialog::getOpenFileName (this, tr ("Import Group into %1").arg (groupName (g)),
                                                       settings.value ("groupDir", Project::defaultDir()).toString(),
                                                       tr ("Fuzzy group (*.fzgroup)"));
    if (path.isEmpty())
        return;
    settings.setValue ("groupDir", QFileInfo (path).absolutePath());
    QFile f (path);
    const QJsonObject doc = f.open (QIODevice::ReadOnly) ? QJsonDocument::fromJson (f.readAll()).object() : QJsonObject();
    if (doc["format"].toString() != "fuzzy-group")
    {
        QMessageBox::warning (this, tr ("Import Group"), tr ("%1 is not a Fuzzy group file.").arg (QFileInfo (path).fileName()));
        return;
    }
    if (QMessageBox::question (this, tr ("Import Group"),
            tr ("Replace group %1 with \"%2\"?\nIts 16 sounds, all its patterns and its volume are replaced.")
                .arg (groupName (g), QFileInfo (path).completeBaseName())) != QMessageBox::Yes)
        return;
    importGroupFrom (path, doc);
}

void MainWindow::importGroupFrom (const QString& path, const QJsonObject& doc)
{
    const int g = state.group;
    rememberSounds ({}, { g }, "Import group");
    const QDir base = QFileInfo (path).absoluteDir();
    const QJsonObject sounds = doc["sounds"].toObject();
    int missing = 0;
    for (int p = 1; p <= 16; ++p)
    {
        const QJsonObject sd = sounds[QString::number (p)].toObject();
        SoundCopy s;
        s.valid = true;
        s.mode = std::clamp (sd["mode"].toInt (session::State::defaultMode (p)), 0, 2);
        s.release = std::clamp (sd["release"].toInt (150), 5, 5000);
        s.synth = std::clamp (sd["synth"].toInt (-1), -1, 5);
        s.volume = std::clamp (sd["volume"].toInt (100), 0, 127);
        s.pan = std::clamp (sd["pan"].toInt (64), 0, 127);
        s.pitch = std::clamp (sd["pitch"].toInt (64), 0, 127);
        s.cutoff = std::clamp (sd["cutoff"].toInt (90), 0, 127);
        s.reverse = sd["reverse"].toBool();
        s.poly = std::clamp (sd["polyphony"].toInt (4), 1, 16);
        s.loopStart = (float) std::clamp (sd["loop"].toObject()["start"].toDouble (0), 0.0, 1.0);
        s.loopEnd = (float) std::clamp (sd["loop"].toObject()["end"].toDouble (1), 0.0, 1.0);
        QString sample = sd["sample"].toString();
        if (! sample.isEmpty() && QFileInfo (sample).isRelative())
            sample = base.filePath (sample);          // samples copies a cote du fichier
        if (! sample.isEmpty() && ! QFileInfo::exists (sample))
            ++missing;
        s.sample = sample;
        putSound (g, p, s);
    }
    // Patterns : ceux du fichier remplacent tous ceux du groupe
    seq::GroupPatterns gp;
    gp.current = std::clamp (doc["current"].toInt (0), 0, seq::kPatternSlots - 1);
    gp.off = sequencer.groupOff (g);
    for (const auto& pv : doc["patterns"].toArray())
    {
        const QJsonObject pd = pv.toObject();
        const int idx = pd["index"].toInt (-1);
        if (idx < 0 || idx >= seq::kPatternSlots)
            continue;
        seq::Pattern pat;
        pat.length = std::clamp (pd["length"].toInt (16), 1, seq::kMaxRows);
        for (const auto& cv : pd["cells"].toArray())
        {
            int row = 0, track = 0;
            seq::Cell c;
            if (Project::cellFromJson (cv.toArray(), row, track, c))
                pat.rows[(size_t) row][(size_t) track] = c;
        }
        gp.patterns.push_back ({ idx, pat });
        if (! pd["name"].toString().isEmpty())
            gp.names.push_back ({ idx, pd["name"].toString().toStdString() });
    }
    sequencer.setGroupPatterns (g, gp);
    state.groupVolume[(size_t) g] = std::clamp (doc["volume"].toInt (100), 0, 127);
    state.groupLabel[(size_t) g] = doc["name"].toString().trimmed().left (8).toUpper();
    syncSamples();
    patternsChanged();
    refreshMuteSolo();
    if (missing)
        QMessageBox::warning (this, tr ("Import Group"), tr ("%1 sample(s) not found: those pads stay without sample.").arg (missing));
    toast (QString ("GROUP %1 imported").arg (groupName (g)));
}

// ------------------------------------------------------------ gestionnaire de patterns (groupe choisi)

void MainWindow::openPatternManager()
{
    // Une fenetre (non bloquante) : les 64 cases de patterns du groupe choisi, et des actions
    if (patternManagerDialog)
    {
        patternManagerDialog->show();
        patternManagerDialog->raise();
        patternManagerDialog->activateWindow();
        return;
    }
    auto* box = new QDialog (this);
    patternManagerDialog = box;
    box->setAttribute (Qt::WA_DeleteOnClose);
    box->resize (720, 520);
    auto* col = new QVBoxLayout (box);
    auto* title = new QLabel;
    title->setObjectName ("section");
    col->addWidget (title);
    auto* table = new QTableWidget (seq::kPatternSlots, 6);
    table->setHorizontalHeaderLabels ({ tr ("#"), tr ("Name"), tr ("Lines"), tr ("Notes"), tr ("In scenes"), tr ("Now") });
    table->verticalHeader()->hide();
    table->setSelectionBehavior (QAbstractItemView::SelectRows);
    table->setSelectionMode (QAbstractItemView::SingleSelection);
    table->setEditTriggers (QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setSectionResizeMode (QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode (4, QHeaderView::Stretch);
    col->addWidget (table, 1);
    auto* row = new QHBoxLayout;
    auto button = [&] (const QString& text, const QString& tip)
    {
        auto* b = new QPushButton (text);
        b->setFocusPolicy (Qt::NoFocus);
        b->setToolTip (tip);
        row->addWidget (b);
        return b;
    };
    auto* play = button (tr ("Play"), tr ("Play this pattern (at the end of the bar if playing)"));
    auto* rename = button (tr ("Rename"), tr ("Short name (8 characters, as on the Maschine)"));
    auto* copy = button (tr ("Copy"), tr ("Copy this pattern"));
    auto* paste = button (tr ("Paste"), tr ("Paste the copied pattern into this slot"));
    auto* dup = button (tr ("Duplicate"), tr ("Copy into the first free slot"));
    auto* twice = button (tr ("Double length"), tr ("Length x 2, notes copied into the new half"));
    auto* erase = button (tr ("Delete"), tr ("Delete this pattern (the playing one is only cleared)"));
    auto* scenes = button (tr ("Scenes..."), tr ("Tick the scenes that play this pattern for this group"));
    col->addLayout (row);
    auto* hint = new QLabel (tr ("Group chosen in the app · double-click = play · Cmd+Z undoes every change"));
    hint->setObjectName ("hint");
    col->addWidget (hint);

    auto chosen = [table] { return table->currentRow(); };
    auto usedIn = [this] (int g, int idx)            // les scenes ou ce pattern joue pour ce groupe
    {
        QStringList out;
        for (int i = 0; i < seq::kPatternSlots; ++i)
        {
            const seq::Scene sc = sequencer.scene (i);
            if (sc.used && sc.pattern[(size_t) g] == idx)
                out << QString ("S%1").arg (i + 1) + (sc.name.empty() ? QString() : " " + QString::fromStdString (sc.name));
        }
        return out;
    };
    auto refresh = [this, box, title, table, play, rename, copy, paste, dup, twice, erase, scenes, chosen, usedIn]
    {
        const int g = state.group;
        box->setWindowTitle (tr ("Patterns of group %1").arg (groupTitle (state, g)));
        title->setText (tr ("PATTERNS  %1").arg (groupTitle (state, g)));
        title->setStyleSheet (QString ("QLabel { color: %1; }").arg (css (groupColor (g))));
        const int cur = sequencer.currentPattern (g);
        for (int i = 0; i < seq::kPatternSlots; ++i)
        {
            const bool exists = sequencer.hasPattern (g, i);
            const seq::Pattern pat = sequencer.patternAt (g, i);
            int notes = 0;
            for (int r = 0; exists && r < pat.length; ++r)
                for (const auto& c : pat.rows[(size_t) r])
                    notes += c.empty() ? 0 : 1;
            QString name = QString::fromStdString (sequencer.patternName (g, i));
            if (exists && name.isEmpty())
                name = QString ("Pat%1").arg (i + 1);
            const QStringList cells { QString::number (i + 1), exists ? name : tr ("(empty)"),
                                      exists ? QString::number (pat.length) : QString(), exists ? QString::number (notes) : QString(),
                                      exists ? usedIn (g, i).join (", ") : QString(),
                                      exists && i == cur ? (sequencer.groupOff (g) ? tr ("chosen (off)") : QStringLiteral ("▶")) : QString() };
            for (int c = 0; c < cells.size(); ++c)
            {
                auto* item = table->item (i, c);
                if (! item)
                    table->setItem (i, c, item = new QTableWidgetItem);
                item->setText (cells[c]);
                if (exists)                          // couleur par defaut : lisible aussi quand la ligne est choisie
                    item->setData (Qt::ForegroundRole, QVariant());
                else
                    item->setForeground (QColor (90, 90, 100));
            }
        }
        const int i = chosen();
        const bool exists = i >= 0 && sequencer.hasPattern (g, i);
        for (auto* b : { play, rename, copy, dup, twice, erase, scenes })
            b->setEnabled (exists);
        const bool listening = transport->playing() && state.audition.load() == g;
        play->setText (listening ? tr ("Stop") : tr ("Play"));
        play->setEnabled (exists || listening);
        paste->setEnabled (i >= 0 && patternBar->clipPatternFull);
    };
    patternManagerRefresh = refresh;
    connect (box, &QObject::destroyed, this, [this] { patternManagerRefresh = nullptr; patternManagerDialog = nullptr; });
    connect (table, &QTableWidget::currentCellChanged, box, [refresh] { refresh(); });
    auto done = [this] (const QString& msg)          // apres une action : toutes les vues suivent
    {
        patternBar->refresh (true);
        patternsChanged();
        if (! msg.isEmpty())
            toast (msg);
    };
    auto playIt = [this, chosen, done]
    {
        const int g = state.group, i = chosen();
        if (transport->playing() && state.audition.load() == g)   // en ecoute : Stop
        {
            transport->stop();
            return;
        }
        if (i < 0 || ! sequencer.hasPattern (g, i))
            return;
        patternBar->playPattern (g, i);              // allume + lance la lecture si arretee
        done ({});
    };
    connect (play, &QPushButton::clicked, box, playIt);
    connect (table, &QTableWidget::cellDoubleClicked, box, playIt);
    connect (rename, &QPushButton::clicked, box, [this, chosen, done]
    {
        const int i = chosen();
        if (i >= 0)
            patternBar->renamePattern (state.group, i);
        done ({});
    });
    connect (copy, &QPushButton::clicked, box, [this, chosen, refresh]
    {
        const int g = state.group, i = chosen();
        if (i < 0 || ! sequencer.hasPattern (g, i))
            return;
        patternBar->clipPattern = sequencer.patternAt (g, i);
        patternBar->clipPatternName = sequencer.patternName (g, i);
        patternBar->clipPatternFull = true;
        toast (QString ("PATTERN %1 copied").arg (i + 1));
        refresh();
    });
    connect (paste, &QPushButton::clicked, box, [this, chosen, done]
    {
        const int g = state.group, i = chosen();
        if (i < 0 || ! patternBar->clipPatternFull)
            return;
        rememberPatterns (g, tr ("Paste pattern"));
        sequencer.setPatternAt (g, i, patternBar->clipPattern);
        sequencer.setPatternName (g, i, patternBar->clipPatternName);
        done (QString ("PASTE > PATTERN %1").arg (i + 1));
    });
    connect (dup, &QPushButton::clicked, box, [this, chosen, done, table]
    {
        const int g = state.group, i = chosen();
        if (i < 0 || ! sequencer.hasPattern (g, i))
            return;
        int to = -1;
        for (int k = 1; k < seq::kPatternSlots && to < 0; ++k)
            if (! sequencer.hasPattern (g, (i + k) % seq::kPatternSlots))
                to = (i + k) % seq::kPatternSlots;
        if (to < 0)
        {
            toast ("No free pattern slot");
            return;
        }
        rememberPatterns (g, tr ("Duplicate pattern"));
        sequencer.setPatternAt (g, to, sequencer.patternAt (g, i));
        sequencer.setPatternName (g, to, sequencer.patternName (g, i));
        done (QString ("PATTERN %1 > %2").arg (i + 1).arg (to + 1));
        table->setCurrentCell (to, 0);
    });
    connect (twice, &QPushButton::clicked, box, [this, chosen, done]
    {
        const int g = state.group, i = chosen();
        if (i < 0 || ! sequencer.hasPattern (g, i))
            return;
        const seq::Pattern p = sequencer.patternAt (g, i);
        if (p.length * 2 > seq::kMaxRows)
        {
            toast (QString ("Too long (max %1 lines)").arg (seq::kMaxRows));
            return;
        }
        rememberPatterns (g, tr ("Double pattern"));
        seq::Pattern d = p;
        for (int r = 0; r < p.length; ++r)
            d.rows[(size_t) (r + p.length)] = p.rows[(size_t) r];
        d.length = p.length * 2;
        sequencer.setPatternAt (g, i, d);
        done (QString ("PATTERN %1: %2 lines").arg (i + 1).arg (d.length));
    });
    connect (erase, &QPushButton::clicked, box, [this, box, chosen, done, usedIn]
    {
        const int g = state.group, i = chosen();
        if (i < 0 || ! sequencer.hasPattern (g, i))
            return;
        for (int k = 0; k < seq::kPatternSlots; ++k)   // une scene verrouillee le joue : on le garde
        {
            const seq::Scene sc = sequencer.scene (k);
            if (sc.used && sc.locked && sc.pattern[(size_t) g] == i)
            {
                toast (QString ("Pattern %1 is used by locked scene S%2").arg (i + 1).arg (k + 1));
                return;
            }
        }
        const QStringList in = usedIn (g, i);
        if (! in.isEmpty() && QMessageBox::question (box, tr ("Delete pattern"),
                                                     tr ("Pattern %1 is used in %2.\nDelete it anyway? (those scenes will play nothing for this group)")
                                                         .arg (i + 1).arg (in.join (", "))) != QMessageBox::Yes)
            return;
        rememberPatterns (g, tr ("Delete pattern"));
        for (int k = 0; k < seq::kPatternSlots; ++k)   // les scenes qui le jouaient : groupe eteint
        {
            seq::Scene sc = sequencer.scene (k);
            if (sc.used && sc.pattern[(size_t) g] == i && ! sc.locked)
            {
                sc.pattern[(size_t) g] = seq::kOff;
                sequencer.setScene (k, sc);
            }
        }
        sequencer.removePattern (g, i);
        done (QString ("PATTERN %1 deleted").arg (i + 1));
    });
    connect (scenes, &QPushButton::clicked, box, [this, box, chosen, done]
    {
        // Les scenes ou ce pattern joue (pour ce groupe) : tout cocher d'un coup, puis OK
        const int g = state.group, i = chosen();
        if (i < 0 || ! sequencer.hasPattern (g, i))
            return;
        QDialog pick (box);
        pick.setWindowTitle (tr ("Pattern %1 in scenes").arg (i + 1));
        auto* v = new QVBoxLayout (&pick);
        v->addWidget (new QLabel (tr ("Group %1, pattern %2: tick the scenes that play it\n(untick = the group plays nothing in that scene)")
                                      .arg (groupTitle (state, g)).arg (PatternBar::patternName (sequencer, g, i))));
        auto* list = new QListWidget;
        for (int k = 0; k < seq::kPatternSlots; ++k)
        {
            const seq::Scene sc = sequencer.scene (k);
            if (! sc.used)
                continue;
            auto* item = new QListWidgetItem (QString ("S%1 %2%3").arg (k + 1).arg (QString::fromStdString (sc.name))
                                                  .arg (sc.locked ? QString ("  (locked)") : QString()));
            item->setData (Qt::UserRole, k);
            item->setFlags (sc.locked ? Qt::ItemIsUserCheckable : Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
            item->setCheckState (sc.pattern[(size_t) g] == i ? Qt::Checked : Qt::Unchecked);
            list->addItem (item);
        }
        if (list->count() == 0)
        {
            toast ("No scene yet");
            return;
        }
        v->addWidget (list, 1);
        auto* buttons = new QDialogButtonBox (QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect (buttons, &QDialogButtonBox::accepted, &pick, &QDialog::accept);
        connect (buttons, &QDialogButtonBox::rejected, &pick, &QDialog::reject);
        v->addWidget (buttons);
        pick.resize (360, 420);
        if (pick.exec() != QDialog::Accepted)
            return;
        rememberPatterns (g, tr ("Pattern in scenes"));
        int changedCount = 0;
        for (int r = 0; r < list->count(); ++r)
        {
            const int k = list->item (r)->data (Qt::UserRole).toInt();
            seq::Scene sc = sequencer.scene (k);
            if (sc.locked)
                continue;
            const bool on = list->item (r)->checkState() == Qt::Checked;
            const int want = on ? i : (sc.pattern[(size_t) g] == i ? (int) seq::kOff : sc.pattern[(size_t) g]);
            if (want != sc.pattern[(size_t) g])
            {
                sc.pattern[(size_t) g] = want;
                sequencer.setScene (k, sc);
                ++changedCount;
            }
        }
        done (QString ("PATTERN %1: %2 scene(s) changed").arg (i + 1).arg (changedCount));
    });
    table->setCurrentCell (std::max (0, sequencer.currentPattern (state.group)), 0);
    refresh();
    box->show();
}

// ------------------------------------------------------------ liste des scenes (vue Scenes > List)

void MainWindow::openSceneList()
{
    // L'inverse de la liste des patterns : chaque scene et ses patterns (S1 - AP1, BP3, CP1...)
    if (sceneListDialog)
    {
        sceneListDialog->show();
        sceneListDialog->raise();
        sceneListDialog->activateWindow();
        return;
    }
    auto* box = new QDialog (this);
    sceneListDialog = box;
    box->setAttribute (Qt::WA_DeleteOnClose);
    box->setWindowTitle (tr ("Scenes"));
    box->resize (760, 520);
    auto* col = new QVBoxLayout (box);
    auto* title = new QLabel (tr ("SCENES"));
    title->setObjectName ("section");
    col->addWidget (title);
    auto* table = new QTableWidget (seq::kPatternSlots, 4);
    table->setHorizontalHeaderLabels ({ tr ("Scene"), tr ("Name"), tr ("Patterns (group + pattern)"), tr ("Lock") });
    table->verticalHeader()->hide();
    table->setSelectionBehavior (QAbstractItemView::SelectRows);
    table->setSelectionMode (QAbstractItemView::SingleSelection);
    table->setEditTriggers (QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setSectionResizeMode (QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode (2, QHeaderView::Stretch);
    col->addWidget (table, 1);
    auto* row = new QHBoxLayout;
    auto button = [&] (const QString& text, const QString& tip)
    {
        auto* b = new QPushButton (text);
        b->setFocusPolicy (Qt::NoFocus);
        b->setToolTip (tip);
        row->addWidget (b);
        return b;
    };
    auto* launch = button (tr ("Launch"), tr ("Launch this scene (at the end of the bar if playing)"));
    auto* rename = button (tr ("Rename"), tr ("Short name (8 characters)"));
    auto* copy = button (tr ("Copy"), tr ("Copy this scene"));
    auto* paste = button (tr ("Paste"), tr ("Paste the copied scene into this slot"));
    auto* lock = button (tr ("Lock"), tr ("Lock / unlock: a locked scene launches but no longer changes"));
    auto* erase = button (tr ("Delete"), tr ("Delete this scene"));
    col->addLayout (row);
    auto* hint = new QLabel (tr ("AP1 = group A, pattern 1 · A~ = group A free (keeps its pattern) · double-click = launch · Cmd+Z undoes"));
    hint->setObjectName ("hint");
    col->addWidget (hint);

    auto chosen = [table] { return table->currentRow(); };
    auto refresh = [this, table, launch, rename, copy, paste, lock, erase, chosen]
    {
        const int now = sequencer.currentScene();
        for (int i = 0; i < seq::kPatternSlots; ++i)
        {
            const seq::Scene sc = sequencer.scene (i);
            QStringList parts;
            for (int g = 0; sc.used && g < kGroups; ++g)
            {
                const int v = sc.pattern[(size_t) g];
                if (v >= 0)
                    parts << groupName (g) + QString ("P%1").arg (v + 1);
                else if (v == seq::kFree)
                    parts << groupName (g) + "~";
            }
            const QStringList cells { QString ("S%1").arg (i + 1) + (i == now ? QStringLiteral (" ▶") : QString()),
                                      sc.used ? QString::fromStdString (sc.name) : tr ("(empty)"),
                                      sc.used ? (parts.isEmpty() ? tr ("(nothing plays)") : parts.join (", ")) : QString(),
                                      sc.used && sc.locked ? tr ("locked") : QString() };
            for (int c = 0; c < cells.size(); ++c)
            {
                auto* item = table->item (i, c);
                if (! item)
                    table->setItem (i, c, item = new QTableWidgetItem);
                item->setText (cells[c]);
                if (sc.used)
                    item->setData (Qt::ForegroundRole, QVariant());   // lisible aussi sur la ligne choisie
                else
                    item->setForeground (QColor (90, 90, 100));
            }
        }
        const int i = chosen();
        const seq::Scene sc = i >= 0 ? sequencer.scene (i) : seq::Scene();
        launch->setText (transport->playing() ? tr ("Stop") : tr ("Launch"));
        launch->setEnabled (sc.used || transport->playing());
        copy->setEnabled (sc.used);
        lock->setEnabled (sc.used);
        lock->setText (sc.locked ? tr ("Unlock") : tr ("Lock"));
        rename->setEnabled (sc.used && ! sc.locked);
        erase->setEnabled (sc.used && ! sc.locked);
        paste->setEnabled (i >= 0 && patternBar->clipSceneFull && ! sc.locked);
    };
    sceneListRefresh = refresh;
    connect (box, &QObject::destroyed, this, [this] { sceneListRefresh = nullptr; sceneListDialog = nullptr; });
    connect (table, &QTableWidget::currentCellChanged, box, [refresh] { refresh(); });
    auto done = [this] (const QString& msg)
    {
        patternBar->refresh (true);
        patternsChanged();
        if (! msg.isEmpty())
            toast (msg);
    };
    auto launchIt = [this, chosen, done]
    {
        if (transport->playing())                    // en lecture : Stop
        {
            transport->stop();
            return;
        }
        const int i = chosen();
        if (i < 0 || ! sequencer.scene (i).used)
            return;
        patternBar->chooseScene (i);
        if (! transport->playing())
            transport->play();
        done ({});
    };
    connect (launch, &QPushButton::clicked, box, launchIt);
    connect (table, &QTableWidget::cellDoubleClicked, box, launchIt);
    connect (rename, &QPushButton::clicked, box, [this, chosen, done]
    {
        if (chosen() >= 0)
            patternBar->renameScene (chosen());
        done ({});
    });
    connect (copy, &QPushButton::clicked, box, [this, chosen, refresh]
    {
        const int i = chosen();
        if (i < 0 || ! sequencer.scene (i).used)
            return;
        patternBar->clipScene = sequencer.scene (i);
        patternBar->clipScene.locked = false;
        patternBar->clipSceneFull = true;
        toast (QString ("SCENE %1 copied").arg (i + 1));
        refresh();
    });
    connect (paste, &QPushButton::clicked, box, [this, chosen, done]
    {
        const int i = chosen();
        if (i < 0 || ! patternBar->clipSceneFull || sequencer.scene (i).locked)
            return;
        rememberPatterns (-1, tr ("Paste scene"));
        sequencer.setScene (i, patternBar->clipScene);
        done (QString ("PASTE > SCENE %1").arg (i + 1));
    });
    connect (lock, &QPushButton::clicked, box, [this, chosen, done]
    {
        const int i = chosen();
        seq::Scene sc = i >= 0 ? sequencer.scene (i) : seq::Scene();
        if (! sc.used)
            return;
        rememberPatterns (-1, tr ("Lock scene"));
        sc.locked = ! sc.locked;
        sequencer.setScene (i, sc);
        done (QString ("SCENE %1 %2").arg (i + 1).arg (sc.locked ? "LOCKED" : "UNLOCKED"));
    });
    connect (erase, &QPushButton::clicked, box, [this, chosen, done]
    {
        const int i = chosen();
        if (i < 0 || ! sequencer.scene (i).used || sequencer.scene (i).locked)
            return;
        rememberPatterns (-1, tr ("Delete scene"));
        sequencer.setScene (i, seq::Scene());
        if (sequencer.currentScene() == i)
            sequencer.setCurrentScene (-1);
        done (QString ("SCENE %1 deleted").arg (i + 1));
    });
    table->setCurrentCell (std::max (0, sequencer.currentScene()), 0);
    refresh();
    box->show();
}

// ------------------------------------------------------------ accords User (mode Chords)

void MainWindow::chordPadMenu (int pad, const QPoint& at)
{
    // Clic droit sur un pad en mode Chords : mettre cet accord dans un set User ;
    // dans un set User : copier, coller, effacer l'accord du pad
    const int key = padChordKey (pad);
    const QList<int> notes = key > 0 ? music::chordFor (key, keySettings) : QList<int>();
    const int base = 12 * (state.octave + 1) + keySettings.root;   // comme le pilote
    QMenu menu (this);
    auto* title = menu.addAction (tr ("Pad %1: %2").arg (pad).arg (notes.isEmpty() ? QString ("-") : padChordLabel (pad)));
    title->setEnabled (false);
    menu.addSeparator();
    auto* add = menu.addMenu (tr ("Add this chord to User..."));
    add->setEnabled (! notes.isEmpty());
    for (int b = 0; b < music::kUserBanks; ++b)
    {
        auto* bank = add->addMenu (QString ("User %1").arg (b + 1));
        for (int c = 0; c < 12; ++c)
        {
            const auto& existing = music::userChords()[(size_t) b][(size_t) c];
            QList<int> ex;
            for (int i : existing)
                ex << base + i;
            auto* a = bank->addAction (QString ("Pad %1  %2").arg (c + 1).arg (existing.empty() ? tr ("(empty)") : music::chordName (ex)));
            connect (a, &QAction::triggered, this, [this, b, c, notes, base]
            {
                std::vector<int> intervals;
                for (int n : notes)
                    intervals.push_back (n - base);
                std::sort (intervals.begin(), intervals.end());
                music::userChords()[(size_t) b][(size_t) c] = intervals;
                music::saveUserChords();
                toast (QString ("USER %1 PAD %2 = %3").arg (b + 1).arg (c + 1).arg (music::chordName (QList<int> (notes.begin(), notes.end()))));
                refreshAll();
            });
        }
    }
    const bool user = keySettings.chordMode == 4 && pad <= 12;
    QAction *copy = nullptr, *paste = nullptr, *erase = nullptr;
    if (user)
    {
        menu.addSeparator();
        auto& slot = music::userChords()[(size_t) keySettings.userBank][(size_t) pad - 1];
        copy = menu.addAction (tr ("Copy chord"));
        copy->setEnabled (! slot.empty());
        paste = menu.addAction (tr ("Paste chord here"));
        paste->setEnabled (chordClipFull);
        erase = menu.addAction (tr ("Delete chord"));
        erase->setEnabled (! slot.empty());
    }
    auto* picked = menu.exec (at);
    if (! picked || ! user)
        return;
    auto& slot = music::userChords()[(size_t) keySettings.userBank][(size_t) pad - 1];
    if (picked == copy)
    {
        chordClip = slot;
        chordClipFull = true;
        toast (QString ("CHORD %1 copied").arg (pad));
        return;
    }
    if (picked == paste)
        slot = chordClip;
    else if (picked == erase)
        slot.clear();
    else
        return;
    music::saveUserChords();
    refreshAll();
}

void MainWindow::chordDropMenu (int fp, int tp, const QPoint& at)
{
    // Set User : pad lache sur un pad = Copy / Move / Swap des ACCORDS (jamais des sons)
    if (fp > 12 || tp > 12 || fp == tp)
        return;
    auto& bank = music::userChords()[(size_t) keySettings.userBank];
    QMenu menu (this);
    auto* title = menu.addAction (tr ("User %1: chord %2 > %3").arg (keySettings.userBank + 1).arg (fp).arg (tp));
    title->setEnabled (false);
    menu.addSeparator();
    auto* copy = menu.addAction (tr ("Copy chord here"));
    auto* move = menu.addAction (tr ("Move chord here"));
    auto* swap = menu.addAction (tr ("Swap chords"));
    auto* picked = menu.exec (at);
    auto& a = bank[(size_t) fp - 1];
    auto& b = bank[(size_t) tp - 1];
    if (picked == copy)
        b = a;
    else if (picked == move)
    {
        b = a;
        a.clear();
    }
    else if (picked == swap)
        std::swap (a, b);
    else
        return;
    music::saveUserChords();
    toast (QString ("CHORD %1 %2 %3").arg (fp).arg (picked == swap ? "<>" : ">").arg (tp));
    refreshAll();
}

// ------------------------------------------------------------ vue Song (morceau)

QWidget* MainWindow::buildSong()
{
    // Le morceau : des scenes jouees dans l'ordre (comme le mode ARRANGER du pilote)
    auto* panel = new QWidget;
    auto* col = new QVBoxLayout (panel);
    col->setContentsMargins (6, 4, 6, 4);
    auto* head = new QHBoxLayout;
    auto* title = new QLabel (tr ("SONG"));
    title->setObjectName ("section");
    head->addWidget (title);
    auto button = [&] (const QString& text, const QString& tip, bool checkable = false)
    {
        auto* b = new QPushButton (text);
        b->setFocusPolicy (Qt::NoFocus);
        b->setToolTip (tip);
        b->setCheckable (checkable);
        if (checkable)
            b->setObjectName ("keys");
        head->addWidget (b);
        return b;
    };
    // Plusieurs songs (versions) : la liste + son menu
    auto* songList = new QComboBox;
    songList->setFocusPolicy (Qt::NoFocus);
    songList->setMinimumWidth (130);
    songList->setToolTip (tr ("The song that plays (and that the Maschine sees)"));
    head->addWidget (songList);
    auto* songsMenuBtn = button (tr ("Songs \u25BE"), tr ("New, duplicate (alternative version), rename, delete a song"));
    songButton = button (tr ("SONG"), tr ("Song mode: PLAY follows the song (off = patterns and scenes by hand)"), true);
    loopButton = button (tr ("Loop"), tr ("At the end of the song, start again (off = stop)"), true);
    auto* from = button (tr ("▶ From here"), tr ("Play the song from the chosen block"));
    auto* add = button (tr ("+ Scene"), tr ("Add a scene after the chosen block (or drag a scene name from the Scenes view)"));
    auto* dup = button (tr ("Duplicate"), tr ("Copy the chosen block just after it"));
    auto* del = button (tr ("Delete"), tr ("Remove the chosen block from the song"));
    auto* stepBox = new QComboBox;
    stepBox->addItems ({ tr ("Step 1 bar"), tr ("Step 1/2"), tr ("Step 1/3"), tr ("Step 1/4") });
    stepBox->setToolTip (tr ("Length step when dragging the right edge of a block (1/3 bar is rounded to 5 or 6 lines)"));
    stepBox->setFocusPolicy (Qt::NoFocus);
    head->addWidget (stepBox);
    auto* followBtn = button (tr ("Follow"), tr ("The view follows the playhead"), true);
    head->addStretch();
    auto* foot = new QHBoxLayout;                    // en bas : l'aide, puis les zooms
    // Zoom H / V : un curseur chacun, comme dans le Piano Roll
    QSlider *zoomH = nullptr, *zoomV = nullptr;
    auto addZooms = [&]
    {
        auto* box = new QWidget;
        auto* zr = new QHBoxLayout (box);
        zr->setContentsMargins (0, 0, 0, 0);
        zr->setSpacing (6);
        auto* zl = new QLabel (tr ("Zoom  H"));
        zl->setObjectName ("hint");
        auto* vl = new QLabel (tr ("V"));
        vl->setObjectName ("hint");
        zoomH = new QSlider (Qt::Horizontal);
        zoomV = new QSlider (Qt::Horizontal);
        zoomH->setRange (16, 320);
        zoomV->setRange (16, 64);
        for (auto* z : { zoomH, zoomV })
        {
            z->setFixedWidth (110);
            z->setFocusPolicy (Qt::NoFocus);
        }
        zoomH->setToolTip (tr ("Horizontal zoom: bar width (Cmd + wheel)"));
        zoomV->setToolTip (tr ("Vertical zoom: height of the group rows"));
        zr->addWidget (zl);
        zr->addWidget (zoomH);
        zr->addSpacing (8);
        zr->addWidget (vl);
        zr->addWidget (zoomV);
        box->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Preferred);
        foot->addWidget (box);
    };
    col->addLayout (head);
    songView = new SongView (sequencer, state);
    songView->isPlaying = [this] { return transport->playing(); };
    // Liste des scenes, a gauche : on les glisse dans la timeline (double-clic = ajouter)
    struct SceneDragList : QListWidget
    {
        using QListWidget::QListWidget;
        QMimeData* mimeData (const QList<QListWidgetItem*>& items) const override
        {
            auto* m = new QMimeData;
            if (! items.isEmpty())
                m->setData ("application/x-fuzzy-scene", QByteArray::number (items.first()->data (Qt::UserRole).toInt()));
            return m;
        }
        QStringList mimeTypes() const override { return { "application/x-fuzzy-scene" }; }
    };
    songSceneList = new SceneDragList;
    songSceneList->setDragEnabled (true);
    songSceneList->setDragDropMode (QAbstractItemView::DragOnly);
    songSceneList->setFixedWidth (150);
    songSceneList->setToolTip (tr ("Drag a scene into the song (double-click = add after the chosen block)"));
    songSceneListRefresh = [this]
    {
        songSceneList->clear();
        for (int i = 0; i < seq::kPatternSlots; ++i)
        {
            const seq::Scene sc = sequencer.scene (i);
            if (! sc.used)
                continue;
            QPixmap swatch (10, 18);
            swatch.fill (seq::sceneColor (i, sc.color));
            auto* item = new QListWidgetItem (QIcon (swatch), QString ("S%1 %2").arg (i + 1).arg (QString::fromStdString (sc.name)));
            item->setData (Qt::UserRole, i);
            songSceneList->addItem (item);
        }
        if (songSceneList->count() == 0)
        {
            auto* item = new QListWidgetItem (tr ("(no scene yet)"));
            item->setFlags (Qt::NoItemFlags);
            songSceneList->addItem (item);
        }
    };
    songSceneListRefresh();
    connect (songSceneList, &QListWidget::itemDoubleClicked, this, [this] (QListWidgetItem* item)
    {
        if (item->data (Qt::UserRole).isValid())
            addSongScene (item->data (Qt::UserRole).toInt(), -1);
    });
    auto* songScroll = new QScrollArea;              // les lignes des groupes defilent en hauteur
    songScroll->setWidget (songView);
    songScroll->setWidgetResizable (true);
    songScroll->setHorizontalScrollBarPolicy (Qt::ScrollBarAlwaysOff);   // en largeur : la molette
    songScroll->setFrameShape (QFrame::NoFrame);
    auto* songArea = new QHBoxLayout;                // la liste des scenes, puis la timeline
    songArea->addWidget (songSceneList);
    songArea->addWidget (songScroll, 1);
    col->addLayout (songArea, 1);
    auto* hint = new QLabel (tr ("click a group cell = its pattern in the scene \u00B7 drag on the ruler = loop zone \u00B7 drag a block = move · drag its right edge = length · double-click = scene · "
                                 "right-click = repeats, length, duplicate, delete · click the ruler = play from there · 1 bar = 16 lines"));
    hint->setObjectName ("hint");
    registerHint (hint, "SONG");
    hint->setWordWrap (true);
    hint->setSizePolicy (QSizePolicy::Ignored, QSizePolicy::Preferred);   // peut retrecir
    hint->setMinimumWidth (0);
    foot->addWidget (hint, 1);
    foot->addStretch (1);                            // l'aide est dans la ligne du bas : zooms a droite
    addZooms();
    col->addLayout (foot);

    songButton->setChecked (sequencer.songMode());
    loopButton->setChecked (sequencer.songLoop());
    connect (songButton, &QPushButton::clicked, this, [this] (bool on) { setSongMode (on); });
    connect (loopButton, &QPushButton::clicked, this, [this] (bool on)
    {
        sequencer.setSongLoop (on);
        toast (on ? "SONG LOOP ON" : "SONG LOOP OFF");
    });
    connect (stepBox, &QComboBox::currentIndexChanged, this, [this] (int i)
    {
        songView->step = i == 0 ? 1.0 : i == 1 ? 0.5 : i == 2 ? 1.0 / 3.0 : 0.25;
    });
    followBtn->setChecked (QSettings().value ("song/follow", false).toBool());
    songView->follow = followBtn->isChecked();
    connect (followBtn, &QPushButton::toggled, this, [this] (bool on)
    {
        songView->follow = on;
        QSettings().setValue ("song/follow", on);
    });
    connect (songView, &SongView::loopRegion, this, [this] (int a, int b)
    {
        sequencer.setSongLoopRange (a, b);
        if (a >= 0)
            toast (QString ("LOOP BLOCKS %1 - %2%3").arg (a + 1).arg (b + 1).arg (sequencer.songLoop() ? "" : " (turn Loop on)"));
        songView->update();
    });
    connect (songView, &SongView::cellClicked, this, &MainWindow::songCellMenu);
    zoomH->setValue (songView->barWidth());
    zoomV->setValue (songView->laneHeight());
    connect (zoomH, &QSlider::valueChanged, this, [this] (int v) { songView->setBarWidth (v); });
    connect (zoomV, &QSlider::valueChanged, this, [this] (int v) { songView->setLaneHeight (v); });
    connect (songView, &SongView::zoomChanged, this, [this, zoomH]
    {
        const QSignalBlocker block (zoomH);
        zoomH->setValue (songView->barWidth());
    });
    // Songs : liste a jour, choisir, menu
    songsRefresh = [this, songList]
    {
        const QSignalBlocker block (songList);
        songList->clear();
        const auto all = sequencer.allSongs();
        for (const auto& sl : all)
            songList->addItem (QString::fromStdString (sl.name));
        songList->setCurrentIndex (sequencer.currentSong());
        if (loopButton)
            loopButton->setChecked (sequencer.songLoop());
        songView->selectOnly (0);
    };
    connect (songList, &QComboBox::currentIndexChanged, this, [this] (int i)
    {
        if (i < 0 || i == sequencer.currentSong())
            return;
        rememberPatterns (-1, tr ("Choose song"));
        sequencer.switchSong (i);
        songsRefresh();
        toast (QString ("SONG: %1").arg (QString::fromStdString (sequencer.allSongs()[(size_t) i].name)));
        refreshMaschine();
    });
    connect (songsMenuBtn, &QPushButton::clicked, this, [this, songsMenuBtn]
    {
        auto all = sequencer.allSongs();
        const int cur = sequencer.currentSong();
        QMenu menu (this);
        auto* fresh = menu.addAction (tr ("New song (empty)"));
        auto* dupS = menu.addAction (tr ("Duplicate this song (alternative version)"));
        auto* ren = menu.addAction (tr ("Rename this song..."));
        menu.addSeparator();
        auto* delS = menu.addAction (tr ("Delete this song..."));
        fresh->setEnabled ((int) all.size() < seq::kSongsMax);
        dupS->setEnabled ((int) all.size() < seq::kSongsMax);
        delS->setEnabled (all.size() > 1);
        auto* picked = menu.exec (songsMenuBtn->mapToGlobal (QPoint (0, songsMenuBtn->height())));
        if (! picked)
            return;
        auto askName = [this] (const QString& title, const QString& start, QString& out)
        {
            bool ok = false;
            out = QInputDialog::getText (this, title, tr ("Song name:"), QLineEdit::Normal, start, &ok).trimmed().left (24);
            return ok && ! out.isEmpty();
        };
        QString name;
        if (picked == fresh || picked == dupS)
        {
            if (! askName (picked == fresh ? tr ("New song") : tr ("Duplicate song"),
                           picked == fresh ? QString ("Song %1").arg (all.size() + 1)
                                           : QString::fromStdString (all[(size_t) cur].name) + " B", name))
                return;
            rememberPatterns (-1, tr ("New song"));
            seq::SongSlot sl = picked == dupS ? all[(size_t) cur] : seq::SongSlot();
            sl.name = name.toStdString();
            all.push_back (sl);
            const bool mode = sequencer.songMode();
            sequencer.setAllSongs (all, (int) all.size() - 1);
            sequencer.setSongMode (mode);
        }
        else if (picked == ren)
        {
            if (! askName (tr ("Rename song"), QString::fromStdString (all[(size_t) cur].name), name))
                return;
            rememberPatterns (-1, tr ("Rename song"));
            all[(size_t) cur].name = name.toStdString();
            const bool mode = sequencer.songMode();
            sequencer.setAllSongs (all, cur);
            sequencer.setSongMode (mode);
        }
        else if (picked == delS)
        {
            QMessageBox box (QMessageBox::Question, tr ("Delete song"),
                             tr ("Delete the song \"%1\"?").arg (QString::fromStdString (all[(size_t) cur].name)),
                             QMessageBox::Yes | QMessageBox::Cancel, this);
            box.setDefaultButton (QMessageBox::Cancel);
            if (box.exec() != QMessageBox::Yes)
                return;
            rememberPatterns (-1, tr ("Delete song"));
            all.erase (all.begin() + cur);
            const bool mode = sequencer.songMode();
            sequencer.setAllSongs (all, std::max (0, cur - 1));
            sequencer.setSongMode (mode);
        }
        songsRefresh();
        refreshMaschine();
    });
    songsRefresh();
    connect (songView, &SongView::rulerMenu, this, [this] (const QPoint& at)
    {
        QMenu menu (this);
        int la = -1, lb = -1;
        sequencer.songLoopRange (la, lb);
        auto* clear = menu.addAction (tr ("Clear loop zone"));
        clear->setEnabled (la >= 0);
        if (menu.exec (at) == clear)
        {
            sequencer.setSongLoopRange (-1, -1);
            songView->update();
            toast ("LOOP ZONE CLEARED");
        }
    });
    connect (from, &QPushButton::clicked, this, [this]
    {
        if (sequencer.song().empty())
            return;
        sequencer.songStartFrom (songView->selected);
        setSongMode (true);
        if (! transport->playing())
            transport->play();
    });
    connect (add, &QPushButton::clicked, this, [this, add]
    {
        songSceneMenu (add->mapToGlobal (QPoint (0, add->height())), [this] (int scene) { addSongScene (scene, -1); });
    });
    connect (dup, &QPushButton::clicked, this, [this] { songDuplicateBlocks(); });
    connect (del, &QPushButton::clicked, this, [this] { songDeleteBlocks(); });
    connect (songView, &SongView::startFrom, this, [this] (int i)
    {
        songView->selectOnly (i);
        const bool playing = transport->playing() && sequencer.songMode();
        sequencer.songStartFrom (i, playing);        // en lecture : a la fin de la mesure, sans a-coup
        toast (QString ("SONG FROM BLOCK %1%2").arg (i + 1).arg (playing ? " (next bar)" : ""));
    });
    connect (songView, &SongView::moveEntries, this, [this] (const QList<int>& list, int before, bool copy)
    {
        // Deplacer (ou copier, Option) plusieurs blocs : ils arrivent ensemble, dans leur ordre
        if (list.isEmpty())
            return;
        int at = before;
        songEdit (copy ? tr ("Copy blocks") : tr ("Move blocks"), [list, &at, copy] (std::vector<seq::SongEntry>& s)
        {
            std::vector<seq::SongEntry> moved;
            for (int i : list)
                moved.push_back (s[(size_t) i]);
            if (! copy)
            {
                for (int k = (int) list.size() - 1; k >= 0; --k)
                {
                    if (list[k] < at)
                        --at;
                    s.erase (s.begin() + list[k]);
                }
            }
            at = std::clamp (at, 0, (int) s.size());
            if ((int) (s.size() + moved.size()) > seq::kSongMax)
                moved.resize ((size_t) std::max (0, seq::kSongMax - (int) s.size()));
            s.insert (s.begin() + at, moved.begin(), moved.end());
        });
        songView->chosen.clear();
        for (int k = 0; k < list.size(); ++k)
            songView->chosen.insert (at + k);
        songView->selected = at;
        songView->update();
    });
    connect (songView, &SongView::laneMuteSolo, this, [this] (int g, bool solo)
    {
        auto& flag = solo ? state.groupSolo[(size_t) g] : state.groupMute[(size_t) g];
        flag = ! flag;
        refreshMuteSolo();
        songView->update();
    });
    songView->bpm = [this] { return bpmBox ? (double) bpmBox->value() : 120.0; };
    connect (songView, &SongView::lengthChanged, this, [this] (int i, double bars)
    {
        songEdit (tr ("Block length"), [i, bars] (std::vector<seq::SongEntry>& s)
        {
            if (i >= 0 && i < (int) s.size())
                s[(size_t) i].bars = bars;           // longueur fixe, en mesures
        });
        toast (QString ("BLOCK %1: %2 BARS").arg (i + 1).arg (bars));
    });
    connect (songView, &SongView::insertScene, this, [this] (int scene, int before) { addSongScene (scene, before); });
    connect (songView, &SongView::chooseScene, this, [this] (int i, const QPoint& at)
    {
        songSceneMenu (at, [this, i] (int scene)
        {
            songEdit (tr ("Block scene"), [i, scene] (std::vector<seq::SongEntry>& s)
            {
                if (i >= 0 && i < (int) s.size())
                    s[(size_t) i].scene = scene;
            });
        });
    });
    connect (songView, &SongView::emptyMenu, this, [this] (const QPoint& at)
    {
        songSceneMenu (at, [this] (int scene) { addSongScene (scene, (int) sequencer.song().size()); });
    });
    connect (songView, &SongView::entryMenu, this, [this] (int i, const QPoint& at)
    {
        const auto song = sequencer.song();
        if (i < 0 || i >= (int) song.size())
            return;
        const seq::SongEntry e = song[(size_t) i];
        QMenu menu (this);
        auto* t = menu.addAction (QString ("Block %1: S%2").arg (i + 1).arg (e.scene + 1));
        t->setEnabled (false);
        menu.addSeparator();
        auto* reps = menu.addMenu (tr ("Repeats (length follows the scene)"));
        for (double r : { 0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 6.0, 8.0, 12.0, 16.0 })
        {
            auto* a = reps->addAction (QString::fromUtf8 ("×%1").arg (r));
            a->setCheckable (true);
            a->setChecked (e.bars <= 0 && e.repeats == r);
            connect (a, &QAction::triggered, this, [this, i, r]
            {
                songEdit (tr ("Block repeats"), [i, r] (std::vector<seq::SongEntry>& s)
                {
                    s[(size_t) i].repeats = r;
                    s[(size_t) i].bars = 0;          // longueur automatique
                });
            });
        }
        auto* lengths = menu.addMenu (tr ("Fixed length (bars)"));
        for (double b : { 0.5, 1.0, 2.0, 3.0, 4.0, 6.0, 8.0, 12.0, 16.0, 32.0 })
        {
            auto* a = lengths->addAction (QString ("%1").arg (b));
            a->setCheckable (true);
            a->setChecked (e.bars == b);
            connect (a, &QAction::triggered, this, [this, i, b]
            {
                songEdit (tr ("Block length"), [i, b] (std::vector<seq::SongEntry>& s) { s[(size_t) i].bars = b; });
            });
        }
        auto* autoLen = menu.addAction (tr ("Length auto (scene x repeats)"));
        autoLen->setEnabled (e.bars > 0);
        menu.addSeparator();
        auto* change = menu.addAction (tr ("Change scene..."));
        auto* copyScene = menu.addAction (tr ("New scene from this one (copy)"));
        auto* renameScene = menu.addAction (tr ("Rename scene S%1...").arg (e.scene + 1));
        sceneColorMenu (menu.addMenu (tr ("Scene S%1 color").arg (e.scene + 1)), e.scene);
        auto* before = menu.addAction (tr ("Insert a scene before..."));
        auto* dupA = menu.addAction (tr ("Duplicate"));
        auto* play = menu.addAction (tr ("Play from here"));
        menu.addSeparator();
        auto* delA = menu.addAction (tr ("Delete"));
        auto* picked = menu.exec (at);
        if (picked == autoLen)
            songEdit (tr ("Block length"), [i] (std::vector<seq::SongEntry>& s) { s[(size_t) i].bars = 0; });
        else if (picked == change)
            emit songView->chooseScene (i, at);
        else if (picked == renameScene)
        {
            patternBar->renameScene (e.scene);       // verrouillee : refuse
            patternsChanged();
            songView->update();
        }
        else if (picked == copyScene)
        {
            const int to = freeScene();
            if (to < 0)
            {
                toast ("No free scene slot");
                return;
            }
            rememberPatterns (-1, tr ("New scene"));
            seq::Scene sc = sequencer.scene (e.scene);
            sc.locked = false;
            sequencer.setScene (to, sc);
            std::vector<seq::SongEntry> s = sequencer.song();
            s[(size_t) i].scene = to;
            sequencer.setSong (s);
            patternBar->refresh (true);
            patternsChanged();
            songView->update();
            toast (QString ("BLOCK %1 = NEW SCENE S%2 (copy of S%3)").arg (i + 1).arg (to + 1).arg (e.scene + 1));
        }
        else if (picked == before)
            songSceneMenu (at, [this, i] (int scene) { addSongScene (scene, i); });
        else if (picked == dupA)
            songDuplicateBlocks();
        else if (picked == play)
        {
            sequencer.songStartFrom (i);
            setSongMode (true);
            if (! transport->playing())
                transport->play();
        }
        else if (picked == delA)
            songDeleteBlocks();
    });
    connect (transport, &Transport::row, songView, [this] { songView->followPlayhead(); songView->update(); });
    return panel;
}

void MainWindow::songEdit (const QString& label, const std::function<void (std::vector<seq::SongEntry>&)>& change)
{
    // Toute modification du morceau passe ici : Cmd+Z la defait
    rememberPatterns (-1, label);
    std::vector<seq::SongEntry> s = sequencer.song();
    change (s);
    sequencer.setSong (s);
    if (songView)
    {
        songView->selected = std::clamp (songView->selected, 0, std::max (0, (int) s.size() - 1));
        songView->update();
    }
    refreshMaschine();
}

void MainWindow::setSongMode (bool on)
{
    sequencer.setSongMode (on);
    for (auto* b : { songButton, songTransportButton })
        if (b)
        {
            const QSignalBlocker block (b);
            b->setChecked (on);
        }
    if (songView)
        songView->update();
    toast (on ? "SONG MODE ON: PLAY follows the song" : "SONG MODE OFF");
    refreshMaschine();
}

void MainWindow::addSongScene (int scene, int before)
{
    if (scene < 0 || scene >= seq::kPatternSlots || ! sequencer.scene (scene).used)
    {
        toast ("Empty scene (make it in Scenes)");
        return;
    }
    if ((int) sequencer.song().size() >= seq::kSongMax)
    {
        toast (QString ("%1 blocks maximum").arg (seq::kSongMax));
        return;
    }
    const int n = (int) sequencer.song().size();
    const int at = before >= 0 ? std::clamp (before, 0, n) : (n == 0 ? 0 : std::clamp (songView->selected + 1, 0, n));
    songEdit (tr ("Add scene to song"), [scene, at] (std::vector<seq::SongEntry>& s)
    {
        seq::SongEntry e;
        e.scene = scene;
        s.insert (s.begin() + at, e);
    });
    songView->selectOnly (at);
    toast (QString ("SONG + S%1").arg (scene + 1));
}

void MainWindow::songSceneMenu (const QPoint& at, const std::function<void (int)>& picked)
{
    // Nouvelle scene (vide, ou ce qui joue), puis les scenes faites (nom et ce qu'elles jouent)
    QMenu menu (this);
    auto* empty = menu.addAction (tr ("New empty scene"));
    auto* playing = menu.addAction (tr ("New scene from what's playing"));
    connect (empty, &QAction::triggered, this, [this, picked] { const int i = newScene (false); if (i >= 0) picked (i); });
    connect (playing, &QAction::triggered, this, [this, picked] { const int i = newScene (true); if (i >= 0) picked (i); });
    menu.addSeparator();
    int count = 0;
    for (int i = 0; i < seq::kPatternSlots; ++i)
    {
        const seq::Scene sc = sequencer.scene (i);
        if (! sc.used)
            continue;
        QStringList parts;
        for (int g = 0; g < kGroups; ++g)
            if (sc.pattern[(size_t) g] >= 0)
                parts << groupName (g) + QString ("P%1").arg (sc.pattern[(size_t) g] + 1);
        auto* a = menu.addAction (QString ("S%1 %2   %3").arg (i + 1).arg (QString::fromStdString (sc.name)).arg (parts.join (", ")));
        connect (a, &QAction::triggered, this, [picked, i] { picked (i); });
        ++count;
    }
    Q_UNUSED (count);
    menu.exec (at);
}

// ------------------------------------------------------------ Maschine en mode ARRANGER (vue Song)

bool MainWindow::arrangerButton (const QString& name)
{
    // Comme le pilote : ecran 5 SONG, 6 GO, 7 LOOP, 3 HALF ; ERASE / DUPLICATE sur le bloc ;
    // < > : banque des scenes des pads
    const int sel = songView->selected;
    const int n = (int) sequencer.song().size();
    if (name == "screen_5")
        setSongMode (! sequencer.songMode());
    else if (name == "screen_6" && n > 0)            // GO : jouer depuis le bloc choisi
    {
        sequencer.songStartFrom (sel);
        setSongMode (true);
        if (! transport->playing())
            transport->play();
    }
    else if (name == "screen_7")
    {
        sequencer.setSongLoop (! sequencer.songLoop());
        if (loopButton)
            loopButton->setChecked (sequencer.songLoop());
        toast (sequencer.songLoop() ? "SONG LOOP ON" : "SONG LOOP OFF");
    }
    else if (name == "screen_3")
    {
        songHalf = ! songHalf;
        toast (songHalf ? "HALF: by 0.5" : "HALF off: by 1");
    }
    else if (name == "erase" && n > 0)
        songEdit (tr ("Delete block"), [sel] (std::vector<seq::SongEntry>& s) { s.erase (s.begin() + sel); });
    else if (name == "duplicate" && n > 0 && ! maschine->property ("shiftHeld").toBool())
    {
        songEdit (tr ("Duplicate block"), [sel] (std::vector<seq::SongEntry>& s)
        {
            if ((int) s.size() < seq::kSongMax)
                s.insert (s.begin() + sel + 1, s[(size_t) sel]);
        });
        songView->selectOnly (sel + 1);
    }
    else if (name == "nav_prev" || name == "nav_next")
    {
        patternBar->sceneBank = std::clamp (patternBar->sceneBank + (name == "nav_next" ? 1 : -1), 0, 3);
        patternBar->refresh (true);
        toast (QString ("SCENES BANK %1").arg (patternBar->sceneBank + 1));
    }
    else
        return false;
    songView->update();
    refreshMaschine();
    return true;
}

void MainWindow::arrangerKnob (int index, int delta)
{
    // 1 bloc choisi, 2 sa scene, 3 repetitions, 4 longueur en mesures (pas a pas, comme le pilote)
    int& acc = songKnobAcc[(size_t) index];
    acc += delta;
    const int steps = acc / 8;
    acc %= 8;
    const auto song = sequencer.song();
    if (! steps || song.empty())
        return;
    const int sel = std::clamp (songView->selected, 0, (int) song.size() - 1);
    const double unit = songHalf ? 0.5 : 1;
    if (index == 0)
    {
        songView->selectOnly (std::clamp (sel + steps, 0, (int) song.size() - 1));
        toast (QString ("BLOCK %1  S%2").arg (songView->selected + 1).arg (song[(size_t) songView->selected].scene + 1));
    }
    else if (index == 1)                             // scene suivante / precedente (faites seulement)
    {
        int sc = song[(size_t) sel].scene;
        for (int k = 0; k < std::abs (steps); ++k)
            for (int t = 1; t <= seq::kPatternSlots; ++t)
            {
                const int c = ((sc + (steps > 0 ? t : -t)) % seq::kPatternSlots + seq::kPatternSlots) % seq::kPatternSlots;
                if (sequencer.scene (c).used)
                {
                    sc = c;
                    break;
                }
            }
        songEdit (tr ("Block scene"), [sel, sc] (std::vector<seq::SongEntry>& s) { s[(size_t) sel].scene = sc; });
        toast (QString ("BLOCK %1 = S%2").arg (sel + 1).arg (sc + 1));
    }
    else if (index == 2)
    {
        const double r = std::clamp (song[(size_t) sel].repeats + steps * unit, 0.5, 16.0);
        songEdit (tr ("Block repeats"), [sel, r] (std::vector<seq::SongEntry>& s) { s[(size_t) sel].repeats = r; s[(size_t) sel].bars = 0; });
        toast (QString::fromUtf8 ("BLOCK %1 ×%2").arg (sel + 1).arg (r));
    }
    else
    {
        const seq::SongEntry e = song[(size_t) sel];
        const double now = e.bars > 0 ? e.bars : sequencer.entryRows (e) / double (seq::kRowsPerBar);
        const double b = std::clamp (std::round ((now + steps * unit) * 2) / 2, 0.5, 64.0);
        songEdit (tr ("Block length"), [sel, b] (std::vector<seq::SongEntry>& s) { s[(size_t) sel].bars = b; });
        toast (QString ("BLOCK %1: %2 BARS").arg (sel + 1).arg (b));
    }
    songView->update();
}

int MainWindow::freeScene() const
{
    for (int i = 0; i < seq::kPatternSlots; ++i)
        if (! sequencer.scene (i).used)
            return i;
    return -1;
}

int MainWindow::newScene (bool fromPlaying)
{
    const int i = freeScene();
    if (i < 0)
    {
        toast ("No free scene slot");
        return -1;
    }
    rememberPatterns (-1, tr ("New scene"));
    seq::Scene sc;
    sc.used = true;
    if (fromPlaying)                                 // ce qui joue maintenant (eteint = Off)
        for (int g = 0; g < kGroups; ++g)
            sc.pattern[(size_t) g] = sequencer.groupOff (g) ? (int) seq::kOff : sequencer.currentPattern (g);
    else
        sc.pattern.fill (seq::kOff);
    sequencer.setScene (i, sc);
    patternBar->refresh (true);
    patternsChanged();
    toast (QString ("NEW SCENE S%1").arg (i + 1));
    return i;
}

void MainWindow::songCellMenu (int entry, int g, const QPoint& at)
{
    // Case groupe x bloc de la vue Song : le pattern de ce groupe dans la scene du bloc.
    // La scene peut servir a plusieurs blocs : « Only this block » en fait une copie pour ce bloc.
    const auto song = sequencer.song();
    if (entry < 0 || entry >= (int) song.size())
        return;
    const int sc = song[(size_t) entry].scene;
    const seq::Scene scene = sequencer.scene (sc);
    if (! scene.used)
    {
        toast ("Empty scene");
        return;
    }
    QStringList users;
    for (int i = 0; i < (int) song.size(); ++i)
        if (song[(size_t) i].scene == sc)
            users << QString::number (i + 1);
    constexpr int kNew = -100;
    auto apply = [this, entry, g, sc] (int value, bool onlyThis)
    {
        seq::Scene target = sequencer.scene (sc);
        int where = sc;
        std::vector<seq::SongEntry> s = sequencer.song();
        int usersCount = 0;
        for (const auto& e : s)
            usersCount += e.scene == sc ? 1 : 0;
        if (target.locked && ! onlyThis)
        {
            toast (QString ("SCENE %1 is locked (use \"Only this block\")").arg (sc + 1));
            return;
        }
        rememberPatterns (g, tr ("Pattern in scene"));
        if (value == kNew)                           // nouveau pattern vide dans le groupe
        {
            value = -1;
            for (int k = 0; k < seq::kPatternSlots && value < 0; ++k)
                if (! sequencer.hasPattern (g, k))
                    value = k;
            if (value < 0)
            {
                toast ("No free pattern slot");
                return;
            }
            seq::Pattern p;
            p.length = std::min (seq::kMaxRows, sequencer.sceneRows (sc));   // la longueur de la scene
            sequencer.setPatternAt (g, value, p);
        }
        if (onlyThis && (usersCount > 1 || target.locked))   // une copie de la scene, pour ce bloc seul
        {
            where = freeScene();
            if (where < 0)
            {
                toast ("No free scene slot");
                return;
            }
            target.locked = false;
            s[(size_t) entry].scene = where;
            sequencer.setSong (s);
        }
        target.pattern[(size_t) g] = value;
        sequencer.setScene (where, target);
        patternBar->refresh (true);
        patternsChanged();
        songView->update();
        toast (QString ("S%1 %2 = %3").arg (where + 1).arg (groupName (g))
                   .arg (value >= 0 ? PatternBar::patternName (sequencer, g, value) : value == seq::kFree ? QString ("free") : QString ("off")));
    };
    QMenu menu (this);
    auto* title = menu.addAction (tr ("Block %1 · S%2 · group %3").arg (entry + 1).arg (sc + 1).arg (groupTitle (state, g)));
    title->setEnabled (false);
    if (users.size() > 1)
    {
        auto* note = menu.addAction (tr ("S%1 is used by blocks %2: changes apply to all").arg (sc + 1).arg (users.join (", ")));
        note->setEnabled (false);
    }
    if (scene.locked)
    {
        auto* note = menu.addAction (tr ("S%1 is locked: use \"Only this block\"").arg (sc + 1));
        note->setEnabled (false);
    }
    menu.addSeparator();
    auto fill = [&] (QMenu* m, bool onlyThis)
    {
        const int now = scene.pattern[(size_t) g];
        for (int k = 0; k < seq::kPatternSlots; ++k)
            if (sequencer.hasPattern (g, k))
            {
                auto* a = m->addAction (groupName (g) + QString ("P%1  %2").arg (k + 1).arg (PatternBar::patternName (sequencer, g, k)));
                a->setCheckable (true);
                a->setChecked (now == k);
                a->setEnabled (onlyThis || ! scene.locked);
                connect (a, &QAction::triggered, this, [apply, k, onlyThis] { apply (k, onlyThis); });
            }
        m->addSeparator();
        auto* fresh = m->addAction (tr ("New pattern (empty)"));
        fresh->setEnabled (onlyThis || ! scene.locked);
        connect (fresh, &QAction::triggered, this, [apply, onlyThis] { apply (kNew, onlyThis); });
        auto* off = m->addAction (tr ("Off (remove from the scene)"));
        off->setCheckable (true);
        off->setChecked (now == seq::kOff);
        off->setEnabled (onlyThis || ! scene.locked);
        connect (off, &QAction::triggered, this, [apply, onlyThis] { apply (seq::kOff, onlyThis); });
        auto* free = m->addAction (tr ("Free (the group keeps its pattern)"));
        free->setCheckable (true);
        free->setChecked (now == seq::kFree);
        free->setEnabled (onlyThis || ! scene.locked);
        connect (free, &QAction::triggered, this, [apply, onlyThis] { apply (seq::kFree, onlyThis); });
    };
    fill (&menu, false);
    if (scene.pattern[(size_t) g] >= 0)               // renommer le pattern de cette case
    {
        const int pi = scene.pattern[(size_t) g];
        menu.addSeparator();
        connect (menu.addAction (tr ("Rename pattern %1...").arg (PatternBar::patternName (sequencer, g, pi))), &QAction::triggered, this,
                 [this, g, pi]
        {
            patternBar->renamePattern (g, pi);
            patternsChanged();
            songView->update();
        });
    }
    if (users.size() > 1 || scene.locked)
    {
        menu.addSeparator();
        fill (menu.addMenu (tr ("Only this block (new scene)...")), true);
    }
    menu.exec (at);
}

// ------------------------------------------------------------ ligne d'aide

void MainWindow::registerHint (QLabel* hint, const QString& view)
{
    // Les aides des vues ne prennent plus de place dans les barres : elles vont dans la ligne d'aide
    hint->hide();
    helps.push_back ({ hint, view });
}

void MainWindow::showHelpFor (QWidget* w)
{
    auto dockOf = [] (QWidget* x) -> QWidget*
    {
        while (x && ! qobject_cast<QDockWidget*> (x))
            x = x->parentWidget();
        return x;
    };
    QWidget* dock = dockOf (w);
    if (! dock)
        return;
    for (const auto& h : helps)
    {
        if (! h.label || dockOf (h.label) != dock || ! h.label->parentWidget() || ! h.label->parentWidget()->isVisible())
            continue;
        // « action = effet · ... » : la vue en blanc, les actions en orange pale, le reste en gris clair
        QStringList parts;
        for (QString part : h.label->text().split (QChar (0x00B7), Qt::SkipEmptyParts))
        {
            part = part.trimmed().toHtmlEscaped();
            const int eq = (int) part.indexOf (" = ");
            parts << (eq > 0 ? QString ("<span style='color:#f5c9a0'>%1</span> = %2").arg (part.left (eq), part.mid (eq + 3)) : part);
        }
        helpLine->setText (QString ("<b style='color:#ffffff'>%1</b>&nbsp;&nbsp;&nbsp;%2")
                               .arg (h.view.toHtmlEscaped(), parts.join (" <span style='color:#66666e'>&middot;</span> ")));
        return;
    }
}

void MainWindow::showShortcuts()
{
    // Help > Keyboard shortcuts : la liste, en un coup d'oeil
#ifdef Q_OS_MACOS
    const QString mod = "Cmd";
#else
    const QString mod = "Ctrl";
#endif
    QMessageBox box (this);
    box.setWindowTitle (tr ("Keyboard shortcuts"));
    box.setTextFormat (Qt::RichText);
    box.setText (QString ("<table cellspacing=4>"
        "<tr><td><b>Space</b></td><td>Play / pause</td></tr>"
        "<tr><td><b>%1+Z / %1+Shift+Z</b></td><td>Undo / redo</td></tr>"
        "<tr><td><b>%1+C / X / V</b></td><td>Copy / cut / paste (pads, groups, notes: last clicked area)</td></tr>"
        "<tr><td><b>%1+S / %1+O / %1+N</b></td><td>Save / open / new project</td></tr>"
        "<tr><td><b>%1+,</b></td><td>Settings</td></tr>"
        "<tr><td><b>%1+1 / %1+2</b></td><td>Show Pads / Groups</td></tr>"
        "<tr><td><b>%1+I</b></td><td>Status window</td></tr>"
        "<tr><td><b>%1+U</b></td><td>Sound tab</td></tr>"
        "<tr><td><b>F12</b></td><td>Display diagnostics</td></tr>"
        "<tr><td><b>W / X (Z / X)</b></td><td>Octave - / + (PC keys)</td></tr>"
        "<tr><td><b>C / V</b></td><td>Velocity - / + (PC keys)</td></tr>"
        "<tr><td><b>Enter</b></td><td>Edit a cell (Pattern, Tracker)</td></tr>"
        "<tr><td><b>Del</b></td><td>Erase the selection</td></tr>"
        "<tr><td><b>%1 + wheel</b></td><td>Zoom (Song) / value (Tracker)</td></tr>"
        "</table>").arg (mod));
    box.exec();
}

// ------------------------------------------------------------ Song : blocs choisis, clavier, couleurs

void MainWindow::songDuplicateBlocks()
{
    const QList<int> list = songView->chosenList();
    if (list.isEmpty())
        return;
    const int at = list.last() + 1;
    songEdit (tr ("Duplicate blocks"), [list, at] (std::vector<seq::SongEntry>& s)
    {
        std::vector<seq::SongEntry> copy;
        for (int i : list)
            copy.push_back (s[(size_t) i]);
        if ((int) (s.size() + copy.size()) > seq::kSongMax)
            copy.resize ((size_t) std::max (0, seq::kSongMax - (int) s.size()));
        s.insert (s.begin() + std::min (at, (int) s.size()), copy.begin(), copy.end());
    });
    songView->chosen.clear();
    for (int k = 0; k < list.size(); ++k)
        songView->chosen.insert (at + k);
    songView->selected = at;
    songView->update();
}

void MainWindow::songDeleteBlocks()
{
    const QList<int> list = songView->chosenList();
    if (list.isEmpty())
        return;
    songEdit (list.size() > 1 ? tr ("Delete blocks") : tr ("Delete block"), [list] (std::vector<seq::SongEntry>& s)
    {
        for (int k = (int) list.size() - 1; k >= 0; --k)
            if (list[k] < (int) s.size())
                s.erase (s.begin() + list[k]);
    });
    songView->selectOnly (std::max (0, list.first() - 1));
    toast (QString ("%1 BLOCK(S) DELETED").arg (list.size()));
}

void MainWindow::songCopyBlocks (bool cut)
{
    const auto song = sequencer.song();
    songClip.clear();
    for (int i : songView->chosenList())
        songClip.push_back (song[(size_t) i]);
    toast (QString ("%1 BLOCK(S) %2").arg (songClip.size()).arg (cut ? "CUT" : "COPIED"));
    if (cut)
        songDeleteBlocks();
}

void MainWindow::songPasteBlocks()
{
    if (songClip.empty())
        return;
    const int n = (int) sequencer.song().size();
    const int at = n == 0 ? 0 : std::clamp (songView->selected + 1, 0, n);
    const auto clip = songClip;
    songEdit (tr ("Paste blocks"), [clip, at] (std::vector<seq::SongEntry>& s)
    {
        auto add = clip;
        if ((int) (s.size() + add.size()) > seq::kSongMax)
            add.resize ((size_t) std::max (0, seq::kSongMax - (int) s.size()));
        s.insert (s.begin() + std::min (at, (int) s.size()), add.begin(), add.end());
    });
    songView->chosen.clear();
    for (int k = 0; k < (int) clip.size(); ++k)
        songView->chosen.insert (at + k);
    songView->selected = at;
    songView->update();
}

bool MainWindow::songKey (QKeyEvent* e)
{
    // La Song a la main (derniere zone cliquee, vue montree) : Del, fleches, Entree
    const int n = (int) sequencer.song().size();
    switch (e->key())
    {
        case Qt::Key_Delete:
        case Qt::Key_Backspace:
            songDeleteBlocks();
            return true;
        case Qt::Key_Left:
        case Qt::Key_Right:
            if (n > 0)
            {
                songView->selectOnly (std::clamp (songView->selected + (e->key() == Qt::Key_Right ? 1 : -1), 0, n - 1));
                emit songView->selectedChanged (songView->selected);
            }
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (n > 0)
            {
                sequencer.songStartFrom (songView->selected, transport->playing() && sequencer.songMode());
                setSongMode (true);
                if (! transport->playing())
                    transport->play();
            }
            return true;
        default:
            return false;
    }
}

void MainWindow::sceneColorMenu (QMenu* menu, int scene)
{
    // 12 teintes + automatique ; s'applique aux vues Song et Scenes
    const seq::Scene sc = sequencer.scene (scene);
    auto* autoA = menu->addAction (tr ("Automatic"));
    autoA->setCheckable (true);
    autoA->setChecked (sc.color < 0);
    connect (autoA, &QAction::triggered, this, [this, scene]
    {
        seq::Scene s = sequencer.scene (scene);
        rememberPatterns (-1, tr ("Scene color"));
        s.color = -1;
        sequencer.setScene (scene, s);
        patternsChanged();
        songView->update();
    });
    static const char* names[12] { "Red", "Orange", "Yellow", "Lime", "Green", "Teal", "Cyan", "Sky", "Blue", "Violet", "Magenta", "Pink" };
    for (int k = 0; k < 12; ++k)
    {
        const int hue = k * 30;
        QPixmap swatch (14, 14);
        swatch.fill (seq::sceneColor (0, hue));
        auto* a = menu->addAction (QIcon (swatch), names[k]);
        a->setCheckable (true);
        a->setChecked (sc.color == hue);
        connect (a, &QAction::triggered, this, [this, scene, hue]
        {
            seq::Scene s = sequencer.scene (scene);
            rememberPatterns (-1, tr ("Scene color"));
            s.color = hue;
            sequencer.setScene (scene, s);
            patternsChanged();
            songView->update();
        });
    }
}
