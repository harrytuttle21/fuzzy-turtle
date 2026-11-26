#pragma once
#include "sequencer.h"

#include <QWidget>

#include <functional>

class QComboBox;
class QLabel;
class QPushButton;

// Barre PATTERNS / SCENES (en haut de la fenetre), comme les modes PATTERN et SCENE de la Maschine :
//  - PATTERNS : les 16 patterns de la banque pour le groupe choisi ; clic = le jouer (en lecture :
//    a la fin du pattern en cours, cadre orange en attendant), 2e clic = eteint (silence),
//    clic sur un vide = nouveau pattern ;
//    clic droit = dupliquer, renommer, effacer...
//  - SCENES : un pattern par groupe ; clic sur un vide = y ranger ce qui joue, clic = lancer.
class PatternBar : public QWidget
{
    Q_OBJECT
public:
    PatternBar (seq::Sequencer& sequencer, const session::State& state, QWidget* parent = nullptr);

    void refresh (bool force = false);               // redessine si quelque chose a change
    std::function<bool()> isPlaying;                 // lecture en cours ? (changements en attente)
    std::function<void (int group)> startAudition;   // Play d'un pattern : ce groupe seul, lecture lancee
    std::function<void()> stopPlayback;              // Stop (pendant l'ecoute)
    std::function<void()> endAudition;               // une scene lancee : tous les groupes rejouent
    // Presse-papiers (barre du haut et listes) : un pattern, une scene
    seq::Pattern clipPattern;
    std::string clipPatternName;
    bool clipPatternFull = false;
    seq::Scene clipScene;
    bool clipSceneFull = false;
    int patternBank = 0, sceneBank = 0;

signals:
    void changed();                                  // pattern ou scene change : rafraichir les vues
    void message (const QString& text);              // petit message (barre d'etat / Maschine)
    void aboutToChange (int group);                  // avant de modifier un pattern (annuler)
    void beforeEdit (int group, const QString& label);   // avant de modifier patterns / scenes (annuler tout)

public:
    void choosePattern (int index);                  // aussi pour la Maschine (PATTERN + pad)
    void playPattern (int group, int index);         // Play : allume (jamais n'eteint), lance la lecture
    void chooseScene (int index);                    // SCENE + pad
    void patternMenu (int index, const QPoint& at);
    void sceneMenu (int index, const QPoint& at);
    void storeScene (int index);                     // ranger ce qui joue dans la scene
    void renamePattern (int group, int index);
    void renameScene (int index);
    static QString patternName (const seq::Sequencer& s, int group, int index);   // nom, sinon « Pat3 »
    static QString sceneName (const seq::Sequencer& s, int index);                // nom, sinon « S3 »

private:
    int freeSlot (int from) const;
    bool confirm (const QString& question, const QString& detail);   // Yes / Cancel (Cancel par defaut)                   // premier emplacement libre apres `from`
    QString patternLabel (int g, int index) const;

    seq::Sequencer& sequencer;
    const session::State& state;
    QLabel *patTitle = nullptr, *sceneTitle = nullptr;
    QList<QPushButton*> patButtons, sceneButtons;
    QPushButton *patBankButton = nullptr, *sceneBankButton = nullptr;
    QComboBox *patList = nullptr, *sceneList = nullptr;   // listes deroulantes
    QString lastKey;                                 // pour ne redessiner qu'au besoin
};
