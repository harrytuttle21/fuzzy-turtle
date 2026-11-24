// Fuzzy John DAW Pulses — les 16 pads a l'ecran. Licence : GPLv3.
#include "pad_grid.h"

#include <QMouseEvent>
#include <QPainter>

PadGrid::PadGrid (QWidget* parent) : QWidget (parent)
{
    setMinimumSize (280, 280);
    setSizePolicy (QSizePolicy::Expanding, QSizePolicy::Expanding);
}

QRectF PadGrid::padRect (int pad) const
{
    const double side = std::min (width(), height());
    const double cell = side / 4.0, gap = 8;
    const int col = (pad - 1) % 4, row = 3 - (pad - 1) / 4;      // pad 1 en bas a gauche
    const double x0 = (width() - side) / 2, y0 = (height() - side) / 2;
    return { x0 + col * cell + gap / 2, y0 + row * cell + gap / 2, cell - gap, cell - gap };
}

int PadGrid::padAt (const QPointF& p) const
{
    for (int pad = 1; pad <= 16; ++pad)
        if (padRect (pad).contains (p))
            return pad;
    return 0;
}

void PadGrid::flash (int pad, bool isDown)
{
    if (pad >= 1 && pad <= 16)
    {
        down[(size_t) pad - 1] = isDown;
        update();
    }
}

void PadGrid::paintEvent (QPaintEvent*)
{
    QPainter g (this);
    g.setRenderHint (QPainter::Antialiasing);
    QFont f = font();
    f.setPixelSize (qMax (10, int (height() / 34)));   // suit la taille des pads
    g.setFont (f);
    for (int pad = 1; pad <= 16; ++pad)
    {
        const QRectF r = padRect (pad);
        const bool hit = down[(size_t) pad - 1];
        const QColor fill = hit ? color : color.darker (pad == selected ? 260 : 420);
        g.setPen (pad == selected ? QPen (Qt::white, 2) : QPen (color.darker (200), 1));
        g.setBrush (fill);
        g.drawRoundedRect (r, 8, 8);
        g.setPen (hit ? QColor (20, 20, 20) : QColor (230, 230, 230));
        g.drawText (r.adjusted (8, 6, -8, -6), Qt::AlignLeft | Qt::AlignTop, QString ("%1").arg (pad, 2, 10, QChar ('0')));
        g.drawText (r.adjusted (6, 6, -6, -8), Qt::AlignHCenter | Qt::AlignBottom | Qt::TextWordWrap,
                    labels[(size_t) pad - 1]);
    }
}

void PadGrid::mousePressEvent (QMouseEvent* e)
{
    mousePad = padAt (e->position());
    if (! mousePad)
        return;
    // Velocite selon la hauteur du clic dans le pad : en haut = fort
    const QRectF r = padRect (mousePad);
    const int vel = qBound (20, int (127 - (e->position().y() - r.top()) / r.height() * 90), 127);
    emit pressed (mousePad, vel);
}

void PadGrid::mouseReleaseEvent (QMouseEvent*)
{
    if (mousePad)
        emit released (mousePad);
    mousePad = 0;
}
