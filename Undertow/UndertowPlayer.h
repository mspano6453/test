#pragma once
#include "UndertowDSP.h"
#include "../Source/BandBus.h"

// ---------------------------------------------------------------------------
// The player: writes 2-bar bass motifs in a late-70s/80s post-punk style
// (pumping picked eighths, high melodic hooks, octave pops, descending gothic
// lines, walk-ups), repeats them like a real bass player does, and lets them
// drift with VARIATION. All pitches are chord-relative so it follows any chord.
// ---------------------------------------------------------------------------
namespace ut
{
enum Feel { FPump = 0, FMelodic, FGothic, FOctaves, FWalk, FRoot, FHalf, FGallop,
            FMotor, FPhilly, FFunk, FTrap, FBoomBap, FHouse, FTechno, FAcid, FNewJack, FSixties, NumFeels };
inline const char* feelName (int f)
{
    static const char* n[] = { "Pump", "Melodic", "Gothic", "Octaves", "Walk", "Root Eighths", "Half-Time Heavy", "Gallop",
                               "Motor City", "Philly", "Funk 16ths", "Trap Sub", "Boom Bap", "House", "Techno", "Acid", "New Jack", "Melodic 60s" };
    return n[f];
}
inline juce::StringArray feelNames() { juce::StringArray a; for (int i = 0; i < NumFeels; ++i) a.add (feelName (i)); return a; }
// the electronic feels keep their groove in the bridge instead of going half-time
inline bool feelIsMachine (int f) { return f == FTrap || f == FHouse || f == FTechno || f == FAcid; }
enum Section { SVerse = 0, SChorus, SBridge, NumSections };

constexpr int kSteps = 32;           // 2 bars of 16ths

struct Step
{
    bool on = false;
    int deg = 0;          // scale degree relative to the chord root (0 root, 2 third, 4 fifth, 7 octave, -1 seventh below)
    int semis = 0;        // extra chromatic offset (walk-ups)
    float vel = 0.8f;
    float len = 1.7f;     // in 16ths
    bool ghost = false;
    bool slide = false;   // synth basses glide into this note from the last one
};
struct Motif { std::array<Step, kSteps> s; };

inline void put (Motif& m, int i, int deg, float vel, float len, int semis = 0, bool ghost = false, bool slide = false)
{
    if (i < 0 || i >= kSteps) return;
    m.s[(size_t) i] = { true, deg, semis, vel, len, ghost, slide };
}

// fill each note's length up to the next note (legato), scaled
inline void legato (Motif& m, float amount)
{
    for (int i = 0; i < kSteps; ++i)
    {
        if (! m.s[(size_t) i].on) continue;
        int j = i + 1;
        while (j < kSteps && ! m.s[(size_t) j].on) ++j;
        m.s[(size_t) i].len = std::max (0.6f, (float) (j - i) * amount);
    }
}

class MotifWriter
{
public:
    static Motif write (int feel, int section, uint32_t seed, float hook, float density)
    {
        Rng r; r.seed (seed * 2654435761u + (uint32_t) feel * 977u + (uint32_t) section * 131u + 7u);
        Motif m;
        switch (feel)
        {
            case FPump:    pump (m, r, section, hook, density); break;
            case FMelodic: melodic (m, r, section, hook, density); break;
            case FGothic:  gothic (m, r, section, hook, density); break;
            case FOctaves: octaves (m, r, section, hook, density); break;
            case FWalk:    walk (m, r, section, hook, density); break;
            case FRoot:    root8 (m, r, section, hook, density); break;
            case FHalf:    halfHeavy (m, r, section, hook, density); break;
            case FMotor:   motorCity (m, r, section, hook, density); break;
            case FPhilly:  philly (m, r, section, hook, density); break;
            case FFunk:    funk (m, r, section, hook, density); break;
            case FTrap:    trap (m, r, section, hook, density); break;
            case FBoomBap: boomBap (m, r, section, hook, density); break;
            case FHouse:   house (m, r, section, hook, density); break;
            case FTechno:  techno (m, r, section, hook, density); break;
            case FAcid:    acid (m, r, section, hook, density); break;
            case FNewJack: newJack (m, r, section, hook, density); break;
            case FSixties: sixties (m, r, section, hook, density); break;
            default:       gallop (m, r, section, hook, density); break;
        }
        if (section == SBridge && ! feelIsMachine (feel)) halfTime (m, r);
        else if (section == SBridge) for (auto& s : m.s) if (s.on && r.chance (0.25f)) s.on = false;   // thin it out a little
        if (section == SChorus) for (auto& s : m.s) if (s.on && ! s.ghost) s.vel = std::min (1.0f, s.vel + 0.07f);
        return m;
    }

