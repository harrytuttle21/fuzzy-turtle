// Fuzzy John DAW Pulses — application Qt (premier squelette).
// Licence : GPLv3.
//
//   --no-maschine        ne pas ouvrir la Maschine (le prototype Python peut l'utiliser)
//   --no-engine          ne pas demarrer le moteur audio
//   --screenshot FICHIER enregistre une image de la fenetre puis quitte (tests)
#include "main_window.h"

#include <QApplication>
#include <QIcon>
#include <QCommandLineParser>
#include <QTimer>

int main (int argc, char** argv)
{
    QApplication app (argc, argv);
    QApplication::setOrganizationName ("Fuzzy Turtle Software");
    QApplication::setApplicationName ("DAW Pulses");
    QApplication::setApplicationVersion (FUZZY_VERSION);
#ifdef FUZZY_HAS_LOGO
    QApplication::setWindowIcon (QIcon (":/logo_icon.png"));   // icone de l'app (resources/logo.png)
#endif   // la version du projet (CMakeLists.txt)

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption ({ "no-maschine", "Ne pas ouvrir la Maschine." });
    parser.addOption ({ "no-engine", "Ne pas demarrer le moteur audio." });
    parser.addOption ({ "screenshot", "Enregistre une image de la fenetre puis quitte.", "fichier" });
    parser.process (app);

    Options options;
    options.maschine = ! parser.isSet ("no-maschine");
    options.engine = ! parser.isSet ("no-engine");

    MainWindow window (options);
    window.show();

    if (parser.isSet ("screenshot"))
    {
        const QString file = parser.value ("screenshot");
        QTimer::singleShot (600, &window, [&window, file]
        {
            window.grab().save (file);
            window.close();
        });
    }
    return app.exec();
}
