#include <unistd.h>
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
//          --output "nom"  impose une sortie audio ; --quiet  pas de ligne par note
// En mode --osc, le moteur obeit aussi a SETTINGS > AUDIO / MIDI de mk3_live.py :
//   /fuzzy/proto/audio/set   s sortie, s entree, i frequence, i tampon
//   /fuzzy/proto/midi/set    s entree, s sortie, i horloge (0 off, 1 envoyer, 2 recevoir),
//                            i start/stop, i thru
//   /fuzzy/proto/midi/note   i canal, i note, i velocite, i duree ms ; /fuzzy/proto/midi/off
//   /fuzzy/proto/transport   i lecture, f tempo
// et renvoie son etat a l'interface (port 57391) : /fuzzy/proto/audio/state, /midi/state,
// /midi/in, /clock/in, /transport/in.
// Licence : GPLv3 (JUCE utilise sous AGPLv3).

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_osc/juce_osc.h>
#include <hidapi.h>

#include "midi_bridge.h"

#include <array>
#include <atomic>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <mutex>
#include <optional>
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
constexpr int kLiveOff = -2; // evenement special : fin de toutes les notes jouees en direct (tenues)

struct PadEvent
{
    int pad = 0;          // 1..16
    int velocity = 0;     // 1..127 (0 = fin de note : voir kNoteOff)
    int note = -1;        // hauteur MIDI pour les pads 5-16 (-1 = hauteur par defaut du pad)
    juce::int64 ticks = 0; // instant de la lecture USB
    int tag = -1;         // groupe (0-15) : une fin de note ne touche que ce groupe
    int mode = 0;         // 0 = ONE-SHOT (le son va au bout), 1 = ADSR (tient, puis relache)
    int releaseMs = 150;  // ADSR : relachement apres la fin de note
    int gateMs = 0;       // ADSR : duree de la note (0 = jusqu'a /fuzzy/proto/off ; -1 = Loop SP : seulement /off)
    bool off = false;     // fin de note (ADSR)
    int synth = 0;        // synthe : 1 Kick, 2 Snare, 3 Closed HH, 4 Open HH, 5 Tone, 6 Click (0 = celui du pad)
    int pan = 64;         // stereo : 0 gauche, 64 centre, 127 droite
    int poly = 4;         // polyphonie du son : notes en meme temps (1 = mono, comme Polyphony de Maschine)
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

// ----------------------------------------------------------------------------
// Sampler : un fichier audio par son (16 groupes x 16 sons). Les fichiers sont lus par le
// fil principal ; le moteur audio ne fait que lire un pointeur (aucun verrou, aucune attente).
struct Sample
{
    juce::AudioBuffer<float> data;
    double rate = 44100;   // frequence du fichier
    int root = 60;         // note a laquelle le fichier joue a sa hauteur d'origine
};

// Boucle d'un son : entre start et end (fractions du fichier), tant que la note est tenue
struct LoopInfo { bool on = false; float start = 0, end = 1; };

class SampleBank
{
public:
    LoopInfo loop (int tag, int pad) const
    {
        if (tag < 0 || tag > 15 || pad < 1 || pad > 16) return {};
        const auto& l = loops[(size_t) (tag * 16 + pad - 1)];
        return { l.on.load(), l.start.load(), l.end.load() };
    }
    void setLoop (int tag, int pad, LoopInfo info)
    {
        if (tag < 0 || tag > 15 || pad < 1 || pad > 16) return;
        auto& l = loops[(size_t) (tag * 16 + pad - 1)];
        l.start = juce::jlimit (0.0f, 1.0f, info.start);
        l.end = juce::jlimit (0.0f, 1.0f, info.end);
        l.on = info.on;
    }
    const Sample* get (int tag, int pad) const
    {
        if (tag < 0 || tag > 15 || pad < 1 || pad > 16) return nullptr;
        return slots[(size_t) (tag * 16 + pad - 1)].load (std::memory_order_acquire);
    }
    // Ce qui est joue : le sample, ou sa copie a l'envers si le son est en Reverse
    const Sample* play (int tag, int pad) const
    {
        if (tag < 0 || tag > 15 || pad < 1 || pad > 16) return nullptr;
        const size_t i = (size_t) (tag * 16 + pad - 1);
        return (reverse[i] ? backwards[i] : slots[i]).load (std::memory_order_acquire);
    }
    bool reversed (int tag, int pad) const
    {
        return tag >= 0 && tag <= 15 && pad >= 1 && pad <= 16 && reverse[(size_t) (tag * 16 + pad - 1)];
    }
    void setReverse (int tag, int pad, bool on)
    {
        if (tag < 0 || tag > 15 || pad < 1 || pad > 16) return;
        reverse[(size_t) (tag * 16 + pad - 1)] = on;
    }
    // Fil principal seulement. Les anciens samples sont gardes en memoire : une voix qui
    // joue encore peut les lire sans risque.
    void set (int tag, int pad, std::unique_ptr<Sample> sample)
    {
        if (tag < 0 || tag > 15 || pad < 1 || pad > 16) return;
        const Sample* p = sample.get();
        const Sample* r = nullptr;
        if (sample)
        {
            auto back = std::make_unique<Sample> (*sample);   // copie a l'envers (Reverse)
            back->data.reverse (0, back->data.getNumSamples());
            r = back.get();
            keep.push_back (std::move (sample));
            keep.push_back (std::move (back));
        }
        backwards[(size_t) (tag * 16 + pad - 1)].store (r, std::memory_order_release);
        slots[(size_t) (tag * 16 + pad - 1)].store (p, std::memory_order_release);
    }
private:
    std::array<std::atomic<const Sample*>, 256> slots {}, backwards {};
    std::array<std::atomic<bool>, 256> reverse {};
    std::vector<std::unique_ptr<Sample>> keep;
    struct AtomicLoop { std::atomic<bool> on { false }; std::atomic<float> start { 0 }, end { 1 }; };
    std::array<AtomicLoop, 256> loops {};
};

// Crêtes du fichier en n morceaux (0-1), pour dessiner sa forme d'onde
std::vector<float> samplePeaks (const Sample& smp, int n)
{
    std::vector<float> out ((size_t) n, 0.0f);
    const int len = smp.data.getNumSamples();
    for (int ch = 0; ch < smp.data.getNumChannels(); ++ch)
    {
        const float* d = smp.data.getReadPointer (ch);
        for (int i = 0; i < len; ++i)
        {
            auto& p = out[(size_t) std::min (n - 1, (int) ((juce::int64) i * n / len))];
            p = std::max (p, std::abs (d[i]));
        }
    }
    return out;
}

std::unique_ptr<Sample> loadSample (juce::AudioFormatManager& formats, const juce::String& path, int root)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (juce::File (path)));
    if (reader == nullptr || reader->lengthInSamples <= 0) return nullptr;
    const int length = (int) std::min<juce::int64> (reader->lengthInSamples,
                                                    (juce::int64) (reader->sampleRate * 30));   // 30 s maximum
    const int channels = (int) std::min (2u, reader->numChannels);
    auto sample = std::make_unique<Sample>();
    sample->data.setSize (channels, length + 1);
    sample->data.clear();
    reader->read (&sample->data, 0, length, 0, true, channels > 1);
    sample->rate = reader->sampleRate;
    sample->root = root;
    return sample;
}

