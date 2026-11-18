#pragma once
#include "../Source/SongForm.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/MidiCapture.h"
#include "../Source/BandStyles.h"
#include "LoonDSP.h"
#include "FoundSounds.h"

class LoonProcessor : public juce::AudioProcessor
{
public:
    LoonProcessor();
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Loon"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return true; }      // the notes it plays, for other instruments
    mc::Capture capture;
    double getTailLengthSeconds() const override { return 12.0; }

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

    // read by the UI
    std::atomic<int> uiRoot { 9 }, uiMinor { 1 }, uiLastNote { -1 }, uiNoteKind { 0 };
    std::atomic<bool> uiFollowing { false }, uiLinked { false };
    std::atomic<float> uiInput { 0 }, uiJourney { 0 };
    std::atomic<uint32_t> uiNoteCounter { 0 };

    void processCore (juce::AudioBuffer<float>&, juce::MidiBuffer&);
    band::Bench bench;
private:
    float v (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    double sr = 44100;
    ln::KeyDetector key;
    ln::Generator gen;
    ln::Cloud cloud;
    ln::Nature nature;
    ln::FoundSounds found;
    ln::Space space;
    std::vector<float> mono, gL, gR, sL, sR;
    double journeyTime = 0;
    int followedRoot = 9, followedMinor = 1;
    float liftS = 0, dropS = 0, duck = 0, waitGain = 1;
    int64_t lastDuckStep = -1;
    int currentProgram = 0;

    struct PendingNote { int64_t t; int note; bool on; float vel; };
    PendingNote pending[128];
    int pendingCount = 0;
    int64_t sampleClock = 0;
    juce::MidiBuffer* currentMidi = nullptr;
    int blockSize = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoonProcessor)
    int64_t lastCollageBar = -1;
};
