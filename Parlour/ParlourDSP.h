#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>

// ---------------------------------------------------------------------------
// Parlour's sounds. Real samples where they exist (upright piano, vibraphone,
// glockenspiel, all CC0 from the VCSL library) and modelled keyboards for the
// electric ones: a tine electric piano, an 80s digital FM piano, a tonewheel-
// style drawbar organ, a clav, a string machine, synth brass and a poly pad.
// ---------------------------------------------------------------------------
namespace pk
{
constexpr double kTwoPi = 6.283185307179586476925;

enum Instrument { IPiano = 0, IEPiano, IFMPiano, IOrgan, IClav, IStrings, IBrass, IPad, IVibes, IGlock, ILofiPiano, NumInstruments };
inline const char* instrumentName (int i)
{
    static const char* n[] = { "Upright Piano", "Electric Piano", "80s Digital Piano", "Drawbar Organ", "Clav", "String Machine",
                               "Synth Brass", "80s Poly Pad", "Vibraphone", "Glockenspiel", "Lo-fi Piano" };
    return (i >= 0 && i < NumInstruments) ? n[i] : "";
}
inline bool usesSamples (int i) { return i == IPiano || i == IVibes || i == IGlock || i == ILofiPiano; }
// which sample set an instrument plays (0 piano, 1 vibes, 2 glock)
inline int sampleSet (int i) { return i == IVibes ? 1 : (i == IGlock ? 2 : 0); }

struct Rng
{
    uint32_t s = 0x9e3779b9u;
    void seed (uint32_t x) { s = x ? x : 1u; }
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float u() { return (float) (next() & 0xffffff) / 16777216.0f; }
    float n() { return u() * 2.0f - 1.0f; }
    bool chance (float p) { return u() < p; }
    int i (int n) { return n <= 1 ? 0 : (int) (next() % (uint32_t) n); }
};

// ---- samples -----------------------------------------------------------------
struct Zone
{
    int root = 60, layer = 0;
    double sr = 44100;
    std::vector<float> l, r;     // stereo, with 2 samples of padding
};
struct SampleSet
{
    std::vector<Zone> zones;
    const Zone* find (int note, int layer) const
    {
        const Zone* best = nullptr; int bestD = 1000;
        for (const auto& z : zones)
        {
            const int d = std::abs (z.root - note) * 2 + (z.layer == layer ? 0 : 7);   // a nearby root matters more than the layer
            if (d < bestD) { bestD = d; best = &z; }
        }
        return best;
    }
};
struct SampleLibrary { SampleSet sets[3]; };

// ---- one voice ---------------------------------------------------------------
class Voice
{
public:
    int note = -1;          // -1 = free
    bool held = false;
    int64_t age = 0;

    void prepare (double s) { sr = s; }

    void start (int instrument, int midi, float vel, float lengthSec, const SampleLibrary* lib, Rng& rng)
    {
        inst = instrument; note = midi; velocity = vel; held = true; releasing = false;
        holdLeft = lengthSec > 0 ? (int64_t) (lengthSec * sr) : -1;
        t = 0; env = 0; relEnv = 1; age = 0;
        freq = 440.0 * std::pow (2.0, (midi - 69) / 12.0);
        detune = 1.0 + rng.n() * 0.0012;                     // keys are never perfectly in tune
        for (auto& p : ph) p = rng.u();
        lp1 = lp2 = 0; ks.clear();
        zone = nullptr;
        if (usesSamples (inst) && lib != nullptr)
        {
            const auto& set = lib->sets[sampleSet (inst)];
            zone = set.find (midi, vel > 0.58f ? 1 : 0);
            if (zone != nullptr)
            {
                pos = 0;
                rate = std::pow (2.0, (midi - zone->root) / 12.0) * zone->sr / sr * detune;
            }
        }
        if (inst == IClav)
        {
            // a plucked string: a burst of filtered noise in a delay line (Karplus-Strong)
            const int n = juce::jmax (2, (int) (sr / (freq * detune)));
            ks.assign ((size_t) n, 0.0f);
            float y = 0;
            for (auto& s : ks) { const float x = rng.n(); y += (x - y) * (0.35f + 0.6f * vel); s = y; }
            ksPos = 0;
        }
    }
    void release() { if (note >= 0 && ! releasing) { releasing = true; held = false; } }
    void kill() { note = -1; }
    bool active() const { return note >= 0; }

