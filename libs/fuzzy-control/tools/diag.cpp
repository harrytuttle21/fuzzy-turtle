// fuzzy-control-diag : diagnostic USB de la Maschine, SANS rien modifier.
// Ne detache aucun pilote : la Maschine reste utilisable par le logiciel NI pendant le test.
// A lancer deux fois : sans sudo, puis avec sudo, pour comparer.
#include "fuzzy_control/maschine_mk3.h"

#include <libusb.h>
#include <unistd.h>

#include <cstdio>

using fuzzy::control::MaschineMk3;

namespace {
const char* className (int c)
{
    switch (c)
    {
        case 0x01: return "audio";
        case 0x03: return "HID (pads, boutons, LED)";
        case 0xFE: return "application (mise a jour)";
        case 0xFF: return "constructeur (ecrans, ...)";
        default:   return "autre";
    }
}

const char* answer (int rc)
{
    switch (rc)
    {
        case 0: return "OUI";
        case 1: return "OUI";
        case LIBUSB_ERROR_ACCESS: return "NON (acces refuse : droits)";
        case LIBUSB_ERROR_BUSY: return "NON (occupe par un pilote)";
        case LIBUSB_ERROR_NOT_SUPPORTED: return "inconnu (non gere ici)";
        default: return libusb_error_name (rc);
    }
}
} // namespace

int main()
{
    std::printf ("Diagnostic Maschine (%s)\n\n", geteuid() == 0 ? "avec sudo" : "sans sudo");
    libusb_context* ctx = nullptr;
    if (libusb_init (&ctx) != 0)
    {
        std::puts ("libusb ne demarre pas");
        return 1;
    }
    libusb_device** list = nullptr;
    const ssize_t n = libusb_get_device_list (ctx, &list);
    libusb_device* dev = nullptr;
    uint16_t pid = 0;
    for (ssize_t i = 0; i < n && dev == nullptr; ++i)
    {
        libusb_device_descriptor d {};
        libusb_get_device_descriptor (list[i], &d);
        if (d.idVendor == MaschineMk3::kVendor
            && (d.idProduct == MaschineMk3::kProductMk3 || d.idProduct == MaschineMk3::kProductPlus))
        {
            dev = list[i];
            pid = d.idProduct;
        }
    }
    if (dev == nullptr)
    {
        std::puts ("Maschine introuvable (branchee et allumee ?)");
        return 1;
    }
    std::printf ("Appareil : %s (17cc:%04x), bus %d adresse %d\n", pid == MaschineMk3::kProductMk3 ? "Maschine MK3" : "Maschine+",
                 pid, libusb_get_bus_number (dev), libusb_get_device_address (dev));

    libusb_config_descriptor* cfg = nullptr;
    if (libusb_get_active_config_descriptor (dev, &cfg) == 0)
    {
        std::puts ("\nInterfaces :");
        for (int i = 0; i < cfg->bNumInterfaces; ++i)
        {
            const auto& alt = cfg->interface[i].altsetting[0];
            std::printf ("  %d : %-28s points :", alt.bInterfaceNumber, className (alt.bInterfaceClass));
            for (int e = 0; e < alt.bNumEndpoints; ++e)
                std::printf (" 0x%02x", alt.endpoint[e].bEndpointAddress);
            std::puts ("");
        }
    }

    libusb_device_handle* h = nullptr;
    const int rcOpen = libusb_open (dev, &h);
    std::printf ("\nOuvrir la Maschine : %s\n", answer (rcOpen));
    if (rcOpen == 0 && cfg != nullptr)
    {
        std::puts ("\nPour chaque interface : pilote du systeme attache ? / reservable SANS detacher ?");
        for (int i = 0; i < cfg->bNumInterfaces; ++i)
        {
            const int num = cfg->interface[i].altsetting[0].bInterfaceNumber;
            const int active = libusb_kernel_driver_active (h, num);
            const int claim = libusb_claim_interface (h, num);
            std::printf ("  %d : pilote attache : %-26s reservable : %s\n", num,
                         active == 1 ? "OUI" : active == 0 ? "non" : answer (active), answer (claim));
            if (claim == 0)
                libusb_release_interface (h, num);
        }
        libusb_close (h);
    }
    if (cfg != nullptr)
        libusb_free_config_descriptor (cfg);
    libusb_free_device_list (list, 1);
    libusb_exit (ctx);
    std::puts ("\nRien n'a ete modifie. Envoie tout ce texte (sans sudo ET avec sudo).");
    return 0;
}
