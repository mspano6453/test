#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// Schaltwerk DSP: electronic drum box, robot synth (+ vowel / vocoder), Strumchord, echo.
// Everything here is synthesised - the originals were electronic machines too.
// ---------------------------------------------------------------------------
namespace sw
{
constexpr double kTwoPi = 6.283185307179586476925;
constexpr double kPi = 3.14159265358979323846;

struct Noise
{
    uint32_t s = 0x1234567u;
    inline float next() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (int32_t) s * (1.0f / 2147483648.0f); }
};

struct Env
{
    float v = 0, k = 0;
    void start (float amp, double tau, double sr) { v = amp; k = (float) std::exp (-1.0 / (std::max (tau, 1.0e-4) * sr)); }
    inline float next() noexcept { const float o = v; v *= k; return o; }
};

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void set (int type, double sr, double f, double q)   // 0 LP, 1 HP, 2 BP
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

inline float sine (double ph) { return (float) std::sin (kTwoPi * ph); }
inline float sq (double ph) { return ph - std::floor (ph) < 0.5 ? 1.0f : -1.0f; }
inline void adv (double& ph, double inc) { ph += inc; if (ph >= 1.0) ph -= std::floor (ph); }

// ===========================================================================
// DRUM BOX
// ===========================================================================
enum Drum { DKick = 0, DClank, DSnare, DClap, DZap, DTomLo, DHatC, DTomHi, DClave, DCowbell, DHatO, DSteam, NumDrums };

inline const char* drumName (int d)
{
    static const char* n[] = { "Kick", "Clank", "Snare", "Clap", "Zap", "Tom Lo", "Hat", "Tom Hi", "Clave", "Cowbell", "Open Hat", "Steam" };
    return n[d];
}
constexpr int kDrumBaseNote = 36;   // C1 .. B1

struct DrumSettings { float tune = 1, decay = 1, tone = 0.5f, gain = 1, panL = 1, panR = 1; };

struct DrumVoice
{
    bool active = false;
    double t = 0;
    float vel = 1;
    double ph[3] {};
    Env e[4];
    Biquad f1, f2;
    Noise noise;
    float choke = 1, chokeK = 1;
    bool choking = false;
    DrumSettings s;
};

class DrumBox
{
public:
    void prepare (double newSr) { sr = newSr; for (auto& v : voices) v.active = false; }

    void trigger (int d, float vel, const DrumSettings& s)
    {
        if (d == DHatC)
        {
            auto& o = voices[DHatO];
            if (o.active) { o.choking = true; o.chokeK = (float) std::exp (-1.0 / (0.01 * sr)); }
        }
        auto& v = voices[d];
        v.active = true; v.t = 0; v.vel = vel; v.s = s;
        v.choke = 1; v.choking = false;
        for (auto& p : v.ph) p = 0;
        for (auto& e : v.e) e.v = 0;
        v.f1.reset(); v.f2.reset();
        const double dec = s.decay, tone = s.tone, tn = s.tune;
        switch (d)
        {
            case DKick:   v.e[0].start (1.0f, 0.22 * dec, sr); v.e[1].start (0.35f * (float) tone, 0.002, sr); v.e[2].start (3.0f, 0.007, sr); v.f1.set (0, sr, 3000, 0.7); break;
            case DClank:  v.e[0].start (1.0f, 0.09 * dec, sr); v.e[1].start (0.4f, 0.004, sr); v.f1.set (2, sr, 2200 * (0.5 + tone) * tn, 3.0); break;
            case DSnare:  v.e[0].start (0.5f, 0.05 * dec, sr); v.e[1].start (0.8f, 0.13 * dec, sr); v.f1.set (2, sr, 1800 + 3000 * tone, 0.8); break;
            case DClap:   v.e[1].start (0.9f, 0.12 * dec, sr); v.f1.set (2, sr, 1200 + 800 * tone, 2.0); break;
            case DZap:    v.e[0].start (0.9f, 0.15 * dec, sr); break;
            case DTomLo:
            case DTomHi:  v.e[0].start (1.0f, 0.25 * dec, sr); v.e[1].start (0.2f, 0.003, sr); v.f1.set (0, sr, 2000 + 4000 * tone, 0.7); break;
            case DHatC:   v.e[0].start (0.8f, 0.035 * dec, sr); v.f1.set (1, sr, 6000 + 3000 * tone, 0.7); break;
            case DHatO:   v.e[0].start (0.7f, 0.30 * dec, sr); v.f1.set (1, sr, 6000 + 3000 * tone, 0.7); break;
            case DClave:  v.e[0].start (0.9f, 0.025 * dec, sr); break;
            case DCowbell:v.e[0].start (0.8f, 0.18 * dec, sr); v.e[1].start (0.5f, 0.02, sr); v.f1.set (2, sr, 900 * tn, 1.5); break;
            case DSteam:  v.e[0].start (1.0f, 0.4 * dec, sr); break;
            default: break;
        }
    }

