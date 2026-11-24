// Fuzzy Turtle - prototype : MIDI du moteur audio (SETTINGS > MIDI de mk3_live.py).
//
// - MIDI IN  : les notes recues sont transmises a l'interface (OSC /fuzzy/proto/midi/in),
//              qui choisit le groupe et le son ; option Thru (renvoi vers la sortie).
// - MIDI OUT : notes envoyees par l'interface (/fuzzy/proto/midi/note), avec note-off
//              automatique apres une duree (gate) ou sur demande (/fuzzy/proto/midi/off).
// - Horloge  : Envoyer (24 impulsions par noire + Start/Stop) ou Recevoir (tempo mesure,
//              Start/Stop transmis a l'interface).
// - Port virtuel "Fuzzy" (macOS / Linux) pour parler aux autres logiciels de l'ordinateur.
// Licence : GPLv3 (JUCE utilise sous AGPLv3).
#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_osc/juce_osc.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

class MidiBridge : private juce::MidiInputCallback
{
public:
    static constexpr const char* kVirtual = "Fuzzy (virtual)";
    enum ClockMode { ClockOff = 0, ClockSend = 1, ClockReceive = 2 };

    using Sink = std::function<void (const juce::OSCMessage&)>;

    explicit MidiBridge (Sink toInterface) : toUi (std::move (toInterface))
    {
        worker = std::thread ([this] { run(); });
    }

    ~MidiBridge() override
    {
        running = false;
        if (worker.joinable())
            worker.join();
        configure ({}, {}, ClockOff, false, false);
    }

    static juce::StringArray inputNames()
    {
        juce::StringArray names;
        for (const auto& d : juce::MidiInput::getAvailableDevices())
            if (d.name != "Fuzzy") names.add (d.name);
       #if JUCE_MAC || JUCE_LINUX
        names.add (kVirtual);
       #endif
        return names;
    }

    static juce::StringArray outputNames()
    {
        juce::StringArray names;
        for (const auto& d : juce::MidiOutput::getAvailableDevices())
            if (d.name != "Fuzzy") names.add (d.name);
       #if JUCE_MAC || JUCE_LINUX
        names.add (kVirtual);
       #endif
        return names;
    }

    // A appeler depuis le fil principal (ouvre / ferme les ports). Nom vide = aucun port.
    void configure (const juce::String& inName, const juce::String& outName, int clock, bool transport, bool thru)
    {
        clockMode = clock;
        sendTransport = transport;
        midiThru = thru;
        if (inName != currentIn)
        {
            if (input) input->stop();
            input.reset();
            currentIn = {};
            if (inName == kVirtual)
            {
               #if JUCE_MAC || JUCE_LINUX
                input = juce::MidiInput::createNewDevice ("Fuzzy", this);
               #endif
            }
            else if (inName.isNotEmpty())
                for (const auto& d : juce::MidiInput::getAvailableDevices())
                    if (d.name == inName) { input = juce::MidiInput::openDevice (d.identifier, this); break; }
            if (input) { input->start(); currentIn = inName; }
        }
        if (outName != currentOut)
        {
            std::lock_guard<std::mutex> g (outLock);
            allNotesOffLocked();
            output.reset();
            currentOut = {};
            if (outName == kVirtual)
            {
               #if JUCE_MAC || JUCE_LINUX
                output = juce::MidiOutput::createNewDevice ("Fuzzy");
               #endif
            }
            else if (outName.isNotEmpty())
                for (const auto& d : juce::MidiOutput::getAvailableDevices())
                    if (d.name == outName) { output = juce::MidiOutput::openDevice (d.identifier); break; }
            if (output) currentOut = outName;
        }
    }

    juce::String inputName() const { return currentIn; }
    juce::String outputName() const { return currentOut; }

    // Depuis n'importe quel fil (OSC) : note jouee vers l'exterieur. gateMs = 0 : attendre noteOff.
    void noteOut (int channel, int note, int velocity, int gateMs)
    {
        channel = juce::jlimit (1, 16, channel);
        note = juce::jlimit (0, 127, note);
        std::lock_guard<std::mutex> g (outLock);
        if (! output) return;
        send (juce::MidiMessage::noteOn (channel, note, (juce::uint8) juce::jlimit (1, 127, velocity)));
        if (gateMs > 0)
            pendingOffs.push_back ({ now() + gateMs, channel, note });
    }

    void noteOff (int channel, int note)
    {
        std::lock_guard<std::mutex> g (outLock);
        if (output) send (juce::MidiMessage::noteOff (juce::jlimit (1, 16, channel), juce::jlimit (0, 127, note)));
    }