    // fills replace the end of bar 2 (the last beat, or the last two beats when big)
    static void fill (Motif& m, Rng& r, int feel, bool big, int barEnd = kSteps)
    {
        // written for the end of bar 2, then shifted to whichever bar end was asked for
        if (barEnd != kSteps)
        {
            Motif t = m;
            for (int i = 0; i < kSteps; ++i) t.s[(size_t) i].on = false;
            fill (t, r, feel, big, kSteps);
            const int shift = kSteps - barEnd;
            for (int i = barEnd - (big ? 8 : 4); i < barEnd; ++i) m.s[(size_t) i] = t.s[(size_t) (i + shift)];
            return;
        }
        const int from = big ? 24 : 28;
        for (int i = from; i < kSteps; ++i) m.s[(size_t) i].on = false;
        if (feelIsMachine (feel))
        {
            // machine fills: an octave stutter or a slide up into the next bar
            if (r.chance (0.5f)) for (int k = 0; k < 4; ++k) put (m, 28 + k, k % 2 ? 7 : 0, 0.8f + 0.05f * (float) k, 0.9f);
            else { put (m, 28, 0, 0.9f, 2.2f); put (m, 30, 7, 0.95f, 2.0f, 0, false, true); }
            return;
        }
        if (feel == FGothic)
        {
            static const int a[][4] = { { 7, 6, 4, 2 }, { 0, -1, -2, -3 }, { 4, 5, 6, 7 } };
            const auto& f = a[r.i (3)];
            for (int k = 0; k < (big ? 4 : 2); ++k) put (m, from + k * 2, f[k + (big ? 0 : 2)], 0.85f, 1.8f);
            return;
        }
        const int type = r.i (big ? 5 : 6);
        if (big)
        {
            static const int runs[][8] = { { 0, 2, 4, 7, 9, 7, 4, 2 }, { 7, 6, 5, 4, 3, 2, 1, 0 }, { 0, 4, 7, 11, 9, 7, 4, 0 },
                                           { 7, 0, 7, 0, 7, 0, 6, 7 }, { 4, 4, 5, 5, 6, 6, 7, 7 } };
            for (int k = 0; k < 8; ++k) put (m, from + k, runs[type][k], k % 2 == 0 ? 0.9f : 0.72f, 0.95f);
            return;
        }
        switch (type)
        {
            case 0: { const int d[] = { 0, 2, 4, 7 }; for (int k = 0; k < 4; ++k) put (m, 28 + k, d[k], 0.85f, 0.95f); break; }   // up the chord
            case 1: { const int d[] = { 7, 6, 4, 2 }; for (int k = 0; k < 4; ++k) put (m, 28 + k, d[k], 0.85f, 0.95f); break; }   // down from the octave
            case 2: { for (int k = 0; k < 4; ++k) put (m, 28 + k, k % 2 ? 0 : 7, 0.88f, 0.9f); break; }                          // octave pops
            case 3: { put (m, 28, 0, 0.86f, 0.95f); put (m, 29, 0, 0.8f, 0.95f, -3); put (m, 30, 0, 0.84f, 0.95f, -2); put (m, 31, 0, 0.88f, 0.95f, -1); break; }  // chromatic walk-up
            case 4: { put (m, 28, 4, 0.9f, 1.6f); put (m, 30, 5, 0.85f, 0.8f); put (m, 31, 6, 0.85f, 0.8f); break; }               // 5-6-7 into the root
            default: { put (m, 28, 7, 0.95f, 1.6f); put (m, 30, 4, 0.8f, 0.8f); put (m, 31, 2, 0.75f, 0.8f); break; }
        }
    }

    // a fresh take on the motif for this pass (what a player changes without thinking)
    static Motif vary (const Motif& base, Rng& r, float variation, float density)
    {
        Motif m = base;
        if (variation <= 0.001f) return m;
        for (int i = 1; i < kSteps; ++i)
        {
            auto& s = m.s[(size_t) i];
            if (s.on && ! s.ghost)
            {
                if (r.chance (variation * 0.10f)) { static const int tones[] = { 0, 4, 7, 2 }; s.deg = tones[r.i (4)]; s.semis = 0; }
                else if (s.deg == 0 && r.chance (variation * 0.08f)) s.deg = 7;                      // octave pop
                else if (i % 4 != 0 && r.chance (variation * 0.05f)) s.on = false;                   // leave a gap
            }
            else if (! s.on && i % 2 == 1 && i + 1 < kSteps && m.s[(size_t) i + 1].on && r.chance (variation * 0.05f * (0.5f + density)))
            {
                put (m, i, m.s[(size_t) i + 1].deg - 1, 0.55f, 0.7f);                                  // passing note
            }
        }
        return m;
    }

private:
    static void pump (Motif& m, Rng& r, int sec, float hook, float density)
    {
        for (int i = 0; i < kSteps; i += 2)
            put (m, i, 0, i % 8 == 0 ? 0.92f : (i % 4 == 0 ? 0.82f : 0.74f), 1.7f);
        if (sec == SVerse && density < 0.65f)
            for (int i = 2; i < kSteps; i += 4) if (r.chance ((0.65f - density) * 0.8f)) m.s[(size_t) i].on = false;
        if (sec == SChorus && r.chance (0.6f))
            for (int i = 16; i < 24; i += 4) put (m, i + 2, 7, 0.86f, 1.6f);                          // octave jumps
        if (density > 0.7f) for (int i = 3; i < kSteps; i += 8) if (r.chance (density - 0.6f)) put (m, i, 0, 0.45f, 0.6f, 0, true);
        // bar 1 ending
        if (r.chance (0.35f + hook * 0.3f)) { static const int f[][2] = { { 4, 4 }, { 0, 7 }, { 7, 6 }, { 2, 4 } }; const auto& p = f[r.i (4)]; put (m, 12, p[0], 0.8f, 1.6f); put (m, 14, p[1], 0.8f, 1.6f); }
        // bar 2 ending: the hook
        if (hook > 0.55f && r.chance (hook))
        {
            int d = 7 + r.i (3) * 2;
            for (int i = 24; i < kSteps; i += 2) { put (m, i, d, 0.86f, 1.8f); d += r.chance (0.6f) ? -1 : (r.chance (0.5f) ? 1 : -2); d = std::clamp (d, 4, 11); }
        }
        else
        {
            static const int f[][2] = { { 7, 7 }, { 4, 7 }, { -1, 0 }, { 2, 4 }, { 0, 4 } };
            const auto& p = f[r.i (5)]; put (m, 28, p[0], 0.86f, 1.6f); put (m, 30, p[1], 0.86f, 1.6f);
        }
    }

