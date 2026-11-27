// Fuzzy John DAW Pulses — mixeur. Licence : GPLv3.
#include "mixer_view.h"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <cmath>

using namespace session;

namespace {
// Niveau (0-1) -> hauteur (0-1) en decibels : -48 dB en bas, +3 dB en haut
double meterPos (float level)
{
    if (level <= 0.0001f)
        return 0;
    const double db = 20.0 * std::log10 (level);
    return std::clamp ((db + 48.0) / 51.0, 0.0, 1.0);
}
} // namespace

MixerView::MixerView (session::State& state, QWidget* parent) : QWidget (parent), st (state)
{
    setMinimumHeight (150);
    setMouseTracking (false);
    fall.setInterval (33);                                  // les barres retombent en douceur
    connect (&fall, &QTimer::timeout, this, [this]
    {
        // Retombee lente (~ 15 dB / s), comme un vrai vumetre ; la crete reste ~1 s puis retombe
        bool moving = false;
        for (size_t i = 0; i < 17; ++i)
        {
            float& v = shown[i];
            v *= 0.94f;
            if (v < 0.0005f) v = 0;
            if (holdTicks[i] > 0)
                --holdTicks[i];
            else
                hold[i] *= 0.88f;
            if (hold[i] < 0.0005f) hold[i] = 0;
            moving |= v > 0 || hold[i] > 0;
        }
        update();
        if (! moving)
            fall.stop();
    });
}

void MixerView::setLevels (const std::array<float, 17>& peaks)
{
    for (size_t i = 0; i < 17; ++i)
    {
        // Montee adoucie (pas de saut brutal a chaque message) ; descente : par la minuterie
        if (peaks[i] > shown[i])
            shown[i] += (peaks[i] - shown[i]) * 0.6f;
        if (peaks[i] >= hold[i])
        {
            hold[i] = peaks[i];
            holdTicks[i] = 30;                      // ~1 s (minuterie 33 ms)
        }
    }
    if (! fall.isActive())
        fall.start();
    update();
}

int MixerView::groupOf (int strip) const
{
    return strip >= 8 ? -1 : (st.group >= 8 ? 8 : 0) + strip;
}

std::atomic<int>& MixerView::volumeOf (int strip)
{
    const int g = groupOf (strip);
    return g < 0 ? st.masterVolume : st.groupVolume[(size_t) g];
}

QRectF MixerView::meterRect (int strip) const
{
    const double w = width() / 9.4;                         // 8 voies + master un peu a part
    const double x = strip < 8 ? strip * w : 8.4 * w;
    return { x + w * 0.28, 22.0, w * 0.44, height() - 92.0 };
}

QRectF MixerView::buttonRect (int strip, bool solo) const
{
    const QRectF m = meterRect (strip);
    const double bw = std::min (22.0, m.width() * 0.9 + 4), y = m.bottom() + 6;
    return { m.center().x() + (solo ? 1.0 : -1.0 - bw), y, bw, 18.0 };
}

QRectF MixerView::panRect (int strip) const
{
    const QRectF m = meterRect (strip);
    const double w = std::min (64.0, width() / 9.4 * 0.8);
    return { m.center().x() - w / 2, m.bottom() + 30, w, 12.0 };
}

void MixerView::setPanFromX (int strip, double x)
{
    const int g = groupOf (strip);
    if (g < 0)
        return;
    const QRectF r = panRect (strip);
    int v = std::clamp ((int) std::lround ((x - r.left()) / r.width() * 127.0), 0, 127);
    if (std::abs (v - 64) <= 5)                             // aimant : colle au centre
        v = 64;
    st.groupPan[(size_t) g] = v;
    emit volumeChanged();
    update();
}

int MixerView::stripAt (double x) const
{
    for (int s = 0; s < 9; ++s)
    {
        const QRectF r = meterRect (s).adjusted (-width() / 30.0, 0, width() / 30.0, 0);
        if (x >= r.left() && x <= r.right())
            return s;
    }
    return -1;
}

void MixerView::setVolumeFromY (int strip, double y)
{
    const QRectF r = meterRect (strip);
    const int v = (int) std::lround ((r.bottom() - y) / r.height() * 127.0);
    volumeOf (strip) = std::clamp (v, 0, 127);
    emit volumeChanged();
    update();
}

