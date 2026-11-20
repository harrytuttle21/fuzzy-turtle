// fuzzy-control : connexion a la Maschine MK3 / Maschine+.
//
// Mode partage (par defaut, sans sudo) : pads / boutons / lumieres a travers le pilote HID du
// systeme (hidapi), ecrans par libusb sur la seule interface 5. Rien n'est detache : la carte
// son de la Maschine reste disponible. Sur macOS, l'agent NI (NIHostIntegrationAgent) tient
// les ecrans : il faut le mettre en pause (voir ni_agent.h).
// Mode exclusif (ancien, sudo) : libusb detache les pilotes ; la carte son disparait.
#pragma once
#include "decoder.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

struct libusb_context;
struct libusb_device_handle;
struct hid_device_;

namespace fuzzy::control {

class MaschineMk3
{
public:
    static constexpr uint16_t kVendor = 0x17cc;
    static constexpr uint16_t kProductMk3 = 0x1600;
    static constexpr uint16_t kProductPlus = 0x1820;
    static constexpr int kHidInterface = 4;
    static constexpr int kDisplayInterface = 5;

    enum class Access { Shared, Exclusive };
    using Callback = std::function<void (const Event&)>;

    MaschineMk3();
    ~MaschineMk3();
    MaschineMk3 (const MaschineMk3&) = delete;
    MaschineMk3& operator= (const MaschineMk3&) = delete;

    // Ouvre la Maschine et reserve l'interface des pads / boutons.
    // En cas d'echec, renvoie false et explique pourquoi dans `error`.
    bool open (std::string& error, Access access = Access::Shared);
    void close();
    bool isOpen() const { return hid != nullptr || claimed; }
    Access access() const { return mode; }
    std::string productName() const { return product; }

    // Lit la Maschine dans une tache a part et appelle `callback` pour chaque evenement.
    // Le rappel est appele depuis cette tache : il doit etre court (ex. mettre en file).
    bool start (Callback callback, std::string& error);
    void stop();

    Mk3Decoder& decoder() { return dec; }

    // Envoie un rapport brut (LED : 0x80 boutons, 0x81 pads).
    bool write (const uint8_t* data, int length);

    // Ecrans : reserver l'interface 5 (apres open), puis envoyer des paquets d'image.
    bool openDisplay (std::string& error);
    bool writeDisplay (const uint8_t* data, int length);

private:
    void readLoop();
    bool openUsb (std::string& error);
    bool openExclusive (std::string& error);

    Access mode = Access::Shared;
    hid_device_* hid = nullptr;
    libusb_context* context = nullptr;
    libusb_device_handle* handle = nullptr;
    unsigned char endpointIn = 0, endpointOut = 0;
    unsigned char endpointDisplay = 0;
    bool claimed = false, detached = false, displayClaimed = false, displayDetached = false;
    std::string product;
    Mk3Decoder dec;
    Callback onEvent;
    std::thread reader;
    std::atomic<bool> running { false };
};

} // namespace fuzzy::control
