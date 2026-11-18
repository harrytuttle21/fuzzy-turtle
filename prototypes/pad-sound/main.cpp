// Fuzzy Turtle - prototype 4 : un son joue par JUCE au toucher d'un pad Maschine MK3.
//
// - lit les pads en HID (hidapi), comme le prototype 1 (protocole du pilote r22yb3m, MIT) ;
// - joue un son synthetise par JUCE : pads 1-4 batterie, pads 5-16 notes en C mineur pentatonique ;
// - mesure le temps entre la frappe et le depart du son.
//
// Usage :  ./fuzzy_pad_sound
//          ./fuzzy_pad_sound --buffer 64   (plus reactif, plus exigeant pour l'ordinateur)
//          (sans option : 96 echantillons)
//          ./fuzzy_pad_sound --osc         (ne lit pas la Maschine : recoit les notes de
//                                           mk3_live.py par OSC sur le port 57390)
// Licence : GPLv3 (JUCE utilise sous AGPLv3).

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_osc/juce_osc.h>
#include <hidapi.h>

#include <array>
#include <atomic>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <thread>

namespace
{
constexpr unsigned short kVid = 0x17cc, kPidMk3 = 0x1600, kPidPlus = 0x1820;
constexpr int kHidInterface = 4;
constexpr double kVelocityExponent = 0.5;   // courbe "soft", validee au prototype 1

std::atomic<bool> gRunning { true };

// ----------------------------------------------------------------------------
// Evenement "pad frappe", transmis du lecteur HID au moteur audio sans verrou.
constexpr int kPanic = -1;   // evenement special : couper toutes les voix

struct PadEvent
{
    int pad = 0;          // 1..16
    int velocity = 0;     // 1..127
    int note = -1;        // hauteur MIDI pour les pads 5-16 (-1 = hauteur par defaut du pad)
    juce::int64 ticks = 0; // instant de la lecture USB
};

class EventQueue
{
public:
    bool push (const PadEvent& e)
    {
        const auto scope = fifo.write (1);
        if (scope.blockSize1 == 0) return false;
        items[(size_t) scope.startIndex1] = e;
        return true;
    }
    bool pop (PadEvent& e)
    {
        const auto scope = fifo.read (1);
        if (scope.blockSize1 == 0) return false;
        e = items[(size_t) scope.startIndex1];
        return true;
    }
private:
    juce::AbstractFifo fifo { 256 };
    std::array<PadEvent, 256> items {};
};

// ----------------------------------------------------------------------------
// Synthe minimal : une voix par pad.
// Hauteur par defaut des pads 5-16 : C mineur pentatonique a partir de C3
int defaultNote (int pad)
{
    static constexpr int scale[] = { 0, 3, 5, 7, 10 };
    const int deg = std::max (0, pad - 5);
    return 48 + scale[deg % 5] + 12 * (deg / 5);
}

struct Voice
{
    bool active = false;
    double phase = 0, time = 0, amp = 0;
    int pad = 0;
    juce::Random noise;
    float hpState = 0;
    double fade = 1, fadeStep = 0;   // fondu de sortie quand le meme pad est rejoue

    int note = 60;

    void start (int p, int velocity, int midiNote)
    {
        note = midiNote >= 0 ? midiNote : defaultNote (p);
        pad = p; active = true; phase = 0; time = 0; fade = 1; fadeStep = 0;
        amp = 0.35 * velocity / 127.0;   // volume par voix : laisse de la marge a plusieurs notes
    }

    void release (double sr)          // s'eteindre en 10 ms, sans clic
    {
        if (fadeStep <= 0) fadeStep = 1.0 / (0.010 * sr);
    }

