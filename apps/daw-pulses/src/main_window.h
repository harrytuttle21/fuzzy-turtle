// Fuzzy John DAW Pulses — fenetre principale (squelette Qt).
// Disposition inspiree de la Maschine : deux "ecrans" en haut, groupes a gauche avec le
// transport en dessous, pads a droite, clavier en bas. Licence : GPLv3.
#pragma once
#include "generate_dialog.h"
#include "keyboard_view.h"
#include "project.h"
#include "sequencer.h"
#include "session.h"
#include "system_stats.h"

#include <QHash>
#include <QMainWindow>
#include <QTimer>

#include <array>
#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>

class EngineLink;
class MaschineLink;
class MixerView;
class PatternView;
class QStackedWidget;
class QDockWidget;
class QToolBar;
class QMenu;
class QCheckBox;
class PadGrid;
class PianoKeys;
class Transport;
class QComboBox;
class QFrame;
class QLabel;
class QPushButton;
class QMimeData;
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
    QWidget* buildTransportBar();
    QWidget* buildMixer();
    QWidget* buildLeftPanel();                   // onglets MIXER / PATTERN
    void showLeftTab (int index);
    void openSettings (int page = 0);

    // Projets .fuzzy (menu File)
    void newProject();
    void openProject();
    bool openProjectFile (const QString& path, bool quiet = false);
    bool saveProject();
    bool saveProjectAs (bool withSamples = false);   // choix de l'emplacement ; avec ses samples copies
    bool maybeSave();                                // projet modifie : enregistrer ?
    void projectChanged();                           // l'app reprend le projet (patterns, volumes, tempo)
    void updateTitle();

    // Panneaux deplacables (Pads, Groups, Keyboard, Status) et vues enregistrees
    QDockWidget* makeDock (const QString& title, const QString& name, QWidget* content);
    void buildViewMenu (QMenu* menu);
    void applyView (const QString& name);
    void saveViewAs();
    QByteArray defaultLayout;                        // disposition par defaut (vue Default)
    QString projectsDir() const;
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
    seq::Sequencer sequencer;
    EngineLink* engine = nullptr;
    Transport* transport = nullptr;
    MaschineLink* maschine = nullptr;
    SystemStats stats;
    QTimer statsTimer;
    Project project;
    QTimer projectTimer;                             // surveille les modifications (titre « * »)
    QLabel* projectLabel = nullptr;
    QDockWidget* padsDock = nullptr;
    QDockWidget* groupsDock = nullptr;
    QDockWidget* keysDock = nullptr;
    QDockWidget* statusDock = nullptr;
    QToolBar* transportBar = nullptr;
    bool projectModified = false;

    PadGrid* pads = nullptr;
    PianoKeys* piano = nullptr;
    std::array<QPushButton*, 8> groupButtons {};     // 8 boutons, comme la Maschine (A-H ou A2-H2)
    QPushButton* bankButton = nullptr;
    QPushButton* mixBankButton = nullptr;            // la meme bascule, dans le Mixer               // passe a A2-H2 (Shift + groupe sur la Maschine)
    QPushButton* playButton = nullptr;               // lecture / pause
    QPushButton* recButton = nullptr;
    MixerView* mixer = nullptr;
    PatternView* patternView = nullptr;
    QDockWidget* patternDock = nullptr;
    QDockWidget* mixerDock = nullptr;
    QDockWidget* rollDock = nullptr;
    QDockWidget* keysTabDock = nullptr;              // onglet Keyboard (grand clavier, gammes, accords)
    KeyboardView* keyboardView = nullptr;
    QLabel* keyInfo = nullptr;                       // (plus utilise : remplace par les cases ci-dessous)
    QSpinBox* octaveSpin = nullptr;                  // onglet Keyboard : octave et velocite, comme le BPM
    QSpinBox* velocitySpin = nullptr;
    void showAbout();
    void releaseAllKeys();                           // l'app perd la main : plus aucune note tenue
    void transposePattern (int semitones);           // selection (ou case du curseur) : demi-tons / octaves
    void transposeRoll (int semitones);              // piano roll : selection, sinon la derniere note
    // Copier / deplacer un son (sample, synthe, mode, boucle, reglages) ou un groupe entier
    struct SoundCopy
    {
        bool valid = false;
        QString sample;
        int mode = 0, release = 150, synth = -1, volume = 100, pan = 64, pitch = 64, cutoff = 90;
        float loopStart = 0.0f, loopEnd = 1.0f;
        bool reverse = false;
    };
    SoundCopy takeSound (int group, int pad) const;
    void putSound (int group, int pad, const SoundCopy& s);
    void emptySound (int group, int pad);            // pad vide (aucun instrument)
    bool soundIsEmpty (int group, int pad) const;
    enum class Transfer { Copy, Move, Swap };
    void transferSound (int fromGroup, int fromPad, int toGroup, int toPad, Transfer how);
    void transferGroup (int from, int to, Transfer how);
    void soundDropMenu (int fromGroup, int fromPad, int toPad, const QPoint& at);   // pad lache sur un pad
    void pickPadDialog (int fromGroup, int fromPad, int toGroup, bool cut = false);   // pad lache sur un groupe
    bool confirmGroup (int from, int to, Transfer how);
    void groupDropped (int buttonIndex, const QMimeData* mime);
    SoundCopy soundClip;                             // « Copy Sound » : son en attente de collage
    int soundClipGroup = -1, soundClipPad = 0;       // d'ou il vient (« Cut » : a vider au collage)
    bool soundClipCut = false;
    int groupClip = -1;                              // « Copy Group »
    bool groupClipCut = false;
    int dupGroup = -1, dupPad = 0;                   // Maschine : DUPLICATE + source + cible
    std::atomic<bool> duplicateHeld { false };
    QPushButton *groupUnmute = nullptr, *groupUnsolo = nullptr;
    QPoint groupPress { -1, -1 };                    // glisser un bouton de groupe
    int groupPressIndex = -1;
    bool editMode = false;                           // Pattern : Entree / Suppr ajoutent, effacent
    QPushButton* editButton = nullptr;
    QCheckBox* reverseBox = nullptr;                 // onglet Sound : sample a l'envers
    std::array<int, session::kGroups * 16> sentReverse {};   // deja envoye au moteur (-1 = jamais)
    music::KeySettings keySettings;
    QHash<int, QList<int>> keyChords;                // touche -> notes jouees (accord compris)
    bool chordMerge = false;                         // enregistrement : ajouter a l'accord de la case
    int viewBefore = -1;                             // Maschine : 2e appui sur une vue = retour
    int currentView() const;                         // 0 Mixer, 1 Pattern, 2 Piano Roll, 3 Keyboard
    void toggleView (int index, const QString& name);
    QWidget* buildKeyboardTab();
    // Onglet Sound : reglages du son choisi (source, lecture, volume, pan, pitch, cutoff)
    QDockWidget* soundDock = nullptr;
    QWidget* buildSoundTab();
    void refreshSoundPanel();
    void soundKnob (int index, int delta);           // encodeurs de la Maschine (1-8, comme le pilote)
    // Pads : PAD MODE (16 sons) ou KEYBOARD (le son choisi sur 16 notes, gamme et accords)
    std::atomic<bool> padKeyboard { false };
    std::array<std::atomic<int>, 17> padKey {};      // note jouee par chaque pad en mode KEYBOARD
    int padNote (int pad) const;                     // note d'un pad en mode KEYBOARD
    void setPadKeyboard (bool on);
    struct SoundWidgets
    {
        QLabel* title = nullptr;
        QComboBox* synth = nullptr;
        QLabel* sample = nullptr;
        QPushButton* removeSample = nullptr;
        QComboBox* mode = nullptr;
        QPushButton* editLoop = nullptr;
        QSpinBox* release = nullptr;
        std::array<QSlider*, 4> sliders {};          // Volume, Pan, Pitch, Cutoff
        std::array<QLabel*, 4> values {};
    } soundUi;
    void playKey (int key, int velocity, bool down); // touche de clavier : note ou accord
    std::atomic<bool> maschineShift { false };       // Shift tenu sur la Maschine (lu par la tache des pads)
    void shiftPad (int pad);                         // Shift + pad : fonctions de la serigraphie
    void switchBank();                               // A-H <-> A2-H2
    void patternMenu (int row, int track, const QPoint& at);   // clic droit dans la grille
    void noteOptions (int row, int track);           // vue avancee d'une note
    void fillFrame (int every);                      // la zone tracee : la note du son choisi
    void moveCells (const QList<QPoint>& cells, int dr, int dt, bool copy);
    qint64 lastShiftPadMs = 0;
    int lastShiftPad = 0;
    class PianoRollView* pianoRoll = nullptr;
    QLabel* rollTitle = nullptr;
    QWidget* buildPianoRoll();
    bool patternShown() const;                       // le panneau Pattern est-il visible ?
    bool rollShown() const;                          // et le Piano Roll ?
    QSpinBox* lengthBox = nullptr;
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
    QComboBox* layoutBox = nullptr;
    QLabel* octaveLabel = nullptr;
    QFrame* leftScreen = nullptr;

    std::array<std::atomic<int>, 17> padGroup {};    // groupe de la frappe : la fin de note y va
    std::array<std::atomic<int>, 17> padSent {};     // note envoyee au moteur (pitch compris)
    struct LiveNote { int sound, sent, group; };
    QHash<int, LiveNote> liveNotes;                  // clavier : note jouee -> ce qui a ete envoye
    QHash<int, int> keyNotes;                        // touche tenue -> note (clavier de l'ordinateur)
    // Enregistrement en cours (REC + lecture) : la duree est donnee au relachement
    struct RecNote { seq::Sequencer::Written where; std::chrono::steady_clock::time_point start; bool active = false; };
    std::atomic<bool> recArmed { false };
    std::mutex recLock;                              // pads (tache de la Maschine) et interface
    std::array<RecNote, 17> padRec {};
    QHash<int, RecNote> noteRec;                     // note du clavier -> case ecrite
    void recordStart (RecNote& slot, int track, int note, int velocity);

    // Annuler / refaire : le pattern d'un groupe tel qu'il etait avant chaque modification
    struct UndoStep { int group; seq::Pattern pattern; };
    std::mutex undoLock;                             // l'enregistrement vient aussi de la Maschine
    std::vector<UndoStep> undoStack, redoStack;
    std::array<std::atomic<bool>, 16> takeSaved {};  // une prise REC = une seule annulation par groupe
    void remember (int group);
    void undo (bool redoIt);
    void clearHistory();
    // Copier / coller : notes relatives a la premiere ligne copiee
    struct Clip { int row, track; seq::Cell cell; };
    std::vector<Clip> clipboard;
    void copySelection (bool cut);
    void paste();
    void selectAll();
    void doublePattern();
    void showPatternState();                         // longueur et vue apres un changement

    // Mute / solo (groupes et sons) ; MUTE / SOLO tenus sur la Maschine
    std::atomic<bool> muteHeld { false }, soloHeld { false };
    void toggleGroup (int group, bool solo);
    void toggleSound (int group, int sound, bool solo);
    void refreshMuteSolo();                          // pads, pattern, mixeur, groupes, Maschine
    QPushButton* padsUnmute = nullptr;               // « Unmute all » / « Unsolo all »
    QPushButton* padsUnsolo = nullptr;
    QPushButton* mixUnmute = nullptr;
    QPushButton* mixUnsolo = nullptr;
    QWidget* muteSoloRow (QPushButton*& unmute, QPushButton*& unsolo, bool groups);

    // Samples des pads (envoyes au moteur ; ce qui a deja ete envoye, pour n'envoyer que les changements)
    std::array<QString, session::kGroups * 16> sentSamples;
    std::array<QString, session::kGroups * 16> sentLoops;   // « mode;debut;fin » deja envoye
    void editLoop (int pad);
    void openGenerate();                             // remplissage aleatoire du son choisi
    void importMidi (const QString& path, int sound);   // fichier .mid -> pattern du groupe
    void addNoteAtCursor();                          // Entree / appui molette : la note du son choisi
    void quickVariation();                           // VARIATION : nouveau tirage, derniers reglages
    void cyclePlayMode();                            // SAMPLING : One-shot -> Gate -> Loop
    void toast (const QString& text);                // message 2 s (ecran droit de la Maschine + barre d'etat)
    QString toastText;
    qint64 toastUntil = 0;
    std::optional<GenerateSettings> lastGenerate;   // derniers reglages de Generate
    void setPadSample (int group, int sound, const QString& path);
    void syncSamples (bool all = false);
    void loadSampleDialog (int pad);
    QString padName (int group, int sound) const;    // nom du fichier, sinon nom du son
    void recordEnd (RecNote& slot);

    int beatShown = -1;
    bool tapTurned = false;
};
