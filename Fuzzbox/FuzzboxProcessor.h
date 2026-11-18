#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>

// ---------------------------------------------------------------------------
// Fuzzbox: a 60s pedalboard for the guitar, in the order you'd chain them:
// WAH (your foot on a pedal, or following your playing) -> FUZZ (two
// germanium transistors) -> OCTAVE FUZZ (the ring of an octave up) ->
// TREMOLO (the amp's own, in time with Live) -> SPRING (the amp's reverb tank).
// Stomp them on and off from the footswitch (MIDI notes C3-G3), and work the
// wah with an expression pedal (CC 11 or CC 4).
// ---------------------------------------------------------------------------
class FuzzboxProcessor : public juce::AudioProcessor
{
public:
    enum Pedal { PWah = 0, PFuzz, POct, PTrem, PSpring, NumPedals };
    FuzzboxProcessor();
    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
        return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono()) && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
    }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Fuzzbox"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;
    static juce::StringArray presetNames();
    void loadPreset (int);
    static const char* onId (int p) { static const char* ids[] = { "wah_on", "fuzz_on", "oct_on", "trem_on", "spring_on" }; return ids[p]; }
    std::atomic<float> uiWah { 0 }, uiTrem { 1 }, uiIn { 0 }, uiOut { 0 };

private:
    double sr = 44100;
    // wah: a state-variable filter
    float svL[2][2] {}; float env = 0; double lfoPh = 0; float wahPos = 0;
    // fuzz + octave, run 4x oversampled
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    float fuzzHp[2] {}, fuzzLp[2] {}, octBp[2][2] {}, octLp[2] {}, dcX[2] {}, dcY[2] {}, toneZ[2] {};
    // tremolo
    double tremPh = 0;
    // spring tank
    struct Tank
    {
        std::vector<float> d; int w = 0; float ap[12] {}; float lp = 0, fb = 0; double mph = 0;
    } tank[2];
    float springIn[2] {};
    void processSpring (float* L, float* R, int n, float mix, float decay, float drip);
    void handleMidi (const juce::MidiMessage&);
};