    // PLAY / STOP et tempo de l'interface (pour Envoyer l'horloge et Start / Stop)
    void setTransport (bool isPlaying, double newBpm)
    {
        bpm = juce::jlimit (20.0, 300.0, newBpm);
        const bool was = playing.exchange (isPlaying);
        if (was == isPlaying || ! sendTransport) return;
        std::lock_guard<std::mutex> g (outLock);
        if (output) send (isPlaying ? juce::MidiMessage::midiStart() : juce::MidiMessage::midiStop());
    }

    double receivedBpm() const { return clockBpm.load(); }
    std::atomic<int> inCount { 0 }, outCount { 0 };   // activite (temoins de l'onglet MIDI)

private:
    struct Off { double due; int channel, note; };

    static double now() { return juce::Time::getMillisecondCounterHiRes(); }

    void send (const juce::MidiMessage& m)              // outLock tenu
    {
        output->sendMessageNow (m);
        outCount.fetch_add (1);
    }

    void allNotesOffLocked()
    {
        if (! output) return;
        for (const auto& o : pendingOffs)
            output->sendMessageNow (juce::MidiMessage::noteOff (o.channel, o.note));
        pendingOffs.clear();
        for (int ch = 1; ch <= 16; ++ch)
            output->sendMessageNow (juce::MidiMessage::allNotesOff (ch));
    }

    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m) override
    {
        inCount.fetch_add (1);
        if (midiThru && ! m.isMidiClock())
        {
            std::lock_guard<std::mutex> g (outLock);
            if (output) send (m);
        }
        if (m.isMidiClock())
        {
            const double t = now();
            if (ticks.size() >= 25) ticks.erase (ticks.begin());
            ticks.push_back (t);
            if (++tickCount % 24 == 0 && ticks.size() == 25)
            {
                const double beatMs = ticks.back() - ticks.front();
                if (beatMs > 0)
                {
                    clockBpm = 60000.0 / beatMs;
                    juce::OSCMessage msg ("/fuzzy/proto/clock/in");
                    msg.addFloat32 ((float) clockBpm.load());
                    toUi (msg);
                }
            }
            return;
        }
        if (m.isMidiStart() || m.isMidiContinue() || m.isMidiStop())
        {
            juce::OSCMessage msg ("/fuzzy/proto/transport/in");
            msg.addInt32 (m.isMidiStop() ? 0 : 1);
            toUi (msg);
            return;
        }
        if (m.isNoteOnOrOff())
        {
            juce::OSCMessage msg ("/fuzzy/proto/midi/in");
            msg.addInt32 (m.getChannel());
            msg.addInt32 (m.getNoteNumber());
            msg.addInt32 (m.isNoteOn() ? (int) m.getVelocity() : 0);
            toUi (msg);
        }
    }

    // Fil de travail (1 ms) : impulsions d'horloge et note-off programmes
    void run()
    {
        double nextTick = now();
        while (running)
        {
            std::this_thread::sleep_for (std::chrono::microseconds (500));
            const double t = now();
            std::lock_guard<std::mutex> g (outLock);
            if (! output)
            {
                nextTick = t;
                continue;
            }
            if (clockMode == ClockSend)
            {
                const double interval = 60000.0 / (bpm.load() * 24.0);
                if (t >= nextTick)
                {
                    send (juce::MidiMessage::midiClock());
                    nextTick += interval;
                    if (t - nextTick > interval * 4) nextTick = t + interval;   // rattrapage
                }
            }
            else
                nextTick = t;
            for (size_t i = 0; i < pendingOffs.size();)
                if (pendingOffs[i].due <= t)
                {
                    send (juce::MidiMessage::noteOff (pendingOffs[i].channel, pendingOffs[i].note));
                    pendingOffs.erase (pendingOffs.begin() + (long) i);
                }
                else
                    ++i;
        }
    }

    Sink toUi;
    std::mutex outLock;
    std::unique_ptr<juce::MidiInput> input;
    std::unique_ptr<juce::MidiOutput> output;
    juce::String currentIn, currentOut;
    std::vector<Off> pendingOffs;
    std::atomic<int> clockMode { ClockOff };
    std::atomic<bool> sendTransport { false }, midiThru { false }, playing { false }, running { true };
    std::atomic<double> bpm { 120.0 }, clockBpm { 0.0 };
    std::vector<double> ticks;                          // fil MIDI entrant seulement
    int tickCount = 0;
    std::thread worker;
};
