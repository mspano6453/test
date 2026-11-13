#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <vector>

// ---------------------------------------------------------------------------
// Jangle's sound: real guitars, one sampled note per string, played back like
// six strings (a new note on a string stops the last one, just like a finger
// moving). Then an amp, tremolo, chorus and a room.
//   Steel acoustic: Jeff Learman's Martin (CC0). Clean electric and palm mutes:
//   Karoryfer "Black And Green Guitars" (CC0). Hollow-body and dead-string
//   scratches: Karoryfer "Shinyguitar" (CC0).
// ---------------------------------------------------------------------------
namespace jg
{
constexpr double kTwoPi = 6.283185307179586;
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

enum Guitar { GAcoustic = 0, GClean, GTwelve, GHollow, GCrunch, GFuzz, GMuted, NumGuitars };
inline const char* guitarName (int g)
{
    static const char* n[] = { "Steel Acoustic", "Clean Electric", "12-String Jangle", "Hollow Body", "Crunch", "Fuzz", "Palm-Muted" };
    return (g >= 0 && g < NumGuitars) ? n[g] : "";
}
// sample sets
enum SetId { SetAcoustic = 0, SetClean, SetMute, SetHollow, SetScratch, NumSets };
inline int setFor (int g) { return g == GAcoustic ? SetAcoustic : g == GHollow ? SetHollow : g == GMuted ? SetMute : SetClean; }

struct Zone { int root = 60, layer = 1; std::vector<float> d; double sr = 44100; };
struct SampleSet
{
    std::vector<Zone> zones;
    const Zone* pick (int note, float vel, Rng& r) const
    {
        const Zone* best = nullptr; int bd = 999;
        const int want = vel > 0.62f ? 2 : 1;
        for (auto& z : zones)
        {
            const int d = std::abs (z.root - note) * 4 + (z.layer == want ? 0 : 1) + (r.chance (0.5f) ? 0 : 0);
            if (d < bd) { bd = d; best = &z; }
        }
        return best;
    }
};
struct Library { SampleSet sets[NumSets]; };

// one string
class Voice
{
public:
    void prepare (double s) { sr = s; }
    bool active() const { return z != nullptr; }
    void start (const Zone* zone, int midiNote, float vel, float lenSec, float panIn, float detuneCents, double brightness)
    {
        z = zone; note = midiNote;
        if (z == nullptr) return;
        inc = std::pow (2.0, (midiNote - z->root + detuneCents / 100.0) / 12.0) * z->sr / sr;
        pos = 0; env = 0; att = (float) (1.0 / (0.0015 * sr));
        gain = vel * (0.35f + 0.65f * vel);
        relRate = 0; holdLeft = lenSec > 0 ? (int64_t) (lenSec * sr) : -1;
        pan = panIn; age = 0; releasing = false;
        lp = 0; lpK = (float) juce::jlimit (0.05, 1.0, brightness);
    }
    // a finger lifts (or the string is stopped by the next note): a quick fade
    void choke (float sec = 0.03f) { if (z != nullptr && ! releasing) { releasing = true; relRate = (float) (1.0 / juce::jmax (1.0, sec * sr)); } }
    void kill() { z = nullptr; }
    void render (float* L, float* R, int n)
    {
        if (z == nullptr) return;
        const auto& d = z->d;
        const int len = (int) d.size() - 2;
        const float gl = std::sqrt (0.5f * (1.0f - pan)), gr = std::sqrt (0.5f * (1.0f + pan));
        for (int i = 0; i < n; ++i)
        {
            const int ip = (int) pos;
            if (ip >= len) { z = nullptr; return; }
            const float fr = (float) (pos - ip);
            float x = d[(size_t) ip] + (d[(size_t) ip + 1] - d[(size_t) ip]) * fr;
            lp += lpK * (x - lp); x = lp;
            if (env < 1.0f && ! releasing) env = std::min (1.0f, env + att);
            if (holdLeft > 0 && --holdLeft == 0) choke (0.06f);
            if (releasing) { env -= relRate; if (env <= 0.0f) { z = nullptr; return; } }
            const float v = x * env * gain;
            L[i] += v * gl; R[i] += v * gr;
            pos += inc;
        }
        age += n;
    }
    int note = -1, string = -1; int64_t age = 0;
private:
    const Zone* z = nullptr;
    double sr = 44100, pos = 0, inc = 1;
    float env = 0, att = 0.01f, gain = 1, relRate = 0, pan = 0, lp = 0, lpK = 1;
    int64_t holdLeft = -1;
    bool releasing = false;
};

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void set (double sr, double f, double q, double gainDb, int type)   // 0 lp, 1 hp, 2 peak, 3 low shelf
    {
        const double w = kTwoPi * juce::jlimit (20.0, sr * 0.45, f) / sr, c = std::cos (w), s = std::sin (w), al = s / (2 * q), A = std::pow (10.0, gainDb / 40.0);
        double B0, B1, B2, A0, A1, A2;
        if (type == 0)      { B0 = (1 - c) / 2; B1 = 1 - c; B2 = (1 - c) / 2; A0 = 1 + al; A1 = -2 * c; A2 = 1 - al; }
        else if (type == 1) { B0 = (1 + c) / 2; B1 = -(1 + c); B2 = (1 + c) / 2; A0 = 1 + al; A1 = -2 * c; A2 = 1 - al; }
        else if (type == 2) { B0 = 1 + al * A; B1 = -2 * c; B2 = 1 - al * A; A0 = 1 + al / A; A1 = -2 * c; A2 = 1 - al / A; }
        else { const double sq = 2 * std::sqrt (A) * al; B0 = A * ((A + 1) - (A - 1) * c + sq); B1 = 2 * A * ((A - 1) - (A + 1) * c); B2 = A * ((A + 1) - (A - 1) * c - sq);
               A0 = (A + 1) + (A - 1) * c + sq; A1 = -2 * ((A - 1) + (A + 1) * c); A2 = (A + 1) + (A - 1) * c - sq; }
        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0); a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }
    float process (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void reset() { z1 = z2 = 0; }
};

