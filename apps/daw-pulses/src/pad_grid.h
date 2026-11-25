// Fuzzy John DAW Pulses — les 16 pads a l'ecran (comme sur la Maschine : 1 en bas a gauche).
// Licence : GPLv3.
#pragma once
#include <QColor>
#include <QElapsedTimer>
#include <QWidget>

#include <array>

inline constexpr const char* kPadMime = "application/x-fuzzy-pad";   // pad glisse

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
    QSize minimumSizeHint() const override { return { 220, 220 }; }   // texte encore lisible
    void setTags (const std::array<char, 16>& t) { tags = t; update(); }   // M / S par pad
    void setEmpty (const std::array<bool, 16>& e) { empty = e; update(); }  // sons sans instrument
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth (int w) const override { return w; }

signals:
    void pressed (int pad, int velocity);
    void released (int pad);
    void menuRequested (int pad, const QPoint& globalPos);
    void filesDropped (int pad, const QStringList& paths);   // fichiers audio deposes (Finder)
    void midiDropped (const QString& path, int sound);       // fichier .mid depose sur un pad

protected:
    void paintEvent (QPaintEvent*) override;
    void mousePressEvent (QMouseEvent*) override;
    void mouseReleaseEvent (QMouseEvent*) override;
    void mouseMoveEvent (QMouseEvent*) override;      // glisser un pad vers le pattern
    void contextMenuEvent (QContextMenuEvent*) override;   // clic droit, deux doigts, Ctrl + clic
    void dragEnterEvent (QDragEnterEvent*) override;  // fichiers audio depuis le Finder
    void dragMoveEvent (QDragMoveEvent*) override;
    void dragLeaveEvent (QDragLeaveEvent*) override;
    void dropEvent (QDropEvent*) override;

private:
    int padAt (const QPointF& p) const;
    QRectF padRect (int pad) const;

    QColor color { 252, 95, 0 };
    int selected = 1, mousePad = 0;
    QPointF pressPos;
    int dropPad = 0;                                 // pad vise pendant un depot de fichiers
    std::array<QString, 16> labels;
    std::array<bool, 16> down {};
    std::array<char, 16> tags {};
    std::array<bool, 16> empty {};
};
