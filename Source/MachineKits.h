#pragma once
#include "DrumEngine.h"
#include <random>

// ---------------------------------------------------------------------------
// Synthesized drum-machine kits. Each sound is rendered once into a buffer
// (three slightly different takes, so no two hits are identical) and then
// played by the same engine as the recorded kit.
//   1 Boom Machine      - deep booming kick, snappy snare, metallic hats (hip-hop, trap, electro)
//   2 Club Machine      - punchy driven kick, bright noisy snare and hats (house, techno)
//   3 Studio Machine '84 - tight kick, big snare, sweeping electronic toms (80s pop)
// ---------------------------------------------------------------------------
namespace tt
{
class MachineSynth
{
public:
    explicit MachineSynth (uint32_t seed) : gen (seed) {}

    // one rendered take
    SamplePtr render (double seconds, const std::function<float (double t, int i)>& fn, float gain = 1.0f)
    {
        auto d = std::make_shared<SampleData>();
        const int n = juce::jmax (8, (int) (seconds * sr));
        d->mono.setSize (1, n + 2);
        d->mono.clear();
        float* w = d->mono.getWritePointer (0);
        float peak = 1e-6f;
        for (int i = 0; i < n; ++i) { w[i] = fn ((double) i / sr, i); peak = std::max (peak, std::abs (w[i])); }
        const float norm = gain * 0.6f / peak;
        for (int i = 0; i < n; ++i) w[i] *= norm;
        // tiny fade so nothing clicks at the end
        const int f = std::min (n / 4, (int) (0.004 * sr));
        for (int i = 0; i < f; ++i) w[n - 1 - i] *= (float) i / (float) f;
        d->sampleRate = sr;
        return d;
    }
    float noise() { return dist (gen); }
    float jit (float amt) { return 1.0f + amt * dist (gen); }
    double sr = 44100.0;

private:
    std::mt19937 gen;
    std::uniform_real_distribution<float> dist { -1.0f, 1.0f };
};

struct OnePole
{
    float a = 0, z = 0;
    void lp (double sr, double f) { a = (float) std::exp (-6.283185307 * f / sr); }
    float process (float x) { z = x + a * (z - x); return z; }
};

inline void buildMachineKit (int type, KitBank& bank)
{
    MachineSynth m (0x5EED0000u + (uint32_t) type * 977u);
    const double sr = m.sr;
    auto put = [&] (int piece, int artic, const std::function<SamplePtr (int take)>& make)
    {
        for (int t = 0; t < 3; ++t)
        {
            auto s = make (t);
            bank.art[piece][artic].v[(size_t) (t + 1)] = s;
            if (t == 0) bank.art[piece][artic].v[0] = s;
        }
        bank.art[piece][artic].v[4] = bank.art[piece][artic].v[3];
    };
    const double metal[6] = { 205.3, 304.4, 369.6, 522.7, 540.0, 800.0 };

    // ---- the shared building blocks
    auto kick = [&] (double f0, double f1, double sweep, double decay, double click, double drive)
    {
        return [=, &m] (int take) mutable
        {
            const double dj = decay * m.jit (0.05f);
            double ph = 0;
            return m.render (dj * 4.0, [=, &m] (double t, int) mutable
            {
                const double f = f1 + (f0 - f1) * std::exp (-t / sweep);
                ph += 6.283185307 * f / sr;
                double y = std::sin (ph) * std::exp (-t / dj);
                y += click * m.noise() * std::exp (-t / 0.0015);
                return (float) (drive > 0 ? std::tanh (y * (1.0 + drive)) / std::tanh (1.0 + drive) : y);
            });
            juce::ignoreUnused (take);
        };
    };
    auto snare = [&] (double ft1, double ft2, double toneDecay, double noiseDecay, double noiseMix, double lpHz)
    {
        return [=, &m] (int) mutable
        {
            double p1 = 0, p2 = 0; Biquad hp, lp; hp.highpass (sr, 900.0); lp.lowpass (sr, lpHz);
            const double nd = noiseDecay * m.jit (0.08f);
            return m.render (nd * 5.0, [=, &m] (double t, int) mutable
            {
                p1 += 6.283185307 * ft1 * (1.0 + 0.3 * std::exp (-t / 0.01)) / sr;
                p2 += 6.283185307 * ft2 / sr;
                const double tone = (std::sin (p1) + 0.6 * std::sin (p2)) * std::exp (-t / toneDecay);
                const double nz = lp.process (hp.process (m.noise())) * std::exp (-t / nd);
                return (float) (tone * (1.0 - noiseMix) + nz * noiseMix * 1.6);
            });
        };
    };
    auto metalHit = [&] (double decay, double bpHz, double hpHz, double ring, double noiseAmt)
    {
        return [=, &m] (int) mutable
        {
            double ph[6] {}; Biquad bp, hp, hp2; bp.peak (sr, bpHz, 0.7, 10.0); hp.highpass (sr, hpHz); hp2.highpass (sr, hpHz);
            const double dj = decay * m.jit (0.08f);
            return m.render (std::min (4.0, dj * 5.0 + 0.02), [=, &m] (double t, int) mutable
            {
                double x = 0;
                for (int k = 0; k < 6; ++k) { ph[k] += metal[k] * ring / sr; x += (std::fmod (ph[k], 1.0) < 0.5 ? 1.0 : -1.0); }
                x = x / 6.0 * (1.0 - noiseAmt) + m.noise() * noiseAmt;
                const double y = hp2.process (hp.process (bp.process ((float) x)));
                return (float) (y * std::exp (-t / dj));
            });
        };
    };
    auto tom = [&] (double f, double sweepAmt, double sweepTime, double decay, double noiseAmt)
    {
        return [=, &m] (int) mutable
        {
            double ph = 0;
            const double dj = decay * m.jit (0.06f);
            return m.render (dj * 4.5, [=, &m] (double t, int) mutable
            {
                const double fr = f * (1.0 + sweepAmt * std::exp (-t / sweepTime));
                ph += 6.283185307 * fr / sr;
                return (float) ((std::sin (ph) + noiseAmt * m.noise() * std::exp (-t / 0.02)) * std::exp (-t / dj));
            });
        };
    };
    auto clap = [&] (double tail, double bpHz)
    {
        return [=, &m] (int) mutable
        {
            Biquad bp, hp; bp.peak (sr, bpHz, 1.2, 12.0); hp.highpass (sr, 600.0);
            const double sp = 0.009 * m.jit (0.2f);
            return m.render (tail * 4.0 + 0.04, [=, &m] (double t, int) mutable
            {
                double e = 0;
                for (int k = 0; k < 3; ++k) { const double tk = t - k * sp; if (tk >= 0) e = std::max (e, std::exp (-tk / 0.004)); }
                const double t3 = t - 3 * sp;
                if (t3 >= 0) e = std::max (e, 0.8 * std::exp (-t3 / tail));
                return (float) (hp.process (bp.process (m.noise())) * e);
            });
        };
    };
    auto tonal = [&] (double f1, double f2, double decay, double bpHz, bool square)
    {
        return [=, &m] (int) mutable
        {
            double p1 = 0, p2 = 0; Biquad bp; bp.peak (sr, bpHz, 1.0, 8.0);
            const double dj = decay * m.jit (0.06f);
            return m.render (dj * 4.5, [=, &m] (double t, int) mutable
            {
                p1 += f1 / sr; p2 += f2 / sr;
                double x = square ? ((std::fmod (p1, 1.0) < 0.5 ? 1.0 : -1.0) + (f2 > 0 ? (std::fmod (p2, 1.0) < 0.5 ? 1.0 : -1.0) : 0.0))
                                  : std::sin (6.283185307 * p1) + (f2 > 0 ? 0.5 * std::sin (6.283185307 * p2) : 0.0);
                return (float) (bp.process ((float) x) * std::exp (-t / dj));
            });
        };
    };
    auto noiseHit = [&] (double attack, double decay, double hpHz, double lpHz, int bursts)
    {
        return [=, &m] (int) mutable
        {
            Biquad hp, lp; hp.highpass (sr, hpHz); lp.lowpass (sr, lpHz);
            return m.render (attack + decay * 4.0 + bursts * 0.02, [=, &m] (double t, int) mutable
            {
                double e = t < attack ? t / attack : std::exp (-(t - attack) / decay);
                for (int k = 1; k < bursts; ++k) { const double tk = t - k * 0.018; if (tk >= 0) e = std::max (e, 0.6 * std::exp (-tk / decay)); }
                return (float) (lp.process (hp.process (m.noise())) * e);
            });
        };
    };

    switch (type)
    {
        case 1:   // Boom Machine
            put (Kick, Normal, kick (130, 49, 0.035, 0.55, 0.15, 0.0));
            put (Snare, Normal, snare (180, 330, 0.06, 0.14, 0.62, 9000));
            put (Snare, Rim, tonal (1700, 0, 0.012, 1700, false));
            put (Snare, SideStick, tonal (1700, 0, 0.012, 1700, false));
            put (RackTom, Normal, tom (165, 0.15, 0.04, 0.32, 0.05));
            put (FloorTom, Normal, tom (110, 0.15, 0.04, 0.38, 0.05));
            put (FloorTom, FloorLow, tom (85, 0.15, 0.04, 0.42, 0.05));
            put (HiHat, HatClosed, metalHit (0.035, 10000, 7000, 1.0, 0.15));
            put (HiHat, HatPedal, metalHit (0.025, 10000, 7000, 1.0, 0.15));
            put (HiHat, HatOpen, metalHit (0.32, 10000, 7000, 1.0, 0.15));
            put (Ride, Normal, metalHit (0.9, 7500, 5000, 1.0, 0.1));
            put (Ride, RideBell, tonal (540, 800, 0.35, 1400, true));
            put (Crash, Normal, metalHit (1.2, 8000, 4500, 1.4, 0.5));
            put (Perc, Clap, clap (0.12, 1100));
            put (Perc, Cowbell, tonal (540, 800, 0.22, 1300, true));
            put (Perc, CongaHi, tom (360, 0.12, 0.01, 0.16, 0.0));
            put (Perc, CongaLo, tom (250, 0.12, 0.01, 0.2, 0.0));
            put (Perc, Claves, tonal (2500, 0, 0.025, 2500, false));
            put (Perc, Tamb, noiseHit (0.001, 0.09, 6000, 14000, 3));
            put (Perc, TambShake, noiseHit (0.03, 0.12, 5000, 13000, 1));
            put (Perc, Shaker, noiseHit (0.012, 0.04, 5000, 12000, 1));
            break;
        case 2:   // Club Machine
            put (Kick, Normal, kick (260, 52, 0.018, 0.3, 0.35, 1.5));
            put (Snare, Normal, snare (190, 290, 0.05, 0.2, 0.72, 11000));
            put (Snare, Rim, tonal (1100, 2200, 0.015, 1800, false));
            put (Snare, SideStick, tonal (1100, 2200, 0.015, 1800, false));
            put (RackTom, Normal, tom (190, 0.4, 0.05, 0.25, 0.25));
            put (FloorTom, Normal, tom (130, 0.4, 0.05, 0.3, 0.25));
            put (FloorTom, FloorLow, tom (100, 0.4, 0.05, 0.32, 0.25));
            put (HiHat, HatClosed, metalHit (0.045, 11000, 8000, 1.3, 0.6));
            put (HiHat, HatPedal, metalHit (0.03, 11000, 8000, 1.3, 0.6));
            put (HiHat, HatOpen, metalHit (0.28, 11000, 8000, 1.3, 0.6));
            put (Ride, Normal, metalHit (1.1, 9000, 6000, 1.6, 0.5));
            put (Ride, RideBell, tonal (600, 900, 0.4, 1600, true));
            put (Crash, Normal, metalHit (1.5, 9000, 5000, 1.8, 0.75));
            put (Perc, Clap, clap (0.17, 1200));
            put (Perc, Cowbell, tonal (560, 845, 0.18, 1400, true));
            put (Perc, CongaHi, tom (390, 0.2, 0.01, 0.13, 0.05));
            put (Perc, CongaLo, tom (270, 0.2, 0.01, 0.17, 0.05));
            put (Perc, Claves, tonal (2600, 0, 0.02, 2600, false));
            put (Perc, Tamb, noiseHit (0.001, 0.08, 7000, 15000, 3));
            put (Perc, TambShake, noiseHit (0.025, 0.1, 6000, 14000, 1));
            put (Perc, Shaker, noiseHit (0.01, 0.035, 6000, 13000, 1));
            break;
        default:  // Studio Machine '84
            put (Kick, Normal, kick (160, 60, 0.012, 0.22, 0.3, 0.6));
            put (Snare, Normal, snare (210, 350, 0.07, 0.24, 0.66, 8500));
            put (Snare, Rim, tonal (1500, 0, 0.015, 1500, false));
            put (Snare, SideStick, tonal (1300, 0, 0.02, 1300, false));
            put (RackTom, Normal, tom (220, 0.9, 0.12, 0.45, 0.1));      // the big electronic sweep
            put (FloorTom, Normal, tom (140, 0.9, 0.13, 0.55, 0.1));
            put (FloorTom, FloorLow, tom (105, 0.9, 0.14, 0.6, 0.1));
            put (HiHat, HatClosed, noiseHit (0.001, 0.03, 8500, 16000, 1));
            put (HiHat, HatPedal, noiseHit (0.001, 0.022, 8500, 16000, 1));
            put (HiHat, HatOpen, noiseHit (0.002, 0.25, 8000, 16000, 1));
            put (Ride, Normal, metalHit (1.0, 8500, 5500, 1.5, 0.35));
            put (Ride, RideBell, tonal (650, 980, 0.4, 1700, true));
            put (Crash, Normal, metalHit (1.6, 8500, 4500, 1.9, 0.7));
            put (Perc, Clap, clap (0.14, 1300));
            put (Perc, Cowbell, tonal (580, 870, 0.2, 1400, true));
            put (Perc, CongaHi, tom (400, 0.15, 0.01, 0.15, 0.0));
            put (Perc, CongaLo, tom (280, 0.15, 0.01, 0.2, 0.0));
            put (Perc, Claves, tonal (2400, 0, 0.025, 2400, false));
            put (Perc, Tamb, noiseHit (0.001, 0.1, 6500, 15000, 3));
            put (Perc, TambShake, noiseHit (0.03, 0.12, 5500, 14000, 1));
            put (Perc, Shaker, noiseHit (0.012, 0.04, 5500, 13000, 1));
            break;
    }
}
} // namespace tt
