// Fuzzy John DAW Pulses — options d'une note du pattern. Licence : GPLv3.
#include "note_dialog.h"
#include "piano_roll.h"
#include "session.h"

#include <QDialogButtonBox>
#include <QRegularExpression>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
// « C4 E4 G4 » ou « 60 64 67 » -> notes MIDI (vide si un mot n'est pas une note)
QList<int> parseNotes (const QString& text, bool& ok)
{
    ok = true;
    QList<int> out;
    for (const QString& word : text.split (QRegularExpression ("[\\s,;]+"), Qt::SkipEmptyParts))
    {
        bool number = false;
        int n = word.toInt (&number);
        if (! number)
        {
            n = -1;
            for (int k = 0; k < 128; ++k)
                if (session::noteName (k).compare (word, Qt::CaseInsensitive) == 0)
                    n = k;
        }
        if (n < 0 || n > 127)
        {
            ok = false;
            return {};
        }
        if (! out.contains (n))
            out << n;
    }
    return out;
}
}

NoteDialog::NoteDialog (const seq::Cell& c, int row, int track, int maxLength, QWidget* parent)
    : QDialog (parent), base (c)
{
    setWindowTitle (tr ("Note — line %1, track %2").arg (row, 2, 16, QChar ('0')).toUpper().arg (track + 1, 2, 10, QChar ('0')));
    auto* col = new QVBoxLayout (this);
    auto* form = new QFormLayout;
    QStringList names;
    for (int n : PianoRollView::notesOf (c))
        names << session::noteName (n);
    notes = new QLineEdit (names.join (' '));
    notes->setToolTip (tr ("One note, or several for a chord (up to 8): C4 E4 G4"));
    form->addRow (tr ("Notes"), notes);
    velocity = new QSpinBox;
    velocity->setRange (1, 127);
    velocity->setValue (c.empty() ? 100 : c.velocity);
    form->addRow (tr ("Velocity"), velocity);
    length = new QSpinBox;
    length->setRange (1, std::max (1, maxLength));
    length->setValue (c.empty() ? 1 : c.length);
    length->setSuffix (tr (" line(s)"));
    form->addRow (tr ("Length"), length);
    timing = new QSpinBox;
    timing->setRange (-seq::kTiming / 2, seq::kTiming / 2);
    timing->setValue (c.offset);
    timing->setSuffix (tr (" / 24 of a line"));
    timing->setToolTip (tr ("Negative = a little early, positive = a little late"));
    form->addRow (tr ("Timing"), timing);
    col->addLayout (form);
    problem = new QLabel;
    problem->setObjectName ("hint");
    col->addWidget (problem);
    auto* buttons = new QDialogButtonBox (QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect (buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect (buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    col->addWidget (buttons);
    connect (notes, &QLineEdit::textChanged, this, [this, buttons]
    {
        check();
        buttons->button (QDialogButtonBox::Ok)->setEnabled (problem->text().isEmpty());
    });
    check();
}

void NoteDialog::check()
{
    bool ok = false;
    const QList<int> list = parseNotes (notes->text(), ok);
    problem->setText (! ok ? tr ("Unknown note: write them like C4, F#3, or as numbers 0-127")
                      : list.size() > seq::kChordExtra + 1 ? tr ("8 notes at most") : QString());
}

seq::Cell NoteDialog::cell() const
{
    bool ok = false;
    QList<int> list = parseNotes (notes->text(), ok);
    if (! ok || list.isEmpty())
        return {};
    while (list.size() > seq::kChordExtra + 1)
        list.removeLast();
    seq::Cell c { list[0], velocity->value(), length->value(), timing->value() };
    return PianoRollView::withNotes (c, list);
}
