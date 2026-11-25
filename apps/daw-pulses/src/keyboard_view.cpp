// Fuzzy John DAW Pulses — onglet Keyboard. Licence : GPLv3.
#include "keyboard_view.h"
#include "music_tables.h"

#include <QDateTime>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <cmath>
#include <set>

namespace music {

namespace {
bool isBlack (int n)
{
    const int pc = n % 12;
    return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
}

const std::vector<int>& steps (const KeySettings& k)
{
    return scales()[(size_t) std::clamp (k.scale, 0, (int) scales().size() - 1)].steps;
}

// k-ieme note de la gamme a partir de « base » (k peut depasser l'octave)
int degreeNote (int base, const std::vector<int>& s, int k)
{
    const int n = (int) s.size();
    const int oct = (int) std::floor (k / (double) n);
    return base + s[(size_t) (((k % n) + n) % n)] + 12 * oct;
}
}

bool inScale (int note, const KeySettings& k)
{
    const int pc = ((note - k.root) % 12 + 12) % 12;
    const auto& s = steps (k);
    return std::find (s.begin(), s.end(), pc) != s.end();
}

QList<int> chordFor (int note, const KeySettings& k)
{
    QList<int> out;
    if (k.chordMode == 1)                            // Harmonizer : degres de la gamme empiles
    {
        const auto& s = steps (k);
        const int base = k.root + 12 * (int) std::floor ((note - k.root) / 12.0);
        // degre de la note (la note de la gamme juste en dessous si elle n'y est pas)
        int deg = 0;
        for (int d = 0; d < (int) s.size(); ++d)
            if (base + s[(size_t) d] <= note)
                deg = d;
        const Harmony& h = harmonies()[(size_t) std::clamp (k.harmony, 0, (int) harmonies().size() - 1)];
        const int first = degreeNote (base, s, deg);
        if (h.degrees.empty())                       // Oct +1 / -1... : la note doublee
            out << first << first + h.octave;
        else
            for (int d : h.degrees)
                out << degreeNote (base, s, deg + d);
    }
    else if (k.chordMode == 2)                       // Chord Set : 12 accords sur les 12 notes de l'octave
    {
        const ChordSet& set = chordSets()[(size_t) std::clamp (k.chordSet, 0, (int) chordSets().size() - 1)];
        const int index = ((note - k.root) % 12 + 12) % 12;
        const int base = note - index;               // la tonique de cette octave
        const Chord& c = set.chords[(size_t) index];
        if (c.bass >= 0)
            out << base + c.bass - 12;
        for (int i : c.intervals)
            out << base + c.root + i;
    }
    else
        out << note;
    QList<int> clean;
    for (int n : out)
        if (n >= 0 && n <= 127 && ! clean.contains (n))
            clean << n;
    return clean;
}

QString chordName (QList<int> notes)
{
    std::sort (notes.begin(), notes.end());
    if (notes.isEmpty())
        return {};
    // Essai de chaque note comme fondamentale : on cherche un accord connu
    for (int root : notes)
    {
        std::set<int> pcs;
        for (int n : notes)
            pcs.insert (((n - root) % 12 + 12) % 12);
        for (const ChordName& c : chordNames())
        {
            const std::set<int> want (c.intervals.begin(), c.intervals.end());
            if (pcs == want)
            {
                QString name = session::noteName (root);
                name.chop (name.endsWith ('-') ? 2 : 1);  // sans l'octave
                while (! name.isEmpty() && (name.back().isDigit() || name.back() == '-'))
                    name.chop (1);
                QString out = name + c.suffix;
                if (root != notes.first())           // renversement : la basse
                {
                    QString bass = session::noteName (notes.first());
                    while (! bass.isEmpty() && (bass.back().isDigit() || bass.back() == '-'))
                        bass.chop (1);
                    out += "/" + bass;
                }
                return out;
            }
        }
    }
    QStringList names;
    for (int n : notes)
        names << session::noteName (n);
    return names.join ("  ");
}

QString scaleLabel (int index)
{
    const Scale& s = scales()[(size_t) index];
    return QString ("%1 : %2").arg (s.bank, s.name);
}

} // namespace music

KeyboardView::KeyboardView (const session::State& st, const music::KeySettings& k, QWidget* parent)
    : QWidget (parent), state (st), keys (k)
{
    tick.setInterval (40);
    connect (&tick, &QTimer::timeout, this, [this]
    {
        // Allumages du pattern termines : on rafraichit
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        bool changed = false;
        for (auto it = flashes.begin(); it != flashes.end();)
            if (it.value() <= now)
            {
                it = flashes.erase (it);
                changed = true;
            }
            else
                ++it;
        if (changed || isVisible())
            update();
    });
    tick.start();
}

void KeyboardView::setActive (int note, bool on)
{
    held[note] = std::max (0, held.value (note) + (on ? 1 : -1));
    if (held[note] == 0)
        held.remove (note);
    update();
}

void KeyboardView::flash (int note, int ms)
{
    flashes[note] = std::max (flashes.value (note), QDateTime::currentMSecsSinceEpoch() + std::max (60, ms));
    update();
}

QList<int> KeyboardView::activeNotes() const
{
    QList<int> out = held.keys();
    for (auto it = flashes.begin(); it != flashes.end(); ++it)
        if (! out.contains (it.key()))
            out << it.key();
    return out;
}

int KeyboardView::firstNote() const
{
    return std::clamp (12 * state.octave.load(), 0, 127 - 12 * kOctaves);   // une octave sous celle jouee
}

