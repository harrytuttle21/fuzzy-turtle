// Fuzzy John DAW Pulses — reglage One-shot / Gate / Loop d'un son. Licence : GPLv3.
#include "loop_dialog.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {
constexpr double kMargin = 10;                       // marge gauche / droite de la forme d'onde
constexpr float kMinLoop = 0.002f;                   // boucle mini (fraction du fichier)
}

// ------------------------------------------------------------------------------- forme d'onde

WaveView::WaveView (QWidget* parent) : QWidget (parent)
{
    setMinimumSize (420, 140);
    setMouseTracking (true);
}

void WaveView::setPeaks (const QList<float>& p, double s)
{
    peaks = p;
    seconds = s;
    update();
}

void WaveView::setLoop (float s, float e, bool on)
{
    start = s;
    end = e;
    active = on;
    update();
}

double WaveView::xOf (float f) const
{
    return kMargin + f * (width() - 2 * kMargin);
}

float WaveView::fractionAt (double x) const
{
    return (float) std::clamp ((x - kMargin) / (width() - 2 * kMargin), 0.0, 1.0);
}

void WaveView::paintEvent (QPaintEvent*)
{
    QPainter g (this);
    g.fillRect (rect(), QColor (18, 18, 22));
    const double mid = height() / 2.0, half = height() / 2.0 - 14;
    if (peaks.isEmpty())
    {
        g.setPen (QColor (120, 120, 128));
        g.drawText (rect(), Qt::AlignCenter, seconds > 0 || ! isEnabled() ? tr ("No sample on this pad")
                                                                          : tr ("Reading the sample..."));
        return;
    }
    // Zone de la boucle
    const double x0 = xOf (start), x1 = xOf (end);
    QColor zone = color;
    zone.setAlphaF (active ? 0.22 : 0.08);
    g.fillRect (QRectF (x0, 0, x1 - x0, height()), zone);
    // Forme d'onde : une barre par crete
    const double w = (width() - 2 * kMargin) / peaks.size();
    for (int i = 0; i < peaks.size(); ++i)
    {
        const double x = kMargin + i * w, f = (i + 0.5) / peaks.size();
        const bool inside = f >= start && f <= end;
        g.fillRect (QRectF (x, mid - peaks[i] * half, std::max (1.0, w - 0.5), std::max (1.0, 2 * peaks[i] * half)),
                    inside && active ? color : QColor (110, 110, 120));
    }
    // Poignees : debut (S) et fin (E)
    for (const bool isEnd : { false, true })
    {
        const double x = isEnd ? x1 : x0;
        g.setPen (QPen (Qt::white, 2));
        g.drawLine (QPointF (x, 0), QPointF (x, height()));
        const QPointF tri[3] { { x, 0 }, { x + (isEnd ? -10.0 : 10.0), 0 }, { x, 12 } };
        g.setPen (Qt::NoPen);
        g.setBrush (Qt::white);
        g.drawPolygon (tri, 3);
        g.setPen (QColor (220, 220, 226));
        g.drawText (QRectF (isEnd ? x - 46 : x + 4, height() - 16, 42, 14), isEnd ? Qt::AlignRight : Qt::AlignLeft,
                    isEnd ? "END" : "START");
    }
}

void WaveView::mousePressEvent (QMouseEvent* e)
{
    // La poignee la plus proche du clic ; clic loin des deux = la plus proche aussi
    const double x = e->position().x();
    dragging = std::abs (x - xOf (start)) <= std::abs (x - xOf (end)) ? 1 : 2;
    mouseMoveEvent (e);
}

void WaveView::mouseMoveEvent (QMouseEvent* e)
{
    const double x = e->position().x();
    if (! dragging)
    {
        const bool near = std::min (std::abs (x - xOf (start)), std::abs (x - xOf (end))) < 8;
        setCursor (near ? Qt::SizeHorCursor : Qt::ArrowCursor);
        return;
    }
    const float f = fractionAt (x);
    if (dragging == 1)
        start = std::min (f, end - kMinLoop);
    else
        end = std::max (f, start + kMinLoop);
    update();
    emit loopChanged (start, end);
}

void WaveView::mouseReleaseEvent (QMouseEvent*)
{
    dragging = 0;
}

// ------------------------------------------------------------------------------- fenetre