    float render (double sr)
    {
        const double dt = 1.0 / sr;
        float s = 0;
        double level = 1;   // niveau de l'enveloppe principale, pour savoir quand la voix s'eteint
        auto env = [this, &level] (double decay) { level = std::exp (-time / decay); return level; };
        switch (pad)
        {
            case 1: { // grosse caisse : sinus qui descend de 150 a 45 Hz
                const double f = 45 + 105 * std::exp (-time / 0.04);
                phase += 2 * juce::MathConstants<double>::pi * f * dt;
                s = (float) (std::sin (phase) * env (0.35));
                break;
            }
            case 2: { // caisse claire : bruit + corps a 180 Hz
                phase += 2 * juce::MathConstants<double>::pi * 180 * dt;
                s = (float) (std::sin (phase) * 0.5 * env (0.08)
                             + (noise.nextFloat() * 2 - 1) * 0.7 * env (0.15));
                break;
            }
            case 3:   // charleston ferme
            case 4: { // charleston ouvert
                const float n = noise.nextFloat() * 2 - 1;
                const float hp = n - hpState; hpState = n;   // filtre passe-haut tres simple
                s = (float) (hp * 0.5 * env (pad == 3 ? 0.04 : 0.3));
                break;
            }
            default: { // notes
                const double f = 440.0 * std::pow (2.0, (note - 69) / 12.0);
                phase += 2 * juce::MathConstants<double>::pi * f * dt;
                s = (float) ((std::sin (phase) + 0.3 * std::sin (2 * phase)) * 0.6 * env (0.6));
                break;
            }
        }
        // Attaque : le son ne demarre jamais d'un coup (sinon "clic").
        // 1 ms pour garder le punch de la batterie, 5 ms pour les notes.
        const double attack = std::min (1.0, time / (pad <= 4 ? 0.001 : 0.005));
        // La voix s'arrete quand elle est devenue inaudible (plus de coupure franche)
        if (time > 0.01 && level < 0.0005) active = false;
        if (fadeStep > 0)
        {
            fade -= fadeStep;
            if (fade <= 0) { fade = 0; active = false; }
        }
        time += dt;
        return (float) (s * amp * attack * fade);
    }
};

// ----------------------------------------------------------------------------
class Engine : public juce::AudioIODeviceCallback
{
public:
    explicit Engine (EventQueue& q) : queue (q) {}

    void audioDeviceAboutToStart (juce::AudioIODevice* device) override
    {
        sampleRate = device->getCurrentSampleRate();
        bufferSize = device->getCurrentBufferSizeSamples();
        outputLatency = device->getOutputLatencyInSamples();
    }
    void audioDeviceStopped() override {}

    void audioDeviceIOCallbackWithContext (const float* const*, int,
                                           float* const* out, int numOut, int numSamples,
                                           const juce::AudioIODeviceCallbackContext&) override
    {
        PadEvent e;
        while (queue.pop (e))
        {
            if (e.pad == kPanic)
            {
                for (auto& v : voices)
                    if (v.active) v.release (sampleRate);   // fondu de 10 ms, sans clic
                continue;
            }
            trigger (e.pad, e.velocity, e.note);
            // Temps passe dans le programme entre la lecture USB et le depart du son
            const auto waited = juce::Time::highResolutionTicksToSeconds (
                                    juce::Time::getHighResolutionTicks() - e.ticks) * 1000.0;
            lastPad.store (e.pad);
            lastVelocity.store (e.velocity);
            lastWaitMs.store (waited);
            hits.fetch_add (1);
        }
        for (int i = 0; i < numSamples; ++i)
        {
            float mix = 0;
            for (auto& v : voices)
                if (v.active) mix += v.render (sampleRate);
            mix = std::tanh (mix);                       // securite si beaucoup de notes s'empilent
            for (int ch = 0; ch < numOut; ++ch)
                if (out[ch] != nullptr) out[ch][i] = mix;
        }
    }

    double sampleRate = 48000;
    int bufferSize = 0, outputLatency = 0;
    std::atomic<int> hits { 0 }, lastPad { 0 }, lastVelocity { 0 };
    std::atomic<double> lastWaitMs { 0 };

private:
    EventQueue& queue;
    // Un pad rejoue (meme note) fait s'eteindre son son precedent en 10 ms (fondu),
    // comme sur une Maschine. 48 voix laissent la place a ces fondus.
    std::array<Voice, 48> voices {};

