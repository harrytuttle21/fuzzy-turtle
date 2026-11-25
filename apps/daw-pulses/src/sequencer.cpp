// Fuzzy John DAW Pulses — sequenceur. Licence : GPLv3.
#include "sequencer.h"

#include <algorithm>
#include <cmath>

namespace seq {

std::vector<Hit> Sequencer::hitsForRow (int row, double rowMs) const
{
    std::vector<Hit> out;
    std::lock_guard<std::mutex> g (lock);
    // Toutes les notes de la case (accord compris)
    auto add = [&out, rowMs] (int grp, int t, const Cell& c, double delay)
    {
        const int gate = (int) (rowMs * (c.length - 0.1));
        out.push_back ({ grp, t, c.note, c.velocity, gate, delay });
        for (int n : c.extra)
            if (n >= 0)
                out.push_back ({ grp, t, n, c.velocity, gate, delay });
    };
    for (int grp = 0; grp < session::kGroups; ++grp)
    {
        if (off[(size_t) grp])
            continue;
        const Pattern& p = patterns[(size_t) grp];
        const int r = row % p.length, next = (row + 1) % p.length;
        for (int t = 0; t < kTracks; ++t)
        {
            const Cell& c = p.rows[(size_t) r][(size_t) t];        // a l'heure ou en retard
            if (! c.empty() && c.offset >= 0)
                add (grp, t, c, c.offset / double (kTiming));
            const Cell& e = p.rows[(size_t) next][(size_t) t];     // ligne suivante, en avance
            if (! e.empty() && e.offset < 0)
                add (grp, t, e, 1.0 + e.offset / double (kTiming));
        }
    }
    return out;
}

Sequencer::Written Sequencer::record (int group, int track, int note, int velocity, double rowPos, bool merge)
{
    std::lock_guard<std::mutex> g (lock);
    Pattern& p = patterns[(size_t) group];
    // Ligne la plus proche : 2e moitie d'une ligne -> ligne suivante, en avance
    int row = (int) std::floor (rowPos);
    int offset = (int) std::lround ((rowPos - row) * kTiming);
    if (offset > kTiming / 2)
    {
        ++row;
        offset -= kTiming;
    }
    row = ((row % p.length) + p.length) % p.length;
    Cell& c = p.rows[(size_t) row][(size_t) track];
    if (merge && ! c.empty())                        // accord joue : les notes vont dans la meme case
    {
        if (c.note == note)
            return { group, row, track };
        for (int& e : c.extra)
        {
            if (e == note)
                break;
            if (e < 0)
            {
                e = note;
                break;
            }
        }
        return { group, row, track };
    }
    c = { note, velocity, 1, offset };
    return { group, row, track };
}

void Sequencer::setLength (const Written& w, double rows)
{
    if (w.group < 0)
        return;
    std::lock_guard<std::mutex> g (lock);
    Cell& c = patterns[(size_t) w.group].rows[(size_t) w.row][(size_t) w.track];
    if (! c.empty())
        c.length = std::clamp ((int) std::lround (rows), 1, kMaxRows);
}

Pattern Sequencer::pattern (int group) const
{
    std::lock_guard<std::mutex> g (lock);
    return patterns[(size_t) group];
}

void Sequencer::setCell (int group, int row, int track, const Cell& c)
{
    std::lock_guard<std::mutex> g (lock);
    patterns[(size_t) group].rows[(size_t) row][(size_t) track] = c;
}

void Sequencer::setPatternLength (int group, int length)
{
    std::lock_guard<std::mutex> g (lock);
    patterns[(size_t) group].length = std::clamp (length, 1, kMaxRows);
}

int Sequencer::patternLength (int group) const
{
    std::lock_guard<std::mutex> g (lock);
    return patterns[(size_t) group].length;
}

void Sequencer::clear (int group)
{
    std::lock_guard<std::mutex> g (lock);
    for (auto& row : patterns[(size_t) group].rows)
        row.fill ({});
}

void Sequencer::setGroupOff (int group, bool isOff)
{
    std::lock_guard<std::mutex> g (lock);
    off[(size_t) group] = isOff;
}

bool Sequencer::groupOff (int group) const
{
    std::lock_guard<std::mutex> g (lock);
    return off[(size_t) group];
}

void Sequencer::setPattern (int group, const Pattern& p)
{
    std::lock_guard<std::mutex> g (lock);
    patterns[(size_t) group] = p;
}

} // namespace seq
