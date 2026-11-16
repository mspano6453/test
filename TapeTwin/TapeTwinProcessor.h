#pragma once
#include "../Source/SongForm.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/BandStyles.h"
#include <array>
#include <vector>

// ---------------------------------------------------------------------------
// Tape Twin: a tape echo (wow, flutter, worn tape, runaway feedback) plus
// Classic studio-style artificial double tracking (ADT).
// ---------------------------------------------------------------------------
namespace tw
{
enum Param { Drive = 0, Time, Sync, Feedback, Wow, Flutter, Age, Spread, EchoMix, AdtMix, AdtTime, AdtWobble, Output, NumParams };

inline const char* paramId (int p)
{
    static const char* n[] = { "drive", "time", "sync", "feedback", "wow", "flutter", "age", "spread",
                               "echo_mix", "adt_mix", "adt_time", "adt_wobble", "output" };
    return n[p];
}

inline const juce::StringArray& syncNames()
{
    static const juce::StringArray s { "Free (ms)", "1/4", "1/8 dotted", "1/8", "1/8 triplet", "1/16" };
    return s;
}
inline double syncBeats (int i)   // in quarter notes
{
    static const double b[] = { 0.0, 1.0, 0.75, 0.5, 1.0 / 3.0, 0.25 };
    return b[i];
}

struct Preset { const char* name; float v[NumParams]; };
inline const std::array<Preset, 6>& presets()
{
    //                   drive time  sync fb    wow   flut  age   sprd  echo  adt   adtT  adtW  out
    static const std::array<Preset, 6> p { {
        { "Slapback '57",       { 0.30f, 110,  0, 0.10f, 0.15f, 0.25f, 0.35f, 0.00f, 0.40f, 0.00f, 25, 0.3f, 0 } },
        { "Studio ADT",         { 0.15f, 300,  0, 0.00f, 0.10f, 0.20f, 0.20f, 0.00f, 0.00f, 0.80f, 28, 0.5f, -1 } },
        { "Multi-Head Space",   { 0.25f, 380,  2, 0.55f, 0.30f, 0.35f, 0.50f, 0.60f, 0.45f, 0.00f, 25, 0.3f, 0 } },
        { "Runaway Feedback",   { 0.40f, 420,  3, 0.92f, 0.35f, 0.40f, 0.55f, 0.50f, 0.50f, 0.00f, 25, 0.3f, -3 } },
        { "Worn-Out Cassette",  { 0.50f, 260,  3, 0.45f, 0.75f, 0.70f, 0.85f, 0.30f, 0.40f, 0.20f, 22, 0.6f, 0 } },
        { "Double + Echo",      { 0.20f, 250,  3, 0.30f, 0.20f, 0.25f, 0.35f, 0.40f, 0.30f, 0.60f, 30, 0.4f, -1 } },
    } };
    return p;
}

// cubic-interpolated circular delay line
struct DelayLine
{
    std::vector<float> buf;
    int w = 0;
    void setSize (int n) { buf.assign ((size_t) juce::jmax (8, n), 0.0f); w = 0; }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    inline void write (float x) noexcept { buf[(size_t) w] = x; if (++w >= (int) buf.size()) w = 0; }
    inline float read (double delay) const noexcept   // delay in samples (>= 1)
    {
        const int n = (int) buf.size();
        delay = juce::jlimit (1.0, (double) (n - 4), delay);
        double rp = (double) w - delay;
        while (rp < 0) rp += n;
        const int i1 = (int) rp;
        const float f = (float) (rp - i1);
        const int i0 = (i1 - 1 + n) % n, i2 = (i1 + 1) % n, i3 = (i1 + 2) % n;
        const float y0 = buf[(size_t) i0], y1 = buf[(size_t) i1], y2 = buf[(size_t) i2], y3 = buf[(size_t) i3];
        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }
};

struct OnePole
{
    float a = 0, z = 0;
    void lp (double sr, double f) { a = (float) std::exp (-juce::MathConstants<double>::twoPi * f / sr); }
    inline float lpProcess (float x) noexcept { z = (1.0f - a) * x + a * z; return z; }
    inline float hpProcess (float x) noexcept { z = (1.0f - a) * x + a * z; return x - z; }
};
} // namespace tw

class TapeTwinProcessor : public juce::AudioProcessor
{
public:
    TapeTwinProcessor();
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Tape Twin"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return (int) tw::presets().size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;
    // genre styles from Bandleader (moves this plugin's knobs when a style is picked)
    void applyStyle (const band::StyleDef&);
    band::StyleFollower styleFollower { apvts, [this] (const band::StyleDef& s) { applyStyle (s); } };
    std::atomic<float> echoLevel { 0.0f }, currentDelayMs { 0.0f };
    std::atomic<bool> uiLinked { false };

    void processCore (juce::AudioBuffer<float>&, juce::MidiBuffer&);
    band::Bench bench;
private:
    std::atomic<float>* prm[tw::NumParams] {};
    double sr = 44100.0;
    tw::DelayLine echo[2], adt[2];
    tw::OnePole fbLp[2], fbHp[2], inLp[2];
    double smoothDelay = 0.0;
    double wowPh = 0, wowPh2 = 0, flPh = 0, flPh2 = 0, adtPh[2] { 0.0, 0.37 };
    float drift = 0, driftTarget = 0, adtDrift[2] {}, adtDriftTarget[2] {};
    int driftCounter = 0;
    float throwEnv = 0;
    uint32_t rng = 22222u;
    int currentProgram = 0;

    float rand01() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (float) (rng & 0xffffff) / 16777216.0f; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeTwinProcessor)
};
