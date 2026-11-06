#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <vector>

// ---------------------------------------------------------------------------
// Lathe DSP: the mastering modules. Everything processes stereo (L, R) in place.
// ---------------------------------------------------------------------------
namespace lt
{
constexpr double kPi = 3.14159265358979323846;

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g)  { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }

struct Coeffs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };

enum class FT { LowPass, HighPass, Peak, LowShelf, HighShelf, BandPass };

inline Coeffs design (FT type, double sr, double f, double q, double db)
{
    f = std::clamp (f, 10.0, sr * 0.49);
    q = std::max (q, 0.05);
    const double w = 2.0 * kPi * f / sr, c = std::cos (w), s = std::sin (w), al = s / (2.0 * q);
    const double A = std::pow (10.0, db / 40.0), sA = std::sqrt (A);
    double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;
    switch (type)
    {
        case FT::LowPass:  b0 = (1 - c) / 2; b1 = 1 - c; b2 = b0; a0 = 1 + al; a1 = -2 * c; a2 = 1 - al; break;
        case FT::HighPass: b0 = (1 + c) / 2; b1 = -(1 + c); b2 = b0; a0 = 1 + al; a1 = -2 * c; a2 = 1 - al; break;
        case FT::BandPass: b0 = al; b1 = 0; b2 = -al; a0 = 1 + al; a1 = -2 * c; a2 = 1 - al; break;
        case FT::Peak:     b0 = 1 + al * A; b1 = -2 * c; b2 = 1 - al * A; a0 = 1 + al / A; a1 = -2 * c; a2 = 1 - al / A; break;
        case FT::LowShelf:
            b0 = A * ((A + 1) - (A - 1) * c + 2 * sA * al); b1 = 2 * A * ((A - 1) - (A + 1) * c);
            b2 = A * ((A + 1) - (A - 1) * c - 2 * sA * al); a0 = (A + 1) + (A - 1) * c + 2 * sA * al;
            a1 = -2 * ((A - 1) + (A + 1) * c);             a2 = (A + 1) + (A - 1) * c - 2 * sA * al; break;
        case FT::HighShelf:
            b0 = A * ((A + 1) + (A - 1) * c + 2 * sA * al); b1 = -2 * A * ((A - 1) + (A + 1) * c);
            b2 = A * ((A + 1) + (A - 1) * c - 2 * sA * al); a0 = (A + 1) - (A - 1) * c + 2 * sA * al;
            a1 = 2 * ((A - 1) - (A + 1) * c);              a2 = (A + 1) - (A - 1) * c - 2 * sA * al; break;
    }
    return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
}

inline double magnitudeDb (const Coeffs& k, double f, double sr)
{
    const std::complex<double> z1 = std::polar (1.0, -2.0 * kPi * f / sr), z2 = z1 * z1;
    const auto h = (k.b0 + k.b1 * z1 + k.b2 * z2) / (1.0 + k.a1 * z1 + k.a2 * z2);
    return 20.0 * std::log10 (std::max (std::abs (h), 1.0e-9));
}

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void set (const Coeffs& k) { b0 = (float) k.b0; b1 = (float) k.b1; b2 = (float) k.b2; a1 = (float) k.a1; a2 = (float) k.a2; }
    void reset() { z1 = z2 = 0; }
    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// ---------------------------------------------------------------------------
// 3-band Linkwitz-Riley splitter (phase-aligned, sums back flat)
// ---------------------------------------------------------------------------
struct Splitter3
{
    juce::dsp::LinkwitzRileyFilter<float> lowHigh, midHigh, lowAp;
    void prepare (double sr)
    {
        juce::dsp::ProcessSpec spec { sr, 512, 2 };
        lowHigh.prepare (spec); midHigh.prepare (spec); lowAp.prepare (spec);
        lowAp.setType (juce::dsp::LinkwitzRileyFilterType::allpass);
    }
    void setCrossovers (float f1, float f2)
    {
        f2 = std::max (f2, f1 * 1.5f);
        lowHigh.setCutoffFrequency (f1);
        midHigh.setCutoffFrequency (f2);
        lowAp.setCutoffFrequency (f2);
    }
    inline void split (int ch, float x, float& lo, float& mid, float& hi) noexcept
    {
        float l, h;
        lowHigh.processSample (ch, x, l, h);
        midHigh.processSample (ch, h, mid, hi);
        lo = lowAp.processSample (ch, l);
    }
};