    static void melodic (Motif& m, Rng& r, int sec, float hook, float density)
    {
        // the melody lives an octave up (deg 7+); a low root on the downbeat anchors it
        int d = (r.chance (0.5f) ? 11 : (r.chance (0.5f) ? 9 : 14));
        auto next = [&]
        {
            const float x = r.u();
            d += x < 0.32f ? -1 : x < 0.55f ? 1 : x < 0.68f ? -2 : x < 0.8f ? 2 : 0;
            if (d > 15) d -= 2;
            if (d < 7) d += 2;
            return d;
        };
        const float rest = std::clamp (0.28f - density * 0.25f + (sec == SVerse ? 0.06f : 0.0f), 0.0f, 0.4f);
        for (int beat = 0; beat < 8; ++beat)
        {
            const int b = beat * 4;
            const float x = r.u();
            if (beat == 0 && hook < 0.85f) { put (m, 0, 0, 0.9f, 2.0f); put (m, 2, next(), 0.8f, 2); continue; }
            if (x < rest && beat % 4 != 0) continue;
            if (x < 0.55f) { put (m, b, next(), 0.82f, 2); put (m, b + 2, next(), 0.74f, 2); }          // two eighths
            else if (x < 0.75f) put (m, b, next(), 0.84f, 4);                                           // quarter
            else if (x < 0.9f) { put (m, b, next(), 0.84f, 3); put (m, b + 3, next(), 0.7f, 1); }       // dotted eighth + 16th
            else { put (m, b + 2, next(), 0.8f, 2); }                                                   // push on the &
        }
        // call and response: bar 2 starts like bar 1, then answers differently
        if (r.chance (0.7f)) for (int i = 0; i < 8; ++i) m.s[(size_t) (16 + i)] = m.s[(size_t) i];
        // resolve to a chord tone at the end
        for (int i = kSteps - 1; i >= 20; --i)
            if (m.s[(size_t) i].on) { static const int res[] = { 7, 11, 9, 14 }; m.s[(size_t) i].deg = res[r.i (4)]; break; }
        legato (m, 0.92f);
    }

    static void gothic (Motif& m, Rng& r, int sec, float hook, float density)
    {
        const int t = sec == SChorus ? 1 : r.i (3);
        if (t == 0)
        {
            // long root, then a slow descending line
            put (m, 0, 0, 0.9f, 7.6f); put (m, 8, 0, 0.78f, 3.6f); put (m, 12, hook > 0.5f ? 7 : 4, 0.8f, 3.6f);
            const bool down = r.chance (0.7f);
            for (int k = 0; k < 4; ++k) put (m, 16 + k * 4, down ? -k : (k == 3 ? 6 : k * 2), 0.84f, 3.7f);
        }
        else if (t == 1)
        {
            // driving quarters with pickups (a dark, tom-heavy groove)
            for (int i = 0; i < kSteps; i += 4) put (m, i, 0, i % 16 == 0 ? 0.92f : 0.8f, 3.0f);
            for (int i = 3; i < kSteps; i += 8) if (r.chance (0.4f + density * 0.4f)) put (m, i, 0, 0.5f, 0.7f, 0, true);
            if (r.chance (0.6f)) { put (m, 24, 4, 0.82f, 3); put (m, 28, hook > 0.5f ? 7 : 6, 0.84f, 3); }
        }
        else
        {
            // half notes walking a shape: root, fifth, octave, seventh
            static const int shapes[][4] = { { 0, 4, 7, 6 }, { 0, -1, -2, -3 }, { 0, 2, 4, 7 }, { 7, 6, 4, 0 } };
            const auto& s = shapes[r.i (4)];
            for (int k = 0; k < 4; ++k) put (m, k * 8, s[k], 0.86f, 7.6f);
            if (density > 0.6f) for (int k = 0; k < 4; ++k) put (m, k * 8 + 6, s[k], 0.6f, 1.6f);
        }
    }

