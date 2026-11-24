// Fuzzy John DAW Pulses — messages OSC minimaux (entiers, decimaux, textes).
// Licence : GPLv3.
#pragma once
#include <QByteArray>
#include <QString>
#include <QVariantList>

namespace fuzzy::osc {

// Construit un message : int -> 'i', double / float -> 'f', QString -> 's'.
QByteArray message (const QString& address, const QVariantList& args = {});

// Lit un message recu. Renvoie false s'il est mal forme.
bool parse (const QByteArray& data, QString& address, QVariantList& args);

} // namespace fuzzy::osc
