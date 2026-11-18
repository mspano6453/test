#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <vector>
#include <cmath>

// ---------------------------------------------------------------------------
// Found sounds, the way the 60s studios used them: a crow, a till, a door, a
// toy train whistle, a siren whistle, a rubbed wine glass, the sea, a clock
// ticking, a record spun back, and a radio dial being turned. Now and then on
// their own, and as a tape collage in the gap before a new section.
// ---------------------------------------------------------------------------
namespace ln
{
class FoundSounds
{
public:
    enum Kind { KCrow = 0, KTill, KDoor, KRewind, KBackspin, KWhistle, KSiren, KGlass, KTick, KSea, NumKinds };

    void load (const char* (*getRes) (const char*, int&), const char* const* names, int count)
    {
        static const char* stems[NumKinds] = { "crow", "till", "door", "rewind", "backspin", "whistle", "siren", "glass", "tick", "sea" };
        juce::FlacAudioFormat flac;
        for (int r = 0; r < count; ++r)
        {
            const juce::String res (names[r]);
            if (! res.startsWith ("found_")) continue;
            const juce::String stem = res.fromFirstOccurrenceOf ("_", false, false).upToFirstOccurrenceOf ("_", false, false).retainCharacters ("abcdefghijklmnopqrstuvwxyz");
            int kind = -1; for (int k = 0; k < NumKinds; ++k) if (stem == stems[k]) kind = k;
            if (kind < 0) continue;
            int size = 0; const char* data = getRes (res.toRawUTF8(), size);
            if (data == nullptr) continue;
            std::unique_ptr<juce::AudioFormatReader> rd (flac.createReaderFor (new juce::MemoryInputStream (data, (size_t) size, false), true));
            if (rd == nullptr) continue;
            Clip c; c.sr = rd->sampleRate;
            juce::AudioBuffer<float> t ((int) rd->numChannels, (int) rd->lengthInSamples);
            rd->read (&t, 0, t.getNumSamples(), 0, true, true);
            c.d.assign (t.getReadPointer (0), t.getReadPointer (0) + t.getNumSamples());
            c.d.push_back (0); c.d.push_back (0);
            clips[kind].push_back (std::move (c));
        }
    }
    bool loaded() const { for (auto& k : clips) if (! k.empty()) return true; return false; }

    void prepare (double s) { sr = s; for (auto& v : voices) v.on = false; radioLeft = 0; clockLeft = 0; seaGain = 0; untilNext = (int64_t) (sr * 6); }

    struct Params { float amount = 0, radio = 0, collage = 0, sea = 0, clocks = 0, level = 0.7f; };

    // the tape collage: called once, half a bar before a new section (len = seconds to the downbeat)
    void collage (double len, const Params& p)
    {
        if (p.collage <= 0.001f) return;
        const int count = 2 + rnd.nextInt (2);
        for (int i = 0; i < count; ++i)
        {
            static const int pick[] = { KCrow, KTill, KDoor, KRewind, KBackspin, KWhistle, KSiren, KGlass };
            start (pick[rnd.nextInt (8)], 0.5 + rnd.nextDouble() * 1.1, rnd.nextFloat() < 0.4f, p.collage * (0.55f + 0.3f * rnd.nextFloat()),
                   (int) (rnd.nextDouble() * len * 0.4 * sr), len);
        }
        if (rnd.nextFloat() < 0.6f) { radioLeft = (int64_t) (len * sr); radioLen = radioLeft; radioGain = p.collage * 0.5f; }
    }

