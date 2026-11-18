#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <cmath>
#include <map>
#include <memory>
#include <vector>

// ---------------------------------------------------------------------------
// Chamber: a 60s-style orchestra. Real string, brass and woodwind sections
// (VSCO 2 Community Edition, CC0), panned the way 60s records did it, with
// a WARP control for the wobbly, slightly out-of-tune 90s orchestra and a
// tape mode for the tape-replay strings and flutes.
// ---------------------------------------------------------------------------
namespace ch
{
constexpr double kTwoPi = 6.283185307179586476925;

enum Instr { VLN = 0, VLA, VC, SVL, TPT, HRN, TBN, FL, OB, CL, BSN, TIM, NumInstr };
inline const char* instrCode (int i) { static const char* c[] = { "vln", "vla", "vc", "svl", "tpt", "hrn", "tbn", "fl", "ob", "cl", "bsn", "tim" }; return c[i]; }
// playable ranges (MIDI), so each part sits where the real instrument does
inline void instrRange (int i, int& lo, int& hi)
{
    static const int r[NumInstr][2] = { { 55, 96 }, { 48, 84 }, { 36, 72 }, { 55, 100 }, { 54, 84 }, { 41, 77 }, { 40, 72 },
                                        { 60, 96 }, { 58, 91 }, { 50, 89 }, { 34, 70 }, { 36, 60 } };
    lo = r[i][0]; hi = r[i][1];
}
enum Art { ASus = 0, AShort, APizz, ATrem, NumArts };
inline const char* artCode (int a) { static const char* c[] = { "sus", "spc", "pz", "trm" }; return c[a]; }

enum Ensemble { EStrings = 0, EQuartet, EBrass, EWinds, EStringsHorns, ETapeStrings, ETapeFlutes, ETapeCellos, ETapeBrass, ETapeWinds, NumEnsembles };
inline const char* ensembleName (int e)
{
    static const char* n[] = { "String Section", "String Quartet", "Brass", "Woodwinds", "Strings + Horns", "Tape Strings", "Tape Flutes", "Tape Cellos", "Tape Brass", "Tape Woodwinds" };
    return (e >= 0 && e < NumEnsembles) ? n[e] : "";
}
// the four parts, top to bottom
inline void ensembleParts (int e, int out[4])
{
    static const int p[NumEnsembles][4] = { { VLN, VLN, VLA, VC }, { SVL, VLN, VLA, VC }, { TPT, TPT, HRN, TBN }, { FL, OB, CL, BSN },
                                            { VLN, VLA, HRN, VC }, { VLN, VLN, VLA, VC }, { FL, FL, CL, BSN },
                                            { VC, VC, VC, VC }, { TPT, TPT, TBN, TBN }, { FL, OB, CL, BSN } };
    for (int i = 0; i < 4; ++i) out[i] = p[juce::jlimit (0, NumEnsembles - 1, e)][i];
}
inline bool ensembleIsTape (int e) { return e >= ETapeStrings; }

struct Rng
{
    uint32_t s = 0x6d2b79f5u;
    void seed (uint32_t x) { s = x ? x : 1u; }
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float u() { return (float) (next() & 0xffffff) / 16777216.0f; }
    float n() { return u() * 2.0f - 1.0f; }
    bool chance (float p) { return u() < p; }
    int i (int n) { return n <= 1 ? 0 : (int) (next() % (uint32_t) n); }
};

struct Zone
{
    int root = 60, layer = 0, loopStart = -1;
    double sr = 32000;
    std::vector<float> d;          // mono, padded
};
class Library
{
public:
    std::vector<Zone> zones[NumInstr][NumArts];
    int count = 0;
    // nearest sample for this note (preferring the velocity layer), falling back to another articulation
    const Zone* find (int instr, int art, int note, int layer) const
    {
        static const int fallback[NumArts][3] = { { ASus, -1, -1 }, { AShort, ASus, -1 }, { APizz, AShort, ASus }, { ATrem, ASus, -1 } };
        for (int k = 0; k < 3; ++k)
        {
            const int a = fallback[art][k];
            if (a < 0) break;
            const auto& zs = zones[instr][a];
            if (zs.empty()) continue;
            const Zone* best = nullptr; int bd = 1 << 20;
            for (const auto& z : zs) { const int d = std::abs (z.root - note) * 4 + (z.layer == layer ? 0 : 5); if (d < bd) { bd = d; best = &z; } }
            return best;
        }
        return nullptr;
    }
};

class Voice
{
public:
    int note = -1; bool held = false; int64_t age = 0; int part = 0;
    float pan = 0;
    void prepare (double s) { sr = s; }
    // len < 0: hold until released. attackSec shapes the bow: slow for a swell, fast for a bite.
    void start (const Library& lib, int instr, int art, int midi, float vel, float lenSec, float attackSec, float detuneCents, bool tape, Rng& r)
    {
        zone = lib.find (instr, art, midi, vel > 0.62f ? 1 : 0);
        if (zone == nullptr) { note = -1; return; }
        note = midi; held = true; releasing = false; age = 0; t = 0;
        artic = art; inst = instr; tapeMode = tape;
        velocity = vel;
        pos = 0;
        baseRate = std::pow (2.0, (midi - zone->root + detuneCents / 100.0) / 12.0) * zone->sr / sr;
        glideTo = 0; glideK = 0; bend = 0;
        attack = juce::jmax (0.002f, attackSec);
        env = 0; relEnv = 1;
        holdLeft = lenSec > 0 ? (int64_t) (lenSec * sr) : -1;
        wobPh = r.u(); wobRate = 0.25 + 0.5 * r.u();
        // brass and winds have no short samples of their own: play the sustain, cut short
        shortCut = (art == AShort || art == APizz) && zone->loopStart >= 0;
    }
    // glide up or down by `semis` over `sec` (the orchestral rise)
    void glide (float semis, float sec) { glideTo = semis; glideK = 1.0 / juce::jmax (1.0, (double) sec * sr); }
    void release() { if (note >= 0 && ! releasing) { releasing = true; held = false; } }
    void kill() { note = -1; }
    bool active() const { return note >= 0; }
    float warp = 0, gain = 1;      // set by the processor each block

