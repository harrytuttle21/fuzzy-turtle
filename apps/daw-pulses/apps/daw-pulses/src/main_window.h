// Fuzzy John DAW Pulses — fenetre principale (squelette Qt).
// Disposition inspiree de la Maschine : deux "ecrans" en haut, groupes a gauche avec le
// transport en dessous, pads a droite, clavier en bas. Licence : GPLv3.
#pragma once
#include "session.h"
#include "system_stats.h"

#include <QHash>
#include <QMainWindow>
#include <QTimer>

#include <array>
#include <atomic>

class EngineLink;
class MaschineLink;
class PadGrid;
class PianoKeys;
class Transport;
class QComboBox;
class QFrame;
class QLabel;
class QPushButton;
class QSlider;
class QSpinBox;

struct Options
{
    bool maschine = true;        // --no-maschine : ne pas ouvrir la Maschine
    bool engine = true;          // --no-engine : ne pas demarrer le moteur audio
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow (const Options& options, QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    bool eventFilter (QObject* watched, QEvent* event) override;
    void closeEvent (QCloseEvent* event) override;

private:
    QWidget* buildTopBar();
    QWidget* buildScreens();
    QWidget* buildGroupBlock();
    QWidget* buildTransportBlock();
    QWidget* buildKeyboardPanel();
    void applyTheme();

    // Jouer (utilisable depuis la tache des pads de la Maschine)
    void playPad (int pad, int velocity, bool down);
    void playNote (int note, int velocity, bool down);

    void selectGroup (int group);
    void selectSound (int sound);
    void setOctave (int octave);
    void refreshAll();                       // ecran de l'ordinateur + Maschine
    void refreshGroups();
    void refreshEngine();
    void refreshMaschine();
    void refreshStats();
    void onMaschineButton (const QString& name, bool down);
    bool handleKey (QKeyEvent* e, bool down);
    void updateKeyLabels();

    Options opts;
    session::State state;
    EngineLink* engine = nullptr;
    Transport* transport = nullptr;
    MaschineLink* maschine = nullptr;
    SystemStats stats;
    QTimer statsTimer;

    PadGrid* pads = nullptr;
    PianoKeys* piano = nullptr;
    std::array<QPushButton*, 8> groupButtons {};     // 8 boutons, comme la Maschine (A-H ou A2-H2)
    QPushButton* bankButton = nullptr;               // passe a A2-H2 (Shift + groupe sur la Maschine)
    QPushButton* playButton = nullptr;
    QPushButton* metroButton = nullptr;
    QPushButton* keysButton = nullptr;               // jeu au clavier de l'ordinateur : marche / arret
    QSpinBox* bpmBox = nullptr;
    QSlider* metroVolume = nullptr;
    QLabel* beatLabel = nullptr;
    QLabel* soundLabel = nullptr;
    QLabel* engineLabel = nullptr;
    QLabel* maschineLabel = nullptr;
    QLabel* statsLabel = nullptr;
    QPushButton* maschineButton = nullptr;
    QComboBox* outputBox = nullptr;
    QComboBox* layoutBox = nullptr;
    QLabel* octaveLabel = nullptr;
    QFrame* leftScreen = nullptr;

    std::array<std::atomic<int>, 17> padGroup {};    // groupe de la frappe : la fin de note y va
    QHash<int, int> keyNotes;                        // touche tenue -> note (clavier de l'ordinateur)
    int beatShown = -1;
    bool tapTurned = false;
};
