// fuzzy-control : connexion USB a la Maschine MK3 / Maschine+ (hidapi + libusb).
#include "fuzzy_control/maschine_mk3.h"

#include <hidapi.h>
#if defined(__APPLE__) && __has_include(<hidapi_darwin.h>)
 #include <hidapi_darwin.h>
 #define FUZZY_HIDAPI_DARWIN 1
#endif
#include <libusb.h>

#include <vector>

namespace fuzzy::control {

MaschineMk3::MaschineMk3() = default;

MaschineMk3::~MaschineMk3() { close(); }

bool MaschineMk3::open (std::string& error, Access access)
{
    if (isOpen())
        return true;
    mode = access;
    if (access == Access::Exclusive)
        return openExclusive (error);

    if (hid_init() != 0)
    {
        error = "hidapi ne demarre pas";
        return false;
    }
#ifdef FUZZY_HIDAPI_DARWIN
    hid_darwin_set_open_exclusive (0);     // partage avec le systeme (et l'agent NI)
#endif
    for (const uint16_t pid : { kProductMk3, kProductPlus })
        if ((hid = hid_open (kVendor, pid, nullptr)) != nullptr)
        {
            product = pid == kProductMk3 ? "Maschine MK3" : "Maschine+";
            break;
        }
    if (hid == nullptr)
    {
        error = "Maschine introuvable ou deja prise (branchee ? un autre programme l'a-t-il ouverte ?)";
        return false;
    }
    return true;
}

bool MaschineMk3::openUsb (std::string& error)
{
    if (handle != nullptr)
        return true;
    if (context == nullptr && libusb_init (&context) != 0)
    {
        error = "libusb ne demarre pas";
        return false;
    }
    for (const uint16_t pid : { kProductMk3, kProductPlus })
        if ((handle = libusb_open_device_with_vid_pid (context, kVendor, pid)) != nullptr)
        {
            if (product.empty())
                product = pid == kProductMk3 ? "Maschine MK3" : "Maschine+";
            hidInterface = pid == kProductPlus ? kHidInterfacePlus : kHidInterface;
            displayInterface = pid == kProductPlus ? kDisplayInterfacePlus : kDisplayInterface;
            return true;
        }
    error = "Maschine introuvable (branchee et allumee ?)";
    return false;
}

bool MaschineMk3::openExclusive (std::string& error)
{
    if (! openUsb (error))
    {
        close();
        return false;
    }

    // Points d'entree / sortie de l'interface 4 (pads, boutons, LED)
    libusb_config_descriptor* cfg = nullptr;
    if (libusb_get_active_config_descriptor (libusb_get_device (handle), &cfg) == 0)
    {
        for (int i = 0; i < cfg->bNumInterfaces; ++i)
        {
            const auto& alt = cfg->interface[i].altsetting[0];
            if (alt.bInterfaceNumber != hidInterface)
                continue;
            for (int e = 0; e < alt.bNumEndpoints; ++e)
            {
                const unsigned char addr = alt.endpoint[e].bEndpointAddress;
                if (addr & LIBUSB_ENDPOINT_IN) { if (! endpointIn) endpointIn = addr; }
                else if (! endpointOut) endpointOut = addr;
            }
        }
        libusb_free_config_descriptor (cfg);
    }
    if (! endpointIn)
    {
        error = "interface HID de la Maschine introuvable";
        close();
        return false;
    }

    int rc = libusb_claim_interface (handle, hidInterface);
    if (rc != 0 && libusb_kernel_driver_active (handle, hidInterface) == 1)
    {
        // Pilote du systeme (NI) en place : on le detache (macOS : droits administrateur)
        if (libusb_detach_kernel_driver (handle, hidInterface) == 0)
        {
            detached = true;
            rc = libusb_claim_interface (handle, hidInterface);
        }
    }
    if (rc != 0)
    {
        error = std::string ("acces a la Maschine refuse (") + libusb_error_name (rc)
              + ") : lance avec sudo, et quitte les logiciels Native Instruments";
        close();
        return false;
    }
    claimed = true;
    return true;
}

void MaschineMk3::close()
{
    stop();
    if (hid != nullptr)
    {
        hid_close (hid);
        hid = nullptr;
        hid_exit();
    }
    if (handle != nullptr)
    {
        if (claimed)
            libusb_release_interface (handle, hidInterface);
        if (displayClaimed)
            libusb_release_interface (handle, displayInterface);
        if (displayDetached)
            libusb_attach_kernel_driver (handle, displayInterface);
        if (detached)
            libusb_attach_kernel_driver (handle, hidInterface);
        libusb_close (handle);
        handle = nullptr;
    }
    claimed = detached = displayClaimed = displayDetached = false;
    endpointIn = endpointOut = endpointDisplay = 0;
    if (context != nullptr)
    {
        libusb_exit (context);
        context = nullptr;
    }
}

bool MaschineMk3::start (Callback callback, std::string& error)
{
    if (! isOpen())
    {
        error = "Maschine pas ouverte";
        return false;
    }
    stop();
    onEvent = std::move (callback);
    running = true;
    reader = std::thread ([this] { readLoop(); });
    return true;
}

void MaschineMk3::stop()
{
    running = false;
    if (reader.joinable())
        reader.join();
}

void MaschineMk3::readLoop()
{
    unsigned char buf[64];
    std::vector<Event> events;
    events.reserve (32);
    while (running)
    {
        if (hid != nullptr)
        {
            // Lecture bloquante avec delai de 100 ms (a travers le pilote HID du systeme)
            const int got = hid_read_timeout (hid, buf, sizeof buf, 100);
            if (got < 0)                             // Maschine debranchee
                break;
            if (got == 0)
                continue;
            events.clear();
            dec.parse ({ buf, (size_t) got }, events);
            for (const auto& e : events)
                if (onEvent)
                    onEvent (e);
            continue;
        }
        int n = 0;
        // Lecture bloquante avec delai de 100 ms : pas d'attente active, arret rapide
        const int rc = libusb_interrupt_transfer (handle, endpointIn, buf, sizeof buf, &n, 100);
        if (rc == LIBUSB_ERROR_TIMEOUT || n <= 0)
            continue;
        if (rc != 0)
        {
            if (rc == LIBUSB_ERROR_NO_DEVICE)        // Maschine debranchee
                break;
            continue;
        }
        events.clear();
        dec.parse ({ buf, (size_t) n }, events);
        for (const auto& e : events)
            if (onEvent)
                onEvent (e);
    }
    running = false;
}

bool MaschineMk3::openDisplay (std::string& error)
{
    if (! isOpen())
    {
        error = "Maschine pas ouverte";
        return false;
    }
    if (displayClaimed)
        return true;
    if (! openUsb (error))
        return false;
    libusb_config_descriptor* cfg = nullptr;
    if (libusb_get_active_config_descriptor (libusb_get_device (handle), &cfg) == 0)
    {
        for (int i = 0; i < cfg->bNumInterfaces; ++i)
        {
            const auto& alt = cfg->interface[i].altsetting[0];
            if (alt.bInterfaceNumber != displayInterface)
                continue;
            for (int e = 0; e < alt.bNumEndpoints; ++e)
            {
                const auto& ep = alt.endpoint[e];
                if (! (ep.bEndpointAddress & LIBUSB_ENDPOINT_IN)
                    && (ep.bmAttributes & 0x03) == LIBUSB_TRANSFER_TYPE_BULK)
                    endpointDisplay = ep.bEndpointAddress;
            }
        }
        libusb_free_config_descriptor (cfg);
    }
    if (! endpointDisplay)
        endpointDisplay = 0x04;                       // valeur connue (MK3 et Maschine+)
    int rc = libusb_claim_interface (handle, displayInterface);
    // Mode exclusif seulement : detacher le pilote (sudo ; emporte toute la Maschine sur macOS)
    if (rc != 0 && mode == Access::Exclusive && libusb_detach_kernel_driver (handle, displayInterface) == 0)
    {
        displayDetached = true;
        rc = libusb_claim_interface (handle, displayInterface);
    }
    if (rc != 0)
    {
        error = std::string ("ecrans occupes (") + libusb_error_name (rc)
              + ") : sur macOS, l'agent Native Instruments les tient (NIHostIntegrationAgent)";
        return false;
    }
    displayClaimed = true;
    return true;
}

bool MaschineMk3::writeDisplay (const uint8_t* data, int length)
{
    if (! displayClaimed)
        return false;
    int done = 0;
    const int rc = libusb_bulk_transfer (handle, endpointDisplay, const_cast<unsigned char*> (data),
                                         length, &done, 2500);
    return rc == 0 && done == length;
}

bool MaschineMk3::write (const uint8_t* data, int length)
{
    if (length <= 0)
        return false;
    if (hid != nullptr)
        return hid_write (hid, data, (size_t) length) == length;
    if (handle == nullptr || ! claimed)
        return false;
    int done = 0;
    auto* bytes = const_cast<unsigned char*> (data);
    if (endpointOut)
        return libusb_interrupt_transfer (handle, endpointOut, bytes, length, &done, 100) == 0;
    // Sans point de sortie : HID SET_REPORT par le canal de controle
    return libusb_control_transfer (handle, 0x21, 0x09, (uint16_t) (0x0200 | data[0]),
                                    hidInterface, bytes, (uint16_t) length, 100) == length;
}

} // namespace fuzzy::control
