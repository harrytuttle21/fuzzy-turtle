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
        gd["current"] = sequencer.currentPattern (g);
        gd["volume"] = state.groupVolume[(size_t) g].load();
        gd["off"] = sequencer.groupOff (g);
        gd["name"] = state.groupLabel[(size_t) g];     // meme cle que la Maschine
        if (! gd.contains ("pan"))
            gd["pan"] = 64;
        if (! gd.contains ("name"))
            gd["name"] = "";

        // Tous les patterns du groupe (emplacement, nom, longueur, notes)
        QJsonArray patterns;
        for (int idx = 0; idx < seq::kPatternSlots; ++idx)
        {
            if (! sequencer.hasPattern (g, idx))
                continue;
            const seq::Pattern p = sequencer.patternAt (g, idx);
            QJsonArray cells;
            for (int r = 0; r < seq::kMaxRows; ++r)
                for (int t = 0; t < seq::kTracks; ++t)
                    if (! p.rows[(size_t) r][(size_t) t].empty())
                        cells.append (cellToJson (r, t, p.rows[(size_t) r][(size_t) t]));
            // pattern 1 vide et de 16 lignes : rien a ecrire (comme avant)
            if (idx == 0 && cells.isEmpty() && p.length == 16 && sequencer.patternName (g, idx).empty()
                && sequencer.currentPattern (g) == 0)
                continue;
            patterns.append (QJsonObject { { "index", idx }, { "name", QString::fromStdString (sequencer.patternName (g, idx)) },
                                           { "length", p.length }, { "cells", cells } });
        }
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
            // Volume et reglages (Volume, Pan, Pitch, Cutoff), comme le pilote
            const QJsonArray params { state.soundVolume[i].load(), state.soundPan[i].load(),
                                      state.soundPitch[i].load(), state.soundCutoff[i].load() };
            if (sd.contains ("params") || state.soundVolume[i] != 100 || state.soundPan[i] != 64
                || state.soundPitch[i] != 64 || state.soundCutoff[i] != 90)
            {
                sd["volume"] = state.soundVolume[i].load();
                sd["params"] = params;
            }
            if (state.soundReverse[i])                    // sample a l'envers
                sd["reverse"] = true;
            else
                sd.remove ("reverse");
            if (state.poly (g, s) != 4)                   // polyphonie (4 par defaut : pas ecrite)
                sd["polyphony"] = state.poly (g, s);
            else
                sd.remove ("polyphony");
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
    // Scenes : le pattern de chaque groupe (null = groupe libre, -1 = silence), comme le pilote
    QJsonArray scenes;
    for (int i = 0; i < seq::kPatternSlots; ++i)
    {
        const seq::Scene sc = sequencer.scene (i);
        if (! sc.used)
            continue;
        QJsonObject combo;
        for (int g = 0; g < session::kGroups; ++g)
            combo[key (g)] = sc.pattern[(size_t) g] == seq::kFree ? QJsonValue (QJsonValue::Null) : QJsonValue (sc.pattern[(size_t) g]);
        QJsonObject so { { "index", i }, { "name", QString::fromStdString (sc.name) }, { "groups", combo } };
        if (sc.locked)
            so["locked"] = true;                     // meme cle que le pilote
        scenes.append (so);
    }
    d["scenes"] = scenes;
    // Morceau : meme format que le pilote (arrangement, arr_loop, song_mode)
    QJsonArray arr;
    for (const seq::SongEntry& e : sequencer.song())
    {
        QJsonObject eo { { "scene", e.scene }, { "repeats", e.repeats } };
        if (e.bars > 0)
            eo["bars"] = e.bars;
        arr.append (eo);
    }
    // (ecrits seulement s'ils servent : un ancien projet n'est pas « modifie » a l'ouverture)
    if (! arr.isEmpty() || d.contains ("arrangement"))
        d["arrangement"] = arr;
    if (! sequencer.songLoop() || d.contains ("arr_loop"))
        d["arr_loop"] = sequencer.songLoop();
    if (sequencer.songMode() || d.contains ("song_mode"))
        d["song_mode"] = sequencer.songMode();
    int la = -1, lb = -1;                             // zone de boucle (l'app seulement ; le pilote l'ignore)
    sequencer.songLoopRange (la, lb);
    if (la >= 0 || d.contains ("song_loop_from"))
    {
        d["song_loop_from"] = la;
        d["song_loop_to"] = lb;
    }
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
        state.groupLabel[(size_t) g] = gd["name"].toString().trimmed().left (8).toUpper();
        const QJsonObject sounds = gd["sounds"].toObject();
        for (int s = 1; s <= 16; ++s)
        {
            const size_t i = (size_t) (g * 16 + s - 1);
            const QJsonObject sd = sounds[QString::number (s)].toObject();
            state.samples[i] = sd["sample"].toString();
            static const QStringList synthNames { "Kick", "Snare", "Closed HH", "Open HH", "Tone" };
            const QString syn = sd["synth"].toString();
            state.synth[i] = syn == "None" ? 0 : synthNames.contains (syn) ? (int) synthNames.indexOf (syn) + 1 : -1;
            state.soundReverse[i] = sd["reverse"].toBool (false);
            state.soundPoly[i] = std::clamp (sd["polyphony"].toInt (4), 1, 16);
            const QJsonArray params = sd["params"].toArray();
            state.soundVolume[i] = std::clamp (sd["volume"].toInt (params.size() > 0 ? params[0].toInt (100) : 100), 0, 127);
            state.soundPan[i] = std::clamp (params.size() > 1 ? params[1].toInt (64) : 64, 0, 127);
            state.soundPitch[i] = std::clamp (params.size() > 2 ? params[2].toInt (64) : 64, 0, 127);
            state.soundCutoff[i] = std::clamp (params.size() > 3 ? params[3].toInt (90) : 90, 0, 127);
            const QJsonObject env = sd["envelope"].toObject();
            const QJsonObject loop = sd["loop"].toObject();
            state.playMode[i] = ! env.contains ("mode") ? session::State::defaultMode (s)
                              : env["mode"].toString() == "ONE-SHOT" ? session::State::OneShot
                              : loop["on"].toBool() ? session::State::Loop : session::State::Gate;
            state.releaseMs[i] = std::clamp (env["release"].toInt (150), 5, 5000);
            state.loopStart[i] = (float) std::clamp (loop["start"].toDouble (0), 0.0, 1.0);
            state.loopEnd[i] = (float) std::clamp (loop["end"].toDouble (1), 0.0, 1.0);
        }
        // Tous les patterns du groupe ; le pattern choisi (« current ») existe toujours
        sequencer.selectPattern (g, 0, false);         // on repart de rien
        for (int idx = 1; idx < seq::kPatternSlots; ++idx)
            sequencer.removePattern (g, idx);
        sequencer.removePattern (g, 0);
        sequencer.setPatternLength (g, 16);
        const int current = std::clamp (gd["current"].toInt (0), 0, seq::kPatternSlots - 1);
        for (const auto& pv : gd["patterns"].toArray())
        {
            const QJsonObject pd = pv.toObject();
            const int idx = pd["index"].toInt (-1);
            if (idx < 0 || idx >= seq::kPatternSlots)
                continue;
            seq::Pattern p;
            p.length = std::clamp (pd["length"].toInt (16), 1, seq::kMaxRows);
            for (const auto& cv : pd["cells"].toArray())
            {
                int row = 0, track = 0;
                seq::Cell c;
                if (cellFromJson (cv.toArray(), row, track, c))
                    p.rows[(size_t) row][(size_t) track] = c;
            }
            sequencer.setPatternAt (g, idx, p);
            sequencer.setPatternName (g, idx, pd["name"].toString().toStdString());
        }
        sequencer.selectPattern (g, current, false);   // cree un pattern vide s'il manque
        sequencer.setGroupOff (g, gd["off"].toBool (true));   // pas d'info : eteint (actif des qu'on ecrit)
    }
    // Scenes
    for (int i = 0; i < seq::kPatternSlots; ++i)
        sequencer.setScene (i, seq::Scene());
    sequencer.setCurrentScene (-1);
    for (const auto& sv : data["scenes"].toArray())
    {
        const QJsonObject sd = sv.toObject();
        const int i = sd["index"].toInt (-1);
        if (i < 0 || i >= seq::kPatternSlots)
            continue;
        seq::Scene sc;
        sc.used = true;
        sc.name = sd["name"].toString().toStdString();
        sc.locked = sd["locked"].toBool (false);
        const QJsonObject combo = sd["groups"].toObject();
        for (int g = 0; g < session::kGroups; ++g)
        {
            const QJsonValue v = combo[key (g)];
            sc.pattern[(size_t) g] = v.isDouble() ? std::clamp (v.toInt(), -1, seq::kPatternSlots - 1) : seq::kFree;
        }
        sequencer.setScene (i, sc);
    }
    // Morceau
    std::vector<seq::SongEntry> song;
    for (const auto& ev : data["arrangement"].toArray())
    {
        const QJsonObject eo = ev.toObject();
        seq::SongEntry e;
        e.scene = eo["scene"].toInt (-1);
        if (e.scene < 0 || e.scene >= seq::kPatternSlots)
            continue;
        e.repeats = std::clamp (eo["repeats"].toDouble (1), 0.5, 16.0);
        e.bars = std::max (0.0, eo["bars"].toDouble (0));
        song.push_back (e);
    }
    sequencer.setSong (song);
    sequencer.songStartFrom (0);
    sequencer.setSongLoop (data["arr_loop"].toBool (true));
    sequencer.setSongMode (data["song_mode"].toBool (false));
    sequencer.setSongLoopRange (data["song_loop_from"].toInt (-1), data["song_loop_to"].toInt (-1));
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

QJsonArray Project::cellToJson (int row, int track, const seq::Cell& c) { return ::cellToJson (row, track, c); }
bool Project::cellFromJson (const QJsonArray& a, int& row, int& track, seq::Cell& c) { return ::cellFromJson (a, row, track, c); }
