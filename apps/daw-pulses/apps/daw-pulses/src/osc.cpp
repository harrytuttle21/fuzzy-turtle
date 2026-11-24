// Fuzzy John DAW Pulses — messages OSC minimaux. Licence : GPLv3.
#include "osc.h"

#include <QtEndian>
#include <cstring>

namespace fuzzy::osc {

namespace {

void addString (QByteArray& out, const QByteArray& s)
{
    out.append (s);
    out.append ('\0');
    while (out.size() % 4 != 0)
        out.append ('\0');
}

void addInt (QByteArray& out, qint32 v)
{
    char b[4];
    qToBigEndian (v, b);
    out.append (b, 4);
}

bool readString (const QByteArray& data, int& pos, QString& s)
{
    const int end = data.indexOf ('\0', pos);
    if (end < 0)
        return false;
    s = QString::fromUtf8 (data.constData() + pos, end - pos);
    pos = (end + 4) & ~3;                         // aligne sur 4 octets
    return pos <= data.size();
}

} // namespace

QByteArray message (const QString& address, const QVariantList& args)
{
    QByteArray tags (",");
    QByteArray body;
    for (const auto& a : args)
    {
        switch (a.typeId())
        {
            case QMetaType::Double:
            case QMetaType::Float:
            {
                tags.append ('f');
                const float f = a.toFloat();
                quint32 bits;
                std::memcpy (&bits, &f, 4);
                addInt (body, (qint32) bits);
                break;
            }
            case QMetaType::QString:
                tags.append ('s');
                addString (body, a.toString().toUtf8());
                break;
            default:
                tags.append ('i');
                addInt (body, a.toInt());
                break;
        }
    }
    QByteArray out;
    addString (out, address.toUtf8());
    addString (out, tags);
    out.append (body);
    return out;
}

bool parse (const QByteArray& data, QString& address, QVariantList& args)
{
    int pos = 0;
    QString tags;
    if (! readString (data, pos, address) || ! readString (data, pos, tags) || ! tags.startsWith (','))
        return false;
    args.clear();
    for (int i = 1; i < tags.size(); ++i)
    {
        const QChar t = tags[i];
        if (t == 'i' || t == 'f')
        {
            if (pos + 4 > data.size())
                return false;
            const qint32 v = qFromBigEndian<qint32> (data.constData() + pos);
            pos += 4;
            if (t == 'i')
                args.append (v);
            else
            {
                float f;
                std::memcpy (&f, &v, 4);
                args.append ((double) f);
            }
        }
        else if (t == 's')
        {
            QString s;
            if (! readString (data, pos, s))
                return false;
            args.append (s);
        }
        else
            return false;                             // type inconnu : on s'arrete
    }
    return true;
}

} // namespace fuzzy::osc
