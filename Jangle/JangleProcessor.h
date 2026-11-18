#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/BandStyles.h"
#include "../Undertow/UndertowPlayer.h"     // shared song form + progressions
#include "JanglePlayer.h"
#include "../Source/MidiCapture.h"

// ---------------------------------------------------------------------------
// Jangle: a rhythm guitarist for the band. Real steel-string acoustic, clean
// and hollow-body electrics (with crunch, fuzz, a 12-string shimmer and palm
// mutes), playing real chord shapes in a strum or picking pattern. Follows the
// Bandleader's chords, MIDI chords, or you just play it.
// ---------------------------------------------------------------------------
namespace jg
{
enum Follow { FBand = 0, FMidiChords, FPlay };
constexpr int kNumVoices = 24;
}

class JangleProcessor : public juce::AudioProcessor
{
public:
    JangleProcessor();
    ~JangleProcessor() override { renderer.reset(); }
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Jangle"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }      // the notes it strums, so you can record them
    double getTailLengthSeconds() const override { return 2.0; }
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
    std::atomic<int> uiChord { -1 }, uiSection { 0 }, uiStep { -1 }, uiSteps { 16 }, uiStroke { -1 };
    std::atomic<bool> uiLinked { false }, uiTransport { false };
    std::atomic<int> uiFret[6] { -1, -1, -1, -1, -1, -1 };      // the shape being played
    std::atomic<float> uiString[6] {};                          // how hard each string was just hit
    bool samplesLoaded() const { return loaded > 0; }
    mc::Capture capture;
    std::unique_ptr<mc::SongRenderer> renderer;

private:
    struct Ev { int64_t at; int type; int string; int note; float vel; float len; int set; };   // type 1 on, 2 choke string, 3 all off
    void readParams();
    void loadSamples();
    void runPlayer (int n);
    void playStep (int64_t k, int64_t at);
    void strum (const jg::Hit& h, int64_t at, const jg::Shape& shape, double sixteenth);
    void schedule (const Ev& e) { if (queue.size() < 2048) queue.push_back (e); }
    void startString (const Ev& e);
    jg::Shape shapeFor (int root, int q) const;
    void chordAt (int64_t bar, int& root, int& q) const;
    int sectionAt (int64_t bar, int& barIn, bool& lastBar) const;

    double sr = 44100;
    jg::Library lib; int loaded = 0;
    jg::Voice voices[jg::kNumVoices];
    jg::Rig rig;
    jg::Rng rng;
    std::vector<Ev> queue;
    std::vector<jg::Hit> bar;
    std::vector<float> tmpL, tmpR;

    // params
    int guitar = 0, feel = 0, follow = 0, key = 9, progs[3] { 0, 2, 5 }, chordBars = 1, sectionBars = 8, section = 0, position = 0;
    bool autoSong = true; int leadFills = 1;
    float busy = 0.5f, human = 0.6f, swing = 0, dynamics = 0.7f, lengthK = 1, strumSpeed = 0.5f;

    // state
    band::Leader leader; bool led = false;
    std::array<bool, 128> held {};
    int midiRoot = -1, midiQ = 0;
    jg::Shape shape; int lastRoot = -1, lastQ = -1;
    int64_t sampleClock = 0, lastStep = -1, curBar = -1, filledBar = -1;
    double lastPpqEnd = -1, bpmNow = 120, blockPpq = 0;
    bool wasPlaying = false, bandOut = false, blockPlaying = false;
    int meterNow = 0, bs = 16, barSection = 0, barInSec = 0, playVoice = 0;
    band::Fade fade;
    mc::NoteOut midiOut;
    juce::MidiBuffer outMidi;
    uint32_t lastMutate = 0;   // Bandleader's MUTATE
};
