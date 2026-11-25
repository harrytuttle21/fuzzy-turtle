// Fuzzy John DAW Pulses — import de fichiers MIDI. Licence : GPLv3.
#include "midi_import.h"
#include "piano_roll.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFormLayout>
#include <QHash>
#include <QLabel>
#include <QMap>
#include <QSet>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace midi {

namespace {

// Lecture octet par octet, sans jamais depasser la fin
struct Reader
{
    const QByteArray& data;
    qsizetype pos = 0, end = 0;
    bool ok = true;
    int byte()
    {
        if (pos >= end) { ok = false; return 0; }
        return (unsigned char) data[pos++];
    }
    quint32 be (int n)                               // entier sur n octets, gros-boutiste
    {
        quint32 v = 0;
        for (int i = 0; i < n; ++i)
            v = (v << 8) | (quint32) byte();
        return v;
    }
    quint32 vlq()                                    // longueur variable du MIDI (7 bits par octet)
    {
        quint32 v = 0;
        for (int i = 0; i < 4; ++i)
        {
            const int b = byte();
            v = (v << 7) | (quint32) (b & 0x7F);
            if (! (b & 0x80))
                break;
        }
        return v;
    }
};

} // namespace

File read (const QString& path)
{
    File out;
    QFile f (path);
    if (! f.open (QIODevice::ReadOnly))
    {
        out.error = QObject::tr ("Cannot open the file");
        return out;
    }
    const QByteArray data = f.readAll();
    Reader r { data, 0, data.size() };
    if (data.left (4) != "MThd")
    {
        out.error = QObject::tr ("Not a MIDI file");
        return out;
    }
    r.pos = 4;
    const quint32 headerLength = r.be (4);
    out.format = (int) r.be (2);
    const int tracks = (int) r.be (2);
    const int division = (int) r.be (2);
    r.pos = 8 + headerLength;
    if (division & 0x8000)
    {
        out.error = QObject::tr ("SMPTE timing is not supported");
        return out;
    }
    out.division = std::max (1, division);

    for (int t = 0; t < tracks && r.ok && r.pos + 8 <= data.size(); ++t)
    {
        if (data.mid (r.pos, 4) != "MTrk")
            break;
        r.pos += 4;
        const quint32 length = r.be (4);
        Reader tr { data, r.pos, std::min<qsizetype> (data.size(), r.pos + length) };
        r.pos += length;
        qint64 tick = 0;
        int status = 0;
        QHash<int, QList<int>> open;                 // (canal, note) -> notes commencees (index)
        while (tr.ok && tr.pos < tr.end)
        {
            tick += tr.vlq();
            int b = tr.byte();
            if (b < 0x80)                            // « running status » : meme commande qu'avant
            {
                --tr.pos;
                b = status;
            }
            else if (b < 0xF0)
                status = b;
            const int kind = b & 0xF0, ch = b & 0x0F;
            if (b == 0xFF)                           // meta : tempo, fin de piste...
            {
                const int type = tr.byte();
                const quint32 len = tr.vlq();
                if (type == 0x51 && len == 3 && out.bpm == 0)
                    out.bpm = 60000000.0 / tr.be (3);
                else
                    tr.pos += len;
                if (type == 0x2F)
                    break;
            }
            else if (b == 0xF0 || b == 0xF7)         // sysex : ignore
                tr.pos += tr.vlq();
            else if (kind == 0x90 || kind == 0x80)
            {
                const int pitch = tr.byte(), vel = tr.byte();
                const int key = ch * 128 + pitch;
                if (kind == 0x90 && vel > 0)
                {
                    open[key].append ((int) out.notes.size());
                    out.notes.append ({ tick, 0, pitch, vel, ch, t });
                }
                else if (! open.value (key).isEmpty())   // fin de note (note-on a 0 compris)
                {
                    Note& n = out.notes[open[key].takeFirst()];
                    n.length = std::max<qint64> (1, tick - n.tick);
                }
            }
            else if (kind == 0xA0 || kind == 0xB0 || kind == 0xE0)
                tr.pos += 2;
            else if (kind == 0xC0 || kind == 0xD0)
                tr.pos += 1;
            else
                break;                               // octet inattendu : on s'arrete la
        }
        for (const auto& list : open)                // notes jamais terminees : jusqu'a la fin
            for (int i : list)
                out.notes[i].length = std::max<qint64> (1, tick - out.notes[i].tick);
    }
    if (out.notes.isEmpty() && out.error.isEmpty())
        out.error = QObject::tr ("No notes in this file");
    return out;
}

namespace {
// Batterie General MIDI : grosse caisse, caisse claire, charleston ferme, ouvert
int gmPad (int pitch)
{
    switch (pitch)
    {
        case 35: case 36: return 1;
        case 37: case 38: case 39: case 40: return 2;
        case 42: case 44: return 3;
        case 46: return 4;
        default: return 0;
    }
}
}