    void trigger (int pad, int velocity, int note)
    {
        // Meme pad ET meme note : on eteint l'ancienne (un accord garde toutes ses notes)
        const int wanted = note >= 0 ? note : defaultNote (pad);
        for (auto& v : voices)
            if (v.active && v.pad == pad && v.note == wanted) v.release (sampleRate);
        Voice* chosen = nullptr;
        for (auto& v : voices)
            if (! v.active) { chosen = &v; break; }
        if (chosen == nullptr)                       // toutes occupees : la plus ancienne
            for (auto& v : voices)
                if (chosen == nullptr || v.time > chosen->time) chosen = &v;
        chosen->start (pad, velocity, note);
    }
};

// ----------------------------------------------------------------------------
// Lecture des pads (rapport HID 0x02 : triplets id, statut+poids fort, poids faible).
int padNumberForId (int id) { return (3 - id / 4) * 4 + id % 4 + 1; }

int velocityFromRaw (int raw)
{
    const double x = std::min (raw, 4095) / 4095.0;
    return juce::jlimit (1, 127, (int) std::lround (127.0 * std::pow (x, kVelocityExponent)));
}

hid_device* openMaschine()
{
    hid_device* dev = nullptr;
    hid_device_info* list = hid_enumerate (kVid, 0);
    for (auto* d = list; d != nullptr && dev == nullptr; d = d->next)
        if ((d->product_id == kPidMk3 || d->product_id == kPidPlus)
            && (d->interface_number == kHidInterface || d->interface_number == -1))
            dev = hid_open_path (d->path);
    hid_free_enumeration (list);
    return dev;
}

void readPads (hid_device* dev, EventQueue& queue)
{
    std::array<bool, 17> down {};
    unsigned char buf[64];
    while (gRunning.load())
    {
        const int n = hid_read_timeout (dev, buf, sizeof (buf), 100);
        if (n <= 0 || buf[0] != 0x02) continue;
        const auto ticks = juce::Time::getHighResolutionTicks();
        for (int pos = 1; pos + 2 < n; pos += 3)
        {
            const int id = buf[pos], hi = buf[pos + 1], lo = buf[pos + 2];
            if (id >= 16 || (id == 0 && hi == 0 && lo == 0)) continue;
            const int status = hi >> 4, raw = ((hi & 0x0f) << 8) | lo;
            const int pad = padNumberForId (id);
            if ((status == 0 || status == 1) && raw > 0 && ! down[(size_t) pad])
            {
                down[(size_t) pad] = true;
                queue.push ({ pad, velocityFromRaw (raw), -1, ticks });
            }
            else if (status == 2 || status == 3)
                down[(size_t) pad] = false;
        }
    }
}
// ----------------------------------------------------------------------------
// Mode --osc : les notes arrivent de mk3_live.py (message /fuzzy/proto/hit son velocite note).
// Le rappel "temps reel" les transmet tout de suite au moteur, sans attendre l'interface.
class OscInput : private juce::OSCReceiver::Listener<juce::OSCReceiver::RealtimeCallback>
{
public:
    explicit OscInput (EventQueue& q) : queue (q) {}
    bool start (int port)
    {
        if (! receiver.connect (port)) return false;
        receiver.addListener (this);
        return true;
    }
private:
    void oscMessageReceived (const juce::OSCMessage& m) override
    {
        const auto address = m.getAddressPattern().toString();
        if (address == "/fuzzy/proto/panic")          // Shift + MUTE : tout couper
        {
            queue.push ({ kPanic, 0, -1, juce::Time::getHighResolutionTicks() });
            return;
        }
        if (address != "/fuzzy/proto/hit") return;
        if (m.size() < 3 || ! m[0].isInt32() || ! m[1].isInt32() || ! m[2].isInt32()) return;
        const int pad = m[0].getInt32();
        if (pad < 1 || pad > 16) return;
        queue.push ({ pad, juce::jlimit (1, 127, m[1].getInt32()), m[2].getInt32(),
                      juce::Time::getHighResolutionTicks() });
    }
    EventQueue& queue;
    juce::OSCReceiver receiver;
};
} // namespace

