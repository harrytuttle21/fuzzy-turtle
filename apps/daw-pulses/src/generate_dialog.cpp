// Fuzzy John DAW Pulses — remplissage aleatoire. Licence : GPLv3.
#include "generate_dialog.h"
#include "piano_roll.h"
#include "keyboard_view.h"
#include "music_tables.h"
#include "scale_menu.h"
#include <QMenu>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>

GenerateSettings GenerateDialog::defaults (int sound)
{
    GenerateSettings s;
    s.fixedPitch = sound <= 4;                       // batterie : la note du pad
    s.lenMax = sound <= 4 ? 1 : 2;
    s.scale = 1;
    s.root = std::clamp (session::defaultNote (sound), 24, 96);
    return s;
}

QStringList GenerateDialog::scaleNames()
{
    // Les memes gammes que le pilote de la Maschine (banques Main, Major, Minor, Modes...)
    QStringList out;
    for (int i = 0; i < (int) music::scales().size(); ++i)
        out << music::scaleLabel (i);
    return out;
}

seq::Pattern GenerateDialog::generate (const seq::Pattern& base, int track, int sound,
                                       const GenerateSettings& s, unsigned seed)
{
    QRandomGenerator rng (seed);
    seq::Pattern p = base;
    if (s.replace)
        for (int r = 0; r < seq::kMaxRows; ++r)
            p.rows[(size_t) r][(size_t) track] = {};

    // Notes possibles : la gamme sur l'etendue choisie
    std::vector<int> notes;
    const auto& steps = music::scaleSteps (s.scale);    // gamme du pilote ou personnalisee
    for (int o = 0; o < s.octaves; ++o)
        for (int st : steps)
            if (s.root + 12 * o + st <= 127)
                notes.push_back (s.root + 12 * o + st);
    notes.push_back (std::min (127, s.root + 12 * s.octaves));   // la tonique en haut
    int index = (int) notes.size() / 2;              // melodie en marche : on part du milieu

    int freeFrom = 0;                                // pas de notes qui se chevauchent
    for (int r = 0; r < p.length; r += std::max (1, s.grid))
    {
        if (r < freeFrom || ! p.rows[(size_t) r][(size_t) track].empty())
            continue;
        const bool beat = r % 4 == 0;
        double chance = s.density / 100.0;
        if (s.accents)
            chance = beat ? std::min (1.0, chance * 1.6) : chance * 0.8;
        if (rng.generateDouble() >= chance)
            continue;
        seq::Cell c;
        int vel = rng.bounded (std::min (s.velMin, s.velMax), std::max (s.velMin, s.velMax) + 1);
        if (s.accents && beat)
            vel = std::min (127, vel + 15);
        c.velocity = std::clamp (vel, 1, 127);
        c.length = std::clamp ((int) rng.bounded (std::min (s.lenMin, s.lenMax), std::max (s.lenMin, s.lenMax) + 1),
                               1, p.length - r);
        if (s.fixedPitch)
            c.note = session::defaultNote (sound);
        else
        {
            if (s.walk)                              // pas de 1 a 2 notes de la gamme, rarement plus
            {
                const int jump = rng.bounded (10) < 8 ? (int) rng.bounded (1, 3) : (int) rng.bounded (3, 5);
                int next = index + (rng.bounded (2) ? jump : -jump);
                if (next < 0 || next >= (int) notes.size())    // au bord : on repart dans l'autre sens
                    next = index - (next - index);
                index = std::clamp (next, 0, (int) notes.size() - 1);
            }
            else
                index = (int) rng.bounded ((quint32) notes.size());
            c.note = notes[(size_t) index];
            // Accord : la tierce et la quinte de la gamme (2 et 4 degres au-dessus)
            if (s.chords > 0 && (int) rng.bounded (100) < s.chords)
            {
                QList<int> chord { c.note };
                for (int up : { 2, 4 })
                {
                    const size_t k = (size_t) index + (size_t) up;
                    chord << (k < notes.size() ? notes[k] : std::min (127, c.note + (up == 2 ? 4 : 7)));
                }
                c = PianoRollView::withNotes (c, chord);
            }
        }
        p.rows[(size_t) r][(size_t) track] = c;
        freeFrom = r + c.length;
    }
    return p;
}

