// Fuzzy John DAW Pulses — les 16 pads a l'ecran. Licence : GPLv3.
#include "pad_grid.h"
#include "session.h"

#include <QApplication>
#include <QFontMetricsF>
#include <algorithm>
#include <QDrag>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QContextMenuEvent>
#include <QFileInfo>
#include <QUrl>
#include <QPixmap>

#include <QMouseEvent>
#include <QPainter>

PadGrid::PadGrid (QWidget* parent) : QWidget (parent)
{
    setMinimumSize (220, 220);
    setAcceptDrops (true);                           // fichiers audio glisses depuis le Finder
    setSizePolicy (QSizePolicy::Expanding, QSizePolicy::Expanding);
}

QRectF PadGrid::padRect (int pad) const
{
    const double side = std::min (width(), height());
    const double cell = side / 4.0, gap = 8;
    const int col = (pad - 1) % 4, row = 3 - (pad - 1) / 4;      // pad 1 en bas a gauche
    const double x0 = (width() - side) / 2, y0 = (height() - side) / 2;   // centres
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
    // Texte a la taille d'un pad (lisible meme quand le panneau est etroit)
    const double cell = padRect (1).width();
    QFont f = font();
    f.setPixelSize (std::clamp (int (cell * 0.15), 8, 15));
    g.setFont (f);
    const QFontMetricsF fm (f);
    for (int pad = 1; pad <= 16; ++pad)
    {
        const QRectF r = padRect (pad);
        const bool hit = down[(size_t) pad - 1];
        const char tag = tags[(size_t) pad - 1];          // 'M' coupe, 'S' solo, 'm' hors solo
        const bool silent = tag == 'M' || tag == 'm';
        QColor fill = hit ? color : color.darker (pad == selected ? 260 : 420);
        if (silent && ! hit)
            fill = QColor (34, 34, 38);                   // son coupe : gris
        if (empty[(size_t) pad - 1] && ! hit)
            fill = QColor (22, 22, 26);                   // son vide : presque noir
        g.setPen (pad == dropPad ? QPen (Qt::white, 2, Qt::DashLine)
                  : pad == selected ? QPen (Qt::white, 2) : QPen (color.darker (200), 1));
        g.setBrush (fill);
        g.drawRoundedRect (r, 8, 8);
        g.setPen (hit ? QColor (20, 20, 20) : QColor (230, 230, 230));
        g.drawText (r.adjusted (8, 6, -8, -6), Qt::AlignLeft | Qt::AlignTop, QString ("%1").arg (pad, 2, 10, QChar ('0')));
        if (tag == 'M' || tag == 'S')                    // M / S en haut a droite
        {
            g.setPen (tag == 'S' ? QColor (250, 210, 60) : QColor (150, 150, 158));
            g.drawText (r.adjusted (8, 6, -8, -6), Qt::AlignRight | Qt::AlignTop, QString (QChar (tag)));
            g.setPen (hit ? QColor (20, 20, 20) : QColor (230, 230, 230));
        }
        if ((silent || empty[(size_t) pad - 1]) && ! hit)
            g.setPen (QColor (110, 110, 118));
        // Nom au centre : sur deux lignes si besoin, sinon raccourci avec « ... »
        const QRectF nameBox = r.adjusted (5, fm.height() + 4, -5, -4);
        if (nameBox.width() > 16 && nameBox.height() > fm.height())
        {
            const QString name = labels[(size_t) pad - 1];
            const QRectF need = fm.boundingRect (nameBox, Qt::AlignCenter | Qt::TextWordWrap, name);
            if (need.width() <= nameBox.width() + 0.5 && need.height() <= nameBox.height())
                g.drawText (nameBox, Qt::AlignCenter | Qt::TextWordWrap, name);
            else
                g.drawText (nameBox, Qt::AlignCenter, fm.elidedText (name, Qt::ElideRight, nameBox.width()));
        }
    }
}