    void render (float* L, float* R, float* sendL, float* sendR, int n, float send)
    {
        for (int d = 0; d < NumDrums; ++d)
        {
            auto& v = voices[d];
            if (! v.active) continue;
            const double isr = 1.0 / sr, tn = v.s.tune;
            for (int i = 0; i < n; ++i)
            {
                float y = 0;
                const double t = v.t;
                switch (d)
                {
                    case DKick:
                    {
                        const double f = 50.0 * tn * (1.0 + v.e[2].next());
                        adv (v.ph[0], f * isr);
                        y = sine (v.ph[0]) * v.e[0].next() + v.f1.process (v.noise.next()) * v.e[1].next();
                        y = std::tanh (1.5f * y);
                        break;
                    }
                    case DClank:
                        adv (v.ph[0], 587.0 * tn * isr); adv (v.ph[1], 845.0 * tn * isr);
                        y = v.f1.process (sq (v.ph[0]) * sq (v.ph[1])) * v.e[0].next() * 2.0f + v.noise.next() * v.e[1].next() * 0.3f;
                        break;
                    case DSnare:
                        adv (v.ph[0], 190.0 * tn * isr);
                        y = sine (v.ph[0]) * v.e[0].next() + v.f1.process (v.noise.next()) * v.e[1].next() * 1.6f;
                        break;
                    case DClap:
                    {
                        float a;
                        if (t < 0.03) a = (float) std::exp (-std::fmod (t, 0.01) / 0.003);
                        else a = v.e[1].next();
                        y = v.f1.process (v.noise.next()) * a * 2.2f;
                        break;
                    }
                    case DZap:
                    {
                        const double f = 120.0 * tn + 2500.0 * tn * std::exp (-t / (0.04 * v.s.decay));
                        adv (v.ph[0], f * isr);
                        y = sine (v.ph[0]) * v.e[0].next();
                        break;
                    }
                    case DTomLo:
                    case DTomHi:
                    {
                        const double f = (d == DTomLo ? 110.0 : 180.0) * tn * (1.0 + 0.6 * std::exp (-t / 0.03));
                        adv (v.ph[0], f * isr);
                        y = sine (v.ph[0]) * v.e[0].next() + v.f1.process (v.noise.next()) * v.e[1].next();
                        break;
                    }
                    case DHatC:
                    case DHatO:
                        adv (v.ph[0], 3150.0 * tn * isr); adv (v.ph[1], 4650.0 * tn * isr);
                        y = v.f1.process (v.noise.next() * 0.8f + sq (v.ph[0]) * sq (v.ph[1]) * 0.3f) * v.e[0].next() * v.choke;
                        if (v.choking) v.choke *= v.chokeK;
                        break;
                    case DClave:
                        adv (v.ph[0], 2400.0 * tn * isr);
                        y = sine (v.ph[0]) * v.e[0].next();
                        break;
                    case DCowbell:
                        adv (v.ph[0], 540.0 * tn * isr); adv (v.ph[1], 800.0 * tn * isr);
                        y = v.f1.process ((sq (v.ph[0]) + sq (v.ph[1])) * 0.5f) * (v.e[0].next() + v.e[1].next()) * 1.6f;
                        break;
                    case DSteam:
                    {
                        if ((((int) (t * sr)) & 31) == 0)
                            v.f1.set (0, sr, (1000.0 + 8000.0 * v.s.tone) * std::exp (-t / (0.25 * v.s.decay)) + 300.0, 0.9);
                        const float att = (float) std::min (1.0, t / 0.02);
                        y = v.f1.process (v.noise.next()) * v.e[0].next() * att;
                        break;
                    }
                    default: break;
                }
                y *= v.vel * v.s.gain * 0.35f;   // drum bus headroom
                L[i] += y * v.s.panL; R[i] += y * v.s.panR;
                sendL[i] += y * v.s.panL * send; sendR[i] += y * v.s.panR * send;
                v.t += isr;
            }
            float m = 0;
            for (auto& e : v.e) m = std::max (m, e.v);
            if (d == DClap && v.t < 0.04) m = 1;
            if (d == DZap && v.e[0].v > 1.0e-4f) m = 1;
            if ((m < 1.0e-4f && v.t > 0.01) || v.choke < 1.0e-4f || v.t > 4.0) v.active = false;
        }
    }

private:
    double sr = 44100;
    DrumVoice voices[NumDrums];
};