void MixerView::paintEvent (QPaintEvent*)
{
    QPainter g (this);
    g.setRenderHint (QPainter::Antialiasing);
    QFont f = font();
    f.setPixelSize (11);
    g.setFont (f);
    for (int s = 0; s < 9; ++s)
    {
        const int grp = groupOf (s);
        const bool silent = grp >= 0 && ! st.groupAudible (grp);       // coupe / hors solo
        const QColor color = grp < 0 ? kAccent : silent ? QColor (90, 90, 98) : groupColor (grp);
        const QRectF r = meterRect (s);
        // Nom au-dessus, centre
        g.setPen (grp == st.group ? QColor (Qt::white) : color);
        g.drawText (QRectF (r.center().x() - 34, 2, 68, 16), Qt::AlignCenter,
                    grp < 0 ? QString ("MASTER") : g.fontMetrics().elidedText (session::groupTitle (st, grp), Qt::ElideRight, 68));
        // Fond du vumetre
        g.setPen (QColor (40, 40, 48));
        g.setBrush (QColor (14, 14, 18));
        g.drawRoundedRect (r, 3, 3);
        // Niveau (couleur du groupe ; orange puis rouge pres du maximum)
        const double pos = meterPos (shown[(size_t) (grp < 0 ? 16 : grp)]);
        if (pos > 0)
        {
            QRectF bar (r.left() + 2, r.bottom() - 2 - (r.height() - 4) * pos, r.width() - 4, (r.height() - 4) * pos);
            QLinearGradient grad (r.bottomLeft(), r.topLeft());
            grad.setColorAt (0.0, color.darker (130));
            grad.setColorAt (0.78, color);
            grad.setColorAt (0.9, QColor (250, 150, 30));
            grad.setColorAt (1.0, QColor (240, 40, 40));
            g.setPen (Qt::NoPen);
            g.setBrush (grad);
            g.drawRect (bar);
        }
        // Trait de crete
        const double hp = meterPos (hold[(size_t) (grp < 0 ? 16 : grp)]);
        if (hp > 0)
        {
            const double hy = r.bottom() - 2 - (r.height() - 4) * hp;
            g.setPen (QPen (hp > 0.9 ? QColor (240, 60, 50) : QColor (235, 235, 240), 2));
            g.drawLine (QPointF (r.left() + 2, hy), QPointF (r.right() - 2, hy));
        }
        // Graduations 25, 50, 75, 100 (100 = niveau normal)
        g.setPen (QColor (95, 95, 105));
        for (const int mark : { 25, 50, 75, 100 })
        {
            const double my = r.bottom() - r.height() * mark / 127.0;
            g.drawLine (QPointF (r.left() - 4, my), QPointF (r.left() - 1, my));
            g.drawLine (QPointF (r.right() + 1, my), QPointF (r.right() + 4, my));
            if (r.left() - 26 > (s < 8 ? s * width() / 9.4 : 8.4 * width() / 9.4) - 2)
                g.drawText (QRectF (r.left() - 28, my - 7, 22, 14), Qt::AlignRight | Qt::AlignVCenter, QString::number (mark));
        }
        // Volume : le petit trait blanc
        const int vol = volumeOf (s);
        const double vy = r.bottom() - r.height() * vol / 127.0;
        g.setPen (QPen (Qt::white, 2));
        g.drawLine (QPointF (r.left() - 5, vy), QPointF (r.right() + 5, vy));
        // Valeur sous la voie, centree
        g.setPen (QColor (180, 180, 186));
        g.drawText (QRectF (r.center().x() - 30, height() - 20, 60, 16), Qt::AlignCenter, QString::number (vol));
        // Boutons M (mute) et S (solo) des groupes
        if (grp >= 0)
            for (const bool solo : { false, true })
            {
                const QRectF b = buttonRect (s, solo);
                const bool on = solo ? st.groupSolo[(size_t) grp].load() : st.groupMute[(size_t) grp].load();
                g.setPen (QColor (60, 60, 70));
                g.setBrush (on ? (solo ? QColor (232, 192, 64) : QColor (208, 72, 72)) : QColor (22, 22, 27));
                g.drawRoundedRect (b, 3, 3);
                g.setPen (on ? QColor (15, 15, 15) : QColor (150, 150, 158));
                g.drawText (b, Qt::AlignCenter, solo ? "S" : "M");
            }
        // Pan du groupe : barre gauche / droite, curseur au centre = 64
        if (grp >= 0)
        {
            const QRectF pr = panRect (s);
            const int pan = st.groupPan[(size_t) grp];
            g.setPen (QColor (60, 60, 70));
            g.setBrush (QColor (22, 22, 27));
            g.drawRoundedRect (pr, 3, 3);
            const double cx = pr.center().x(), px = pr.left() + pr.width() * pan / 127.0;
            g.setPen (Qt::NoPen);
            g.setBrush (color.darker (120));
            g.drawRect (QRectF (std::min (cx, px), pr.top() + 2, std::abs (px - cx), pr.height() - 4));
            g.setPen (QColor (80, 80, 90));
            g.drawLine (QPointF (cx, pr.top() + 1), QPointF (cx, pr.bottom() - 1));
            g.setPen (QPen (Qt::white, 2));
            g.drawLine (QPointF (px, pr.top() - 2), QPointF (px, pr.bottom() + 2));
            g.setPen (QColor (120, 120, 130));
            g.drawText (QRectF (pr.left() - 12, pr.top() - 2, 10, 16), Qt::AlignCenter, "L");
            g.drawText (QRectF (pr.right() + 2, pr.top() - 2, 10, 16), Qt::AlignCenter, "R");
            if (panDrag && dragging == s)                    // valeur pendant qu'on tire : C, L 20, R 35
            {
                g.setPen (QColor (245, 201, 160));
                g.drawText (QRectF (pr.center().x() - 30, pr.bottom() + 1, 60, 14), Qt::AlignCenter,
                            pan == 64 ? QString ("C") : pan < 64 ? QString ("L %1").arg (64 - pan) : QString ("R %1").arg (pan - 64));
            }
        }
    }
}

