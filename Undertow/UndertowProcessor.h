#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/BandStyles.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include "UndertowPlayer.h"
#include "SynthBass.h"
#include "../Source/BandBus.h"

// ---------------------------------------------------------------------------
// Undertow: a virtual post-punk bass player. Real picked-bass samples, a
// player that follows Live's tempo and your chords (from MIDI or by listening
// to your guitar on the sidechain), and a chorus/flanger/grit/amp rig.
// It can also turn your guitar itself into bass.
// ---------------------------------------------------------------------------
namespace ut
{
enum Mode { MPlayer = 0, MKeys, MOctave, MTracked };
enum Follow { FollowSelf = 0, FollowGuitar, FollowMidi };
// keyswitches for the player (MIDI 12-15; Live calls these C-1, C#-1, D-1, D#-1)
constexpr int kKeyVerse = 12, kKeyChorus = 13, kKeyBridge = 14, kKeyFill = 15;
}

class UndertowProcessor : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    UndertowProcessor();
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Undertow"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }     // the bass line, so you can record it to a clip
    double getTailLengthSeconds() const override { return 1.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;
    // genre styles from Bandleader (moves this plugin's knobs when a style is picked)
    void applyStyle (const band::StyleDef&);
    band::StyleFollower styleFollower { apvts, [this] (const band::StyleDef& s) { applyStyle (s); } };

    // Report the audio input as a sidechain so Live treats this as an instrument and
    // offers your guitar track in the Sidechain menu.
    struct Vst3Ext : juce::VST3ClientExtensions { bool getPluginHasMainInput() const override { return false; } } vst3Ext;
    juce::VST3ClientExtensions* getVST3ClientExtensions() override { return &vst3Ext; }

    // UI -> audio
    std::atomic<bool> uiNewLine { false }, uiFill { false };
    juce::MidiKeyboardState keyboardState;

    // audio -> UI
    std::atomic<int> uiChord { -1 }, uiNote { -1 }, uiStep { -1 }, uiSection { 0 }, uiFillCountdown { 0 };
    std::atomic<bool> uiLinked { false };
    std::atomic<bool> uiBandPlaying { false }, uiTransport { false }, uiSidechain { false };
    std::atomic<float> uiInput { 0 };
    std::atomic<int> uiMotif[ut::kSteps];    // pitch per step (-1 rest), +1000 for ghost notes
    const ut::ChordDetector& debugEars() const { return ears; }
    bool samplesLoaded() const { return sampler.hasBank(); }

private:
    struct Ev { int t; int type; int note; float vel; float len; bool slide = false; };    // type 1 on, 2 off
    void readParams();
    void handleAsyncUpdate() override;
    std::atomic<int> keySection { -1 };
    void loadSamples();
    std::shared_ptr<const ut::Bank> loadBank (const char* prefix, int layers, int rrs);
    int activeNote() const { return bassType >= 2 ? synth.currentNote : sampler.currentNote; }
    void handleMidi (const juce::MidiMessage&, std::vector<Ev>&, int pos);
    void runPlayer (int n, std::vector<Ev>& evs);
    void playStep (int64_t k, int t, std::vector<Ev>& evs);
    void newPass (int64_t k);
    void rebuildBase (int section, bool fresh);
    float offsetMs (int64_t k) const;
    ut::Chord currentChord() const;
    int pitchFor (ut::Chord c, const ut::Step& s) const;
    void publishMotif();

    double sr = 44100;
    ut::BassSampler sampler;
    ut::SynthBass synth;
    std::shared_ptr<const ut::Bank> pickedBank, fingerBank;
    int bassType = 0, loadedType = -1;
    ut::ChordDetector ears;
    ut::PitchTracker tracker;
    ut::OctaveDown octave;
    ut::Rig rig;
    ut::Harmony harmony;
    ut::Rng rng;

    // params (cached per block)
    int mode = 0, follow = 0, feel = 0, section = 0, reg = 0, keysOct = 0;
    int key = 9, progs[ut::NumSections] { 0, 2, 5 }, chordBars = 1, sectionBars = 8; bool autoSong = true;
    ut::Chord selfChord;
    band::Leader leader; bool led = false, lockDrums = true; int64_t filledBar = -1;
    ut::Chord selfChordAt (int64_t k) const;
    float variation = 0.4f, fills = 0.5f, hook = 0.5f, density = 0.55f, push = 0.25f, human = 0.6f, swing = 0, lengthK = 0.9f;
    float sens = 0.5f, guitarDry = 0, lastFillParam = 0;
    bool stopWithBand = true;

    // player state
    ut::Motif base[ut::NumSections], inst;
    uint32_t seeds[ut::NumSections] { 11, 23, 37 };
    int baseFeel = -1; float baseHook = -1, baseDensity = -1;
    int activeSection = 0, motifPos = 0, barsInSection = 0;
    bool fillPending = false;
    int64_t lastStep = -1;
    double lastPpqEnd = -1;
    bool wasPlaying = false;
    uint32_t runSeed = 1;
    int64_t sampleClock = 0, lastNoteAt = -1000000;
    ut::Step lastStepPlayed; ut::Chord lastNoteChord; float lastNoteLen = 0;
    int lastChordChanges = 0, harmonyTick = 0;
    double bpmNow = 120;
    ut::Chord midiChord, heldChord;
    std::array<bool, 128> held {};
    int keysNote = -1;

    // MIDI out
    int midiOutNote = -1, midiOffIn = -1;

    std::vector<float> sc, mono, tmpR;
    std::vector<Ev> events;
};
