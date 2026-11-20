// fuzzy-control : decodage des rapports USB de la Maschine MK3 (sans materiel).
//
// Rapport 0x01 : boutons (bits), molette 4D (octet 11), encodeurs (octets 12-27).
// Rapport 0x02 : pads, par triplets (id, statut + poids fort, poids faible).
#pragma once
#include "events.h"

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace fuzzy::control {

struct ButtonDef
{
    const char* name;
    int bit;
};

// Boutons connus de la MK3 (valides sur le materiel les 18-20/11/2026).
// L'etape 4 les mettra dans un fichier de calibration.
std::span<const ButtonDef> mk3Buttons();

int padNumberForId (int padId);                  // id du rapport -> pad 1-16 (1 = en bas a gauche)
int velocityFromRaw (int raw, double exponent);  // pression 0-4095 -> velocite 1-127

class Mk3Decoder
{
public:
    static constexpr int kButtonBits = 72;

    explicit Mk3Decoder (double velocityExponent = 0.5);   // 0.5 = courbe "soft" validee

    // Decode un rapport et ajoute les evenements a la fin de `out`.
    void parse (std::span<const uint8_t> report, std::vector<Event>& out);

    void setVelocityExponent (double e) { exponent = e; }
    bool padDown (int pad) const { return pads[(size_t) pad]; }

private:
    void buttons (std::span<const uint8_t> r, std::vector<Event>& out);
    void knobs (std::span<const uint8_t> r, std::vector<Event>& out);
    void jog (std::span<const uint8_t> r, std::vector<Event>& out);
    void padReport (std::span<const uint8_t> r, std::vector<Event>& out);

    double exponent;
    std::bitset<kButtonBits> bits;                 // etat des boutons
    std::array<const char*, kButtonBits> names {}; // nom par bit (nullptr = inconnu)
    std::array<std::optional<int>, 8> knobValue {};
    std::optional<int> jogValue;
    std::array<bool, 17> pads {};                  // pad tenu (index 1-16)
};

} // namespace fuzzy::control
