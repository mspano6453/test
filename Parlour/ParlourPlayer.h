#pragma once
#include "ParlourDSP.h"
#include "../Source/BandBus.h"
#include <algorithm>

// ---------------------------------------------------------------------------
// The keys player: turns a chord into a voicing a real player would use
// (close position, smooth voice leading from the last chord, colour notes)
// and plays it in a genre feel, a bar at a time.
// ---------------------------------------------------------------------------
namespace pk
{
enum Feel { KPads = 0, KSoul8, KDetroit, KStabs, KHouse, KBallad, KArp, KLofi, KRock8, KSkank, KBells, KPhillyStr, NumKFeels };
inline const char* feelName (int f)
{
    static const char* n[] = { "Sustained Pads", "Soul Eighths", "Detroit Quarters", "Funk Stabs", "House Piano", "Ballad Arpeggio",
                               "Synth Arp", "Lo-fi Lazy", "Rock Eighths", "Offbeat Skank", "Sparse Bells", "Philly Strings" };
    return (f >= 0 && f < NumKFeels) ? n[f] : "";
}
inline juce::StringArray feelNames() { juce::StringArray a; for (int i = 0; i < NumKFeels; ++i) a.add (feelName (i)); return a; }
inline juce::StringArray instrumentNames() { juce::StringArray a; for (int i = 0; i < NumInstruments; ++i) a.add (instrumentName (i)); return a; }
// feels that hold a chord across the bar (re-struck when the chord changes)
inline bool feelHolds (int f) { return f == KPads || f == KPhillyStr; }

struct KChord { int root = -1; int q = 0; bool valid() const { return root >= 0; } bool operator== (const KChord& o) const { return root == o.root && q == o.q; } };

struct Voicing
{
    int n = 0; int notes[7] {};
    int top() const { return n > 0 ? notes[n - 1] : 60; }
};

// colour: 0 plain triads, ~0.5 sevenths, 1 sevenths + ninths (and the root left to the bass)
inline Voicing voiceChord (KChord c, float colour, int center, const Voicing& prev)
{
    Voicing v;
    if (! c.valid()) return v;
    int tones[4]; int nt = band::qualityTones (c.q, tones);
    std::vector<int> pcs (tones, tones + nt);
    if (colour > 0.4f && nt == 3 && c.q != band::QDim && c.q != band::QSus4)
        pcs.push_back (c.q == band::QMin ? 10 : (colour > 0.7f ? 11 : 2));   // m7, or maj7 / add9 for major chords
    if (colour > 0.72f && c.q != band::QDim && pcs.size() < 5) pcs.push_back (14);   // the ninth on top
    if (colour > 0.88f && pcs.size() > 3) pcs.erase (pcs.begin());                   // rootless, like a jazz/lo-fi player
    const int k = (int) pcs.size();
    // try every octave placement inside the hand's range; keep the smoothest, tidiest shape
    float bestCost = 1e9f; std::vector<int> best;
    const int lo = center - 8, hi = center + 9;
    for (int mask = 0; mask < (1 << k); ++mask)
    {
        std::vector<int> ns;
        for (int i = 0; i < k; ++i)
        {
            int p = c.root + pcs[(size_t) i];
            while (p < lo) p += 12;
            while (p >= lo + 12) p -= 12;
            if ((mask >> i) & 1) p += 12;
            ns.push_back (p);
        }
        std::sort (ns.begin(), ns.end());
        if (ns.back() > hi + 4 || ns.front() < lo - 2) continue;
        float cost = 0;
        const int span = ns.back() - ns.front();
        if (span > 14) cost += (float) (span - 14) * 3.0f;
        for (size_t i = 1; i < ns.size(); ++i) if (ns[i] - ns[i - 1] < 2 && ns[i] < 60) cost += 4.0f;   // mud down low
        for (size_t i = 1; i < ns.size(); ++i) if (ns[i] == ns[i - 1]) cost += 20.0f;
        if (prev.n > 0)
        {
            // voice leading: each new note near some old note, and the top line moving little
            for (int x : ns) { int d = 99; for (int j = 0; j < prev.n; ++j) d = std::min (d, std::abs (x - prev.notes[j])); cost += (float) d; }
            cost += (float) std::abs (ns.back() - prev.top()) * 1.5f;
        }
        else cost += (float) std::abs ((ns.front() + ns.back()) / 2 - center);
        if (cost < bestCost) { bestCost = cost; best = ns; }
    }
    if (best.empty()) { for (int i = 0; i < k; ++i) best.push_back (center - 5 + ((c.root + pcs[(size_t) i]) % 12)); std::sort (best.begin(), best.end()); }
    v.n = std::min (7, (int) best.size());
    for (int i = 0; i < v.n; ++i) v.notes[i] = best[(size_t) i];
    return v;
}

// one hit in a bar: which 16th, how long, how hard, and what part of the hand plays
enum HitKind { HChord = 0, HLeft, HNote, HChordNoLeft, HTopOct };
struct Hit { int step; float len; float vel; int kind; int index = 0; };

// Writes a bar of comping. busy 0..1, section 0 verse 1 chorus 2 bridge
inline void writeBar (int feel, int section, int barInPhrase, float busy, Rng& r, std::vector<Hit>& out)
{
    out.clear();
    const bool chorus = section == 1, bridge = section == 2;
    const float b = juce::jlimit (0.0f, 1.0f, busy + (chorus ? 0.15f : 0.0f) - (bridge ? 0.2f : 0.0f));
    auto add = [&] (int s, float len, float vel, int kind = HChord, int idx = 0) { out.push_back ({ s, len, vel, kind, idx }); };
    switch (feel)
    {
        case KPads:
            add (0, 16.0f, 0.72f);
            if (b > 0.6f && r.chance (b - 0.4f)) add (10, 6.0f, 0.55f, HChordNoLeft);    // a soft re-voice
            break;
        case KSoul8:
            for (int s = 0; s < 16; s += 2)
            {
                if (b < 0.45f && s % 4 == 2) continue;                                     // quarters when it's laid back
                add (s, 1.6f, (s % 8 == 4) ? 0.86f : (s % 4 == 0 ? 0.72f : 0.6f), s % 8 == 0 ? HChord : HChordNoLeft);
            }
            break;
        case KDetroit:
            for (int s = 0; s < 16; s += 4) add (s, 3.0f, s % 8 == 4 ? 0.9f : 0.82f, s % 8 == 0 ? HChord : HChordNoLeft);
            if (b > 0.6f) for (int s = 2; s < 16; s += 8) if (r.chance (b - 0.4f)) add (s + 4, 1.0f, 0.55f, HChordNoLeft);
            break;
        case KStabs:
        {
            static const int spots[][4] = { { 2, 6, 10, 14 }, { 3, 6, 10, 14 }, { 0, 6, 11, 14 }, { 2, 7, 10, 15 } };
            const auto& p = spots[r.i (4)];
            for (int i = 0; i < 4; ++i) if (i == 0 || r.chance (0.5f + b * 0.5f)) add (p[i], 0.8f, i % 2 ? 0.9f : 0.8f, HChordNoLeft);
            if (chorus && r.chance (0.5f)) add (12, 0.7f, 0.85f, HChordNoLeft);
            break;
        }
        case KHouse:
            if (b < 0.5f) { for (int s = 2; s < 16; s += 4) add (s, 1.4f, 0.85f, HChordNoLeft); }
            else { const int p[] = { 0, 3, 6, 10, 12 }; for (int s : p) add (s, 1.6f, s == 0 ? 0.9f : 0.8f, HChordNoLeft); }
            break;
        case KBallad:
        {
            add (0, 8.0f, 0.75f, HLeft);
            if (chorus || barInPhrase % 4 == 0) add (0, 7.5f, 0.7f, HChordNoLeft);
            static const int up[] = { 0, 1, 2, 3, 2, 1, 2, 3 };
            for (int i = 1; i < 8; ++i) if (! (b < 0.35f && i % 2 == 1)) add (i * 2, 3.0f, 0.62f + (i == 4 ? 0.08f : 0.0f), HNote, up[i]);
            add (8, 8.0f, 0.68f, HLeft, 1);
            break;
        }
        case KArp:
        {
            const bool updown = r.chance (0.5f);
            static const int seqA[] = { 0, 1, 2, 3, 4, 5, 6, 7 }, seqB[] = { 0, 1, 2, 3, 4, 3, 2, 1 };
            for (int s = 0; s < 16; ++s)
            {
                if (b < 0.4f && s % 2 == 1) continue;
                add (s, 1.0f, s % 4 == 0 ? 0.82f : 0.62f, HNote, (updown ? seqB : seqA)[s % 8]);
            }
            break;
        }
        case KLofi:
            add (0, 7.0f, 0.62f);
            if (r.chance (0.6f + b * 0.3f)) add (r.chance (0.5f) ? 6 : 7, 3.0f, 0.5f, HChordNoLeft);
            if (b > 0.55f && r.chance (b - 0.3f)) add (11, 4.0f, 0.48f, HChordNoLeft);
            break;
        case KRock8:
            for (int s = 0; s < 16; s += 2) add (s, 1.7f, s % 4 == 0 ? 0.88f : 0.74f, HChordNoLeft);
            for (int s = 0; s < 16; s += 4) add (s, 3.6f, 0.82f, HLeft, (s / 4) % 2);
            break;
        case KSkank:
            for (int s = 2; s < 16; s += 4) add (s, 0.7f, s % 8 == 6 ? 0.85f : 0.78f, HChordNoLeft);
            if (b > 0.6f) for (int s = 3; s < 16; s += 8) if (r.chance (0.5f)) add (s, 0.6f, 0.6f, HChordNoLeft);
            break;
        case KBells:
        {
            static const int pats[][5] = { { 0, 3, 6, 10, -1 }, { 0, 4, 7, 12, 14 }, { 2, 5, 8, 12, -1 } };
            const auto& p = pats[barInPhrase % 2 == 0 ? 0 : 1 + r.i (2)];
            int idx = r.i (3);
            for (int i = 0; i < 5 && p[i] >= 0; ++i) { add (p[i], 2.5f, 0.7f, HNote, 2 + idx); idx = (idx + (r.chance (0.6f) ? 1 : 2)) % 4; }
            break;
        }
        default:  // KPhillyStr
            add (0, 16.0f, 0.7f);
            if (chorus || b > 0.6f) { add (12, 4.0f, 0.75f, HTopOct); }
            if (chorus && r.chance (0.5f)) add (14, 2.0f, 0.72f, HTopOct, 1);
            break;
    }
}

// fills: what the keys do in the bar before a new section
inline void writeFill (int feel, Rng& r, std::vector<Hit>& out)
{
    if (feelHolds (feel)) { out.push_back ({ 12, 4.0f, 0.82f, HTopOct, 0 }); return; }
    // drop the last beat and run up the chord
    out.erase (std::remove_if (out.begin(), out.end(), [] (const Hit& h) { return h.step >= 12; }), out.end());
    const bool big = r.chance (0.5f);
    for (int i = 0; i < 4; ++i) out.push_back ({ 12 + i, 1.0f, 0.7f + 0.06f * (float) i, HNote, big ? i + 2 : i });
}

// recognise a chord from held MIDI keys
inline KChord chordFromNotes (const std::array<bool, 128>& held)
{
    int pcMask = 0, lowest = -1;
    for (int n = 0; n < 128; ++n) if (held[(size_t) n]) { pcMask |= 1 << (n % 12); if (lowest < 0) lowest = n; }
    if (pcMask == 0) return {};
    KChord best; int bestScore = -100;
    for (int root = 0; root < 12; ++root)
        for (int q = 0; q < band::NumQualities; ++q)
        {
            int t[4]; const int nt = band::qualityTones (q, t); int score = 0, tm = 0;
            for (int i = 0; i < nt; ++i) { const int pc = (root + t[i]) % 12; tm |= 1 << pc; score += (pcMask >> pc) & 1 ? 3 : -2; }
            for (int pc = 0; pc < 12; ++pc) if (((pcMask >> pc) & 1) && ! ((tm >> pc) & 1)) score -= 2;
            if (root == lowest % 12) score += 2;
            if (nt == 3) score += 1;     // prefer the simple name when it fits
            if (score > bestScore) { bestScore = score; best = { root, q }; }
        }
    return best;
}
} // namespace pk
