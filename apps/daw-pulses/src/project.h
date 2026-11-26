// Fuzzy John DAW Pulses — projets .fuzzy (dossier NOM.fuzzy contenant project.json).
//
// Meme format que le prototype (prototypes/maschine-hid/mk3_live.py, bouton FILE) : un projet
// s'ouvre dans les deux. Le fichier entier est garde en memoire ; l'app n'y reecrit que ce
// qu'elle sait modifier (nom, tempo, volumes, sons, tous les patterns, scenes). Le reste
// (arrangement...) est conserve tel quel. Licence : GPLv3.
#pragma once
#include "sequencer.h"
#include "session.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

class Project
{
public:
    static constexpr int kNameLength = 8;             // noms courts, comme sur la Maschine

    QString name = "UNTITLED";
    QString file;                                     // .../NOM.fuzzy/project.json (vide = jamais enregistre)

    void reset();                                     // projet vide
    bool load (const QString& path, QString& error);  // lit le fichier (sans toucher a l'app)
    bool save (const QString& projectsDir, QString& error);   // ecrit NOM.fuzzy/project.json (+ .bak)

    // Echanges avec l'app
    void toApp (seq::Sequencer& sequencer, session::State& state, int& bpm) const;
    void fromApp (const seq::Sequencer& sequencer, const session::State& state, int bpm);

    QByteArray text() const;                          // JSON tel qu'il serait ecrit
    bool modified (const seq::Sequencer& sequencer, const session::State& state, int bpm) const;
    void markSaved() { savedText = text(); }

    static QString cleanName (const QString& name);   // A-Z 0-9 - _, 8 caracteres
    static QString folderFor (const QString& projectsDir, const QString& name);
    static QString defaultDir();                      // projets du prototype, sinon ~/Documents
    // Une case de pattern en JSON ([ligne, piste, note(s), velocite, duree, decalage]) : aussi
    // pour les fichiers de groupe (.fzgroup)
    static QJsonArray cellToJson (int row, int track, const seq::Cell& c);
    static bool cellFromJson (const QJsonArray& a, int& row, int& track, seq::Cell& c);

private:
    QJsonObject data;
    QByteArray savedText;
};
