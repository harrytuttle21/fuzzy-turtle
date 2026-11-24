// Fuzzy John DAW Pulses — fenetre principale (squelette Qt). Licence : GPLv3.
#include "main_window.h"

#include "engine_link.h"
#include "maschine_link.h"
#include "mixer_view.h"
#include "pattern_view.h"
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
#include <QWidgetAction>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QFileInfo>
#include <QMessageBox>
#include <QDir>
#include <QStackedWidget>
#include <QStatusBar>
#include <QVBoxLayout>

#include <cstring>

using namespace session;
namespace fc = fuzzy::control;

namespace {
constexpr int kLayoutVersion = 2;                  // disposition des panneaux (2 : Pattern et Mixer en panneaux)

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

    // Tout est en panneaux (pas de zone centrale fixe) : Pattern et Mixer en onglets
    setDockOptions (AnimatedDocks | AllowNestedDocks | AllowTabbedDocks);
    setTabPosition (Qt::AllDockWidgetAreas, QTabWidget::North);
    patternDock = makeDock (tr ("Pattern"), "patternDock", buildLeftPanel());
    mixerDock = makeDock (tr ("Mixer"), "mixerDock", buildMixer());
    addDockWidget (Qt::LeftDockWidgetArea, patternDock);
    tabifyDockWidget (patternDock, mixerDock);
    pads = new PadGrid;
    auto* padsPanel = new QWidget;                   // pads, et dessous : tout reactiver
    auto* padsCol = new QVBoxLayout (padsPanel);
    padsCol->setContentsMargins (6, 4, 6, 4);
    padsCol->addWidget (pads, 1);
    padsCol->addWidget (muteSoloRow (padsUnmute, padsUnsolo, false));
    padsDock = makeDock (tr ("Pads"), "padsDock", padsPanel);
    groupsDock = makeDock (tr ("Groups"), "groupsDock", buildGroupBlock());
    keysDock = makeDock (tr ("Keyboard"), "keyboardDock", buildKeyboardPanel());
    statusDock = makeDock (tr ("Status"), "statusDock", buildScreens());
    addDockWidget (Qt::RightDockWidgetArea, padsDock);
    addDockWidget (Qt::BottomDockWidgetArea, groupsDock);
    addDockWidget (Qt::BottomDockWidgetArea, keysDock);
    splitDockWidget (groupsDock, keysDock, Qt::Horizontal);   // groupes a gauche du clavier
    addDockWidget (Qt::RightDockWidgetArea, statusDock);
    statusDock->setFloating (true);                  // Status : fenetre a part, cachee
    statusDock->resize (720, 190);
    statusDock->hide();

    transportBar = new QToolBar (tr ("Transport"));
    transportBar->setObjectName ("transportTool");
    transportBar->addWidget (buildTransportBar());
    addToolBar (Qt::BottomToolBarArea, transportBar);
    applyTheme();
    resize (1180, 900);
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
    fileMenu->addAction (settingsAction);            // Mac : range dans le menu de l'app (Cmd + ,)

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
    auto* dbl = editMenu->addAction (tr ("Double Pattern"), this, [this] { doublePattern(); });
    dbl->setShortcut (QKeySequence ("Ctrl+D"));

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
        resizeDocks ({ patternDock, padsDock }, { width() * 62 / 100, width() * 38 / 100 }, Qt::Horizontal);
        resizeDocks ({ patternDock, keysDock }, { height() - 330, 150 }, Qt::Vertical);   // clavier plutot petit
        defaultLayout = saveState (kLayoutVersion);   // vue Default
        QSettings s;
        if (s.contains ("windowState"))
            restoreState (s.value ("windowState").toByteArray(), kLayoutVersion);
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

    // --- charge de l'ordinateur (barre du haut), une fois par seconde
    statsTimer.setInterval (1000);
    connect (&statsTimer, &QTimer::timeout, this, &MainWindow::refreshStats);
    statsTimer.start();
    refreshStats();