// ===========================================================================
// ROBOT SYNTH (poly / mono-glide) with VOWEL and VOCODER modes
// ===========================================================================
struct SynthParams
{
    float wave = 0.3f, pw = 0.5f, detune = 8, cutoff = 2400, reso = 0.3f, envAmt = 0.4f;
    float a = 0.01f, d = 0.3f, s = 0.7f, r = 0.3f, glide = 0.0f;
    int mode = 0;            // 0 synth, 1 vowel choir, 2 vocoder
    float vowel = 0, formantShift = 1, sibilance = 0.3f;
};

struct Adsr
{
    enum Stage { Idle, Attack, Decay, Sustain, Release } stage = Idle;
    float v = 0;
    void on() { stage = Attack; }
    void off() { if (stage != Idle) stage = Release; }
    inline float next (float a, float d, float s, float r, double sr) noexcept
    {
        switch (stage)
        {
            case Attack:  v += (float) (1.0 / (std::max (0.001f, a) * sr)); if (v >= 1) { v = 1; stage = Decay; } break;
            case Decay:   v = s + (v - s) * (float) std::exp (-1.0 / (std::max (0.005f, d) * 0.3 * sr)); if (v - s < 1e-4f) stage = Sustain; break;
            case Sustain: v = s; break;
            case Release: v *= (float) std::exp (-1.0 / (std::max (0.005f, r) * 0.3 * sr)); if (v < 1e-4f) { v = 0; stage = Idle; } break;
            default: break;
        }
        return v;
    }
};

inline float polyBlep (double t, double dt)
{
    if (t < dt) { t /= dt; return (float) (t + t - t * t - 1.0); }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
    return 0.0f;
}

struct SynthVoice
{
    bool active = false;
    int note = -1;
    float vel = 1;
    double freq = 440, target = 440;
    double ph1 = 0, ph2 = 0;
    Adsr amp, flt;
    float ic1 = 0, ic2 = 0;   // SVF state
    uint32_t age = 0;
};

class RobotSynth
{
public:
    static constexpr int kVoices = 8, kBands = 16;

    void prepare (double newSr)
    {
        sr = newSr;
        for (auto& v : voices) v = SynthVoice();
        for (int b = 0; b < kBands; ++b)
        {
            const double f = 110.0 * std::pow (7600.0 / 110.0, (double) b / (kBands - 1));
            ana[b].set (2, sr, f, 6.0); syn[b].set (2, sr, f, 6.0);
            ana[b].reset(); syn[b].reset(); env[b] = 0;
        }
        sibHp.set (1, sr, 5000, 0.7);
        vAtk = std::exp (-1.0f / (0.003f * (float) sr));
        vRel = std::exp (-1.0f / (0.03f * (float) sr));
        for (auto& f : form) f.reset();
    }

    void noteOn (int note, float vel, const SynthParams& p)
    {
        const double f = 440.0 * std::pow (2.0, (note - 69) / 12.0);
        if (p.glide > 0.001f)
        {
            // mono with glide: one voice follows the last key
            auto& v = voices[0];
            held.push_back (note);
            const bool legato = v.active && v.amp.stage != Adsr::Release && v.amp.stage != Adsr::Idle;
            v.note = note; v.vel = vel; v.target = f; v.active = true;
            if (! legato) { v.freq = f; v.amp.on(); v.flt.on(); }
            return;
        }
        SynthVoice* pick = nullptr;
        for (auto& v : voices) if (! v.active) { pick = &v; break; }
        if (pick == nullptr) { pick = &voices[0]; for (auto& v : voices) if (v.age < pick->age) pick = &v; }
        auto& v = *pick;
        v.active = true; v.note = note; v.vel = vel; v.freq = v.target = f; v.age = ++counter;
        v.amp.on(); v.flt.on();
    }