// ---------------------------------------------------------------------------
// EQUALIZER: low cut, low shelf, 4 bells, high shelf, high cut
// ---------------------------------------------------------------------------
enum EqBandType { EqLowCut = 0, EqLowShelf, EqBell1, EqBell2, EqBell3, EqBell4, EqHighShelf, EqHighCut, NumEqBands };

struct EqBand { bool on = false; float freq = 1000, gain = 0, q = 0.707f; };

inline Coeffs eqCoeffs (int band, const EqBand& b, double sr)
{
    switch (band)
    {
        case EqLowCut:    return design (FT::HighPass, sr, b.freq, b.q, 0);
        case EqHighCut:   return design (FT::LowPass, sr, b.freq, b.q, 0);
        case EqLowShelf:  return design (FT::LowShelf, sr, b.freq, b.q, b.gain);
        case EqHighShelf: return design (FT::HighShelf, sr, b.freq, b.q, b.gain);
        default:          return design (FT::Peak, sr, b.freq, b.q, b.gain);
    }
}

struct Equalizer
{
    double sr = 44100;
    EqBand bands[NumEqBands];
    bool steepCuts = true;            // 24 dB/oct cuts (two stages)
    Biquad f[NumEqBands][2][2];       // [band][channel][stage]

    void prepare (double s) { sr = s; for (auto& b : f) for (auto& c : b) for (auto& st : c) st.reset(); }
    void update()
    {
        for (int b = 0; b < NumEqBands; ++b)
        {
            const auto k = eqCoeffs (b, bands[b], sr);
            for (int c = 0; c < 2; ++c) { f[b][c][0].set (k); f[b][c][1].set (k); }
        }
    }
    void process (float* L, float* R, int n)
    {
        for (int b = 0; b < NumEqBands; ++b)
        {
            if (! bands[b].on) continue;
            const bool two = steepCuts && (b == EqLowCut || b == EqHighCut);
            for (int c = 0; c < 2; ++c)
            {
                float* x = c == 0 ? L : R;
                auto& s0 = f[b][c][0]; auto& s1 = f[b][c][1];
                if (two) for (int i = 0; i < n; ++i) x[i] = s1.process (s0.process (x[i]));
                else     for (int i = 0; i < n; ++i) x[i] = s0.process (x[i]);
            }
        }
    }
    double responseDb (double freq) const
    {
        double db = 0;
        for (int b = 0; b < NumEqBands; ++b)
            if (bands[b].on)
            {
                const double m = magnitudeDb (eqCoeffs (b, bands[b], sr), freq, sr);
                db += (steepCuts && (b == EqLowCut || b == EqHighCut)) ? 2 * m : m;
            }
        return db;
    }
};

// ---------------------------------------------------------------------------
// DYNAMIC EQ: bells that only cut when their band gets loud
// ---------------------------------------------------------------------------
struct DynEqBand { bool on = false; float freq = 3500, q = 1.5f, thresholdDb = -24, rangeDb = 6; };

struct DynamicEq
{
    static constexpr int kBands = 2;
    double sr = 44100;
    DynEqBand bands[kBands];
    Biquad detect[kBands], eq[kBands][2];
    float env[kBands] {}, cut[kBands] {};
    float atk = 0, rel = 0;
    int counter = 0;

