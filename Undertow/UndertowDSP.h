#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

// ---------------------------------------------------------------------------
// Undertow DSP: a picked bass (real samples), the rig it goes through, and the
// ears (chord detection + pitch tracking) that let it listen to a guitar.
// ---------------------------------------------------------------------------
namespace ut
{
constexpr double kTwoPi = 6.283185307179586476925;
constexpr double kPi = 3.14159265358979323846;
constexpr int kLowSample = 23, kHighSample = 63;     // B0 .. Eb4, every semitone
constexpr int kLayers = 2, kRR = 3;                  // layer 0 = medium (2 round robins), layer 1 = hard (3)

struct Rng
{
    uint32_t s = 0x2545F491u;
    void seed (uint32_t v) { s = v ? v : 0x2545F491u; }
    inline float u() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s & 0xffffff) / 16777216.0f; }
    inline float n() noexcept { return u() * 2.0f - 1.0f; }
    inline int i (int n) noexcept { return std::min (n - 1, (int) (u() * (float) n)); }
    inline bool chance (float p) noexcept { return u() < p; }
};

inline const char* pcName (int pc) { static const char* n[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; return n[((pc % 12) + 12) % 12]; }
inline juce::String noteName (int midi) { return juce::String (pcName (midi)) + juce::String (midi / 12 - 1); }

// RBJ biquad with shelves and peak
struct Biquad
{
    enum Type { LP, HP, BP, LowShelf, HighShelf, Peak };
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y;
        return y;
    }
    void set (Type t, double sr, double f, double q, double gainDb = 0)
    {
        f = std::clamp (f, 10.0, sr * 0.45);
        const double A = std::pow (10.0, gainDb / 40.0), w = kTwoPi * f / sr, c = std::cos (w), s = std::sin (w);
        const double al = s / (2.0 * std::max (0.05, q));
        double nb0 = 1, nb1 = 0, nb2 = 0, na0 = 1, na1 = 0, na2 = 0;
        switch (t)
        {
            case LP: nb0 = (1 - c) / 2; nb1 = 1 - c; nb2 = nb0; na0 = 1 + al; na1 = -2 * c; na2 = 1 - al; break;
            case HP: nb0 = (1 + c) / 2; nb1 = -(1 + c); nb2 = nb0; na0 = 1 + al; na1 = -2 * c; na2 = 1 - al; break;
            case BP: nb0 = al; nb1 = 0; nb2 = -al; na0 = 1 + al; na1 = -2 * c; na2 = 1 - al; break;
            case Peak: nb0 = 1 + al * A; nb1 = -2 * c; nb2 = 1 - al * A; na0 = 1 + al / A; na1 = -2 * c; na2 = 1 - al / A; break;
            case LowShelf:
            {
                const double sq = 2 * std::sqrt (A) * al;
                nb0 = A * ((A + 1) - (A - 1) * c + sq); nb1 = 2 * A * ((A - 1) - (A + 1) * c); nb2 = A * ((A + 1) - (A - 1) * c - sq);
                na0 = (A + 1) + (A - 1) * c + sq; na1 = -2 * ((A - 1) + (A + 1) * c); na2 = (A + 1) + (A - 1) * c - sq; break;
            }
            case HighShelf:
            {
                const double sq = 2 * std::sqrt (A) * al;
                nb0 = A * ((A + 1) + (A - 1) * c + sq); nb1 = -2 * A * ((A - 1) + (A + 1) * c); nb2 = A * ((A + 1) + (A - 1) * c - sq);
                na0 = (A + 1) - (A - 1) * c + sq; na1 = 2 * ((A - 1) - (A + 1) * c); na2 = (A + 1) - (A - 1) * c - sq; break;
            }
        }
        b0 = (float) (nb0 / na0); b1 = (float) (nb1 / na0); b2 = (float) (nb2 / na0);
        a1 = (float) (na1 / na0); a2 = (float) (na2 / na0);
    }
};