    void noteOff (int note, const SynthParams& p)
    {
        if (p.glide > 0.001f)
        {
            held.erase (std::remove (held.begin(), held.end(), note), held.end());
            auto& v = voices[0];
            if (held.empty()) { v.amp.off(); v.flt.off(); }
            else if (v.note == note) { v.note = held.back(); v.target = 440.0 * std::pow (2.0, (v.note - 69) / 12.0); }
            return;
        }
        for (auto& v : voices) if (v.active && v.note == note) { v.amp.off(); v.flt.off(); }
    }

    void allOff() { for (auto& v : voices) { v.amp.off(); v.flt.off(); } held.clear(); }

    // sideL/R = vocoder modulator (sidechain), may be null
    void render (float* L, float* R, float* sendL, float* sendR, const float* sideL, const float* sideR,
                 int n, const SynthParams& p, float level, float send, float* scratch)
    {
        // 1) raw synth into scratch (mono)
        std::fill (scratch, scratch + n, 0.0f);
        const float k = 2.0f - 1.9f * std::clamp (p.reso, 0.0f, 1.0f);    // SVF damping
        const double glideCoef = p.glide > 0.001f ? 1.0 - std::exp (-1.0 / (p.glide * sr)) : 1.0;
        const double det = std::pow (2.0, p.detune / 1200.0);
        for (auto& v : voices)
        {
            if (! v.active) continue;
            for (int i = 0; i < n; ++i)
            {
                v.freq += (v.target - v.freq) * glideCoef;
                const double dt1 = v.freq / sr, dt2 = v.freq * det / sr;
                auto saw = [] (double ph, double dt) { return (float) (2.0 * ph - 1.0) - polyBlep (ph, dt); };
                auto pulse = [&p] (double ph, double dt)
                {
                    double ph2 = ph + p.pw; if (ph2 >= 1) ph2 -= 1;
                    return (ph < p.pw ? 1.0f : -1.0f) + polyBlep (ph, dt) - polyBlep (ph2, dt);
                };
                const float o1 = (1 - p.wave) * saw (v.ph1, dt1) + p.wave * pulse (v.ph1, dt1);
                const float o2 = (1 - p.wave) * saw (v.ph2, dt2) + p.wave * pulse (v.ph2, dt2);
                adv (v.ph1, dt1); adv (v.ph2, dt2);
                const float fe = v.flt.next (p.a, p.d, p.s * 0.5f, p.r, sr);
                const float a = v.amp.next (p.a, p.d, p.s, p.r, sr);
                const double fc = std::clamp ((double) p.cutoff * std::pow (2.0, p.envAmt * fe * 5.0), 30.0, sr * 0.45);
                const float g = (float) std::tan (kPi * fc / sr);
                // TPT state-variable low-pass
                const float x = 0.5f * (o1 + o2);
                const float hp = (x - (k + g) * v.ic1 - v.ic2) / (1 + g * (k + g));
                const float bp = g * hp + v.ic1; v.ic1 = g * hp + bp;
                const float lp = g * bp + v.ic2; v.ic2 = g * bp + lp;
                scratch[i] += lp * a * v.vel * 0.9f;
            }
            if (v.amp.stage == Adsr::Idle) v.active = false;
        }

        // 2) voice it
        if (p.mode == 1)
        {
            static const float F[5][3] = { { 800, 1150, 2800 }, { 400, 2000, 2550 }, { 300, 2300, 3000 }, { 450, 800, 2830 }, { 325, 700, 2530 } };
            const float pos = std::clamp (p.vowel, 0.0f, 1.0f) * 4.0f;
            const int a = std::min (3, (int) pos); const float fr = pos - (float) a;
            static const float gains[3] = { 1.0f, 0.6f, 0.35f };
            for (int j = 0; j < 3; ++j)
                form[j].set (2, sr, (F[a][j] + (F[a + 1][j] - F[a][j]) * fr) * p.formantShift, 8.0 + 2.0 * j);
            for (int i = 0; i < n; ++i)
            {
                const float x = scratch[i];
                scratch[i] = (form[0].process (x) * gains[0] + form[1].process (x) * gains[1] + form[2].process (x) * gains[2]) * 3.0f;
            }
        }
        else if (p.mode == 2)
        {
            for (int i = 0; i < n; ++i)
            {
                const float m = sideL != nullptr ? 0.5f * (sideL[i] + (sideR != nullptr ? sideR[i] : sideL[i])) : 0.0f;
                const float c = scratch[i];
                float out = 0;
                for (int b = 0; b < kBands; ++b)
                {
                    const float a = std::abs (ana[b].process (m));
                    env[b] = a > env[b] ? a + vAtk * (env[b] - a) : a + vRel * (env[b] - a);
                    out += syn[b].process (c) * env[b];
                }
                scratch[i] = out * 30.0f + sibHp.process (m) * p.sibilance * 0.8f;
            }
        }

        for (int i = 0; i < n; ++i)
        {
            const float y = scratch[i] * level;
            L[i] += y; R[i] += y;
            sendL[i] += y * send; sendR[i] += y * send;
        }
    }

private:
    double sr = 44100;
    SynthVoice voices[kVoices];
    std::vector<int> held;
    uint32_t counter = 0;
    Biquad ana[kBands], syn[kBands], sibHp, form[3];
    float env[kBands] {};
    float vAtk = 0, vRel = 0;
};

