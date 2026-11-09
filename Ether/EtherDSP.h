#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include <vector>

// ---------------------------------------------------------------------------
// Ether: the strange early electronic instruments, modelled from scratch.
//   Wand            - the hands-in-the-air sci-fi instrument: pure, gliding, wide vibrato
//   Wave Ribbon     - the French ribbon keyboard: sweet sine with a metallic resonator
//   Subharmonic Wire- the wire-and-resistor instrument: buzzing saw plus undertones
//   Tape Choir      - the tape-replay keyboard's "aah" choir (8 seconds of tape per key)
//   Stylus Buzz     - the pocket stylus organ: a hard little pulse, no dynamics
//   Radio Workshop  - the 60s radio-studio oscillators: ring-modulated bloops
//   Valve Reed      - the valve keyboard attachment: a reedy, wobbling clarinet
// ---------------------------------------------------------------------------
namespace et
{
constexpr double kTwoPi = 6.283185307179586476925;

enum Instrument { IWand = 0, IRibbon, IWire, IChoir, IStylus, IRadio, IReed, NumInstruments };
inline const char* instrumentName (int i)
{
    static const char* n[] = { "Wand", "Wave Ribbon", "Subharmonic Wire", "Tape Choir", "Stylus Buzz", "Radio Workshop", "Valve Reed" };
    return (i >= 0 && i < NumInstruments) ? n[i] : "";
}
inline const char* instrumentBlurb (int i)
{
    static const char* n[] = { "the hands-in-the-air instrument from 50s sci-fi films", "the 1920s ribbon keyboard: sweet and singing",
                               "1930s wires and valves: buzzing, with eerie undertones", "the 60s tape-replay keyboard choir",
                               "the pocket organ you play with a metal pen", "the 60s radio studio's oscillators and tape tricks",
                               "the 50s valve keyboard: a wobbly electric clarinet" };
    return (i >= 0 && i < NumInstruments) ? n[i] : "";
}
inline bool isPoly (int i) { return i == IChoir; }

struct Rng
{
    uint32_t s = 0x2545f491u;
    void seed (uint32_t x) { s = x ? x : 1u; }
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float u() { return (float) (next() & 0xffffff) / 16777216.0f; }
    float n() { return u() * 2.0f - 1.0f; }
    bool chance (float p) { return u() < p; }
    int i (int n) { return n <= 1 ? 0 : (int) (next() % (uint32_t) n); }
};

struct BandPass
{
    float b0 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void set (double sr, double f, double q)
    {
        const double w = kTwoPi * f / sr, a = std::sin (w) / (2 * q), c = std::cos (w), n = 1 + a;
        b0 = (float) (a / n); b2 = (float) (-a / n); a1 = (float) (-2 * c / n); a2 = (float) ((1 - a) / n);
    }
    float process (float x) { const float y = b0 * x + z1; z1 = -a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
};

inline float blep (double t, double dt)
{
    if (t < dt) { const double x = t / dt; return (float) (x + x - x * x - 1.0); }
    if (t > 1.0 - dt) { const double x = (t - 1.0) / dt; return (float) (x * x + x + x + 1.0); }
    return 0.0f;
}

class Voice
{
public:
    int note = -1; bool held = false; int64_t age = 0;
    void prepare (double s)
    {
        sr = s;
        f1.set (sr, 700, 4); f2.set (sr, 1150, 5); f3.set (sr, 2800, 7); wireF.set (sr, 950, 2.2);
        res.assign ((size_t) (sr * 0.01) + 2, 0.0f);
    }
    // glide = seconds to reach a new pitch when the voice is already sounding
    void start (int instrument, int midi, float vel, float lengthSec, float glide, Rng& r)
    {
        inst = instrument;
        const double f = 440.0 * std::pow (2.0, (midi - 69) / 12.0);
        const bool sounding = note >= 0 && amp > 0.02f;
        const bool legato = sounding && glide > 0.001f && ! isPoly (inst) && inst != IStylus && inst != IRadio;
        target = f;
        if (legato) glideK = 1.0 - std::exp (-1.0 / (glide * sr));
        else
        {
            // a fresh note: jump to the pitch; if the voice was still ringing, carry on from where it is (no click)
            freq = f; glideK = 1.0; t = 0;
            if (! sounding) { amp = 0; for (auto& p : ph) p = r.u() * 0.1; vibPh = r.u(); }
        }
        note = midi; velocity = inst == IStylus ? 0.8f : vel; held = true; releasing = false; age = 0;
        holdLeft = lengthSec > 0 ? (int64_t) (lengthSec * sr) : -1;
        tsNote = 0;
    }
    void release() { if (note >= 0) { releasing = true; held = false; } }
    void kill() { note = -1; amp = 0; }
    bool active() const { return note >= 0; }
    float vibrato = 0.5f;