    void prepare (double s)
    {
        sr = s;
        atk = std::exp (-1.0f / (0.005f * (float) sr));
        rel = std::exp (-1.0f / (0.12f * (float) sr));
        for (int b = 0; b < kBands; ++b) { detect[b].reset(); eq[b][0].reset(); eq[b][1].reset(); env[b] = 0; cut[b] = 0; }
    }
    void process (float* L, float* R, int n)
    {
        for (int b = 0; b < kBands; ++b)
            if (bands[b].on) detect[b].set (design (FT::BandPass, sr, bands[b].freq, bands[b].q, 0));
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (L[i] + R[i]);
            for (int b = 0; b < kBands; ++b)
            {
                if (! bands[b].on) { cut[b] = 0; continue; }
                const float d = std::abs (detect[b].process (m));
                env[b] = d > env[b] ? d + atk * (env[b] - d) : d + rel * (env[b] - d);
                if (((counter + i) & 15) == 0)
                {
                    const float over = gainToDb (env[b]) - bands[b].thresholdDb;
                    cut[b] = std::clamp (over * 0.7f, 0.0f, bands[b].rangeDb);
                    const auto k = design (FT::Peak, sr, bands[b].freq, bands[b].q, -cut[b]);
                    eq[b][0].set (k); eq[b][1].set (k);
                }
                L[i] = eq[b][0].process (L[i]);
                R[i] = eq[b][1].process (R[i]);
            }
        }
        counter += n;
    }
};

// ---------------------------------------------------------------------------
// DYNAMICS: 3-band compressor
// ---------------------------------------------------------------------------
struct CompBand { float thresholdDb = -18, ratio = 2, attackMs = 30, releaseMs = 200, gainDb = 0; };

struct MultibandComp
{
    double sr = 44100;
    Splitter3 split;
    float x1 = 120, x2 = 2500;
    CompBand bands[3];
    float gr[3] {};          // current gain reduction (dB, <= 0)
    float grMeter[3] {};     // max reduction this block, for the UI

    void prepare (double s) { sr = s; split.prepare (s); for (auto& g : gr) g = 0; }
    void process (float* L, float* R, int n)
    {
        split.setCrossovers (x1, x2);
        float atk[3], rel[3], mk[3];
        for (int b = 0; b < 3; ++b)
        {
            atk[b] = std::exp (-1.0f / (std::max (0.1f, bands[b].attackMs) * 0.001f * (float) sr));
            rel[b] = std::exp (-1.0f / (std::max (1.0f, bands[b].releaseMs) * 0.001f * (float) sr));
            mk[b] = dbToGain (bands[b].gainDb);
            grMeter[b] = 0;
        }
        const float knee = 6.0f;
        for (int i = 0; i < n; ++i)
        {
            float l[3], r[3];
            split.split (0, L[i], l[0], l[1], l[2]);
            split.split (1, R[i], r[0], r[1], r[2]);
            float outL = 0, outR = 0;
            for (int b = 0; b < 3; ++b)
            {
                const auto& p = bands[b];
                const float lvl = gainToDb (std::max (std::abs (l[b]), std::abs (r[b])));
                const float over = lvl - p.thresholdDb;
                const float slope = 1.0f / std::max (1.0f, p.ratio) - 1.0f;
                float target = 0;
                if (over > knee * 0.5f)       target = slope * over;
                else if (over > -knee * 0.5f) target = slope * (over + knee * 0.5f) * (over + knee * 0.5f) / (2 * knee);
                gr[b] = target < gr[b] ? target + atk[b] * (gr[b] - target) : target + rel[b] * (gr[b] - target);
                grMeter[b] = std::min (grMeter[b], gr[b]);
                const float g = dbToGain (gr[b]) * mk[b];
                outL += l[b] * g; outR += r[b] * g;
            }
            L[i] = outL; R[i] = outR;
        }
    }
};

// ---------------------------------------------------------------------------
// EXCITER: 3-band harmonic exciter
// ---------------------------------------------------------------------------
enum ExMode { ExTape = 0, ExTube, ExWarm, ExRetro };

struct Exciter
{
    double sr = 44100;
    Splitter3 split;
    float x1 = 300, x2 = 4000, mix = 1.0f;
    float amount[3] { 0, 0.2f, 0.25f };
    int mode[3] { ExWarm, ExTube, ExTape };
    float dcz[3][2] {};
    float dcA = 0.999f;

    void prepare (double s) { sr = s; split.prepare (s); dcA = (float) std::exp (-2.0 * kPi * 12.0 / s); for (auto& b : dcz) b[0] = b[1] = 0; }

