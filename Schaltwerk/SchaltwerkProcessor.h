#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/BandStyles.h"
#include "SchaltwerkDSP.h"

namespace sw
{
// MIDI zones
//   C-1..B-1 (0-11)  Omnichord 7th chords      C0..B0 (12-23) minor chords      (24-35) major chords
//   C1..B1  (36-47)  drums                     C2 and up (48+)  robot synth / vocoder carrier
//   Mod wheel (CC1)  strums the strumplate
constexpr int kSynthLowNote = 48;

inline juce::String dId (int d, const char* what) { return "d" + juce::String (d) + "_" + what; }
inline const char* noteName (int n) { static const char* s[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; return s[((n % 12) + 12) % 12]; }

struct Pattern { const char* name; const char* rows[NumDrums]; };
const std::vector<Pattern>& patterns();
}

class SchaltwerkProcessor : public juce::AudioProcessor
{
public:
    SchaltwerkProcessor();
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Schaltwerk"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return (int) sw::patterns().size(); }
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

    // Report the audio input as a sidechain (not a main input), so Live treats this as an
    // instrument and offers the input in its Sidechain menu for the vocoder.
    struct Vst3Ext : juce::VST3ClientExtensions { bool getPluginHasMainInput() const override { return false; } } vst3Ext;
    juce::VST3ClientExtensions* getVST3ClientExtensions() override { return &vst3Ext; }

    // step sequencer: 0 off, 1 on, 2 accent (UI writes, audio reads)
    std::atomic<uint8_t> steps[sw::NumDrums][16];
    void loadPattern (int index);
    void clearPattern();
    juce::String patternToString() const;
    void patternFromString (const juce::String&);

    // UI interaction
    juce::MidiKeyboardState keyboardState;       // audition drums / play the robot synth
    std::atomic<int> uiStrum { -1 };             // strumplate position from the UI (-1 = not touching)
    std::atomic<int> uiChordGate { 0 };          // a chord button is held
    void pressChord (int root, int type);
    void releaseChord();

    // read by the UI
    std::atomic<int> currentStep { -1 }, lastString { -1 };
    std::atomic<bool> chordSounding { false }, uiLinked { false };
    std::atomic<int> uiOmniRoot { 0 }, uiOmniType { 0 };
    std::atomic<float> drumFlash[sw::NumDrums] {};

private:
    void readParams();
    void handleMidi (const juce::MidiMessage&);
    void renderSegment (float* L, float* R, int start, int end);
    void runSequencer (juce::MidiBuffer& midi, int n);

    double sr = 44100;
    sw::DrumBox drums;
    sw::RobotSynth synth;
    sw::Omnichord omni;
    sw::Echo echo;
    sw::DrumSettings drumSet[sw::NumDrums];
    sw::SynthParams synthP;
    sw::OmniParams omniP;
    float drumSend = 0, synthSend = 0, omniSend = 0, synthLevel = 0.7f;
    std::vector<float> sideL, sideR, sendL, sendR, scratch;
    bool haveSide = false;
    int lastUiStrum = -1, lastUiGate = 0;
    int midiChordKey = -1;
    int64_t lastStepIndex = -1;
    int autoString = 0;
    bool bandChord = false;
    double bpm = 120;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SchaltwerkProcessor)
};
