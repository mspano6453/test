#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <vector>

// ---------------------------------------------------------------------------
// Harmonia's singers. Each singer is a voice built the way a throat builds
// one: a breathy buzz (the vocal folds) shaped by four resonances (the mouth
// and throat, the "formants" that make an "ooh" an "ooh"). A section is a few
// singers on the same part, each a touch out of tune and out of time with the
// others, with their own vibrato: that's what makes a choir sound like people.
// ---------------------------------------------------------------------------
namespace hm
{
constexpr double kTwoPi = 6.283185307179586;
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

enum Vowel { VOo = 0, VOh, VAh, VEe, VMm, NumVowels };
// how a sung note starts: straight in, a "d", a "b", or sliding from "oo" to "ah" ("wah")
enum Onset { ONone = 0, OD, OB, OWah };
struct Syllable { int vowel = VOo; int onset = ONone; };

struct Formants { float f[4], g[4]; };
inline Formants formantsFor (int vowel, bool female)
{
    static const float M[NumVowels][4] = { { 300, 870, 2240, 3300 }, { 570, 840, 2410, 3300 }, { 730, 1090, 2440, 3400 }, { 270, 2290, 3010, 3700 }, { 250, 1100, 2500, 3400 } };
    static const float W[NumVowels][4] = { { 370, 950, 2670, 3600 }, { 590, 920, 2710, 3700 }, { 850, 1220, 2810, 3900 }, { 310, 2790, 3310, 4200 }, { 280, 1300, 2800, 3800 } };
    static const float G[NumVowels][4] = { { 0, -10, -28, -34 }, { 0, -6, -26, -32 }, { 0, -4, -22, -28 }, { 0, -18, -20, -28 }, { 0, -26, -36, -42 } };
    Formants r;
    for (int i = 0; i < 4; ++i) { r.f[i] = (female ? W : M)[vowel][i]; r.g[i] = std::pow (10.0f, G[vowel][i] / 20.0f); }
    return r;
}

struct Bpf
{
    float b0 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void set (double sr, double f, double bw)
    {
        f = juce::jlimit (60.0, sr * 0.45, f);
        const double w = kTwoPi * f / sr, q = f / juce::jmax (20.0, bw), al = std::sin (w) / (2 * q), a0 = 1 + al;
        b0 = (float) (al / a0); b2 = (float) (-al / a0); a1 = (float) (-2 * std::cos (w) / a0); a2 = (float) ((1 - al) / a0);
    }
    float process (float x) { const float y = b0 * x + z1; z1 = -a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void reset() { z1 = z2 = 0; }
};

// one singer
class Singer
{
public:
    void prepare (double s, uint32_t seed) { sr = s; rng.seed (seed); vibRate = 4.8f + rng.u() * 1.0f; driftTarget = rng.n(); reset(); }
    void reset() { env = 0; gate = false; active = false; phase = rng.u(); for (auto& b : bp) b.reset(); burst = 0; lp = 0; }
    bool isActive() const { return active; }
    // sing a note (glides there if already singing and `legato`)
    void sing (int midiNote, float vel, Syllable syl, bool legato, double attackSec, bool femaleIn, float vib, float breathIn, float detuneCents, float delaySec)
    {
        female = femaleIn; vibDepth = vib; breath = breathIn; detune = detuneCents;
        targetNote = (double) midiNote;
        if (! active || ! legato || ! gate) { note = targetNote; vibAge = 0; }
        level = vel;
        att = (float) (1.0 / juce::jmax (1.0, attackSec * sr));
        gate = true; active = true;
        startDelay = (int) (delaySec * sr);
        // the syllable: where the mouth starts and where it goes
        fromF = curF.f[0] > 0 && legato ? curF : formantsFor (syl.onset == OWah ? VOo : syl.vowel, female);
        toF = formantsFor (syl.vowel, female);
        morph = 0; morphRate = (float) (1.0 / ((syl.onset == OWah ? 0.12 : legato ? 0.09 : 0.03) * sr));
        if (syl.onset == OD || syl.onset == OB) { burst = 1.0f; burstLow = syl.onset == OB; dip = 1.0f; }
        else dip = 0.0f;
        if (! legato) env = juce::jmin (env, 0.15f);
    }
    void release (double sec) { gate = false; rel = (float) (1.0 / juce::jmax (1.0, sec * sr)); }
    void render (float* L, float* R, int n, float pan)
    {
        if (! active) return;
        const float gl = std::sqrt (0.5f * (1.0f - pan)), gr = std::sqrt (0.5f * (1.0f + pan));
        for (int i = 0; i < n; ++i)
        {
            if (startDelay > 0) { --startDelay; continue; }
            // the mouth moves (every 16 samples is plenty)
            if ((counter++ & 15) == 0)
            {
                if (morph < 1.0f) morph = juce::jmin (1.0f, morph + morphRate * 16.0f);
                for (int k = 0; k < 4; ++k)
                {
                    curF.f[k] = fromF.f[k] + (toF.f[k] - fromF.f[k]) * morph;
                    curF.g[k] = fromF.g[k] + (toF.g[k] - fromF.g[k]) * morph;
                    static const float bw[4] = { 80, 100, 140, 180 };
                    bp[k].set (sr, curF.f[k] * (1.0f + 0.01f * driftNow), bw[k] * (female ? 1.15f : 1.0f));
                }
                // slow wander in pitch, the way a held note never quite sits still
                driftNow += (driftTarget - driftNow) * 0.0008f * 16.0f;
                if (rng.chance (0.0015f * 16.0f)) driftTarget = rng.n();
            }
            note += (targetNote - note) * (1.0 - std::exp (-1.0 / (0.045 * sr)));   // a glide between notes
            vibAge += 1.0 / sr;
            const double vibIn = juce::jmin (1.0, juce::jmax (0.0, (vibAge - 0.25) / 0.5));
            vibPh += vibRate / sr; if (vibPh > 1) vibPh -= 1;
            const double cents = detune + driftNow * 7.0 + vibDepth * 32.0 * vibIn * std::sin (kTwoPi * vibPh);
            const double f0 = 440.0 * std::pow (2.0, (note - 69.0 + cents / 100.0) / 12.0);
            const double dt = f0 / sr;
            phase += dt; if (phase >= 1.0) phase -= 1.0;
            // the folds: a saw with its corners smoothed (polyBLEP), tilted darker
            double s = 2.0 * phase - 1.0;
            if (phase < dt) { const double t = phase / dt; s -= t + t - t * t - 1.0; }
            else if (phase > 1.0 - dt) { const double t = (phase - 1.0) / dt; s -= t * t + t + t + 1.0; }
            lp += 0.35f * ((float) s - lp);
            const float noise = rng.n();
            float src = lp * 0.9f + noise * breath * 0.25f;
            if (burst > 0.0f) { src += noise * burst * (burstLow ? 0.6f : 0.9f); burst *= burstLow ? 0.985f : 0.97f; if (burst < 0.001f) burst = 0; }
            float y = 0;
            for (int k = 0; k < 4; ++k) y += bp[k].process (src) * curF.g[k];
            y += noise * breath * 0.02f;                                    // air
            // the envelope (with a little dip for consonants)
            if (gate) env = juce::jmin (1.0f, env + att);
            else { env -= rel; if (env <= 0) { env = 0; active = false; return; } }
            if (dip > 0) dip = juce::jmax (0.0f, dip - 0.004f);
            const float v = y * env * level * (1.0f - 0.7f * dip) * 2.2f;
            L[i] += v * gl; R[i] += v * gr;
        }
    }
    int part = -1;
private:
    double sr = 44100, phase = 0, note = 60, targetNote = 60, vibPh = 0, vibAge = 0, detune = 0;
    float env = 0, att = 0.001f, rel = 0.001f, level = 0.8f, vibRate = 5, vibDepth = 0.5f, breath = 0.2f;
    float driftNow = 0, driftTarget = 0, morph = 1, morphRate = 0.01f, burst = 0, dip = 0, lp = 0;
    bool gate = false, active = false, female = false, burstLow = false;
    int startDelay = 0; uint32_t counter = 0;
    Formants fromF {}, toF {}, curF {};
    Bpf bp[4];
    Rng rng;
};

// the room the backing singers stand in
class Room
{
public:
    void prepare (double s, int maxBlock)
    {
        juce::Reverb::Parameters rp; rp.roomSize = 0.7f; rp.damping = 0.45f; rp.width = 1.0f; rp.wetLevel = 1.0f; rp.dryLevel = 0.0f;
        reverb.setParameters (rp); reverb.setSampleRate (s); reverb.reset();
        wl.assign ((size_t) maxBlock, 0.0f); wr.assign ((size_t) maxBlock, 0.0f);
        sr = s;
    }
    void process (float* L, float* R, int n, float amount, float bright, float width, float out)
    {
        // tone: brighter or warmer
        const float k = (float) (1.0 - std::exp (-kTwoPi * (2500.0 + 9000.0 * bright) / sr));
        for (int i = 0; i < n; ++i) { tl += k * (L[i] - tl); tr += k * (R[i] - tr); L[i] = tl; R[i] = tr; }
        for (int i = 0; i < n; ++i) { const float m = 0.5f * (L[i] + R[i]), s = 0.5f * (L[i] - R[i]) * (0.4f + 1.2f * width); L[i] = m + s; R[i] = m - s; }
        if (amount > 0.001f && (int) wl.size() >= n)
        {
            std::copy (L, L + n, wl.begin()); std::copy (R, R + n, wr.begin());
            reverb.processStereo (wl.data(), wr.data(), n);
            for (int i = 0; i < n; ++i) { L[i] += wl[(size_t) i] * amount * 0.5f; R[i] += wr[(size_t) i] * amount * 0.5f; }
        }
        for (int i = 0; i < n; ++i) { L[i] *= out; R[i] *= out; }
    }
private:
    juce::Reverb reverb;
    std::vector<float> wl, wr;
    float tl = 0, tr = 0; double sr = 44100;
};
} // namespace hm