// ===========================================================================
// Sample bank + mono bass voice
// ===========================================================================
struct Sample { std::vector<float> data; double sr = 44100; };

struct Bank
{
    // [note - kLowSample][layer][rr]; empty rr slots are skipped
    std::array<std::array<std::array<std::shared_ptr<Sample>, kRR>, kLayers>, kHighSample - kLowSample + 1> s;
    int rrCount (int note, int layer) const
    {
        int c = 0;
        for (auto& x : s[(size_t) note][(size_t) layer]) if (x) ++c;
        return c;
    }
};

class BassVoice
{
public:
    bool active() const { return smp != nullptr; }
    void start (const std::shared_ptr<Sample>& sample, double rate, float g, double hostSr)
    {
        smp = sample; pos = 0; inc = rate * sample->sr / hostSr; gain = g;
        env = 1; rel = 0; age = 0; srHost = hostSr;
    }
    void release (float seconds) { if (smp && rel == 0) rel = 1.0f / (float) std::max (1.0, seconds * srHost); }
    void kill() { release (0.006f); }
    int age = 0;
    inline float render() noexcept
    {
        if (! smp) return 0;
        const auto& d = smp->data;
        const int i0 = (int) pos;
        if (i0 + 1 >= (int) d.size()) { smp.reset(); return 0; }
        const float fr = (float) (pos - i0);
        float y = d[(size_t) i0] + fr * (d[(size_t) i0 + 1] - d[(size_t) i0]);
        pos += inc; ++age;
        if (rel > 0) { env -= rel; if (env <= 0) { smp.reset(); return 0; } }
        return y * gain * env;
    }
private:
    std::shared_ptr<Sample> smp;
    double pos = 0, inc = 1, srHost = 44100;
    float gain = 1, env = 1, rel = 0;
};

// Plays real picked-bass notes; monophonic like a real player (new note chokes the last)
class BassSampler
{
public:
    void prepare (double s) { sr = s; for (auto& v : voices) v = BassVoice(); }
    void setBank (std::shared_ptr<const Bank> b) { std::atomic_store (&bank, b); }
    bool hasBank() const { return std::atomic_load (&bank) != nullptr; }

    // vel 0..1; lengthSec < 0 = hold until noteOff()
    void noteOn (int midi, float vel, float lengthSec, float detuneCents)
    {
        auto b = std::atomic_load (&bank);
        if (! b) return;
        for (auto& v : voices) if (v.active()) v.kill();
        const int sn = std::clamp (midi, kLowSample, kHighSample);
        const int idx = sn - kLowSample;
        int layer = vel > 0.62f ? 1 : 0;
        if (b->rrCount (idx, layer) == 0) layer = 1 - layer;
        const int n = b->rrCount (idx, layer);
        if (n == 0) return;
        int pick = rng.i (n);
        if (n > 1 && pick == lastRR[(size_t) layer]) pick = (pick + 1) % n;   // never the same take twice in a row
        lastRR[(size_t) layer] = pick;
        int k = 0; std::shared_ptr<Sample> smp;
        for (auto& x : b->s[(size_t) idx][(size_t) layer]) if (x && k++ == pick) { smp = x; break; }
        const double rate = std::pow (2.0, ((midi - sn) + detuneCents * 0.01) / 12.0);
        // velocity -> gain within the layer (layers already carry the tone change)
        const float g = (layer == 1 ? 0.55f + 0.45f * vel : 0.75f + 0.6f * vel) * 0.9f;
        auto& v = voices[(size_t) next]; next = (next + 1) % (int) voices.size();
        v.start (smp, rate, g, sr);
        held = &v; holdLeft = lengthSec < 0 ? -1 : (int) (lengthSec * sr);
        currentNote = midi;
    }
    void noteOff() { if (held) held->release (0.045f); held = nullptr; holdLeft = 0; currentNote = -1; }
    void allOff() { for (auto& v : voices) if (v.active()) v.kill(); held = nullptr; currentNote = -1; }

