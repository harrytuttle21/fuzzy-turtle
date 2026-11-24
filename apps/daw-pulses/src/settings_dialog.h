// Fuzzy John DAW Pulses — fenetre Settings (pour commencer : Audio).
// Licence : GPLv3.
#pragma once
#include <QDialog>

class EngineLink;
class QComboBox;
class QLabel;
class QTabWidget;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog (EngineLink& engine, QWidget* parent = nullptr);
    void showPage (int index);                    // 0 Audio, 1 MIDI, 2 Colors, 3 Keys

private:
    void refresh();                               // listes et valeurs envoyees par le moteur
    void apply();

    EngineLink& engine;
    QComboBox* output = nullptr;
    QComboBox* input = nullptr;
    QComboBox* rate = nullptr;
    QComboBox* buffer = nullptr;
    QLabel* info = nullptr;
    QTabWidget* tabs = nullptr;
};
