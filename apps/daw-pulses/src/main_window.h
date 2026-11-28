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

#include <QDialog>
#include <QPointer>
#include <QLabel>
#include <QHash>
#include <functional>
#include <QJsonObject>
#include <QMainWindow>
#include <QSet>
#include <functional>
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
class QDialog;
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
    QWidget* buildSoundMixer();                      // onglet Sounds : les 16 sons du groupe choisi
    MixerView* soundMixer = nullptr;
    std::function<void()> soundMixRefresh;
    QDockWidget* soundMixDock = nullptr;
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
    QByteArray defaultLayout, factoryLayout;                        // disposition par defaut (vue Default)
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
    QDockWidget* transportDock = nullptr;
    class QSpinBox* swingBox = nullptr;              // Transport : SWING 0-100 %           // Transport : fenetre a part, sous les groupes
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
    QComboBox* rootCombo = nullptr;                  // onglet Keyboard : tonalite (Shift + pad 13 / 14)
    QSpinBox* octaveSpin = nullptr;                  // onglet Keyboard : octave et velocite, comme le BPM
    QSpinBox* velocitySpin = nullptr;
    void showAbout();
    void fixSeparators();                            // macOS : separateurs restes par-dessus un panneau
    void showDisplayDiagnostics();                   // F12 : nommer un trait parasite
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
        int poly = 4;
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
    std::vector<std::pair<int, SoundCopy>> soundClips;   // « Copy Sound » : pad d'origine -> son (un ou plusieurs)
    int soundClipGroup = -1;                         // groupe d'origine (« Cut » : sons a vider au collage)
    bool soundClipCut = false;
    std::vector<int> groupClips;                     // « Copy Group » : un ou plusieurs groupes
    bool groupClipCut = false;
    QSet<int> groupChosen;                           // groupes choisis (Shift / Cmd + clic)
    int groupAnchor = -1;
    QList<int> groupTargets (int g) const;           // groupes choisis si g en fait partie, sinon g
    void copySounds (const QList<int>& pads, bool cut);
    void pasteSounds (int pad);
    void copyGroups (const QList<int>& groups, bool cut);
    void pasteGroups (int group);
    // Ou agissent Cmd+C / X / V : la derniere zone cliquee
    enum class Area { Pattern, Pads, Groups, Song } lastArea = Area::Pattern;
    int lastPad = 1;                                 // dernier pad clique (cible de Coller)
    int dupGroup = -1, dupPad = 0;                   // Maschine : DUPLICATE + source + cible
    std::atomic<bool> duplicateHeld { false };
    QPushButton *groupUnmute = nullptr, *groupUnsolo = nullptr;
    QPoint groupPress { -1, -1 };                    // glisser un bouton de groupe
    int groupPressIndex = -1;
    bool editMode = false;                           // Pattern : Entree / Suppr ajoutent, effacent
    QPushButton* editButton = nullptr;
    QPushButton *rollEditButton = nullptr, *rollSelectButton = nullptr;   // Piano Roll : Edit / Select
    QPushButton* padModeButton = nullptr;            // sous les pads : Keyboard Mode / Pad Mode
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
    std::atomic<bool> padChords { false };           // mode CHORDS : chaque pad joue un accord (menu Chords)
    int padChordKey (int pad) const;                 // touche jouee par un pad en mode CHORDS (-1 = aucun accord)
    QString padChordLabel (int pad) const;           // nom de l'accord du pad
    void chordPadMenu (int pad, const QPoint& at);   // mode Chords, clic droit sur un pad : accords User
    void chordDropMenu (int fromPad, int toPad, const QPoint& at);   // set User : pad lache sur un pad
    std::vector<int> chordClip;                      // Copy / Paste d'un accord User
    bool chordClipFull = false;
    QString padLabel (int pad) const;                // texte d'un pad selon le mode (son, note, accord)
    void setPadChords (bool on);
    void useMelodicSound();                          // KEYBOARD / CHORDS : pas une batterie de synthese
    void refreshPadModeButtons();
    QPushButton* chordsModeButton = nullptr;
    QPushButton* padsOnlyButton = nullptr;           // sous les pads : Pad Mode         // sous les pads : Chords Mode / Pad Mode
    QPushButton* chordMenuButton = nullptr;          // onglet Keyboard : menu des accords
    std::function<void()> saveKeySettings;           // enregistre gamme / accords et rafraichit
    std::array<std::atomic<bool>, 17> padHeld {};     // pads tenus (souris ou Maschine)
    bool rollKeyHeld = false;                        // clavier du Piano Roll tenu
    void liveCheck();                                // plus rien de tenu : filet de securite du moteur
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
    void playKey (int key, int velocity, bool down, bool chords = true);   // touche de clavier : note ou accord
    std::atomic<bool> selectHeld { false };          // SELECT tenu (vue Song : SELECT + pad = ajouter la scene)
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
    QWidget* buildTracker();                         // vue TRACKER (onglet a cote de Pattern)
    bool trackerKey (QKeyEvent* e);                  // touches de la vue Tracker
    void trackerWrite (int row, int track, int note);
    bool trackerShown() const;
    void placeTracker();                             // ajoute le Tracker aux dispositions d'avant
    class TrackerView* tracker = nullptr;
    class PatternBar* patternBar = nullptr;          // barre PATTERNS / SCENES
    class SceneView* sceneView = nullptr;            // onglet Scenes
    class SceneGridView* sceneGrid = nullptr;        // onglet Scenes, bouton Grid (facon Maschine)
    QPushButton* sceneLockBtn = nullptr;             // Scenes : Lock (allume = ecriture verrouillee, lecture libre)
    bool scenesLocked() const;
    int gridScene = 0;                               // Scene Grid : la derniere scene choisie
    std::atomic<bool> gridOnScreen { false };         // pour la tache des pads (lu sans toucher aux widgets)
    bool gridShown() const;                          // la Scene Grid est a l'ecran
    void gridChooseScene (int scene, bool fromMaschine = false);
    void gridToggle (int group, int pattern, bool fromMaschine = false);   // entre / sort de la scene
    QDockWidget* sceneDock = nullptr;
    QWidget* buildScenes();
    class SongView* songView = nullptr;              // onglet Song (morceau)
    QDockWidget* songDock = nullptr;
    QWidget* buildSong();
    // Colonne des groupes, a gauche des onglets (toutes les vues sauf le Mixer) : M / S / nom,
    // puis les 16 patterns de la banque du groupe choisi
    QWidget* buildGroupColumn();
    QDockWidget* groupColDock = nullptr;
    QDockWidget* patternSelDock = nullptr;           // les 16 cases de patterns (sous la colonne, detachable)
    QWidget* patternSelPanel = nullptr;
    void updateColumnFor();
    // ^ colonne selon la vue de devant (Mixer / Scenes / Song)
    int counterRow = 0;
    std::function<void()> groupColRefresh;
    QPushButton *colUnmute = nullptr, *colUnsolo = nullptr;
    QToolBar* transportTool = nullptr;               // le Transport : barre en haut (detachable, flottante)
    QLabel* counterLabel = nullptr;                  // compteur de lecture
    void refreshCounter (int row);
    void stopPressed();
    void rememberRecent (const QString& file);       // File > Open Recent (10 derniers)
    void fillRecentMenu (QMenu* menu);
    void hookTabBars();                              // onglets des panneaux : glisser vers l'autre barre
    bool tabDrag (QTabBar* bar, QEvent* event);
    QDockWidget* tabDragDock = nullptr;
    QPoint tabDragFrom;
    bool tabDragging = false;                              // Song : pause (reprise au meme endroit) ; sinon arret
    void restartPressed();
    void stopSpLoops();                              // Loop SP : tout arreter
    void refreshSpLoops();                           // pads en boucle : clignotement
    QTimer* spBlink = nullptr;
    qint64 lastStopMs = 0;                           // Stop deux fois de suite : couper les boucles SP
    bool spPhase = false;                           // Song : depuis le debut du morceau
    // Ligne d'aide (en bas de la fenetre) : l'aide de la vue sous la souris
    struct HelpEntry { QPointer<QLabel> label; QString view; };
    std::vector<HelpEntry> helps;
    QLabel* helpLine = nullptr;
    void registerHint (QLabel* hint, const QString& view);
    void showHelpFor (QWidget* w);
    void showShortcuts();                            // Help > Keyboard shortcuts
    std::function<void()> songsRefresh;              // la liste des songs suit (annuler, ouvrir...)
    QPushButton* songButton = nullptr;               // SONG (vue Song) et son reflet dans le Transport
    QPushButton* songTransportButton = nullptr;
    QPushButton* loopButton = nullptr;
    void songEdit (const QString& label, const std::function<void (std::vector<seq::SongEntry>&)>& change);
    void setSongMode (bool on);
    void addSongScene (int scene, int before);      // -1 : apres l'entree choisie
    void songSceneMenu (const QPoint& at, const std::function<void (int)>& picked);   // liste des scenes
    void songCellMenu (int entry, int group, const QPoint& at);   // case groupe x bloc : pattern de la scene
    void songDuplicateBlocks();                      // blocs choisis : dupliques juste apres
    void songDeleteBlocks();
    void songCopyBlocks (bool cut);                  // Cmd+C / X dans la Song
    void songPasteBlocks();                          // Cmd+V : apres le bloc choisi
    bool songKey (QKeyEvent* e);                     // Del, fleches, Entree (la Song a la main)
    std::vector<seq::SongEntry> songClip;            // blocs copies
    void sceneColorMenu (QMenu* menu, int scene);    // couleur d'une scene (12 teintes + automatique)
    class QListWidget* songSceneList = nullptr;      // liste des scenes a gauche de la Song (a glisser)
    std::function<void()> songSceneListRefresh;
    int freeScene() const;                           // premiere case de scene libre (-1 : aucune)
    int newScene (bool fromPlaying);                 // nouvelle scene vide (ou ce qui joue), -1 si plein
    std::atomic<bool> arrangerMode { false };        // Maschine : vue Song montree (pads = ajouter la scene)
    std::array<int, 4> songKnobAcc {};               // encodeurs 1-4 en mode ARRANGER (pas a pas)
    bool songHalf = false;                           // ARRANGER : repetitions / longueur par demi
    bool arrangerButton (const QString& name);       // boutons de la Maschine quand la vue Song est montree
    void arrangerKnob (int index, int delta);
    void patternsChanged();                          // pattern / scene change : les vues suivent
    void exportGroup();                              // File > Export Group (.fzgroup)
    void importGroup();                              // File > Import Group : remplace le groupe choisi
    void renameGroup (int g);
    void groupContextMenu (int g, const QPoint& at);   // clic droit sur un groupe (bloc ou colonne)                        // nom du groupe (la lettre reste)
    bool exportGroupTo (const QString& path, bool copySamples);
    void importGroupFrom (const QString& path, const QJsonObject& doc);
    QToolBar* patternsToolBar = nullptr;
    std::atomic<bool> patternHeld { false }, sceneHeld { false };   // Maschine : PATTERN / SCENE tenus
    bool holdUsed = false;                           // un pad a ete tape pendant l'appui
    QDialog* openLoopDialog = nullptr;
    QSpinBox* polyBox = nullptr;                     // onglet Sound : polyphonie               // fenetre Edit Loop ouverte (SAMPLING la ferme)
    class TrackerView* rollTracker = nullptr;        // Piano Roll, affichage Tracker (une piste)
    class StepView* stepView = nullptr;              // Piano Roll, affichage Steps
    class QStackedWidget* rollStack = nullptr;       // Roll / Steps / Tracker
    TrackerView* activeTracker() const;              // Tracker visible (onglet ou Piano Roll)
    bool stepsKey (QKeyEvent* e);
    QDockWidget* trackerDock = nullptr;
    QPushButton* trackerEditButton = nullptr;
    QSpinBox* trackerStep = nullptr;
    QSpinBox* trackerLength = nullptr;
    QSpinBox* rollLength = nullptr;                  // Piano Roll : longueur du pattern               // Tracker : longueur (relie a celle de Pattern)
    QString typedDigits;                             // chiffres tapes (velocite / longueur)
    int lastTrackerNote = -1;
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
    // Un pas d'annulation : un pattern, et / ou des sons (copier, deplacer, charger...) et des groupes
    struct GroupSave { int group; seq::GroupPatterns patterns; int volume; QString label; };   // tous ses patterns
    struct UndoStep
    {
        int group = -1;                              // pattern d'un groupe (-1 = aucun)
        int patternIndex = 0;                        // lequel de ses patterns
        seq::Pattern pattern;
        std::vector<std::pair<int, SoundCopy>> sounds;   // index groupe * 16 + pad - 1
        std::vector<GroupSave> groups;               // groupes entiers (pattern + volume)
        bool hasScenes = false;                      // les 64 scenes (gestionnaire de patterns)
        std::vector<seq::Scene> scenes;
        std::vector<seq::SongSlot> songs;            // les songs (avec les scenes)
        int songIndex = 0;
        QString label;
    };
    void rememberPatterns (int group, const QString& label);   // tous les patterns du groupe + les scenes
    void openPatternManager();                       // Pattern > List : la liste du groupe
    void openSceneList();                            // Scenes > List : les scenes et leurs patterns
    class PaletteView* palette = nullptr;            // Scenes > Palette : carres a glisser
    QDialog* sceneListDialog = nullptr;
    std::function<void()> sceneListRefresh;
    std::function<void()> patternManagerRefresh;     // la fenetre ouverte suit les changements
    QDialog* patternManagerDialog = nullptr;
    void rememberSounds (const std::vector<std::pair<int, int>>& pads, const std::vector<int>& groups = {}, const QString& label = {});
    UndoStep capture (const UndoStep& like) const;   // l'etat actuel des memes elements (pour refaire)
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
    QWidget* muteSoloRow (QPushButton*& unmute, QPushButton*& unsolo, bool groups, bool vertical = false);

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
    void setPadSample (int group, int sound, const QString& path, bool undoable = true);
    void syncSamples (bool all = false);
    void loadSampleDialog (int pad);
    QString padName (int group, int sound) const;    // nom du fichier, sinon nom du son
    void recordEnd (RecNote& slot);

    int beatShown = -1;
    bool tapTurned = false;
};
