// Fuzzy John DAW Pulses — mixeur : 8 voies (la banque de groupes en cours) + master.
// Chaque voie : vumetre dans la couleur du groupe, et le volume = un petit trait blanc
// que l'on deplace a la souris (ou molette ; double-clic = 100). Licence : GPLv3.
#pragma once
#include "session.h"

#include <QTimer>
#include <QWidget>

#include <array>

class MixerView : public QWidget
{
    Q_OBJECT
public:
    explicit MixerView (session::State& state, QWidget* parent = nullptr);

    void setLevels (const std::array<float, 17>& peaks);   // groupes 0-15 + master (0-1)
    QSize sizeHint() const override { return { 460, 220 }; }
    QSize minimumSizeHint() const override { return { 300, 150 }; }

signals:
    void volumeChanged();                                   // un volume (ou un pan) a bouge
    void muteSoloChanged();                                 // bouton M ou S d'une voie

protected:
    void paintEvent (QPaintEvent*) override;
    void mousePressEvent (QMouseEvent*) override;
    void mouseMoveEvent (QMouseEvent*) override;
    void mouseDoubleClickEvent (QMouseEvent*) override;
    void wheelEvent (QWheelEvent*) override;

private:
    int stripAt (double x) const;                           // 0-7 voies, 8 = master, -1 = rien
    QRectF meterRect (int strip) const;
    QRectF buttonRect (int strip, bool solo) const;         // boutons M et S sous la voie
    QRectF panRect (int strip) const;                       // pan gauche / droite (sous M et S)
    void setPanFromX (int strip, double x);
    bool panDrag = false;
    std::atomic<int>& volumeOf (int strip);
    int groupOf (int strip) const;                          // groupe 0-15, -1 = master
    void setVolumeFromY (int strip, double y);

    session::State& st;
    std::array<float, 17> shown {};                         // niveaux affiches (retombent doucement)
    QTimer fall;
    int dragging = -1;
};