Placed apply (const File& file, seq::Pattern& pattern, Target target, int firstSound, bool replace)
{
    Placed result;
    const double rowTicks = file.division / 4.0;     // une ligne = une double-croche
    qint64 lastTick = 0;
    for (const Note& n : file.notes)
        lastTick = std::max (lastTick, n.tick + n.length);
    // Longueur : tout le morceau si possible (au plus 256 lignes), arrondie au temps
    const int rows = (int) std::ceil (lastTick / rowTicks - 0.01);
    result.length = std::clamp ((rows + 3) / 4 * 4, 4, seq::kMaxRows);

    // Quel son pour chaque note
    QMap<int, int> soundOfKey;                       // canal / piste -> son
    QHash<int, int> soundOfDrum;                     // note de batterie -> pad
    int nextPad = 5;
    auto soundFor = [&] (const Note& n) -> int
    {
        switch (target)
        {
            case Target::OneSound: return firstSound;
            case Target::PerChannel:
            {
                const int key = n.track * 16 + n.channel;
                if (! soundOfKey.contains (key))
                    soundOfKey.insert (key, firstSound + (int) soundOfKey.size());
                return soundOfKey[key];
            }
            case Target::Drums:
            {
                if (const int pad = gmPad (n.pitch))
                    return pad;
                if (! soundOfDrum.contains (n.pitch))
                    soundOfDrum.insert (n.pitch, nextPad++);
                return soundOfDrum[n.pitch];
            }
        }
        return firstSound;
    };
    // Ordre du temps : les sons par canal sont donnes dans l'ordre d'apparition
    QList<Note> notes = file.notes;
    std::stable_sort (notes.begin(), notes.end(), [] (const Note& a, const Note& b) { return a.tick < b.tick; });

    if (! replace)                                   // ajouter : le pattern ne raccourcit pas
        result.length = std::max (result.length, pattern.length);
    pattern.length = result.length;
    if (replace)
    {
        QSet<int> tracks;
        for (const Note& n : notes)
            if (const int s = soundFor (n); s >= 1 && s <= 16)
                tracks.insert (s - 1);
        for (int t : tracks)
            for (auto& row : pattern.rows)
                row[(size_t) t] = {};
        soundOfKey.clear();                          // on refait la meme attribution juste apres
        soundOfDrum.clear();
        nextPad = 5;
    }
    for (const Note& n : notes)
    {
        const int sound = soundFor (n);
        // Ligne la plus proche ; la 2e moitie d'une ligne va a la suivante (en avance)
        const double exact = n.tick / rowTicks;
        const int row = (int) std::floor (exact + 0.5);
        const int offset = std::clamp ((int) std::lround ((exact - row) * seq::kTiming), -seq::kTiming / 2, seq::kTiming / 2);
        if (sound < 1 || sound > 16 || row >= result.length)
        {
            ++result.skipped;
            continue;
        }
        const int length = std::clamp ((int) std::lround (n.length / rowTicks), 1, result.length - row);
        const int pitch = target == Target::Drums ? session::defaultNote (sound) : n.pitch;
        seq::Cell& c = pattern.rows[(size_t) row][(size_t) (sound - 1)];
        QList<int> chord = PianoRollView::notesOf (c);
        if (chord.contains (pitch))
            continue;                                // meme note deja la (batterie : plusieurs notes, un pad)
        if (chord.size() > seq::kChordExtra)
        {
            ++result.skipped;
            continue;
        }
        if (c.empty())
            c = { pitch, std::clamp (n.velocity, 1, 127), length, offset };
        else
            c.length = std::max (c.length, length);
        chord << pitch;
        c = PianoRollView::withNotes (c, chord);
        ++result.notes;
    }
    return result;
}

} // namespace midi

MidiImportDialog::MidiImportDialog (const midi::File& file, const QString& name, int sound, QWidget* parent)
    : QDialog (parent)
{
    setWindowTitle (tr ("Import MIDI — %1").arg (name));
    auto* col = new QVBoxLayout (this);
    QSet<int> channels;
    for (const auto& n : file.notes)
        channels.insert (n.track * 16 + n.channel);
    auto* info = new QLabel (tr ("%1 note(s), %2 channel(s) / track(s)%3")
                                 .arg (file.notes.size()).arg (channels.size())
                                 .arg (file.bpm > 0 ? tr (", tempo %1 BPM").arg (file.bpm, 0, 'f', 1) : QString()));
    col->addWidget (info);
    auto* form = new QFormLayout;
    targetBox = new QComboBox;
    targetBox->addItems ({ tr ("All notes on one sound (chords)"), tr ("One sound per MIDI channel / track"),
                           tr ("Drums (General MIDI: kick 1, snare 2, hi-hats 3-4, others 5-16)") });
    bool drums = false;                              // canal 10 = batterie en General MIDI
    for (const auto& n : file.notes)
        drums |= n.channel == 9;
    targetBox->setCurrentIndex (drums ? 2 : channels.size() > 1 ? 1 : 0);
    form->addRow (tr ("Put the notes"), targetBox);
    soundBox = new QSpinBox;
    soundBox->setRange (1, 16);
    soundBox->setValue (sound);
    form->addRow (tr ("On sound (first)"), soundBox);
    modeBox = new QComboBox;
    modeBox->addItems ({ tr ("Replace the notes of these sounds"), tr ("Add to the notes already there") });
    form->addRow (tr ("Mode"), modeBox);
    tempoBox = new QCheckBox (tr ("Use the tempo of the file"));
    tempoBox->setEnabled (file.bpm > 0);
    form->addRow (QString(), tempoBox);
    col->addLayout (form);
    auto* note = new QLabel (tr ("The pattern holds up to 256 lines (16 bars): longer files are cut."));
    note->setObjectName ("hint");
    col->addWidget (note);
    auto* buttons = new QDialogButtonBox (QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect (buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect (buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    col->addWidget (buttons);
    auto soundUse = [this] { soundBox->setEnabled (targetBox->currentIndex() != 2); };
    soundUse();
    connect (targetBox, &QComboBox::currentIndexChanged, this, soundUse);
}

midi::Target MidiImportDialog::target() const { return (midi::Target) targetBox->currentIndex(); }
int MidiImportDialog::firstSound() const { return soundBox->value(); }
bool MidiImportDialog::replace() const { return modeBox->currentIndex() == 0; }
bool MidiImportDialog::useTempo() const { return tempoBox->isChecked(); }
