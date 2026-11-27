// Fuzzy John DAW Pulses — mixeur : 8 voies (la banque de groupes en cours) + master.
// Chaque voie : vumetre dans la couleur du groupe, et le volume = un petit trait blanc
// que l'on deplace a la souris (ou molette ; double-clic = 100). Licence : GPLv3.
#pragma once
#include "session.h"

#include <QTimer>
#include <QWidget>

#include <array>
#include <functional>
#include <vector>

class MixerView : public QWidget
{
    Q_OBJECT
public:
    // sounds = false : 8 voies de groupes + master ; true : les 16 sons du groupe choisi (onglet Sounds)
    explicit MixerView (session::State& state, bool sounds = false, QWidget* parent = nullptr);

    void setLevels (const std::array<float, 17>& peaks);   // groupes 0-15 + master (0-1)
    void setSoundLevels (const std::vector<float>& peaks); // 256 sons : groupe x 16 + son - 1
    std::function<QString (int)> soundName;                 // nom du son 1-16 du groupe choisi
    std::function<void (int)> soundClicked;                 // clic sur le nom : ce son au clavier (1-16)
    QSize sizeHint() const override { return { 460, 220 }; }
    QSize minimumSizeHint() const override { return { 300, 150 }; }

signals:
    void volumeChanged();                                   // un volume (ou un pan) a bouge
    void muteSoloChanged();                                 // bouton M ou S d'une voie

protected:
    void paintEvent (QPaintEvent*) override;
    void mousePressEvent (QMouseEvent*) override;
    void mouseMoveEvent (QMouseEvent*) override;
    void mouseReleaseEvent (QMouseEvent*) override;
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
    int groupOf (int strip) const;                          // groupe 0-15, -1 = master (Sounds : le son 0-15)
    const bool soundsMode;
    int strips() const { return soundsMode ? 16 : 8; }      // voies reglables (sans le master)
    int total() const { return soundsMode ? 16 : 9; }       // voies dessinees
    double stripLeft (int strip) const;
    std::atomic<int>& panOf (int strip);
    std::atomic<bool>& muteOf (int strip);
    std::atomic<bool>& soloOf (int strip);
    int shownGroup = -1;                                    // Sounds : groupe des niveaux affiches
    void setVolumeFromY (int strip, double y);
    void applyLevels (const std::array<float, 17>& peaks);

    session::State& st;
    std::array<float, 17> hold {};                          // trait de crete (reste ~1 s)
    std::array<int, 17> holdTicks {};
    std::array<float, 17> shown {};                         // niveaux affiches (retombent doucement)
    QTimer fall;
    int dragging = -1;
};
