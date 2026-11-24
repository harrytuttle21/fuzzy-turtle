// fuzzy-control : decodage des rapports de la Maschine MK3.
// Protocole repris du pilote "Maschine Zynthian Prototype" r22yb3m (licence MIT) et
// des calibrations faites sur une MK3 les 18-20/11/2026.
#include "fuzzy_control/decoder.h"
#include "fuzzy_control/calibration.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace fuzzy::control {

namespace {
constexpr ButtonDef kMk3Buttons[] = {
    { "nav_click", 0 }, { "nav_up", 2 }, { "nav_right", 3 }, { "nav_down", 4 }, { "nav_left", 5 },
    { "shift", 6 },
    { "group_a", 8 }, { "group_b", 9 }, { "group_c", 10 }, { "group_d", 11 },
    { "group_e", 12 }, { "group_f", 13 }, { "group_g", 14 }, { "group_h", 15 },
    { "volume", 17 }, { "tempo", 19 }, { "note_repeat", 20 }, { "lock", 21 },
    { "pad_mode", 24 }, { "keyboard", 25 }, { "chords", 26 }, { "step", 27 },
    { "scene", 29 }, { "pattern", 30 },
    { "solo", 36 }, { "mute", 37 },
    { "restart", 41 }, { "tap", 43 }, { "follow", 44 }, { "play", 45 }, { "rec", 46 }, { "stop", 47 },
    { "nav_next", 50 }, { "mixer", 52 }, { "plug_in", 53 }, { "midi_channel", 56 },
    { "browser", 58 }, { "nav_prev", 59 },
    // nommes le 20/11/2026
    { "swing", 18 }, { "macro", 48 }, { "settings", 49 }, { "sampling", 51 }, { "arranger", 57 },
    { "file", 60 }, { "auto", 61 }, { "pitch", 38 }, { "mod", 39 },
    { "notes", 16 }, { "variation", 33 }, { "duplicate", 34 }, { "select", 35 }, { "perf_fx", 40 },
    { "events", 31 }, { "erase", 42 }, { "fixed_vel", 28 },
    { "screen_1", 64 }, { "screen_2", 65 }, { "screen_3", 66 }, { "screen_4", 67 },
    { "screen_5", 68 }, { "screen_6", 69 }, { "screen_7", 70 }, { "screen_8", 7 }, { "screen_8", 71 },   // bouton 8 : bit 7 (20/11) ; 71 garde au cas ou
};
} // namespace

std::span<const ButtonDef> mk3Buttons() { return kMk3Buttons; }

int padNumberForId (int padId) { return (3 - padId / 4) * 4 + padId % 4 + 1; }

int velocityFromRaw (int raw, double exponent)
{
    const double x = std::min (raw, 4095) / 4095.0;
    return std::clamp ((int) std::lround (127.0 * std::pow (x, exponent)), 1, 127);
}

std::string describe (const Event& e)
{
    char buf[96];
    switch (e.type)
    {
        case Event::Type::PadOn:       std::snprintf (buf, sizeof buf, "PAD %2d ON  velocite %3d", e.id, e.value); break;
        case Event::Type::PadOff:      std::snprintf (buf, sizeof buf, "PAD %2d OFF", e.id); break;
        case Event::Type::ButtonDown:  std::snprintf (buf, sizeof buf, "BOUTON %s appuye", e.name); break;
        case Event::Type::ButtonUp:    std::snprintf (buf, sizeof buf, "BOUTON %s relache", e.name); break;
        case Event::Type::UnknownDown: std::snprintf (buf, sizeof buf, "BOUTON INCONNU bit %d appuye", e.id); break;
        case Event::Type::UnknownUp:   std::snprintf (buf, sizeof buf, "BOUTON INCONNU bit %d relache", e.id); break;
        case Event::Type::Knob:        std::snprintf (buf, sizeof buf, "ENCODEUR %d %+d", e.id, e.value); break;
        case Event::Type::Jog:         std::snprintf (buf, sizeof buf, "MOLETTE 4D %+d", e.value); break;
    }
    return buf;
}

Mk3Decoder::Mk3Decoder (double velocityExponent) : exponent (velocityExponent)
{
    for (const auto& b : kMk3Buttons)
        names[(size_t) b.bit] = b.name;
}

void Mk3Decoder::applyCalibration (const Calibration& c)
{
    for (auto& n : names)
        n.clear();
    for (const auto& [name, list] : c.buttons)
        for (const int bit : list)
            if (bit >= 0 && bit < kButtonBits)
                names[(size_t) bit] = name;
    exponent = c.velocityExponent;
}

