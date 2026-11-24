// fuzzy-control : fichier de calibration.
#include "fuzzy_control/calibration.h"
#include "fuzzy_control/decoder.h"
#include "fuzzy_control/lights.h"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

namespace fuzzy::control {

namespace {
std::string trim (const std::string& s)
{
    const auto a = s.find_first_not_of (" \t\r");
    if (a == std::string::npos)
        return {};
    const auto b = s.find_last_not_of (" \t\r");
    return s.substr (a, b - a + 1);
}

bool toInt (const std::string& s, int lo, int hi, int& out)
{
    char* end = nullptr;
    const long v = std::strtol (s.c_str(), &end, 10);
    if (s.empty() || *end != '\0' || v < lo || v > hi)
        return false;
    out = (int) v;
    return true;
}

// Sous sudo : dossier et identite de l'utilisateur d'origine
struct RealUser { std::string home; uid_t uid = (uid_t) -1; gid_t gid = (gid_t) -1; };
RealUser realUser()
{
    RealUser u;
    if (geteuid() == 0)
        if (const char* name = std::getenv ("SUDO_USER"))
            if (const passwd* pw = getpwnam (name))
            {
                u.home = pw->pw_dir;
                u.uid = pw->pw_uid;
                u.gid = pw->pw_gid;
                return u;
            }
    if (const char* h = std::getenv ("HOME"))
        u.home = h;
    return u;
}

void makeDirFor (const std::string& path, const RealUser& u)
{
    for (size_t pos = 1; (pos = path.find ('/', pos)) != std::string::npos; ++pos)
    {
        const std::string dir = path.substr (0, pos);
        if (mkdir (dir.c_str(), 0755) == 0 && u.uid != (uid_t) -1 && chown (dir.c_str(), u.uid, u.gid) != 0)
            continue;   // pas grave : le dossier existe quand meme
    }
}
} // namespace

Calibration Calibration::mk3Defaults()
{
    Calibration c;
    for (const auto& b : mk3Buttons())
        c.buttons[b.name].push_back (b.bit);
    for (const auto& l : mk3LightSlots())
        c.lights[std::string (l.name)] = { l.slot, l.screenButton };
    return c;
}

Calibration Calibration::plusDefaults()
{
    Calibration c = mk3Defaults();
    c.model = "Maschine+";
    return c;
}

Calibration Calibration::defaultsFor (const std::string& productName)
{
    return productName == "Maschine+" ? plusDefaults() : mk3Defaults();
}

std::string defaultCalibrationPath (const std::string& productName)
{
    const std::string file = productName == "Maschine+" ? "maschine-plus.conf" : "maschine-mk3.conf";
    return realUser().home + "/.config/fuzzy-turtle/" + file;
}

bool Calibration::load (const std::string& path, std::vector<std::string>& warnings)
{
    std::ifstream in (path);
    if (! in)
        return false;
    std::string line, section;
    int number = 0;
    while (std::getline (in, line))
    {
        ++number;
        if (const auto hash = line.find ('#'); hash != std::string::npos)
            line.erase (hash);
        line = trim (line);
        if (line.empty())
            continue;
        const auto warn = [&] (const std::string& why)
        {
            warnings.push_back ("ligne " + std::to_string (number) + " : " + why + "  (" + line + ")");
        };
        if (line.front() == '[' && line.back() == ']')
        {
            section = trim (line.substr (1, line.size() - 2));
            if (section != "controleur" && section != "boutons" && section != "lumieres")
                warn ("section inconnue");
            continue;
        }
        const auto eq = line.find ('=');
        if (eq == std::string::npos)
        {
            warn ("il manque le signe =");
            continue;
        }
        const std::string key = trim (line.substr (0, eq));
        const std::string value = trim (line.substr (eq + 1));
        int n = 0;

        if (section == "boutons")
        {
            std::istringstream words (value);
            std::vector<int> found;
            std::string w;
            bool ok = true;
            while (words >> w)
                ok = ok && toInt (w, 0, Mk3Decoder::kButtonBits - 1, n) && (found.push_back (n), true);
            if (value == "-")
                buttons.erase (key);
            else if (ok && ! found.empty())
                buttons[key] = found;
            else
                warn ("bit(s) attendu(s) entre 0 et 71");
        }
        else if (section == "lumieres")
        {
            std::istringstream words (value);
            std::string first, second;
            words >> first >> second;
            if (first == "-")
                lights.erase (key);
            else if (toInt (first, 0, 61, n) && (second.empty() || second == "ecran"))
                lights[key] = { n, second == "ecran" };
            else
                warn ("emplacement attendu entre 0 et 61 (suivi eventuellement de \"ecran\")");
        }
        else if (section == "controleur")
        {
            if (key == "modele")
                model = value;
            else if (key == "courbe_velocite")
            {
                char* end = nullptr;
                const double v = std::strtod (value.c_str(), &end);
                if (*end == '\0' && v >= 0.2 && v <= 3.0)
                    velocityExponent = v;
                else
                    warn ("courbe attendue entre 0.2 et 3 (0.5 = soft)");
            }
            else if (key == "groupes_emplacement")
            {
                if (toInt (value, 0, 54, n)) groupSlot = n; else warn ("emplacement attendu entre 0 et 54");
            }
            else if (key == "groupes_couleurs")
            {
                std::istringstream words (value);
                std::array<int, 8> hues {};
                std::string w;
                int count = 0;
                bool ok = true;
                while (words >> w)
                    ok = ok && count < 8 && toInt (w, 0, kWhiteHue, hues[(size_t) count++]);
                if (ok && count == 8) groupHues = hues; else warn ("8 teintes attendues, de 0 a 16");
            }
            else if (key == "niveau_doux" || key == "niveau_vif" || key == "niveau_ecran_repos"
                     || key == "niveau_ecran_actif")
            {
                if (! toInt (value, 0, 127, n))
                    warn ("niveau attendu entre 0 et 127");
                else
                    (key == "niveau_doux" ? buttonDim : key == "niveau_vif" ? buttonBright
                     : key == "niveau_ecran_repos" ? screenIdle : screenActive) = (uint8_t) n;
            }
            else
                warn ("reglage inconnu");
        }
        else
            warn ("ligne hors d'une section [controleur], [boutons] ou [lumieres]");
    }
    return true;
}

bool Calibration::save (const std::string& path, std::string& error) const
{
    const RealUser u = realUser();
    makeDirFor (path, u);
    std::ofstream out (path);
    if (! out)
    {
        error = "impossible d'ecrire " + path;
        return false;
    }
    out << "# Calibration fuzzy-control (Fuzzy Turtle Software)\n"
           "# Une ligne absente garde la valeur par defaut ; \"nom = -\" desactive.\n\n"
           "[controleur]\n"
        << "modele = " << model << "\n"
        << "courbe_velocite = " << velocityExponent << "   # 0.35 softer, 0.5 soft, 1 lineaire, 1.6 dur\n"
        << "groupes_emplacement = " << groupSlot << "   # A-H : 8 emplacements a partir de celui-ci\n"
        << "groupes_couleurs =";
    for (const int h : groupHues)
        out << ' ' << h;
    out << "   # teintes de la palette NI (16 = blanc)\n"
        << "niveau_doux = " << (int) buttonDim << "\n"
        << "niveau_vif = " << (int) buttonBright << "\n"
        << "niveau_ecran_repos = " << (int) screenIdle << "\n"
        << "niveau_ecran_actif = " << (int) screenActive << "\n\n"
        << "[boutons]   # nom = bit dans le rapport 0x01\n";
    // tries par numero, plus faciles a relire
    std::multimap<int, std::string> byBit, bySlot;
    for (const auto& [name, list] : buttons) byBit.emplace (list.empty() ? 0 : list.front(), name);
    for (const auto& [name, l] : lights) bySlot.emplace (l.slot, name);
    for (const auto& [bit, name] : byBit)
    {
        out << name << " =";
        for (const int b : buttons.at (name))
            out << ' ' << b;
        out << "\n";
    }
    out << "\n[lumieres]   # nom = emplacement dans le rapport 0x80 (\"ecran\" = bouton au-dessus des ecrans)\n";
    for (const auto& [slot, name] : bySlot)
        out << name << " = " << slot << (lights.at (name).screenButton ? " ecran" : "") << "\n";
    out.close();
    if (! out)
    {
        error = "erreur d'ecriture de " + path;
        return false;
    }
    // le fichier reste modifiable sans sudo (si ca echoue, il reste lisible : pas grave)
    if (u.uid != (uid_t) -1 && chown (path.c_str(), u.uid, u.gid) != 0)
        return true;
    return true;
}

} // namespace fuzzy::control
