#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/BandBus.h"
#include "../Source/BandStyles.h"
#include "../Undertow/UndertowPlayer.h"
#include "../Source/MidiCapture.h"

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

class BandleaderProcessor : public juce::AudioProcessor, private juce::AsyncUpdater, private juce::Timer
{
public:
    BandleaderProcessor();
    void prepareToPlay (double, int) override { chordOut.ensureSize (2048); }
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Bandleader"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }      // the chords, so you can drag them into a clip
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
    void reapplyStyle();                 // "Apply again": put every plugin's knobs back to the style
    void timerCallback() override;      // public so offline tests can drive it
    std::atomic<int> styleIdx { -1 };
    std::atomic<uint32_t> styleStamp { 0 };

    // audio -> UI
    std::atomic<int> uiSection { 0 }, uiNextSection { -1 }, uiBarIn { 0 }, uiBarsToNext { -1 }, uiFormIndex { 0 }, uiChord { -1 };
    std::atomic<bool> uiPlaying { false }, uiOut { false }, uiDropComing { false }, uiFillComing { false }, uiBusOk { false };
    std::atomic<int> uiLastCmd { 0 };

    mc::Capture capture;

private:
    juce::MidiBuffer chordOut;
    std::vector<int> chordNotes;
    int lastChordKey = -1;
    void handle (Cmd c, int64_t bar, double frac);
    void postParams (int sec, bool manual);
    void handleAsyncUpdate() override;
    void applyStyle (int idx);
    int lastStyleParam = 0;
    std::atomic<int> pendingSection { 0 }; std::atomic<bool> pendingManual { false }, paramsPending { false };
    int sectionBarsNow() const;

    std::atomic<int> uiCmd { 0 };
    band::Leader L;
    bool wasPlaying = false;
    double lastPpqEnd = -1;
    int formIndex = 0, pendingForm = -1;
    std::array<float, 7> lastTrig {};
    std::array<int, 7> lastCC {};
};
