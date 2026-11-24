// Fuzzy John DAW Pulses — clavier dessine (2 octaves), jouable a la souris et au clavier
// de l'ordinateur, notes tenues tant que la touche est appuyee. Licence : GPLv3.
#pragma once
#include <QColor>
#include <QMap>
#include <QWidget>

#include <set>

class PianoKeys : public QWidget
{
    Q_OBJECT
public:
    explicit PianoKeys (QWidget* parent = nullptr);

    void setBaseNote (int note) { base = note; update(); }        // C de l'octave du bas
    int baseNote() const { return base; }
    void setColor (const QColor& c) { color = c; update(); }
    void setHeld (int note, bool held);                           // allume / eteint une touche
    void setKeyLabels (const QMap<int, QString>& l) { labels = l; update(); }   // decalage -> lettre

    QSize sizeHint() const override { return { 700, 90 }; }

signals:
    void noteOn (int note, int velocity);
    void noteOff (int note);

protected:
    void paintEvent (QPaintEvent*) override;
    void mousePressEvent (QMouseEvent*) override;
    void mouseMoveEvent (QMouseEvent*) override;
    void mouseReleaseEvent (QMouseEvent*) override;

private:
    int noteAt (const QPointF& p) const;
    QRectF keyRect (int offset) const;
    static bool black (int offset);

    static constexpr int kKeys = 25;                              // 2 octaves + le C du haut
    int base = 48;
    int mouseNote = -1;
    QColor color { 252, 95, 0 };
    std::set<int> held;
    QMap<int, QString> labels;
};
