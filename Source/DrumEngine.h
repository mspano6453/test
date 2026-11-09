#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include "Params.h"

// ---------------------------------------------------------------------------
// Sample playback engine. Every sound is a real recorded drum hit
// (5 velocity layers per articulation). Towel / Decay / Tone shape the
// recording; nothing is synthesised.
// ---------------------------------------------------------------------------
namespace tt
{
constexpr double kTwoPi = 6.283185307179586476925;

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() noexcept { z1 = z2 = 0; }
    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void lowpass (double sr, double f, double q = 0.7071)
    {
        double w, c, a; prep (sr, f, q, w, c, a);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + a, -2 * c, 1 - a);
    }
    void highpass (double sr, double f, double q = 0.7071)
    {
        double w, c, a; prep (sr, f, q, w, c, a);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + a, -2 * c, 1 - a);
    }
    void peak (double sr, double f, double q, double db)
    {
        double w, c, a; prep (sr, f, q, w, c, a);
        const double A = std::pow (10.0, db / 40.0);
        set (1 + a * A, -2 * c, 1 - a * A, 1 + a / A, -2 * c, 1 - a / A);
    }
private:
    static void prep (double sr, double f, double q, double& w, double& c, double& a)
    {
        f = std::clamp (f, 10.0, sr * 0.45);
        w = kTwoPi * f / sr; c = std::cos (w); a = std::sin (w) / (2.0 * q);
    }
    void set (double nb0, double nb1, double nb2, double a0, double na1, double na2)
    {
        b0 = (float) (nb0 / a0); b1 = (float) (nb1 / a0); b2 = (float) (nb2 / a0);
        a1 = (float) (na1 / a0); a2 = (float) (na2 / a0);
    }
};

struct SampleData
{
    juce::AudioBuffer<float> mono;
    double sampleRate = 44100.0;
    juce::String name;
};
using SamplePtr = std::shared_ptr<const SampleData>;

// Articulations a MIDI note can ask for
enum Artic { Normal = 0, SideStick, Rim, HatClosed, HatPedal, HatOpen, RideBell, FloorLow,
             Clap, Tamb, TambShake, Shaker, Cowbell, CongaHi, CongaLo, Claves, NumArtics };

constexpr int kLayers = 5;
struct Layers
{
    std::array<SamplePtr, kLayers> v;
    bool empty() const { return v[0] == nullptr; }
    // AVL velocity zones: 1-26, 27-52, 53-77, 78-102, 103-127
    const SamplePtr& pick (float vel) const
    {
        const int mv = juce::roundToInt (vel * 127.0f);
        const int idx = mv <= 26 ? 0 : mv <= 52 ? 1 : mv <= 77 ? 2 : mv <= 102 ? 3 : 4;
        for (int i = idx; i >= 0; --i) if (v[(size_t) i] != nullptr) return v[(size_t) i];
        return v[0];
    }
};

struct KitBank
{
    Layers art[NumPieces][NumArtics];

    const Layers* find (int piece, int artic) const
    {
        if (! art[piece][artic].empty()) return &art[piece][artic];
        if (piece == HiHat && ! art[piece][HatClosed].empty()) return &art[piece][HatClosed];
        if (! art[piece][Normal].empty()) return &art[piece][Normal];
        return nullptr;
    }
};

struct PieceSettings
{
    float tuneRatio = 1, damp = 0.5f, decay = 0.5f, tone = 0.5f;
    float gain = 1, panL = 1, panR = 1, room = 0;
};

struct Voice
{
    bool active = false;
    int artic = Normal;
    uint32_t age = 0;
    SamplePtr smp;
    double pos = 0, rate = 1;
    float gain = 1;
    int hold = 0;          // samples of untouched attack before the towel envelope starts
    float env = 1, envK = 1;
    float choke = 1, chokeK = 1;
    bool choking = false;
    Biquad toneLp;
};

class DrumEngine
{
public:
    static constexpr int kVoices = 8;

    void prepare (double newSr) { sr = newSr; allOff(); }

    void allOff()
    {
        for (auto& pool : voices)
            for (auto& v : pool) { v.active = false; v.smp.reset(); }
    }

