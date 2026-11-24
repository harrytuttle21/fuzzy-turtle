// fuzzy-control : lumieres de la Maschine MK3.
// Emplacements repris du pilote r22yb3m (MIT) et calibres sur une MK3 les 18-20/11/2026
// (recherche --scan-lights du 20/11 pour les boutons du haut, PITCH a NOTES, ERASE, etc.).
#include "fuzzy_control/lights.h"
#include "fuzzy_control/maschine_mk3.h"

namespace fuzzy::control {

namespace {
// screenButton = true : niveaux kScreenIdle / kScreenActive ; sinon kButtonDim / kButtonBright
constexpr LightSlot kSlots[] = {
    { "midi_channel", 0, false }, { "plug_in", 1, false }, { "arranger", 2, false },
    { "mixer", 3, false }, { "browser", 4, false }, { "sampling", 5, false },
    { "nav_prev", 6, false }, { "nav_next", 7, false }, { "file", 8, false },
    { "settings", 9, false }, { "auto", 10, false }, { "macro", 11, false },
    { "screen_1", 12, true }, { "screen_2", 13, true }, { "screen_3", 14, true },
    { "screen_4", 15, true }, { "screen_5", 16, true }, { "screen_6", 17, true },
    { "screen_7", 18, true }, { "screen_8", 19, true },
    { "volume", 20, false }, { "swing", 21, false }, { "note_repeat", 22, false },
    { "tempo", 23, false }, { "lock", 24, false },
    { "pitch", 25, false }, { "mod", 26, false }, { "perf_fx", 27, false }, { "notes", 28, false },
    { "restart", 37, false }, { "erase", 38, false }, { "tap", 39, false }, { "follow", 40, false },
    { "play", 41, false }, { "rec", 42, false }, { "stop", 43, false }, { "shift", 44, false },
    { "fixed_vel", 45, false },
    { "pad_mode", 46, false }, { "keyboard", 47, false }, { "chords", 48, false },
    { "step", 49, false }, { "scene", 50, false }, { "pattern", 51, false },
    { "events", 52, false }, { "variation", 53, false }, { "duplicate", 54, false },
    { "select", 55, false }, { "solo", 56, false }, { "mute", 57, false },
};
} // namespace

uint8_t paletteColor (int hue, Shade shade)
{
    if (shade == Shade::Off || hue < 0 || hue > kWhiteHue)
        return 0;
    return (uint8_t) (4 + 4 * hue + (int) shade);
}

std::span<const LightSlot> mk3LightSlots() { return kSlots; }

std::optional<int> lightSlot (std::string_view button)
{
    for (const auto& s : kSlots)
        if (s.name == button)
            return s.slot;
    return std::nullopt;
}

int padLightSlot (int pad) { return 37 - 4 * ((pad - 1) / 4) + ((pad - 1) % 4); }

Mk3Lights::Mk3Lights() : Mk3Lights (Calibration::mk3Defaults()) {}

Mk3Lights::Mk3Lights (const Calibration& c) : cal (c) { clear(); }

void Mk3Lights::applyCalibration (const Calibration& c)
{
    cal = c;
    clear();
}

void Mk3Lights::clear()
{
    pads.fill (0);
    buttons.fill (0);
    pads[0] = 0x81;
    buttons[0] = 0x80;
    padsDirty = buttonsDirty = true;
}

void Mk3Lights::setPad (int pad, uint8_t paletteIndex)
{
    if (pad < 1 || pad > 16)
        return;
    auto& v = pads[1 + (size_t) padLightSlot (pad)];
    if (v != paletteIndex)
    {
        v = paletteIndex;
        padsDirty = true;
    }
}

void Mk3Lights::setSlot (int slot, uint8_t value)
{
    if (slot < 0 || slot > 61)
        return;
    auto& v = buttons[1 + (size_t) slot];
    if (v != value)
    {
        v = value;
        buttonsDirty = true;
    }
}

void Mk3Lights::setGroup (int group, int hue, Shade shade)
{
    if (group >= 0 && group < 8)
        setSlot (cal.groupSlot + group, paletteColor (hue, shade));
}

bool Mk3Lights::setButton (std::string_view name, Shade level)
{
    const auto it = cal.lights.find (name);
    if (it == cal.lights.end())
        return false;
    const bool screen = it->second.screenButton;
    uint8_t v = 0;
    if (level == Shade::Dim)
        v = screen ? cal.screenIdle : cal.buttonDim;
    else if (level != Shade::Off)
        v = screen ? cal.screenActive : cal.buttonBright;
    setSlot (it->second.slot, v);
    return true;
}

int Mk3Lights::send (MaschineMk3& device)
{
    int sent = 0;
    if (padsDirty && device.write (pads.data(), (int) pads.size()))
    {
        padsDirty = false;
        ++sent;
    }
    if (buttonsDirty && device.write (buttons.data(), (int) buttons.size()))
    {
        buttonsDirty = false;
        ++sent;
    }
    return sent;
}

} // namespace fuzzy::control
