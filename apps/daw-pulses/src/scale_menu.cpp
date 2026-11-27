// Fuzzy John DAW Pulses — menus des gammes et des accords. Licence : GPLv3.
#include "scale_menu.h"
#include "music_tables.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QFileDialog>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <algorithm>
#include <QSettings>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace music {

namespace {

// Editeur commun : un nom et des notes a cocher (degres d'une gamme ou intervalles d'un accord)
bool editNotes (QWidget* parent, Custom& item, const QString& title, const QStringList& labels, bool firstFixed)
{
    QDialog dialog (parent);
    dialog.setWindowTitle (title);
    auto* col = new QVBoxLayout (&dialog);
    auto* name = new QLineEdit (item.name);
    name->setPlaceholderText (QObject::tr ("Name"));
    col->addWidget (name);
    auto* grid = new QGridLayout;
    QList<QPushButton*> buttons;
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = new QPushButton (labels[i]);
        b->setCheckable (true);
        b->setChecked (std::find (item.notes.begin(), item.notes.end(), i) != item.notes.end() || (firstFixed && i == 0));
        b->setEnabled (! (firstFixed && i == 0));   // la tonique est toujours la
        b->setMinimumWidth (44);
        grid->addWidget (b, i / 12, i % 12);
        buttons << b;
    }
    col->addLayout (grid);
    auto* hint = new QLabel (firstFixed ? QObject::tr ("Click the notes of the scale (from the root, 1 = root)")
                                        : QObject::tr ("Click the notes of the chord (from the played note, 1 = the note itself)"));
    hint->setObjectName ("hint");
    col->addWidget (hint);
    auto* box = new QDialogButtonBox (QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect (box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect (box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    col->addWidget (box);
    auto check = [&]
    {
        int count = 0;
        for (auto* b : buttons)
            count += b->isChecked();
        box->button (QDialogButtonBox::Ok)->setEnabled (! name->text().trimmed().isEmpty() && count >= (firstFixed ? 2 : 1));
    };
    for (auto* b : buttons)
        QObject::connect (b, &QPushButton::toggled, &dialog, check);
    QObject::connect (name, &QLineEdit::textChanged, &dialog, check);
    check();
    if (dialog.exec() != QDialog::Accepted)
        return false;
    item.name = name->text().trimmed();
    item.notes.clear();
    for (int i = 0; i < buttons.size(); ++i)
        if (buttons[i]->isChecked())
            item.notes.push_back (i);
    return true;
}

// Sous-menu « Delete » commun aux gammes et accords personnalises
void addDelete (QMenu* custom, std::vector<Custom>& list, QWidget* parent, const std::function<void (int)>& removed)
{
    if (list.empty())
        return;
    auto* del = custom->addMenu (QObject::tr ("Delete"));
    for (int i = 0; i < (int) list.size(); ++i)
        QObject::connect (del->addAction (list[(size_t) i].name), &QAction::triggered, parent, [&list, i, parent, removed]
        {
            if (QMessageBox::question (parent, QObject::tr ("Delete"), QObject::tr ("Delete \"%1\" ?").arg (list[(size_t) i].name))
                != QMessageBox::Yes)
                return;
            list.erase (list.begin() + i);
            saveCustom();
            removed (i);
        });
}


// --- Export / Import : gammes et accords perso, accords User (fichiers .json echangeables)

void exportList (QWidget* parent, const std::vector<Custom>& list, const QString& kind, const QString& fileName)
{
    const QString path = QFileDialog::getSaveFileName (parent, QObject::tr ("Export %1").arg (kind),
                                                       QDir::homePath() + "/" + fileName, QObject::tr ("JSON (*.json)"));
    if (path.isEmpty())
        return;
    QJsonArray items;
    for (const Custom& c : list)
    {
        QJsonArray notes;
        for (int n : c.notes)
            notes.append (n);
        items.append (QJsonObject { { "name", c.name }, { "notes", notes } });
    }
    QSaveFile f (path);
    const bool ok = f.open (QIODevice::WriteOnly)
                    && f.write (QJsonDocument (QJsonObject { { "format", "fuzzy-" + kind.toLower().replace (' ', '-') }, { "items", items } })
                                    .toJson (QJsonDocument::Indented)) >= 0
                    && f.commit();
    QMessageBox::information (parent, QObject::tr ("Export"), ok ? QObject::tr ("%1 %2 exported.").arg (list.size()).arg (kind)
                                                                 : QObject::tr ("Cannot write %1").arg (path));
}

bool importList (QWidget* parent, std::vector<Custom>& list, const QString& kind)
{
    // Ajoute a la liste ; un nom deja pris devient « nom (2) »
    const QString path = QFileDialog::getOpenFileName (parent, QObject::tr ("Import %1").arg (kind), QDir::homePath(),
                                                       QObject::tr ("JSON (*.json)"));
    if (path.isEmpty())
        return false;
    QFile f (path);
    const QJsonArray items = f.open (QIODevice::ReadOnly) ? QJsonDocument::fromJson (f.readAll()).object()["items"].toArray() : QJsonArray();
    int added = 0;
    for (const auto& v : items)
    {
        Custom c;
        c.name = v.toObject()["name"].toString().trimmed();
        for (const auto& n : v.toObject()["notes"].toArray())
            c.notes.push_back (n.toInt());
        if (c.name.isEmpty() || c.notes.empty())
            continue;
        auto taken = [&list] (const QString& n)
        {
            return std::any_of (list.begin(), list.end(), [&n] (const Custom& x) { return x.name == n; });
        };
        const QString base = c.name;
        for (int k = 2; taken (c.name); ++k)
            c.name = QString ("%1 (%2)").arg (base).arg (k);
        list.push_back (c);
        ++added;
    }
    QMessageBox::information (parent, QObject::tr ("Import"), added ? QObject::tr ("%1 %2 added.").arg (added).arg (kind)
                                                                     : QObject::tr ("Nothing to import in this file."));
    return added > 0;
}

void addExchange (QMenu* m, std::vector<Custom>& list, const QString& kind, const QString& fileName, QWidget* parent,
                  const std::function<void()> imported)
{
    m->addSeparator();
    QObject::connect (m->addAction (QObject::tr ("Export %1...").arg (kind)), &QAction::triggered, parent,
                      [parent, &list, kind, fileName] { exportList (parent, list, kind, fileName); });
    QObject::connect (m->addAction (QObject::tr ("Import %1...").arg (kind)), &QAction::triggered, parent,
                      [parent, &list, kind, imported]
    {
        if (importList (parent, list, kind))
        {
            saveCustom();
            imported();
        }
    });
}
} // namespace

bool editScale (QWidget* parent, Custom& scale)
{
    return editNotes (parent, scale, QObject::tr ("Custom scale"),
                      { "1", "b2", "2", "b3", "3", "4", "b5", "5", "b6", "6", "b7", "7" }, true);
}

bool editChord (QWidget* parent, Custom& chord)
{
    return editNotes (parent, chord, QObject::tr ("Custom chord"),
                      { "1", "b2", "2", "b3", "3", "4", "b5", "5", "#5", "6", "b7", "7",
                        "8", "b9", "9", "#9", "10", "11", "#11", "12", "b13", "13", "b14", "14" }, false);
}

void fillScaleMenu (QMenu* menu, int current, const std::function<void (int)>& pick, QWidget* parent)
{
    menu->clear();
    // Categories du pilote, dans l'ordre (Main, Major, Minor...)
    QMenu* bank = nullptr;
    QString bankName;
    for (int i = 0; i < (int) scales().size(); ++i)
    {
        if (bank == nullptr || bankName != scales()[(size_t) i].bank)
        {
            bankName = scales()[(size_t) i].bank;
            bank = menu->addMenu (bankName);
        }
        auto* a = bank->addAction (scales()[(size_t) i].name);
        a->setCheckable (true);
        a->setChecked (i == current);
        if (i == current)
            bank->setTitle (QStringLiteral ("\u2713 ") + bankName);
        QObject::connect (a, &QAction::triggered, parent, [pick, i] { pick (i); });
    }
    menu->addSeparator();
    auto* custom = menu->addMenu (current < 0 ? QStringLiteral ("\u2713 Custom") : QStringLiteral ("Custom"));
    for (int i = 0; i < (int) customScales().size(); ++i)
    {
        auto* a = custom->addAction (customScales()[(size_t) i].name);
        a->setCheckable (true);
        a->setChecked (current == customScaleId (i));
        QObject::connect (a, &QAction::triggered, parent, [pick, i] { pick (customScaleId (i)); });
    }
    if (! customScales().empty())
        custom->addSeparator();
    QObject::connect (custom->addAction (QObject::tr ("New Scale...")), &QAction::triggered, parent, [pick, parent]
    {
        Custom scale { QObject::tr ("My scale"), { 0, 2, 4, 5, 7, 9, 11 } };
        if (! editScale (parent, scale))
            return;
        customScales().push_back (scale);
        saveCustom();
        pick (customScaleId ((int) customScales().size() - 1));
    });
    if (current < 0 && -current - 1 < (int) customScales().size())
        QObject::connect (custom->addAction (QObject::tr ("Edit \"%1\"...").arg (customScales()[(size_t) (-current - 1)].name)),
                          &QAction::triggered, parent, [pick, parent, current]
        {
            Custom& scale = customScales()[(size_t) (-current - 1)];
            if (editScale (parent, scale))
            {
                saveCustom();
                pick (current);
            }
        });
    addDelete (custom, customScales(), parent, [pick, current] (int removed)
    {
        if (current == customScaleId (removed) || current < customScaleId (removed))
            pick (0);                                // la gamme jouee a disparu : Chromatic
    });
    addExchange (custom, customScales(), QObject::tr ("custom scales"), "fuzzy_scales.json", parent, [pick, current] { pick (current); });
}

void fillChordMenu (QMenu* menu, const KeySettings& k, const std::function<void (int, int)>& pick, QWidget* parent)
{
    menu->clear();
    auto* off = menu->addAction (QObject::tr ("Chords off"));
    off->setCheckable (true);
    off->setChecked (k.chordMode == 0);
    QObject::connect (off, &QAction::triggered, parent, [pick] { pick (0, 0); });
    auto* harm = menu->addMenu (k.chordMode == 1 ? QStringLiteral ("\u2713 Harmonizer") : QStringLiteral ("Harmonizer"));
    for (int i = 0; i < (int) harmonies().size(); ++i)
    {
        auto* a = harm->addAction (harmonies()[(size_t) i].name);
        a->setCheckable (true);
        a->setChecked (k.chordMode == 1 && k.harmony == i);
        QObject::connect (a, &QAction::triggered, parent, [pick, i] { pick (1, i); });
    }
    auto* sets = menu->addMenu (k.chordMode == 2 ? QStringLiteral ("\u2713 Chord Sets") : QStringLiteral ("Chord Sets"));
    for (int i = 0; i < (int) chordSets().size(); ++i)
    {
        auto* a = sets->addAction (chordSets()[(size_t) i].name);
        a->setCheckable (true);
        a->setChecked (k.chordMode == 2 && k.chordSet == i);
        QObject::connect (a, &QAction::triggered, parent, [pick, i] { pick (2, i); });
    }
    // User 1-10 : accords perso (clic droit sur un pad en mode Chords : « Add to User »)
    auto* user = menu->addMenu (k.chordMode == 4 ? QStringLiteral ("\u2713 User sets") : QStringLiteral ("User sets"));
    for (int i = 0; i < kUserBanks; ++i)
    {
        const int n = userCount (i);
        auto* a = user->addAction (QString ("User %1").arg (i + 1) + (n ? QString ("  (%1)").arg (n) : QObject::tr ("  (empty)")));
        a->setCheckable (true);
        a->setChecked (k.chordMode == 4 && k.userBank == i);
        QObject::connect (a, &QAction::triggered, parent, [pick, i] { pick (4, i); });
    }
    user->addSeparator();
    QObject::connect (user->addAction (QObject::tr ("Export User sets...")), &QAction::triggered, parent, [parent]
    {
        const QString path = QFileDialog::getSaveFileName (parent, QObject::tr ("Export User sets (10 banks)"),
                                                           QDir::homePath() + "/fuzzy_user_chords.json", QObject::tr ("JSON (*.json)"));
        if (path.isEmpty())
            return;
        const QString keep = QSettings().value ("keys/userChordsFile").toString();
        QSettings().setValue ("keys/userChordsFile", path);   // meme format que le pilote
        const bool ok = saveUserChords();
        QSettings().setValue ("keys/userChordsFile", keep);
        QMessageBox::information (parent, QObject::tr ("Export"), ok ? QObject::tr ("User sets exported.") : QObject::tr ("Cannot write %1").arg (path));
    });
    QObject::connect (user->addAction (QObject::tr ("Import User sets...")), &QAction::triggered, parent, [parent, pick, k]
    {
        const QString path = QFileDialog::getOpenFileName (parent, QObject::tr ("Import User sets"), QDir::homePath(), QObject::tr ("JSON (*.json)"));
        if (path.isEmpty())
            return;
        QMessageBox ask (QMessageBox::Question, QObject::tr ("Import User sets"),
                         QObject::tr ("Replace all 10 banks, or only fill the empty slots?"), QMessageBox::Cancel, parent);
        auto* replace = ask.addButton (QObject::tr ("Replace"), QMessageBox::DestructiveRole);
        auto* add = ask.addButton (QObject::tr ("Add (empty slots)"), QMessageBox::AcceptRole);
        ask.setDefaultButton (QMessageBox::Cancel);
        ask.exec();
        if (ask.clickedButton() != replace && ask.clickedButton() != add)
            return;
        const auto before = userChords();
        const QString keep = QSettings().value ("keys/userChordsFile").toString();
        QSettings().setValue ("keys/userChordsFile", path);
        loadUserChords();                            // lit le fichier importe
        QSettings().setValue ("keys/userChordsFile", keep);
        if (ask.clickedButton() == add)              // garder les accords deja la
        {
            auto incoming = userChords();
            userChords() = before;
            for (int b = 0; b < kUserBanks; ++b)
                for (int c = 0; c < 12; ++c)
                    if (userChords()[(size_t) b][(size_t) c].empty())
                        userChords()[(size_t) b][(size_t) c] = incoming[(size_t) b][(size_t) c];
        }
        saveUserChords();                            // dans le fichier partage avec la Maschine
        pick (k.chordMode, k.chordMode == 1 ? k.harmony : k.chordMode == 2 ? k.chordSet
                         : k.chordMode == 3 ? k.customChord : k.userBank);
    });
    auto* file = user->addAction (QObject::tr ("Chords file (shared with the Maschine)..."));
    file->setToolTip (userChordsFile());
    user->setToolTipsVisible (true);
    QObject::connect (file, &QAction::triggered, parent, [pick, parent, k]
    {
        const QString path = QFileDialog::getSaveFileName (parent, QObject::tr ("User chords file (user_chords.json of the Maschine driver)"),
                                                           userChordsFile(), QObject::tr ("Chords (*.json)"),
                                                           nullptr, QFileDialog::DontConfirmOverwrite);
        if (path.isEmpty())
            return;
        QSettings().setValue ("keys/userChordsFile", path);
        loadUserChords();                            // existant : on le lit ; nouveau : vide
        pick (k.chordMode, k.chordMode == 1 ? k.harmony : k.chordMode == 2 ? k.chordSet
                         : k.chordMode == 3 ? k.customChord : k.userBank);   // meme reglage, accords relus
    });
    menu->addSeparator();
    auto* custom = menu->addMenu (k.chordMode == 3 ? QStringLiteral ("\u2713 Custom") : QStringLiteral ("Custom"));
    for (int i = 0; i < (int) customChords().size(); ++i)
    {
        auto* a = custom->addAction (customChords()[(size_t) i].name);
        a->setCheckable (true);
        a->setChecked (k.chordMode == 3 && k.customChord == i);
        QObject::connect (a, &QAction::triggered, parent, [pick, i] { pick (3, i); });
    }
    if (! customChords().empty())
        custom->addSeparator();
    QObject::connect (custom->addAction (QObject::tr ("New Chord...")), &QAction::triggered, parent, [pick, parent]
    {
        Custom chord { QObject::tr ("My chord"), { 0, 4, 7 } };
        if (! editChord (parent, chord))
            return;
        customChords().push_back (chord);
        saveCustom();
        pick (3, (int) customChords().size() - 1);
    });
    if (k.chordMode == 3 && k.customChord < (int) customChords().size())
    {
        const int index = k.customChord;
        QObject::connect (custom->addAction (QObject::tr ("Edit \"%1\"...").arg (customChords()[(size_t) index].name)),
                          &QAction::triggered, parent, [pick, parent, index]
        {
            if (editChord (parent, customChords()[(size_t) index]))
            {
                saveCustom();
                pick (3, index);
            }
        });
    }
    const int mode = k.chordMode, chosen = k.customChord;
    addDelete (custom, customChords(), parent, [pick, mode, chosen] (int removed)
    {
        if (mode == 3 && chosen >= removed)
            pick (0, 0);                             // l'accord joue a disparu : accords coupes
    });
    addExchange (custom, customChords(), QObject::tr ("custom chords"), "fuzzy_chords.json", parent, [pick, mode, chosen] { pick (mode, mode == 3 ? chosen : 0); });
}

} // namespace music