    static inline float shape (int m, float x) noexcept
    {
        switch (m)
        {
            case ExTape:  return std::tanh (x);
            case ExTube:  return std::tanh (x + 0.3f * x * x);
            case ExWarm:  { const float c = std::clamp (x, -1.5f, 1.5f); return c - c * c * c / 6.75f; }
            default:      return (x > 0 ? 1.0f : -1.0f) * (1.0f - std::exp (-2.0f * std::abs (x)));
        }
    }
    void process (float* L, float* R, int n)
    {
        split.setCrossovers (x1, x2);
        for (int i = 0; i < n; ++i)
        {
            float bands[2][3];
            split.split (0, L[i], bands[0][0], bands[0][1], bands[0][2]);
            split.split (1, R[i], bands[1][0], bands[1][1], bands[1][2]);
            for (int c = 0; c < 2; ++c)
            {
                float out = 0;
                for (int b = 0; b < 3; ++b)
                {
                    const float x = bands[c][b];
                    const float a = amount[b];
                    if (a <= 0.0001f) { out += x; continue; }
                    const float k = 1.0f + 8.0f * a;
                    float h = shape (mode[b], k * x * 2.0f) / (k * 2.0f) - x * 0.5f;    // harmonics only
                    // remove DC that even harmonics create
                    const float z = dcz[b][c];
                    dcz[b][c] = (1.0f - dcA) * h + dcA * z;
                    h -= dcz[b][c];
                    out += x + h * a * 2.0f * mix;
                }
                (c == 0 ? L : R)[i] = out;
            }
        }
    }
};

// ---------------------------------------------------------------------------
// IMAGER: 3-band width + Stereoize
// ---------------------------------------------------------------------------
struct Imager
{
    double sr = 44100;
    Splitter3 split;
    float x1 = 250, x2 = 4000, width[3] { 1, 1, 1 }, stereoize = 0;
    std::vector<float> dline; int w = 0, delaySamples = 0;
    Biquad stHp;

    void prepare (double s)
    {
        sr = s; split.prepare (s);
        delaySamples = (int) (0.011 * s);
        dline.assign ((size_t) delaySamples + 4, 0.0f); w = 0;
        stHp.set (design (FT::HighPass, s, 400, 0.707, 0)); stHp.reset();
    }
    void process (float* L, float* R, int n)
    {
        split.setCrossovers (x1, x2);
        for (int i = 0; i < n; ++i)
        {
            float l[3], r[3];
            split.split (0, L[i], l[0], l[1], l[2]);
            split.split (1, R[i], r[0], r[1], r[2]);
            float m = 0, s = 0;
            for (int b = 0; b < 3; ++b)
            {
                m += 0.5f * (l[b] + r[b]);
                s += 0.5f * (l[b] - r[b]) * width[b];
            }
            if (stereoize > 0.0001f)
            {
                dline[(size_t) w] = m;
                int rd = w - delaySamples; if (rd < 0) rd += (int) dline.size();
                if (++w >= (int) dline.size()) w = 0;
                s += stHp.process (dline[(size_t) rd]) * stereoize * 0.5f;
            }
            L[i] = m + s; R[i] = m - s;
        }
    }
};

// ---------------------------------------------------------------------------
// VINTAGE TAPE: saturation, head bump, softened top
// ---------------------------------------------------------------------------
struct VintageTape
{
    double sr = 44100;
    float drive = 0.3f, bumpDb = 1.5f, warmth = 0.3f, mix = 1.0f;
    bool speed30 = true;
    Biquad bump[2], shelf[2], lp[2];

