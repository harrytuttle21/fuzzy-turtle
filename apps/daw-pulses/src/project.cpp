// Fuzzy John DAW Pulses — projets .fuzzy. Licence : GPLv3.
#include "project.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kFormatVersion = 1;                     // PROJECT_VERSION du prototype

// Cles des groupes dans le fichier : A-H puis A2-H2 (comme GROUPS du prototype)
QString key (int g)
{
    return g < 8 ? QString (QChar ('A' + g)) : QString (QChar ('A' + g - 8)) + "2";
}

// Case du fichier : [ligne, piste, note ou [notes], velocite, duree?, decalage?]
QJsonArray cellToJson (int row, int track, const seq::Cell& c)
{
    QJsonArray cell { row, track };
    if (c.chordSize() > 1)
    {
        QJsonArray notes { c.note };
        for (int n : c.extra)
            if (n >= 0)
                notes.append (n);
        cell.append (notes);
    }
    else
        cell.append (c.note);
    cell.append (c.velocity);
    if (c.offset != 0)                                  // make_cell du prototype
    {
        cell.append (std::max (1, c.length));
        cell.append (c.offset);
    }
    else if (c.length > 1)
        cell.append (c.length);
    return cell;
}

bool cellFromJson (const QJsonArray& a, int& row, int& track, seq::Cell& c)
{
    if (a.size() < 4)
        return false;
    row = a[0].toInt (-1);
    track = a[1].toInt (-1);
    if (row < 0 || row >= seq::kMaxRows || track < 0 || track >= seq::kTracks)
        return false;
    c = {};
    if (a[2].isArray())                                 // accord
    {
        const QJsonArray notes = a[2].toArray();
        if (notes.isEmpty())
            return false;
        c.note = notes[0].toInt();
        for (int i = 1; i < notes.size() && i <= seq::kChordExtra; ++i)
            c.extra[(size_t) i - 1] = notes[i].toInt();
    }
    else
        c.note = a[2].toInt (-1);
    c.velocity = std::clamp (a[3].toInt (100), 1, 127);
    c.length = a.size() > 4 ? std::clamp (a[4].toInt (1), 1, seq::kMaxRows) : 1;
    c.offset = a.size() > 5 ? std::clamp (a[5].toInt (0), -seq::kTiming / 2, seq::kTiming / 2) : 0;
    return c.note >= 0 && c.note < 128;
}

