// fuzzy-control : connexion a la Maschine MK3 / Maschine+ par libusb.
//
// Sur macOS, prendre la Maschine retire le pilote Native Instruments de TOUT l'appareil
// (il faut les droits administrateur : sudo) ; la carte son de la Maschine disparait alors.
#pragma once
#include "decoder.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

struct libusb_context;
struct libusb_device_handle;

namespace fuzzy::control {

class MaschineMk3
{
public:
    static constexpr uint16_t kVendor = 0x17cc;
    static constexpr uint16_t kProductMk3 = 0x1600;
    static constexpr uint16_t kProductPlus = 0x1820;
    static constexpr int kHidInterface = 4;

    using Callback = std::function<void (const Event&)>;

    MaschineMk3();
    ~MaschineMk3();
    MaschineMk3 (const MaschineMk3&) = delete;
    MaschineMk3& operator= (const MaschineMk3&) = delete;

    // Ouvre la Maschine et reserve l'interface des pads / boutons.
    // En cas d'echec, renvoie false et explique pourquoi dans `error`.
    bool open (std::string& error);
    void close();
    bool isOpen() const { return handle != nullptr; }
    std::string productName() const { return product; }

    // Lit la Maschine dans une tache a part et appelle `callback` pour chaque evenement.
    // Le rappel est appele depuis cette tache : il doit etre court (ex. mettre en file).
    bool start (Callback callback, std::string& error);
    void stop();

    Mk3Decoder& decoder() { return dec; }

    // Envoie un rapport brut (LED : 0x80 boutons, 0x81 pads). Utilise a l'etape 2.
    bool write (const uint8_t* data, int length);

private:
    void readLoop();

    libusb_context* context = nullptr;
    libusb_device_handle* handle = nullptr;
    unsigned char endpointIn = 0, endpointOut = 0;
    bool claimed = false, detached = false;
    std::string product;
    Mk3Decoder dec;
    Callback onEvent;
    std::thread reader;
    std::atomic<bool> running { false };
};

} // namespace fuzzy::control
