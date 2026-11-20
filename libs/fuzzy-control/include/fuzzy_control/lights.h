// fuzzy-control : lumieres de la Maschine MK3 (pads, boutons, groupes).
//
// Rapport 0x81 (42 octets) : couleurs des 16 pads, en indices de la palette NI.
// Rapport 0x80 (63 octets) : boutons (niveaux) et boutons de groupe (indices de palette).
// On ne renvoie un rapport a la Maschine que s'il a change.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace fuzzy::control {

class MaschineMk3;

// Palette NI : 16 teintes ; teinte n -> indices 4+4n (+1 doux, +2 vif, +3 pale).
enum class Shade : uint8_t { Off = 0, Dim = 1, Bright = 2, Pale = 3 };
uint8_t paletteColor (int hue, Shade shade);             // 0 si Off
inline constexpr int kWhiteHue = 16;                      // blanc (pas en lecture du STEP)

// Couleurs par defaut des groupes A-H, comme sur la Maschine+ (choisies au nuancier).
inline constexpr std::array<int, 8> kDefaultGroupHues { 1, 2, 5, 6, 9, 12, 13, 15 };

// Niveaux des boutons a une couleur, et des boutons au-dessus des ecrans.
inline constexpr uint8_t kButtonDim = 4, kButtonBright = 7;
inline constexpr uint8_t kScreenIdle = 12, kScreenActive = 50;

struct LightSlot { std::string_view name; int slot; bool screenButton; };
std::span<const LightSlot> mk3LightSlots();               // emplacements calibres
std::optional<int> lightSlot (std::string_view button);   // nom -> emplacement

class Mk3Lights
{
public:
    Mk3Lights();

    void setPad (int pad, uint8_t paletteIndex);          // pad 1-16
    void setPad (int pad, int hue, Shade shade) { setPad (pad, paletteColor (hue, shade)); }
    void setGroup (int group, int hue, Shade shade);      // group 0-7 (A-H)

    // Bouton a une couleur : Off / Dim (disponible) / Bright (actif). Renvoie false si
    // le nom n'a pas d'emplacement de lumiere connu.
    bool setButton (std::string_view name, Shade level);
    void setSlot (int slot, uint8_t value);               // acces brut (calibration)
    void clear();

    std::array<uint8_t, 42> padFrame() const { return pads; }
    std::array<uint8_t, 63> buttonFrame() const { return buttons; }

    // Envoie ce qui a change depuis le dernier envoi. Renvoie le nombre de rapports envoyes.
    int send (MaschineMk3& device);
    void forceResend() { padsDirty = buttonsDirty = true; }

private:
    std::array<uint8_t, 42> pads {};
    std::array<uint8_t, 63> buttons {};
    bool padsDirty = true, buttonsDirty = true;
};

int padLightSlot (int pad);   // pad 1-16 -> position dans le rapport 0x81

} // namespace fuzzy::control