    inline float render() noexcept
    {
        if (held && holdLeft > 0 && --holdLeft == 0) noteOff();
        float y = 0;
        for (auto& v : voices) y += v.render();
        return y;
    }
    int currentNote = -1;
private:
    double sr = 44100;
    std::shared_ptr<const Bank> bank;
    std::array<BassVoice, 4> voices;
    BassVoice* held = nullptr;
    int holdLeft = 0, next = 0;
    std::array<int, 2> lastRR { -1, -1 };
    Rng rng;
};

// ===========================================================================
// Ears 1: chord detection on the guitar (fast chroma + triad templates)
// ===========================================================================
struct Chord { int root = -1; bool minor = false; bool valid() const { return root >= 0; } };

class ChordDetector
{
public:
    void prepare (double s)
    {
        sr = s;
        fft = std::make_unique<juce::dsp::FFT> (13);    // 4096 samples zero-padded to 8192
        ring.assign (4096, 0.0f); work.assign (16384, 0.0f); win.resize (4096);
        for (int i = 0; i < 4096; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos ((float) kTwoPi * (float) i / 4095.0f);
        hop = std::max (256, (int) std::lround (sr * 0.0116));
        w = 0; sinceHop = 0; prevEnergy = 0;
        std::fill (std::begin (chroma), std::end (chroma), 0.0f);
        std::fill (std::begin (bass), std::end (bass), 0.0f);
        cur = {}; cand = {}; candCount = 0; silentHops = 1000;
        keyChroma.fill (0);
    }

    void push (const float* x, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            ring[(size_t) w] = x[i]; w = (w + 1) & 4095;
            if (++sinceHop >= hop) { sinceHop = 0; analyse(); }
        }
    }

