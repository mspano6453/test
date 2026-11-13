#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/BandStyles.h"
#include "../Undertow/UndertowPlayer.h"     // shared song form + progressions
#include "HarmoniaPlayer.h"
#include "../Source/MidiCapture.h"

// ---------------------------------------------------------------------------
// Harmonia: backing vocals for the band. A choir, a girl group, the fellas or
// a close trio sing the Bandleader's chords in close harmony: oohs and aahs,
// a hum, doo-wop, "ba ba ba", answers and gospel swells. The voices are made
// from scratch (a throat model), so there are no words - just the sound.
// ---------------------------------------------------------------------------
namespace hm
{
enum Follow { FBand = 0, FMidiChords, FPlay };
constexpr int kMaxParts = 4, kPerPart = 3, kPlayVoices = 8;
constexpr int kNumSingers = kMaxParts * kPerPart + kPlayVoices;
}

class HarmoniaProcessor : public juce::AudioProcessor
{
public:
    HarmoniaProcessor();
    ~HarmoniaProcessor() override { renderer.reset(); }
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Harmonia"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }      // the parts they sing, one MIDI channel each
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
    void applyStyle (const band::StyleDef&);
    band::StyleFollower styleFollower { apvts, [this] (const band::StyleDef& s) { applyStyle (s); } };

    // audio -> UI
    std::atomic<int> uiChord { -1 }, uiSection { 0 }, uiStep { -1 }, uiSteps { 16 }, uiVowel { 0 };
    std::atomic<bool> uiLinked { false }, uiTransport { false };
    std::atomic<int> uiPartNote[hm::kMaxParts] { -1, -1, -1, -1 };
    std::atomic<float> uiLevel { 0 };
    mc::Capture capture;
    std::unique_ptr<mc::SongRenderer> renderer;

private:
    struct Ev { int64_t at; int type; int part; int note; float vel; float len; hm::Syllable syl; bool legato; float attack; };
    // type 1 a part sings, 2 a part stops, 3 everyone stops, 5 play-it note on, 6 play-it note off
    void readParams();
    void runPlayer (int n);
    void playStep (int64_t k, int64_t at);
    void singChord (int64_t at, int root, int q, const hm::Hit& h, bool legato);
    void schedule (const Ev& e) { if (queue.size() < 1024) queue.push_back (e); }
    void handle (const Ev& e, juce::MidiBuffer& out, int pos);
    void chordAt (int64_t bar, int& root, int& q) const;
    int sectionAt (int64_t bar, int& barIn, bool& lastBar) const;

    double sr = 44100;
    hm::Singer singers[hm::kNumSingers];
    hm::Room room;
    hm::Rng rng;
    std::vector<Ev> queue;
    std::vector<hm::Hit> bar;
    std::vector<float> tmpL, tmpR;

    // params
    int group = 0, feel = 0, follow = 0, parts = 3, reg = 1, key = 9, progs[3] { 0, 2, 5 }, chordBars = 1, sectionBars = 8, section = 0;
    bool autoSong = true;
    float busy = 0.5f, dynamics = 0.7f, human = 0.6f, vibrato = 0.5f, breath = 0.3f, ensemble = 0.5f, swell = 0.4f, bright = 0.5f, roomAmt = 0.4f, width = 0.7f, outGain = 1;

    // state
    band::Leader leader; bool led = false;
    std::array<bool, 128> held {};
    int midiRoot = -1, midiQ = 0, lastRoot = -1, lastQ = -1;
    int partNote[hm::kMaxParts] { -1, -1, -1, -1 }, partMidi[hm::kMaxParts] { -1, -1, -1, -1 };
    bool singing = false; hm::Syllable lastSyl;
    int64_t sampleClock = 0, lastStep = -1, curBar = -1, filledBar = -1;
    double lastPpqEnd = -1, bpmNow = 120;
    bool wasPlaying = false, bandOut = false;
    int meterNow = 0, bs = 16, barSection = 0, barInSec = 0, playNext = 0;
    int playNote[hm::kPlayVoices] { -1, -1, -1, -1, -1, -1, -1, -1 };
    band::Fade fade;
    juce::MidiBuffer outMidi;
};
