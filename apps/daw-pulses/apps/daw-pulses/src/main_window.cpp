// Fuzzy John DAW Pulses — fenetre principale (squelette Qt). Licence : GPLv3.
#include "main_window.h"

#include "engine_link.h"
#include "maschine_link.h"
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
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QVBoxLayout>

#include <cstring>

using namespace session;
namespace fc = fuzzy::control;

namespace {

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
    transport = new Transport (*engine, this);
    transport->setBpm (settings.value ("bpm", 120).toDouble());
    transport->setMetronome (settings.value ("metronome", true).toBool());
    transport->setMetronomeVolume (settings.value ("metronomeVolume", 90).toInt());
    state.octave = settings.value ("octave", 3).toInt();

    // --- mise en page, comme la Maschine : ecrans en haut, groupes + transport a gauche,
    //     pads a droite, clavier en bas
    auto* central = new QWidget;
    auto* main = new QVBoxLayout (central);
    main->setContentsMargins (12, 10, 12, 10);
    main->setSpacing (10);
    main->addWidget (buildTopBar());
    main->addWidget (buildScreens());
    auto* middle = new QHBoxLayout;
    middle->setSpacing (14);
    auto* left = new QVBoxLayout;
    left->setSpacing (10);
    left->addWidget (buildGroupBlock());
    left->addStretch();
    left->addWidget (buildTransportBlock());
    pads = new PadGrid;
    middle->addLayout (left, 2);
    middle->addWidget (pads, 3);
    main->addLayout (middle, 1);
    main->addWidget (buildKeyboardPanel());
    setCentralWidget (central);
    applyTheme();
    resize (1100, 900);

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
    connect (transport, &Transport::playingChanged, this, [this] (bool on)
    {
        playButton->setChecked (on);
        playButton->setText (on ? tr ("PLAYING") : tr ("PLAY"));
        if (! on)
            beatLabel->setText (QStringLiteral ("○ ○ ○ ○"));
        refreshMaschine();
    });

    // --- moteur audio
    connect (engine, &EngineLink::stateChanged, this, &MainWindow::refreshEngine);
    connect (engine, &EngineLink::stopped, this, [this] (const QString& why) { engineLabel->setText (why); });
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
        for (int p = 1; p <= 16; ++p)
            l.setPad (p, hue, p == state.sound ? fc::Shade::Bright : fc::Shade::Dim);
        const int bank = g / 8 * 8;
        for (int i = 0; i < 8; ++i)
            l.setGroup (i, groupHue (bank + i), bank + i == g ? fc::Shade::Bright : fc::Shade::Dim);
        l.setButton ("play", transport->playing() ? fc::Shade::Bright : fc::Shade::Dim);
        l.setButton ("stop", fc::Shade::Dim);
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
            const auto name = QString ("%1 %2").arg (p, 2, 10, QChar ('0')).arg (soundName (p)).left (13).toStdString();
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
    row->addWidget (title);
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
    outputBox = new QComboBox;
    outputBox->setFocusPolicy (Qt::NoFocus);
    outputBox->setToolTip (tr ("Audio output"));
    connect (outputBox, &QComboBox::activated, this, [this] (int)
    {
        engine->setOutput (outputBox->currentText());
        QSettings().setValue ("output", outputBox->currentText());
    });
    engineLabel = new QLabel (tr ("Audio engine: starting..."));
    engineLabel->setWordWrap (true);
    auto* next = new QLabel (tr ("<i>Coming next: mixer with meters, sequencer, .fuzzy projects</i>"));
    next->setWordWrap (true);
    next->setObjectName ("hint");
    r->addWidget (audioTitle);
    r->addWidget (outputBox);
    r->addWidget (engineLabel);
    r->addStretch();
    r->addWidget (next);