GenerateDialog::GenerateDialog (seq::Sequencer& seqr, const session::State& /* state */, int g, int snd, QWidget* parent)
    : QDialog (parent), sequencer (seqr), group (g), sound (snd), before (seqr.pattern (g))
{
    setWindowTitle (tr ("Generate notes — sound %1").arg (snd, 2, 10, QChar ('0')));
    seed = QRandomGenerator::global()->generate();
    auto* col = new QVBoxLayout (this);
    auto* form = new QFormLayout;
    auto slider = [] (int max, int value)
    {
        auto* s = new QSlider (Qt::Horizontal);
        s->setRange (0, max);
        s->setValue (value);
        return s;
    };
    auto spin = [] (int lo, int hi, int value)
    {
        auto* s = new QSpinBox;
        s->setRange (lo, hi);
        s->setValue (value);
        return s;
    };
    auto pair = [] (QWidget* a, QWidget* b)
    {
        auto* w = new QWidget;
        auto* h = new QHBoxLayout (w);
        h->setContentsMargins (0, 0, 0, 0);
        h->addWidget (a);
        h->addWidget (new QLabel ("-"));
        h->addWidget (b);
        h->addStretch();
        return w;
    };
    const bool drums = snd <= 4;                     // pads 1-4 : batterie, hauteur fixe

    density = slider (100, 40);
    form->addRow (tr ("Density"), density);
    grid = new QComboBox;
    grid->addItems ({ tr ("Every line (1/16)"), tr ("Every 2 lines (1/8)"), tr ("Every beat (1/4)") });
    form->addRow (tr ("Grid"), grid);
    accents = new QCheckBox (tr ("More and louder notes on the beats"));
    accents->setChecked (true);
    form->addRow (tr ("Accents"), accents);
    velMin = spin (1, 127, 70);
    velMax = spin (1, 127, 120);
    form->addRow (tr ("Velocity"), pair (velMin, velMax));
    lenMin = spin (1, 16, 1);
    lenMax = spin (1, 16, drums ? 1 : 2);
    form->addRow (tr ("Length (lines)"), pair (lenMin, lenMax));
    fixedPitch = new QCheckBox (tr ("Always the pad note (drums)"));
    fixedPitch->setChecked (drums);
    form->addRow (tr ("Pitch"), fixedPitch);
    scale = new QPushButton (music::scaleLabel (scaleValue));   // Main : Major
    auto* scaleMenu = new QMenu (scale);
    scale->setMenu (scaleMenu);
    connect (scaleMenu, &QMenu::aboutToShow, this, [this, scaleMenu]
    {
        music::fillScaleMenu (scaleMenu, scaleValue, [this] (int sc)
        {
            scaleValue = sc;
            scale->setText (music::scaleLabel (sc));
            roll (false);
        }, this);
    });
    root = new QComboBox;
    for (int n = 24; n <= 96; ++n)
        root->addItem (session::noteName (n), n);
    root->setCurrentIndex (std::clamp (session::defaultNote (snd) - 24, 0, 72));
    octaves = spin (1, 4, 1);
    auto* scaleRow = new QWidget;
    auto* sh = new QHBoxLayout (scaleRow);
    sh->setContentsMargins (0, 0, 0, 0);
    sh->addWidget (scale);
    sh->addWidget (new QLabel (tr ("from")));
    sh->addWidget (root);
    sh->addWidget (new QLabel (tr ("octaves")));
    sh->addWidget (octaves);
    sh->addStretch();
    form->addRow (tr ("Scale"), scaleRow);
    walk = new QCheckBox (tr ("Melody moves step by step"));
    walk->setChecked (true);
    form->addRow (tr ("Melody"), walk);
    chords = slider (100, 0);
    form->addRow (tr ("Chords"), chords);
    mode = new QComboBox;
    mode->addItems ({ tr ("Replace the notes of this sound"), tr ("Add to the notes already there") });
    form->addRow (tr ("Mode"), mode);
    col->addLayout (form);

    auto* bottom = new QHBoxLayout;
    auto* dice = new QPushButton (QStringLiteral ("\U0001F3B2 ") + tr ("New roll"));
    dice->setToolTip (tr ("Another random result with the same settings"));
    result = new QLabel;
    result->setObjectName ("hint");
    auto* play = new QPushButton (QStringLiteral ("\u25B6 / \u275A\u275A"));
    play->setToolTip (tr ("Play / pause, to hear the result"));
    connect (play, &QPushButton::clicked, this, &GenerateDialog::playToggle);
    bottom->addWidget (play);
    bottom->addWidget (dice);
    bottom->addWidget (result, 1);
    col->addLayout (bottom);
    auto* buttons = new QDialogButtonBox (QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect (buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect (buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    col->addWidget (buttons);

    // Les melodies n'ont de sens qu'avec une hauteur libre
    auto pitchEnabled = [this]
    {
        const bool free = ! fixedPitch->isChecked();
        for (QWidget* w : std::initializer_list<QWidget*> { scale, root, octaves, walk, chords })
            w->setEnabled (free);
    };
    pitchEnabled();
    connect (fixedPitch, &QCheckBox::toggled, this, pitchEnabled);
    connect (dice, &QPushButton::clicked, this, [this] { roll (true); });
    // Chaque reglage refait le tirage (meme graine : on voit l'effet du reglage)
    for (auto* s : { density, chords })
        connect (s, &QSlider::valueChanged, this, [this] { roll (false); });
    for (auto* b : { velMin, velMax, lenMin, lenMax, octaves })
        connect (b, &QSpinBox::valueChanged, this, [this] { roll (false); });
    for (auto* c : { grid, root, mode })
        connect (c, &QComboBox::currentIndexChanged, this, [this] { roll (false); });
    for (auto* c : { accents, fixedPitch, walk })
        connect (c, &QCheckBox::toggled, this, [this] { roll (false); });
    roll (false);
}

GenerateSettings GenerateDialog::read() const
{
    GenerateSettings s;
    s.density = density->value();
    s.grid = grid->currentIndex() == 0 ? 1 : grid->currentIndex() == 1 ? 2 : 4;
    s.accents = accents->isChecked();
    s.velMin = velMin->value();
    s.velMax = velMax->value();
    s.lenMin = lenMin->value();
    s.lenMax = lenMax->value();
    s.fixedPitch = fixedPitch->isChecked();
    s.scale = scaleValue;
    s.root = root->currentData().toInt();
    s.octaves = octaves->value();
    s.walk = walk->isChecked();
    s.chords = chords->value();
    s.replace = mode->currentIndex() == 0;
    return s;
}

void GenerateDialog::roll (bool newSeed)
{
    if (newSeed)
        seed = QRandomGenerator::global()->generate();
    const seq::Pattern p = generate (before, sound - 1, sound, read(), seed);
    sequencer.setPattern (group, p);
    int count = 0;
    for (int r = 0; r < p.length; ++r)
        count += ! p.rows[(size_t) r][(size_t) (sound - 1)].empty();
    result->setText (tr ("%1 note(s) on %2 lines — press play to hear it").arg (count).arg (p.length));
}

void GenerateDialog::reject()
{
    sequencer.setPattern (group, before);
    QDialog::reject();
}