void MixerView::mousePressEvent (QMouseEvent* e)
{
    for (int strip = 0; strip < 8; ++strip)              // M / S : couper, solo
        for (const bool solo : { false, true })
            if (buttonRect (strip, solo).contains (e->position()))
            {
                auto& flag = solo ? st.groupSolo[(size_t) groupOf (strip)] : st.groupMute[(size_t) groupOf (strip)];
                flag = ! flag;
                emit muteSoloChanged();
                update();
                return;
            }
    for (int strip = 0; strip < 8; ++strip)              // pan : barre gauche / droite
        if (panRect (strip).adjusted (-4, -4, 4, 4).contains (e->position()))
        {
            if (e->modifiers() & Qt::AltModifier)            // Option + clic : au centre
            {
                st.groupPan[(size_t) groupOf (strip)] = 64;
                emit volumeChanged();
                update();
                return;
            }
            dragging = strip;
            panDrag = true;
            setPanFromX (strip, e->position().x());
            return;
        }
    panDrag = false;
    dragging = stripAt (e->position().x());
    if (dragging >= 0 && e->position().y() <= meterRect (dragging).bottom() + 4)
        setVolumeFromY (dragging, e->position().y());
    else
        dragging = -1;
}

void MixerView::mouseMoveEvent (QMouseEvent* e)
{
    if (dragging >= 0)
        panDrag ? setPanFromX (dragging, e->position().x()) : setVolumeFromY (dragging, e->position().y());
}

void MixerView::mouseDoubleClickEvent (QMouseEvent* e)
{
    for (int strip = 0; strip < 8; ++strip)              // pan : double-clic = centre
        if (panRect (strip).adjusted (-4, -4, 4, 4).contains (e->position()))
        {
            st.groupPan[(size_t) groupOf (strip)] = 64;
            emit volumeChanged();
            update();
            return;
        }
    const int s = stripAt (e->position().x());
    if (s >= 0)
    {
        volumeOf (s) = 100;                                 // niveau normal
        emit volumeChanged();
        update();
    }
}

void MixerView::wheelEvent (QWheelEvent* e)
{
    const int s = stripAt (e->position().x());
    if (s < 0)
        return;
    const int step = e->angleDelta().y() > 0 ? 2 : -2;
    volumeOf (s) = std::clamp (volumeOf (s) + step, 0, 127);
    emit volumeChanged();
    update();
}

void MixerView::mouseReleaseEvent (QMouseEvent*)
{
    dragging = -1;
    panDrag = false;
    update();
}
