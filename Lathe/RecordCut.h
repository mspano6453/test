#pragma once
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>

// ---------------------------------------------------------------------------
// RECORD CUT: hear the master the way it would have come out: a 60s mono
// single, a worn-out record, a cassette, or a 33 played at 45.
// ---------------------------------------------------------------------------
namespace lt
{
class RecordCut
{
public:
    enum Mode { Off = 0, MonoSingle, WornVinyl, Cassette, At45, NumModes };

    void prepare (double s)
    {
        sr = s;
        for (auto& d : dl) d.assign ((size_t) (sr * 0.2) + 8, 0.0f);
        w = 0; wowPh = flutPh = 0; shPh = 0; crackleEnv = 0; rumblePh = 0;
        for (auto& f : hp) f.reset(); for (auto& f : lp) f.reset(); for (auto& f : mid) f.reset(); hissLp.reset(); crackLp.reset();
        last = -1;
    }

    void process (float* L, float* R, int n, int mode)
    {
        if (mode <= Off || mode >= NumModes) { last = mode; return; }
        if (mode != last) setup (mode);
        const size_t N = dl[0].size();
        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R[i];
            if (mode == MonoSingle) { const float m = 0.5f * (l + r); l = r = m; }
            // write, then read back with the speed wobbles (or the 45 rpm pitch shift)
            dl[0][w] = l; dl[1][w] = r;
            if (mode == At45)
            {
                // two read heads sliding through a 60 ms window, crossfaded: everything a fourth-and-a-bit up
                const double win = 0.06 * sr;
                shPh += (ratio - 1.0) / win; shPh -= std::floor (shPh);
                float o[2] = { 0, 0 };
                for (int h = 0; h < 2; ++h)
                {
                    const double ph = std::fmod (shPh + 0.5 * h, 1.0);
                    const double d = (1.0 - ph) * win + 4.0;
                    const float g = (float) std::sin (3.14159265 * ph);
                    for (int c = 0; c < 2; ++c) o[c] += read (c, d, N) * g;
                }
                l = o[0]; r = o[1];
            }
            else
            {
                wowPh += wowHz / sr; wowPh -= std::floor (wowPh);
                flutPh += flutHz / sr; flutPh -= std::floor (flutPh);
                const double d = (0.006 + wowDepth * (1.0 + std::sin (6.2831853 * wowPh)) + flutDepth * (1.0 + std::sin (6.2831853 * flutPh))) * sr;
                l = read (0, d, N); r = read (1, d, N);
            }
            w = (w + 1) % N;
            // the bandwidth of the medium
            l = lp[0].lp (hp[0].hp (l, hpK), lpK); r = lp[1].lp (hp[1].hp (r, hpK), lpK);
            if (midGain > 0) { l += mid[0].bp (l, midK) * midGain; r += mid[1].bp (r, midK) * midGain; }
            // a little saturation of the cutting lathe / the tape
            if (drive > 0) { l = std::tanh (l * (1.0f + drive)) / (1.0f + drive * 0.5f); r = std::tanh (r * (1.0f + drive)) / (1.0f + drive * 0.5f); }
            // noise: tape hiss, surface noise, crackle, rumble
            float nz = 0;
            if (hiss > 0) nz += hissLp.lp (rnd() * 2.0f - 1.0f, 0.5f) * hiss;
            if (crackle > 0)
            {
                if (rnd() < crackle * 0.0012f) crackleEnv = (rnd() * 0.6f + 0.4f) * (rnd() < 0.15f ? 1.0f : 0.35f);
                nz += crackLp.lp ((rnd() * 2.0f - 1.0f) * crackleEnv, 0.6f) * 0.09f;
                crackleEnv *= 0.82f;
                rumblePh += 28.0 / sr; rumblePh -= std::floor (rumblePh);
                nz += (float) std::sin (6.2831853 * rumblePh) * 0.0025f;
            }
            if (mode == MonoSingle || mode == At45) { L[i] = l + nz; R[i] = r + nz; }
            else { L[i] = l + nz; R[i] = r + nz * 0.8f + (rnd() - 0.5f) * hiss * 0.3f; }
        }
    }

private:
    struct F1
    {
        float a = 0, b = 0;
        void reset() { a = b = 0; }
        float lp (float x, float k) { a += (x - a) * k; return a; }
        float hp (float x, float k) { a += (x - a) * k; return x - a; }
        float bp (float x, float k) { a += (x - a) * k; b += (a - b) * k; return a - b; }
    };
    double sr = 44100, wowPh = 0, flutPh = 0, shPh = 0, rumblePh = 0, ratio = 1.35;
    std::vector<float> dl[2]; size_t w = 0;
    F1 hp[2], lp[2], mid[2], hissLp, crackLp;
    float hpK = 0, lpK = 1, midK = 0, midGain = 0, drive = 0, hiss = 0, crackle = 0, crackleEnv = 0;
    double wowHz = 0.5, flutHz = 7, wowDepth = 0, flutDepth = 0;
    int last = -1;
    uint32_t seed = 0x1234567u;
    float rnd() { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return (float) (seed & 0xffffff) / 16777216.0f; }
    float read (int c, double d, size_t N)
    {
        const double p = (double) w - d + (double) N * 2;
        const size_t a = (size_t) p % N, b = (a + 1) % N; const float fr = (float) (p - std::floor (p));
        return dl[c][a] + (dl[c][b] - dl[c][a]) * fr;
    }
    float k (double hz) const { return (float) (1.0 - std::exp (-6.2831853 * hz / sr)); }
    void setup (int mode)
    {
        last = mode;
        midGain = 0; drive = 0; hiss = 0; crackle = 0; wowDepth = flutDepth = 0;
        switch (mode)
        {
            case MonoSingle: hpK = k (90); lpK = k (8500); midK = k (2500); midGain = 0.35f; drive = 0.6f; wowDepth = 0.00004; wowHz = 0.55; crackle = 0.15f; break;
            case WornVinyl:  hpK = k (40); lpK = k (10000); drive = 0.25f; hiss = 0.004f; crackle = 1.0f; wowDepth = 0.00012; wowHz = 0.55; flutDepth = 0.00001; flutHz = 6; break;
            case Cassette:   hpK = k (35); lpK = k (11500); midK = k (90); midGain = 0.25f; drive = 0.8f; hiss = 0.012f; wowDepth = 0.00008; wowHz = 0.7; flutDepth = 0.00003; flutHz = 9; break;
            case At45:       hpK = k (60); lpK = k (12000); drive = 0.2f; crackle = 0.4f; ratio = 45.0 / 33.333; break;
            default: break;
        }
    }
};
} // namespace lt