    void prepare (double s) { sr = s; for (int c = 0; c < 2; ++c) { bump[c].reset(); shelf[c].reset(); lp[c].reset(); } }
    void process (float* L, float* R, int n)
    {
        const auto kb = design (FT::Peak, sr, speed30 ? 90.0 : 48.0, 1.0, bumpDb);
        const auto ks = design (FT::HighShelf, sr, 8000.0, 0.7, -6.0 * warmth);
        const auto kl = design (FT::LowPass, sr, speed30 ? 22000.0 : 16500.0, 0.707, 0);
        for (int c = 0; c < 2; ++c) { bump[c].set (kb); shelf[c].set (ks); lp[c].set (kl); }
        const float d = 1.0f + drive * 6.0f, bias = 0.1f * drive, tb = std::tanh (bias);
        const float norm = 1.0f / (d * (1.0f - tb * tb));
        for (int c = 0; c < 2; ++c)
        {
            float* x = c == 0 ? L : R;
            for (int i = 0; i < n; ++i)
            {
                const float dry = x[i];
                float y = (std::tanh (d * dry + bias) - tb) * norm;
                y = lp[c].process (shelf[c].process (bump[c].process (y)));
                x[i] = dry + (y - dry) * mix;
            }
        }
    }
};

// ---------------------------------------------------------------------------
// MAXIMIZER: look-ahead brickwall limiter with (approximate) true-peak detection
// ---------------------------------------------------------------------------
struct Maximizer
{
    double sr = 44100;
    float gainDb = 0, ceilingDb = -1, releaseMs = 120;
    bool truePeak = true;
    int look = 64;                    // latency in samples
    int W = 65;                       // window = look + 1
    std::vector<float> dl[2], box;
    std::vector<std::pair<int64_t, float>> dq;   // monotonic deque (sliding minimum)
    int dqHead = 0, dqSize = 0;
    int64_t t = 0;
    int dw = 0, bw = 0;
    double boxSum = 0;
    float relState = 1.0f;
    float hist[2][4] {};
    float grDb = 0;

    void prepare (double s)
    {
        sr = s;
        look = std::max (8, (int) (0.0015 * s));
        W = look + 1;
        for (auto& d : dl) d.assign ((size_t) look, 0.0f);
        box.assign ((size_t) W, 1.0f);
        dq.assign ((size_t) W + 2, { 0, 1.0f });
        dqHead = dqSize = 0; t = 0; dw = bw = 0; boxSum = W; relState = 1.0f;
        for (auto& h : hist) for (auto& v : h) v = 0;
    }
    int latency() const { return look; }

    void process (float* L, float* R, int n)
    {
        const float g = dbToGain (gainDb), ceil = dbToGain (ceilingDb);
        const float rel = std::exp (-1.0f / (std::max (5.0f, releaseMs) * 0.001f * (float) sr));
        const int cap = (int) dq.size();
        float most = 1.0f;
        for (int i = 0; i < n; ++i)
        {
            const float xl = L[i] * g, xr = R[i] * g;
            float pk = std::max (std::abs (xl), std::abs (xr));
            if (truePeak)
            {
                const float xs[2] = { xl, xr };
                for (int c = 0; c < 2; ++c)
                {
                    float* h = hist[c];
                    h[0] = h[1]; h[1] = h[2]; h[2] = h[3]; h[3] = xs[c];
                    pk = std::max (pk, std::abs ((-h[0] + 9.0f * h[1] + 9.0f * h[2] - h[3]) * (1.0f / 16.0f)));
                }
            }
            const float r = pk > ceil ? ceil / pk : 1.0f;

            // sliding minimum over the last W samples
            while (dqSize > 0 && dq[(size_t) ((dqHead + dqSize - 1) % cap)].second >= r) --dqSize;
            dq[(size_t) ((dqHead + dqSize) % cap)] = { t, r }; ++dqSize;
            while (dq[(size_t) dqHead].first <= t - W) { dqHead = (dqHead + 1) % cap; --dqSize; }
            const float m = dq[(size_t) dqHead].second;
            ++t;

            relState = m < relState ? m : m + rel * (relState - m);

            // box filter over W: every term already respects the delayed sample's limit
            boxSum += relState - box[(size_t) bw];
            box[(size_t) bw] = relState;
            bw = (bw + 1) % W;
            const float gain = (float) std::min (1.0, boxSum / W);

            const float ol = dl[0][(size_t) dw], orr = dl[1][(size_t) dw];
            dl[0][(size_t) dw] = xl; dl[1][(size_t) dw] = xr;
            dw = (dw + 1) % look;

            L[i] = std::clamp (ol * gain, -ceil, ceil);
            R[i] = std::clamp (orr * gain, -ceil, ceil);
            most = std::min (most, gain);
        }
        grDb = gainToDb (most);
    }
};

