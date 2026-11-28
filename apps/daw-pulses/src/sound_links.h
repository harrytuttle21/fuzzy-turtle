#pragma once
// Fuzzy John DAW Pulses — sons lies et coupures (utilises par les pads, la Maschine et la lecture).
// Licence : GPLv3.
#include "engine_link.h"
#include "session.h"

#include <array>
#include <vector>

namespace links
{
struct Held { int group, sound, note; };            // son lie tenu (fin de note au relachement)

// Le son (g, sound) coupe ses « Cuts » : fondu court dans le moteur, boucles SP arretees
inline bool applyCuts (const EngineLink& engine, session::State& st, int g, int sound)
{
    const auto& m = st.cutMask[(size_t) session::State::index (g, sound)];
    if (! session::State::anyBit (m))
        return false;
    bool spStopped = false;
    for (int j = 0; j < session::kGroups * 16; ++j)
        if (session::State::hasBit (m, j))
        {
            engine.cut (j % 16 + 1, j / 16);
            spStopped |= st.spLatched[(size_t) j].exchange (false);
        }
    return spStopped;
}

// Le son (g, sound) joue aussi ses sons lies (un seul niveau : pas de chaine sans fin).
// rawNote : la note jouee sur le son (avant accord) ; les sons lies gardent le meme ecart.
inline std::vector<Held> playLinks (const EngineLink& engine, const session::State& st, int g, int sound,
                                    int velocity, int rawNote, int gateMs)
{
    std::vector<Held> held;
    const auto& m = st.linkMask[(size_t) session::State::index (g, sound)];
    if (! session::State::anyBit (m))
        return held;
    const int shift = rawNote - session::defaultNote (sound);
    for (int j = 0; j < session::kGroups * 16; ++j)
    {
        if (! session::State::hasBit (m, j) || j == session::State::index (g, sound))
            continue;
        const int tg = j / 16, ts = j % 16 + 1;
        const int v = st.outVelocity (tg, ts, velocity);
        if (v <= 0 || ! st.audible (tg, ts) || ! st.hasInstrument[(size_t) j])
            continue;
        const int note = st.pitched (tg, ts, session::defaultNote (ts) + shift);
        const bool isHeld = st.held (tg, ts);
        engine.hit (ts, v, note, tg, isHeld, st.releaseMs[(size_t) j], gateMs,
                    st.effSynth (tg, ts), st.pan (tg, ts), st.poly (tg, ts));
        if (isHeld && gateMs == 0)
            held.push_back ({ tg, ts, note });
    }
    return held;
}
} // namespace links