    static void octaves (Motif& m, Rng& r, int sec, float hook, float density)
    {
        for (int i = 0; i < kSteps; i += 2) put (m, i, (i / 2) % 2 ? 7 : 0, (i / 2) % 2 ? 0.8f : 0.9f, 1.5f);
        for (int i = 1; i < kSteps; i += 4)
            if (r.chance (0.2f + density * 0.4f + (sec == SChorus ? 0.2f : 0.0f))) put (m, i, 0, 0.42f, 0.7f, 0, true);
        if (hook > 0.4f && r.chance (hook)) { put (m, 26, 9, 0.84f, 1.5f); put (m, 28, 11, 0.86f, 1.5f); put (m, 30, 9, 0.8f, 1.5f); }
        if (r.chance (0.5f)) { put (m, 12, 4, 0.84f, 1.5f); put (m, 14, 11, 0.8f, 1.5f); }
    }

    static void walk (Motif& m, Rng& r, int sec, float hook, float density)
    {
        for (int bar = 0; bar < 2; ++bar)
        {
            const int b = bar * 16;
            put (m, b, 0, 0.92f, 3.5f);
            put (m, b + 4, r.chance (0.5f) ? 2 : 4, 0.8f, 3.5f);
            put (m, b + 8, r.chance (0.6f) ? 4 : 7, 0.84f, 3.5f);
            static const int app[] = { -1, 1, 5, 6, -3 };
            put (m, b + 12, app[r.i (5)] + (hook > 0.6f && r.chance (0.5f) ? 7 : 0), 0.8f, 3.5f);
            if (density > 0.5f && r.chance (density)) put (m, b + 14, 0, 0.7f, 1.6f, -1);
            if (sec == SChorus && r.chance (0.5f)) put (m, b + 6, 4, 0.7f, 1.6f);
        }
    }

    // straight root eighths, the engine of punk, shoegaze, krautrock and grunge
    static void root8 (Motif& m, Rng& r, int sec, float hook, float density)
    {
        for (int i = 0; i < kSteps; i += 2) put (m, i, 0, i % 8 == 0 ? 0.9f : 0.78f, 1.9f);
        if (sec == SChorus && r.chance (0.5f)) { put (m, 28, 4, 0.85f, 1.8f); put (m, 30, 7, 0.88f, 1.8f); }
        if (density > 0.75f) for (int i = 1; i < kSteps; i += 8) if (r.chance (density - 0.6f)) put (m, i, 0, 0.45f, 0.7f, 0, true);
        if (hook > 0.6f && r.chance (hook - 0.3f)) { put (m, 12, 4, 0.8f, 1.8f); put (m, 14, 2, 0.8f, 1.8f); }
        else if (r.chance (0.4f)) put (m, 30, 0, 0.8f, 1.8f, -1);                // chromatic lean into the next bar
    }

    // slow and heavy: long roots, octave drops, a slide-like approach
    static void halfHeavy (Motif& m, Rng& r, int sec, float hook, float density)
    {
        put (m, 0, 0, 0.95f, 7.5f);
        put (m, 8, sec == SChorus ? 0 : -7, 0.85f, 3.6f);        // root an octave down on 3 (folded back into range)
        put (m, 12, r.chance (0.5f) ? 0 : 6, 0.8f, 3.6f);
        put (m, 16, 0, 0.95f, 5.5f);
        put (m, 22, r.chance (0.5f) ? 2 : 4, 0.82f, 1.8f);
        put (m, 24, 4, 0.86f, 3.6f);
        put (m, 28, hook > 0.5f ? 7 : 3, 0.82f, 1.7f);
        put (m, 30, hook > 0.5f ? 6 : 1, 0.78f, 1.7f);
        if (density > 0.6f) put (m, 6, 0, 0.7f, 1.6f);
    }

    // eighth + two sixteenths: surf, metal, galloping rock
    static void gallop (Motif& m, Rng& r, int sec, float hook, float density)
    {
        for (int b = 0; b < kSteps; b += 4)
        {
            const int deg = (b == 12 || b == 28) && hook > 0.4f ? (r.chance (0.5f) ? 4 : 7) : 0;
            put (m, b, deg, 0.9f, 1.8f);
            put (m, b + 2, deg, 0.7f, 0.9f);
            put (m, b + 3, deg, 0.74f, 0.9f);
        }
        if (sec == SChorus) for (int b = 16; b < 24; b += 4) for (int k = 0; k < 4; ++k) if (m.s[(size_t) (b + k)].on) m.s[(size_t) (b + k)].deg = 7;
        juce::ignoreUnused (density);
    }