LoopDialog::LoopDialog (EngineLink& e, session::State& s, int g, int snd, const QString& title,
                        const QColor& color, QWidget* parent)
    : QDialog (parent), engine (e), state (s), group (g), sound (snd),
      slot ((size_t) session::State::index (g, snd))
{
    setWindowTitle (tr ("Sound %1 — %2").arg (snd, 2, 10, QChar ('0')).arg (title));
    oldMode = state.playMode[slot];
    oldStart = state.loopStart[slot];
    oldEnd = state.loopEnd[slot];
    const bool hasSample = ! state.samples[slot].isEmpty();

    auto* col = new QVBoxLayout (this);
    // Mode de lecture
    auto* row = new QHBoxLayout;
    modes = new QButtonGroup (this);
    const QStringList names { tr ("One-shot (plays to the end)"), tr ("Gate (stops on release)"), tr ("Loop (while held)") };
    for (int i = 0; i < names.size(); ++i)
    {
        auto* b = new QRadioButton (names[i]);
        b->setEnabled (i != session::State::Loop || hasSample);   // boucle : samples seulement
        modes->addButton (b, i);
        row->addWidget (b);
    }
    modes->button (oldMode)->setChecked (true);
    row->addStretch();
    col->addLayout (row);

    wave = new WaveView;
    wave->setColor (color);
    wave->setLoop (oldStart, oldEnd, oldMode == session::State::Loop);
    wave->setEnabled (hasSample);
    col->addWidget (wave, 1);

    info = new QLabel;
    auto* bottom = new QHBoxLayout;
    auto* preview = new QPushButton (tr ("Hold to preview"));
    preview->setToolTip (tr ("Plays the sound while the button is held"));
    connect (preview, &QPushButton::pressed, this, [this]
    {
        previewNote = state.pitched (group, sound, session::defaultNote (sound));
        engine.hit (sound, 110, previewNote, group, state.held (group, sound), state.releaseMs[slot],
                    0, state.effSynth (group, sound), state.pan (group, sound));
    });
    connect (preview, &QPushButton::released, this, [this]
    {
        engine.noteOff (sound, previewNote, group);
    });
    bottom->addWidget (info, 1);
    bottom->addWidget (preview);
    col->addLayout (bottom);

    auto* buttons = new QDialogButtonBox (QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect (buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect (buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    col->addWidget (buttons);

    connect (modes, &QButtonGroup::idClicked, this, [this] (int id)
    {
        state.playMode[slot] = id;
        wave->setLoop (state.loopStart[slot], state.loopEnd[slot], id == session::State::Loop);
        apply();
    });
    connect (wave, &WaveView::loopChanged, this, [this] (float s, float e)
    {
        state.loopStart[slot] = s;
        state.loopEnd[slot] = e;
        if (state.playMode[slot] != session::State::Loop)   // toucher la boucle = passer en Loop
        {
            state.playMode[slot] = session::State::Loop;
            modes->button (session::State::Loop)->setChecked (true);
            wave->setLoop (s, e, true);
        }
        apply();
    });
    // Forme d'onde : demandee au moteur (c'est lui qui a le fichier en memoire)
    connect (&engine, &EngineLink::samplePeaks, this, [this] (int g, int snd, double secs, const QList<float>& p)
    {
        if (g != group || snd != sound)
            return;
        seconds = secs;
        wave->setPeaks (p, secs);
        refreshText();
    });
    if (hasSample)
        engine.requestPeaks (group, sound);
    refreshText();
    resize (720, 320);
}

void LoopDialog::apply()
{
    engine.setLoop (group, sound, state.playMode[slot] == session::State::Loop, state.loopStart[slot], state.loopEnd[slot]);
    refreshText();
}

void LoopDialog::refreshText()
{
    if (state.playMode[slot] != session::State::Loop || seconds <= 0)
    {
        info->setText (seconds > 0 ? tr ("Length %1 s").arg (seconds, 0, 'f', 3) : QString());
        return;
    }
    const double s = state.loopStart[slot] * seconds, e = state.loopEnd[slot] * seconds;
    info->setText (tr ("Loop  start %1 s   end %2 s   length %3 s")
                       .arg (s, 0, 'f', 3).arg (e, 0, 'f', 3).arg (e - s, 0, 'f', 3));
}

void LoopDialog::reject()
{
    state.playMode[slot] = oldMode;
    state.loopStart[slot] = oldStart;
    state.loopEnd[slot] = oldEnd;
    apply();
    QDialog::reject();
}
