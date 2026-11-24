// fuzzy-control : les deux ecrans couleur 480 x 272 de la Maschine MK3.
//
// On dessine dans une image en memoire (Canvas), puis Mk3Display envoie a la Maschine
// SEULEMENT les lignes qui ont change depuis le dernier envoi (beaucoup plus rapide).
// Format envoye : RGB565 big-endian, interface USB 5, point de sortie bulk 0x04
// (protocole du pilote r22yb3m, MIT). Correction de couleur propre aux ecrans Maschine :
// gamma 1,6 sur le vert (case 3 de la mire, choisie le 18/11/2026).
#pragma once
#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace fuzzy::control {

class MaschineMk3;

struct Rgb
{
    uint8_t r = 0, g = 0, b = 0;
    bool operator== (const Rgb&) const = default;
};

inline constexpr Rgb kBlack {}, kWhite { 255, 255, 255 }, kFuzzyOrange { 252, 95, 0 };

// Couleurs d'ecran des groupes A-H par defaut (proches de leurs boutons, Maschine+)
inline constexpr std::array<Rgb, 8> kGroupScreenColors { {
    { 235, 35, 25 }, { 252, 95, 0 }, { 235, 200, 40 }, { 0, 200, 165 },
    { 25, 90, 240 }, { 105, 55, 240 }, { 230, 40, 225 }, { 240, 20, 70 } } };

class Canvas
{
public:
    static constexpr int kWidth = 480, kHeight = 272;

    Canvas();
    void fill (Rgb c);
    void fillRect (int x, int y, int w, int h, Rgb c);
    void frameRect (int x, int y, int w, int h, Rgb c, int thickness = 1);
    // Texte ASCII, police 8 x 16, agrandie `scale` fois. Renvoie la largeur dessinee.
    int text (int x, int y, std::string_view s, Rgb c, int scale = 1);
    static int textWidth (std::string_view s, int scale = 1) { return (int) s.size() * 8 * scale; }

    Rgb at (int x, int y) const { return px[(size_t) (y * kWidth + x)]; }
    const std::vector<Rgb>& pixels() const { return px; }

private:
    std::vector<Rgb> px;
};

struct Rect { int x = 0, y = 0, w = 0, h = 0; bool empty() const { return w <= 0 || h <= 0; } };

// Bande de lignes entieres qui contient tous les pixels differents entre a et b.
Rect changedArea (const Canvas& a, const Canvas& b);

// Paquet USB pour afficher `area` du canvas sur l'ecran `screen` (0 = gauche, 1 = droit).
std::vector<uint8_t> screenPacket (const Canvas& c, int screen, Rect area, bool colorCorrection = true);

uint16_t toRgb565 (Rgb c, bool colorCorrection = true);

class Mk3Display
{
public:
    Canvas& screen (int index) { return canvas[(size_t) index]; }

    // Envoie les zones modifiees des deux ecrans. Renvoie le nombre d'octets envoyes
    // (0 si rien n'avait change), -1 en cas d'erreur USB.
    long send (MaschineMk3& device);
    void forceFullRedraw() { firstFrame = true; }
    void setColorCorrection (bool on) { correction = on; firstFrame = true; }

private:
    std::array<Canvas, 2> canvas, sent;
    bool firstFrame = true, correction = true;
};

} // namespace fuzzy::control