    // renders one stereo sample, adding into L/R
    inline void render (float& L, float& R) noexcept
    {
        if (note < 0) return;
        if (holdLeft > 0 && --holdLeft == 0) release();
        ++age; ++t;
        const double ts = (double) t / sr;
        float l = 0, r = 0;

        switch (inst)
        {
            case IPiano: case IVibes: case IGlock: case ILofiPiano:
            {
                if (zone == nullptr) { note = -1; return; }
                const int i = (int) pos;
                if (i + 1 >= (int) zone->l.size() - 2) { note = -1; return; }
                const float fr = (float) (pos - i);
                l = zone->l[(size_t) i] + (zone->l[(size_t) i + 1] - zone->l[(size_t) i]) * fr;
                r = zone->r[(size_t) i] + (zone->r[(size_t) i + 1] - zone->r[(size_t) i]) * fr;
                pos += rate;
                const float g = 0.35f + 0.65f * velocity;
                l *= g; r *= g;
                break;
            }
            case IEPiano:
            {
                // tine piano: a sine carrier, a decaying 1:1 modulator for the bark, a short high tine "ping"
                const double f = freq * detune;
                const double idx = (0.6 + 2.6 * velocity) * std::exp (-ts * (2.2 + 0.004 * f));
                ph[1] += f / sr; ph[1] -= std::floor (ph[1]);
                ph[2] += f * 14.0 / sr; ph[2] -= std::floor (ph[2]);
                const double mod = std::sin (kTwoPi * ph[1]) * idx;
                ph[0] += f / sr; ph[0] -= std::floor (ph[0]);
                const double body = std::sin (kTwoPi * ph[0] + mod);
                const double ping = std::sin (kTwoPi * ph[2]) * 0.22 * velocity * std::exp (-ts * 38.0);
                const double decay = std::exp (-ts * (0.55 + f * 0.0012));
                const float y = (float) ((body * decay + ping) * 0.55);
                l = r = y;
                break;
            }
            case IFMPiano:
            {
                // the bright 80s digital piano: 1:1 for body, a bell partial at 1:7 that fades fast
                const double f = freq * detune;
                ph[1] += f / sr; ph[1] -= std::floor (ph[1]);
                ph[3] += f * 7.0 / sr; ph[3] -= std::floor (ph[3]);
                const double idx = (1.0 + 2.0 * velocity) * std::exp (-ts * 3.0) + 0.25;
                const double bell = std::sin (kTwoPi * ph[3]) * (1.6 * velocity) * std::exp (-ts * 9.0);
                ph[0] += f / sr; ph[0] -= std::floor (ph[0]);
                ph[2] += f * 1.003 / sr; ph[2] -= std::floor (ph[2]);
                const double a = std::sin (kTwoPi * ph[0] + std::sin (kTwoPi * ph[1]) * idx + bell);
                const double b = std::sin (kTwoPi * ph[2] + std::sin (kTwoPi * ph[1]) * idx * 0.7);
                const double decay = std::exp (-ts * (0.7 + f * 0.0010));
                l = (float) ((a * 0.7 + b * 0.3) * decay * 0.5);
                r = (float) ((a * 0.3 + b * 0.7) * decay * 0.5);
                break;
            }
            case IOrgan:
            {
                // drawbars 16' 8' 5 1/3' 4' 2 2/3' 2' (a classic "gospel" registration), key click, percussion on the 3rd harmonic
                static const double ratio[6] = { 0.5, 1.0, 1.5, 2.0, 3.0, 4.0 };
                static const double level[6] = { 0.55, 0.8, 0.45, 0.5, 0.3, 0.25 };
                double y = 0;
                for (int k = 0; k < 6; ++k)
                {
                    ph[k] += freq * ratio[k] / sr; ph[k] -= std::floor (ph[k]);
                    y += std::sin (kTwoPi * ph[k]) * level[k];
                }
                const double perc = std::sin (kTwoPi * ph[4]) * 0.6 * std::exp (-ts * 7.0);
                const double click = t < (int64_t) (sr * 0.004) ? (std::sin (kTwoPi * ph[5] * 3.1) * 0.25 * (1.0 - ts / 0.004)) : 0.0;
                l = r = (float) ((y + perc + click) * 0.22);
                break;
            }
            case IClav:
            {
                if (ks.empty()) { note = -1; return; }
                const size_t n = ks.size();
                const size_t j = (ksPos + 1) % n;
                const float damp = 0.4985f + 0.0013f * velocity;     // bright, short-ish ring
                const float y = (ks[ksPos] + ks[j]) * damp;
                ks[ksPos] = y; ksPos = j;
                // the pickup: a high-passed, slightly honky tone
                lp1 += (y - lp1) * 0.08f;
                const float o = (y - lp1) * 1.4f;
                l = r = std::tanh (o * 1.5f) * 0.6f;
                break;
            }
            case IStrings:
            {
                // string machine: three detuned saws per note, slow attack, the ensemble chorus does the rest
                double y = 0;
                static const double det[3] = { 0.9965, 1.0, 1.0042 };
                for (int k = 0; k < 3; ++k)
                {
                    ph[k] += freq * det[k] * detune / sr; ph[k] -= std::floor (ph[k]);
                    y += 2.0 * ph[k] - 1.0;
                }
                ph[3] += freq * 2.0 / sr; ph[3] -= std::floor (ph[3]);
                y += (2.0 * ph[3] - 1.0) * 0.5;      // the octave-up "violin" register
                l = r = (float) lowpass ((float) (y * 0.22), 2600.0f + 1800.0f * velocity);
                break;
            }
            case IBrass:
            {
                // synth brass: two saws, the filter swells open then settles
                ph[0] += freq * 0.998 * detune / sr; ph[0] -= std::floor (ph[0]);
                ph[1] += freq * 1.003 * detune / sr; ph[1] -= std::floor (ph[1]);
                const double y = (2.0 * ph[0] - 1.0) + (2.0 * ph[1] - 1.0);
                const double swell = ts < 0.06 ? ts / 0.06 : std::exp (-(ts - 0.06) * 2.5) * 0.5 + 0.5;
                l = r = (float) lowpass ((float) (y * 0.3), (float) (300.0 + (2200.0 + 2600.0 * velocity) * swell));
                break;
            }
            default:   // IPad
            {
                // 80s poly pad: saw + square, slow filter sweep
                ph[0] += freq * detune / sr; ph[0] -= std::floor (ph[0]);
                ph[1] += freq * 1.006 / sr; ph[1] -= std::floor (ph[1]);
                const double y = (2.0 * ph[0] - 1.0) * 0.6 + (ph[1] < 0.5 ? 0.45 : -0.45);
                const double sweep = 1.0 - std::exp (-ts * 0.9);
                const float lp = lowpass ((float) (y * 0.3), (float) (500.0 + 2400.0 * sweep * (0.6 + 0.4 * velocity)));
                l = lp; r = lp;
                break;
            }
        }

        // amplitude envelope per family
        float a = 1.0f;
        const double atk = inst == IStrings ? 0.18 : inst == IPad ? 0.35 : inst == IBrass ? 0.03 : inst == IOrgan ? 0.004 : 0.001;
        if (env < 1.0f) { env = juce::jmin (1.0f, env + (float) (1.0 / (atk * sr))); }
        a = env;
        if (releasing)
        {
            const double relT = usesSamples (inst) ? (inst == IVibes ? 0.35 : inst == IGlock ? 0.6 : 0.22)
                              : inst == IStrings ? 0.45 : inst == IPad ? 0.8 : inst == IBrass ? 0.18 : inst == IOrgan ? 0.03
                              : inst == IClav ? 0.04 : 0.3;
            relEnv *= (float) std::exp (-1.0 / (relT * sr));
            if (relEnv < 1.0e-4f) { note = -1; return; }
            a *= relEnv;
        }
        // per-instrument level, so switching sounds keeps the mix roughly where it was
        static const float trim[NumInstruments] = { 0.22f, 0.22f, 0.26f, 0.24f, 0.36f, 0.22f, 0.42f, 0.34f, 0.13f, 0.35f, 0.4f };
        const float g = (usesSamples (inst) ? 1.0f : (0.4f + 0.6f * velocity)) * trim[inst];
        L += l * a * g; R += r * a * g;
    }

private:
    float lowpass (float x, float cutoff)
    {
        const float g = (float) (1.0 - std::exp (-kTwoPi * juce::jlimit (40.0f, (float) (sr * 0.45), cutoff) / sr));
        lp1 += g * (x - lp1); lp2 += g * (lp1 - lp2);
        return lp2;
    }

