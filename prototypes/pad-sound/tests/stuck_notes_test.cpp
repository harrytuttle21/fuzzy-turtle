// Test du moteur sans carte son : beaucoup de notes tres rapides -> silence a la fin.
#define main engine_main
#include "../main.cpp"
#undef main
#include <random>

static float runScenario (int sounds, int groups, int maxHeld, int steps, unsigned seed)
{
    EventQueue q; SampleBank bank; Engine eng (q, bank);
    std::vector<float> L (128), R (128); float* outs[2] { L.data(), R.data() };
    juce::AudioIODeviceCallbackContext ctx {};
    auto render = [&] (int blocks) { float peak = 0; for (int b = 0; b < blocks; ++b) {
        eng.audioDeviceIOCallbackWithContext (nullptr, 0, outs, 2, 128, ctx);
        for (float v : L) peak = std::max (peak, std::abs (v)); } return peak; };
    std::mt19937 rng (seed);
    struct Held { int snd, note, tag; };
    std::vector<Held> held;
    for (int step = 0; step < steps; ++step)
    {
        if ((int) held.size() < maxHeld && rng() % 2)
        {
            const int snd = 1 + rng() % sounds, tag = rng() % groups, note = 36 + rng() % 36;
            bool dup = false;
            for (auto& h : held) dup |= h.snd == snd && h.note == note && h.tag == tag;
            if (dup) continue;
            PadEvent e { snd, 100, note, 0 }; e.tag = tag; e.mode = snd > 4 ? 1 : 0;
            q.push (e);
            if (snd > 4) held.push_back ({ snd, note, tag });
        }
        else if (! held.empty())
        {
            const size_t k = rng() % held.size();
            PadEvent e { held[k].snd, 0, held[k].note, 0 }; e.tag = held[k].tag; e.off = true;
            q.push (e);
            held.erase (held.begin() + (long) k);
        }
        if (rng() % 3 == 0) render (1);
    }
    for (auto& h : held) { PadEvent e { h.snd, 0, h.note, 0 }; e.tag = h.tag; e.off = true; q.push (e); }
    render (1);
    render (2000);                                  // ~5 s : les sons ONE-SHOT s'eteignent
    return render (50);
}

int main (int argc, char** argv)
{
    int fails = 0;
    struct S { const char* name; int sounds, groups, maxHeld, steps; } cases[] {
        { "clavier rapide (1 son)", 1, 1, 12, 4000 },
        { "16 sons, 1 groupe", 16, 1, 30, 8000 },
        { "16 sons, 16 groupes, beaucoup de notes", 16, 16, 80, 20000 },
    };
    for (auto& c : cases)
        for (unsigned seed = 1; seed <= 5; ++seed)
        {
            const float tail = runScenario (c.sounds, c.groups, c.maxHeld, c.steps, seed);
            const bool ok = tail < 0.0005f;
            fails += ! ok;
            std::printf ("%s  %-45s graine %u : niveau final %.6f\n", ok ? "OK    " : "ECHEC ", c.name, seed, tail);
        }
    {   // Vumetres : une note du groupe 3 ne fait bouger que le groupe 3 et le master
        EventQueue q; SampleBank bank; Engine eng (q, bank);
        std::vector<float> L (128), R (128); float* outs[2] { L.data(), R.data() };
        juce::AudioIODeviceCallbackContext ctx {};
        PadEvent e { 5, 120, 60, 0 }; e.tag = 3; e.mode = 1; q.push (e);
        for (int b = 0; b < 20; ++b) eng.audioDeviceIOCallbackWithContext (nullptr, 0, outs, 2, 128, ctx);
        const float g3 = eng.meters[3].exchange (0), g0 = eng.meters[0].exchange (0), master = eng.meters[16].exchange (0);
        const bool ok = g3 > 0.01f && g0 == 0.0f && master > 0.01f;
        fails += ! ok;
        std::printf ("%s  vumetres : groupe 3 = %.3f, groupe A = %.3f, master = %.3f\n", ok ? "OK    " : "ECHEC ", g3, g0, master);
    }
    // Samples donnes en argument (WAV, MP3...) : lus directement depuis le fichier, puis joues
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    for (int i = 1; i < argc; ++i)
    {
        auto sample = loadSample (formats, juce::String::fromUTF8 (argv[i]), 60);
        float peak = 0;
        if (sample != nullptr)
        {
            EventQueue q; SampleBank bank; Engine eng (q, bank);
            std::vector<float> L (128), R (128); float* outs[2] { L.data(), R.data() };
            juce::AudioIODeviceCallbackContext ctx {};
            bank.set (0, 1, std::move (sample));
            PadEvent e { 1, 120, 60, 0 }; e.tag = 0; q.push (e);
            for (int b = 0; b < 40; ++b)
            {
                eng.audioDeviceIOCallbackWithContext (nullptr, 0, outs, 2, 128, ctx);
                for (float v : L) peak = std::max (peak, std::abs (v));
            }
        }
        const bool ok = peak > 0.01f;
        fails += ! ok;
        std::printf ("%s  sample %s : %s\n", ok ? "OK    " : "ECHEC ", argv[i],
                     ok ? "lu et joue" : "illisible ou muet");
    }
    return fails ? 1 : 0;
}
