// Fuzzy John DAW Pulses — fenetre principale (squelette Qt). Licence : GPLv3.
#include "main_window.h"

#include "engine_link.h"
#include "maschine_link.h"
#include "mixer_view.h"
#include "pattern_view.h"
#include "loop_dialog.h"
#include "piano_roll.h"
#include "generate_dialog.h"
#include "midi_import.h"
#include "keyboard_view.h"
#include "music_tables.h"
#include "note_dialog.h"
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
#include <QStackedWidget>
#include <QStatusBar>
#include <QVBoxLayout>

#include <cstring>

using namespace session;
namespace fc = fuzzy::control;

namespace {
constexpr int kLayoutVersion = 6;                  // disposition des panneaux (2 : Pattern et Mixer en panneaux)

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
    // Un seul clavier : l'onglet Keyboard. L'ancien panneau garde ses reglages (PC KEYS,
    // AZERTY / QWERTY), invisible ; il est cree avant l'onglet qui les reprend.
    auto* oldKeys = buildKeyboardPanel();
    oldKeys->setParent (this);
    oldKeys->hide();
    // Vue par defaut : en haut, Pattern / Piano Roll / Mixer en onglets sur toute la largeur ;
    // en bas, Groups | Keyboard | Pads cote a cote
    patternDock = makeDock (tr ("Pattern"), "patternDock", buildLeftPanel());
    mixerDock = makeDock (tr ("Mixer"), "mixerDock", buildMixer());
    rollDock = makeDock (tr ("Piano Roll"), "pianoRollDock", buildPianoRoll());
    addDockWidget (Qt::TopDockWidgetArea, patternDock);
    tabifyDockWidget (patternDock, rollDock);       // onglets : Pattern, Piano Roll, Mixer
    tabifyDockWidget (rollDock, mixerDock);
    groupsDock = makeDock (tr ("Groups"), "groupsDock", buildGroupBlock());
    keysTabDock = makeDock (tr ("Keyboard"), "keyboardTabDock", buildKeyboardTab());
    pads = new PadGrid;
    auto* padsPanel = new QWidget;                   // pads, et dessous : tout reactiver
    auto* padsCol = new QVBoxLayout (padsPanel);
    padsCol->setContentsMargins (4, 2, 4, 2);
    padsCol->addWidget (pads, 1);
    padsCol->addWidget (muteSoloRow (padsUnmute, padsUnsolo, false));
    padsDock = makeDock (tr ("Pads"), "padsDock", padsPanel);
    statusDock = makeDock (tr ("Status"), "statusDock", buildScreens());
    addDockWidget (Qt::BottomDockWidgetArea, groupsDock);
    addDockWidget (Qt::BottomDockWidgetArea, keysTabDock);
    splitDockWidget (groupsDock, keysTabDock, Qt::Horizontal);
    addDockWidget (Qt::BottomDockWidgetArea, padsDock);
    splitDockWidget (keysTabDock, padsDock, Qt::Horizontal);

    addDockWidget (Qt::RightDockWidgetArea, statusDock);
    statusDock->setFloating (true);                  // Status : fenetre a part, cachee
    statusDock->resize (720, 190);
    statusDock->hide();

