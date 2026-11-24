// Fuzzy John DAW Pulses — fenetre Settings. Licence : GPLv3.
#include "settings_dialog.h"
#include "engine_link.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QSettings>
#include <QTabWidget>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog (EngineLink& e, QWidget* parent) : QDialog (parent), engine (e)
{
    setWindowTitle (tr ("Settings"));
    setMinimumSize (480, 300);
    tabs = new QTabWidget;

    // --- Audio
    auto* audioPage = new QWidget;
    auto* form = new QFormLayout (audioPage);
    output = new QComboBox;
    input = new QComboBox;
    rate = new QComboBox;
    buffer = new QComboBox;
    info = new QLabel;
    info->setWordWrap (true);
    info->setObjectName ("hint");
    form->addRow (tr ("Output"), output);
    input->setToolTip (tr ("Kept for sampling (coming later)"));
    form->addRow (tr ("Input"), input);
    form->addRow (tr ("Sample rate"), rate);
    form->addRow (tr ("Buffer"), buffer);
    form->addRow (info);
    tabs->addTab (audioPage, tr ("Audio"));

    // Pages prevues (comme SETTINGS sur la Maschine)
    for (const auto& name : { tr ("MIDI"), tr ("Colors"), tr ("Keys") })
    {
        auto* page = new QLabel (tr ("Coming next."));
        page->setAlignment (Qt::AlignCenter);
        tabs->addTab (page, name);
    }

    auto* buttons = new QDialogButtonBox (QDialogButtonBox::Close);
    connect (buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    auto* col = new QVBoxLayout (this);
    col->addWidget (tabs);
    col->addWidget (buttons);

    for (auto* box : { output, input, rate, buffer })
        connect (box, &QComboBox::activated, this, &SettingsDialog::apply);   // applique tout de suite
    connect (&engine, &EngineLink::stateChanged, this, &SettingsDialog::refresh);
    refresh();
}

void SettingsDialog::refresh()
{
    const AudioState& a = engine.state();
    const QSignalBlocker b1 (output), b2 (input), b3 (rate), b4 (buffer);
    output->clear();
    output->addItems (a.outputs);
    output->setCurrentText (a.device);
    input->clear();
    input->addItem (tr ("(none)"));
    input->addItems (a.inputs);
    input->setCurrentText (QSettings().value ("input").toString());
    rate->clear();
    for (int r : a.rates)
        rate->addItem (QString ("%1 Hz").arg (r), r);
    rate->setCurrentIndex (rate->findData ((int) a.rate));
    buffer->clear();
    for (int s : a.buffers)
        buffer->addItem (tr ("%1 samples (%2 ms)").arg (s).arg (a.rate > 0 ? s * 1000.0 / a.rate : 0, 0, 'f', 1), s);
    buffer->setCurrentIndex (buffer->findData (a.buffer));
    info->setText (a.valid()
        ? tr ("Total latency: %1 ms. Smaller buffer = less latency, but more risk of dropouts (%2 so far).")
              .arg (a.latencyMs, 0, 'f', 1).arg (a.xruns)
        : tr ("Audio engine not running."));
}

void SettingsDialog::apply()
{
    const QString in = input->currentIndex() > 0 ? input->currentText() : QString();
    engine.setAudio (output->currentText(), in, rate->currentData().toInt(), buffer->currentData().toInt());
    QSettings s;
    s.setValue ("output", output->currentText());
    s.setValue ("input", in);
}

void SettingsDialog::showPage (int index)
{
    tabs->setCurrentIndex (index);
}