// ----------------------------------------------------------------------------
int main (int argc, char* argv[])
{
    int wantedBuffer = 96;   // compromis entre 128 (tres sur) et 64 (tres reactif)
    bool oscMode = false;
    constexpr int kOscPort = 57390;   // port de test des prototypes (hors plage 57300-57306 des apps)
    for (int i = 1; i < argc; ++i)
    {
        const juce::String arg (argv[i]);
        if (arg == "--buffer" && i + 1 < argc) wantedBuffer = juce::String (argv[++i]).getIntValue();
        else if (arg == "--osc") oscMode = true;
    }

    std::signal (SIGINT, [] (int) { gRunning.store (false); });
    juce::ScopedJuceInitialiser_GUI juceInit;

    if (hid_init() != 0) { std::puts ("hidapi : initialisation impossible"); return 1; }
    hid_device* dev = oscMode ? nullptr : openMaschine();
    if (dev == nullptr && ! oscMode)
    {
        std::puts ("Maschine introuvable. Branchee ? Si un prototype a pris les ecrans (sudo), "
                   "debranche / rebranche-la.");
        hid_exit();
        return 1;
    }

    EventQueue queue;
    Engine engine (queue);
    juce::AudioDeviceManager audio;
    const auto err = audio.initialiseWithDefaultDevices (0, 2);
    if (err.isNotEmpty()) { std::printf ("Audio : %s\n", err.toRawUTF8()); return 1; }
    auto setup = audio.getAudioDeviceSetup();
    setup.bufferSize = wantedBuffer;
    audio.setAudioDeviceSetup (setup, true);
    audio.addAudioCallback (&engine);

    auto* device = audio.getCurrentAudioDevice();
    if (device == nullptr)
    {
        std::puts ("Aucune sortie audio disponible : choisis une sortie dans Reglages Systeme > Son.");
        return 1;
    }
    const double sr = device->getCurrentSampleRate();
    const int buffer = device->getCurrentBufferSizeSamples();
    const int outLat = device->getOutputLatencyInSamples();
    std::printf ("Sortie audio : %s, %.0f Hz, tampon %d echantillons (%.1f ms), "
                 "latence de sortie %.1f ms\n",
                 device->getName().toRawUTF8(), sr, buffer, buffer / sr * 1000.0, outLat / sr * 1000.0);
    std::puts ("Pads 1-4 : batterie ; pads 5-16 : notes. Ctrl+C pour quitter.");

    OscInput osc (queue);
    std::thread reader;
    if (oscMode)
    {
        if (! osc.start (kOscPort))
        {
            std::printf ("Port OSC %d deja utilise : un autre fuzzy_pad_sound tourne-t-il ?\n", kOscPort);
            return 1;
        }
        std::printf ("Mode OSC : en attente de mk3_live.py sur le port %d.\n", kOscPort);
    }
    else
        reader = std::thread (readPads, dev, std::ref (queue));
    int seen = 0;
    while (gRunning.load())
    {
        std::this_thread::sleep_for (std::chrono::milliseconds (50));
        const int h = engine.hits.load();
        if (h == seen) continue;
        seen = h;
        const double wait = engine.lastWaitMs.load();
        // Estimation : attente dans le programme + un tampon + latence de sortie de la carte son
        const double total = wait + (buffer + outLat) / sr * 1000.0;
        // "decrochages" : nombre de fois ou l'ordinateur n'a pas fourni le son a temps
        // (-1 = information non disponible sur cette carte son)
        std::printf ("PAD %2d  velocite %3d  | dans le programme %4.1f ms | total estime %4.1f ms"
                     " | decrochages %d\n",
                     engine.lastPad.load(), engine.lastVelocity.load(), wait, total,
                     device->getXRunCount());
        std::fflush (stdout);
    }

    if (reader.joinable()) reader.join();
    audio.removeAudioCallback (&engine);
    audio.closeAudioDevice();
    if (dev != nullptr) hid_close (dev);
    hid_exit();
    std::puts ("\nArrete.");
    return 0;
}