    Chord current() const { const int v = packed.load(); return v < 0 ? Chord{} : Chord { v & 15, (v >> 4) != 0 }; }
    bool playing() const { return silentHops < silentLimit; }
    std::atomic<float> level { 0 };
    std::atomic<int> packed { -1 };
    int silentLimit = 40;            // hops (~0.5 s); the processor sets this from the tempo
    std::atomic<int> changes { 0 };  // bumps every time the chord changes
    float dbgPeakF[40] {}, dbgPeakM[40] {}; int dbgN = 0;
    float dbgScores[24] {}; float dbgChroma[12] {}, dbgBass[12] {};

private:
    void analyse()
    {
        double e = 0;
        for (int i = 0; i < 4096; ++i)
        {
            const float v = ring[(size_t) ((w + i) & 4095)];
            work[(size_t) i] = v * win[(size_t) i];
            if (i >= 4096 - hop) e += v * v;
        }
        const float rms = (float) std::sqrt (e / hop);
        level = rms;
        if (rms < 0.004f) { ++silentHops; prevEnergy = rms; return; }
        silentHops = 0;
        const bool onset = rms > prevEnergy * 1.8f + 0.003f;   // a new strum: forget the old chord faster
        prevEnergy = 0.7f * prevEnergy + 0.3f * rms;
        std::fill (work.begin() + 4096, work.end(), 0.0f);
        fft->performFrequencyOnlyForwardTransform (work.data());

        // pick spectral peaks (real partials), find their exact frequency, and map those to notes;
        // this keeps leakage from low strings out of the neighbouring notes
        float c[12] {}, b[12] {};
        const int kLo = (int) (60.0 * 8192.0 / sr), kHi = std::min (4094, (int) (1400.0 * 8192.0 / sr));
        float mx = 0;
        for (int k = kLo; k <= kHi; ++k) mx = std::max (mx, work[(size_t) k]);
        if (mx <= 0) return;
        double lowestBass = 1e9; int bassPc = -1; dbgN = 0;
        for (int k = std::max (2, kLo); k <= kHi; ++k)
        {
            const float m0 = work[(size_t) k];
            if (m0 < 0.03f * mx || m0 < work[(size_t) k - 1] || m0 < work[(size_t) k + 1]) continue;
            const float la = std::log (work[(size_t) k - 1] + 1e-9f), lb = std::log (m0 + 1e-9f), lc = std::log (work[(size_t) k + 1] + 1e-9f);
            const float den = la - 2 * lb + lc;
            const double off = std::abs (den) > 1e-9f ? 0.5 * (la - lc) / den : 0.0;
            const double f = (k + off) * sr / 8192.0;
            const double note = 12.0 * std::log2 (f / 440.0) + 69.0;
            const double near = std::round (note);
            if (std::abs (note - near) > 0.4) continue;
            const int pc = ((int) near % 12 + 12) % 12;
            const float wgt = std::sqrt (m0 / mx) / (float) (1.0 + f / 900.0);
            c[pc] += wgt;
            if (dbgN < 40) { dbgPeakF[dbgN] = (float) f; dbgPeakM[dbgN++] = m0 / mx; }
            if (f < 330 && m0 > 0.05f * mx && f < lowestBass) { lowestBass = f; bassPc = pc; }
        }
        if (bassPc >= 0) b[bassPc] = 1.0f;
        normalise (c); normalise (b);
        const float a = onset ? 0.85f : 0.35f;
        for (int i = 0; i < 12; ++i)
        {
            chroma[i] += a * (c[i] - chroma[i]);
            bass[i] += a * (b[i] - bass[i]);
            keyChroma[(size_t) i] = keyChroma[(size_t) i] * 0.998f + c[i];
        }

        // score every major and minor triad
        float best = -1e9f; Chord bestC;
        float curScore = -1e9f;
        for (int r = 0; r < 12; ++r)
            for (int m = 0; m < 2; ++m)
            {
                const float s = score (r, m == 1);
                dbgScores[r * 2 + m] = s;
                if (s > best) { best = s; bestC = { r, m == 1 }; }
                if (cur.valid() && r == cur.root && (m == 1) == cur.minor) curScore = s;
            }
        std::copy (chroma, chroma + 12, dbgChroma); std::copy (bass, bass + 12, dbgBass);
        // power chords (no third heard): take the quality the song's key implies
        const float third = std::max (chroma[(bestC.root + 3) % 12], chroma[(bestC.root + 4) % 12]);
        if (third < 0.35f * chroma[bestC.root]) bestC.minor = keyImpliesMinor (bestC.root);

        if (! cur.valid()) { if (best > 0.2f) set (bestC); return; }
        if (bestC.root == cur.root && bestC.minor == cur.minor) { candCount = 0; return; }
        const bool sameRoot = bestC.root == cur.root;
        if (best > curScore + (sameRoot ? 0.12f : 0.05f))
        {
            if (bestC.root == cand.root && bestC.minor == cand.minor) ++candCount; else { cand = bestC; candCount = 1; }
            if (candCount >= (onset ? 1 : 3)) set (bestC);
        }
        else candCount = 0;
    }
    float score (int r, bool minor) const
    {
        const int t = (r + (minor ? 3 : 4)) % 12, f = (r + 7) % 12, wrongT = (r + (minor ? 4 : 3)) % 12;
        float other = 0;
        for (int i = 0; i < 12; ++i) if (i != r && i != t && i != f) other += chroma[i];
        return chroma[r] * 1.0f + chroma[t] * 0.85f + chroma[f] * 0.75f - chroma[wrongT] * 0.6f - other * 0.12f + bass[r] * 0.4f;
    }
    bool keyImpliesMinor (int r) const
    {
        // crude: compare how much of the minor vs major third the song has used
        return keyChroma[(size_t) ((r + 3) % 12)] > keyChroma[(size_t) ((r + 4) % 12)];
    }
    static void normalise (float* v)
    {
        float m = 0; for (int i = 0; i < 12; ++i) m = std::max (m, v[i]);
        if (m > 0) for (int i = 0; i < 12; ++i) v[i] /= m;
    }
    void set (Chord c) { cur = c; candCount = 0; packed = c.root | ((c.minor ? 1 : 0) << 4); ++changes; }