    transportBar = new QToolBar (tr ("Transport"));
    transportBar->setObjectName ("transportTool");
    transportBar->addWidget (buildTransportBar());
    addToolBar (Qt::BottomToolBarArea, transportBar);
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
        resizeDocks ({ patternDock, padsDock }, { height() * 58 / 100, height() * 42 / 100 }, Qt::Vertical);
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
        editLoopAction->setEnabled (! state.samples[i].isEmpty());
        menu.addSeparator();
        auto* load = menu.addAction (tr ("Load Sample..."));
        QAction* remove = state.samples[i].isEmpty() ? nullptr : menu.addAction (tr ("Remove Sample (%1)").arg (padName (state.group, pad)));
        const bool empty = state.samples[i].isEmpty() && state.effSynth (state.group, pad) == 0;
        auto* emptyPad = menu.addAction (tr ("Empty Pad (no sound)"));
        emptyPad->setEnabled (! empty);
        auto* defaultSound = menu.addAction (tr ("Default Sound (%1)").arg (state.group >= 8 ? tr ("empty in A2-H2") : soundName (pad)));
        defaultSound->setEnabled (state.synth[i] != -1 || ! state.samples[i].isEmpty());
        QAction* chosen = nullptr;
        for (auto* a : { mute, solo })
            connect (a, &QAction::triggered, &menu, [&chosen, a] { chosen = a; });
        if (auto* picked = menu.exec (at))
            chosen = picked;
        if (chosen == mute || chosen == solo)
            toggleSound (state.group, pad, chosen == solo);
        else if (modeActions.contains (chosen))
        {
            state.playMode[i] = (int) modeActions.indexOf (chosen);
            syncSamples();
            refreshAll();
        }
        else if (chosen == editLoopAction)
            editLoop (pad);
        else if (chosen == load)
            loadSampleDialog (pad);
        else if (chosen && chosen == remove)
            setPadSample (state.group, pad, {});
        else if (chosen == emptyPad || chosen == defaultSound)
        {
            state.synth[i] = chosen == emptyPad ? 0 : -1;   // 0 = aucun instrument (« None » du pilote)
            setPadSample (state.group, pad, {});
        }
    });
    connect (pads, &PadGrid::midiDropped, this, &MainWindow::importMidi);
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
    connect (transport, &Transport::row, this, [this] (int r) { patternView->setPlayRow (r); pianoRoll->setPlayRow (r); });
    connect (transport, &Transport::playingChanged, this, [this] (bool on)
    {
        patternView->setPlaying (on);
        pianoRoll->setPlaying (on);
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
        l.setButton ("pattern", shown (patternView) ? fc::Shade::Bright : fc::Shade::Dim);
        l.setButton ("events", shown (pianoRoll) ? fc::Shade::Bright : fc::Shade::Dim);
        l.setButton ("mixer", shown (mixer) ? fc::Shade::Bright : fc::Shade::Dim);
        l.setButton ("keyboard", shown (keyboardView) ? fc::Shade::Bright : fc::Shade::Dim);
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
        b->setContextMenuPolicy (Qt::CustomContextMenu);   // clic droit : Mute (rouge), Solo (jaune)
        connect (b, &QPushButton::customContextMenuRequested, this, [this, i, b] (const QPoint& p)
        {
            const int g = i + (state.group >= 8 ? 8 : 0);
            QMenu menu (this);
            QAction* chosen = nullptr;
            QList<QAction*> items;
            for (const bool solo : { false, true })
            {
                const bool on = solo ? state.groupSolo[(size_t) g].load() : state.groupMute[(size_t) g].load();
                auto* button = new QPushButton ((on ? QStringLiteral ("\u2713 ") : QStringLiteral ("   "))
                                                + (solo ? "Solo " : "Mute ") + groupName (g));
                button->setFlat (true);
                button->setStyleSheet (QString ("QPushButton { color: %1; text-align: left; padding: 5px 14px; border: none; font-weight: bold; }"
                                                "QPushButton:hover { background: #2c2c36; }").arg (solo ? "#e8c040" : "#e05050"));
                auto* action = new QWidgetAction (&menu);
                action->setDefaultWidget (button);
                menu.addAction (action);
                items << action;
                connect (button, &QPushButton::clicked, &menu, [&menu, &chosen, action] { chosen = action; menu.close(); });
            }
            menu.exec (b->mapToGlobal (p));
            if (chosen)
                toggleGroup (g, chosen == items[1]);
        });
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
    hint->setSizePolicy (QSizePolicy::Ignored, QSizePolicy::Preferred);   // peut retrecir
    hint->setMinimumWidth (0);
    tools->addWidget (hint);
    tools->addStretch();
    tools->addSpacing (10);
    tools->addWidget (lenLabel);
    tools->addWidget (lengthBox);
    auto* gen = new QPushButton (QStringLiteral ("\U0001F3B2 ") + tr ("Generate"));
    gen->setFocusPolicy (Qt::NoFocus);
    gen->setToolTip (tr ("Random notes for the chosen sound (Cmd+G)"));
    connect (gen, &QPushButton::clicked, this, [this] { openGenerate(); });
    tools->addWidget (gen);
    tools->addWidget (twice);
    tools->addWidget (clear);
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
    QDockWidget* dock = index == 0 ? mixerDock : index == 2 ? rollDock : index == 3 ? keysTabDock : patternDock;
    dock->show();
    dock->raise();
    QTimer::singleShot (0, this, [this] { refreshMaschine(); });
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
    auto* keysHint = new QLabel (tr ("Space \u25B6 / \u275A\u275A"));   // rappel : la barre d'espace
    keysHint->setObjectName ("hint");
    keysHint->setToolTip (tr ("Space bar: play / pause"));
    row->addWidget (keysHint);
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
        if (maschineShift)                            // Shift + pad : fonctions de la serigraphie
        {
            QMetaObject::invokeMethod (this, [this, pad] { shiftPad (pad); }, Qt::QueuedConnection);
            return;
        }
        if (muteHeld || soloHeld)                   // MUTE / SOLO + pad : couper / solo ce son
        {
            toggleSound (g, pad, soloHeld);
            return;
        }
        const int v = state.outVelocity (g, velocity);   // volume du groupe et master (mixeur)
        if (v > 0 && state.audible (g, pad) && state.hasInstrument[(size_t) state.index (g, pad)])
            engine->hit (pad, v, defaultNote (pad), g, state.held (g, pad), state.releaseMs[(size_t) state.index (g, pad)],
                         0, state.effSynth (g, pad));
        std::lock_guard<std::mutex> lk (recLock);
        recordStart (padRec[(size_t) pad], pad - 1, defaultNote (pad), velocity);
    }
    else
    {
        if (state.held (padGroup[(size_t) pad], pad))
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
        if (v > 0 && state.audible (state.group, snd) && state.hasInstrument[(size_t) state.index (state.group, snd)])
            engine->hit (snd, v, note, state.group, state.held (state.group, snd), state.releaseMs[(size_t) state.index (state.group, snd)],
                         0, state.effSynth (state.group, snd));
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
    if (rollTitle)                                   // Piano Roll : le son choisi
        rollTitle->setText (tr ("SOUND %1 — %2").arg (state.sound.load(), 2, 10, QChar ('0')).arg (padName (state.group, state.sound)));
    const QColor c = groupColor (state.group);
    pads->setColor (c);
    pads->setSelected (state.sound);
    for (int p = 1; p <= 16; ++p)
        pads->setLabel (p, padName (state.group, p));
    {
        std::array<bool, 16> empty {};
        for (int p = 1; p <= 16; ++p)
            empty[(size_t) p - 1] = ! state.hasInstrument[(size_t) state.index (state.group, p)];
        pads->setEmpty (empty);
    }
    piano->setColor (c);
    piano->setBaseNote (12 * (state.octave + 1));
    octaveLabel->setText (tr ("Octave %1   Velocity %2").arg (state.octave.load()).arg (state.velocity.load()));
    if (keyInfo)
        keyInfo->setText (tr ("Oct %1   Vel %2").arg (state.octave.load()).arg (state.velocity.load()));
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
        if (muteHeld || soloHeld)                    // MUTE / SOLO + groupe
            toggleGroup (i + (state.group >= 8 ? 8 : 0), soloHeld);
        else
            selectGroup (i + (bank2 ? 8 : 0));
    }
    else if (name == "nav_up" || name == "nav_down")
        setOctave (state.octave + (name == "nav_up" ? 1 : -1));
    // Vues de l'app (comme PATTERN / EVENTS / MIXER du prototype)
    else if (name == "pattern")                      // 2e appui : retour a la vue d'avant
        toggleView (1, "PATTERN");
    else if (name == "events")
        toggleView (2, "PIANO ROLL");
    else if (name == "keyboard")
        toggleView (3, "KEYBOARD");
    else if (name == "mixer")
        toggleView (0, "MIXER");
    else if (name == "variation")                    // nouveau tirage ; Shift = la fenetre Generate
        shift ? openGenerate() : quickVariation();
    else if (name == "sampling")                     // mode du son ; Shift = Edit Loop
        shift ? editLoop (state.sound) : cyclePlayMode();
    else if (name == "nav_left" || name == "nav_right")   // case du pattern : ligne (Shift = piste)
    {
        const int step = name == "nav_right" ? 1 : -1;
        showLeftTab (1);
        shift ? patternView->moveCursor (0, step) : patternView->moveCursor (step, 0);
    }
    else if (name == "nav_click")                    // appui molette : la note du son choisi
    {
        showLeftTab (1);
        addNoteAtCursor();
    }
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
            addNoteAtCursor();
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
    for (int g = 0; g < kGroups; ++g)
    {
        sequencer.clear (g);
        sequencer.setPatternLength (g, 16);
        sequencer.setGroupOff (g, false);
        state.groupVolume[(size_t) g] = 100;
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
                              Item { statusDock, QKeySequence ("Ctrl+I") } })
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
    auto* rollAction = menu->addAction (tr ("Piano Roll"), this, [this] { showLeftTab (2); });
    rollAction->setShortcut (QKeySequence ("Ctrl+R"));
    auto* keysAction = menu->addAction (tr ("Keyboard"), this, [this] { showLeftTab (3); });
    keysAction->setShortcut (QKeySequence ("Ctrl+K"));
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
    if (undoStack.size() > 100)                       // 100 pas en arriere (patterns de 256 lignes)
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
    dialog.exec();
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
    auto* hint = new QLabel (tr ("click = note, drag = move (%1 copy), double-click = erase, wheel = velocity")
                                 .arg (PatternView::copyKey()));
    hint->setObjectName ("hint");
    hint->setSizePolicy (QSizePolicy::Ignored, QSizePolicy::Preferred);   // peut retrecir
    hint->setMinimumWidth (0);
    head->addWidget (rollTitle);
    head->addStretch();
    head->addWidget (hint);
    auto* gen = new QPushButton (QStringLiteral ("\U0001F3B2 ") + tr ("Generate"));
    gen->setFocusPolicy (Qt::NoFocus);
    gen->setToolTip (tr ("Random notes for this sound (Cmd+G)"));
    connect (gen, &QPushButton::clicked, this, [this] { openGenerate(); });
    head->addWidget (gen);
    col->addLayout (head);
    pianoRoll = new PianoRollView (sequencer, state);
    connect (pianoRoll, &PianoRollView::midiDropped, this, &MainWindow::importMidi);
    connect (pianoRoll, &PianoRollView::aboutToEdit, this, [this] { remember (state.group); });
    connect (pianoRoll, &PianoRollView::edited, this, [this] { patternView->update(); });
    connect (pianoRoll, &PianoRollView::keyPressed, this, [this] (int pitch) { playNote (pitch, state.velocity, true); });
    connect (pianoRoll, &PianoRollView::keyReleased, this, [this] (int pitch) { playNote (pitch, 0, false); });
    // Barres de defilement et zoom (H = temps, V = hauteur des notes)
    auto* hBar = new QScrollBar (Qt::Horizontal);
    auto* vBar = new QScrollBar (Qt::Vertical);
    auto* zoomH = new QSlider (Qt::Horizontal);
    auto* zoomV = new QSlider (Qt::Horizontal);
    zoomH->setRange (100, 1600);
    zoomV->setRange (6, 30);
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
    head->insertWidget (2, zoomLabel);
    head->insertWidget (3, zoomH);
    head->insertWidget (4, vLabel);
    head->insertWidget (5, zoomV);
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
    auto* grid = new QGridLayout;
    grid->setSpacing (0);
    grid->addWidget (pianoRoll, 0, 0);
    grid->addWidget (vBar, 0, 1);
    grid->addWidget (hBar, 1, 0);
    col->addLayout (grid, 1);
    syncBars();
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
    return shown (pianoRoll) ? 2 : shown (mixer) ? 0 : shown (patternView) ? 1 : shown (keyboardView) ? 3 : 1;
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
        toast (QStringList { "MIXER", "PATTERN", "PIANO ROLL", "KEYBOARD" }[back]);
        return;
    }
    if (now != index)
        viewBefore = now;
    showLeftTab (index);
    toast (name);
}