    // Detroit soul: busy, melodic fingered lines that never sit still, with chromatic approaches
    static void motorCity (Motif& m, Rng& r, int sec, float hook, float density)
    {
        for (int bar = 0; bar < 2; ++bar)
        {
            const int b = bar * 16;
            put (m, b, 0, 0.92f, 1.8f);
            if (r.chance (0.5f + density * 0.3f)) put (m, b + 3, 0, 0.62f, 0.8f);                  // the 16th pickup into beat 2
            for (int beat = 1; beat < 4; ++beat)
            {
                const int p = b + beat * 4;
                const float x = r.u();
                static const int tones[] = { 4, 7, 2, 5, 9, 4 };
                if (x < 0.35f) { put (m, p, tones[r.i (6)], 0.82f, 1.8f); put (m, p + 2, tones[r.i (6)], 0.74f, 1.6f); }
                else if (x < 0.6f) { put (m, p, tones[r.i (6)], 0.84f, 2.8f); put (m, p + 3, tones[r.i (6)], 0.66f, 0.9f); }
                else if (x < 0.8f) { put (m, p + 2, tones[r.i (6)], 0.8f, 1.8f); }
                else { put (m, p, 7, 0.84f, 1.6f); put (m, p + 1, 6, 0.6f, 0.9f); put (m, p + 2, 4, 0.72f, 1.6f); }   // a little run down
            }
            // lean into the next chord from a half step below
            put (m, b + 15, 0, 0.72f, 0.9f, -1);
            if (hook > 0.6f && r.chance (hook - 0.2f)) { put (m, b + 12, 9, 0.84f, 1.6f); put (m, b + 14, 11, 0.8f, 1.0f); }
        }
        if (sec == SChorus) for (int i = 0; i < kSteps; i += 8) if (! m.s[(size_t) i + 2].on) put (m, i + 2, 7, 0.8f, 1.6f);
        legato (m, 0.85f);
    }

    // Philadelphia: octaves bouncing on the off-beats with 16th pickups, lush and driving
    static void philly (Motif& m, Rng& r, int sec, float hook, float density)
    {
        for (int b = 0; b < kSteps; b += 4)
        {
            put (m, b, 0, b % 16 == 0 ? 0.92f : 0.84f, 1.4f);
            put (m, b + 2, 7, 0.82f, 1.2f);
            if (r.chance (0.35f + density * 0.5f)) put (m, b + 3, 7, 0.6f, 0.8f);                  // the pickup
        }
        if (hook > 0.4f && r.chance (hook))
        {
            put (m, 24, 4, 0.86f, 1.4f); put (m, 26, 5, 0.82f, 1.2f); put (m, 28, 6, 0.84f, 1.4f); put (m, 30, 7, 0.86f, 1.2f);   // walk up into the change
        }
        if (sec == SChorus) for (int b = 1; b < kSteps; b += 8) if (r.chance (0.4f)) put (m, b, 0, 0.45f, 0.6f, 0, true);
    }

    // funk: the ONE hits hard, then syncopated 16ths with ghost notes and octave pops
    static void funk (Motif& m, Rng& r, int sec, float hook, float density)
    {
        for (int bar = 0; bar < 2; ++bar)
        {
            const int b = bar * 16;
            put (m, b, 0, 1.0f, 1.6f);
            static const int spots[] = { 3, 6, 7, 10, 11, 13, 14 };
            for (int k : spots)
            {
                const float x = r.u();
                if (x < 0.25f + density * 0.25f) put (m, b + k, (k == 6 || k == 14) ? 7 : (k == 10 ? 4 : 0), 0.8f, 0.9f);
                else if (x < 0.45f + density * 0.3f) put (m, b + k, 0, 0.4f, 0.5f, 0, true);
            }
            if (r.chance (0.5f)) put (m, b + 12, 6, 0.82f, 0.9f);                                  // the flat seven
        }
        if (hook > 0.5f && r.chance (hook)) { put (m, 28, 7, 0.88f, 0.9f); put (m, 29, 6, 0.8f, 0.9f); put (m, 30, 4, 0.84f, 0.9f); put (m, 31, 2, 0.8f, 0.9f); }
        juce::ignoreUnused (sec);
    }

    // trap: long booming sub notes on the kick spots, octave slides, rolls at the end
    static void trap (Motif& m, Rng& r, int sec, float hook, float density)
    {
        static const int pats[][6] = { { 0, 6, 10, -1, -1, -1 }, { 0, 3, 10, 14, -1, -1 }, { 0, 7, 10, -1, -1, -1 }, { 0, 6, 11, 14, -1, -1 } };
        for (int bar = 0; bar < 2; ++bar)
        {
            const auto& p = pats[r.i (4)];
            int n = 0; while (n < 6 && p[n] >= 0) ++n;
            for (int k = 0; k < n; ++k)
            {
                const int at = bar * 16 + p[k], next = k + 1 < n ? bar * 16 + p[k + 1] : bar * 16 + 16;
                put (m, at, 0, k == 0 ? 0.95f : 0.85f, (float) (next - at) * 0.95f);
            }
        }
        // the slide: up an octave (or down to the fifth) and back, gliding
        if (r.chance (0.5f + hook * 0.4f)) { put (m, 14, 7, 0.88f, 1.8f, 0, false, true); put (m, 15, 0, 0.84f, 1.0f, 0, false, true); }
        if (r.chance (0.3f + hook * 0.3f)) put (m, 26, r.chance (0.5f) ? 4 : -3, 0.86f, 3.6f, 0, false, true);
        if (density > 0.6f && r.chance (density)) for (int k = 0; k < 3; ++k) put (m, 29 + k, 0, 0.7f, 0.8f);    // a quick roll
        juce::ignoreUnused (sec);
    }