    double sr = 44100;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> ring, work, win;
    int w = 0, hop = 512, sinceHop = 0, candCount = 0, silentHops = 1000;
    float prevEnergy = 0, chroma[12] {}, bass[12] {};
    std::array<float, 12> keyChroma {};
    Chord cur, cand;
};

// ===========================================================================
// Ears 2: monophonic pitch tracker (YIN at half rate) for guitar -> bass notes
// ===========================================================================
class PitchTracker
{
public:
    void prepare (double s)
    {
        sr = s; dsr = sr / 2;
        W = (int) std::lround (dsr * 0.024);                      // ~24 ms window
        maxTau = std::min (W - 2, (int) (dsr / 70.0));            // down to ~70 Hz (guitar low E is 82)
        minTau = std::max (2, (int) (dsr / 1000.0));
        ring.assign (2048, 0.0f); d.assign ((size_t) maxTau + 2, 0.0f);
        lp1.set (Biquad::LP, sr, 1800, 0.707); lp2.set (Biquad::LP, sr, 1800, 0.707);
        hp.set (Biquad::HP, sr, 60, 0.707);
        fastEnv = slowEnv = 0; w = 0; decim = 0; sinceHop = 0;
        hopLen = std::max (64, (int) std::lround (dsr * 0.003));
        state = Idle; pitch = -1; stableCount = 0;
    }