    double sr = 44100, freq = 440, detune = 1, pos = 0, rate = 1;
    double ph[6] {};
    int inst = 0;
    float velocity = 0.8f, env = 0, relEnv = 1, lp1 = 0, lp2 = 0;
    bool releasing = false;
    int64_t t = 0, holdLeft = -1;
    const Zone* zone = nullptr;
    std::vector<float> ks; size_t ksPos = 0;
};

// ---- the box the keys go through -------------------------------------------
// Motion = tremolo for the electric pianos and vibes, a rotating speaker for the
// organ, the ensemble for strings and pads. Then lo-fi, tone, a room, output.
class Rack
{
public:
    void prepare (double s, int maxBlock)
    {
        sr = s;
        const int n = (int) (sr * 0.05) + 4;
        dl.assign ((size_t) n, 0.0f); dr.assign ((size_t) n, 0.0f); w = 0;
        reverb.setSampleRate (sr);
        juce::ignoreUnused (maxBlock);
        lfo = lfo2 = 0; rotorSpeed = 0.8; hissL = hissR = 0; toneL = toneR = 0; wob = 0;
    }
    struct Params { int instrument = 0; float motion = 0.3f, chorus = 0.2f, room = 0.2f, lofi = 0.0f, tone = 0.0f, out = 1.0f; bool fastRotor = false; };
    void set (const Params& p)
    {
        P = p;
        juce::Reverb::Parameters rp; rp.roomSize = 0.55f; rp.damping = 0.55f; rp.width = 0.9f;
        rp.wetLevel = p.room * 0.45f; rp.dryLevel = 1.0f - p.room * 0.25f;
        reverb.setParameters (rp);
    }

