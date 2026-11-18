#pragma once
#include "../Source/SongForm.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/BandStyles.h"
#include <juce_dsp/juce_dsp.h>
#include "LatheDSP.h"

namespace lt
{
enum Module { MEq = 0, MDynEq, MTape, MDyn, MExciter, MImager, MMax, NumModules };

inline const char* moduleName (int m)
{
    static const char* n[] = { "Equalizer", "Dynamic EQ", "Vintage Tape", "Dynamics", "Exciter", "Imager", "Maximizer" };
    return n[m];
}
inline juce::String modOnId (int m) { return "mod_on_" + juce::String (m); }

inline const char* eqBandName (int b)
{
    static const char* n[] = { "Low Cut", "Low Shelf", "Bell 1", "Bell 2", "Bell 3", "Bell 4", "High Shelf", "High Cut" };
    return n[b];
}
inline juce::String eqId (int b, const char* what) { return "eq" + juce::String (b) + "_" + what; }
inline juce::String deId (int b, const char* what) { return "de" + juce::String (b) + "_" + what; }
inline juce::String dyId (int b, const char* what) { return "dy" + juce::String (b) + "_" + what; }
inline juce::String exId (int b, const char* what) { return "ex" + juce::String (b) + "_" + what; }
inline juce::String imId (int b) { return "im" + juce::String (b) + "_w"; }

enum AssistTarget { TargetStreaming = 0, TargetBalanced, TargetLoud };
inline float targetLufs (int t) { static const float v[] = { -14.0f, -11.0f, -9.0f }; return v[t]; }
inline const char* targetName (int t) { static const char* n[] = { "Streaming (-14 LUFS)", "Balanced (-11 LUFS)", "Loud (-9 LUFS)" }; return n[t]; }

// lock-free single-producer/single-consumer sample FIFO
struct SampleFifo
{
    explicit SampleFifo (int size) : fifo (size), data ((size_t) size, 0.0f) {}
    void push (const float* x, int n)
    {
        int s1, z1, s2, z2;
        fifo.prepareToWrite (n, s1, z1, s2, z2);
        if (z1 > 0) std::copy (x, x + z1, data.begin() + s1);
        if (z2 > 0) std::copy (x + z1, x + z1 + z2, data.begin() + s2);
        fifo.finishedWrite (z1 + z2);
    }
    int pull (float* out, int n)
    {
        int s1, z1, s2, z2;
        fifo.prepareToRead (n, s1, z1, s2, z2);
        if (z1 > 0) std::copy (data.begin() + s1, data.begin() + s1 + z1, out);
        if (z2 > 0) std::copy (data.begin() + s2, data.begin() + s2 + z2, out + z1);
        fifo.finishedRead (z1 + z2);
        return z1 + z2;
    }
    int available() const { return fifo.getNumReady(); }
    juce::AbstractFifo fifo;
    std::vector<float> data;
};
} // namespace lt

class LatheProcessor : public juce::AudioProcessor
{
public:
    LatheProcessor();
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Lathe"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override;
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

    // Module order (message thread writes, audio thread reads)
    std::array<std::atomic<int>, lt::NumModules> order;
    void setOrder (const std::array<int, lt::NumModules>& o);

    // Meters
    lt::Loudness inLoud, outLoud;
    std::atomic<float> inPeak { 0 }, outPeak { 0 }, outTruePeak { 0 }, maxGr { 0 };
    std::atomic<float> dynGr[3] {}, dynEqCut[2] {};
    std::atomic<bool> resetLoudness { false }, uiLinked { false };
    std::atomic<float> uiSecLufs[3] { -70.0f, -70.0f, -70.0f };
    double secEnergy[3] {}; long secCount[3] {}; int64_t secClock = 0;
    lt::SampleFifo spectrumFifo { 1 << 15 }, scopeFifo { 1 << 14 }, assistFifo { 1 << 17 };

    // Assistant (driven from the editor's timer on the message thread)
    void startAssistant (int target);
    void assistantTick();
    std::atomic<int> assistPhase { 0 };     // 0 idle, 1 listening (EQ), 2 setting loudness, 3 done
    std::atomic<float> assistProgress { 0 };
    juce::String assistMessage;

    double getSampleRateSafe() const { return sr; }

    void processCore (juce::AudioBuffer<float>&, juce::MidiBuffer&);
    band::Bench bench;
    std::atomic<bool> uiTapeStopped { false };
private:
    void tapeStopBlock (float* L, float* R, int n);
    std::vector<float> tsBuf[2]; int tsW = 0, tsState = 0; double tsRead = 0, tsRate = 1; uint32_t tsStamp = 0; bool tsStampSeen = false, tsEndDone = false, tsWasPlaying = false;

    void readParams();
    void setParam (const juce::String& id, float value);

    double sr = 44100;
    lt::Equalizer eq;
    lt::DynamicEq deq;
    lt::VintageTape tape;
    lt::MultibandComp dyn;
    lt::Exciter ex;
    lt::Imager im;
    lt::Maximizer mx;
    lt::Dither dither;
    std::atomic<bool> assistCapture { false };
    std::vector<float> scratch;
    int currentProgram = 0;

    // assistant state
    int assistTarget = 0;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> fftBuf, fftAccum, window;
    std::vector<float> pullBuf;
    int fftFrames = 0, fillPos = 0;
    double assistSeconds = 0, phase2Seconds = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LatheProcessor)
};
