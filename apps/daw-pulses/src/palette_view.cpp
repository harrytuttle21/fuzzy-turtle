// Palette de patterns : voir palette_view.h
#include "palette_view.h"

#include <QContextMenuEvent>
#include <QDrag>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>

using namespace session;

PaletteView::PaletteView (const seq::Sequencer& seqr, const session::State& st, QWidget* parent)
    : QWidget (parent), sequencer (seqr), state (st)
{
    setFocusPolicy (Qt::NoFocus);
    setToolTip (tr ("Drag a pattern into a scene cell of its group (Edit on) · click = play it (Play on)"));
}

QRectF PaletteView::square (int k) const
{
    return QRectF (4 + (k % 4) * kCell, 30 + (k / 4) * kCell, kCell - 4, kCell - 4);
}

int PaletteView::indexAt (const QPointF& p) const
{
    for (int k = 0; k < 16; ++k)
        if (square (k).contains (p))
        {
            const int pad = 13 + k % 4 - 4 * (k / 4);   // comme les pads : 13 en haut a gauche
            return bank * 16 + pad - 1;
        }
    return -1;
}

void PaletteView::paintEvent (QPaintEvent*)
{
    QPainter p (this);
    p.setRenderHint (QPainter::Antialiasing);
    p.fillRect (rect(), QColor (14, 14, 18));
    const int g = state.group;
    const QColor color = groupColor (g);
    QFont f = font();
    f.setPixelSize (12);
    f.setBold (true);
    p.setFont (f);
    p.setPen (color);
    p.drawText (QRectF (6, 0, width() - 80, 28), Qt::AlignVCenter | Qt::AlignLeft,
                p.fontMetrics().elidedText (tr ("PALETTE %1").arg (groupTitle (state, g)), Qt::ElideRight, width() - 86));
    p.setPen (QColor (200, 200, 210));
    p.drawText (QRectF (width() - 48, 0, 22, 28), Qt::AlignCenter, QString::number (bank + 1));
    for (const bool left : { true, false })
    {
        const QRectF a = left ? bankLeft() : bankRight();
        p.setPen (Qt::NoPen);
        p.setBrush (QColor (44, 44, 54));
        p.drawRoundedRect (a, 3, 3);
        p.setPen (QColor (220, 220, 230));
        p.drawText (a, Qt::AlignCenter, left ? QString::fromUtf8 ("◀") : QString::fromUtf8 ("▶"));
    }
    f.setBold (false);
    f.setPixelSize (11);
    p.setFont (f);
    const int cur = sequencer.groupOff (g) ? -1 : sequencer.currentPattern (g);
    for (int k = 0; k < 16; ++k)
    {
        const int pad = 13 + k % 4 - 4 * (k / 4);
        const int i = bank * 16 + pad - 1;
        const QRectF r = square (k);
        const bool exists = sequencer.hasPattern (g, i);
        p.setPen (exists ? Qt::NoPen : QPen (QColor (44, 44, 52)));
        p.setBrush (i == cur ? color : exists ? color.darker (240) : QColor (20, 20, 25));
        p.drawRoundedRect (r, 5, 5);
        p.setPen (i == cur ? QColor (15, 15, 18) : exists ? QColor (235, 235, 242) : QColor (80, 80, 90));
        p.drawText (r.adjusted (5, 3, -3, -3), Qt::AlignTop | Qt::AlignLeft, QString::number (i + 1));
        if (exists)
        {
            QString name = QString::fromStdString (sequencer.patternName (g, i));
            if (name.isEmpty())
                name = QString ("Pat%1").arg (i + 1);
            p.drawText (r.adjusted (3, 14, -3, -3), Qt::AlignCenter | Qt::TextWordWrap, name);
        }
    }
}

void PaletteView::mousePressEvent (QMouseEvent* e)
{
    pressPos = e->position();
    pressIndex = -1;
    if (e->button() != Qt::LeftButton)
        return;
    if (bankLeft().contains (pressPos) || bankRight().contains (pressPos))
    {
        bank = (bank + (bankLeft().contains (pressPos) ? 3 : 1)) % 4;
        update();
        return;
    }
    const int i = indexAt (pressPos);
    if (i >= 0 && sequencer.hasPattern (state.group, i))
        pressIndex = i;
}

void PaletteView::mouseMoveEvent (QMouseEvent* e)
{
    if (pressIndex < 0 || ! (e->buttons() & Qt::LeftButton) || (e->position() - pressPos).manhattanLength() < 6)
        return;
    // Glisser : le carre suit la souris
    const int g = state.group, i = pressIndex;
    pressIndex = -1;
    auto* mime = new QMimeData;
    mime->setData ("application/x-fuzzy-pattern", QByteArray::number (g) + "," + QByteArray::number (i));
    auto* drag = new QDrag (this);
    drag->setMimeData (mime);
    QPixmap pix (kCell - 4, 30);
    pix.fill (groupColor (g));
    {
        QPainter q (&pix);
        q.setPen (QColor (15, 15, 18));
        QString name = QString::fromStdString (sequencer.patternName (g, i));
        q.drawText (pix.rect(), Qt::AlignCenter, QString ("%1%2").arg (groupName (g)).arg (name.isEmpty() ? QString ("P%1").arg (i + 1) : name));
    }
    drag->setPixmap (pix);
    drag->setHotSpot (QPoint (pix.width() / 2, pix.height() / 2));
    drag->exec (Qt::CopyAction);
}

void PaletteView::contextMenuEvent (QContextMenuEvent* e)
{
    const int i = indexAt (e->pos());
    if (i >= 0)
        emit squareMenu (i, e->globalPos());
}

void PaletteView::mouseReleaseEvent (QMouseEvent* e)
{
    if (pressIndex >= 0 && (e->position() - pressPos).manhattanLength() < 6)
        emit playRequested (state.group, pressIndex);   // simple clic : le jouer
    pressIndex = -1;
}