    // feed audio; returns events through the callbacks set on the struct
    struct Event { int type = 0; int note = 0; float vel = 0; int offset = 0; };   // 1 on, 2 off
    template <typename Fn>
    void push (const float* x, int n, float sensitivity, Fn&& emit)
    {
        const float thr = 0.02f * std::pow (10.0f, -sensitivity * 1.6f);    // sensitivity 0..1 -> -0..-32 dB
        for (int i = 0; i < n; ++i)
        {
            const float v = hp.process (x[i]);
            const float a = std::abs (v);
            fastEnv += (a > fastEnv ? 0.25f : 0.0015f) * (a - fastEnv);
            // onset detection on short peak blocks: loudness of the last ~15 ms vs ~17-46 ms ago
            blockPeak = std::max (blockPeak, a);
            if (++blockPos >= 128)
            {
                peaks[(size_t) peakW] = blockPeak; peakW = (peakW + 1) % 16; blockPeak = 0; blockPos = 0;
                float fast = 0, slow = 0;
                for (int k = 1; k <= 16; ++k) { const float pk = peaks[(size_t) ((peakW - k + 32) % 16)]; if (k <= 5) fast = std::max (fast, pk); else slow = std::max (slow, pk); }
                if (state != Pending && fast > thr && fast > slow * 1.5f + thr * 0.2f && sinceOnset > (int) (sr * 0.05))
                {
                    state = Pending; pendingAge = 0; peak = fast; sinceOnset = 0;
                }
            }
            ++sinceOnset;
            if (state == Pending) { peak = std::max (peak, fastEnv); if (++pendingAge > (int) (sr * 0.08)) state = sounding >= 0 ? Holding : Idle; }
            if (sounding >= 0 && fastEnv < thr * 0.5f) { if (++quiet > (int) (sr * 0.03)) { emit (Event { 2, sounding, 0, i }); sounding = -1; state = Idle; } }
            else quiet = 0;

            const float y = lp2.process (lp1.process (v));
            if ((decim ^= 1) == 0) continue;
            ring[(size_t) w] = y; w = (w + 1) & 2047;
            if (++sinceHop >= hopLen)
            {
                sinceHop = 0;
                float ap = 1; const int p = yin (ap);
                if (p > 0 && ap < 0.22f && fastEnv > thr * 0.6f)
                {
                    const bool agrees = p == prevEstimate; prevEstimate = p;
                    if (state == Pending && agrees && pendingAge > (int) (sr * 0.012))   // let the window fill with the new note, and agree twice
                    {
                        if (sounding >= 0) emit (Event { 2, sounding, 0, i });
                        sounding = p; state = Holding; stableCount = 0;
                        emit (Event { 1, p, std::clamp (peak / (thr * 25.0f), 0.25f, 1.0f), i });
                    }
                    else if (state == Holding && sounding >= 0 && p != sounding)
                    {
                        // slide / hammer-on / new note without a clear attack
                        if (p == cand) ++stableCount; else { cand = p; stableCount = 1; }
                        if (stableCount >= 4)
                        {
                            emit (Event { 2, sounding, 0, i });
                            sounding = p; stableCount = 0;
                            emit (Event { 1, p, std::clamp (fastEnv / (thr * 25.0f), 0.2f, 0.8f), i });
                        }
                    }
                    else stableCount = 0;
                }
            }
        }
    }
    int sounding = -1;
    float envelope() const { return fastEnv; }

private:
    int yin (float& aperiodicity)
    {
        // difference function on the latest W samples (at half rate)
        const int start = (w - W - maxTau + 4096) & 2047;
        auto at = [&] (int k) { return ring[(size_t) ((start + k) & 2047)]; };
        d[0] = 1;
        float run = 0; int best = -1;
        for (int tau = 1; tau <= maxTau; ++tau)
        {
            float s = 0;
            for (int j = 0; j < W; ++j) { const float df = at (j) - at (j + tau); s += df * df; }
            run += s;
            d[(size_t) tau] = run > 0 ? s * (float) tau / run : 1.0f;
        }
        for (int tau = minTau; tau < maxTau; ++tau)
            if (d[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < maxTau && d[(size_t) tau + 1] < d[(size_t) tau]) ++tau;
                best = tau; break;
            }
        if (best < 0)
        {
            float m = 1e9f;
            for (int tau = minTau; tau < maxTau; ++tau) if (d[(size_t) tau] < m) { m = d[(size_t) tau]; best = tau; }
        }
        aperiodicity = d[(size_t) best];
        // parabolic interpolation
        double t = best;
        if (best > 1 && best < maxTau)
        {
            const float a = d[(size_t) best - 1], b = d[(size_t) best], c = d[(size_t) best + 1];
            const float den = a + c - 2 * b;
            if (std::abs (den) > 1e-9f) t += 0.5 * (a - c) / den;
        }
        const double f = dsr / t;
        if (f < 60 || f > 1100) return -1;
        return (int) std::lround (69.0 + 12.0 * std::log2 (f / 440.0));
    }

    enum { Idle, Pending, Holding };
    double sr = 44100, dsr = 22050;
    int W = 512, maxTau = 300, minTau = 20, w = 0, decim = 0, sinceHop = 0, hopLen = 64;
    int state = Idle, pendingAge = 0, sinceOnset = 1 << 20, quiet = 0, pitch = -1, cand = -1, stableCount = 0;
    float fastEnv = 0, slowEnv = 0, peak = 0, blockPeak = 0;
    std::array<float, 16> peaks {}; int peakW = 0, blockPos = 0, prevEstimate = -1;
    std::vector<float> ring, d;
    Biquad lp1, lp2, hp;
};

