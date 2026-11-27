// Fuzzy John DAW Pulses — sequenceur. Licence : GPLv3.
#include "sequencer.h"

#include <algorithm>
#include <cmath>

namespace seq {

Sequencer::Sequencer()
{
    reset();
}

void Sequencer::reset()
{
    std::lock_guard<std::mutex> g (lock);
    for (auto& group : store)
    {
        for (auto& slot : group)
            slot.reset();
        group[0] = std::make_unique<Pattern>();       // chaque groupe a au moins son pattern 1
    }
    for (auto& group : names)
        group.fill ({});
    current.fill (0);
    queue.fill (kNoQueue);
    start.fill (0);
    off.fill (true);                                 // aucun pattern actif au depart : on active en jouant
    scenes.fill (Scene());
    entries.clear();                                 // morceau vide
    loopA = loopB = -1;
    songList = { SongSlot() };
    songIndex = 0;
    songJump = -1;
    songOn = false;
    loopOn = true;
    songEnded = false;
    songPos = songRows = songStart = 0;
    sceneNow = -1;
}

std::vector<Hit> Sequencer::hitsForRow (int row, double rowMs)
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
    // SONG allume (ou saut demande) pendant la lecture : on attend le debut de la mesure
    const bool waiting = songOn && songJump >= 0 && row % kRowsPerBar != 0;
    const bool songPlays = songOn && ! entries.empty() && ! waiting;
    if (songPlays && songJump >= 0)                  // saut demande : en debut de mesure
    {
        songPos = std::clamp (songJump, 0, (int) entries.size() - 1);
        songRows = 0;
        songJump = -1;
    }
    if (songPlays)                                   // morceau : debut d'une entree ?
    {
        if (songRows == 0 || songPos >= (int) entries.size())
        {
            if (songPos >= (int) entries.size())
            {
                if (! loopOn)
                {
                    songEnded = true;                // fin : l'horloge s'arrete
                    songPos = songStart;
                    songRows = 0;
                    return out;
                }
                songPos = 0;
            }
            startEntry (row);
        }
    }
    for (int grp = 0; grp < session::kGroups; ++grp)
    {
        const size_t gi = (size_t) grp;
        // Debut du pattern : le pattern demande (ou le silence) prend la place
        if (queue[gi] != kNoQueue && ((row - start[gi]) % cur (grp).length + cur (grp).length) % cur (grp).length == 0)
        {
            if (queue[gi] == kOff)
                off[gi] = true;
            else
            {
                current[gi] = queue[gi];
                off[gi] = false;
            }
            start[gi] = row;
            queue[gi] = kNoQueue;
        }
        if (off[gi])
            continue;
        const Pattern& p = cur (grp);
        const int local = ((row - start[gi]) % p.length + p.length) % p.length;
        const int r = local, next = (local + 1) % p.length;
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
    if (songPlays && ++songRows >= songLen)          // entree finie : la suivante
    {
        ++songPos;
        songRows = 0;
        if (loopOn && loopA >= 0 && loopB < (int) entries.size() && songPos > loopB)
            songPos = loopA;                         // Loop sur la zone choisie
    }
    return out;
}

// ------------------------------------------------------------ morceau (Song)

void Sequencer::startEntry (int row)
{
    const SongEntry& e = entries[(size_t) songPos];
    if (e.scene >= 0 && e.scene < kPatternSlots && scenes[(size_t) e.scene].used)
    {
        const Scene& s = scenes[(size_t) e.scene];
        for (int grp = 0; grp < session::kGroups; ++grp)
        {
            const size_t gi = (size_t) grp;
            const int v = s.pattern[gi];
            if (v == kFree)                          // libre : il garde son pattern
                continue;
            queue[gi] = kNoQueue;
            if (v == kOff)
                off[gi] = true;
            else
            {
                if (! store[gi][(size_t) v])
                    store[gi][(size_t) v] = std::make_unique<Pattern>();
                current[gi] = v;
                off[gi] = false;
            }
            start[gi] = row;                         // tous ensemble, depuis le debut
        }
        sceneNow = e.scene;
    }
    songLen = std::max (1, entryRowsLocked (e));
    songRows = 0;
}

int Sequencer::sceneRowsLocked (int scene) const
{
    if (scene < 0 || scene >= kPatternSlots || ! scenes[(size_t) scene].used)
        return kRowsPerBar;
    int rows = 0;
    for (int grp = 0; grp < session::kGroups; ++grp)
    {
        const int v = scenes[(size_t) scene].pattern[(size_t) grp];
        if (v >= 0 && store[(size_t) grp][(size_t) v])
            rows = std::max (rows, store[(size_t) grp][(size_t) v]->length);
    }
    return rows > 0 ? rows : kRowsPerBar;
}

int Sequencer::entryRowsLocked (const SongEntry& e) const
{
    if (e.bars > 0)
        return (int) std::lround (e.bars * kRowsPerBar);
    return std::max (1, (int) std::lround (sceneRowsLocked (e.scene) * e.repeats));
}

int Sequencer::sceneRows (int scene) const
{
    std::lock_guard<std::mutex> g (lock);
    return sceneRowsLocked (scene);
}

int Sequencer::entryRows (const SongEntry& e) const
{
    std::lock_guard<std::mutex> g (lock);
    return entryRowsLocked (e);
}

std::vector<SongEntry> Sequencer::song() const
{
    std::lock_guard<std::mutex> g (lock);
    return entries;
}

void Sequencer::setSong (const std::vector<SongEntry>& list)
{
    std::lock_guard<std::mutex> g (lock);
    entries = list;
    if ((int) entries.size() > kSongMax)
        entries.resize ((size_t) kSongMax);
    songStart = std::clamp (songStart, 0, std::max (0, (int) entries.size() - 1));
    if (loopB >= (int) entries.size())               // zone de boucle hors du morceau : effacee
        loopA = loopB = -1;
    if (songPos > (int) entries.size())
        songPos = (int) entries.size();
}

bool Sequencer::songMode() const
{
    std::lock_guard<std::mutex> g (lock);
    return songOn;
}

void Sequencer::setSongMode (bool on)
{
    std::lock_guard<std::mutex> g (lock);
    if (on && ! songOn)                              // le morceau reprend a l'entree choisie,
        songJump = songStart;                        // au debut de la mesure (sans couper)
    if (! on)
        songJump = -1;
    songOn = on;
}

bool Sequencer::songLoop() const
{
    std::lock_guard<std::mutex> g (lock);
    return loopOn;
}

void Sequencer::setSongLoop (bool on)
{
    std::lock_guard<std::mutex> g (lock);
    loopOn = on;
}

void Sequencer::songStartFrom (int entry, bool atBar)
{
    std::lock_guard<std::mutex> g (lock);
    songStart = std::clamp (entry, 0, std::max (0, (int) entries.size() - 1));
    if (atBar)                                       // en lecture : a la fin de la mesure, sans a-coup
    {
        songJump = songStart;
        return;
    }
    songJump = -1;
    songPos = songStart;
    songRows = 0;
}

void Sequencer::storeSong()
{
    SongSlot& s = songList[(size_t) songIndex];
    s.entries = entries;
    s.loop = loopOn;
    s.loopFrom = loopA;
    s.loopTo = loopB;
}

void Sequencer::loadSong (int index)
{
    songIndex = std::clamp (index, 0, (int) songList.size() - 1);
    const SongSlot& s = songList[(size_t) songIndex];
    entries = s.entries;
    loopOn = s.loop;
    loopA = s.loopFrom;
    loopB = s.loopTo;
    songStart = songPos = songRows = 0;
    songJump = -1;
}

std::vector<SongSlot> Sequencer::allSongs() const
{
    std::lock_guard<std::mutex> g (lock);
    std::vector<SongSlot> out = songList;
    SongSlot& s = out[(size_t) songIndex];
    s.entries = entries;
    s.loop = loopOn;
    s.loopFrom = loopA;
    s.loopTo = loopB;
    return out;
}

void Sequencer::setAllSongs (const std::vector<SongSlot>& list, int current)
{
    std::lock_guard<std::mutex> g (lock);
    songList = list.empty() ? std::vector<SongSlot> { SongSlot() } : list;
    if ((int) songList.size() > kSongsMax)
        songList.resize ((size_t) kSongsMax);
    loadSong (current);
}

int Sequencer::currentSong() const
{
    std::lock_guard<std::mutex> g (lock);
    return songIndex;
}

void Sequencer::switchSong (int index)
{
    std::lock_guard<std::mutex> g (lock);
    if (index < 0 || index >= (int) songList.size() || index == songIndex)
        return;
    storeSong();
    loadSong (index);
}

int Sequencer::songFrom() const
{
    std::lock_guard<std::mutex> g (lock);
    return songStart;
}

void Sequencer::songPosition (int& entry, int& rowInEntry) const
{
    std::lock_guard<std::mutex> g (lock);
    entry = songPos;
    rowInEntry = songRows;
}

void Sequencer::setSongLoopRange (int from, int to)
{
    std::lock_guard<std::mutex> g (lock);
    if (from < 0 || to < from)
        loopA = loopB = -1;
    else
    {
        loopA = from;
        loopB = to;
    }
}

void Sequencer::songLoopRange (int& from, int& to) const
{
    std::lock_guard<std::mutex> g (lock);
    from = loopA;
    to = loopB;
}

bool Sequencer::takeSongEnd()
{
    std::lock_guard<std::mutex> g (lock);
    const bool was = songEnded;
    songEnded = false;
    return was;
}

Sequencer::Written Sequencer::record (int group, int track, int note, int velocity, double rowPos, bool merge)
{
    std::lock_guard<std::mutex> g (lock);
    Pattern& p = cur (group);
    rowPos -= start[(size_t) group];                 // position dans le pattern du groupe
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
    if (off[(size_t) group] && queue[(size_t) group] == kNoQueue)
        off[(size_t) group] = false;                 // enregistrement : le pattern s'active
    return { group, row, track };
}

void Sequencer::setLength (const Written& w, double rows)
{
    if (w.group < 0)
        return;
    std::lock_guard<std::mutex> g (lock);
    Cell& c = cur (w.group).rows[(size_t) w.row][(size_t) w.track];
    if (! c.empty())
        c.length = std::clamp ((int) std::lround (rows), 1, kMaxRows);
}

Pattern Sequencer::pattern (int group) const
{
    std::lock_guard<std::mutex> g (lock);
    return cur (group);
}

void Sequencer::setCell (int group, int row, int track, const Cell& c)
{
    std::lock_guard<std::mutex> g (lock);
    cur (group).rows[(size_t) row][(size_t) track] = c;
    if (! c.empty() && off[(size_t) group] && queue[(size_t) group] == kNoQueue)
        off[(size_t) group] = false;                 // une note ecrite : le pattern s'active
}

void Sequencer::setPatternLength (int group, int length)
{
    std::lock_guard<std::mutex> g (lock);
    cur (group).length = std::clamp (length, 1, kMaxRows);
}

int Sequencer::patternLength (int group) const
{
    std::lock_guard<std::mutex> g (lock);
    return cur (group).length;
}

void Sequencer::clear (int group)
{
    std::lock_guard<std::mutex> g (lock);
    for (auto& row : cur (group).rows)
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
    cur (group) = p;
}

// ------------------------------------------------------------ patterns

int Sequencer::currentPattern (int group) const
{
    std::lock_guard<std::mutex> g (lock);
    return current[(size_t) group];
}

bool Sequencer::hasPattern (int group, int index) const
{
    std::lock_guard<std::mutex> g (lock);
    return index >= 0 && index < kPatternSlots && store[(size_t) group][(size_t) index] != nullptr;
}

Pattern Sequencer::patternAt (int group, int index) const
{
    std::lock_guard<std::mutex> g (lock);
    if (index < 0 || index >= kPatternSlots || ! store[(size_t) group][(size_t) index])
        return {};
    return *store[(size_t) group][(size_t) index];
}

void Sequencer::setPatternAt (int group, int index, const Pattern& p)
{
    if (index < 0 || index >= kPatternSlots)
        return;
    std::lock_guard<std::mutex> g (lock);
    auto& slot = store[(size_t) group][(size_t) index];
    if (! slot)
        slot = std::make_unique<Pattern>();
    *slot = p;
}

void Sequencer::removePattern (int group, int index)
{
    if (index < 0 || index >= kPatternSlots)
        return;
    std::lock_guard<std::mutex> g (lock);
    auto& slot = store[(size_t) group][(size_t) index];
    names[(size_t) group][(size_t) index].clear();
    if (index == current[(size_t) group])            // le pattern choisi reste, vide
    {
        const int length = slot->length;
        *slot = Pattern();
        slot->length = length;
        return;
    }
    slot.reset();
    if (queue[(size_t) group] == index)
        queue[(size_t) group] = kNoQueue;
}

std::string Sequencer::patternName (int group, int index) const
{
    std::lock_guard<std::mutex> g (lock);
    return index >= 0 && index < kPatternSlots ? names[(size_t) group][(size_t) index] : std::string();
}

void Sequencer::setPatternName (int group, int index, const std::string& name)
{
    if (index < 0 || index >= kPatternSlots)
        return;
    std::lock_guard<std::mutex> g (lock);
    names[(size_t) group][(size_t) index] = name;
}

void Sequencer::choose (int group, int index, bool playing)
{
    const size_t gi = (size_t) group;
    if (index != kOff)
    {
        if (index < 0 || index >= kPatternSlots)
            return;
        if (! store[gi][(size_t) index])                 // emplacement vide : un pattern vide
        {
            store[gi][(size_t) index] = std::make_unique<Pattern>();
            store[gi][(size_t) index]->length = cur (group).length;
        }
    }
    const bool same = index == kOff ? off[gi] : (index == current[gi] && ! off[gi]);
    if (playing && ! same)
        queue[gi] = index;                           // a la fin du pattern en cours
    else
    {
        queue[gi] = kNoQueue;
        if (index == kOff)
            off[gi] = true;
        else
        {
            current[gi] = index;
            off[gi] = false;
        }
    }
}

void Sequencer::selectPattern (int group, int index, bool playing)
{
    std::lock_guard<std::mutex> g (lock);
    choose (group, index, playing);
}

int Sequencer::queuedPattern (int group) const
{
    std::lock_guard<std::mutex> g (lock);
    return queue[(size_t) group];
}

int Sequencer::localRow (int group, int row) const
{
    std::lock_guard<std::mutex> g (lock);
    const int length = cur (group).length;
    return ((row - start[(size_t) group]) % length + length) % length;
}

void Sequencer::resetPlayback()
{
    std::lock_guard<std::mutex> g (lock);
    songPos = songStart;                             // le morceau repart de l'entree choisie
    songRows = 0;
    songJump = -1;
    for (int grp = 0; grp < session::kGroups; ++grp)
    {
        const size_t gi = (size_t) grp;
        if (queue[gi] != kNoQueue)
            choose (grp, queue[gi], false);
        start[gi] = 0;
    }
}

GroupPatterns Sequencer::groupPatterns (int group) const
{
    std::lock_guard<std::mutex> g (lock);
    GroupPatterns gp;
    const size_t gi = (size_t) group;
    for (int i = 0; i < kPatternSlots; ++i)
        if (store[gi][(size_t) i])
        {
            gp.patterns.push_back ({ i, *store[gi][(size_t) i] });
            if (! names[gi][(size_t) i].empty())
                gp.names.push_back ({ i, names[gi][(size_t) i] });
        }
    gp.current = current[gi];
    gp.off = off[gi];
    return gp;
}

void Sequencer::setGroupPatterns (int group, const GroupPatterns& gp)
{
    std::lock_guard<std::mutex> g (lock);
    const size_t gi = (size_t) group;
    for (auto& slot : store[gi])
        slot.reset();
    names[gi].fill ({});
    for (const auto& [i, p] : gp.patterns)
        if (i >= 0 && i < kPatternSlots)
            store[gi][(size_t) i] = std::make_unique<Pattern> (p);
    for (const auto& [i, n] : gp.names)
        if (i >= 0 && i < kPatternSlots)
            names[gi][(size_t) i] = n;
    current[gi] = std::clamp (gp.current, 0, kPatternSlots - 1);
    if (! store[gi][(size_t) current[gi]])
        store[gi][(size_t) current[gi]] = std::make_unique<Pattern>();
    off[gi] = gp.off;
    queue[gi] = kNoQueue;
}

void Sequencer::clearGroupPatterns (int group, int length)
{
    GroupPatterns gp;
    Pattern p;
    p.length = std::clamp (length, 1, kMaxRows);
    gp.patterns.push_back ({ 0, p });
    setGroupPatterns (group, gp);
}

// ------------------------------------------------------------ scenes

Scene Sequencer::scene (int index) const
{
    std::lock_guard<std::mutex> g (lock);
    return index >= 0 && index < kPatternSlots ? scenes[(size_t) index] : Scene();
}

void Sequencer::setScene (int index, const Scene& s)
{
    if (index < 0 || index >= kPatternSlots)
        return;
    std::lock_guard<std::mutex> g (lock);
    scenes[(size_t) index] = s;
    if (! s.used && sceneNow == index)
        sceneNow = -1;
}

std::array<int, session::kGroups> Sequencer::currentCombo() const
{
    std::lock_guard<std::mutex> g (lock);
    std::array<int, session::kGroups> out {};
    for (size_t gi = 0; gi < out.size(); ++gi)
        out[gi] = queue[gi] != kNoQueue ? queue[gi] : off[gi] ? kOff : current[gi];
    return out;
}

void Sequencer::launchScene (int index, bool playing)
{
    std::lock_guard<std::mutex> g (lock);
    if (index < 0 || index >= kPatternSlots || ! scenes[(size_t) index].used)
        return;
    const Scene& s = scenes[(size_t) index];
    for (int grp = 0; grp < session::kGroups; ++grp)
        if (s.pattern[(size_t) grp] != kFree)        // groupe libre : il garde son pattern
            choose (grp, s.pattern[(size_t) grp], playing);
    sceneNow = index;
}

int Sequencer::currentScene() const
{
    std::lock_guard<std::mutex> g (lock);
    return sceneNow;
}

void Sequencer::setCurrentScene (int index)
{
    std::lock_guard<std::mutex> g (lock);
    sceneNow = index;
}

} // namespace seq
