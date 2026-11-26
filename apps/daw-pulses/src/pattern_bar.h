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
    int patternBank = 0, sceneBank = 0;

signals:
    void changed();                                  // pattern ou scene change : rafraichir les vues
    void message (const QString& text);              // petit message (barre d'etat / Maschine)
    void aboutToChange (int group);                  // avant de modifier un pattern (annuler)

public:
    void choosePattern (int index);                  // aussi pour la Maschine (PATTERN + pad)
    void chooseScene (int index);                    // SCENE + pad
    void patternMenu (int index, const QPoint& at);
    void sceneMenu (int index, const QPoint& at);
    void storeScene (int index);                     // ranger ce qui joue dans la scene
    void renamePattern (int group, int index);
    void renameScene (int index);
    static QString patternName (const seq::Sequencer& s, int group, int index);   // nom, sinon « Pat3 »
    static QString sceneName (const seq::Sequencer& s, int index);                // nom, sinon « S3 »

private:
    int freeSlot (int from) const;                   // premier emplacement libre apres `from`
    QString patternLabel (int g, int index) const;

    seq::Sequencer& sequencer;
    const session::State& state;
    QLabel *patTitle = nullptr, *sceneTitle = nullptr;
    QList<QPushButton*> patButtons, sceneButtons;
    QPushButton *patBankButton = nullptr, *sceneBankButton = nullptr;
    QComboBox *patList = nullptr, *sceneList = nullptr;   // listes deroulantes
    QString lastKey;                                 // pour ne redessiner qu'au besoin
};