    inline void render (float& L, float& R) noexcept
    {
        if (note < 0) return;
        if (holdLeft > 0 && --holdLeft == 0) release();
        ++t; ++age; ++tsNote;
        freq += (target - freq) * glideK;
        const double ts = (double) tsNote / sr;

        // vibrato: most of these instruments were played with a shaking hand, coming in after the note starts
        const double vRate = inst == IReed ? 6.4 : inst == IRibbon ? 5.9 : inst == IWand ? 5.4 : 4.8;
        const double vDepth = (inst == IStylus || inst == IRadio) ? 0.0 : (inst == IChoir ? 0.002 : 0.009) * vibrato * std::min (1.0, ts / 0.35);
        vibPh += vRate / sr; if (vibPh >= 1) vibPh -= 1;
        const double f = freq * (1.0 + vDepth * std::sin (kTwoPi * vibPh));
        const double dt = f / sr;
        float y = 0, yR = 0; bool stereo = false;
        switch (inst)
        {
            case IWand:
            {
                ph[0] += dt; ph[0] -= std::floor (ph[0]);
                const double s = std::sin (kTwoPi * ph[0]);
                y = (float) (std::tanh (1.25 * s) * 0.8 + std::sin (2 * kTwoPi * ph[0]) * 0.06);
                break;
            }
            case IRibbon:
            {
                ph[0] += dt; ph[0] -= std::floor (ph[0]);
                const double s = std::sin (kTwoPi * ph[0] + 0.35 * std::sin (kTwoPi * ph[0]));
                // the metallic resonator (a short comb), like the instrument's spring speaker
                const size_t n = res.size(); const size_t d = (size_t) (sr * 0.0037);
                const float fb = res[(rp + n - d) % n];
                res[rp] = (float) s * 0.6f + fb * 0.55f; rp = (rp + 1) % n;
                y = (float) s * 0.7f + fb * 0.35f;
                break;
            }
            case IWire:
            {
                ph[0] += dt; ph[0] -= std::floor (ph[0]);
                ph[1] += dt * 0.5; ph[1] -= std::floor (ph[1]);
                ph[2] += dt / 3.0; ph[2] -= std::floor (ph[2]);
                const float saw = (float) (2 * ph[0] - 1) - blep (ph[0], dt);
                const float sub2 = (float) (2 * ph[1] - 1) - blep (ph[1], dt * 0.5);
                const float sub3 = (ph[2] < 0.5 ? 0.6f : -0.6f);
                const float x = saw * 0.5f + sub2 * 0.35f + sub3 * 0.25f;
                y = std::tanh (wireF.process (x) * 2.2f + x * 0.15f) * 0.75f;
                break;
            }
            case IChoir:
            {
                static const double det[3] = { 0.996, 1.0, 1.0048 };
                float x = 0;
                for (int k = 0; k < 3; ++k) { ph[k] += dt * det[k]; ph[k] -= std::floor (ph[k]); x += (float) (2 * ph[k] - 1) - blep (ph[k], dt * det[k]); }
                x *= 0.33f;
                y = (f1.process (x) * 1.0f + f2.process (x) * 0.7f + f3.process (x) * 0.35f) * 2.6f;
                // eight seconds of tape behind each key
                if (ts > 7.0) y *= (float) std::max (0.0, 1.0 - (ts - 7.0));
                if (ts > 8.0) { note = -1; return; }
                break;
            }
            case IStylus:
            {
                ph[0] += dt; ph[0] -= std::floor (ph[0]);
                const double w = 0.18;
                float p = (ph[0] < w ? 1.0f : -0.22f) + blep (ph[0], dt) - blep (std::fmod (ph[0] + 1.0 - w, 1.0), dt);
                lp += (p - lp) * 0.35f;
                y = lp * 0.55f;
                break;
            }
            case IRadio:
            {
                // a bloop: the pitch drops in from above, ring-modulated against a second oscillator
                const double drop = 1.0 + 1.0 * std::exp (-ts * 18.0);
                ph[0] += dt * drop; ph[0] -= std::floor (ph[0]);
                ph[1] += dt * 2.71 * drop; ph[1] -= std::floor (ph[1]);
                const double s = std::sin (kTwoPi * ph[0]), m = std::sin (kTwoPi * ph[1]);
                y = (float) (s * 0.55 + s * m * 0.45);
                ph[2] += 0.37 / sr; ph[2] -= std::floor (ph[2]);
                const float pan = (float) std::sin (kTwoPi * ph[2]) * 0.6f;
                yR = y * (1.0f + pan); y *= (1.0f - pan); stereo = true;
                break;
            }
            default:  // IReed
            {
                ph[0] += dt; ph[0] -= std::floor (ph[0]);
                float sq = (ph[0] < 0.5 ? 1.0f : -1.0f) + blep (ph[0], dt) - blep (std::fmod (ph[0] + 0.5, 1.0), dt);
                lp += (sq - lp) * 0.12f; lp2 += (lp - lp2) * 0.25f;
                y = std::tanh (lp2 * 1.8f) * 0.6f;
                break;
            }
        }
        // envelope
        const double atk = inst == IWand ? 0.09 : inst == IRibbon ? 0.03 : inst == IChoir ? 0.16 : inst == IStylus ? 0.001 : inst == IRadio ? 0.002 : 0.02;
        const double rel = inst == IWand ? 0.3 : inst == IRibbon ? 0.35 : inst == IChoir ? 0.4 : inst == IStylus ? 0.015 : inst == IRadio ? 0.12 : 0.08;
        if (! releasing) amp += (1.0f - amp) * (float) (1.0 - std::exp (-1.0 / (atk * sr)));
        else { amp *= (float) std::exp (-1.0 / (rel * sr)); if (amp < 1.0e-4f) { note = -1; return; } }
        if (inst == IRadio && ! releasing) amp *= (float) std::exp (-1.0 / (0.6 * sr));      // bloops die away
        static const float trim[NumInstruments] = { 0.5f, 0.45f, 0.95f, 0.3f, 0.85f, 0.55f, 0.45f };
        const float g = amp * (0.45f + 0.55f * velocity) * trim[inst];
        L += y * g; R += (stereo ? yR : y) * g;
    }

private:
    double sr = 44100, freq = 440, target = 440, glideK = 1, ph[4] {}, vibPh = 0;
    int inst = 0; float velocity = 0.8f, amp = 0, lp = 0, lp2 = 0;
    bool releasing = false; int64_t t = 0, tsNote = 0, holdLeft = -1;
    BandPass f1, f2, f3, wireF;
    std::vector<float> res; size_t rp = 0;
};

// tape echo + spring-ish room + age, the way these were heard on old records
class Rack
{
public:
    void prepare (double s)
    {
        sr = s;
        dl.assign ((size_t) (sr * 2.0) + 4, 0.0f); dr.assign (dl.size(), 0.0f); w = 0;
        reverb.setSampleRate (sr);
    }
    struct Params { float echo = 0.3f, echoTime = 0.375f, space = 0.4f, age = 0.2f, out = 1.0f; };
    void set (const Params& p)
    {
        P = p;
        juce::Reverb::Parameters rp; rp.roomSize = 0.7f; rp.damping = 0.4f; rp.width = 1.0f;
        rp.wetLevel = p.space * 0.5f; rp.dryLevel = 1.0f - p.space * 0.3f;
        reverb.setParameters (rp);
    }
    void process (float* L, float* R, int n, Rng& rng)
    {
        const size_t N = dl.size();
        for (int i = 0; i < n; ++i)
        {
            wob += 0.7 / sr; if (wob >= 1) wob -= 1;
            const double d = juce::jlimit (0.01, 1.9, (double) P.echoTime) * (1.0 + 0.004 * P.age * std::sin (kTwoPi * wob));
            const double rpos = (double) w - d * sr + (double) N;
            const size_t a = (size_t) rpos % N, b = (a + 1) % N; const float fr = (float) (rpos - std::floor (rpos));
            const float el = dl[a] + (dl[b] - dl[a]) * fr, er = dr[a] + (dr[b] - dr[a]) * fr;
            fl += (el - fl) * 0.35f; fr2 += (er - fr2) * 0.35f;              // each repeat a little darker
            dl[w] = L[i] + fr2 * 0.45f * (P.echo > 0 ? 1.0f : 0.0f);          // ping-pong
            dr[w] = R[i] + fl * 0.45f * (P.echo > 0 ? 1.0f : 0.0f);
            w = (w + 1) % N;
            float l = L[i] + fl * P.echo, r = R[i] + fr2 * P.echo;
            if (P.age > 0.001f)
            {
                tl += (l - tl) * (1.0f - P.age * 0.7f); tr += (r - tr) * (1.0f - P.age * 0.7f);
                l = tl + rng.n() * 0.003f * P.age; r = tr + rng.n() * 0.003f * P.age;
            }
            L[i] = l; R[i] = r;
        }
        if (P.space > 0.001f) reverb.processStereo (L, R, n);
        for (int i = 0; i < n; ++i) { L[i] *= P.out; R[i] *= P.out; }
    }
private:
    double sr = 44100, wob = 0;
    std::vector<float> dl, dr; size_t w = 0;
    float fl = 0, fr2 = 0, tl = 0, tr = 0;
    juce::Reverb reverb;
    Params P;
};
} // namespace et
