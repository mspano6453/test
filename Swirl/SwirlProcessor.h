#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/SongForm.h"
#include <vector>

// ---------------------------------------------------------------------------
// Swirl: the psychedelic studio tricks in one box, for guitar, vocals, drums
// or the whole mix. BACKWARDS plays what you just played in reverse, in time
// with the song. PHASE is a slow sweeping phaser. ROTARY is a spinning-speaker
// cabinet. TAPE FLANGE is two tape machines drifting against each other.
// ---------------------------------------------------------------------------
class SwirlProcessor : public juce::AudioProcessor
{
public:
    SwirlProcessor();
    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override
    { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && l.getMainInputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Swirl"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
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
    std::atomic<float> uiSpin { 0 }, uiSweep { 0 }, uiRev { 0 };
    std::atomic<bool> uiLinked { false };

private:
    void processCore (juce::AudioBuffer<float>&);
    band::Bench bench;
    double sr = 44100;
    // backwards
    std::vector<float> rec[2]; int recW = 0, revLen = 22050, revK = 0, revStart = 0;
    // phaser
    float apZ[2][6] {}; float phFb[2] {}; double phPh = 0;
    // rotary + flange share one short delay line each
    std::vector<float> rot[2], fl[2]; int rotW = 0, flW = 0;
    double rotPh = 0, rotRate = 0.8, flPh = 0;
    float flFb[2] {};
};
