#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// Loon DSP: ambient generator that listens to the input (guitar) and grows
// pads, bells, drones, granular clouds and synthesized nature around it.
// ---------------------------------------------------------------------------
namespace ln
{
constexpr double kTwoPi = 6.283185307179586476925;
constexpr double kPi = 3.14159265358979323846;

struct Rng
{
    uint32_t s = 0x9E3779B9u;
    inline float u() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s & 0xffffff) / 16777216.0f; }   // 0..1
    inline float n() noexcept { return u() * 2.0f - 1.0f; }                                                                  // -1..1
};

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    inline float process (float x) noexcept { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void set (int type, double sr, double f, double q)   // 0 LP 1 HP 2 BP
    {
        f = std::clamp (f, 15.0, sr * 0.45);
        const double w = kTwoPi * f / sr, c = std::cos (w), al = std::sin (w) / (2.0 * std::max (0.1, q));
        double nb0, nb1, nb2;
        if (type == 0)      { nb0 = (1 - c) / 2; nb1 = 1 - c; nb2 = nb0; }
        else if (type == 1) { nb0 = (1 + c) / 2; nb1 = -(1 + c); nb2 = nb0; }
        else                { nb0 = al; nb1 = 0; nb2 = -al; }
        const double a0 = 1 + al;
        b0 = (float) (nb0 / a0); b1 = (float) (nb1 / a0); b2 = (float) (nb2 / a0);
        a1 = (float) (-2 * c / a0); a2 = (float) ((1 - al) / a0);
    }
};

inline float polyBlep (double t, double dt)
{
    if (t < dt) { t /= dt; return (float) (t + t - t * t - 1.0); }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
    return 0.0f;
}
inline void adv (double& ph, double inc) { ph += inc; if (ph >= 1.0) ph -= std::floor (ph); }

// ===========================================================================
// Scales & moods
// ===========================================================================
enum Mood { MWarm = 0, MDreamy, MHopeful, MDark, MEerie, NumMoods };
inline const char* moodName (int m) { static const char* n[] = { "Warm", "Dreamy", "Hopeful", "Dark", "Eerie" }; return n[m]; }