    // --- pads et clavier de l'ecran
    connect (pads, &PadGrid::pressed, this, [this] (int pad, int vel)
    {
        playPad (pad, vel, true);
        pads->flash (pad, true);
        selectSound (pad);
    });
    connect (pads, &PadGrid::released, this, [this] (int pad)
    {
        playPad (pad, 0, false);
        pads->flash (pad, false);
    });
    connect (pads, &PadGrid::menuRequested, this, [this] (int pad, const QPoint& at)
    {
        // Clic droit sur un pad : Mute (rouge), Solo (jaune), puis le sample
        const size_t i = (size_t) (state.group * 16 + pad - 1);
        QMenu menu (this);
        auto item = [&menu] (const QString& text, const QString& color, bool on)
        {
            auto* button = new QPushButton ((on ? QStringLiteral ("\u2713 ") : QStringLiteral ("   ")) + text);
            button->setFlat (true);
            button->setStyleSheet (QString ("QPushButton { color: %1; text-align: left; padding: 5px 14px; border: none; font-weight: bold; }"
                                            "QPushButton:hover { background: #2c2c36; }").arg (color));
            auto* action = new QWidgetAction (&menu);
            action->setDefaultWidget (button);
            menu.addAction (action);
            connect (button, &QPushButton::clicked, &menu, [&menu, action] { menu.setActiveAction (action); action->trigger(); menu.close(); });
            return action;
        };
        const QString num = QString ("%1").arg (pad, 2, 10, QChar ('0'));
        auto* mute = item ("Mute " + num, "#e05050", state.soundMute[i]);
        auto* solo = item ("Solo " + num, "#e8c040", state.soundSolo[i]);
        menu.addSeparator();
        auto* load = menu.addAction (tr ("Load Sample..."));
        QAction* remove = state.samples[i].isEmpty() ? nullptr : menu.addAction (tr ("Remove Sample (%1)").arg (padName (state.group, pad)));
        QAction* chosen = nullptr;
        for (auto* a : { mute, solo })
            connect (a, &QAction::triggered, &menu, [&chosen, a] { chosen = a; });
        if (auto* picked = menu.exec (at))
            chosen = picked;
        if (chosen == mute || chosen == solo)
            toggleSound (state.group, pad, chosen == solo);
        else if (chosen == load)
            loadSampleDialog (pad);
        else if (chosen && chosen == remove)
            setPadSample (state.group, pad, {});
    });
    connect (pads, &PadGrid::filesDropped, this, [this] (int pad, const QStringList& files)
    {
        // Un fichier = ce pad ; plusieurs = ce pad et les suivants (par ordre de nom)
        for (int k = 0; k < files.size() && pad + k <= 16; ++k)
            setPadSample (state.group, pad + k, files[k]);
        selectSound (pad);
    });
    connect (piano, &PianoKeys::noteOn, this, [this] (int n, int v) { playNote (n, v, true); piano->setHeld (n, true); });
    connect (piano, &PianoKeys::noteOff, this, [this] (int n) { playNote (n, 0, false); piano->setHeld (n, false); });

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
    connect (transport, &Transport::row, this, [this] (int r) { patternView->setPlayRow (r); });
    connect (transport, &Transport::playingChanged, this, [this] (bool on)
    {
        patternView->setPlaying (on);
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
    });
    if (opts.engine && ! engine->start (settings.value ("output").toString()))
        engineLabel->setText (engine->error());

