#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/BandStyles.h"
#include "../Undertow/UndertowPlayer.h"     // shared song form + progressions
#include "ParlourPlayer.h"
#include "../Source/MidiCapture.h"

// ---------------------------------------------------------------------------
// Parlour: a keys player for the band. Real piano, vibes and glockenspiel,
// modelled electric piano, organ, clav, string machine, brass and pads. It
// comps the Bandleader's chords in a genre feel, follows MIDI chords, or you
// just play it.
// ---------------------------------------------------------------------------
namespace pk
{
enum Follow { FBand = 0, FMidiChords, FPlay };
constexpr int kNumVoices = 24;
}

class ParlourProcessor : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    ParlourProcessor();
    ~ParlourProcessor() override { renderer.reset(); }
    std::unique_ptr<mc::SongRenderer> renderer;
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Parlour"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }      // what it plays, so you can record it
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
    std::atomic<int> uiChord { -1 }, uiSection { 0 }, uiStep { -1 };
    std::atomic<bool> uiLinked { false }, uiTransport { false };
    std::atomic<uint32_t> uiNotes[4] {};   // 128 bits: which keys are sounding
    bool samplesLoaded() const { return loaded > 0; }
    mc::Capture capture;

private:
    struct Ev { int64_t at; int type; int note; float vel; float len; };   // type 1 on, 2 off, 3 all off
    void readParams();
    void handleAsyncUpdate() override {}
    void loadSamples();
    void runPlayer (int n);
    void playStep (int64_t k, int64_t at);
    void schedule (const Ev& e) { if (queue.size() < 1024) queue.push_back (e); }
    void noteOn (int note, float vel, float len);
    void noteOff (int note);
    void allOff();
    pk::KChord chordAt (int64_t bar) const;
    int sectionAt (int64_t bar, int& barIn, bool& lastBar) const;

    double sr = 44100;
    pk::SampleLibrary lib; int loaded = 0;
    pk::Voice voices[pk::kNumVoices];
    pk::Rack rack;
    pk::Rng rng;
    std::vector<Ev> queue;
    std::vector<pk::Hit> bar;
    std::vector<float> tmpL, tmpR;

    // params
    int instrument = 0, feel = 0, follow = 0, key = 9, progs[3] { 0, 2, 5 }, chordBars = 1, sectionBars = 8, section = 0, reg = 1;
    bool autoSong = true, leftHand = false, stopWithBand = true, sustainPedal = false;
    float busy = 0.5f, colour = 0.3f, human = 0.6f, swing = 0, lengthK = 1, dynamics = 0.7f;

    // state
    band::Leader leader; bool led = false;
    std::array<bool, 128> held {}, sustained {};
    pk::KChord midiChord, lastChord;
    pk::Voicing voicing;
    int64_t sampleClock = 0, lastStep = -1, curBar = -1, filledBar = -1;
    double lastPpqEnd = -1;
    bool wasPlaying = false, bandOut = false;
    int lastInstrument = -1;
    double bpmNow = 120, blockPpq = 0;
    int meterNow = 0, bs = 16;
    band::Fade fade;
    mc::NoteOut midiOut;
    juce::MidiBuffer outMidi;
    int barSection = 0, barInSec = 0;
    uint32_t lastMutate = 0;   // Bandleader's MUTATE
};
