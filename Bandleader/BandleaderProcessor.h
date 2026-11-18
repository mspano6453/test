#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/BandBus.h"
#include "../Source/BandStyles.h"
#include "../Source/SongForm.h"
#include "../Undertow/UndertowPlayer.h"
#include "../Source/MidiCapture.h"

// ---------------------------------------------------------------------------
// Bandleader: runs the song for the whole DEMOCRAZY band. Key, meter, chords
// per section, the song form (count-in, intro, breakdown, key change, ending),
// section changes, fills, drop-outs and energy go out on the band bus; every
// player follows it. It can learn a song's chords from your guitar or keyboard,
// and render the whole song to MIDI files. Makes no sound itself.
// ---------------------------------------------------------------------------
namespace bl
{
// footswitch notes (send these from the MIDI Captain to the Bandleader track)
constexpr int kNumCmds = 18;
inline const int kNotes[kNumCmds] = { 48, 50, 52, 53, 55, 57, 59, 60, 62, 64, 65, 67, 69, 71, 72, 74, 76, 77 };
constexpr int kCCBase = 90;   // CC 90..100 in the same order
constexpr int kLearnedProg = -2;   // (marker) the "Learned" item is the last one in the chord menus
}

class BandleaderProcessor : public juce::AudioProcessor, private juce::AsyncUpdater, private juce::Timer
{
public:
    BandleaderProcessor();
    ~BandleaderProcessor() override;
    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
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

    enum Cmd { CNone = 0, CNext, CFill, CDrop, CAllIn, CVerse, CChorus, CBridge, CEnd, CHalf, CKeyUp, CLearn,
               CStrangeUp, CStrangeDown, CMutate, CLoop, CTapeStop, CSketchNext, CSketchPrev, CLearnCancel };
    void command (Cmd c) { uiCmd.store ((int) c); }
    void reapplyStyle();                 // "Apply again": put every plugin's knobs back to the style
    void timerCallback() override;      // public so offline tests can drive it
    std::atomic<int> styleIdx { -1 };
    std::atomic<uint32_t> styleStamp { 0 };

    // ---- the song form, as the screen shows it
    band::FormSpec currentSpec() const;
    band::Song currentSong() const { return band::buildSong (currentSpec()); }
    int meterNow() const { return uiMeter.load(); }

    // ---- render the whole song to MIDI files (one per player)
    bool startRender();
    juce::File renderFolder;
    uint32_t renderStamp = 0;
    int64_t renderStartedMs = 0;

    // ---- learned chords (what LEARN heard), per section
    struct Learned
    {
        bool has[band::NumSec] {};
        int bars[band::NumSec] {};
        int8_t off[band::NumSec][band::kMaxProg] {};
        int8_t q[band::NumSec][band::kMaxProg] {};
    };
    Learned learnedCopy() const { const juce::SpinLock::ScopedLockType l (learnLock); return learned; }
    juce::String learnedChords (int sec, int key) const;
    int learnedIndex() const { return (int) ut::progressions().size(); }   // the "Learned" item in the chord menus

    // audio -> UI
    std::atomic<int> uiSection { 0 }, uiNextSection { -1 }, uiBarIn { 0 }, uiBarsToNext { -1 }, uiFormIndex { 0 }, uiChord { -1 };
    std::atomic<bool> uiPlaying { false }, uiOut { false }, uiDropComing { false }, uiFillComing { false }, uiBusOk { false };
    std::atomic<int> uiLastCmd { 0 }, uiMeter { 0 }, uiHostMeter { 0 }, uiPart { 0 }, uiKeyShift { 0 };
    std::atomic<int> uiLearnState { 0 }, uiLearnSec { 0 }, uiLearnBars { 0 }, uiLearnHeard { -1 };
    std::atomic<bool> uiOver { false }, uiEndComing { false }, uiHalfOn { false };
    std::atomic<int64_t> uiBar { 0 };

    std::atomic<uint32_t> benched { 0 };     // band members switched off with the lights in THE BAND
    void toggleBench (int who) { benched ^= (1u << who); }
    // ---- the chord chart: the chord now and the next changes (root | quality << 4, -1 = none) and how many bars away
    std::atomic<int> uiChartChord[5] { -1, -1, -1, -1, -1 }, uiChartBars[5] { 0, 0, 0, 0, 0 }, uiChartSec[5] { 0, 0, 0, 0, 0 };
    std::atomic<bool> uiLooping { false };
    // ---- the tuner: the sidechain, kept for the screen to read
    std::atomic<bool> tunerOn { false };
    static constexpr int kTunerN = 8192;
    float tunerBuf[kTunerN] {}; std::atomic<int> tunerW { 0 };
    // ---- the sketchbook: 20 saved song ideas
    static constexpr int kSketches = 20;
    void saveSketch (int slot);
    bool loadSketch (int slot);
    juce::String sketchName (int slot) const;
    std::atomic<int> currentSketch { -1 };
    void mutate();
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

    // the song, rebuilt on the message thread when its settings change; the audio thread takes a copy
    mutable juce::SpinLock songLock;
    band::Song songShared, songAudio;
    std::atomic<uint32_t> songVersion { 1 };
    uint32_t songVersionAudio = 0;
    juce::String lastSpecKey;
    void rebuildSong();
    int64_t shift = 0;                      // auto song: NEXT SECTION moves the song along by this many bars
    // footswitch overrides on top of the song
    int64_t endOverride = band::kNever, manualFill = -band::kNever;
    int64_t halfFrom = band::kNever, halfTo = band::kNever;
    int keyOverride = 0; int64_t keyOverrideFrom = band::kNever;
    bool looping = false; int64_t loopStart = 0, loopLen = 0;
    std::atomic<int> pendingSketchStep { 0 }; std::atomic<float> pendingStrange { -1.0f }; std::atomic<bool> pendingMutate { false };
    uint32_t strangeSeed = 1, mutateStamp = 0, tapeStamp = 0;
    int64_t manualCountFrom = band::kNever, manualCountTo = band::kNever, manualIntroFrom = band::kNever, manualIntroTo = band::kNever;

    // ---- learning
    mutable juce::SpinLock learnLock;
    Learned learned;
    ut::ChordDetector ears;
    std::vector<float> mono;
    int learnState = 0, learnSec = 0;      // 0 idle, 1 waiting for the first chord, 2 recording
    int64_t accBar = -band::kNever, learnSwitchAt = band::kNever;
    float accChroma[12] {}, accBass[12] {}, accVotes[24] {};
    bool accHeard = false;
    std::array<bool, 128> held {};
    struct LearnBuf { int bars = 0; uint8_t root[64] {}; int8_t q[64] {}; };
    LearnBuf lb[band::NumSec];
    bool lbHas[band::NumSec] {};
    LearnBuf pendingLb[band::NumSec]; bool pendingHas[band::NumSec] {};
    std::atomic<bool> learnPending { false };
    int lastRoot = -1, lastQ = 0;
    void learnFinalize (int64_t b);
    void learnSwitch();
    void learnFinish (bool keep);
    void applyLearned (const LearnBuf* bufs, const bool* has);
    void saveLearned();
    void loadLearned();

    std::atomic<int> uiCmd { 0 };
    band::Leader L;
    bool wasPlaying = false;
    double lastPpqEnd = -1;
    std::array<float, bl::kNumCmds> lastTrig {};
    std::array<int, bl::kNumCmds> lastCC {};
    std::unique_ptr<mc::SongRenderer> renderer;
};