struct Voice
{
    bool active = false;
    double phase = 0, time = 0, amp = 0;
    int pad = 0, tag = -1;
    bool adsr = false, held = false;        // ADSR : la note tient tant que "held"
    double gate = 0, releaseTime = 0.15;    // gate : fin de note programmee (s, 0 = aucune)
    juce::Random noise;
    float hpState = 0;
    double fade = 1, fadeStep = 0;   // fondu de sortie quand le meme pad est rejoue

    int note = 60;
    int kind = 5;                            // synthe joue (1-4 batterie, 5 Tone)
    float gainL = 1, gainR = 1;              // stereo (au centre : 1 et 1)
    const Sample* sample = nullptr;          // fichier joue (nullptr = son de synthese)
    double pos = 0, step = 1;                // lecture du fichier
    bool looping = false;                    // boucle : de loopStart a loopEnd tant que la note tient
    bool latched = false;                    // Loop SP : le filet « plus aucun pad tenu » ne l'arrete pas
    bool autoSustain = false;                // note tenue sans boucle reglee : tenue longue (piano)
    bool wrapped = false;                    // la boucle a deja fait un tour
    double sustainGain = 1;                  // tenue longue : descend lentement (comme une corde)
    double drumRatio = 1;                    // batterie de synthese : 1 = do 4 (hauteur d'origine)
    double loopStart = 0, loopEnd = 0, xfade = 0;   // en echantillons du fichier

    void start (const PadEvent& e, const Sample* smp = nullptr, double sr = 48000, LoopInfo loop = {})
    {
        note = e.note >= 0 ? e.note : defaultNote (e.pad);
        pad = e.pad; tag = e.tag; active = true; phase = 0; time = 0; fade = 1; fadeStep = 0;
        sample = smp;
        kind = e.synth >= 1 && e.synth <= 6 ? e.synth : (e.pad <= 4 ? e.pad : 5);
        const float p = juce::jlimit (0, 127, e.pan) / 127.0f;
        gainL = std::min (1.0f, 2.0f * (1.0f - p));
        gainR = std::min (1.0f, 2.0f * p);
        pos = 0;
        step = smp ? smp->rate / sr * std::pow (2.0, (note - smp->root) / 12.0) : 1;
        // volume par voix : laisse de la marge a plusieurs notes (un sample est deja mixe)
        amp = (smp ? 0.8 : 0.35) * e.velocity / 127.0;
        adsr = e.mode == 1;
        held = adsr;
        gate = adsr && e.gateMs > 0 ? e.gateMs / 1000.0 : 0;
        latched = e.gateMs < 0;                  // Loop SP : ne s'arrete que sur sa fin de note
        releaseTime = juce::jlimit (0.005, 5.0, e.releaseMs / 1000.0);
        drumRatio = std::pow (2.0, (e.note - 60) / 12.0);   // batterie de synthese : hauteur jouee
        // Boucle : seulement pour un sample tenu (mode ADSR), et assez longue (>= 5 ms)
        looping = false;
        // Note tenue sans boucle reglee : sustain automatique, comme un synthe. Le sample se lit
        // normalement, puis une boucle douce sur sa deuxieme moitie tient le son jusqu'au
        // relachement (le fichier ne s'arrete plus au bout de 2-3 s).
        autoSustain = smp && adsr && ! loop.on && smp->data.getNumSamples() > 0.2 * smp->rate;
        wrapped = false;
        sustainGain = 1;
        if (autoSustain)
            loop = { true, 0.55f, 0.95f };
        if (smp && adsr && loop.on)
        {
            const double last = smp->data.getNumSamples() - 2;
            loopStart = juce::jlimit (0.0, last, (double) loop.start * last);
            loopEnd = juce::jlimit (0.0, last, (double) loop.end * last);
            if (loopEnd - loopStart >= 0.005 * smp->rate)
            {
                looping = true;
                // fondu enchaine au raccord : 10 ms (sustain automatique : 60 ms, plus doux),
                // au plus un quart de la boucle
                xfade = std::min ((autoSustain ? 0.060 : 0.010) * smp->rate, (loopEnd - loopStart) / 4);
            }
        }
    }