// Octave-down for the whole guitar (two-head rotating delay, like an octave pedal)
class OctaveDown
{
public:
    void prepare (double s)
    {
        sr = s; buf.assign ((size_t) (sr * 0.06), 0.0f); w = 0; phase = 0;
        hp.set (Biquad::HP, sr, 70, 0.7); lp.set (Biquad::LP, sr, 1600, 0.7); lp2.set (Biquad::LP, sr, 1600, 0.7);
    }
    inline float process (float x) noexcept
    {
        x = hp.process (x);
        const int size = (int) buf.size();
        buf[(size_t) w] = x;
        const double win = size - 4;
        phase += 0.5 / win; if (phase >= 1) phase -= 1;     // ratio 0.5
        float out = 0;
        for (int h = 0; h < 2; ++h)
        {
            double p = phase + 0.5 * h; if (p >= 1) p -= 1;
            const double dl = p * win + 2.0;
            double rp = w - dl; while (rp < 0) rp += size;
            const int i0 = (int) rp, i1 = (i0 + 1) % size;
            const float fr = (float) (rp - i0);
            out += (buf[(size_t) i0] + fr * (buf[(size_t) i1] - buf[(size_t) i0])) * (float) std::sin (kPi * p);
        }
        if (++w >= size) w = 0;
        return lp2.process (lp.process (out * 0.9f));
    }
private:
    double sr = 44100, phase = 0;
    std::vector<float> buf;
    int w = 0;
    Biquad hp, lp, lp2;
};

// ===========================================================================
// The rig: compressor -> grit -> chorus -> flanger -> amp EQ/cab
// ===========================================================================
struct RigParams
{
    float comp = 0.4f, grit = 0.2f;
    float chorusMix = 0.45f, chorusRate = 0.6f, chorusDepth = 0.5f;
    float flangerMix = 0, flangerRate = 0.15f, flangerFb = 0.5f;
    float bassDb = 2, midDb = 1, trebleDb = 2; bool bright = true;
    float outGain = 1;
};

class ModDelay
{
public:
    void prepare (double s, double maxSec) { sr = s; buf.assign ((size_t) (sr * maxSec) + 8, 0.0f); w = 0; }
    inline void write (float x) noexcept { buf[(size_t) w] = x; if (++w >= (int) buf.size()) w = 0; }
    inline float read (double delaySamples) const noexcept
    {
        const int size = (int) buf.size();
        double rp = w - 1 - delaySamples; while (rp < 0) rp += size;
        const int i0 = (int) rp, i1 = (i0 + 1) % size;
        const float fr = (float) (rp - i0);
        return buf[(size_t) i0] + fr * (buf[(size_t) i1] - buf[(size_t) i0]);
    }
private:
    double sr = 44100; std::vector<float> buf; int w = 0;
};