// Le fichier complet, avec l'etat actuel de l'app par-dessus ce qui a ete lu
QJsonObject build (QJsonObject d, const QString& name, const seq::Sequencer& sequencer,
                   const session::State& state, int bpm)
{
    d["format"] = "fuzzy-proto";
    d["version"] = kFormatVersion;
    d["name"] = name;
    d["bpm"] = bpm;
    d["master"] = state.masterVolume.load();
    QJsonObject groups = d["groups"].toObject();
    for (int g = 0; g < session::kGroups; ++g)
    {
        QJsonObject gd = groups[key (g)].toObject();
        const int current = gd["current"].toInt (0);
        gd["current"] = current;
        gd["volume"] = state.groupVolume[(size_t) g].load();
        gd["off"] = sequencer.groupOff (g);
        if (! gd.contains ("pan"))
            gd["pan"] = 64;
        if (! gd.contains ("name"))
            gd["name"] = "";

        const seq::Pattern p = sequencer.pattern (g);
        QJsonArray cells;
        for (int r = 0; r < seq::kMaxRows; ++r)
            for (int t = 0; t < seq::kTracks; ++t)
                if (! p.rows[(size_t) r][(size_t) t].empty())
                    cells.append (cellToJson (r, t, p.rows[(size_t) r][(size_t) t]));

        QJsonArray patterns = gd["patterns"].toArray();
        bool found = false;
        for (int i = 0; i < patterns.size(); ++i)
        {
            QJsonObject pd = patterns[i].toObject();
            if (pd["index"].toInt (-1) != current)
                continue;
            pd["length"] = p.length;
            pd["cells"] = cells;
            patterns[i] = pd;
            found = true;
        }
        if (! found && (! cells.isEmpty() || p.length != 16))
            patterns.append (QJsonObject { { "index", current }, { "name", "" },
                                           { "length", p.length }, { "cells", cells } });
        gd["patterns"] = patterns;

        // Samples des sons (le reste des reglages de chaque son est garde tel quel)
        QJsonObject sounds = gd["sounds"].toObject();
        for (int s = 1; s <= 16; ++s)
        {
            const size_t i = (size_t) (g * 16 + s - 1);
            const QString path = state.samples[i];
            QJsonObject sd = sounds[QString::number (s)].toObject();
            if (! path.isEmpty())
                sd["sample"] = path;
            else
                sd.remove ("sample");
            // Lecture : enveloppe du prototype (ONE-SHOT / ADSR) + boucle (cle « loop »)
            const int mode = state.playMode[i];
            if (sd.contains ("envelope") || mode != session::State::defaultMode (s) || state.releaseMs[i] != 150)
            {
                QJsonObject env = sd["envelope"].toObject();
                env["mode"] = mode == session::State::OneShot ? "ONE-SHOT" : "ADSR";
                env["release"] = state.releaseMs[i].load();
                sd["envelope"] = env;
            }
            // Synthe comme le pilote : nom, « None » = son vide, sans cle = celui du pad
            static const QStringList synthNames { "Kick", "Snare", "Closed HH", "Open HH", "Tone" };
            const int syn = state.synth[i];
            if (syn < 0)
                sd.remove ("synth");
            else
                sd["synth"] = syn == 0 ? QString ("None") : synthNames.value (syn - 1, "Tone");
            if (mode == session::State::Loop || sd.contains ("loop"))
            {
                auto round4 = [] (float v) { return std::round (v * 10000.0) / 10000.0; };
                sd["loop"] = QJsonObject { { "on", mode == session::State::Loop },
                                           { "start", round4 (state.loopStart[i]) },
                                           { "end", round4 (state.loopEnd[i]) } };
            }
            if (! sd.isEmpty() || sounds.contains (QString::number (s)))
                sounds[QString::number (s)] = sd;
        }
        if (! sounds.isEmpty())
            gd["sounds"] = sounds;
        groups[key (g)] = gd;
    }
    d["groups"] = groups;
    return d;
}

} // namespace

void Project::reset()
{
    name = "UNTITLED";
    file.clear();
    data = {};
    savedText.clear();
}

bool Project::load (const QString& path, QString& error)
{
    QFile f (path);
    if (! f.open (QIODevice::ReadOnly))
    {
        error = QCoreApplication::translate ("Project", "Cannot open %1").arg (path);
        return false;
    }
    QJsonParseError parse;
    const QJsonDocument doc = QJsonDocument::fromJson (f.readAll(), &parse);
    if (! doc.isObject())
    {
        error = QCoreApplication::translate ("Project", "Not a Fuzzy project: %1").arg (parse.errorString());
        return false;
    }
    data = doc.object();
    // Nom : celui du fichier, sinon celui du dossier NOM.fuzzy
    const QString folder = QFileInfo (QFileInfo (path).absolutePath()).completeBaseName();
    name = cleanName (data["name"].toString (folder));
    file = QFileInfo (path).absoluteFilePath();
    return true;
}

