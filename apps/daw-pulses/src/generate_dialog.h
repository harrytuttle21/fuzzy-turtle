// Fuzzy John DAW Pulses — remplissage aleatoire des notes d'un son (sa piste du pattern),
// avec quelques reglages : densite, grille, accents, velocite, duree, gamme, accords.
// Chaque clic sur le de refait un tirage qu'on entend tout de suite (pattern en lecture) ;
// OK garde le resultat, Cancel remet le pattern d'avant. Licence : GPLv3.
#pragma once
#include "sequencer.h"
#include "session.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QSlider;
class QSpinBox;
class QLabel;
class QPushButton;

struct GenerateSettings
{
    int density = 40;                                // % de chances d'une note par pas
    int grid = 1;                                    // une note possible toutes les 1, 2 ou 4 lignes
    bool accents = true;                             // plus de notes et plus fortes sur les temps
    int velMin = 70, velMax = 120;
    int lenMin = 1, lenMax = 2;                      // duree en lignes
    bool fixedPitch = false;                         // batterie : toujours la note du pad
    int scale = 1;                                   // voir GenerateDialog::scaleNames()
    int root = 48;                                   // note de depart (do 3)
    int octaves = 1;
    bool walk = true;                                // melodie : notes voisines
    int chords = 0;                                  // % d'accords (3 notes de la gamme)
    bool replace = true;                             // remplacer les notes du son, sinon ajouter
};

class GenerateDialog : public QDialog
{
    Q_OBJECT
public:
    GenerateDialog (seq::Sequencer& sequencer, const session::State& state, int group, int sound,
                    QWidget* parent = nullptr);
    static QStringList scaleNames();
    GenerateSettings settings() const { return read(); }
    static GenerateSettings defaults (int sound);    // reglages de depart selon le son
    // Tirage : la piste du son dans le pattern (base = le pattern d'avant)
    static seq::Pattern generate (const seq::Pattern& base, int track, int sound,
                                  const GenerateSettings& s, unsigned seed);

signals:
    void playToggle();                               // ▶ / ❚❚ sans fermer la fenetre

protected:
    void reject() override;                          // Cancel : le pattern d'avant

private:
    void roll (bool newSeed);
    GenerateSettings read() const;

    seq::Sequencer& sequencer;
    const int group, sound;
    const seq::Pattern before;
    unsigned seed = 1;
    QSlider* density = nullptr;
    QComboBox* grid = nullptr;
    QCheckBox* accents = nullptr;
    QSpinBox *velMin = nullptr, *velMax = nullptr, *lenMin = nullptr, *lenMax = nullptr;
    QCheckBox* fixedPitch = nullptr;
    QPushButton* scale = nullptr;                    // menu des gammes (categories, personnalisees)
    int scaleValue = 1;
    QComboBox* root = nullptr;
    QSpinBox* octaves = nullptr;
    QCheckBox* walk = nullptr;
    QSlider* chords = nullptr;
    QComboBox* mode = nullptr;
    QLabel* result = nullptr;
};
