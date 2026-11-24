// Fuzzy John DAW Pulses — la Maschine MK3 / Maschine+ dans l'app Qt (libs/fuzzy-control).
//
// - Pads : lus dans la tache de fuzzy-control ; le son part TOUT DE SUITE depuis cette tache
//   (padHandler), sans attendre l'interface. L'interface est prevenue ensuite (signal pad).
// - Boutons, encodeurs, molette : signaux dans la tache de l'interface.
// - Lumieres et ecrans : redessines 30 fois par seconde au plus, et seulement s'ils ont change
//   (le trafic USB vers les ecrans peut s'entendre dans la sortie audio de la Maschine).
// Mode partage, sans sudo : l'agent NI est mis en pause tant que l'app tient les ecrans.
// Licence : GPLv3.
#pragma once
#include <QObject>
#include <QString>
#include <QTimer>

#include "fuzzy_control/display.h"
#include "fuzzy_control/lights.h"
#include "fuzzy_control/maschine_mk3.h"

#include <functional>
#include <memory>
#include <mutex>
#include <vector>

class MaschineLink : public QObject
{
    Q_OBJECT
public:
    using PadHandler = std::function<void (int pad, int velocity, bool down)>;   // tache des pads
    using LightPainter = std::function<void (fuzzy::control::Mk3Lights&)>;
    using ScreenPainter = std::function<void (fuzzy::control::Canvas& left, fuzzy::control::Canvas& right)>;

    explicit MaschineLink (QObject* parent = nullptr);
    ~MaschineLink() override;

    bool open();                         // ouvre la Maschine (pads, lumieres, ecrans)
    void close();                        // la rend au logiciel NI
    bool isOpen() const { return device && device->isOpen(); }
    QString status() const { return statusText; }
    QString product() const { return productName; }

    void setPadHandler (PadHandler h) { padHandler = std::move (h); }
    void setLightPainter (LightPainter p) { paintLights = std::move (p); }
    void setScreenPainter (ScreenPainter p) { paintScreens = std::move (p); }

    void updateLights() { lightsDirty = true; }      // redessiner au prochain tour
    void updateScreens() { screensDirty = true; }

signals:
    void pad (int pad, int velocity, bool down);
    void button (const QString& name, bool down);
    void knob (int index, int delta);
    void jog (int delta);
    void statusChanged();

private:
    void poll();

    std::unique_ptr<fuzzy::control::MaschineMk3> device;
    std::unique_ptr<fuzzy::control::Mk3Lights> lights;
    fuzzy::control::Mk3Display display;
    bool screens = false, niPaused = false;
    bool lightsDirty = true, screensDirty = true;
    PadHandler padHandler;
    LightPainter paintLights;
    ScreenPainter paintScreens;
    std::mutex lock;
    std::vector<fuzzy::control::Event> pending;      // evenements a passer a l'interface
    QTimer timer;
    QString statusText, productName;
};
