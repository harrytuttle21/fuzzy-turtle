// Tests du decodage MK3, sans materiel : on fabrique des rapports et on verifie les evenements.
#include "fuzzy_control/decoder.h"
#include "fuzzy_control/lights.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace fuzzy::control;

namespace {
int failures = 0;

void check (bool ok, const char* what)
{
    std::printf ("%s  %s\n", ok ? "OK   " : "ECHEC", what);
    if (! ok)
        ++failures;
}

std::vector<uint8_t> buttonReport()
{
    std::vector<uint8_t> r (28, 0);
    r[0] = 0x01;
    for (int i = 0; i < 8; ++i)
    {
        r[12 + 2 * i] = 100;                    // encodeurs a 100
        r[13 + 2 * i] = 0;
    }
    return r;
}

void setBit (std::vector<uint8_t>& r, int bit, bool on)
{
    auto& byte = r[1 + (size_t) bit / 8];
    byte = on ? (uint8_t) (byte | (1 << (bit % 8))) : (uint8_t) (byte & ~(1 << (bit % 8)));
}

std::vector<uint8_t> padReport (int padId, int status, int raw)
{
    return { 0x02, (uint8_t) padId, (uint8_t) ((status << 4) | (raw >> 8)), (uint8_t) (raw & 0xFF),
             0, 0, 0 };
}
} // namespace

int main()
{
    Mk3Decoder dec;
    std::vector<Event> ev;

    // --- Pads ---
    check (padNumberForId (12) == 1 && padNumberForId (3) == 16, "numerotation des pads (1 en bas a gauche)");
    check (velocityFromRaw (4095, 0.5) == 127 && velocityFromRaw (1, 0.5) >= 1, "velocite bornee 1-127");
    dec.parse (padReport (12, 1, 2000), ev);
    check (ev.size() == 1 && ev[0].type == Event::Type::PadOn && ev[0].id == 1, "frappe du pad 1");
    ev.clear();
    dec.parse (padReport (12, 1, 1500), ev);
    check (ev.empty(), "pression continue : pas de nouvelle frappe");
    dec.parse (padReport (12, 2, 0), ev);
    check (ev.size() == 1 && ev[0].type == Event::Type::PadOff && ev[0].id == 1, "relache du pad 1");
    ev.clear();
    dec.parse (padReport (12, 1, 900), ev);
    check (ev.size() == 1 && ev[0].type == Event::Type::PadOn, "nouvelle frappe apres relache");
    ev.clear();
    dec.parse (padReport (12, 1, 0), ev);
    check (ev.size() == 1 && ev[0].type == Event::Type::PadOff, "pression nulle = relache");
    ev.clear();

    // --- Boutons, encodeurs, molette ---
    auto r = buttonReport();
    dec.parse (r, ev);                              // premier rapport : reference, rien a signaler
    check (ev.empty(), "premier rapport sans evenement");
    setBit (r, 45, true);                           // PLAY
    dec.parse (r, ev);
    check (ev.size() == 1 && ev[0].type == Event::Type::ButtonDown && std::string (ev[0].name) == "play",
           "bouton PLAY appuye");
    ev.clear();
    setBit (r, 45, false);
    setBit (r, 1, true);                            // bit 1 : pas encore identifie
    dec.parse (r, ev);
    check (ev.size() == 2 && ev[0].type == Event::Type::UnknownDown && ev[0].id == 1
           && ev[1].type == Event::Type::ButtonUp, "bouton inconnu signale + PLAY relache");
    ev.clear();
    setBit (r, 1, false);
    dec.parse (r, ev);
    ev.clear();
    r[12] = 105;                                    // encodeur 1 : +5
    r[14] = 98;                                     // encodeur 2 : -2
    dec.parse (r, ev);
    check (ev.size() == 2 && ev[0].type == Event::Type::Knob && ev[0].id == 1 && ev[0].value == 5
           && ev[1].id == 2 && ev[1].value == -2, "encodeurs : +5 et -2");
    ev.clear();
    r[12] = (uint8_t) (998 & 0xFF); r[13] = (uint8_t) (998 >> 8);   // 105 -> 998 : -107 (passage par 0)
    dec.parse (r, ev);
    check (ev.size() == 1 && ev[0].value == -107, "encodeur qui repasse par 0");
    ev.clear();
    r[11] = 0x0F;                                   // molette : premier rapport de reference (0 -> 15)
    dec.parse (r, ev);
    check (ev.size() == 1 && ev[0].type == Event::Type::Jog && ev[0].value == -1, "molette : 0 -> 15 = -1");
    ev.clear();
    r[11] = 0x01;                                   // 15 -> 1 : +2
    dec.parse (r, ev);
    check (ev.size() == 1 && ev[0].value == 2, "molette : 15 -> 1 = +2");
    ev.clear();

    // --- Lumieres ---
    check (paletteColor (1, Shade::Dim) == 9 && paletteColor (1, Shade::Bright) == 10
           && paletteColor (1, Shade::Off) == 0, "palette : groupe A rouge = 9 / 10");
    check (paletteColor (kWhiteHue, Shade::Dim) == 69, "palette : blanc doux = 69");
    check (padLightSlot (1) == 37 && padLightSlot (16) == 28, "emplacements des pads");
    Mk3Lights leds;
    leds.setPad (1, 30);
    leds.setGroup (0, 1, Shade::Bright);
    check (leds.setButton ("play", Shade::Bright) && leds.setButton ("screen_6", Shade::Dim)
           && ! leds.setButton ("inconnu", Shade::Dim), "boutons connus / inconnu");
    const auto pf = leds.padFrame();
    const auto bf = leds.buttonFrame();
    check (pf[0] == 0x81 && pf[38] == 30, "rapport 0x81 : pad 1");
    check (bf[0] == 0x80 && bf[42] == kButtonBright && bf[18] == kScreenIdle && bf[30] == 10,
           "rapport 0x80 : PLAY vif, bouton 6 doux, groupe A vif");
    leds.clear();
    check (leds.buttonFrame()[42] == 0 && leds.padFrame()[38] == 0, "tout eteindre");

    std::printf ("\n%s (%d echec%s)\n", failures ? "TESTS EN ECHEC" : "Tous les tests passent",
                 failures, failures > 1 ? "s" : "");
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