void PadGrid::mousePressEvent (QMouseEvent* e)
{
    // Clic secondaire (bouton droit, ou Ctrl + clic sur Mac) : pas de son, le menu arrive
    // par contextMenuEvent
    if (e->button() == Qt::RightButton || (e->modifiers() & Qt::MetaModifier))
        return;
    mousePad = padAt (e->position());
    if (! mousePad)
        return;
    // Velocite selon la hauteur du clic dans le pad : en haut = fort
    const QRectF r = padRect (mousePad);
    const int vel = qBound (20, int (127 - (e->position().y() - r.top()) / r.height() * 90), 127);
    pressPos = e->position();
    emit pressed (mousePad, vel);
}

void PadGrid::mouseReleaseEvent (QMouseEvent*)
{
    if (mousePad)
        emit released (mousePad);
    mousePad = 0;
}

void PadGrid::mouseMoveEvent (QMouseEvent* e)
{
    // Pad tenu et souris deplacee : on emporte le son (a deposer dans le PATTERN)
    if (! mousePad || (e->position() - pressPos).manhattanLength() < QApplication::startDragDistance())
        return;
    const int pad = mousePad;
    emit released (pad);                              // le son s'arrete
    mousePad = 0;
    const QRectF r = padRect (pad);
    QPixmap image (r.size().toSize());
    image.fill (Qt::transparent);
    render (&image, QPoint(), QRegion (r.toRect()));
    auto* mime = new QMimeData;
    mime->setData (kPadMime, QByteArray::number (pad));
    auto* drag = new QDrag (this);
    drag->setMimeData (mime);
    drag->setPixmap (image.scaled (image.size() / 2, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    drag->setHotSpot (QPoint (image.width() / 4, image.height() / 4));
    drag->exec (Qt::CopyAction);
}

namespace {
// Fichiers audio parmi ce qui est glisse (WAV, AIFF, FLAC, Ogg, MP3, et sur Mac M4A / AAC)
QStringList audioFiles (const QMimeData* mime)
{
    static const QStringList exts { "wav", "aif", "aiff", "flac", "ogg", "mp3", "m4a", "aac", "caf" };
    QStringList out;
    for (const QUrl& u : mime->urls())
        if (u.isLocalFile() && exts.contains (QFileInfo (u.toLocalFile()).suffix().toLower()))
            out << u.toLocalFile();
    out.sort (Qt::CaseInsensitive);                    // plusieurs fichiers : par ordre de nom
    return out;
}
}

void PadGrid::dragEnterEvent (QDragEnterEvent* e)
{
    if (! audioFiles (e->mimeData()).isEmpty() || ! session::midiFileIn (e->mimeData()->urls()).isEmpty())
        e->acceptProposedAction();
}

void PadGrid::dragMoveEvent (QDragMoveEvent* e)
{
    dropPad = padAt (e->position());
    dropPad ? e->acceptProposedAction() : e->ignore();
    update();
}

void PadGrid::dragLeaveEvent (QDragLeaveEvent*)
{
    dropPad = 0;
    update();
}

void PadGrid::dropEvent (QDropEvent* e)
{
    const int pad = padAt (e->position());
    const QString midiPath = session::midiFileIn (e->mimeData()->urls());
    if (pad && ! midiPath.isEmpty())                 // fichier MIDI : les notes pour ce son
    {
        dropPad = 0;
        update();
        e->acceptProposedAction();
        emit midiDropped (midiPath, pad);
        return;
    }
    const QStringList files = audioFiles (e->mimeData());
    dropPad = 0;
    update();
    if (pad && ! files.isEmpty())
    {
        e->acceptProposedAction();
        emit filesDropped (pad, files);
    }
}

void PadGrid::contextMenuEvent (QContextMenuEvent* e)
{
    if (const int pad = padAt (e->pos()))
        emit menuRequested (pad, e->globalPos());
}
