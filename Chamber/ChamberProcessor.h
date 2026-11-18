#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/BandStyles.h"
#include "../Source/MidiCapture.h"
#include "../Undertow/UndertowPlayer.h"     // shared song form + progressions
#include "ChamberDSP.h"

// ---------------------------------------------------------------------------
// Chamber: a 60s orchestral arranger. Four parts (violins, violins, violas,
// cellos - or brass, or woodwinds), voiced like a real arranger would and
// played in a feel: long chords, driving staccato eighths, a descending cello
// line, pizzicato, a tremolo swell, the chaotic orchestral rise...
// ---------------------------------------------------------------------------
namespace ch
{
enum Feel { FLong = 0, FStaccato, FCounter, FDescend, FPizz, FTremolo, FRise, FRiff, FFanfare, FUnison, FBaroque, NumFeels };
inline const char* feelName (int f)
{
    static const char* n[] = { "Long Chords", "Staccato Eighths", "Countermelody", "Descending Line", "Pizzicato", "Tremolo Swell",
                               "Rising Chaos", "Cello Riff", "Fanfare", "Unison Hook", "Baroque Lines" };
    return (f >= 0 && f < NumFeels) ? n[f] : "";
}
enum Follow { FBand = 0, FMidiChords, FPlay, FGuitar };   // FGuitar: the orchestra plays what the guitar on the sidechain plays
constexpr int kNumVoices = 40;
}

class ChamberProcessor : public juce::AudioProcessor
{
public:
    ChamberProcessor();
    ~ChamberProcessor() override { renderer.reset(); }
    std::unique_ptr<mc::SongRenderer> renderer;
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        const auto in = l.getMainInputChannelSet();
        return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && (in.isDisabled() || in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
    }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Chamber"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }      // the parts it plays, so you can record them
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
    mc::Capture capture;

    std::atomic<int> uiChord { -1 }, uiSection { 0 };
    std::atomic<bool> uiLinked { false }, uiTransport { false };
    std::atomic<int> uiPartNote[5] { { -1 }, { -1 }, { -1 }, { -1 }, { -1 } };   // the four parts + timpani
    std::atomic<float> uiPartLevel[5] {};
    bool samplesLoaded() const { return lib.count > 0; }

private:
    struct Ev { int64_t at; int type; int part; int instr; int art; int note; float vel; float len; float attack = 0.01f; float glide = 0; float glideSec = 0; };
    // type 1 = note on, 3 = release the parts in `part` (a bit mask; 31 = everything)
    struct Hit { int step; int parts; int art; float len; float vel; int kind; int arg; };
    enum Kind { KChord = 0, KMelody, KBass, KRiff, KUnison, KRun };
    void readParams();
    void loadSamples();
    void runPlayer (int n);
    void playStep (int64_t k, int64_t at);
    void writeBar (int sec, int barIn, bool fillBar);
    void writePhrase (int sec);
    void voiceChord (int root, int q);
    int fitTo (int instr, int note) const;
    void schedule (const Ev& e) { if (queue.size() < 2048) queue.push_back (e); }
    void startNote (const Ev& e);
    void releaseParts (int mask);
    int sectionAt (int64_t bar, int& barIn, bool& lastBar) const;
    void chordAt (int64_t bar, int& root, int& q) const;
    int scaleNote (int deg) const;

    double sr = 44100, bpmNow = 120;
    int meterNow = 0, bs = 16;
    band::Fade fade;
    ch::Library lib;
    ch::Voice voices[ch::kNumVoices];
    ch::Rack rack;
    ch::Rng rng;
    std::vector<Ev> queue;
    std::vector<Hit> bar;
    mc::NoteOut midiOut;

    int ensemble = 0, feel = 0, follow = 0, key = 9, progs[3] { 0, 2, 5 }, chordBars = 1, sectionBars = 8, section = 0, reg = 1, playArt = 0;
    bool autoSong = true, sitOut = false, timpani = false;
    float busy = 0.5f, dynamics = 0.65f, human = 0.6f, swing = 0, lengthK = 1, swell = 0.4f, warp = 0, width = 0.9f;
    int parts[4] { ch::VLN, ch::VLN, ch::VLA, ch::VC };

    band::Leader leader; bool led = false;
    std::array<bool, 128> held {};
    int midiRoot = -1, midiQ = 0;
    int64_t sampleClock = 0, lastStep = -1, curBar = -1;
    double lastPpqEnd = -1, blockPpq = 0;
    bool wasPlaying = false, outNow = false;
    int lastEnsemble = -1, curRoot = -1, curQ = 0, lastRoot = -1, lastQ = -1, barSec = 0, barIn = 0;
    int voicing[4] { 72, 67, 60, 48 };
    bool minorKey = false;
    std::vector<std::pair<int, int>> phrase[3];       // (step 0-31, scale degree)
    uint32_t seeds[3] { 11, 23, 37 };
    bool rising = false;
    uint32_t lastMutate = 0;   // Bandleader's MUTATE
    ut::PitchTracker tracker;            // guitar to orchestra
    std::vector<float> scBuf;
    int guitarNote = -1;
public:
    std::atomic<int> uiGuitarNote { -1 };
    bool liveFollow() const { return follow == ch::FPlay || follow == ch::FGuitar; }
};