// ---------------------------------------------------------------------------
// LOUDNESS (EBU R128 / ITU BS.1770): momentary, short-term, integrated
// ---------------------------------------------------------------------------
struct Loudness
{
    double sr = 48000;
    Biquad k1[2], k2[2];
    int blockLen = 4800, blockPos = 0;
    double blockSum = 0;
    double ring[30] {};       // 100 ms block energies (3 s)
    int ringPos = 0, ringCount = 0;
    // integrated: histogram of 400 ms gating-block loudness (0.1 LU bins from -70 to +5)
    static constexpr int kBins = 750;
    double binEnergy[kBins] {}; int binCount[kBins] {};
    std::atomic<float> momentary { -70 }, shortTerm { -70 }, integrated { -70 };

    void prepare (double s)
    {
        sr = s;
        const auto c1 = design (FT::HighShelf, s, 1681.974450955533, 0.7071752369554196, 3.999843853973347);
        const auto c2 = design (FT::HighPass, s, 38.13547087602444, 0.5003270373238773, 0);
        for (int c = 0; c < 2; ++c) { k1[c].set (c1); k2[c].set (c2); k1[c].reset(); k2[c].reset(); }
        blockLen = (int) (0.1 * s);
        reset();
    }
    void reset()
    {
        blockPos = 0; blockSum = 0; ringPos = ringCount = 0;
        for (auto& r : ring) r = 0;
        for (int i = 0; i < kBins; ++i) { binEnergy[i] = 0; binCount[i] = 0; }
        momentary = -70; shortTerm = -70; integrated = -70;
    }
    static double lufs (double meanSquare) { return -0.691 + 10.0 * std::log10 (std::max (meanSquare, 1.0e-12)); }

    void process (const float* L, const float* R, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            const double a = k2[0].process (k1[0].process (L[i]));
            const double b = k2[1].process (k1[1].process (R[i]));
            blockSum += a * a + b * b;
            if (++blockPos >= blockLen)
            {
                ring[ringPos] = blockSum / blockLen;
                ringPos = (ringPos + 1) % 30;
                ringCount = std::min (ringCount + 1, 30);
                blockSum = 0; blockPos = 0;
                auto avg = [this] (int blocks)
                {
                    double s = 0; const int c = std::min (blocks, ringCount);
                    for (int k = 1; k <= c; ++k) s += ring[(ringPos - k + 30) % 30];
                    return c > 0 ? s / c : 0.0;
                };
                const double m400 = avg (4);
                momentary = (float) lufs (m400);
                shortTerm = (float) lufs (avg (30));
                if (ringCount >= 4)
                {
                    const double l = lufs (m400);
                    if (l > -70.0)
                    {
                        const int bin = std::clamp ((int) ((l + 70.0) * 10.0), 0, kBins - 1);
                        binEnergy[bin] += m400; binCount[bin] += 1;
                    }
                    integrated = (float) computeIntegrated();
                }
            }
        }
    }
    double computeIntegrated() const
    {
        double e = 0; long c = 0;
        for (int i = 0; i < kBins; ++i) { e += binEnergy[i]; c += binCount[i]; }
        if (c == 0) return -70;
        const double relGate = lufs (e / c) - 10.0;
        e = 0; c = 0;
        for (int i = std::max (0, (int) ((relGate + 70.0) * 10.0)); i < kBins; ++i) { e += binEnergy[i]; c += binCount[i]; }
        return c > 0 ? lufs (e / c) : -70;
    }
};

// TPDF dither to 16 or 24 bits
struct Dither
{
    uint32_t s = 12345;
    inline float rnd() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s & 0xffffff) / 16777216.0f; }
    void process (float* x, int n, int bits)
    {
        if (bits <= 0) return;
        const float q = 1.0f / (float) (1 << (bits - 1));
        for (int i = 0; i < n; ++i)
            x[i] = std::round ((x[i] + (rnd() - rnd()) * q) / q) * q;
    }
};
} // namespace lt