void Mk3Decoder::parse (std::span<const uint8_t> r, std::vector<Event>& out)
{
    if (r.empty())
        return;
    if (r[0] == 0x01 && r.size() >= 7)
    {
        buttons (r, out);
        knobs (r, out);
        jog (r, out);
    }
    else if (r[0] == 0x02 && r.size() >= 4)
        padReport (r, out);
}

void Mk3Decoder::buttons (std::span<const uint8_t> r, std::vector<Event>& out)
{
    for (int bit = 0; bit < kButtonBits; ++bit)
    {
        const size_t idx = 1 + (size_t) bit / 8;
        if (idx >= r.size())
            break;
        const bool pressed = (r[idx] >> (bit % 8)) & 1;
        if (pressed == bits[(size_t) bit])
            continue;
        bits[(size_t) bit] = pressed;
        const std::string& name = names[(size_t) bit];
        if (! name.empty())
            out.push_back ({ pressed ? Event::Type::ButtonDown : Event::Type::ButtonUp, bit, 0, name.c_str() });
        else
            out.push_back ({ pressed ? Event::Type::UnknownDown : Event::Type::UnknownUp, bit, 0, "" });
    }
}

void Mk3Decoder::knobs (std::span<const uint8_t> r, std::vector<Event>& out)
{
    if (r.size() < 28)
        return;
    for (size_t i = 0; i < 8; ++i)
    {
        const int v = r[12 + 2 * i] | (r[13 + 2 * i] << 8);   // 16 bits, petit-boutiste, 0-999
        if (v > 999)
            continue;
        const auto before = knobValue[i];
        knobValue[i] = v;
        if (! before)
            continue;
        int d = ((v - *before) % 1000 + 1000) % 1000;
        if (d > 500)
            d -= 1000;
        if (d != 0 && std::abs (d) <= 200)                    // au-dela : saut aberrant ignore
            out.push_back ({ Event::Type::Knob, (int) i + 1, d, "" });
    }
}

void Mk3Decoder::jog (std::span<const uint8_t> r, std::vector<Event>& out)
{
    if (r.size() < 12)
        return;
    const int v = r[11] & 0x0F;                                // compteur 4 bits
    const auto before = jogValue;
    jogValue = v;
    if (! before)
        return;
    const int d = ((v - *before + 8) % 16 + 16) % 16 - 8;
    if (d != 0 && d >= -3 && d <= 3)
        out.push_back ({ Event::Type::Jog, 0, d, "" });
}

void Mk3Decoder::padReport (std::span<const uint8_t> r, std::vector<Event>& out)
{
    for (size_t pos = 1; pos + 2 < r.size(); pos += 3)
    {
        const int id = r[pos], hi = r[pos + 1], lo = r[pos + 2];
        if (id >= 16 || (id == 0 && hi == 0 && lo == 0))
            continue;
        const int status = hi >> 4, raw = ((hi & 0x0F) << 8) | lo;
        const int pad = padNumberForId (id);
        if ((status == 0 || status == 1) && raw > 0)
        {
            const auto now = std::chrono::steady_clock::now();
            if (pads[(size_t) pad])                            // pad deja tenu : ecart mesure
                maxGapMs = std::max (maxGapMs, (int) std::chrono::duration_cast<std::chrono::milliseconds> (
                                                   now - padSeen[(size_t) pad]).count());
            padSeen[(size_t) pad] = now;
            if (! pads[(size_t) pad])                          // pression continue : une seule frappe
            {
                pads[(size_t) pad] = true;
                out.push_back ({ Event::Type::PadOn, pad, velocityFromRaw (raw, exponent), "" });
            }
        }
        else if ((status == 2 || status == 3 || raw == 0) && pads[(size_t) pad])
        {
            pads[(size_t) pad] = false;
            out.push_back ({ Event::Type::PadOff, pad, 0, "" });
        }
    }
}

void Mk3Decoder::expirePads (std::chrono::steady_clock::time_point now, std::chrono::milliseconds timeout,
                             std::vector<Event>& out)
{
    for (int pad = 1; pad <= 16; ++pad)
        if (pads[(size_t) pad] && now - padSeen[(size_t) pad] > timeout)
        {
            pads[(size_t) pad] = false;
            ++missed;
            out.push_back ({ Event::Type::PadOff, pad, 0, "missed" });
        }
}

} // namespace fuzzy::control