    // boom bap: a fat root on one, a lazy syncopation on the 'a' of two, a turn at the end
    static void boomBap (Motif& m, Rng& r, int sec, float hook, float density)
    {
        for (int bar = 0; bar < 2; ++bar)
        {
            const int b = bar * 16;
            put (m, b, 0, 0.92f, 5.0f);
            put (m, b + 7, 0, 0.74f, 2.0f);
            put (m, b + 10, r.chance (0.5f) ? 0 : 4, 0.82f, 2.6f);
            if (r.chance (0.4f + density * 0.4f)) put (m, b + 14, r.chance (0.5f) ? 6 : 4, 0.76f, 1.6f);
        }
        if (hook > 0.5f && r.chance (hook)) { put (m, 26, 4, 0.82f, 1.8f); put (m, 28, 2, 0.78f, 1.8f); put (m, 30, 1, 0.74f, 1.8f); }
        if (sec == SChorus) put (m, 20, 7, 0.82f, 2.0f);
    }

    // house: the bass lives on the off-beats between the four-on-the-floor kicks
    static void house (Motif& m, Rng& r, int sec, float hook, float density)
    {
        for (int b = 0; b < kSteps; b += 4)
        {
            const bool oct = hook > 0.5f && (b % 8 == 4) && r.chance (hook);
            put (m, b + 2, oct ? 7 : 0, 0.88f, 1.4f);
            if (density > 0.55f && r.chance (density - 0.3f)) put (m, b + 3, 0, 0.62f, 0.8f);
        }
        if (sec == SChorus && r.chance (0.6f)) { put (m, 27, 4, 0.8f, 0.9f); put (m, 30, 6, 0.86f, 1.4f); }
    }

    // techno: rolling 16ths that duck out of the way of the kick
    static void techno (Motif& m, Rng& r, int sec, float hook, float density)
    {
        for (int b = 0; b < kSteps; b += 4)
            for (int k = 1; k < 4; ++k)
                if (k == 2 || r.chance (0.55f + density * 0.4f)) put (m, b + k, 0, k == 2 ? 0.88f : 0.68f, 0.8f);
        if (hook > 0.5f) for (int b = 12; b < kSteps; b += 16) if (r.chance (hook)) { put (m, b + 2, 7, 0.88f, 0.8f); put (m, b + 3, 4, 0.7f, 0.8f); }
        juce::ignoreUnused (sec);
    }

    // acid: a 16-step sequence with accents and slides, the classic squelch machine
    static void acid (Motif& m, Rng& r, int sec, float hook, float density)
    {
        static const int pool[] = { 0, 0, 0, 7, 0, 2, 6, 4, 7, 14, -2 };
        Step bar[16];
        int last = -1;
        for (int i = 0; i < 16; ++i)
        {
            const bool on = i == 0 || r.chance (0.45f + density * 0.4f);
            if (! on) continue;
            const int deg = i == 0 ? 0 : pool[r.i (hook > 0.5f ? 11 : 8)];
            const bool accent = r.chance (0.3f), slide = last >= 0 && last == i - 1 && r.chance (0.35f);
            bar[i] = { true, deg, 0, accent ? 0.95f : 0.7f, slide ? 1.3f : 0.85f, false, slide };
            if (slide && bar[last].on) bar[last].len = 1.3f;   // overlap so it glides
            last = i;
        }
        for (int i = 0; i < 16; ++i) { m.s[(size_t) i] = bar[i]; m.s[(size_t) (16 + i)] = bar[i]; }
        // the second bar answers with a twist
        for (int i = 20; i < kSteps; ++i) if (m.s[(size_t) i].on && r.chance (0.2f)) m.s[(size_t) i].deg = pool[r.i (8)];
        juce::ignoreUnused (sec);
    }

    // new jack swing: swung, syncopated, punchy octaves
    static void newJack (Motif& m, Rng& r, int sec, float hook, float density)
    {
        for (int bar = 0; bar < 2; ++bar)
        {
            const int b = bar * 16;
            put (m, b, 0, 0.94f, 2.4f);
            put (m, b + 3, 0, 0.7f, 0.9f);
            put (m, b + 6, 7, 0.84f, 1.4f);
            put (m, b + 10, 0, 0.86f, 1.0f);
            if (r.chance (0.5f + density * 0.4f)) put (m, b + 11, 0, 0.68f, 0.9f);
            put (m, b + 14, r.chance (0.5f) ? 6 : 4, 0.82f, 1.4f);
        }
        if (hook > 0.5f && r.chance (hook)) { put (m, 28, 7, 0.88f, 0.9f); put (m, 29, 9, 0.8f, 0.9f); put (m, 30, 7, 0.84f, 1.4f); }
        juce::ignoreUnused (sec);
    }