void Project::toApp (seq::Sequencer& sequencer, session::State& state, int& bpm) const
{
    bpm = std::clamp (data["bpm"].toInt (120), 30, 300);
    state.masterVolume = std::clamp (data["master"].toInt (100), 0, 127);
    const QJsonObject groups = data["groups"].toObject();
    for (int g = 0; g < session::kGroups; ++g)
    {
        const QJsonObject gd = groups[key (g)].toObject();
        state.groupVolume[(size_t) g] = std::clamp (gd["volume"].toInt (100), 0, 127);
        const QJsonObject sounds = gd["sounds"].toObject();
        for (int s = 1; s <= 16; ++s)
        {
            const size_t i = (size_t) (g * 16 + s - 1);
            const QJsonObject sd = sounds[QString::number (s)].toObject();
            state.samples[i] = sd["sample"].toString();
            static const QStringList synthNames { "Kick", "Snare", "Closed HH", "Open HH", "Tone" };
            const QString syn = sd["synth"].toString();
            state.synth[i] = syn == "None" ? 0 : synthNames.contains (syn) ? (int) synthNames.indexOf (syn) + 1 : -1;
            const QJsonObject env = sd["envelope"].toObject();
            const QJsonObject loop = sd["loop"].toObject();
            state.playMode[i] = ! env.contains ("mode") ? session::State::defaultMode (s)
                              : env["mode"].toString() == "ONE-SHOT" ? session::State::OneShot
                              : loop["on"].toBool() ? session::State::Loop : session::State::Gate;
            state.releaseMs[i] = std::clamp (env["release"].toInt (150), 5, 5000);
            state.loopStart[i] = (float) std::clamp (loop["start"].toDouble (0), 0.0, 1.0);
            state.loopEnd[i] = (float) std::clamp (loop["end"].toDouble (1), 0.0, 1.0);
        }
        sequencer.clear (g);
        sequencer.setPatternLength (g, 16);
        sequencer.setGroupOff (g, gd["off"].toBool (false));
        const int current = gd["current"].toInt (0);
        for (const auto& pv : gd["patterns"].toArray())
        {
            const QJsonObject pd = pv.toObject();
            if (pd["index"].toInt (-1) != current)
                continue;
            sequencer.setPatternLength (g, pd["length"].toInt (16));
            for (const auto& cv : pd["cells"].toArray())
            {
                int row = 0, track = 0;
                seq::Cell c;
                if (cellFromJson (cv.toArray(), row, track, c))
                    sequencer.setCell (g, row, track, c);
            }
        }
    }
}

void Project::fromApp (const seq::Sequencer& sequencer, const session::State& state, int bpm)
{
    data = build (data, name, sequencer, state, bpm);
}

QByteArray Project::text() const
{
    return QJsonDocument (data).toJson (QJsonDocument::Indented);
}

bool Project::modified (const seq::Sequencer& sequencer, const session::State& state, int bpm) const
{
    return QJsonDocument (build (data, name, sequencer, state, bpm)).toJson (QJsonDocument::Indented) != savedText;
}

bool Project::save (const QString& projectsDir, QString& error)
{
    const QString folder = folderFor (projectsDir, name);
    if (! QDir().mkpath (folder))
    {
        error = QCoreApplication::translate ("Project", "Cannot create folder %1").arg (folder);
        return false;
    }
    const QString path = folder + "/project.json";
    if (QFile::exists (path))                           // l'ancienne version devient project.bak.json
    {
        QFile::remove (folder + "/project.bak.json");
        QFile::copy (path, folder + "/project.bak.json");
    }
    QSaveFile out (path);                               // ecriture complete ou rien
    if (! out.open (QIODevice::WriteOnly) || out.write (text()) < 0 || ! out.commit())
    {
        error = QCoreApplication::translate ("Project", "Cannot write %1").arg (path);
        return false;
    }
    file = path;
    markSaved();
    return true;
}

QString Project::cleanName (const QString& raw)
{
    QString out;
    for (const QChar c : raw.trimmed().toUpper())
    {
        if (out.size() >= kNameLength)
            break;
        out += (c.isLetterOrNumber() && c.unicode() < 128) || c == '-' || c == '_' ? c : QChar ('_');
    }
    return out.isEmpty() ? QString ("UNTITLED") : out;
}

QString Project::folderFor (const QString& projectsDir, const QString& name)
{
    return QDir (projectsDir).filePath (cleanName (name) + ".fuzzy");
}

QString Project::defaultDir()
{
    // Les projets du prototype (prototypes/maschine-hid/projects), en remontant jusqu'au depot
    QDir dir (QCoreApplication::applicationDirPath());
    for (int i = 0; i < 10; ++i)
    {
        if (dir.exists ("prototypes/maschine-hid"))
            return dir.filePath ("prototypes/maschine-hid/projects");
        if (! dir.cdUp())
            break;
    }
    return QStandardPaths::writableLocation (QStandardPaths::DocumentsLocation) + "/Fuzzy Projects";
}