// ===========================================================================
// STRUMCHORD: chord section + strumplate harp
// ===========================================================================
enum ChordType { ChMajor = 0, ChMinor, ChSeventh, NumChordTypes };
constexpr int kStrings = 13;

struct OmniParams
{
    float chordLevel = 0.5f, chordTone = 0.4f, bassLevel = 0.5f, harpLevel = 0.7f, harpDecay = 0.9f, vibrato = 0.3f;
    int harpOctave = 0;     // -1..+1
    bool hold = false;
};

class Strumchord
{
public:
    void prepare (double newSr)
    {
        sr = newSr;
        for (auto& h : harp) h.active = false;
        chordLp[0].set (0, sr, 1500, 0.7); chordLp[1] = chordLp[0];
        harpLp.set (0, sr, 4500, 0.7);
        chordEnv = 0;
    }

    void setChord (int root, int type)
    {
        chordRoot = ((root % 12) + 12) % 12; chordType = type; chordOn = true;
    }
    void releaseChord (bool hold) { if (! hold) chordOn = false; }
    bool isChordOn() const { return chordOn; }
    int root() const { return chordRoot; }
    int type() const { return chordType; }

    // strum to a string index (0..12); plays every string crossed since the last position
    void strumTo (int idx, float vel, const OmniParams& p)
    {
        idx = std::clamp (idx, 0, kStrings - 1);
        if (lastString < 0) { pluck (idx, vel, p); lastString = idx; return; }
        if (idx == lastString) return;
        const int step = idx > lastString ? 1 : -1;
        for (int s = lastString + step; s != idx + step; s += step) pluck (s, vel, p);
        lastString = idx;
    }
    void strumRelease() { lastString = -1; }

    void pluck (int s, float vel, const OmniParams& p)
    {
        const int n = intervalCount();
        const int note = 60 + 12 * p.harpOctave + chordRoot + interval (s % n) + 12 * (s / n) - 12;
        auto* v = &harp[0];
        for (auto& h : harp) if (! h.active) { v = &h; break; }
        for (auto& h : harp) if (h.active && h.string == s) { v = &h; break; }   // a string retriggers itself
        v->active = true; v->string = s; v->ph = 0; v->vel = vel;
        v->freq = 440.0 * std::pow (2.0, (note - 69) / 12.0);
        v->env.start (1.0f, 0.15 + 2.2 * p.harpDecay, sr);
        v->att = 0;
        lastPlucked = s;
    }