    // melodic 60s: a tune of its own under the song - root on one, climbing through the chord,
    // octave pops, and a walk down into the next chord from a step above
    static void sixties (Motif& m, Rng& r, int sec, float hook, float density)
    {
        // bar one: the chord, sung
        put (m, 0, 0, 0.92f, 5.0f);
        if (r.chance (0.6f)) put (m, 6, 4, 0.74f, 1.8f); else put (m, 6, 7, 0.76f, 1.8f);
        put (m, 8, 2, 0.82f, 3.0f);
        if (r.chance (0.35f + density * 0.4f)) put (m, 11, 4, 0.62f, 0.9f);
        put (m, 12, 4, 0.8f, 1.8f);
        put (m, 14, r.chance (0.5f) ? 5 : 6, 0.74f, 1.8f);
        // bar two: up to the octave and walk back down to the next chord
        put (m, 16, 0, 0.9f, 2.6f);
        if (r.chance (0.5f + hook * 0.4f)) { put (m, 19, 7, 0.66f, 0.9f); put (m, 20, 7, 0.82f, 1.8f); }
        else put (m, 20, 4, 0.8f, 1.8f);
        const bool run = r.chance (0.45f + density * 0.4f);
        if (run) { put (m, 22, 6, 0.72f, 1.8f); put (m, 24, 5, 0.76f, 1.8f); put (m, 26, 4, 0.76f, 1.8f); put (m, 28, 2, 0.76f, 1.8f); put (m, 30, 1, 0.74f, 1.8f); }
        else { put (m, 24, 4, 0.8f, 3.0f); put (m, 28, 2, 0.76f, 1.8f); put (m, 30, 1, 0.72f, 1.8f); }
        if (r.chance (0.3f)) put (m, 15, 0, 0.4f, 0.6f, 0, true);   // a little ghost before the second bar
        if (sec == SChorus) put (m, 10, 7, 0.8f, 1.8f);
        legato (m, 0.9f);
    }