    // userSample (if set) replaces the built-in recording for that drum
    void noteOn (int piece, int artic, float noteMul, float vel, const PieceSettings& s,
                 const KitBank& bank, const SamplePtr& userSample)
    {
        // Humanize every hit: a real stick never lands the same way twice.
        const bool cym = piece == HiHat || piece == Ride || piece == Crash || piece == Perc;
        float layerGain = 1.0f;
        SamplePtr smp = userSample;
        if (smp == nullptr)
            if (const auto* layers = bank.find (piece, artic))
            {
                smp = layers->pick (juce::jlimit (0.0f, 1.0f, vel + (rnd() - 0.5f) * 0.12f));
                // don't fire the identical recording twice in a row (the "machine gun" giveaway)
                if (smp == lastSample[piece])
                    for (int i = 0; i < kLayers; ++i)
                        if (layers->v[(size_t) i] == smp)
                        {
                            const int up = i + 1 < kLayers && layers->v[(size_t) (i + 1)] != nullptr ? i + 1 : -1;
                            const int dn = i > 0 ? i - 1 : -1;
                            const int alt = (rnd() < 0.5f && up >= 0) || dn < 0 ? up : dn;
                            if (alt >= 0) { smp = layers->v[(size_t) alt]; layerGain = alt > i ? 0.84f : 1.18f; }
                            break;
                        }
            }
        if (smp == nullptr || smp->mono.getNumSamples() < 2) return;
        lastSample[piece] = smp;
        const float pitchJitter = std::pow (2.0f, (rnd() - 0.5f) * (cym ? 0.06f : 0.16f) / 12.0f);
        const float gainJitter = layerGain * (1.0f + (rnd() - 0.5f) * 0.14f);
        const float toneJitter = 1.0f + (rnd() - 0.5f) * 0.18f;

        if (piece == HiHat && (artic == HatClosed || artic == HatPedal))
            for (auto& o : voices[HiHat])
                if (o.active && o.artic == HatOpen && ! o.choking)
                {
                    o.choking = true;
                    o.chokeK = (float) std::exp (-1.0 / (0.015 * sr));
                }

        Voice& v = grabVoice (piece);
        v.active = true;
        v.artic = artic;
        v.age = ++counter;
        v.smp = std::move (smp);
        v.pos = 0;
        v.rate = v.smp->sampleRate / sr * s.tuneRatio * noteMul * pitchJitter;
        v.choke = 1; v.chokeK = 1; v.choking = false;

        // Velocity: the layers carry the dynamics, this just smooths between them.
        // A user sample has one layer, so it gets the full velocity curve.
        v.gain = (userSample != nullptr ? std::pow (vel, 1.3f) : 0.55f + 0.45f * vel) * gainJitter;

        // Towel: how soon the ring gets choked off after the stick hits.
        const bool cymbal = piece == HiHat || piece == Ride || piece == Crash;
        const double d = s.damp, dec = s.decay;
        double tau = cymbal ? (0.15 + 6.0 * dec * dec) * (1.0 - 0.75 * d) + 0.05
                            : (0.05 + 3.0 * dec * dec) * (1.0 - 0.80 * d) + 0.04;
        if (artic == HatClosed || artic == HatPedal) tau = std::max (tau, 0.4);   // already short
        v.hold = (int) ((cymbal ? 0.010 : 0.018) * sr);
        v.env = 1.0f;
        v.envK = (float) std::exp (-1.0 / (tau * sr));

        // Tone: a towel also dulls the head, so it darkens a little too
        const double cutoff = 1800.0 * std::pow (11.0, (double) s.tone) * (1.0 - 0.35 * d) * (0.75 + 0.25 * vel) * toneJitter;
        v.toneLp.reset();
        v.toneLp.lowpass (sr, cutoff, 0.6);
    }

    void render (int piece, float* out, int n)
    {
        for (auto& v : voices[piece])
            if (v.active) renderVoice (v, out, n);
    }

private:
    SamplePtr lastSample[NumPieces];
    uint32_t rng = 0xC0FFEEu;
    float rnd() noexcept { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (float) (rng & 0xffffff) / 16777216.0f; }
    Voice voices[NumPieces][kVoices];
    double sr = 44100.0;
    uint32_t counter = 0;

    Voice& grabVoice (int piece)
    {
        Voice* oldest = &voices[piece][0];
        for (auto& v : voices[piece])
        {
            if (! v.active) return v;
            if (v.age < oldest->age) oldest = &v;
        }
        return *oldest;
    }

    void renderVoice (Voice& v, float* out, int n)
    {
        const float* d = v.smp->mono.getReadPointer (0);
        const int len = v.smp->mono.getNumSamples();
        for (int i = 0; i < n; ++i)
        {
            if (v.pos >= (double) (len - 1)) { v.active = false; break; }
            const int idx = (int) v.pos;
            const float fr = (float) (v.pos - idx);
            float s = d[idx] + fr * (d[idx + 1] - d[idx]);
            s = v.toneLp.process (s) * v.env * v.choke * v.gain;
            out[i] += s;
            if (v.hold > 0) --v.hold; else v.env *= v.envK;
            if (v.choking) v.choke *= v.chokeK;
            v.pos += v.rate;
        }
        if (v.env < 1.0e-4f || v.choke < 1.0e-4f) v.active = false;
        if (! v.active) v.smp.reset();
    }
};
} // namespace tt
