#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/BandStyles.h"
#include "../Undertow/UndertowPlayer.h"     // shared song form + progressions
#include "EtherDSP.h"
#include "../Source/MidiCapture.h"

// ---------------------------------------------------------------------------
// Ether: early electronic instruments that play along. It writes melodies over
// the Bandleader's chords (memorable ones: a phrase, repeated and answered),
// holds eerie drones, or adds sci-fi swoops and radio bloops. Or play it.
// ---------------------------------------------------------------------------
namespace et
{
enum Role { RMelody = 0, RDrone, RSwoops, NumRoles };
enum Follow { FBand = 0, FPlay };
constexpr int kNumVoices = 6;
struct Note16 { int step = -1; int len = 2; int deg = 0; float vel = 0.8f; };   // one note of a phrase (deg: scale steps from the tonic)
}

class EtherProcessor : public juce::AudioProcessor
{
public:
    EtherProcessor();
    ~EtherProcessor() override { renderer.reset(); }
    std::unique_ptr<mc::SongRenderer> renderer;
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Ether"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }      // the tune it plays, so you can record it
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
    void applyStyle (const band::StyleDef&);
    band::StyleFollower styleFollower { apvts, [this] (const band::StyleDef& s) { applyStyle (s); } };

    // audio -> UI
    std::atomic<int> uiNote { -1 }, uiChord { -1 }, uiSection { 0 };
    std::atomic<bool> uiLinked { false }, uiTransport { false };
    std::atomic<float> uiLevel { 0 };
    std::atomic<int> uiTrail[32];   // the last notes played, for the scope
    mc::Capture capture;

private:
    struct Ev { int64_t at; int type; int note; float vel; float len; float gl = -1.0f; };   // 1 on, 2 off, 3 all off; gl = glide override (s)
    void readParams();
    void runPlayer (int n);
    void playStep (int64_t k, int64_t at);
    void writePhrase (int section, bool fresh);
    void schedule (const Ev& e) { if (queue.size() < 512) queue.push_back (e); }
    void noteOn (int note, float vel, float len, float glideSec = -1.0f);
    void noteOff (int note);
    void allOff();
    int sectionAt (int64_t bar, int& barIn, bool& lastBar) const;
    void chordAt (int64_t bar, int& root, int& q) const;
    int pitchFor (int deg, int root, int q) const;

    double sr = 44100, bpmNow = 120, blockPpq = 0;
    int meterNow = 0, bs = 16;
    std::vector<std::pair<int, int>> rhythmScratch = std::vector<std::pair<int, int>> (32);
    band::Fade fade;
    bool strongStep (int step) const { return meterNow == band::M44 ? step % 8 == 0 : meterNow == band::M34 ? step % 12 == 0 : step % 6 == 0; }
    mc::NoteOut midiOut;
    juce::MidiBuffer outMidi;
    et::Voice voices[et::kNumVoices];
    et::Rack rack;
    et::Rng rng;
    std::vector<Ev> queue;

    int instrument = 0, role = 0, follow = 0, key = 9, progs[3] { 0, 2, 5 }, chordBars = 1, sectionBars = 8, section = 0, reg = 1;
    bool autoSong = true, sitOut = false;
    float busy = 0.5f, wander = 0.5f, repeat = 0.6f, glide = 0.3f, vibrato = 0.5f;

    band::Leader leader; bool led = false;
    std::array<bool, 128> held {};
    int64_t sampleClock = 0, lastStep = -1, curBar = -1;
    double lastPpqEnd = -1;
    bool wasPlaying = false, outNow = false;
    int lastInstrument = -1, lastSection = -1, trailPos = 0;
    bool minorKey = true;
    std::vector<et::Note16> phrase[3];      // 2-bar phrase per section
    std::vector<et::Note16> answer;         // the reply to it
    uint32_t seeds[3] { 101, 202, 303 };
    int lastRoot = -1;
    uint32_t lastMutate = 0;   // Bandleader's MUTATE
};
