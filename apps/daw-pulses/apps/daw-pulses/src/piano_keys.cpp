// Fuzzy John DAW Pulses — clavier dessine. Licence : GPLv3.
#include "piano_keys.h"
#include "session.h"

#include <QMouseEvent>
#include <QPainter>

PianoKeys::PianoKeys (QWidget* parent) : QWidget (parent)
{
    setMinimumHeight (110);
    setSizePolicy (QSizePolicy::Expanding, QSizePolicy::Fixed);
}

bool PianoKeys::black (int offset)
{
    const int n = offset % 12;
    return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
}

QRectF PianoKeys::keyRect (int offset) const
{
    // 15 touches blanches ; une noire est posee a cheval entre deux blanches
    int whites = 0;
    for (int i = 0; i < offset; ++i)
        if (! black (i))
            ++whites;
    const double ww = width() / 15.0;
    if (! black (offset))
        return { whites * ww, 0, ww, (double) height() };
    return { whites * ww - ww * 0.32, 0, ww * 0.64, height() * 0.6 };
}

int PianoKeys::noteAt (const QPointF& p) const
{
    for (int o = 0; o < kKeys; ++o)                  // les noires d'abord (dessus)
        if (black (o) && keyRect (o).contains (p))
            return base + o;
    for (int o = 0; o < kKeys; ++o)
        if (! black (o) && keyRect (o).contains (p))
            return base + o;
    return -1;
}

void PianoKeys::setHeld (int note, bool isHeld)
{
    if (isHeld)
        held.insert (note);
    else
        held.erase (note);
    update();
}

void PianoKeys::paintEvent (QPaintEvent*)
{
    QPainter g (this);
    g.setRenderHint (QPainter::Antialiasing);
    QFont small = font();
    small.setPixelSize (11);
    g.setFont (small);
    for (int pass = 0; pass < 2; ++pass)             // blanches puis noires
        for (int o = 0; o < kKeys; ++o)
        {
            if (black (o) != (pass == 1))
                continue;
            const QRectF r = keyRect (o).adjusted (1, 0, -1, -1);
            const bool on = held.count (base + o) > 0;
            g.setPen (QColor (40, 40, 46));
            g.setBrush (on ? color : black (o) ? QColor (24, 24, 30) : QColor (225, 225, 225));
            g.drawRoundedRect (r, 3, 3);
            const bool dark = black (o) && ! on;
            if (labels.contains (o))                 // lettre du clavier de l'ordinateur
            {
                g.setPen (dark ? QColor (200, 200, 200) : QColor (40, 40, 40));
                g.drawText (r.adjusted (0, 0, 0, -22), Qt::AlignHCenter | Qt::AlignBottom, labels[o]);
            }
            if (o % 12 == 0)                         // nom des C
            {
                g.setPen (QColor (90, 90, 90));
                g.drawText (r.adjusted (0, 0, 0, -4), Qt::AlignHCenter | Qt::AlignBottom,
                            session::noteName (base + o));
            }
        }
}

void PianoKeys::mousePressEvent (QMouseEvent* e)
{
    mouseNote = noteAt (e->position());
    if (mouseNote >= 0)
        emit noteOn (mouseNote, 100);
}

void PianoKeys::mouseMoveEvent (QMouseEvent* e)
{
    if (mouseNote < 0)
        return;
    const int n = noteAt (e->position());             // glisser d'une touche a l'autre
    if (n >= 0 && n != mouseNote)
    {
        emit noteOff (mouseNote);
        mouseNote = n;
        emit noteOn (n, 100);
    }
}

void PianoKeys::mouseReleaseEvent (QMouseEvent*)
{
    if (mouseNote >= 0)
        emit noteOff (mouseNote);
    mouseNote = -1;
}
