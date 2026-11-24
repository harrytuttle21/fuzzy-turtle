// Fuzzy John DAW Pulses — les 16 pads a l'ecran (comme sur la Maschine : 1 en bas a gauche).
// Licence : GPLv3.
#pragma once
#include <QColor>
#include <QElapsedTimer>
#include <QWidget>

#include <array>

class PadGrid : public QWidget
{
    Q_OBJECT
public:
    explicit PadGrid (QWidget* parent = nullptr);

    void setColor (const QColor& c) { color = c; update(); }
    void setSelected (int pad) { selected = pad; update(); }
    void setLabel (int pad, const QString& text) { labels[(size_t) pad - 1] = text; update(); }
    void flash (int pad, bool down);                 // pad frappe (souris ou Maschine)

    QSize sizeHint() const override { return { 420, 420 }; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth (int w) const override { return w; }

signals:
    void pressed (int pad, int velocity);
    void released (int pad);

protected:
    void paintEvent (QPaintEvent*) override;
    void mousePressEvent (QMouseEvent*) override;
    void mouseReleaseEvent (QMouseEvent*) override;

private:
    int padAt (const QPointF& p) const;
    QRectF padRect (int pad) const;

    QColor color { 252, 95, 0 };
    int selected = 1, mousePad = 0;
    std::array<QString, 16> labels;
    std::array<bool, 16> down {};
};
