// Fuzzy John DAW Pulses — menus des gammes et des accords (en arborescence : categorie, puis
// gamme), avec les gammes et accords personnalises (New..., Edit, Delete). Licence : GPLv3.
#pragma once
#include "keyboard_view.h"

#include <functional>

class QMenu;
class QWidget;

namespace music {

// Gammes : Main ▸, Major ▸ ... (celles du pilote), puis Custom ▸ (les siennes, New Scale...)
void fillScaleMenu (QMenu* menu, int current, const std::function<void (int scale)>& pick, QWidget* dialogParent);

// Accords : Off, Harmonizer ▸, Chord Sets ▸, Custom ▸ (les siens, New Chord...)
void fillChordMenu (QMenu* menu, const KeySettings& current,
                    const std::function<void (int mode, int index)>& pick, QWidget* dialogParent);

// Editeurs (true = OK) : gamme sur 12 degres, accord sur deux octaves
bool editScale (QWidget* parent, Custom& scale);
bool editChord (QWidget* parent, Custom& chord);

} // namespace music