struct Scale { int n; int deg[7]; };
inline const Scale& scaleFor (int mood, bool minor)
{
    static const Scale s[NumMoods][2] = {
        { { 5, { 0, 2, 4, 7, 9 } },        { 5, { 0, 3, 5, 7, 10 } } },        // pentatonic
        { { 7, { 0, 2, 4, 6, 7, 9, 11 } }, { 7, { 0, 2, 3, 5, 7, 9, 10 } } },  // lydian / dorian
        { { 7, { 0, 2, 4, 5, 7, 9, 11 } }, { 7, { 0, 2, 3, 5, 7, 8, 10 } } },  // major / aeolian
        { { 7, { 0, 2, 3, 5, 7, 8, 10 } }, { 7, { 0, 2, 3, 5, 7, 8, 10 } } },  // aeolian
        { { 6, { 0, 2, 4, 6, 8, 10 } },    { 7, { 0, 1, 3, 5, 7, 8, 10 } } },  // whole tone / phrygian
    };
    return s[std::clamp (mood, 0, NumMoods - 1)][minor ? 1 : 0];
}
inline const char* pcName (int pc) { static const char* n[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; return n[((pc % 12) + 12) % 12]; }

// ===========================================================================
// Key detection from audio (Krumhansl-Schmuckler on a decaying chroma)
// ===========================================================================
class KeyDetector
{
public:
    void prepare (double newSr)
    {
        sr = newSr;
        fft = std::make_unique<juce::dsp::FFT> (12);
        buf.assign (8192, 0.0f); win.resize (4096);
        for (int i = 0; i < 4096; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * (float) kPi * (float) i / 4095.0f);
        pos = 0; for (auto& c : chroma) c = 0;
        binPc.assign (2049, -1);
        for (int k = 1; k <= 2048; ++k)
        {
            const double f = k * sr / 4096.0;
            if (f < 70 || f > 1100) continue;
            binPc[(size_t) k] = (int) std::lround (12.0 * std::log2 (f / 440.0) + 69.0) % 12;
        }
    }
    void push (const float* x, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            buf[(size_t) pos++] = x[i];
            if (pos == 4096) { analyse(); pos = 0; }
        }
    }
    std::atomic<int> root { 9 }, minor { 1 };
    std::atomic<float> confidence { 0 }, inputLevel { 0 };

private:
    void analyse()
    {
        double e = 0;
        for (int i = 0; i < 4096; ++i) e += buf[(size_t) i] * buf[(size_t) i];
        inputLevel = (float) std::sqrt (e / 4096.0);
        for (auto& c : chroma) c *= 0.992f;           // ~10 s memory: follow the song, not each chord
        if (e / 4096.0 < 1.0e-6) return;              // ignore silence
        ++framesHeard;
        for (int i = 0; i < 4096; ++i) buf[(size_t) i] *= win[(size_t) i];
        std::fill (buf.begin() + 4096, buf.end(), 0.0f);
        fft->performFrequencyOnlyForwardTransform (buf.data());
        // weight low bins more, so fundamentals count more than overtones (which pull toward the fifth)
        for (int k = 1; k <= 2048; ++k)
            if (binPc[(size_t) k] >= 0) chroma[binPc[(size_t) k]] += buf[(size_t) k] / std::sqrt ((float) k);

        static const float maj[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
        static const float mnr[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };
        float best = -2, second = -2, currentScore = -2; int bestR = 0, bestM = 0;
        for (int m = 0; m < 2; ++m)
            for (int r = 0; r < 12; ++r)
            {
                const float* prof = m == 0 ? maj : mnr;
                float mx = 0, my = 0;
                for (int i = 0; i < 12; ++i) { mx += chroma[(i + r) % 12]; my += prof[i]; }
                mx /= 12; my /= 12;
                float sxy = 0, sxx = 0, syy = 0;
                for (int i = 0; i < 12; ++i)
                {
                    const float dx = chroma[(i + r) % 12] - mx, dy = prof[i] - my;
                    sxy += dx * dy; sxx += dx * dx; syy += dy * dy;
                }
                const float c = sxy / std::sqrt (std::max (1e-12f, sxx * syy));
                if (r == root.load() && m == minor.load()) currentScore = c;
                if (c > best) { second = best; best = c; bestR = r; bestM = m; }
                else if (c > second) second = c;
            }
        confidence = best - second + best * 0.5f;
        // only change key when a new one is clearly and consistently better (about 1.5 s)
        if (best > 0.55f && (bestR != root.load() || bestM != minor.load()) && best - currentScore > 0.08f)
        {
            if (bestR == candR && bestM == candM) ++candCount; else { candR = bestR; candM = bestM; candCount = 1; }
            if (framesHeard >= 60 && (candCount >= 35 || ! locked)) { root = bestR; minor = bestM; locked = true; candCount = 0; }
        }
        else candCount = 0;
    }

    double sr = 44100;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> buf, win;
    std::vector<int> binPc;
    int pos = 0;
    float chroma[12] {};
    int candR = -1, candM = -1, candCount = 0, framesHeard = 0;
    bool locked = false;
public:
    bool hasLocked() const { return locked; }
};

// ===========================================================================
// Generative voices: pads, bells, glass, drone
// ===========================================================================
enum VoiceKind { KPad = 0, KBell, KGlass };

struct GenVoice
{
    bool active = false;
    int kind = KPad, note = 60;
    double freq = 261.6, ph[3] {}, mph = 0;
    float vel = 0.5f, env = 0, attack = 0, release = 0, hold = 0;   // hold = seconds left before release
    bool releasing = false;
    float ic1 = 0, ic2 = 0;
    float panL = 0.7f, panR = 0.7f;
    float idx = 0;   // FM index (bells)
};

struct Event { double time; int degree; int octave; float vel; };
struct Loop
{
    double length = 20, pos = 0;
    int kind = KPad;
    std::vector<Event> events;
};

class Generator
{
public:
    struct Params
    {
        bool on = true;
        int mood = MWarm, root = 9;
        bool minor = true;
        float weather = 0.35f, evolve = 0.4f, brightness = 0.5f;
        float padLvl = 0.7f, bellLvl = 0.5f, glassLvl = 0.4f, droneLvl = 0.5f;
    };

    std::function<void (int note, float vel, bool on)> midiOut;   // optional MIDI echo of what it plays
    int lastNoteForUi = -1;

    void prepare (double newSr)
    {
        sr = newSr;
        for (auto& v : voices) v.active = false;
        loops.clear();
        const double lens[3] = { 17.3, 23.9, 29.7 };
        const int kinds[3] = { KPad, KBell, KGlass };
        for (int i = 0; i < 3; ++i)
        {
            Loop l; l.length = lens[i]; l.kind = kinds[i]; l.pos = lens[i] * 0.7;
            l.events.reserve (8);
            loops.push_back (l);
        }
        droneRoot = -1;
        for (auto& l : loops) reseed (l, Params());
    }

    void render (float* L, float* R, int n, const Params& p, float level)
    {
        // key change: drone moves, loops re-pitch automatically (they store scale degrees)
        if (p.on && p.root != droneRoot) startDrone (p);
        if (! p.on && droneRoot >= 0) { releaseDrone(); droneRoot = -1; }

        const double dt = 1.0 / sr;
        if (p.on)
            for (auto& l : loops)
            {
                const double start = l.pos, end = l.pos + n * dt;
                for (auto& e : l.events)
                {
                    double t = e.time;
                    if (t < start) t += l.length;
                    if (t >= start && t < end) trigger (l.kind, e, p);
                }
                l.pos = end;
                if (l.pos >= l.length) { l.pos -= l.length; mutate (l, p); }
            }

        const float bright = 0.25f + 0.75f * p.brightness;
        for (auto& v : voices)
        {
            if (! v.active) continue;
            const float lvl = (v.kind == KPad ? p.padLvl : v.kind == KBell ? p.bellLvl : p.glassLvl) * (v.note < 48 ? p.droneLvl / std::max (0.01f, p.padLvl) : 1.0f);
            const double fc = std::clamp ((double) (300.0 + 4500.0 * bright * (0.6 + 0.4 * v.vel)), 80.0, sr * 0.45);
            const float g = (float) std::tan (kPi * fc / sr), k = 1.3f;
            for (int i = 0; i < n; ++i)
            {
                // envelope
                if (! v.releasing) { v.env = std::min (1.0f, v.env + v.attack); v.hold -= (float) dt; if (v.hold <= 0) v.releasing = true; }
                else { v.env *= v.release; if (v.env < 1.0e-4f) { v.active = false; break; } }

                float y = 0;
                if (v.kind == KPad)
                {
                    float s = 0;
                    const double det[3] = { 1.0, 1.0035, 0.9968 };
                    for (int o = 0; o < 3; ++o)
                    {
                        const double d = v.freq * det[o] * dt;
                        s += (float) (2.0 * v.ph[o] - 1.0) - polyBlep (v.ph[o], d);
                        adv (v.ph[o], d);
                    }
                    const float x = s * 0.33f;
                    const float hp = (x - (k + g) * v.ic1 - v.ic2) / (1 + g * (k + g));
                    const float bp = g * hp + v.ic1; v.ic1 = g * hp + bp;
                    const float lp = g * bp + v.ic2; v.ic2 = g * bp + lp;
                    y = lp * 0.5f;
                }
                else if (v.kind == KBell)
                {
                    adv (v.mph, v.freq * 3.5 * dt);
                    v.idx *= 0.99995f;
                    adv (v.ph[0], v.freq * dt);
                    y = (float) std::sin (kTwoPi * v.ph[0] + v.idx * std::sin (kTwoPi * v.mph)) * 0.3f;
                }
                else
                {
                    adv (v.ph[0], v.freq * dt); adv (v.ph[1], v.freq * 2.01 * dt); adv (v.ph[2], v.freq * 3.98 * dt);
                    y = (float) (std::sin (kTwoPi * v.ph[0]) + 0.3 * std::sin (kTwoPi * v.ph[1]) * bright + 0.12 * std::sin (kTwoPi * v.ph[2]) * bright) * 0.25f;
                }
                y *= v.env * v.vel * lvl * level;
                L[i] += y * v.panL; R[i] += y * v.panR;
            }
        }
    }

private:
    int noteFor (const Event& e, const Params& p) const
    {
        const auto& sc = scaleFor (p.mood, p.minor);
        const int d = ((e.degree % sc.n) + sc.n) % sc.n;
        const int octs = e.degree / sc.n;
        return 12 * (e.octave + 1) + p.root + sc.deg[d] + 12 * octs;
    }

    void trigger (int kind, const Event& e, const Params& p)
    {
        const int note = noteFor (e, p);
        const float vel = e.vel * (0.85f + 0.3f * p.weather) * (0.9f + 0.2f * rng.u());
        GenVoice* v = nullptr;
        for (auto& x : voices) if (! x.active) { v = &x; break; }
        if (v == nullptr) { v = &voices[0]; for (auto& x : voices) if (x.releasing && x.env < v->env) v = &x; }
        v->active = true; v->kind = kind; v->note = note; v->vel = vel;
        v->freq = 440.0 * std::pow (2.0, (note - 69) / 12.0);
        v->env = 0; v->releasing = false; v->ic1 = v->ic2 = 0;
        for (auto& ph : v->ph) ph = rng.u();
        const float pan = rng.n() * 0.7f;
        v->panL = std::cos ((pan + 1) * 0.25f * (float) kPi) * 1.2f; v->panR = std::sin ((pan + 1) * 0.25f * (float) kPi) * 1.2f;
        switch (kind)
        {
            case KPad:  v->attack = (float) (1.0 / (2.5 * sr)); v->hold = 6.0f + 6.0f * rng.u(); v->release = (float) std::exp (-1.0 / (3.5 * sr)); break;
            case KBell: v->attack = (float) (1.0 / (0.003 * sr)); v->hold = 0.01f; v->release = (float) std::exp (-1.0 / (2.8 * sr)); v->idx = 2.5f; break;
            default:    v->attack = (float) (1.0 / (0.4 * sr)); v->hold = 1.5f; v->release = (float) std::exp (-1.0 / (2.0 * sr)); break;
        }
        lastNoteForUi = note;
        if (midiOut) midiOut (note, vel, true);
    }

    void startDrone (const Params& p)
    {
        releaseDrone();
        droneRoot = p.root;
        for (int k = 0; k < 2; ++k)
        {
            GenVoice* v = nullptr;
            for (auto& x : voices) if (! x.active) { v = &x; break; }
            if (v == nullptr) return;
            v->active = true; v->kind = KPad; v->note = 36 + p.root + (k == 1 ? 7 : 0); v->vel = 0.8f;
            v->freq = 440.0 * std::pow (2.0, (v->note - 69) / 12.0);
            v->env = 0; v->releasing = false; v->ic1 = v->ic2 = 0;
            v->attack = (float) (1.0 / (6.0 * sr)); v->hold = 1.0e9f; v->release = (float) std::exp (-1.0 / (4.0 * sr));
            v->panL = k == 0 ? 0.9f : 0.6f; v->panR = k == 0 ? 0.6f : 0.9f;
        }
    }
    void releaseDrone() { for (auto& v : voices) if (v.active && v.note < 48 && v.hold > 1.0e8f) v.releasing = true; }

    void reseed (Loop& l, const Params& p)
    {
        l.events.clear();
        const int count = 1 + (int) std::lround (p.weather * 3.0f) + (rng.u() < 0.5f ? 1 : 0);
        for (int i = 0; i < count; ++i) l.events.push_back (randomEvent (l, p));
    }
    Event randomEvent (const Loop& l, const Params&)
    {
        Event e;
        e.time = rng.u() * l.length;
        e.degree = (int) (rng.u() * 9.0f);
        e.octave = l.kind == KPad ? 3 : l.kind == KBell ? 4 : 5;
        e.vel = 0.5f + 0.4f * rng.u();
        return e;
    }
    void mutate (Loop& l, const Params& p)
    {
        // Evolve: now and then a note moves, appears or disappears - slowly, like Eno's loops
        if (! l.events.empty() && rng.u() < p.evolve * 0.6f)
            l.events[(size_t) (rng.u() * (float) l.events.size()) % l.events.size()].degree = (int) (rng.u() * 9.0f);
        const int target = 1 + (int) std::lround (p.weather * 4.0f);
        if ((int) l.events.size() < target && rng.u() < 0.5f + p.evolve * 0.4f) l.events.push_back (randomEvent (l, p));
        else if ((int) l.events.size() > target && rng.u() < 0.6f) l.events.erase (l.events.begin() + (long) ((size_t) (rng.u() * (float) l.events.size()) % l.events.size()));
    }

    double sr = 44100;
    GenVoice voices[24];
    std::vector<Loop> loops;
    int droneRoot = -1;
    Rng rng;
};

// ===========================================================================
// Granular cloud & freeze (fed by the input)
// ===========================================================================
enum CloudPitch { CPUnison = 0, CPOctUp, CPOctDown, CPFifth, CPOctaves };

class Cloud
{
public:
    struct Params { float amount = 0.5f, density = 0.5f, size = 0.5f, spray = 0.5f, swell = 0.6f; int pitch = CPOctUp; bool freeze = false; };

    void prepare (double newSr)
    {
        sr = newSr;
        buf.assign ((size_t) (sr * 4.0), 0.0f); w = 0; filled = 0;
        for (auto& g : grains) g.active = false;
        envAtk = std::exp (-1.0f / (0.8f * (float) sr)); envRel = std::exp (-1.0f / (4.0f * (float) sr));
        follow = 0; nextGrain = 0;
    }

    void process (const float* inMono, float* L, float* R, int n, const Params& p)
    {
        const int size = (int) buf.size();
        const double grainEvery = sr / (2.0 + p.density * 38.0);
        const double grainLen = (0.08 + p.size * 0.45) * sr;
        for (int i = 0; i < n; ++i)
        {
            const float x = inMono[i];
            if (! p.freeze) { buf[(size_t) w] = x; if (++w >= size) w = 0; filled = std::min (filled + 1, size); }
            const float a = std::abs (x);
            follow = a > follow ? a + envAtk * (follow - a) : a + envRel * (follow - a);

            if (--nextGrain <= 0 && filled > (int) (grainLen + sr * 0.1))
            {
                nextGrain = (int) (grainEvery * (0.6 + 0.8 * rng.u()));
                startGrain (p, grainLen, size);
            }

            float l = 0, r = 0;
            for (auto& g : grains)
            {
                if (! g.active) continue;
                const double ph = g.age / g.len;
                const float env = (float) (0.5 - 0.5 * std::cos (kTwoPi * ph));
                double rp = g.pos; while (rp < 0) rp += size; while (rp >= size) rp -= size;
                const int i0 = (int) rp, i1 = (i0 + 1) % size;
                const float fr = (float) (rp - i0);
                const float s = (buf[(size_t) i0] + fr * (buf[(size_t) i1] - buf[(size_t) i0])) * env;
                l += s * g.panL; r += s * g.panR;
                g.pos += g.rate; g.age += 1.0;
                if (g.age >= g.len) g.active = false;
            }
            const float swellGain = p.freeze ? 1.0f : (1.0f - p.swell) + p.swell * std::min (1.0f, follow * 12.0f);
            const float out = p.amount * swellGain * 0.55f;
            L[i] += l * out; R[i] += r * out;
        }
    }

    float level() const { return follow; }

private:
    void startGrain (const Params& p, double len, int size)
    {
        for (auto& g : grains)
        {
            if (g.active) continue;
            g.active = true; g.len = len * (0.7 + 0.6 * rng.u()); g.age = 0;
            static const double ups[] = { 1.0, 2.0, 0.5, 1.4983 };
            double rate = 1.0;
            switch (p.pitch)
            {
                case CPOctUp:   rate = rng.u() < 0.6f ? 2.0 : 1.0; break;
                case CPOctDown: rate = rng.u() < 0.6f ? 0.5 : 1.0; break;
                case CPFifth:   rate = rng.u() < 0.5f ? 1.4983 : 1.0; break;
                case CPOctaves: rate = ups[(int) (rng.u() * 3.0f)]; break;
                default: break;
            }
            g.rate = rate * (1.0 + rng.n() * 0.003);
            const double back = (0.05 + p.spray * 3.0 * rng.u()) * sr + g.len * std::max (0.0, rate - 1.0);
            g.pos = (double) w - std::min (back, (double) filled - 2.0);
            if (g.pos < 0) g.pos += size;
            const float pan = rng.n();
            g.panL = std::cos ((pan + 1) * 0.25f * (float) kPi); g.panR = std::sin ((pan + 1) * 0.25f * (float) kPi);
            return;
        }
    }

    struct Grain { bool active = false; double pos = 0, rate = 1, age = 0, len = 1; float panL = 0.7f, panR = 0.7f; };
    double sr = 44100;
    std::vector<float> buf;
    int w = 0, filled = 0;
    Grain grains[32];
    int nextGrain = 0;
    float follow = 0, envAtk = 0, envRel = 0;
    Rng rng;
};

// ===========================================================================
// Nature: wind, rain, creek, crickets, birds, loons, thunder - all synthesized
// ===========================================================================
class Nature
{
public:
    struct Params { float wind = 0.3f, rain = 0, creek = 0, crickets = 0.2f, birds = 0.1f, loons = 0.1f, thunder = 0; };

    void prepare (double newSr)
    {
        sr = newSr;
        for (int c = 0; c < 2; ++c) { windBp[c].set (2, sr, 500, 0.8); rainHp[c].set (1, sr, 2000, 0.7); rainLp[c].set (0, sr, 9000, 0.7); }
        creekBp.set (2, sr, 800, 4); thunderLp.set (0, sr, 120, 0.7);
        for (auto& b : blips) b.active = false;
        for (auto& c : crickets) { c.freq = 4300 + 600 * rng.u(); c.phase = rng.u(); c.rate = 14 + 6 * rng.u(); c.chirpTimer = rng.u() * 2; c.pan = rng.n() * 0.9f; }
        bird.active = false; loon.active = false; thunder.active = false;
        gust = 0; gustTarget = 0.5f; gustTimer = 0;
    }

    void process (float* L, float* R, int n, const Params& p, float level)
    {
        const double dt = 1.0 / sr;
        for (int i = 0; i < n; ++i)
        {
            float l = 0, r = 0;
            // WIND: band-passed noise with slow gusts sweeping its colour
            if (p.wind > 0.001f)
            {
                gustTimer -= (float) dt;
                if (gustTimer <= 0) { gustTimer = 1.5f + 3.0f * rng.u(); gustTarget = 0.2f + 0.8f * rng.u(); }
                gust += (gustTarget - gust) * 0.00003f;
                if ((counter & 63) == 0) for (int c = 0; c < 2; ++c) windBp[c].set (2, sr, 250 + 900 * gust + c * 60, 0.7);
                l += windBp[0].process (rng.n()) * gust * p.wind * 0.6f;
                r += windBp[1].process (rng.n()) * gust * p.wind * 0.6f;
            }
            // RAIN: a soft bed plus thousands of tiny drops
            if (p.rain > 0.001f)
            {
                float dl = 0, dr = 0;
                if (rng.u() < p.rain * 0.02f) { const float a = rng.u() * rng.u(); if (rng.u() < 0.5f) dl = a; else dr = a; }
                l += (rainLp[0].process (rainHp[0].process (rng.n() * 0.08f + dl))) * p.rain * 0.8f;
                r += (rainLp[1].process (rainHp[1].process (rng.n() * 0.08f + dr))) * p.rain * 0.8f;
            }
            // CREEK: bubbling - short rising sine blips over a gurgle
            if (p.creek > 0.001f)
            {
                if (rng.u() < p.creek * 0.004f)
                    for (auto& b : blips) if (! b.active) { b.active = true; b.ph = 0; b.f = 300 + 1200 * rng.u(); b.age = 0; b.len = 0.015 + 0.04 * rng.u(); b.pan = rng.n() * 0.8f; break; }
                const float g = creekBp.process (rng.n()) * 0.15f;
                l += g * p.creek; r += g * p.creek;
                for (auto& b : blips)
                {
                    if (! b.active) continue;
                    const double ph01 = b.age / b.len;
                    adv (b.ph, b.f * (1.0 + 1.5 * ph01) * dt);
                    const float y = (float) (std::sin (kTwoPi * b.ph) * std::sin (kPi * ph01)) * 0.25f * p.creek;
                    l += y * (1 - b.pan) * 0.5f; r += y * (1 + b.pan) * 0.5f;
                    b.age += dt; if (b.age >= b.len) b.active = false;
                }
            }
            // CRICKETS: pulsed high sines, each with its own rhythm
            if (p.crickets > 0.001f)
                for (auto& c : crickets)
                {
                    c.chirpTimer -= (float) dt;
                    if (c.chirpTimer <= 0) { c.chirpTimer = 0.9f + 1.4f * rng.u(); c.chirpLeft = 0.18f + 0.1f * rng.u(); }
                    float y = 0;
                    if (c.chirpLeft > 0)
                    {
                        c.chirpLeft -= (float) dt;
                        adv (c.phase, c.freq * dt); adv (c.pulse, c.rate * 2.0 * dt);
                        y = (float) std::sin (kTwoPi * c.phase) * (c.pulse < 0.5 ? (float) std::sin (kTwoPi * c.pulse) : 0.0f) * 0.05f * p.crickets;
                    }
                    l += y * (1 - c.pan) * 0.5f; r += y * (1 + c.pan) * 0.5f;
                }
            // BIRDS: occasional little chirp phrases
            if (p.birds > 0.001f)
            {
                if (! bird.active && rng.u() < p.birds * 0.00004f) { bird.active = true; bird.notes = 2 + (int) (rng.u() * 5); bird.t = 0; bird.base = 2500 + 2000 * rng.u(); bird.pan = rng.n() * 0.8f; }
                if (bird.active)
                {
                    const double noteLen = 0.09;
                    const int k = (int) (bird.t / noteLen);
                    const double ph01 = std::fmod (bird.t, noteLen) / noteLen;
                    if (k >= bird.notes) bird.active = false;
                    else
                    {
                        adv (bird.ph, bird.base * (1.0 + 0.4 * std::sin (kPi * ph01) + 0.15 * (k % 2)) * dt);
                        const float y = (float) (std::sin (kTwoPi * bird.ph) * std::sin (kPi * ph01)) * 0.08f * p.birds;
                        l += y * (1 - bird.pan) * 0.5f; r += y * (1 + bird.pan) * 0.5f;
                        bird.t += dt;
                    }
                }
            }
            // LOONS: the long, gliding wail across the lake
            if (p.loons > 0.001f)
            {
                if (! loon.active && rng.u() < p.loons * 0.000006f) { loon.active = true; loon.t = 0; loon.len = 3.5 + 2.0 * rng.u(); loon.pan = rng.n() * 0.6f; }
                if (loon.active)
                {
                    const double x = loon.t / loon.len;
                    // three-note wail: low, up a fifth, up an octave, with vibrato
                    const double base = 620.0;
                    const double f = base * (x < 0.3 ? 1.0 + 0.5 * (x / 0.3) * (x / 0.3) : x < 0.6 ? 1.5 : 1.5 + 0.45 * ((x - 0.6) / 0.4))
                                     * (1.0 + 0.012 * std::sin (kTwoPi * 5.5 * loon.t));
                    adv (loon.ph, f * dt); adv (loon.ph2, f * 2.0 * dt);
                    const double env = std::sin (kPi * std::min (1.0, x)) * std::min (1.0, loon.t / 0.25);
                    const float y = (float) ((std::sin (kTwoPi * loon.ph) + 0.25 * std::sin (kTwoPi * loon.ph2)) * env) * 0.07f * p.loons;
                    l += y * (1 - loon.pan) * 0.5f; r += y * (1 + loon.pan) * 0.5f;
                    loon.t += dt; if (loon.t >= loon.len) loon.active = false;
                }
            }
            // THUNDER: rare distant rumbles
            if (p.thunder > 0.001f)
            {
                if (! thunder.active && rng.u() < p.thunder * 0.000003f) { thunder.active = true; thunder.t = 0; thunder.len = 4 + 4 * rng.u(); }
                if (thunder.active)
                {
                    const double x = thunder.t / thunder.len;
                    const double env = std::min (1.0, thunder.t / 0.4) * std::exp (-3.0 * x) * (1.0 + 0.5 * std::sin (kTwoPi * 0.7 * thunder.t));
                    const float y = thunderLp.process (rng.n()) * (float) env * 1.6f * p.thunder;
                    l += y; r += y;
                    thunder.t += dt; if (thunder.t >= thunder.len) thunder.active = false;
                }
            }
            L[i] += l * level; R[i] += r * level;
            ++counter;
        }
    }

private:
    struct Blip { bool active = false; double ph = 0, f = 500, age = 0, len = 0.03; float pan = 0; };
    struct Cricket { double freq = 4500, phase = 0, pulse = 0, rate = 15; float chirpTimer = 0, chirpLeft = 0, pan = 0; };
    struct Bird { bool active = false; int notes = 3; double t = 0, ph = 0, base = 3000; float pan = 0; };
    struct Call { bool active = false; double t = 0, len = 4, ph = 0, ph2 = 0; float pan = 0; };
    double sr = 44100;
    Biquad windBp[2], rainHp[2], rainLp[2], creekBp, thunderLp;
    Blip blips[24];
    Cricket crickets[4];
    Bird bird; Call loon, thunder;
    float gust = 0, gustTarget = 0.5f, gustTimer = 0;
    uint32_t counter = 0;
    Rng rng;
};

// ===========================================================================
// Space: big reverb with shimmer (octave-up in the feedback) + tape wobble
// ===========================================================================
class PitchShifter   // rotating two-head delay, +12 semitones
{
public:
    void prepare (double newSr) { sr = newSr; buf.assign ((size_t) (sr * 0.1), 0.0f); w = 0; phase = 0; }
    inline float process (float x, double ratio) noexcept
    {
        const int size = (int) buf.size();
        buf[(size_t) w] = x;
        const double win = size - 4;
        phase += (ratio - 1.0) / win; if (phase >= 1) phase -= 1; if (phase < 0) phase += 1;
        float out = 0;
        for (int h = 0; h < 2; ++h)
        {
            double p = phase + 0.5 * h; if (p >= 1) p -= 1;
            const double d = (1.0 - p) * win + 2.0;
            double rp = w - d; while (rp < 0) rp += size;
            const int i0 = (int) rp, i1 = (i0 + 1) % size;
            const float fr = (float) (rp - i0);
            const float s = buf[(size_t) i0] + fr * (buf[(size_t) i1] - buf[(size_t) i0]);
            out += s * (float) std::sin (kPi * p);
        }
        if (++w >= size) w = 0;
        return out * 0.7071f;
    }
private:
    double sr = 44100, phase = 0;
    std::vector<float> buf;
    int w = 0;
};

class Space
{
public:
    void prepare (double newSr)
    {
        sr = newSr;
        verb.reset(); verb.setSampleRate (sr);
        for (auto& s : shift) s.prepare (sr);
        for (auto& d : wob) d.assign ((size_t) (sr * 0.05), 0.0f);
        ww = 0; wph = 0; fbL = fbR = 0;
        shimLp[0].set (0, sr, 7000, 0.7); shimLp[1] = shimLp[0];
    }
    // in = generated sound (heard dry + in the space); send = extra signal that only goes into the space (the guitar)
    void process (float* inL, float* inR, const float* sendL, const float* sendR, float* outL, float* outR,
                  int n, float size, float mix, float shimmer, float wobble)
    {
        // tape wobble on what goes into the space
        if (wobble > 0.001f)
        {
            const int sz = (int) wob[0].size();
            for (int i = 0; i < n; ++i)
            {
                wph += 0.6 / sr; if (wph > 1) wph -= 1;
                const double d = (0.012 + 0.006 * wobble * std::sin (kTwoPi * wph) + 0.0015 * wobble * std::sin (kTwoPi * wph * 9.3)) * sr;
                for (int c = 0; c < 2; ++c)
                {
                    float* x = c == 0 ? inL : inR;
                    wob[c][(size_t) ww] = x[i];
                    double rp = ww - d; while (rp < 0) rp += sz;
                    const int i0 = (int) rp, i1 = (i0 + 1) % sz;
                    const float fr = (float) (rp - i0);
                    const float y = wob[c][(size_t) i0] + fr * (wob[c][(size_t) i1] - wob[c][(size_t) i0]);
                    x[i] = x[i] * (1 - wobble) + y * wobble;
                }
                if (++ww >= sz) ww = 0;
            }
        }
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.7f + 0.27f * size; rp.damping = 0.35f; rp.wetLevel = 0.35f; rp.dryLevel = 0.0f; rp.width = 1.0f;
        verb.setParameters (rp);
        for (int i = 0; i < n; ++i)
        {
            // shimmer: an octave-up copy of the tail feeds back into the reverb
            float l = inL[i] + sendL[i] + fbL * shimmer * 0.55f, r = inR[i] + sendR[i] + fbR * shimmer * 0.55f;
            verb.processStereo (&l, &r, 1);
            fbL = std::tanh (shimLp[0].process (shift[0].process (l, 2.0)) * 0.6f);   // soft-clipped, so it can bloom but never run away
            fbR = std::tanh (shimLp[1].process (shift[1].process (r, 2.0)) * 0.6f);
            outL[i] += l * mix + inL[i] * (1.0f - mix * 0.5f);
            outR[i] += r * mix + inR[i] * (1.0f - mix * 0.5f);
        }
    }
private:
    double sr = 44100;
    juce::Reverb verb;
    PitchShifter shift[2];
    std::vector<float> wob[2];
    int ww = 0; double wph = 0;
    float fbL = 0, fbR = 0;
    Biquad shimLp[2];
};
} // namespace ln
