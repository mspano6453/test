#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "BandStyles.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>
#include "Params.h"
#include "DrumEngine.h"
#include "Player.h"

class TeaTowelProcessor : public juce::AudioProcessor
{
public:
    std::atomic<bool> uiLinked { false };
    TeaTowelProcessor();
    ~TeaTowelProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Tea Towel"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }   // the Player's hits, for recording
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return (int) tt::presets().size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Sample slots (call from the message thread)
    bool loadSample (int piece, const juce::File& file);
    void clearSample (int piece);
    juce::String getSampleName (int piece) const;      // user sample name, empty if built-in
    static juce::String builtInName (int piece);

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;
    // genre styles from Bandleader (moves this plugin's knobs when a style is picked)
    void applyStyle (const band::StyleDef&);
    band::StyleFollower styleFollower { apvts, [this] (const band::StyleDef& s) { applyStyle (s); } };
    juce::MidiKeyboardState keyboardState;                     // lets the UI play the kit
    std::array<std::atomic<float>, tt::NumPieces> hitFlash;    // UI hit lights
    std::atomic<float> gainReductionDb { 0.0f };               // Squash meter
    tt::GroovePlayer player;                                   // the PLAYER page

private:
    void handleMidi (const juce::MidiMessage& m);
    void renderSegment (juce::AudioBuffer<float>& out, int start, int end);

    std::atomic<float>* pp[tt::NumPieces][tt::NumPieceParams] {};
    std::atomic<float>* mp[tt::NumMasterParams] {};

    void loadBuiltInKit();
    tt::KitBank bank;
    tt::DrumEngine engine;
    tt::PieceSettings settings[tt::NumPieces];
    std::shared_ptr<const tt::SampleData> samples[tt::NumPieces];
    std::shared_ptr<const tt::SampleData> blockSamples[tt::NumPieces];
    std::vector<std::shared_ptr<const tt::SampleData>> retired;
    juce::String samplePaths[tt::NumPieces];
    juce::CriticalSection pathLock;
    juce::AudioFormatManager formatManager;
    juce::MidiBuffer playerOut;
    int64_t sampleCounter = 0;
    std::atomic<float>* playerParams[8] {};

    double sr = 44100.0;
    std::vector<float> scratch;
    juce::AudioBuffer<float> roomBuf;
    juce::Reverb reverb;
    tt::Biquad lowCut[2], thudEq[2], tapeLp[2], dcBlock[2];
    float grState = 0.0f;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TeaTowelProcessor)
};