    void release (double sr)          // s'eteindre en 10 ms, sans clic
    {
        if (fadeStep <= 0) fadeStep = 1.0 / (0.010 * sr);
    }

    void noteOff (double sr)          // ADSR : fin de note, relachement
    {
        if (! held) return;
        held = false;
        fadeStep = 1.0 / (releaseTime * sr);
    }

    float render (double sr)
    {
        const double dt = 1.0 / sr;
        float s = 0;
        double level = 1;   // niveau de l'enveloppe principale, pour savoir quand la voix s'eteint
        auto env = [this, &level] (double decay) { level = std::exp (-time / decay); return level; };
        if (sample != nullptr)
        {   // sample : lecture avec interpolation, la voix s'arrete a la fin du fichier
            const auto& data = sample->data;
            auto readAt = [&data] (double p)
            {
                const int i0 = (int) p;
                const float frac = (float) (p - i0);
                float v = 0;
                for (int ch = 0; ch < data.getNumChannels(); ++ch)
                {
                    const float* d = data.getReadPointer (ch);
                    v += d[i0] + (d[i0 + 1] - d[i0]) * frac;
                }
                return v / (float) data.getNumChannels();
            };
            if (looping)
            {
                // Boucle : a la fin, on repart juste apres le debut ; les derniers xfade
                // echantillons se melangent avec le debut de la boucle (pas de clic au raccord)
                if (pos >= loopEnd)
                {
                    pos = loopStart + xfade + (pos - loopEnd);
                    wrapped = true;
                }
                s = readAt (pos);
                if (xfade > 0 && pos > loopEnd - xfade)
                {
                    const double a = (loopEnd - pos) / xfade;
                    s = (float) (s * a + readAt (loopStart + (pos - (loopEnd - xfade))) * (1 - a));
                }
                if (autoSustain && wrapped)
                {
                    // Tenue longue : apres la fin du fichier, le son continue en s'eteignant
                    // doucement (-60 dB en 12 s environ), comme une note de piano tenue
                    sustainGain *= std::exp (-dt / 1.75);
                    s *= (float) sustainGain;
                    if (sustainGain < 0.001) active = false;
                }
                pos += step;
            }
            else
            {
                if ((int) pos + 1 >= data.getNumSamples())
                {
                    active = false;
                    return 0;
                }
                s = readAt (pos);
                // fin du fichier : fondu de 5 ms (un fichier coupe net fait un petit clic)
                const double left = (data.getNumSamples() - 1 - pos) / sample->rate / step;
                if (left < 0.005)
                    s *= (float) std::max (0.0, left / 0.005);
                pos += step;
            }
        }
        else switch (kind)
        {
            case 1: { // grosse caisse : sinus qui descend de 150 a 45 Hz (do 4 ; suit la note jouee)
                const double f = (45 + 105 * std::exp (-time / 0.04)) * drumRatio;
                phase += 2 * juce::MathConstants<double>::pi * f * dt;
                s = (float) (std::sin (phase) * env (0.35));
                break;
            }
            case 2: { // caisse claire : bruit + corps a 180 Hz
                phase += 2 * juce::MathConstants<double>::pi * 180 * drumRatio * dt;
                s = (float) (std::sin (phase) * 0.5 * env (0.08)
                             + (noise.nextFloat() * 2 - 1) * 0.7 * env (0.15));
                break;
            }
            case 3:   // charleston ferme
            case 4: { // charleston ouvert
                const float n = noise.nextFloat() * 2 - 1;
                const float hp = n - hpState; hpState = n;   // filtre passe-haut tres simple
                s = (float) (hp * 0.5 * env (kind == 3 ? 0.04 : 0.3));
                break;
            }
            case 6: { // metronome : bip court a la hauteur de la note
                const double f = 440.0 * std::pow (2.0, (note - 69) / 12.0);
                phase += 2 * juce::MathConstants<double>::pi * f * dt;
                s = (float) (std::sin (phase) * env (0.018));
                break;
            }
            default: { // notes
                const double f = 440.0 * std::pow (2.0, (note - 69) / 12.0);
                phase += 2 * juce::MathConstants<double>::pi * f * dt;
                // Note tenue : descend vers 50 %, puis s'eteint lentement (environ 15 s) comme un piano
                const double e = adsr ? (level = (0.5 + 0.5 * std::exp (-time / 0.3)) * std::exp (-time / 2.2)) : env (0.6);
                s = (float) ((std::sin (phase) + 0.3 * std::sin (2 * phase)) * 0.6 * e);
                break;
            }
        }
        // Attaque : le son ne demarre jamais d'un coup (sinon "clic").
        // 1 ms pour garder le punch de la batterie, 5 ms pour les notes.
        const double attack = std::min (1.0, time / (sample ? 0.0005 : kind <= 4 || kind == 6 ? 0.001 : 0.005));
        // La voix s'arrete quand elle est devenue inaudible (plus de coupure franche)
        if (time > 0.01 && level < 0.0005) active = false;
        if (held && gate > 0 && time >= gate) noteOff (sr);       // fin de note programmee
        if (held && time > 30) noteOff (sr);                      // securite : note oubliee
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
    Engine (EventQueue& q, const SampleBank& b) : queue (q), bank (b) {}

    void audioDeviceAboutToStart (juce::AudioIODevice* device) override
    {
        sampleRate = device->getCurrentSampleRate();
        bufferSize = device->getCurrentBufferSizeSamples();
        outputLatency = device->getOutputLatencyInSamples();
        // Sorties qui recoivent le son : 1-2 (Main), et la prise casque si elle a ses propres
        // sorties (Maschine+ : 3-4 "Headphone"). La MK3 recopie deja Main dans le casque.
        const auto names = device->getOutputChannelNames();
        const auto active = device->getActiveOutputChannels();
        const bool copiesItself = device->getName().containsIgnoreCase ("MK3");
        int slot = 0;
        for (int ch = 0; ch < names.size() && slot < kMaxOut; ++ch)
        {
            if (! active[ch])
                continue;
            const bool phones = names[ch].containsIgnoreCase ("headphone")
                             || names[ch].containsIgnoreCase ("phones");
            playOn[slot++] = ch < 2 || (phones && ! copiesItself);
        }
        for (; slot < kMaxOut; ++slot)
            playOn[slot] = false;
    }
    void audioDeviceStopped() override {}

    void audioDeviceIOCallbackWithContext (const float* const*, int,
                                           float* const* out, int numOut, int numSamples,
                                           const juce::AudioIODeviceCallbackContext&) override
    {
        PadEvent e;
        while (queue.pop (e))
        {
            if (e.pad == kLiveOff)
            {   // plus aucun pad tenu : toute note encore "tenue" sans duree est relachee
                for (auto& v : voices)
                    if (v.active && v.held && v.gate <= 0 && ! v.latched) v.noteOff (sampleRate);
                continue;
            }
            if (e.pad == kPanic)
            {
                for (auto& v : voices)
                    if (v.active) v.release (sampleRate);   // fondu de 10 ms, sans clic
                continue;
            }
            if (e.off)
            {
                for (auto& v : voices)                   // ADSR : fin des notes de ce son
                    if (v.active && v.pad == e.pad && v.tag == e.tag
                        && (e.note < 0 || v.note == e.note))
                        v.noteOff (sampleRate);
                continue;
            }
            trigger (e);
            // Temps passe dans le programme entre la lecture USB et le depart du son
            const auto waited = juce::Time::highResolutionTicksToSeconds (
                                    juce::Time::getHighResolutionTicks() - e.ticks) * 1000.0;
            lastPad.store (e.pad);
            lastVelocity.store (e.velocity);
            lastWaitMs.store (waited);
            hits.fetch_add (1);
        }
        std::array<float, 16> groupPeak {};
        std::array<float, 48> voicePeak {};             // crete de chaque voix (vumetres des sons)
        float masterPeak = 0;
        for (int i = 0; i < numSamples; ++i)
        {
            float left = 0, right = 0;
            std::array<float, 16> group {};              // vumetres : somme par groupe
            for (size_t k = 0; k < voices.size(); ++k)
            {
                auto& v = voices[k];
                if (v.active)
                {
                    const float s = v.render (sampleRate);
                    left += s * v.gainL;
                    right += s * v.gainR;
                    if (v.tag >= 0 && v.tag < 16)
                        group[(size_t) v.tag] += s;
                    voicePeak[k] = std::max (voicePeak[k], std::abs (s));   // vumetre du son
                }
            }
            for (size_t g = 0; g < 16; ++g)
                groupPeak[g] = std::max (groupPeak[g], std::abs (group[g]));
            left = std::tanh (left);                     // securite si beaucoup de notes s'empilent
            right = std::tanh (right);
            masterPeak = std::max (masterPeak, std::max (std::abs (left), std::abs (right)));
            for (int ch = 0; ch < numOut; ++ch)          // sorties paires = gauche, impaires = droite
                if (out[ch] != nullptr)
                    out[ch][i] = (ch < kMaxOut && playOn[ch]) ? (ch % 2 == 0 ? left : right) : 0.0f;
        }
        // Crete du bloc : gardee jusqu'a ce que le fil principal l'envoie a l'interface
        for (size_t g = 0; g < 16; ++g)
            raiseMeter (meters[g], groupPeak[g]);
        raiseMeter (meters[16], masterPeak);
        for (size_t k = 0; k < voices.size(); ++k)       // sons : groupe x 16 + pad - 1
            if (voicePeak[k] > 0 && voices[k].tag >= 0 && voices[k].tag < 16 && voices[k].pad >= 1 && voices[k].pad <= 16)
                raiseMeter (soundMeters[(size_t) (voices[k].tag * 16 + voices[k].pad - 1)], voicePeak[k]);
    }
    // Vumetres des sons (vue Sounds) : 16 groupes x 16 sons
    std::array<std::atomic<float>, 256> soundMeters {};

    // Vumetres : groupes A-H, A2-H2 puis master. Lus (et remis a zero) par le fil principal.
    std::array<std::atomic<float>, 17> meters {};
    static void raiseMeter (std::atomic<float>& m, float v)
    {
        float old = m.load (std::memory_order_relaxed);
        while (v > old && ! m.compare_exchange_weak (old, v, std::memory_order_relaxed)) {}
    }

    double sampleRate = 48000;
    int bufferSize = 0, outputLatency = 0;
    static constexpr int kMaxOut = 32;
    bool playOn[kMaxOut] = { true, true };       // sorties qui recoivent le son
    std::atomic<int> hits { 0 }, lastPad { 0 }, lastVelocity { 0 };
    std::atomic<double> lastWaitMs { 0 };

private:
    EventQueue& queue;
    const SampleBank& bank;
    // Un pad rejoue (meme note) fait s'eteindre son son precedent en 10 ms (fondu),
    // comme sur une Maschine. 48 voix laissent la place a ces fondus.
    std::array<Voice, 48> voices {};

    void trigger (const PadEvent& e)
    {
        // Polyphonie du son (pad + groupe) : au-dela de `poly` notes, les plus anciennes
        // s'eteignent (10 ms, sans clic). poly = 1 : mono, la note d'avant s'arrete.
        const int poly = juce::jlimit (1, 16, e.poly);
        for (;;)
        {
            int count = 0;
            Voice* oldest = nullptr;
            for (auto& v : voices)
                if (v.active && v.pad == e.pad && v.tag == e.tag && v.fadeStep <= 0)
                {
                    ++count;
                    if (oldest == nullptr || v.time > oldest->time) oldest = &v;
                }
            if (count < poly || oldest == nullptr)
                break;
            oldest->release (sampleRate);
        }
        Voice* chosen = nullptr;
        for (auto& v : voices)
            if (! v.active) { chosen = &v; break; }
        if (chosen == nullptr)                       // toutes occupees : la plus ancienne
            for (auto& v : voices)
                if (chosen == nullptr || v.time > chosen->time) chosen = &v;
        LoopInfo loop = bank.loop (e.tag, e.pad);
        if (bank.reversed (e.tag, e.pad))                 // a l'envers : la boucle aussi
            loop = { loop.on, 1.0f - loop.end, 1.0f - loop.start };
        chosen->start (e, bank.play (e.tag, e.pad), sampleRate, loop);
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
// Reglages demandes par l'interface : appliques par le fil principal (ouverture de peripheriques)
struct AudioRequest { juce::String output, input; int sampleRate = 0, buffer = 0; };
struct MidiRequest { juce::String input, output; int clock = 0; bool transport = false, thru = false; };

struct SampleRequest { int tag = 0, pad = 1, root = 60; juce::String path; };
struct LoopRequest { int tag = 0, pad = 1; LoopInfo loop; };
struct ReverseRequest { int tag = 0, pad = 1; bool on = false; };

struct Requests
{
    std::mutex lock;
    std::optional<AudioRequest> audio;
    std::optional<MidiRequest> midi;
    std::vector<SampleRequest> samples;
    std::vector<LoopRequest> loops;
    std::vector<ReverseRequest> reverses;
    std::vector<std::pair<int, int>> peaks;              // forme d'onde demandee (groupe, son)
};

// ----------------------------------------------------------------------------
// Mode --osc : les notes arrivent de mk3_live.py (message /fuzzy/proto/hit son velocite note).
// Le rappel "temps reel" les transmet tout de suite au moteur, sans attendre l'interface.
class OscInput : private juce::OSCReceiver::Listener<juce::OSCReceiver::RealtimeCallback>
{
public:
    OscInput (EventQueue& q, Requests& r, MidiBridge& m) : queue (q), requests (r), midi (m) {}
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
        const auto str = [&m] (int i) { return i < m.size() && m[i].isString() ? m[i].getString() : juce::String(); };
        const auto num = [&m] (int i) { return i < m.size() && m[i].isInt32() ? m[i].getInt32() : 0; };
        if (address == "/fuzzy/proto/audio/set")
        {
            std::lock_guard<std::mutex> g (requests.lock);
            requests.audio = AudioRequest { str (0), str (1), num (2), num (3) };
            return;
        }
        if (address == "/fuzzy/proto/midi/set")
        {
            std::lock_guard<std::mutex> g (requests.lock);
            requests.midi = MidiRequest { str (0), str (1), num (2), num (3) != 0, num (4) != 0 };
            return;
        }
        if (address == "/fuzzy/proto/sample/loop")    // groupe, son, boucle (0/1), debut, fin (0-1)
        {
            const auto flt = [&m] (int i, float d) { return i < m.size() && m[i].isFloat32() ? m[i].getFloat32() : d; };
            std::lock_guard<std::mutex> g (requests.lock);
            requests.loops.push_back ({ num (0), num (1), { num (2) != 0, flt (3, 0), flt (4, 1) } });
            return;
        }
        if (address == "/fuzzy/proto/sample/reverse") // groupe, son, a l'envers (0/1)
        {
            std::lock_guard<std::mutex> g (requests.lock);
            requests.reverses.push_back ({ num (0), num (1), num (2) != 0 });
            return;
        }
        if (address == "/fuzzy/proto/sample/peaks")   // groupe, son : forme d'onde pour l'editeur
        {
            std::lock_guard<std::mutex> g (requests.lock);
            requests.peaks.push_back ({ num (0), num (1) });
            return;
        }
        if (address == "/fuzzy/proto/sample")         // groupe, son, fichier ("" = synthese), note d'origine
        {
            std::lock_guard<std::mutex> g (requests.lock);
            requests.samples.push_back ({ num (0), num (1), num (3), str (2) });
            return;
        }
        if (address == "/fuzzy/proto/midi/note")
        {
            midi.noteOut (num (0), num (1), num (2), num (3));
            return;
        }
        if (address == "/fuzzy/proto/midi/off")
        {
            midi.noteOff (num (0), num (1));
            return;
        }
        if (address == "/fuzzy/proto/transport")
        {
            midi.setTransport (num (0) != 0, m.size() > 1 && m[1].isFloat32() ? m[1].getFloat32() : 120.0);
            return;
        }
        if (address == "/fuzzy/proto/off_live")        // plus aucun pad tenu
        {
            queue.push ({ kLiveOff, 0, -1, juce::Time::getHighResolutionTicks() });
            return;
        }
        if (address == "/fuzzy/proto/off")             // fin de note (son, note, groupe)
        {
            const int pad = num (0);
            if (pad < 1 || pad > 16) return;
            PadEvent e { pad, 0, num (1), juce::Time::getHighResolutionTicks() };
            e.tag = m.size() > 2 ? num (2) : -1;
            e.off = true;
            queue.push (e);
            return;
        }
        if (address != "/fuzzy/proto/hit") return;
        // son, velocite, note [, groupe, mode (0 ONE-SHOT / 1 ADSR), relachement ms, duree ms,
        //                      synthe, pan, polyphonie]
        if (m.size() < 3 || ! m[0].isInt32() || ! m[1].isInt32() || ! m[2].isInt32()) return;
        const int pad = m[0].getInt32();
        if (pad < 1 || pad > 16) return;
        PadEvent e { pad, juce::jlimit (1, 127, m[1].getInt32()), m[2].getInt32(),
                     juce::Time::getHighResolutionTicks() };
        if (m.size() >= 7)
        {
            e.tag = num (3);
            e.mode = num (4);
            e.releaseMs = num (5);
            e.gateMs = num (6);
        }
        if (m.size() >= 8)
            e.synth = num (7);
        if (m.size() >= 9)
            e.pan = num (8);
        if (m.size() >= 10)                          // polyphonie du son (sinon 4)
            e.poly = num (9);
        queue.push (e);
    }
    EventQueue& queue;
    Requests& requests;
    MidiBridge& midi;
    juce::OSCReceiver receiver;
};

// Toutes les sorties de la carte sont ouvertes (le moteur choisit ou envoyer le son)
void openAllOutputs (juce::AudioDeviceManager::AudioDeviceSetup& setup)
{
    setup.useDefaultOutputChannels = false;
    setup.outputChannels.clear();
    setup.outputChannels.setRange (0, 32, true);
}

// Applique une demande de reglage audio (fil principal)
void applyAudio (juce::AudioDeviceManager& audio, const AudioRequest& r)
{
    auto setup = audio.getAudioDeviceSetup();
    if (r.output.isNotEmpty())
        setup.outputDeviceName = r.output;
    if (r.sampleRate > 0)
        setup.sampleRate = r.sampleRate;
    if (r.buffer > 0)
        setup.bufferSize = r.buffer;
    openAllOutputs (setup);
    const auto err = audio.setAudioDeviceSetup (setup, true);
    if (err.isNotEmpty())
        std::printf ("Audio : %s\n", err.toRawUTF8());
}

juce::String joinInts (const juce::Array<int>& values)
{
    juce::StringArray parts;
    for (int v : values) parts.add (juce::String (v));
    return parts.joinIntoString (",");
}
} // namespace

// ----------------------------------------------------------------------------
int main (int argc, char* argv[])
{
    int wantedBuffer = 96;   // compromis entre 128 (tres sur) et 64 (tres reactif)
    bool oscMode = false, quiet = false;
    juce::String wantedOutput;               // --output "nom" : sortie audio imposee
    constexpr int kOscPort = 57390;   // port de test des prototypes (hors plage 57300-57306 des apps)
    for (int i = 1; i < argc; ++i)
    {
        const juce::String arg (argv[i]);
        if (arg == "--buffer" && i + 1 < argc) wantedBuffer = juce::String (argv[++i]).getIntValue();
        else if (arg == "--osc") oscMode = true;
        else if (arg == "--quiet") quiet = true;                 // pas de ligne par note
        else if (arg == "--output" && i + 1 < argc) wantedOutput = argv[++i];
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
    SampleBank bank;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();                  // WAV, AIFF, FLAC, Ogg, MP3 (+ formats du Mac : M4A, AAC)
    Engine engine (queue, bank);
    juce::AudioDeviceManager audio;
    const auto err = audio.initialiseWithDefaultDevices (0, 2);
    if (err.isNotEmpty()) { std::printf ("Audio : %s\n", err.toRawUTF8()); return 1; }
    auto setup = audio.getAudioDeviceSetup();
    setup.bufferSize = wantedBuffer;
    // Sortie audio : celle demandee (--output), sinon la sortie par defaut, sauf si c'est la
    // Maschine en mode OSC (ses ecrans pris, sa carte son disparait) : on prend alors une autre.
    {
        juce::StringArray outputs;
        if (auto* type = audio.getCurrentDeviceTypeObject())
        {
            type->scanForDevices();
            outputs = type->getDeviceNames (false);
        }
        juce::String chosen = setup.outputDeviceName;
        if (wantedOutput.isNotEmpty())
        {
            for (auto& name : outputs)
                if (name.containsIgnoreCase (wantedOutput)) { chosen = name; break; }
        }
        // (plus d'evitement de la sortie "Maschine" : en mode partage, sa carte son reste la)
        if (chosen.isNotEmpty())
            setup.outputDeviceName = chosen;
    }
    openAllOutputs (setup);
    audio.setAudioDeviceSetup (setup, true);
    audio.addAudioCallback (&engine);

    auto* device = audio.getCurrentAudioDevice();
    if (device == nullptr)
    {
        std::puts ("Aucune sortie audio disponible : choisis une sortie dans Reglages Systeme > Son.");
        return 1;
    }
    std::printf ("Sortie audio : %s, %.0f Hz, tampon %d echantillons (%.1f ms), "
                 "latence de sortie %.1f ms\n",
                 device->getName().toRawUTF8(), device->getCurrentSampleRate(),
                 device->getCurrentBufferSizeSamples(),
                 device->getCurrentBufferSizeSamples() / device->getCurrentSampleRate() * 1000.0,
                 device->getOutputLatencyInSamples() / device->getCurrentSampleRate() * 1000.0);
    std::puts ("Pads 1-4 : batterie ; pads 5-16 : notes. Ctrl+C pour quitter.");

    // Retour vers l'interface (mk3_live.py ecoute sur le port 57391)
    juce::OSCSender toUi;
    std::mutex toUiLock;
    const bool uiConnected = oscMode && toUi.connect ("127.0.0.1", kOscPort + 1);
    auto sendUi = [&] (const juce::OSCMessage& m)
    {
        if (! uiConnected) return;
        std::lock_guard<std::mutex> g (toUiLock);
        toUi.send (m);
    };
    MidiBridge midi (sendUi);
    Requests requests;

    OscInput osc (queue, requests, midi);
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
    int seen = 0, loops = 0, rescans = 0;
    bool metersWereOn = false, soundMetersWereOn = false;
    while (gRunning.load())
    {
        // Mode OSC : l'appli (ou mk3_live.py) qui nous a lance a disparu (plantage) : on s'arrete,
        // sinon on garderait le port et le moteur suivant ne pourrait pas demarrer
        if (oscMode && ::getppid() == 1)
        {
            std::printf ("Interface fermee : arret du moteur.\n");
            break;
        }
        std::this_thread::sleep_for (std::chrono::milliseconds (50));

        // Reglages demandes par SETTINGS > AUDIO / MIDI
        std::optional<AudioRequest> audioReq;
        std::optional<MidiRequest> midiReq;
        std::vector<SampleRequest> sampleReqs;
        std::vector<LoopRequest> loopReqs;
        std::vector<ReverseRequest> reverseReqs;
        std::vector<std::pair<int, int>> peakReqs;
        {
            std::lock_guard<std::mutex> g (requests.lock);
            loopReqs.swap (requests.loops);
            reverseReqs.swap (requests.reverses);
            peakReqs.swap (requests.peaks);
            audioReq.swap (requests.audio);
            midiReq.swap (requests.midi);
            sampleReqs.swap (requests.samples);
        }
        for (const auto& r : sampleReqs)                 // samples a charger (kit, ou retour a la synthese)
        {
            if (r.path.isEmpty())
            {
                bank.set (r.tag, r.pad, nullptr);
                continue;
            }
            auto sample = loadSample (formats, r.path, r.root);
            if (sample == nullptr)
            {
                std::printf ("Sample illisible : %s\n", r.path.toRawUTF8());
                juce::OSCMessage bad ("/fuzzy/proto/sample/error");
                bad.addString (r.path);
                sendUi (bad);
                continue;
            }
            bank.set (r.tag, r.pad, std::move (sample));
        }
        for (const auto& r : loopReqs)
            bank.setLoop (r.tag, r.pad, r.loop);
        for (const auto& r : reverseReqs)
            bank.setReverse (r.tag, r.pad, r.on);
        for (const auto& [tag, pad] : peakReqs)          // forme d'onde : duree (s) + 256 crêtes
        {
            juce::OSCMessage reply ("/fuzzy/proto/sample/peaks");
            reply.addInt32 (tag);
            reply.addInt32 (pad);
            const Sample* smp = bank.play (tag, pad);
            if (smp == nullptr)
            {
                reply.addFloat32 (0);
                sendUi (reply);
                continue;
            }
            reply.addFloat32 ((float) (smp->data.getNumSamples() / smp->rate));
            for (const float p : samplePeaks (*smp, 256))
                reply.addFloat32 (p);
            sendUi (reply);
        }
        if (audioReq)
        {
            applyAudio (audio, *audioReq);
            if (auto* d = audio.getCurrentAudioDevice())
                std::printf ("Sortie audio : %s, %.0f Hz, tampon %d\n", d->getName().toRawUTF8(),
                             d->getCurrentSampleRate(), d->getCurrentBufferSizeSamples());
            loops = 9;                                    // etat renvoye tout de suite
        }
        if (midiReq)
        {
            midi.configure (midiReq->input, midiReq->output, midiReq->clock, midiReq->transport, midiReq->thru);
            loops = 9;
        }

        // Vumetres, 20 fois par seconde (seulement quand il y a du son, ou pour revenir a zero)
        if (uiConnected)
        {
            juce::OSCMessage mt ("/fuzzy/proto/meters");
            bool any = false;
            for (auto& m : engine.meters)
            {
                const float v = m.exchange (0.0f);
                any |= v > 0.0005f;
                mt.addFloat32 (v);
            }
            if (any || metersWereOn)
                sendUi (mt);
            metersWereOn = any;
            // Les 256 sons (groupe x 16 + pad - 1) : /fuzzy/proto/soundmeters
            juce::OSCMessage ms ("/fuzzy/proto/soundmeters");
            bool anySound = false;
            for (auto& m : engine.soundMeters)
            {
                const float v = m.exchange (0.0f);
                anySound |= v > 0.0005f;
                ms.addFloat32 (v);
            }
            if (anySound || soundMetersWereOn)
                sendUi (ms);
            soundMetersWereOn = anySound;
        }

        // Etat pour l'interface, deux fois par seconde
        if (++loops >= 10 && uiConnected)
        {
            loops = 0;
            juce::StringArray outputs, inputs;
            if (auto* type = audio.getCurrentDeviceTypeObject())
            {
                if (++rescans % 4 == 0)
                    type->scanForDevices();               // carte branchee / debranchee (toutes les 2 s)
                outputs = type->getDeviceNames (false);
                inputs = type->getDeviceNames (true);
            }
            auto* d = audio.getCurrentAudioDevice();
            juce::OSCMessage a ("/fuzzy/proto/audio/state");
            a.addString (d ? d->getName() : juce::String());
            a.addFloat32 (d ? (float) d->getCurrentSampleRate() : 0.0f);
            a.addInt32 (d ? d->getCurrentBufferSizeSamples() : 0);
            a.addFloat32 (d && d->getCurrentSampleRate() > 0
                          ? (float) ((d->getCurrentBufferSizeSamples() + d->getOutputLatencyInSamples())
                                     / d->getCurrentSampleRate() * 1000.0) : 0.0f);
            a.addInt32 (d ? d->getXRunCount() : -1);
            a.addString (outputs.joinIntoString ("\n"));
            juce::Array<int> rates;
            if (d) for (double r : d->getAvailableSampleRates()) rates.add ((int) r);
            a.addString (joinInts (rates));
            a.addString (d ? joinInts (d->getAvailableBufferSizes()) : juce::String());
            a.addString (inputs.joinIntoString ("\n"));
            a.addFloat32 ((float) (audio.getCpuUsage() * 100.0));    // charge du calcul audio (%)
            sendUi (a);

            juce::OSCMessage m ("/fuzzy/proto/midi/state");
            m.addString (MidiBridge::inputNames().joinIntoString ("\n"));
            m.addString (MidiBridge::outputNames().joinIntoString ("\n"));
            m.addString (midi.inputName());
            m.addString (midi.outputName());
            m.addInt32 (midi.inCount.load());
            m.addInt32 (midi.outCount.load());
            m.addFloat32 ((float) midi.receivedBpm());
            sendUi (m);
        }

        const int h = engine.hits.load();
        if (h == seen || quiet) continue;
        seen = h;
        auto* d = audio.getCurrentAudioDevice();
        if (d == nullptr) continue;
        const double sr = d->getCurrentSampleRate();
        const double wait = engine.lastWaitMs.load();
        // Estimation : attente dans le programme + un tampon + latence de sortie de la carte son
        const double total = wait + (d->getCurrentBufferSizeSamples() + d->getOutputLatencyInSamples()) / sr * 1000.0;
        // "decrochages" : nombre de fois ou l'ordinateur n'a pas fourni le son a temps
        // (-1 = information non disponible sur cette carte son)
        std::printf ("PAD %2d  velocite %3d  | dans le programme %4.1f ms | total estime %4.1f ms"
                     " | decrochages %d\n",
                     engine.lastPad.load(), engine.lastVelocity.load(), wait, total,
                     d->getXRunCount());
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
