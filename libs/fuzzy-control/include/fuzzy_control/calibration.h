// fuzzy-control : calibration d'un controleur, rangee dans un fichier texte.
//
// Les numeros des boutons (bits) et des lumieres (emplacements) ne sont plus fixes dans
// le code : on peut les corriger dans un fichier, sans recompiler. Exemple :
//
//   [controleur]
//   modele = Maschine MK3
//   courbe_velocite = 0.5
//   [boutons]            # nom = bit dans le rapport 0x01
//   play = 45
//   nav_next = 50 49     # plusieurs bits possibles pour un meme bouton
//   [lumieres]           # nom = emplacement dans le rapport 0x80 (+ "ecran" si bouton d'ecran)
//   play = 41
//   screen_3 = 14 ecran
//
// Une ligne absente garde la valeur par defaut ; "nom = -" desactive le bouton ou la lumiere.
#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace fuzzy::control {

struct Calibration
{
    struct Light
    {
        int slot = 0;
        bool screenButton = false;   // niveaux ecran (12 / 50) au lieu de 4 / 7
    };

    std::string model = "Maschine MK3";
    double velocityExponent = 0.5;
    std::map<std::string, std::vector<int>, std::less<>> buttons;  // nom -> bit(s) 0-71
    std::map<std::string, Light, std::less<>> lights;       // nom -> emplacement (0-61)
    int groupSlot = 29;                                     // groupes A-H : 8 emplacements
    std::array<int, 8> groupHues { 1, 2, 5, 6, 9, 12, 13, 15 };
    uint8_t buttonDim = 4, buttonBright = 7, screenIdle = 12, screenActive = 50;

    static Calibration mk3Defaults();     // valide sur MK3 les 18-20/11/2026
    static Calibration plusDefaults();    // Maschine+ : valeurs MK3, a verifier
    static Calibration defaultsFor (const std::string& productName);

    // Lit un fichier et remplace les valeurs qu'il contient. Les lignes incomprises sont
    // ignorees et expliquees dans `warnings`. Renvoie false si le fichier ne s'ouvre pas.
    bool load (const std::string& path, std::vector<std::string>& warnings);
    bool save (const std::string& path, std::string& error) const;
};

// ~/.config/fuzzy-turtle/maschine-mk3.conf (ou maschine-plus.conf). Sous sudo, c'est le
// dossier de l'utilisateur qui a lance sudo, pas celui de root.
std::string defaultCalibrationPath (const std::string& productName);

} // namespace fuzzy::control