class Rig
{
public:
    void prepare (double s)
    {
        sr = s;
        chL.prepare (sr, 0.04); chR.prepare (sr, 0.04); fl.prepare (sr, 0.02);
        split.set (Biquad::LP, sr, 180, 0.707); split2.set (Biquad::LP, sr, 180, 0.707);
        gritLow.set (Biquad::LP, sr, 140, 0.707); gritPre.set (Biquad::Peak, sr, 900, 0.8, 9); gritPost.set (Biquad::LP, sr, 4500, 0.7);
        dcBlock.set (Biquad::HP, sr, 25, 0.707);
        env = 0; ph1 = ph2 = phF = 0; flFb = 0; lastP = {}; lastP.bassDb = -99; update ({});
    }
    void update (const RigParams& p)
    {
        if (p.bassDb != lastP.bassDb || p.midDb != lastP.midDb || p.trebleDb != lastP.trebleDb || p.bright != lastP.bright)
        {
            for (int c = 0; c < 2; ++c)
            {
                eqLow[c].set (Biquad::LowShelf, sr, 90, 0.7, p.bassDb);
                eqMid[c].set (Biquad::Peak, sr, 750, 0.9, p.midDb);
                eqHigh[c].set (Biquad::HighShelf, sr, 2400, 0.7, p.trebleDb);
                cab1[c].set (Biquad::LP, sr, p.bright ? 7500 : 4200, 0.75);
                cab2[c].set (Biquad::Peak, sr, p.bright ? 2200 : 1400, 1.2, p.bright ? 3.0 : 1.5);
            }
        }
        lastP = p;
    }
    void process (const float* in, float* L, float* R, int n)
    {
        const auto& p = lastP;
        const float compRatio = 1.0f + p.comp * 7.0f, compThr = 0.25f - p.comp * 0.17f;
        const float atk = 1.0f - std::exp (-1.0f / (float) (sr * 0.004)), relC = 1.0f - std::exp (-1.0f / (float) (sr * 0.15));
        const float drive = 1.0f + p.grit * p.grit * 40.0f;
        const double cInc = (0.15 + p.chorusRate * 2.5) / sr, fInc = (0.03 + p.flangerRate * 1.2) / sr;
        const double cBase = sr * 0.009, cDepth = sr * 0.0055 * p.chorusDepth;
        const double fBase = sr * 0.0008, fDepth = sr * 0.0045;
        for (int i = 0; i < n; ++i)
        {
            float x = in[i];
            // compressor (peak follower, soft knee-ish)
            const float a = std::abs (x);
            env += (a > env ? atk : relC) * (a - env);
            float g = 1;
            if (env > compThr) g = std::pow (compThr / env, 1.0f - 1.0f / compRatio);
            x *= g * (1.0f + p.comp * 1.6f);
            // grit: lows stay clean, mids get driven (a bass player's distortion)
            if (p.grit > 0.001f)
            {
                const float low = gritLow.process (x);
                float hi = gritPre.process (x - low);
                hi = std::tanh (hi * drive) / std::sqrt (drive) * 1.6f;
                hi = gritPost.process (hi);
                x = low + (x - low) * (1.0f - p.grit) + hi * p.grit;
            }
            x = dcBlock.process (x);
            // chorus on everything above ~180 Hz; the bottom stays mono and solid
            const float lowPart = split2.process (split.process (x));
            const float top = x - lowPart;
            chL.write (top); chR.write (top);
            ph1 += cInc; if (ph1 >= 1) ph1 -= 1;
            const float wl = chL.read (cBase + cDepth * (0.5 + 0.5 * std::sin (kTwoPi * ph1)));
            const float wr = chR.read (cBase + cDepth * (0.5 + 0.5 * std::sin (kTwoPi * ph1 + kPi * 0.5)));
            float l = lowPart + top * (1.0f - 0.5f * p.chorusMix) + wl * p.chorusMix;
            float r = lowPart + top * (1.0f - 0.5f * p.chorusMix) + wr * p.chorusMix;
            // flanger (jet sweep, both sides)
            if (p.flangerMix > 0.001f)
            {
                const float m = 0.5f * (l + r);
                fl.write (m + flFb * p.flangerFb * 0.9f);
                phF += fInc; if (phF >= 1) phF -= 1;
                const double tri = phF < 0.5 ? phF * 2 : 2 - phF * 2;
                const float wv = fl.read (fBase + fDepth * tri * tri);
                flFb = wv;
                l = l * (1.0f - 0.5f * p.flangerMix) + wv * p.flangerMix * 0.9f;
                r = r * (1.0f - 0.5f * p.flangerMix) + wv * p.flangerMix * 0.9f;
            }
            // amp EQ + cabinet
            l = cab2[0].process (cab1[0].process (eqHigh[0].process (eqMid[0].process (eqLow[0].process (l))))) * p.outGain;
            r = cab2[1].process (cab1[1].process (eqHigh[1].process (eqMid[1].process (eqLow[1].process (r))))) * p.outGain;
            L[i] = l; R[i] = r;
        }
    }
private:
    double sr = 44100, ph1 = 0, ph2 = 0, phF = 0;
    float env = 0, flFb = 0;
    ModDelay chL, chR, fl;
    Biquad split, split2, gritLow, gritPre, gritPost, dcBlock;
    Biquad eqLow[2], eqMid[2], eqHigh[2], cab1[2], cab2[2];
    RigParams lastP;
};
} // namespace ut
