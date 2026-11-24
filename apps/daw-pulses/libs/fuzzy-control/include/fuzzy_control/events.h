// fuzzy-control : evenements envoyes par un controleur.
#pragma once
#include <cstdint>
#include <string>

namespace fuzzy::control {

struct Event
{
    enum class Type : uint8_t
    {
        PadOn,          // id = pad 1-16, value = velocite 1-127
        PadOff,         // id = pad 1-16
        ButtonDown,     // id = numero de bit, name = nom du bouton
        ButtonUp,
        UnknownDown,    // bouton pas encore identifie : id = numero de bit (calibration)
        UnknownUp,
        Knob,           // id = encodeur 1-8, value = deplacement (+ droite, - gauche)
        Jog             // grosse molette 4D, value = crans (+ / -)
    };

    Type type;
    int id = 0;
    int value = 0;
    const char* name = "";   // nom du bouton (ex. "play"), "" sinon
};

std::string describe (const Event& e);   // texte lisible, pour les journaux et les tests

} // namespace fuzzy::control
