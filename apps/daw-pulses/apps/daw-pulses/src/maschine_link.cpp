// Fuzzy John DAW Pulses — la Maschine dans l'app Qt. Licence : GPLv3.
#include "maschine_link.h"

#include "fuzzy_control/calibration.h"
#include "fuzzy_control/ni_agent.h"

using namespace fuzzy::control;

MaschineLink::MaschineLink (QObject* parent) : QObject (parent)
{
    timer.setInterval (33);                          // 30 fois par seconde au plus
    connect (&timer, &QTimer::timeout, this, &MaschineLink::poll);
}

MaschineLink::~MaschineLink()
{
    close();
}

bool MaschineLink::open()
{
    if (isOpen())
        return true;
    device = std::make_unique<MaschineMk3>();
    std::string error;
    if (! device->open (error, MaschineMk3::Access::Shared))
    {
        statusText = tr ("Maschine: %1").arg (QString::fromStdString (error));
        device.reset();
        emit statusChanged();
        return false;
    }
    productName = QString::fromStdString (device->productName());

    // Ecrans : sur macOS l'agent NI les tient -> pause, relance a la fermeture
    screens = device->openDisplay (error);
    if (! screens && ni::agentRunning())
    {
        niPaused = ni::pauseAgent();
        error.clear();
        screens = device->openDisplay (error);
    }

    // Calibration : valeurs du modele, puis le fichier de l'utilisateur s'il existe
    Calibration cal = Calibration::defaultsFor (device->productName());
    std::vector<std::string> warnings;
    cal.load (defaultCalibrationPath (device->productName()), warnings);
    device->decoder().applyCalibration (cal);
    lights = std::make_unique<Mk3Lights> (cal);

    if (! device->start ([this] (const Event& e)
        {
            // Tache des pads : le son part tout de suite, l'interface suit.
            if ((e.type == Event::Type::PadOn || e.type == Event::Type::PadOff) && padHandler)
                padHandler (e.id, e.value, e.type == Event::Type::PadOn);
            std::lock_guard<std::mutex> g (lock);
            pending.push_back (e);
        }, error))
    {
        statusText = tr ("Maschine: %1").arg (QString::fromStdString (error));
        close();
        return false;
    }
    statusText = tr ("%1 connected%2").arg (productName, screens ? "" : tr (" (no screens)"));
    lightsDirty = screensDirty = true;
    display.forceFullRedraw();
    timer.start();
    emit statusChanged();
    return true;
}

void MaschineLink::close()
{
    timer.stop();
    if (device)
    {
        device->stop();
        if (lights)
        {
            lights->clear();
            lights->send (*device);
        }
        if (screens)
        {
            display.screen (0).fill (kBlack);
            display.screen (1).fill (kBlack);
            display.send (*device);
        }
        device->close();
        device.reset();
    }
    if (niPaused)
        ni::resumeAgent();                           // la Maschine revient au logiciel NI
    niPaused = screens = false;
}

void MaschineLink::poll()
{
    std::vector<Event> events;
    {
        std::lock_guard<std::mutex> g (lock);
        events.swap (pending);
    }
    for (const auto& e : events)
    {
        switch (e.type)
        {
            case Event::Type::PadOn:      emit pad (e.id, e.value, true); break;
            case Event::Type::PadOff:
                if (std::string (e.name) == "missed")    // relachement perdu, rattrape par la garde
                    qWarning ("[maschine] pad %d : relachement perdu, note arretee "
                              "(plus grand ecart normal entre deux pressions : %d ms)",
                              e.id, device ? device->decoder().maxPadGapMs() : 0);
                emit pad (e.id, 0, false);
                break;
            case Event::Type::ButtonDown: emit button (QString::fromUtf8 (e.name), true); break;
            case Event::Type::ButtonUp:   emit button (QString::fromUtf8 (e.name), false); break;
            case Event::Type::Knob:       emit knob (e.id, e.value); break;
            case Event::Type::Jog:        emit jog (e.value); break;
            default: break;
        }
    }
    if (! device)
        return;
    if (lightsDirty && lights && paintLights)
    {
        lightsDirty = false;
        paintLights (*lights);
        lights->send (*device);                      // seulement ce qui a change
    }
    if (screensDirty && screens && paintScreens)
    {
        screensDirty = false;
        paintScreens (display.screen (0), display.screen (1));
        display.send (*device);                      // seulement les lignes qui ont change
    }
}