    void render (float* L, float* R, float* sendL, float* sendR, int n, const OmniParams& p, float send)
    {
        // chord section: soft square organ + auto bass, gated by the chord buttons
        const int cnt = intervalCount();
        double cf[5]; int nc = 0;
        for (int k = 0; k < cnt; ++k) cf[nc++] = 440.0 * std::pow (2.0, (60 + ((chordRoot + interval (k)) % 12) - 69) / 12.0);
        const double bassF = 440.0 * std::pow (2.0, (36 + chordRoot - 69) / 12.0);
        const float target = chordOn ? 1.0f : 0.0f;
        const float envCoef = chordOn ? 1.0f - std::exp (-1.0f / (0.01f * (float) sr)) : 1.0f - std::exp (-1.0f / (0.15f * (float) sr));
        chordLp[0].set (0, sr, 500 + 4000 * p.chordTone, 0.7);
        const double vibInc = 5.5 / sr;

        for (int i = 0; i < n; ++i)
        {
            chordEnv += (target - chordEnv) * envCoef;
            adv (vibPh, vibInc);
            const double vib = 1.0 + 0.004 * p.vibrato * std::sin (kTwoPi * vibPh);
            float chord = 0;
            if (chordEnv > 1.0e-4f)
            {
                for (int k = 0; k < nc; ++k) { adv (cph[k], cf[k] * vib / sr); chord += sq (cph[k]) * 0.18f; }
                adv (bph, bassF / sr);
                chord = chordLp[0].process (chord) * p.chordLevel * chordEnv;
                chord += (sine (bph) * 0.7f + sq (bph) * 0.1f) * p.bassLevel * chordEnv * 0.5f;
            }
            float harpOut = 0;
            for (auto& h : harp)
            {
                if (! h.active) continue;
                adv (h.ph, h.freq * vib / sr);
                h.att = std::min (1.0f, h.att + (float) (1.0 / (0.002 * sr)));
                const double ph = h.ph;
                const float tri = (float) (ph < 0.5 ? 4 * ph - 1 : 3 - 4 * ph);
                harpOut += (tri * 0.7f + sq (ph) * 0.25f) * h.env.next() * h.att * h.vel * 0.3f;
                if (h.env.v < 1.0e-4f) h.active = false;
            }
            harpOut = harpLp.process (harpOut) * p.harpLevel;
            const float y = (chord + harpOut) * 0.4f;
            L[i] += y; R[i] += y;
            sendL[i] += y * send; sendR[i] += y * send;
        }
    }

    int lastPlucked = -1;

private:
    int intervalCount() const { return chordType == ChSeventh ? 4 : 3; }
    int interval (int k) const
    {
        static const int maj[] = { 0, 4, 7 }, mnr[] = { 0, 3, 7 }, sev[] = { 0, 4, 7, 10 };
        return chordType == ChMinor ? mnr[k] : chordType == ChSeventh ? sev[k] : maj[k];
    }

    struct HarpVoice { bool active = false; int string = 0; double ph = 0, freq = 440; float vel = 1, att = 0; Env env; };
    double sr = 44100;
    HarpVoice harp[16];
    int chordRoot = 0, chordType = ChMajor;
    bool chordOn = false;
    int lastString = -1;
    double cph[5] {}, bph = 0, vibPh = 0;
    float chordEnv = 0;
    Biquad chordLp[2], harpLp;
};

// ===========================================================================
// ECHO (stereo, tempo-synced)
// ===========================================================================
class Echo
{
public:
    void prepare (double newSr)
    {
        sr = newSr;
        for (auto& b : buf) b.assign ((size_t) (sr * 2.1), 0.0f);
        w = 0;
        lp[0].set (0, sr, 5000, 0.7); lp[1] = lp[0];
    }
    void process (float* L, float* R, const float* inL, const float* inR, int n, double delaySec, float feedback, float mix)
    {
        const int size = (int) buf[0].size();
        const int d = std::clamp ((int) (delaySec * sr), 1, size - 1);
        for (int i = 0; i < n; ++i)
        {
            int r = w - d; if (r < 0) r += size;
            const float yl = buf[0][(size_t) r], yr = buf[1][(size_t) r];
            // ping-pong: each side feeds the other
            buf[0][(size_t) w] = inL[i] + lp[0].process (yr) * feedback;
            buf[1][(size_t) w] = inR[i] + lp[1].process (yl) * feedback;
            if (++w >= size) w = 0;
            L[i] += yl * mix; R[i] += yr * mix;
        }
    }
private:
    double sr = 44100;
    std::vector<float> buf[2];
    int w = 0;
    Biquad lp[2];
};
} // namespace sw