    inline void render (float& L, float& R) noexcept
    {
        if (note < 0) return;
        if (holdLeft > 0 && --holdLeft == 0) release();
        ++t; ++age;
        const auto& d = zone->d;
        // the wobble: each player drifts a little, more as WARP goes up
        wobPh += wobRate / sr; if (wobPh >= 1) wobPh -= 1;
        double cents = warp * 18.0 * std::sin (kTwoPi * wobPh);
        if (tapeMode) cents += 6.0 * std::sin (kTwoPi * wobPh * 2.3);
        if (glideK > 0) { bend += (glideTo - bend) * glideK * 4.0; }
        const double rate = baseRate * std::pow (2.0, (cents / 100.0 + bend) / 12.0);
        const int i = (int) pos;
        if (i + 1 >= (int) d.size() - 1)
        {
            if (zone->loopStart >= 0 && ! shortCut) { pos -= (double) ((int) d.size() - 2 - zone->loopStart); }
            else { note = -1; return; }
        }
        const int i0 = (int) pos; const float fr = (float) (pos - i0);
        float y = d[(size_t) i0] + (d[(size_t) i0 + 1] - d[(size_t) i0]) * fr;
        pos += rate;
        // envelope
        if (env < 1.0f) env = juce::jmin (1.0f, env + (float) (1.0 / (attack * sr)));
        float a = env * env;
        const double ts = (double) t / sr;
        if (shortCut) { if (ts > (artic == APizz ? 0.12 : 0.16)) release(); }
        if (tapeMode && ts > 7.0) { a *= (float) juce::jmax (0.0, 1.0 - (ts - 7.0)); if (ts > 8.0) { note = -1; return; } }   // 8 seconds of tape per key
        if (releasing)
        {
            const double rt = artic == ASus || artic == ATrem ? (inst >= TPT ? 0.12 : 0.28) : 0.08;
            relEnv *= (float) std::exp (-1.0 / (rt * sr));
            if (relEnv < 1.0e-4f) { note = -1; return; }
            a *= relEnv;
        }
        y *= a * gain * (0.35f + 0.65f * velocity);
        const float pl = std::sqrt (0.5f * (1.0f - pan)), pr = std::sqrt (0.5f * (1.0f + pan));
        L += y * pl; R += y * pr;
    }
    int inst = 0;

private:
    double sr = 44100, pos = 0, baseRate = 1, glideTo = 0, glideK = 0, bend = 0, wobPh = 0, wobRate = 0.4;
    float velocity = 0.7f, env = 0, relEnv = 1, attack = 0.01f;
    int artic = 0;
    bool releasing = false, tapeMode = false, shortCut = false;
    int64_t t = 0, holdLeft = -1;
    const Zone* zone = nullptr;
};

// the room: a plate-ish reverb, a tape machine for the tape modes and AGE, tone and output
class Rack
{
public:
    void prepare (double s)
    {
        sr = s;
        reverb.setSampleRate (sr);
        dl.assign ((size_t) (sr * 0.05) + 4, 0.0f); dr.assign (dl.size(), 0.0f); w = 0;
    }
    struct Params { float room = 0.3f, tape = 0.0f, tone = 0.0f, out = 1.0f, width = 1.0f; };
    void set (const Params& p)
    {
        P = p;
        juce::Reverb::Parameters rp; rp.roomSize = 0.62f; rp.damping = 0.45f; rp.width = 1.0f;
        rp.wetLevel = p.room * 0.42f; rp.dryLevel = 1.0f - p.room * 0.22f;
        reverb.setParameters (rp);
    }
    void process (float* L, float* R, int n, Rng& rng)
    {
        const size_t N = dl.size();
        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R[i];
            // width: 0 = mono (like a 60s mono mix), 1 = the sections spread across the stereo field
            const float m = 0.5f * (l + r), s = 0.5f * (l - r) * P.width;
            l = m + s; r = m - s;
            if (P.tape > 0.001f)
            {
                wob += (0.6 + 0.2 * rng.u()) / sr; if (wob >= 1) wob -= 1;
                dl[w] = l; dr[w] = r;
                const double dly = (0.006 + 0.0025 * P.tape * std::sin (kTwoPi * wob)) * sr;
                const double rp = (double) w - dly + (double) N;
                const size_t a = (size_t) rp % N, b = (a + 1) % N; const float fr = (float) (rp - std::floor (rp));
                l = dl[a] + (dl[b] - dl[a]) * fr; r = dr[a] + (dr[b] - dr[a]) * fr;
                w = (w + 1) % N;
                const float k = 1.0f - P.tape * 0.75f;
                tl += (l - tl) * k; tr += (r - tr) * k;
                l = std::tanh (tl * (1.0f + P.tape)) / (1.0f + P.tape * 0.5f) + rng.n() * 0.002f * P.tape;
                r = std::tanh (tr * (1.0f + P.tape)) / (1.0f + P.tape * 0.5f) + rng.n() * 0.002f * P.tape;
            }
            if (std::abs (P.tone) > 0.01f)
            {
                ql += (l - ql) * 0.05f; qr += (r - qr) * 0.05f;
                l += (l - ql) * P.tone * 0.8f - ql * P.tone * 0.25f;
                r += (r - qr) * P.tone * 0.8f - qr * P.tone * 0.25f;
            }
            L[i] = l; R[i] = r;
        }
        if (P.room > 0.001f) reverb.processStereo (L, R, n);
        for (int i = 0; i < n; ++i) { L[i] *= P.out; R[i] *= P.out; }
    }
private:
    double sr = 44100, wob = 0;
    juce::Reverb reverb;
    std::vector<float> dl, dr; size_t w = 0;
    float tl = 0, tr = 0, ql = 0, qr = 0;
    Params P;
};
} // namespace ch
