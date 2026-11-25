// Fuzzy John DAW Pulses — reglage de la lecture d'un son : One-shot, Gate ou Loop,
// avec la forme d'onde du sample et les points de debut / fin de la boucle a la souris.
// Les changements s'entendent tout de suite (bouton « Hold to preview ») ; Cancel revient
// aux reglages d'avant. Licence : GPLv3.
#pragma once
#include "engine_link.h"
#include "session.h"

#include <QColor>
#include <QDialog>
#include <QList>

class QLabel;
class QButtonGroup;

// Forme d'onde avec la zone de boucle (deux poignees : debut et fin)
class WaveView : public QWidget
{
    Q_OBJECT
public:
    explicit WaveView (QWidget* parent = nullptr);
    void setPeaks (const QList<float>& p, double seconds);
    void setLoop (float start, float end, bool active);
    void setColor (const QColor& c) { color = c; update(); }
    QSize sizeHint() const override { return { 640, 180 }; }

signals:
    void loopChanged (float start, float end);

protected:
    void paintEvent (QPaintEvent*) override;
    void mousePressEvent (QMouseEvent*) override;
    void mouseMoveEvent (QMouseEvent*) override;
    void mouseReleaseEvent (QMouseEvent*) override;

private:
    double xOf (float f) const;
    float fractionAt (double x) const;
    QList<float> peaks;
    double seconds = 0;
    float start = 0, end = 1;
    bool active = true;                              // mode Loop : zone en couleur
    int dragging = 0;                                // 1 = debut, 2 = fin
    QColor color { 230, 60, 60 };
};

class LoopDialog : public QDialog
{
    Q_OBJECT
public:
    LoopDialog (EngineLink& engine, session::State& state, int group, int sound,
                const QString& title, const QColor& color, QWidget* parent = nullptr);

protected:
    void reject() override;                          // Cancel : reglages d'avant

private:
    void apply();                                    // etat + moteur (s'entend tout de suite)
    void refreshText();

    EngineLink& engine;
    session::State& state;
    const int group, sound;
    const size_t slot;
    int oldMode;
    float oldStart, oldEnd;
    double seconds = 0;
    int previewNote = 60;
    WaveView* wave = nullptr;
    QLabel* info = nullptr;
    QButtonGroup* modes = nullptr;
};