// The amp, the pedals and the room
class Rig
{
public:
    struct Params { int guitar = 0; float drive = 0.3f, tone = 0.5f, trem = 0, tremRate = 4, chorus = 0, room = 0.25f, width = 0.6f, out = 1; };
    void prepare (double s, int maxBlock)
    {
        sr = s;
        for (auto& d : dl) d.assign ((size_t) (0.05 * sr) + 8, 0.0f);
        w = 0;
        juce::Reverb::Parameters rp; rp.roomSize = 0.5f; rp.damping = 0.5f; rp.width = 1.0f; rp.wetLevel = 1.0f; rp.dryLevel = 0.0f;
        reverb.setParameters (rp); reverb.setSampleRate (sr); reverb.reset();
        wl.assign ((size_t) maxBlock, 0.0f); wr.assign ((size_t) maxBlock, 0.0f);
        lastGuitar = -1;
    }
    void set (const Params& np)
    {
        p = np;
        if (p.guitar != lastGuitar || std::abs (p.tone - lastTone) > 0.01f)
        {
            lastGuitar = p.guitar; lastTone = p.tone;
            const double t = p.tone;
            for (int c = 0; c < 2; ++c)
            {
                if (p.guitar == GAcoustic)
                {
                    pre[c].set (sr, 110, 0.7, 0, 1);
                    body[c].set (sr, 220, 1.0, 2.0, 2);
                    cab[c].set (sr, 6000 + 9000 * t, 0.7, 0, 0);
                }
                else
                {
                    const bool dirty = p.guitar == GCrunch || p.guitar == GFuzz;
                    pre[c].set (sr, dirty ? 140 : 90, 0.7, 0, 1);
                    body[c].set (sr, 800 + 1400 * t, 0.9, dirty ? 3.0 : 1.5, 2);
                    cab[c].set (sr, (dirty ? 3200 : 4500) + 4000 * t, 0.8, 0, 0);
                }
            }
        }
    }
    void process (float* L, float* R, int n, double ppq, double bpm, bool playing)
    {
        const bool dirty = p.guitar == GCrunch || p.guitar == GFuzz;
        const float preGain = p.guitar == GFuzz ? 12.0f + 40.0f * p.drive : p.guitar == GCrunch ? 3.0f + 12.0f * p.drive
                            : p.guitar == GAcoustic ? 1.0f : 1.0f + 2.0f * p.drive;
        const float makeUp = p.guitar == GFuzz ? 0.22f : p.guitar == GCrunch ? 0.34f : 1.0f;
        const double sec = 60.0 / juce::jmax (30.0, bpm);
        const float chorusDepth = (p.guitar == GTwelve ? juce::jmax (0.35f, p.chorus) : p.chorus);
        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R[i];
            l = pre[0].process (l); r = pre[1].process (r);
            if (p.guitar != GAcoustic)
            {
                if (dirty) { l = std::tanh (l * preGain) * makeUp; r = std::tanh (r * preGain) * makeUp; if (p.guitar == GFuzz) { l = juce::jlimit (-0.3f, 0.3f, l * 1.6f); r = juce::jlimit (-0.3f, 0.3f, r * 1.6f); } }
                else { l = std::tanh (l * preGain) / std::max (1.0f, preGain * 0.8f); r = std::tanh (r * preGain) / std::max (1.0f, preGain * 0.8f); }
            }
            l = body[0].process (l); r = body[1].process (r);
            l = cab[0].process (l); r = cab[1].process (r);
            // tremolo: in time with the song when it's playing (eighth notes), else free
            if (p.trem > 0.001f)
            {
                const double ph = playing ? (ppq + i * bpm / 60.0 / sr) * 2.0 : (tremPh += p.tremRate / sr);
                const float g = 1.0f - p.trem * (0.5f + 0.5f * (float) std::sin (kTwoPi * ph));
                l *= g; r *= g;
            }
            // chorus / the 12-string's shimmer
            dl[0][(size_t) w] = l; dl[1][(size_t) w] = r;
            if (chorusDepth > 0.001f)
            {
                chPh += 0.7 / sr; if (chPh > 1) chPh -= 1;
                for (int c = 0; c < 2; ++c)
                {
                    const double dms = 7.0 + 3.0 * std::sin (kTwoPi * (chPh + c * 0.25));
                    double rp = w - dms * 0.001 * sr; const int sz = (int) dl[(size_t) c].size();
                    while (rp < 0) rp += sz;
                    const int a = (int) rp; const float f = (float) (rp - a);
                    const float v = dl[(size_t) c][(size_t) a] + (dl[(size_t) c][(size_t) ((a + 1) % sz)] - dl[(size_t) c][(size_t) a]) * f;
                    (c == 0 ? l : r) += v * chorusDepth * 0.8f;
                }
            }
            // width: a short delay on one side
            {
                const int sz = (int) dl[1].size();
                const int a = (w - (int) (0.011 * sr * p.width) + sz) % sz;
                r = r * (1.0f - 0.4f * p.width) + dl[0][(size_t) a] * 0.4f * p.width;
            }
            w = (w + 1) % (int) dl[0].size();
            L[i] = l; R[i] = r;
        }
        juce::ignoreUnused (sec);
        // the room
        if (p.room > 0.001f && (int) wl.size() >= n)
        {
            std::copy (L, L + n, wl.begin()); std::copy (R, R + n, wr.begin());
            reverb.processStereo (wl.data(), wr.data(), n);
            for (int i = 0; i < n; ++i) { L[i] += wl[(size_t) i] * p.room * 0.45f; R[i] += wr[(size_t) i] * p.room * 0.45f; }
        }
        for (int i = 0; i < n; ++i) { L[i] *= p.out; R[i] *= p.out; }
    }
private:
    double sr = 44100, tremPh = 0, chPh = 0;
    Params p;
    int lastGuitar = -1; float lastTone = -1;
    Biquad pre[2], body[2], cab[2];
    std::vector<float> dl[2]; int w = 0;
    juce::Reverb reverb;
    std::vector<float> wl, wr;
};
} // namespace jg
