// fuzzy-control : decodage des rapports USB de la Maschine MK3 (sans materiel).
//
// Rapport 0x01 : boutons (bits), molette 4D (octet 11), encodeurs (octets 12-27).
// Rapport 0x02 : pads, par triplets (id, statut + poids fort, poids faible).
#pragma once
#include "events.h"

#include <array>
#include <chrono>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace fuzzy::control {

struct ButtonDef
{
    const char* name;
    int bit;
};

struct Calibration;

// Boutons connus de la MK3 (valides sur le materiel les 18-20/11/2026) : valeurs par
// defaut ; un fichier de calibration peut les remplacer (calibration.h).
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

    // Remplace les noms des boutons et la courbe. A appeler avant MaschineMk3::start().
    void applyCalibration (const Calibration& calibration);

    void setVelocityExponent (double e) { exponent = e; }
    bool padDown (int pad) const { return pads[(size_t) pad]; }

    // Pad tenu dont la Maschine ne parle plus depuis `timeout` : son relachement s'est perdu
    // (jeu tres rapide). On le relache (PadOff, nom "missed"). Tant qu'un pad est vraiment
    // tenu, la Maschine renvoie sa pression en continu.
    void expirePads (std::chrono::steady_clock::time_point now, std::chrono::milliseconds timeout,
                     std::vector<Event>& out);
    int missedReleases() const { return missed; }
    // Plus grand ecart observe entre deux pressions d'un pad tenu (ms) : la garde doit etre
    // nettement plus longue, sinon elle couperait des notes vraiment tenues.
    int maxPadGapMs() const { return maxGapMs; }

private:
    void buttons (std::span<const uint8_t> r, std::vector<Event>& out);
    void knobs (std::span<const uint8_t> r, std::vector<Event>& out);
    void jog (std::span<const uint8_t> r, std::vector<Event>& out);
    void padReport (std::span<const uint8_t> r, std::vector<Event>& out);

    double exponent;
    std::bitset<kButtonBits> bits;                 // etat des boutons
    std::array<std::string, kButtonBits> names {}; // nom par bit (vide = inconnu)
    std::array<std::optional<int>, 8> knobValue {};
    std::optional<int> jogValue;
    std::array<bool, 17> pads {};                  // pad tenu (index 1-16)
    static constexpr int kLostGapMs = 400;           // silence plus long : nouvelle frappe (relachement perdu)
    std::array<std::chrono::steady_clock::time_point, 17> padSeen {};   // derniere pression recue
    int missed = 0, maxGapMs = 0;
};

} // namespace fuzzy::control
