// fuzzy-control : connexion USB a la Maschine MK3 / Maschine+ (libusb).
#include "fuzzy_control/maschine_mk3.h"

#include <libusb.h>

#include <vector>

namespace fuzzy::control {

MaschineMk3::MaschineMk3() = default;

MaschineMk3::~MaschineMk3() { close(); }

bool MaschineMk3::open (std::string& error)
{
    if (handle != nullptr)
        return true;
    if (libusb_init (&context) != 0)
    {
        error = "libusb ne demarre pas";
        return false;
    }
    for (const uint16_t pid : { kProductMk3, kProductPlus })
        if ((handle = libusb_open_device_with_vid_pid (context, kVendor, pid)) != nullptr)
        {
            product = pid == kProductMk3 ? "Maschine MK3" : "Maschine+";
            break;
        }
    if (handle == nullptr)
    {
        error = "Maschine introuvable (branchee et allumee ?)";
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
            if (alt.bInterfaceNumber != kHidInterface)
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
        error = "interface 4 de la Maschine introuvable";
        close();
        return false;
    }

    int rc = libusb_claim_interface (handle, kHidInterface);
    if (rc != 0 && libusb_kernel_driver_active (handle, kHidInterface) == 1)
    {
        // Pilote du systeme (NI) en place : on le detache (macOS : droits administrateur)
        if (libusb_detach_kernel_driver (handle, kHidInterface) == 0)
        {
            detached = true;
            rc = libusb_claim_interface (handle, kHidInterface);
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
    if (handle != nullptr)
    {
        if (claimed)
            libusb_release_interface (handle, kHidInterface);
        if (detached)
            libusb_attach_kernel_driver (handle, kHidInterface);
        libusb_close (handle);
        handle = nullptr;
    }
    claimed = detached = false;
    endpointIn = endpointOut = 0;
    if (context != nullptr)
    {
        libusb_exit (context);
        context = nullptr;
    }
}

bool MaschineMk3::start (Callback callback, std::string& error)
{
    if (handle == nullptr)
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

bool MaschineMk3::write (const uint8_t* data, int length)
{
    if (handle == nullptr || length <= 0)
        return false;
    int done = 0;
    auto* bytes = const_cast<unsigned char*> (data);
    if (endpointOut)
        return libusb_interrupt_transfer (handle, endpointOut, bytes, length, &done, 100) == 0;
    // Sans point de sortie : HID SET_REPORT par le canal de controle
    return libusb_control_transfer (handle, 0x21, 0x09, (uint16_t) (0x0200 | data[0]),
                                    kHidInterface, bytes, (uint16_t) length, 100) == length;
}

} // namespace fuzzy::control
