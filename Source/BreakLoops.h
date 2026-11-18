#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "BandBus.h"
#include <vector>
#include <algorithm>

// ---------------------------------------------------------------------------
// The drum loop kit: recorded breaks, chopped into sixteenths and played back
// on Live's grid at any tempo (each slice keeps its pitch; it's the order and
// the timing that follow the song). Fills stutter and flip the last beat.
// ---------------------------------------------------------------------------
namespace tt
{
struct Break { juce::String name; int beats = 4; double sr = 44100; juce::AudioBuffer<float> d; };

class BreakPlayer
{
public:
    std::vector<Break> breaks;

    void load (const char* (*getRes) (const char*, int&), const char* const* names, int count)
    {
        juce::FlacAudioFormat flac;
        for (int r = 0; r < count; ++r)
        {
            const juce::String res (names[r]);
            if (! res.startsWith ("brk_")) continue;
            int size = 0; const char* data = getRes (res.toRawUTF8(), size);
            if (data == nullptr) continue;
            std::unique_ptr<juce::AudioFormatReader> rd (flac.createReaderFor (new juce::MemoryInputStream (data, (size_t) size, false), true));
            if (rd == nullptr) continue;
            Break b;
            juce::StringArray t; t.addTokens (res, "_", "");
            b.beats = t[2].getIntValue();
            b.name = prettyName (t[1]);
            b.sr = rd->sampleRate;
            b.d.setSize (2, (int) rd->lengthInSamples);
            rd->read (&b.d, 0, (int) rd->lengthInSamples, 0, true, true);
            if (rd->numChannels == 1) b.d.copyFrom (1, 0, b.d, 0, 0, b.d.getNumSamples());
            if (b.beats > 0) breaks.push_back (std::move (b));
        }
        std::sort (breaks.begin(), breaks.end(), [] (const Break& a, const Break& b) { return a.name.compare (b.name) < 0; });
    }
    static juce::String prettyName (const juce::String& s)
    {
        static const char* m[][2] = { { "hotbreak", "Hot Break (4 bars)" }, { "shortbreak", "Short Break" }, { "streetbreak", "Street Break" },
                                      { "safarihands", "Safari Hands (4 bars)" }, { "tablagroove", "Tabla Groove (4 bars)" }, { "kitchenperc", "Kitchen Percussion" },
                                      { "kitchenperc2", "Kitchen Percussion 2" }, { "brightbreak", "Bright Break (4 bars)" }, { "heavybreak", "Heavy Break (4 bars)" } };
        for (auto& p : m) if (s == p[0]) return p[1];
        return s;
    }

    void prepare (double s) { sr = s; for (auto& v : v2) v.on = false; lastK = -1; }

    struct Ctx { bool playing; double ppq, bpm; int meter; bool silent; bool fill; float gain; };
    // adds the loop into out (stereo) for this block
    void process (juce::AudioBuffer<float>& out, int n, int which, const Ctx& c)
    {
        if (which < 0 || which >= (int) breaks.size() || ! c.playing) { for (auto& v : v2) v.on = false; lastK = -1; return; }
        const Break& b = breaks[(size_t) which];
        const int loopSteps = b.beats * 4;
        const double sliceLen = (double) b.d.getNumSamples() / loopSteps;
        const double ppqPerSample = c.bpm / 60.0 / sr;
        const int ns = band::meterSteps (c.meter);
        // sixteenths that start in this block
        for (int64_t k = (int64_t) std::ceil (c.ppq * 4.0 - 1e-9); (double) k * 0.25 < c.ppq + ppqPerSample * n; ++k)
        {
            if (k == lastK) continue;
            lastK = k;
            const int step = (int) (((k % ns) + ns) % ns);
            const int64_t bar = (k - step) / ns;
            int off = (int) (((double) k * 0.25 - c.ppq) / ppqPerSample);
            off = juce::jlimit (0, n - 1, off + (int) (rnd.nextFloat() * 0.002 * sr));
            if (c.silent) { fadeAll (off); continue; }
            int src = band::oddSrc (c.meter, step) & 15;
            bool rev = false;
            if (c.fill && step >= ns - 4)
            {
                // the fill: stutter the beat before, or play it backwards
                if (step == ns - 4) fillKind = rnd.nextInt (3);
                if (fillKind == 0) src = 12 + ((step - (ns - 4)) % 2);
                else if (fillKind == 1) src = 8;
                else { src = 15 - (step - (ns - 4)); rev = true; }
            }
            const int slice = (int) ((bar * 16 + src) % loopSteps);
            fadeAll (off);
            auto& v = v2[next]; next ^= 1;
            v.on = true; v.rev = rev; v.start = slice * sliceLen; v.len = sliceLen; v.pos = 0; v.wait = off; v.fade = 1;
            v.gain = c.gain * (1.0f + (rnd.nextFloat() - 0.5f) * 0.1f);
            v.fadeAt = -1;
        }
        const double rate = b.sr / sr;
        const float* L = b.d.getReadPointer (0); const float* R = b.d.getReadPointer (1);
        const int N = b.d.getNumSamples();
        float* oL = out.getWritePointer (0); float* oR = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;
        const float release = (float) std::exp (-1.0 / (0.004 * sr));
        for (auto& v : v2)
        {
            if (! v.on) continue;
            for (int i = 0; i < n; ++i)
            {
                if (v.wait > 0) { --v.wait; continue; }
                if (v.fadeAt >= 0 && i >= v.fadeAt) v.fade *= release;
                if (v.pos >= v.len) { v.on = false; break; }
                const double p = v.rev ? v.start + v.len - 1 - v.pos : v.start + v.pos;
                const int i0 = juce::jlimit (0, N - 2, (int) p); const float fr = (float) (p - (double) i0);
                // a soft edge at the slice's start and end, so nothing clicks
                const float edge = (float) juce::jmin (1.0, juce::jmin (v.pos, v.len - v.pos) / (0.0015 * sr));
                const float g = v.gain * v.fade * edge;
                oL[i] += (L[i0] + (L[i0 + 1] - L[i0]) * fr) * g;
                if (oR) oR[i] += (R[i0] + (R[i0 + 1] - R[i0]) * fr) * g;
                v.pos += rate;
                if (v.fade < 1.0e-4f) { v.on = false; break; }
            }
            if (v.on && v.fadeAt >= 0) v.fadeAt = 0;     // keep fading in the next block
        }
    }

private:
    struct V { bool on = false, rev = false; double start = 0, len = 0, pos = 0; int wait = 0; float gain = 1, fade = 1; int fadeAt = -1; };
    V v2[2]; int next = 0, fillKind = 0;
    int64_t lastK = -1;
    double sr = 44100;
    juce::Random rnd { 4321 };
    void fadeAll (int at) { for (auto& v : v2) if (v.on && v.fadeAt < 0 && v.wait < at) v.fadeAt = at; }
};
} // namespace tt