    static void halfTime (Motif& m, Rng& r)
    {
        for (int i = 0; i < kSteps; ++i)
        {
            auto& s = m.s[(size_t) i];
            if (! s.on) continue;
            const bool keep = i % 8 == 0 || (i % 8 == 6 && r.chance (0.35f));
            if (! keep) s.on = false;
        }
        legato (m, 0.9f);
    }
};

// Turns a step into a real pitch given the chord, a register and which notes the song has been using
struct Harmony
{
    std::array<float, 12> used {};      // decaying histogram of chord tones heard
    void hear (Chord c, float amount = 1.0f)
    {
        if (! c.valid()) return;
        for (auto& u : used) u *= 0.985f;
        used[(size_t) c.root] += amount;
        used[(size_t) ((c.root + (c.minor ? 3 : 4)) % 12)] += amount;
        used[(size_t) ((c.root + 7) % 12)] += amount;
    }
    int semisFor (Chord c, int deg) const
    {
        static const int major[7] = { 0, 2, 4, 5, 7, 9, 11 }, minor[7] = { 0, 2, 3, 5, 7, 8, 10 };
        static const int majorAlt[7] = { 0, 1, 4, 6, 7, 8, 10 }, minorAlt[7] = { 0, 1, 3, 6, 7, 9, 11 };
        const int o = (int) std::floor (deg / 7.0);
        const int d = deg - o * 7;
        int s = (c.minor ? minor : major)[d];
        // the chord's own colour wins: 7ths, flat fives, suspended fourths
        if (c.q >= 0)
        {
            const int o2 = o * 12;
            if (d == 6) { if (c.q == 2 || c.q == 4 || c.q == 7) return 10 + o2; if (c.q == 3) return 11 + o2; }
            if (d == 4 && (c.q == 5 || c.q == 7)) return 6 + o2;
            if (d == 2 && c.q == 6) return 5 + o2;
        }
        // non-chord tones: borrow whichever version the song has actually been using
        // (a major chord in a minor song gets a flat 7, a minor chord in a major song gets a natural 6...)
        if (d == 1 || d == 3 || d == 5 || d == 6)
        {
            const int alt = (c.minor ? minorAlt : majorAlt)[d];
            const float a = used[(size_t) ((c.root + s) % 12)], b = used[(size_t) ((c.root + alt) % 12)];
            if (b > a * 1.5f + 0.5f) s = alt;
        }
        return s + o * 12;
    }
};
// ---------------------------------------------------------------------------
// Self-playing: built-in progressions (relative to a key) and a song form
// ---------------------------------------------------------------------------
struct ProgChord { int off; bool minor; int q = -1; int quality() const { return q >= 0 ? q : (minor ? 1 : 0); } };
struct Progression { const char* label; std::vector<ProgChord> chords; };
inline const std::vector<Progression>& progressions()
{
    static const std::vector<Progression> p = {
        { "i - VI - III - VII", { { 0, true }, { 8, false }, { 3, false }, { 10, false } } },
        { "i - VII - VI - VII", { { 0, true }, { 10, false }, { 8, false }, { 10, false } } },
        { "VI - VII - i - i",   { { 8, false }, { 10, false }, { 0, true }, { 0, true } } },
        { "i - iv - VI - v",    { { 0, true }, { 5, true }, { 8, false }, { 7, true } } },
        { "i - III - VII - VI", { { 0, true }, { 3, false }, { 10, false }, { 8, false } } },
        { "i - iv",             { { 0, true }, { 5, true } } },
        { "i - VII",            { { 0, true }, { 10, false } } },
        { "i (one chord)",      { { 0, true } } },
        { "I - V - vi - IV",    { { 0, false }, { 7, false }, { 9, true }, { 5, false } } },
        { "vi - IV - I - V",    { { 9, true }, { 5, false }, { 0, false }, { 7, false } } },
        { "I - vi - IV - V",    { { 0, false }, { 9, true }, { 5, false }, { 7, false } } },
        { "I - IV",             { { 0, false }, { 5, false } } },
        { "I - bVII - IV - I",  { { 0, false }, { 10, false }, { 5, false }, { 0, false } } },
        { "I - IV - V - IV",    { { 0, false }, { 5, false }, { 7, false }, { 5, false } } },
        { "I - IV - I - V",     { { 0, false }, { 5, false }, { 0, false }, { 7, false } } },
        { "I - bIII - IV - I",  { { 0, false }, { 3, false }, { 5, false }, { 0, false } } },
        { "i - VII - VI - V",   { { 0, true }, { 10, false }, { 8, false }, { 7, false } } },
        { "I - V",              { { 0, false }, { 7, false } } },
        { "12-bar blues",       { { 0, false }, { 0, false }, { 0, false }, { 0, false }, { 5, false }, { 5, false },
                                  { 0, false }, { 0, false }, { 7, false }, { 5, false }, { 0, false }, { 7, false } } },
        { "I - iii - IV - iv",  { { 0, false }, { 4, true }, { 5, false }, { 5, true } } },
        { "I - bVI - bVII - I", { { 0, false }, { 8, false }, { 10, false }, { 0, false } } },
        { "i - v",              { { 0, true }, { 7, true } } },
        { "I - vi - ii - V",    { { 0, false }, { 9, true }, { 2, true }, { 7, false } } },
        // 7th-chord progressions (soul, house, lo-fi, 80s)
        { "Imaj7 - vi7 - ii7 - V7",     { { 0, false, 3 }, { 9, true, 4 }, { 2, true, 4 }, { 7, false, 2 } } },
        { "ii7 - V7 - Imaj7 - Imaj7",   { { 2, true, 4 }, { 7, false, 2 }, { 0, false, 3 }, { 0, false, 3 } } },
        { "IVmaj7 - iii7 - ii7 - Imaj7",{ { 5, false, 3 }, { 4, true, 4 }, { 2, true, 4 }, { 0, false, 3 } } },
        { "Imaj7 - iii7 - vi7 - IVmaj7",{ { 0, false, 3 }, { 4, true, 4 }, { 9, true, 4 }, { 5, false, 3 } } },
        { "i7 - iv7",                   { { 0, true, 4 }, { 5, true, 4 } } },
        { "i7 (one chord)",             { { 0, true, 4 } } },
        { "I7 (one chord)",             { { 0, false, 2 } } },
        { "Imaj7 - IVmaj7",             { { 0, false, 3 }, { 5, false, 3 } } },
        { "ii7 - V7 - iii7 - vi7",      { { 2, true, 4 }, { 7, false, 2 }, { 4, true, 4 }, { 9, true, 4 } } },
        { "I7 - IV7 - I7 - V7",         { { 0, false, 2 }, { 5, false, 2 }, { 0, false, 2 }, { 7, false, 2 } } },
        { "i - i - VI - VII",           { { 0, true }, { 0, true }, { 8, false }, { 10, false } } },
        { "I - V - vi - iii - IV - I - IV - V", { { 0, false }, { 7, false }, { 9, true }, { 4, true }, { 5, false }, { 0, false }, { 5, false }, { 7, false } } },
        { "Isus4 - I",                  { { 0, false, 6 }, { 0, false } } },
        { "I - II - IV - I",            { { 0, false }, { 2, false }, { 5, false }, { 0, false } } },
        { "i7 - VImaj7 - iv7 - v7",     { { 0, true, 4 }, { 8, false, 3 }, { 5, true, 4 }, { 7, true, 4 } } },
        { "vi7 - ii7 - V7 - Imaj7",     { { 9, true, 4 }, { 2, true, 4 }, { 7, false, 2 }, { 0, false, 3 } } },
    };
    return p;
}
inline juce::StringArray progressionNames() { juce::StringArray a; for (auto& p : progressions()) a.add (p.label); return a; }
inline juce::String progressionChords (int prog, int key)
{
    juce::String s;
    const auto& ps = progressions();
    if (prog < 0 || prog >= (int) ps.size()) return s;
    for (auto& c : ps[(size_t) prog].chords) s << pcName (key + c.off) << band::qualitySuffix (c.quality()) << " ";
    return s.trim();
}

// the song the band plays when it's on its own: verse, chorus, verse, chorus, bridge, chorus
struct SongPos { int section; int barIn; bool lastBar; };
inline int sectionBarsFor (int choiceIndex) { static const int b[] = { 4, 8, 16, 12 }; return b[std::clamp (choiceIndex, 0, 3)]; }
inline SongPos songPosition (int64_t bar, int sectionBars)
{
    static const int form[] = { SVerse, SChorus, SVerse, SChorus, SBridge, SChorus };
    const int64_t len = (int64_t) sectionBars * 6;
    const int64_t b = ((bar % len) + len) % len;
    const int part = (int) (b / sectionBars);
    const int in = (int) (b % sectionBars);
    return { form[part], in, in == sectionBars - 1 && form[(part + 1) % 6] != form[part] };
}
} // namespace ut