    void process (float* L, float* R, int n, Rng& rng)
    {
        const int inst = P.instrument;
        const bool trem = inst == IEPiano || inst == IFMPiano || inst == IVibes;
        const bool rotor = inst == IOrgan;
        const bool ens = inst == IStrings || inst == IPad || inst == IBrass;
        const float lofi = inst == ILofiPiano ? juce::jmax (0.55f, P.lofi) : P.lofi;
        const size_t N = dl.size();
        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R[i];
            if (trem && P.motion > 0.001f)
            {
                // stereo tremolo (panning) like a suitcase electric piano
                lfo += (inst == IVibes ? 5.5 : 4.2) / sr; if (lfo >= 1) lfo -= 1;
                const float s = (float) std::sin (kTwoPi * lfo) * P.motion * 0.8f;
                l *= 1.0f - juce::jmax (0.0f, s); r *= 1.0f - juce::jmax (0.0f, -s);
            }
            // write into the modulated delay (shared by rotor, ensemble, chorus, lo-fi wow)
            dl[w] = l; dr[w] = r;
            float wetL = 0, wetR = 0; bool useWet = false;
            if (rotor)
            {
                const double target = P.fastRotor ? 6.8 : 0.8;
                rotorSpeed += (target - rotorSpeed) * (P.fastRotor ? 0.6 : 0.3) / sr;   // the horn spins up and down slowly
                lfo += rotorSpeed / sr; if (lfo >= 1) lfo -= 1;
                const float s = (float) std::sin (kTwoPi * lfo), c = (float) std::cos (kTwoPi * lfo);
                const float depth = 0.3f + 0.7f * P.motion;
                wetL = tap (dl, 0.0015f + 0.0012f * depth * (1 + s)) * (1.0f + 0.35f * depth * s);
                wetR = tap (dr, 0.0015f + 0.0012f * depth * (1 + c)) * (1.0f + 0.35f * depth * c);
                useWet = true;
            }
            else if (ens || P.chorus > 0.001f)
            {
                lfo2 += 0.6 / sr; if (lfo2 >= 1) lfo2 -= 1;
                const double a = kTwoPi * lfo2;
                const float amt = ens ? juce::jmax (0.6f, P.chorus) : P.chorus;
                // three taps, 120 degrees apart: the string-machine ensemble
                const float d1 = 0.007f + 0.0025f * (float) std::sin (a), d2 = 0.007f + 0.0025f * (float) std::sin (a + 2.094), d3 = 0.007f + 0.0025f * (float) std::sin (a + 4.189);
                const float cl = (tap (dl, d1) + tap (dl, d2) * 0.6f) / 1.6f, cr = (tap (dr, d3) + tap (dr, d2) * 0.6f) / 1.6f;
                wetL = l * (1.0f - amt * 0.5f) + cl * amt; wetR = r * (1.0f - amt * 0.5f) + cr * amt;
                useWet = true;
            }
            if (lofi > 0.001f)
            {
                // tape wow and a dusty top end
                wob += (0.55 + 0.3 * rng.u()) / sr; if (wob >= 1) wob -= 1;
                const float d = 0.004f + 0.0022f * lofi * (float) std::sin (kTwoPi * wob);
                const float wl = tap (dl, d), wr = tap (dr, d);
                wetL = useWet ? (wetL + wl) * 0.5f : wl; wetR = useWet ? (wetR + wr) * 0.5f : wr; useWet = true;
            }
            if (useWet) { l = wetL; r = wetR; }
            if (++w >= N) w = 0;

            if (lofi > 0.001f)
            {
                const float k = 1.0f - lofi * 0.82f;            // darker the more lo-fi
                toneL += (l - toneL) * k; toneR += (r - toneR) * k; l = toneL; r = toneR;
                hissL = rng.n() * 0.004f * lofi; hissR = rng.n() * 0.004f * lofi;
                if (rng.chance (0.00012f * lofi)) { hissL += rng.n() * 0.12f * lofi; hissR += hissL; }
                l += hissL; r += hissR;
            }
            // tone: a gentle tilt
            if (std::abs (P.tone) > 0.01f)
            {
                tiltL += (l - tiltL) * 0.06f; tiltR += (r - tiltR) * 0.06f;
                l += (l - tiltL) * P.tone * 0.9f - tiltL * P.tone * 0.3f;
                r += (r - tiltR) * P.tone * 0.9f - tiltR * P.tone * 0.3f;
            }
            L[i] = l; R[i] = r;
        }
        if (P.room > 0.001f) reverb.processStereo (L, R, n);
        for (int i = 0; i < n; ++i) { L[i] *= P.out; R[i] *= P.out; }
    }

private:
    float tap (const std::vector<float>& d, float sec) const
    {
        const size_t N = d.size();
        const double p = (double) w - sec * sr;
        double q = p < 0 ? p + (double) N : p;
        const size_t i = (size_t) q % N, j = (i + 1) % N;
        const float fr = (float) (q - std::floor (q));
        return d[i] + (d[j] - d[i]) * fr;
    }
    double sr = 44100, lfo = 0, lfo2 = 0, rotorSpeed = 0.8, wob = 0;
    std::vector<float> dl, dr; size_t w = 0;
    juce::Reverb reverb;
    Params P;
    float hissL = 0, hissR = 0, toneL = 0, toneR = 0, tiltL = 0, tiltR = 0;
};
} // namespace pk