    // --- Maschine
    maschine = new MaschineLink (this);
    maschine->setPadHandler ([this] (int pad, int vel, bool down) { playPad (pad, vel, down); });
    connect (maschine, &MaschineLink::pad, this, [this] (int pad, int, bool down)
    {
        pads->flash (pad, down);
        if (down)
            selectSound (pad);
        maschine->updateLights();
    });
    connect (maschine, &MaschineLink::button, this, &MainWindow::onMaschineButton);
    connect (maschine, &MaschineLink::jog, this, [this] (int d)
    {
        if (maschine->property ("tempoHeld").toBool())
            bpmBox->setValue (bpmBox->value() + d);
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
            l.setPad (p, hue, tag == 'M' || tag == 'm' ? fc::Shade::Off
                              : p == state.sound || tag == 'S' ? fc::Shade::Bright : fc::Shade::Dim);
        }
        const int bank = g / 8 * 8;
        for (int i = 0; i < 8; ++i)
            l.setGroup (i, groupHue (bank + i), ! state.groupAudible (bank + i) ? fc::Shade::Off
                                               : bank + i == g || state.groupSolo[(size_t) (bank + i)] ? fc::Shade::Bright : fc::Shade::Dim);
        l.setButton ("mute", muteHeld ? fc::Shade::Bright : fc::Shade::Dim);
        l.setButton ("solo", soloHeld ? fc::Shade::Bright : fc::Shade::Dim);
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
        const std::string groupText = ("Group " + groupName (state.group)).toStdString();
        left.text (480 - 8 - fc::Canvas::textWidth (groupText), 5, groupText, fc::kBlack);
        for (int p = 1; p <= 16; ++p)
        {
            const int x = 8 + (p - 1) % 4 * 118, y = 34 + (3 - (p - 1) / 4) * 58;
            const bool chosen = p == state.sound;
            left.fillRect (x, y, 110, 50, chosen ? rgb (acc) : fc::Rgb { 26, 26, 34 });
            left.frameRect (x, y, 110, 50, rgb (acc.darker (150)));
            const auto name = QString ("%1 %2").arg (p, 2, 10, QChar ('0')).arg (padName (state.group, p)).left (13).toStdString();
            left.text (x + 4, y + 17, name, chosen ? fc::kBlack : fc::kWhite);
        }
        // Ecran droit : transport
        right.fill (fc::kBlack);
        right.fillRect (0, 0, 480, 26, rgb (acc));
        right.text (8, 5, "TRANSPORT", fc::kBlack);
        right.text (8, 50, QString ("%1 BPM").arg (transport->bpm(), 0, 'f', 0).toStdString(), fc::kWhite, 3);
        right.text (8, 120, transport->playing() ? "PLAY" : "STOP", transport->playing() ? rgb (acc) : fc::kWhite, 2);
        for (int i = 0; i < 4; ++i)
            right.fillRect (200 + i * 40, 122, 28, 28, transport->playing() && i == beatShown ? rgb (acc) : fc::Rgb { 40, 40, 48 });
        right.text (8, 180, QString ("Metronome %1  vol %2").arg (transport->metronome() ? "ON" : "OFF")
                                .arg (transport->metronomeVolume()).toStdString(), fc::kWhite);
        right.text (8, 204, QString ("Keyboard : sound %1, octave %2").arg (state.sound.load())
                                .arg (state.octave.load()).toStdString(), fc::Rgb { 170, 170, 170 });
        right.text (8, 248, "Qt app - first skeleton", fc::Rgb { 110, 110, 110 });
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
    head->addWidget (bankButton);
    col->addLayout (head);
    auto* grid = new QGridLayout;
    grid->setSpacing (8);
    for (int i = 0; i < 8; ++i)
    {
        auto* b = new QPushButton;
        b->setCheckable (true);
        b->setFocusPolicy (Qt::NoFocus);
        b->setMinimumHeight (48);
        connect (b, &QPushButton::clicked, this, [this, i] { selectGroup (i + (state.group >= 8 ? 8 : 0)); });
        grid->addWidget (b, i / 4, i % 4);
        groupButtons[(size_t) i] = b;
    }
    col->addLayout (grid);
    return panel;
}

QWidget* MainWindow::buildMixer()
{
    auto* panel = new QFrame;
    panel->setObjectName ("panel");
    auto* col = new QVBoxLayout (panel);
    auto* head = new QHBoxLayout;
    auto* hint = new QLabel (tr ("drag the white line = volume, double-click = 100, M = mute, S = solo"));
    hint->setObjectName ("hint");
    head->addWidget (muteSoloRow (mixUnmute, mixUnsolo, true));
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
    tools->addWidget (hint);
    tools->addStretch();
    tools->addSpacing (10);
    tools->addWidget (lenLabel);
    tools->addWidget (lengthBox);
    tools->addWidget (twice);
    tools->addWidget (clear);
    head->addWidget (patternTools);
    patternTools->setObjectName ("patternTools");

    patternView = new PatternView (sequencer, state);
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
    QDockWidget* dock = index == 0 ? mixerDock : patternDock;
    dock->show();
    dock->raise();
}

bool MainWindow::patternShown() const
{
    return patternDock->isVisible() && ! patternView->visibleRegion().isEmpty();
}

QWidget* MainWindow::buildTransportBar()
{
    // Tout en bas, long et fin : symboles habituels du transport
    auto* bar = new QFrame;
    bar->setObjectName ("transport");
    auto* row = new QHBoxLayout (bar);
    row->setContentsMargins (10, 4, 10, 4);
    row->setSpacing (8);

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
        b->setFixedSize (46, 30);
    }
    for (auto* b : { restart, playButton, stop, recButton })
        row->addWidget (b);
    row->addSpacing (14);

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
    row->addWidget (beatLabel);
    row->addSpacing (14);

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
    metroVolume->setFixedWidth (110);
    metroVolume->setToolTip (tr ("Metronome volume (Maschine: hold TAP + turn the 4D wheel)"));
    connect (metroVolume, &QSlider::valueChanged, this, [this] (int v)
    {
        transport->setMetronomeVolume (v);
        QSettings().setValue ("metronomeVolume", v);
        refreshMaschine();
    });
    row->addWidget (metroButton);
    row->addWidget (metroVolume);
    row->addStretch();
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
                playNote (it.value(), 0, false);
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
        help->setText (tr ("Octave %1   Velocity C / V   Space \u25B6 / \u275A\u275A").arg (oct));
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
        QSpinBox { padding-right: 22px; }
        QSpinBox::up-button, QSpinBox::down-button { width: 20px; border: none; background: #30303a; }
        QSpinBox::up-button { subcontrol-origin: border; subcontrol-position: top right; border-top-right-radius: 6px; border-bottom: 1px solid #22222a; }
        QSpinBox::down-button { subcontrol-origin: border; subcontrol-position: bottom right; border-bottom-right-radius: 6px; }
        QSpinBox::up-button:hover, QSpinBox::down-button:hover { background: #44444f; }
        QSpinBox::up-arrow { image: url(:/arrow_up.png); width: 10px; height: 6px; }
        QSpinBox::down-arrow { image: url(:/arrow_down.png); width: 10px; height: 6px; }
        QPushButton#unmute, QPushButton#unsolo { font-size: 11px; padding: 2px 8px; }
        QPushButton#unmute:enabled { color: #e05050; border-color: #e05050; }
        QPushButton#unsolo:enabled { color: #e8c040; border-color: #e8c040; }
        QPushButton#unmute:disabled, QPushButton#unsolo:disabled { color: #55555e; }
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
    if (down)
    {
        const int g = state.group;
        padGroup[(size_t) pad] = g;                  // la fin de note ira a ce groupe
        if (muteHeld || soloHeld)                   // MUTE / SOLO + pad : couper / solo ce son
        {
            toggleSound (g, pad, soloHeld);
            return;
        }
        const int v = state.outVelocity (g, velocity);   // volume du groupe et master (mixeur)
        if (v > 0 && state.audible (g, pad))
            engine->hit (pad, v, defaultNote (pad), g, adsr (pad));
        std::lock_guard<std::mutex> lk (recLock);
        recordStart (padRec[(size_t) pad], pad - 1, defaultNote (pad), velocity);
    }
    else
    {
        if (adsr (pad))
            engine->noteOff (pad, defaultNote (pad), padGroup[(size_t) pad]);
        std::lock_guard<std::mutex> lk (recLock);
        recordEnd (padRec[(size_t) pad]);
    }
}

void MainWindow::playNote (int note, int velocity, bool down)
{
    const int snd = state.sound;
    const int v = state.outVelocity (state.group, velocity);
    if (down)
    {
        if (v > 0 && state.audible (state.group, snd))
            engine->hit (snd, v, note, state.group, adsr (snd));
        std::lock_guard<std::mutex> lk (recLock);
        recordStart (noteRec[note], snd - 1, note, velocity);
        return;
    }
    {
        std::lock_guard<std::mutex> lk (recLock);
        if (noteRec.contains (note))
            recordEnd (noteRec[note]);
    }
    engine->noteOff (snd, note, state.group);
}

void MainWindow::recordStart (RecNote& slot, int track, int note, int velocity)
{
    // REC arme et lecture en cours : la note va dans le pattern du groupe, a sa place exacte
    slot.active = false;
    if (! recArmed || ! transport->playing())
        return;
    if (! takeSaved[(size_t) state.group].exchange (true))
        remember (state.group);                       // avant la 1re note de la prise
    slot.where = sequencer.record (state.group, track, note, velocity, transport->position());
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
    state.group = qBound (0, group, kGroups - 1);
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
    const QColor c = groupColor (state.group);
    pads->setColor (c);
    pads->setSelected (state.sound);
    for (int p = 1; p <= 16; ++p)
        pads->setLabel (p, padName (state.group, p));
    piano->setColor (c);
    piano->setBaseNote (12 * (state.octave + 1));
    octaveLabel->setText (tr ("Octave %1   Velocity %2").arg (state.octave.load()).arg (state.velocity.load()));
    soundLabel->setText (tr ("Group <b>%1</b> — sound <b>%2 %3</b> (%4)<br>The keyboard plays this sound; click a pad to change it.")
                             .arg (groupName (state.group))
                             .arg (state.sound.load(), 2, 10, QChar ('0'))
                             .arg (soundName (state.sound))
                             .arg (adsr (state.sound) ? "ADSR" : "ONE-SHOT"));
    refreshMaschine();
}

void MainWindow::refreshGroups()
{
    // Les 8 boutons montrent la banque du groupe choisi (A-H ou A2-H2), dans leurs couleurs
    const int bank = state.group >= 8 ? 8 : 0;
    bankButton->setChecked (bank == 8);
    for (int i = 0; i < 8; ++i)
    {
        auto* b = groupButtons[(size_t) i];
        const int g = bank + i;
        const QString tag = state.groupMute[(size_t) g] ? " M" : state.groupSolo[(size_t) g] ? " S" : "";
        b->setText (groupName (g) + tag);
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
    if (name == "mute" || name == "solo")           // tenus : + groupe ou + pad
    {
        (name == "mute" ? muteHeld : soloHeld) = down;
        refreshMaschine();
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
    if (! down)
        return;
    const bool shift = maschine->property ("shiftHeld").toBool();
    if (name == "shift")
        maschine->setProperty ("shiftHeld", true);
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
        if (muteHeld || soloHeld)                    // MUTE / SOLO + groupe
            toggleGroup (i + (state.group >= 8 ? 8 : 0), soloHeld);
        else
            selectGroup (i + (bank2 ? 8 : 0));
    }
    else if (name == "nav_up" || name == "nav_down")
        setOctave (state.octave + (name == "nav_up" ? 1 : -1));
}

// ------------------------------------------------------------ clavier de l'ordinateur

bool MainWindow::eventFilter (QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::KeyRelease && maschine && static_cast<QKeyEvent*> (event)->key() == Qt::Key_Shift)
        maschine->setProperty ("shiftHeld", false);
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
    {
        // Pas de notes quand on tape dans un champ (tempo...)
        QWidget* focus = QApplication::focusWidget();
        const bool typing = qobject_cast<QLineEdit*> (focus) || qobject_cast<QAbstractSpinBox*> (focus);
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
        playNote (note, 0, false);
        piano->setHeld (note, false);
        return true;
    }
    if (e->key() == Qt::Key_Space)
    {
        transport->playing() ? transport->pause() : transport->play();
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
    playNote (note, state.velocity, true);
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
    for (int g = 0; g < kGroups; ++g)
    {
        sequencer.clear (g);
        sequencer.setPatternLength (g, 16);
        sequencer.setGroupOff (g, false);
        state.groupVolume[(size_t) g] = 100;
    }
    state.samples = {};
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

bool MainWindow::saveProjectAs()
{
    bool ok = false;
    const QString typed = QInputDialog::getText (this, tr ("Save Project As"),
                                                 tr ("Project name (8 characters: A-Z, 0-9, - _):"),
                                                 QLineEdit::Normal, project.name, &ok);
    if (! ok)
        return false;
    const QString name = Project::cleanName (typed);
    const QString folder = Project::folderFor (projectsDir(), name);
    const QString target = folder + "/project.json";
    if (QFile::exists (target) && QFileInfo (target).absoluteFilePath() != project.file
        && QMessageBox::question (this, tr ("Save Project As"),
                                  tr ("A project named %1 already exists. Replace it?").arg (name))
               != QMessageBox::Yes)
        return false;
    const QString oldName = project.name;
    project.name = name;
    project.fromApp (sequencer, state, bpmBox->value());
    QString error;
    if (! project.save (projectsDir(), error))
    {
        project.name = oldName;
        QMessageBox::warning (this, tr ("Save Project"), error);
        return false;
    }
    QSettings().setValue ("lastProject", project.file);
    projectModified = false;
    updateTitle();
    statusBar()->showMessage (tr ("Saved %1").arg (project.file), 3000);
    return true;
}

bool MainWindow::maybeSave()
{
    if (! project.modified (sequencer, state, bpmBox->value()))
        return true;
    const auto answer = QMessageBox::question (this, tr ("Unsaved changes"),
                                               tr ("Save the changes to project %1?").arg (project.name),
                                               QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
                                               QMessageBox::Save);
    if (answer == QMessageBox::Save)
        return saveProject();
    return answer == QMessageBox::Discard;
}

// ------------------------------------------------------------------------------- panneaux et vues

QDockWidget* MainWindow::makeDock (const QString& title, const QString& name, QWidget* content)
{
    auto* dock = new QDockWidget (title, this);
    dock->setObjectName (name);                      // necessaire pour retrouver la disposition
    dock->setWidget (content);
    dock->setFeatures (QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable
                       | QDockWidget::DockWidgetClosable);
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
                              Item { keysDock, QKeySequence ("Ctrl+3") }, Item { statusDock, QKeySequence ("Ctrl+I") } })
    {
        auto* a = item.dock->toggleViewAction();
        a->setShortcut (item.key);
        menu->addAction (a);
    }
    menu->addAction (transportBar->toggleViewAction());
    menu->addSeparator();
    auto* mixerAction = menu->addAction (tr ("Mixer"), this, [this] { showLeftTab (0); });
    mixerAction->setShortcut (QKeySequence ("Ctrl+M"));
    auto* patternAction = menu->addAction (tr ("Pattern"), this, [this] { showLeftTab (1); });
    patternAction->setShortcut (QKeySequence ("Ctrl+P"));

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
        statusDock->hide();
        if (name == "Live")                          // jouer : pattern, pads, groupes
        {
            keysDock->hide();
            showLeftTab (1);
        }
        else if (name == "Mix")                      // mixer : mixeur en grand, groupes
        {
            padsDock->hide();
            keysDock->hide();
            showLeftTab (0);
        }
        else
            showLeftTab (1);
    }
    else
    {
        QSettings s;
        restoreState (s.value ("views/" + name + "/state").toByteArray(), kLayoutVersion);
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
    undoStack.push_back ({ group, sequencer.pattern (group) });
    if (undoStack.size() > 200)                       // 200 pas en arriere
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
        to.push_back ({ step.group, sequencer.pattern (step.group) });
    }
    sequencer.setPattern (step.group, step.pattern);
    if (step.group != state.group)                    // on montre le groupe concerne
        selectGroup (step.group);
    showPatternState();
    statusBar()->showMessage (QString ("%1 (%2)").arg (redoIt ? tr ("Redo") : tr ("Undo"), groupName (step.group)), 1200);
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
        groupButtons[(size_t) i]->setText (groupName (g) + tag);
        const QColor c = state.groupAudible (g) ? groupColor (g) : QColor (90, 90, 98);
        groupButtons[(size_t) i]->setStyleSheet (QString ("QPushButton { color: %1; border: 2px solid %1; background: #17171c; font-size: 16px; font-weight: bold; }"
                                                          "QPushButton:checked { color: #111; background: %1; }").arg (css (c)));
    }
    refreshMaschine();
}

QWidget* MainWindow::muteSoloRow (QPushButton*& unmute, QPushButton*& unsolo, bool groups)
{
    // Deux petits boutons : tout reactiver d'un coup (sons du groupe, ou groupes du mixeur)
    auto* row = new QWidget;
    auto* h = new QHBoxLayout (row);
    h->setContentsMargins (0, 0, 0, 0);
    h->setSpacing (6);
    unmute = new QPushButton (tr ("Unmute all"));
    unsolo = new QPushButton (tr ("Unsolo all"));
    unmute->setObjectName ("unmute");
    unsolo->setObjectName ("unsolo");
    for (auto* b : { unmute, unsolo })
    {
        b->setFocusPolicy (Qt::NoFocus);
        b->setEnabled (false);
    }
    unmute->setToolTip (groups ? tr ("Turn every muted group back on") : tr ("Turn every muted sound of this group back on"));
    unsolo->setToolTip (groups ? tr ("Clear the solo of every group") : tr ("Clear the solo of every sound of this group"));
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
    return path.isEmpty() ? soundName (sound) : QFileInfo (path).completeBaseName();
}

void MainWindow::setPadSample (int group, int sound, const QString& path)
{
    state.samples[(size_t) (group * 16 + sound - 1)] = path;
    syncSamples();
    refreshAll();
    if (! path.isEmpty())
        statusBar()->showMessage (tr ("Pad %1: %2").arg (sound, 2, 10, QChar ('0')).arg (QFileInfo (path).fileName()), 2000);
}

void MainWindow::syncSamples (bool all)
{
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