    row->addWidget (leftScreen, 1);
    row->addWidget (rightScreen, 1);
    box->setFixedHeight (150);
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

QWidget* MainWindow::buildTransportBlock()
{
    // En bas a gauche, comme le transport de la Maschine
    auto* panel = new QFrame;
    panel->setObjectName ("panel");
    auto* col = new QVBoxLayout (panel);
    auto* title = new QLabel (tr ("TRANSPORT"));
    title->setObjectName ("section");
    col->addWidget (title);

    playButton = new QPushButton (tr ("PLAY"));
    playButton->setCheckable (true);
    playButton->setObjectName ("play");
    connect (playButton, &QPushButton::clicked, this, [this] (bool on) { on ? transport->play() : transport->stop(); });
    auto* stop = new QPushButton (tr ("STOP"));
    connect (stop, &QPushButton::clicked, this, [this] { transport->stop(); engine->panic(); });
    metroButton = new QPushButton (tr ("METRO"));
    metroButton->setCheckable (true);
    metroButton->setChecked (transport->metronome());
    metroButton->setToolTip (tr ("Metronome (Maschine: TAP)"));
    connect (metroButton, &QPushButton::toggled, this, [this] (bool on)
    {
        transport->setMetronome (on);
        QSettings().setValue ("metronome", on);
        refreshMaschine();
    });
    for (auto* b : { playButton, stop, metroButton })
    {
        b->setFocusPolicy (Qt::NoFocus);
        b->setMinimumHeight (40);
    }
    auto* buttons = new QHBoxLayout;
    buttons->addWidget (playButton);
    buttons->addWidget (stop);
    buttons->addWidget (metroButton);
    col->addLayout (buttons);

    metroVolume = new QSlider (Qt::Horizontal);
    metroVolume->setRange (0, 127);
    metroVolume->setValue (transport->metronomeVolume());
    metroVolume->setFocusPolicy (Qt::NoFocus);
    metroVolume->setToolTip (tr ("Metronome volume (Maschine: hold TAP + turn the 4D wheel)"));
    connect (metroVolume, &QSlider::valueChanged, this, [this] (int v)
    {
        transport->setMetronomeVolume (v);
        QSettings().setValue ("metronomeVolume", v);
        refreshMaschine();
    });
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
    auto* row = new QHBoxLayout;
    row->addWidget (bpmBox);
    row->addWidget (beatLabel);
    row->addStretch();
    auto* volLabel = new QLabel (tr ("Metro vol."));
    volLabel->setObjectName ("hint");
    row->addWidget (volLabel);
    row->addWidget (metroVolume, 1);
    col->addLayout (row);
    return panel;
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
    help->setText (tr ("Computer keys play notes, held as long as the key is down. "
                       "Octave: W / X (QWERTY: Z / X), velocity: C / V, space: PLAY / STOP."));
    help->setWordWrap (true);
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
        #screen { background: #0b0b0e; border: 1px solid #2a2a33; border-top: 3px solid %1; border-radius: 6px; }
        #stats { color: #9a9aa4; font-family: monospace; background: transparent; }
        #title { font-size: 18px; color: %1; background: transparent; }
        #section { color: %1; font-weight: bold; letter-spacing: 1px; background: transparent; }
        #hint { color: #8a8a92; background: transparent; }
        #beats { font-size: 18px; color: %1; background: transparent; }
        QLabel { background: transparent; }
        QPushButton { background: #22222a; border: 1px solid #34343e; border-radius: 6px; padding: 6px 12px; }
        QPushButton:hover { border-color: %1; }
        QPushButton:checked { background: %1; color: #111; }
        QComboBox, QSpinBox { background: #22222a; border: 1px solid #34343e; border-radius: 6px; padding: 4px 8px; }
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
        engine->hit (pad, velocity, defaultNote (pad), g, adsr (pad));
    }
    else if (adsr (pad))
        engine->noteOff (pad, defaultNote (pad), padGroup[(size_t) pad]);
}

void MainWindow::playNote (int note, int velocity, bool down)
{
    const int snd = state.sound;
    if (down)
        engine->hit (snd, velocity, note, state.group, adsr (snd));
    else
        engine->noteOff (snd, note, state.group);
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
        pads->setLabel (p, soundName (p));
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
        b->setText (groupName (g));
        b->setChecked (g == state.group);
        b->setStyleSheet (QString ("QPushButton { color: %1; border: 2px solid %1; background: #17171c; font-size: 16px; font-weight: bold; }"
                                   "QPushButton:checked { color: #111; background: %1; }").arg (css (groupColor (g))));
    }
    // L'ecran de gauche reprend la couleur du groupe, comme sur la Maschine
    leftScreen->setStyleSheet (QString ("#screen { border-top: 3px solid %1; }").arg (css (groupColor (state.group))));
}

void MainWindow::refreshStats()
{
    stats.update();
    QStringList parts;
    if (stats.valid())
    {
        parts << tr ("CPU %1 %").arg (stats.cpuPercent(), 0, 'f', 0)
              << tr ("RAM %1 / %2 GB").arg (stats.memUsedGB(), 0, 'f', 1).arg (stats.memTotalGB(), 0, 'f', 0);
    }
    if (engine->state().cpu >= 0)
        parts << tr ("Audio %1 %").arg (engine->state().cpu, 0, 'f', 0);
    parts << tr ("App %1 MB").arg (stats.appMB(), 0, 'f', 0);
    statsLabel->setText (parts.join ("   |   "));
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
    if (outputBox->count() != a.outputs.size() || outputBox->currentText() != a.device)
    {
        const QSignalBlocker block (outputBox);
        outputBox->clear();
        outputBox->addItems (a.outputs);
        outputBox->setCurrentText (a.device);
    }
}

// --------------------------------------------------------------------- Maschine : boutons

void MainWindow::onMaschineButton (const QString& name, bool down)
{
    if (name == "tempo")
        maschine->setProperty ("tempoHeld", down);
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
        transport->playing() ? transport->stop() : transport->play();
    else if (name == "stop")
    {
        transport->stop();
        engine->panic();
    }
    else if (name.startsWith ("group_") && name.size() == 7)
    {
        const int i = name.at (6).toLatin1() - 'a';                // A-H ; Shift = A2-H2
        const bool bank2 = shift ? state.group < 8 : state.group >= 8;
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
        transport->playing() ? transport->stop() : transport->play();
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
    transport->stop();
    maschine->close();                               // la Maschine revient au logiciel NI
    engine->stop();
    QMainWindow::closeEvent (event);
}