void MainWindow::playKey (int key, int velocity, bool down)
{
    // Une touche joue une note, ou un accord (Harmonizer, Chord Set) ; on garde les notes
    // jouees pour les relacher toutes, meme si les reglages changent entre-temps
    if (down)
    {
        const QList<int> notes = music::chordFor (key, keySettings);
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
}

QWidget* MainWindow::buildKeyboardTab()
{
    // Onglet Keyboard : reglages (octave, gamme, accords, clavier de l'ordinateur) + grand clavier
    QSettings settings;
    keySettings.root = settings.value ("keys/root", 0).toInt();
    keySettings.scale = std::clamp (settings.value ("keys/scale", 0).toInt(), 0, (int) music::scales().size() - 1);
    keySettings.chordMode = settings.value ("keys/chordMode", 0).toInt();
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
    auto* octDown = button (tr ("OCT - (W)"), tr ("One octave down (computer key W, QWERTY: Z)"));
    auto* octUp = button (tr ("OCT + (X)"), tr ("One octave up (computer key X)"));
    connect (octDown, &QPushButton::clicked, this, [this] { setOctave (state.octave - 1); keyboardView->update(); });
    connect (octUp, &QPushButton::clicked, this, [this] { setOctave (state.octave + 1); keyboardView->update(); });
    auto* velDown = button (tr ("VEL - (C)"), tr ("Velocity down (computer key C)"));
    auto* velUp = button (tr ("VEL + (V)"), tr ("Velocity up (computer key V)"));
    connect (velDown, &QPushButton::clicked, this, [this] { state.velocity = qBound (1, state.velocity - 16, 127); refreshAll(); });
    connect (velUp, &QPushButton::clicked, this, [this] { state.velocity = qBound (1, state.velocity + 16, 127); refreshAll(); });
    auto combo = [] (const QStringList& items, int current)
    {
        auto* c = new QComboBox;
        c->addItems (items);
        c->setCurrentIndex (current);
        c->setFocusPolicy (Qt::ClickFocus);
        return c;
    };
    auto* rootBox = combo ({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, keySettings.root);
    rootBox->setToolTip (tr ("Root note of the scale"));
    QStringList scaleItems;
    for (int i = 0; i < (int) music::scales().size(); ++i)
        scaleItems << music::scaleLabel (i);
    auto* scaleBox = combo (scaleItems, keySettings.scale);
    scaleBox->setToolTip (tr ("Scale (same list as the Maschine driver); its notes are shown in color"));
    scaleBox->setMaxVisibleItems (20);
    auto* modeBox = combo ({ tr ("Chords off"), tr ("Harmonizer"), tr ("Chord Set") }, keySettings.chordMode);
    modeBox->setToolTip (tr ("Harmonizer: each key plays a chord built on the scale. Chord Set: the 12 keys of an octave play 12 chords"));
    auto* typeBox = new QComboBox;
    typeBox->setFocusPolicy (Qt::ClickFocus);
    auto fillTypes = [this, typeBox]
    {
        const QSignalBlocker block (typeBox);
        typeBox->clear();
        if (keySettings.chordMode == 1)
            for (const auto& h : music::harmonies())
                typeBox->addItem (h.name);
        else if (keySettings.chordMode == 2)
            for (const auto& c : music::chordSets())
                typeBox->addItem (c.name);
        typeBox->setEnabled (keySettings.chordMode != 0);
        typeBox->setCurrentIndex (keySettings.chordMode == 1 ? keySettings.harmony : keySettings.chordSet);
    };
    fillTypes();
    auto save = [this]
    {
        QSettings s;
        s.setValue ("keys/root", keySettings.root);
        s.setValue ("keys/scale", keySettings.scale);
        s.setValue ("keys/chordMode", keySettings.chordMode);
        s.setValue ("keys/harmony", keySettings.harmony);
        s.setValue ("keys/chordSet", keySettings.chordSet);
        keyboardView->update();
    };
    connect (rootBox, &QComboBox::currentIndexChanged, this, [this, save] (int i) { keySettings.root = i; save(); });
    connect (scaleBox, &QComboBox::currentIndexChanged, this, [this, save] (int i) { keySettings.scale = i; save(); });
    connect (modeBox, &QComboBox::currentIndexChanged, this, [this, save, fillTypes] (int i) { keySettings.chordMode = i; fillTypes(); save(); });
    connect (typeBox, &QComboBox::currentIndexChanged, this, [this, save] (int i)
    {
        (keySettings.chordMode == 1 ? keySettings.harmony : keySettings.chordSet) = std::max (0, i);
        save();
    });
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
    keyInfo = new QLabel;
    keyInfo->setObjectName ("hint");
    keyInfo->setSizePolicy (QSizePolicy::Ignored, QSizePolicy::Preferred);
    for (auto* c : { scaleBox, typeBox })
    {
        c->setSizeAdjustPolicy (QComboBox::AdjustToMinimumContentsLengthWithIcon);
        c->setMinimumContentsLength (12);
    }
    row->addWidget (octDown);
    row->addWidget (octUp);
    row->addWidget (velDown);
    row->addWidget (velUp);
    row->addWidget (keyInfo, 1);
    row->addWidget (pcKeys);
    row->addWidget (layout2);
    col->addLayout (row);
    auto* row2 = new QHBoxLayout;
    row2->addWidget (rootBox);
    row2->addWidget (scaleBox, 2);
    row2->addSpacing (12);
    row2->addWidget (modeBox);
    row2->addWidget (typeBox, 1);
    col->addLayout (row2);
    keyboardView = new KeyboardView (state, keySettings);
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
    auto* cut = menu.addAction (tr ("Cut"));
    auto* copy = menu.addAction (tr ("Copy"));
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