    void process (float* L, float* R, int n, const Params& p)
    {
        // now and then, on its own
        if (p.amount > 0.001f)
        {
            untilNext -= n;
            if (untilNext <= 0)
            {
                untilNext = (int64_t) ((4.0 + (1.0 - p.amount) * 30.0) * (0.5 + rnd.nextDouble()) * sr);
                const int k = rnd.nextInt (8);
                if (k == KRewind || k == KBackspin) start (k, 1.0, false, p.amount * 0.5f, 0, 0);
                else start (k, 0.85 + rnd.nextDouble() * 0.3, false, p.amount * (0.35f + 0.3f * rnd.nextFloat()), 0, 0);
            }
        }
        // the radio dial, turned now and then
        if (p.radio > 0.001f && radioLeft <= 0 && rnd.nextDouble() < (double) n / (sr * (6.0 + 30.0 * (1.0 - p.radio))))
        { radioLeft = (int64_t) ((1.5 + 3.0 * rnd.nextDouble()) * sr); radioLen = radioLeft; radioGain = p.radio * 0.4f; }
        // the clock
        if (p.clocks > 0.001f && clockLeft <= 0 && rnd.nextDouble() < (double) n / (sr * (8.0 + 30.0 * (1.0 - p.clocks))))
        { clockLeft = (int64_t) ((4.0 + 6.0 * rnd.nextDouble()) * sr); clockGain = p.clocks * 0.6f; tickIn = 0; tock = false; }

        for (int i = 0; i < n; ++i)
        {
            float l = 0, r = 0;
            for (auto& v : voices)
            {
                if (! v.on) continue;
                if (v.wait > 0) { --v.wait; continue; }
                const auto& d = v.clip->d;
                const double len = (double) d.size() - 2;
                if (v.pos >= len || (v.maxLen > 0 && v.t >= v.maxLen)) { v.on = false; continue; }
                const double pp = v.rev ? len - 1 - v.pos : v.pos;
                const size_t i0 = (size_t) juce::jmax (0.0, pp); const float fr = (float) (pp - (double) i0);
                float x = d[i0] + (d[i0 + 1] - d[i0]) * fr;
                // fade in a touch, and out near the end (or the cut-off)
                const double left = v.maxLen > 0 ? juce::jmin ((len - v.pos) / v.rate, (double) (v.maxLen - v.t)) : (len - v.pos) / v.rate;
                x *= (float) juce::jmin (1.0, juce::jmin ((double) v.t / (0.005 * sr), left / (0.08 * sr)));
                l += x * v.gain * (1.0f - v.pan); r += x * v.gain * (1.0f + v.pan);
                v.pos += v.rate * v.clip->sr / sr; ++v.t;
            }
            if (radioLeft > 0)
            {
                // between stations: hiss through a sweeping band, a whistle sliding past, crackle
                const double ph = 1.0 - (double) radioLeft / (double) juce::jmax ((int64_t) 1, radioLen);
                const float env = (float) juce::jmin (1.0, juce::jmin (ph * 8.0, (1.0 - ph) * 8.0));
                const double f = 400.0 + 2600.0 * (0.5 + 0.5 * std::sin (ph * 5.3 + sweepSeed));
                bp.set (sr, f, 3.0);
                float x = bp.process (rnd.nextFloat() * 2.0f - 1.0f) * 0.8f;
                whistlePh += (900.0 + 1800.0 * std::sin (ph * 3.1 + sweepSeed * 2.0)) / sr; whistlePh -= std::floor (whistlePh);
                x += (float) std::sin (6.283185307 * whistlePh) * 0.12f * (float) (0.5 + 0.5 * std::sin (ph * 40.0));
                if (rnd.nextFloat() < 0.0006f) x += (rnd.nextFloat() - 0.5f) * 1.5f;
                x = std::tanh (x * 1.5f) * env * radioGain;
                l += x; r += x;
                if (--radioLeft == 0) sweepSeed = rnd.nextDouble() * 6.0;
            }
            if (clockLeft > 0)
            {
                if (--tickIn <= 0 && ! clips[KTick].empty())
                {
                    tickIn = (int) (sr * (0.5 + (rnd.nextFloat() - 0.5f) * 0.004f));
                    start (KTick, tock ? 0.88 : 1.0, false, clockGain * (tock ? 0.8f : 1.0f), 0, 0, 0.15f);
                    tock = ! tock;
                }
                --clockLeft;
            }
            L[i] += l * p.level; R[i] += r * p.level;
        }
        // the sea: one long wash under everything, slowly breathing
        if (! clips[KSea].empty())
        {
            const float want = p.sea;
            for (int i = 0; i < n; ++i)
            {
                seaGain += (want - seaGain) * 0.00002f;
                if (seaGain < 0.0005f) continue;
                const auto& d = clips[KSea][(size_t) seaClip].d;
                const size_t len = d.size() - 2;
                const float edge = (float) juce::jmin (1.0, juce::jmin (seaPos / (1.5 * sr), ((double) len - seaPos) / (1.5 * sr)));
                const float x = d[(size_t) seaPos] * edge * seaGain * 0.6f * p.level;
                L[i] += x; R[i] += x * 0.9f;
                seaPos += clips[KSea][(size_t) seaClip].sr / sr;
                if (seaPos >= (double) len - 1) { seaPos = 0; seaClip = (seaClip + 1) % (int) clips[KSea].size(); }
            }
        }
    }

private:
    struct Clip { std::vector<float> d; double sr = 44100; };
    struct Voice { bool on = false, rev = false; const Clip* clip = nullptr; double pos = 0, rate = 1; float gain = 0, pan = 0; int wait = 0; int64_t t = 0, maxLen = 0; };
    std::vector<Clip> clips[NumKinds];
    Voice voices[10];
    double sr = 44100;
    juce::Random rnd { 2468 };
    int64_t untilNext = 0, radioLeft = 0, radioLen = 1, clockLeft = 0;
    float radioGain = 0, clockGain = 0, seaGain = 0;
    double whistlePh = 0, sweepSeed = 1.0, seaPos = 0; int seaClip = 0, tickIn = 0; bool tock = false;
    struct BP { float b0 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void set (double s, double f, double q) { const double w = 6.283185307 * f / s, a = std::sin (w) / (2 * q), c = std::cos (w), nn = 1 + a; b0 = (float) (a / nn); b2 = (float) (-a / nn); a1 = (float) (-2 * c / nn); a2 = (float) ((1 - a) / nn); }
        float process (float x) { const float y = b0 * x + z1; z1 = -a1 * y + z2; z2 = b2 * x - a2 * y; return y; } } bp;

    void start (int kind, double rate, bool rev, float gain, int wait, double maxSec, float panAmt = 0.7f)
    {
        if (clips[kind].empty()) return;
        Voice* v = nullptr;
        for (auto& x : voices) if (! x.on) { v = &x; break; }
        if (v == nullptr) return;
        v->on = true; v->clip = &clips[kind][(size_t) rnd.nextInt ((int) clips[kind].size())];
        v->rate = rate; v->rev = rev; v->gain = gain; v->pan = (rnd.nextFloat() * 2.0f - 1.0f) * panAmt;
        v->pos = 0; v->t = 0; v->wait = wait; v->maxLen = maxSec > 0 ? (int64_t) (maxSec * sr) - wait : 0;
    }
};
} // namespace ln
