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
    // Pads aussi grands que la place le permet ; un peu plus larges que hauts si le panneau
    // est large (jusqu'a 1,35 fois) : plus de place pour les noms
    const double ch = std::min (width(), height()) / 4.0;
    const double cw = std::min (width() / 4.0, ch * 1.35), gap = 6;
    const int col = (pad - 1) % 4, row = 3 - (pad - 1) / 4;      // pad 1 en bas a gauche
    const double x0 = (width() - 4 * cw) / 2, y0 = (height() - 4 * ch) / 2;   // centres
    return { x0 + col * cw + gap / 2, y0 + row * ch + gap / 2, cw - gap, ch - gap };
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
    const double cell = std::min (padRect (1).width(), padRect (1).height() * 1.2);
    QFont f = font();
    f.setPixelSize (std::clamp (int (cell * 0.17), 10, 20));   // plus grand : lisible
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
        if (chosen.contains (pad))                   // pad choisi : cadre orange en plus
        {
            g.setPen (QPen (QColor (252, 140, 40), 3));
            g.setBrush (Qt::NoBrush);
            g.drawRoundedRect (r.adjusted (3, 3, -3, -3), 6, 6);
        }
        if (latched[(size_t) pad - 1])               // Loop SP : la boucle tourne (cadre qui clignote)
        {
            g.setPen (QPen (blink ? QColor (90, 230, 120) : QColor (40, 120, 60), 3));
            g.setBrush (Qt::NoBrush);
            g.drawRoundedRect (r.adjusted (2, 2, -2, -2), 7, 7);
            g.setPen (QColor (120, 240, 150));
            g.drawText (r.adjusted (8, 6, -8, -6), Qt::AlignRight | Qt::AlignTop, QStringLiteral ("LOOP"));   // en haut a droite
        }
        g.setPen (hit ? QColor (20, 20, 20) : QColor (230, 230, 230));
        g.drawText (r.adjusted (8, 6, -8, -6), Qt::AlignLeft | Qt::AlignTop, QString ("%1").arg (pad, 2, 10, QChar ('0')));
        if (tag == 'M' || tag == 'S')                    // M / S en haut a droite
        {
            g.setPen (tag == 'S' ? QColor (250, 210, 60) : QColor (150, 150, 158));
            g.drawText (r.adjusted (8, 6, latched[(size_t) pad - 1] ? -48 : -8, -6), Qt::AlignRight | Qt::AlignTop, QString (QChar (tag)));   // a gauche de LOOP
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
    const int at = padAt (e->position());
    if (at && (e->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier)))
    {
        // Selection (sans jouer) : Cmd + clic = ce pad ; Shift + clic = du dernier pad a celui-ci
        if (chosen.isEmpty())                        // la selection part du dernier pad clique
            chosen.insert (anchor ? anchor : selected);
        if (e->modifiers() & Qt::ControlModifier)
        {
            chosen.contains (at) ? (void) chosen.remove (at) : (void) chosen.insert (at);
            anchor = at;
        }
        else
        {
            const int from = anchor ? anchor : selected;
            const int r0 = (from - 1) / 4, c0 = (from - 1) % 4, r1 = (at - 1) / 4, c1 = (at - 1) % 4;
            for (int r = std::min (r0, r1); r <= std::max (r0, r1); ++r)
                for (int c = std::min (c0, c1); c <= std::max (c0, c1); ++c)
                    chosen.insert (r * 4 + c + 1);
        }
        update();
        emit selectionChanged();
        return;
    }
    mousePad = at;
    if (! mousePad)
        return;
    if (! chosen.isEmpty())                          // clic simple : fin de la selection
    {
        chosen.clear();
        emit selectionChanged();
    }
    anchor = mousePad;
    // Velocite selon la hauteur du clic dans le pad : en haut = fort
    const QRectF r = padRect (mousePad);
    const int vel = qBound (20, int (127 - (e->position().y() - r.top()) / r.height() * 90), 127);
    pressPos = e->position();
    emit pressed (mousePad, vel);
}

QList<int> PadGrid::selection() const
{
    QList<int> out (chosen.begin(), chosen.end());
    std::sort (out.begin(), out.end());
    return out;
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
    mime->setData (kSoundMime, QString ("%1:%2").arg (group).arg (pad).toLatin1());
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
    if (e->mimeData()->hasFormat (kSoundMime) || ! audioFiles (e->mimeData()).isEmpty()
        || ! session::midiFileIn (e->mimeData()->urls()).isEmpty())
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
    if (e->mimeData()->hasFormat (kSoundMime))       // un autre pad lache ici : copier / deplacer
    {
        const QStringList from = QString::fromLatin1 (e->mimeData()->data (kSoundMime)).split (':');
        dropPad = 0;
        update();
        if (pad && from.size() == 2)
        {
            e->acceptProposedAction();
            const int g = from[0].toInt(), p = from[1].toInt();
            const QPoint at = mapToGlobal (e->position().toPoint());
            if (! (g == group && p == pad))           // apres le glisser (pas pendant)
                QMetaObject::invokeMethod (this, [this, g, p, pad, at] { emit soundDropped (g, p, pad, at); }, Qt::QueuedConnection);
        }
        return;
    }
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