QRectF KeyboardView::keyRect (int note) const
{
    const int first = firstNote();
    const int whites = 7 * kOctaves + 1;
    const double w = width() / (double) whites, h = height() - kTop;
    auto whiteIndex = [] (int n)                     // rang de la touche blanche
    {
        static const int idx[12] { 0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6 };
        return (n / 12) * 7 + idx[n % 12];
    };
    const int wi = whiteIndex (note) - whiteIndex (first);
    if (! music::isBlack (note))
        return { wi * w, (double) kTop, w, h };
    return { (wi + 1) * w - w * 0.32, (double) kTop, w * 0.64, h * 0.6 };
}

namespace music { bool isBlackKey (int n); }

int KeyboardView::noteAt (const QPointF& p) const
{
    const int first = firstNote(), last = first + 12 * kOctaves;
    for (int n = first; n <= last; ++n)              // noires d'abord (dessus)
        if (music::isBlackKey (n) && keyRect (n).contains (p))
            return n;
    for (int n = first; n <= last; ++n)
        if (! music::isBlackKey (n) && keyRect (n).contains (p))
            return n;
    return -1;
}

namespace music {
bool isBlackKey (int n) { return isBlack (n); }
}

void KeyboardView::paintEvent (QPaintEvent*)
{
    QPainter g (this);
    g.setRenderHint (QPainter::Antialiasing);
    const QColor color = session::groupColor (state.group);
    const QList<int> active = activeNotes();
    g.fillRect (rect(), QColor (16, 16, 20));

    // En haut : l'accord ou les notes jouees, en grand ; a droite : gamme et accords
    QFont big = font();
    big.setPixelSize (28);
    big.setBold (true);
    g.setFont (big);
    g.setPen (color);
    g.drawText (QRectF (10, 0, width() * 0.55, kTop - 4), Qt::AlignVCenter | Qt::AlignLeft,
                active.isEmpty() ? QStringLiteral ("—") : music::chordName (active));
    QFont small = font();
    small.setPixelSize (12);
    g.setFont (small);
    g.setPen (QColor (150, 150, 160));
    static const char* roots[12] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const QString chordText = keys.chordMode == 1 ? QString ("Harmonizer %1").arg (music::harmonies()[(size_t) keys.harmony].name)
                            : keys.chordMode == 2 ? QString ("Chord Set %1").arg (music::chordSets()[(size_t) keys.chordSet].name)
                            : QString ("Chords off");
    g.drawText (QRectF (width() * 0.45, 0, width() * 0.55 - 10, kTop - 4), Qt::AlignVCenter | Qt::AlignRight,
                QString ("%1 %2   |   %3   |   Sound %4")
                    .arg (roots[keys.root]).arg (music::scaleLabel (keys.scale)).arg (chordText)
                    .arg (state.sound.load(), 2, 10, QChar ('0')));

    // Touches : blanches puis noires ; gamme teintee, tonique marquee, notes jouees allumees
    const int first = firstNote(), last = first + 12 * kOctaves;
    for (const bool black : { false, true })
        for (int n = first; n <= last; ++n)
        {
            if (music::isBlackKey (n) != black)
                continue;
            const QRectF r = keyRect (n).adjusted (1, 0, -1, -1);
            QColor fill = black ? QColor (34, 34, 40) : QColor (232, 232, 236);
            if (keys.scale != 0 && music::inScale (n, keys))
                fill = black ? color.darker (220) : color.lighter (185);   // dans la gamme
            if (active.contains (n))
                fill = color;                         // jouee
            g.setPen (QColor (10, 10, 12));
            g.setBrush (fill);
            g.drawRoundedRect (r, 3, 3);
            if ((n - keys.root) % 12 == 0)            // tonique : point
            {
                g.setPen (Qt::NoPen);
                g.setBrush (active.contains (n) ? QColor (15, 15, 15) : color);
                const double d = std::min (r.width() * 0.35, 9.0);
                g.drawEllipse (QPointF (r.center().x(), r.bottom() - (black ? 10 : 24)), d / 2, d / 2);
            }
            if (keyLabels.contains (n))                // lettre de l'ordinateur (PC KEYS)
            {
                g.setPen (active.contains (n) ? QColor (15, 15, 15) : black ? QColor (220, 220, 226) : QColor (40, 40, 46));
                QFont lf = font();
                lf.setPixelSize (12);
                lf.setBold (true);
                g.setFont (lf);
                // en bas des touches noires, sous les noires pour les blanches (pas de chevauchement)
                const QRectF box = black ? QRectF (r.left(), r.bottom() - 22, r.width(), 18)
                                         : QRectF (r.left(), r.top() + r.height() * 0.62, r.width(), 18);
                g.drawText (box, Qt::AlignCenter, keyLabels[n]);
                QFont sf = font();
                sf.setPixelSize (12);
                g.setFont (sf);
            }
            if (! black && n % 12 == 0)               // nom des do
            {
                g.setPen (QColor (60, 60, 66));
                g.drawText (r.adjusted (0, 0, 0, -4), Qt::AlignBottom | Qt::AlignHCenter, session::noteName (n));
            }
        }
}

void KeyboardView::mousePressEvent (QMouseEvent* e)
{
    mouseNote = noteAt (e->position());
    if (mouseNote >= 0)
        emit keyPressed (mouseNote);
}

void KeyboardView::mouseReleaseEvent (QMouseEvent*)
{
    if (mouseNote >= 0)
        emit keyReleased (mouseNote);
    mouseNote = -1;
}
