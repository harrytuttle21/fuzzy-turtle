// Fuzzy John DAW Pulses — options d'une note du pattern (vue avancee) : notes de l'accord,
// velocite, duree, timing fin. Licence : GPLv3.
#pragma once
#include "sequencer.h"

#include <QDialog>

class QLineEdit;
class QSpinBox;
class QLabel;

class NoteDialog : public QDialog
{
    Q_OBJECT
public:
    NoteDialog (const seq::Cell& cell, int row, int track, int maxLength, QWidget* parent = nullptr);
    seq::Cell cell() const;                          // case modifiee (vide si aucune note valable)

private:
    void check();
    QLineEdit* notes = nullptr;
    QSpinBox *velocity = nullptr, *length = nullptr, *timing = nullptr;
    QLabel* problem = nullptr;
    seq::Cell base;
};
