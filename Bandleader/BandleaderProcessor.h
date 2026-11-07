#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/BandBus.h"
#include "../Undertow/UndertowPlayer.h"

// ---------------------------------------------------------------------------
// Bandleader: runs the song for the whole Tea Towel band. Key, chords per
// section, the song form, section changes, fills, drop-outs and energy go out
// on the band bus; Tea Towel and Undertow follow it. Makes no sound itself.
// ---------------------------------------------------------------------------
namespace bl
{
// footswitch notes (send these from the MIDI Captain to the Bandleader track)
constexpr int kNext = 48, kFill = 50, kDrop = 52, kAllIn = 53, kVerse = 55, kChorus = 57, kBridge = 59;
constexpr int kCCBase = 90;   // CC 90..96 in the same order
inline const int kForm[6] = { band::Verse, band::Chorus, band::Verse, band::Chorus, band::Bridge, band::Chorus };
}

class BandleaderProcessor : public juce::AudioProcessor
{
public:
    BandleaderProcessor();
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Bandleader"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    enum Cmd { CNone = 0, CNext, CFill, CDrop, CAllIn, CVerse, CChorus, CBridge };
    void command (Cmd c) { uiCmd.store ((int) c); }

    // audio -> UI
    std::atomic<int> uiSection { 0 }, uiNextSection { -1 }, uiBarIn { 0 }, uiBarsToNext { -1 }, uiFormIndex { 0 }, uiChord { -1 };
    std::atomic<bool> uiPlaying { false }, uiOut { false }, uiDropComing { false }, uiFillComing { false }, uiBusOk { false };
    std::atomic<int> uiLastCmd { 0 };

private:
    void handle (Cmd c, int64_t bar, double frac);
    int sectionBarsNow() const;

    std::atomic<int> uiCmd { 0 };
    band::Leader L;
    bool wasPlaying = false;
    double lastPpqEnd = -1;
    int formIndex = 0;
    std::array<float, 7> lastTrig {};
    std::array<int, 7> lastCC {};
};
