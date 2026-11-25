// Fuzzy John DAW Pulses — import de fichiers MIDI (.mid, formats 0 et 1) dans le pattern
// d'un groupe : tout sur un son (accords), un son par canal / piste, ou batterie General MIDI.
// Lecteur ecrit ici, sans bibliotheque. Licence : GPLv3.
#pragma once
#include "sequencer.h"
#include "session.h"

#include <QDialog>
#include <QList>
#include <QString>

namespace midi {

struct Note
{
    qint64 tick = 0, length = 0;                     // en « ticks » du fichier
    int pitch = 60, velocity = 100, channel = 0, track = 0;
};

struct File
{
    int format = 0, division = 480;                  // division : ticks par noire
    double bpm = 0;                                  // premier tempo du fichier (0 = aucun)
    QList<Note> notes;
    QString error;                                   // vide si tout va bien
};

File read (const QString& path);

enum class Target { OneSound, PerChannel, Drums };

struct Placed { int notes = 0, skipped = 0, length = 16; };   // notes posees, hors pattern, longueur

// Pose les notes dans le pattern (4 lignes par noire, timing fin au 1/24 de ligne)
Placed apply (const File& file, seq::Pattern& pattern, Target target, int firstSound, bool replace);

} // namespace midi

class QComboBox;
class QCheckBox;
class QSpinBox;

class MidiImportDialog : public QDialog
{
    Q_OBJECT
public:
    MidiImportDialog (const midi::File& file, const QString& name, int sound, QWidget* parent = nullptr);
    midi::Target target() const;
    int firstSound() const;
    bool replace() const;
    bool useTempo() const;

private:
    QComboBox* targetBox = nullptr;
    QSpinBox* soundBox = nullptr;
    QComboBox* modeBox = nullptr;
    QCheckBox* tempoBox = nullptr;
};
