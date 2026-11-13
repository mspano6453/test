#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <atomic>
#include <vector>

// ---------------------------------------------------------------------------
// Carousel: a stereo looper in the spirit of the classic stereo looper pedals.
// Memory is a pool of 1-second stereo chunks shared by 10 loop slots; each loop
// is a stack of layers (the first recording + one per overdub) so undo/redo is free.
// ---------------------------------------------------------------------------
namespace cr
{
constexpr int kSlots = 10;
constexpr int kMaxLayers = 24;
constexpr int kPoolSeconds = 600;       // 10 minutes of stereo audio

enum State { Empty = 0, Recording, Playing, Overdubbing, Stopped, StoppingFade, StoppingEnd };
inline const char* stateName (int s)
{
    static const char* n[] = { "EMPTY", "RECORDING", "PLAYING", "OVERDUB", "STOPPED", "STOPPING", "STOPPING" };
    return n[s];
}

enum Action { ARecPlayDub = 1, AStop, AUndo, ARedo, ANext, APrev, AClear, AClearAll, AToggleReverse, AToggleHalf, ASelect0 = 100 };

// MIDI footswitch map (notes, any channel) - Ableton names these C3, D3, E3...
constexpr int kNoteRec = 60, kNoteStop = 62, kNoteUndo = 64, kNoteRedo = 65, kNoteRev = 67, kNoteHalf = 69, kNoteNext = 71, kNotePrev = 72, kNoteClear = 74;
// ...or CCs 80-88 (value 64+ = press), same order
constexpr int kFirstCC = 80;

struct Layer
{
    int nChunks = 0;
    int chunk[kPoolSeconds];
    uint8_t init[kPoolSeconds];
    float gain = 1.0f;
};

struct Slot
{
    int state = Empty;
    int64_t length = 0;         // samples
    int64_t syncTarget = 0;     // >0: keep recording until this length
    double pos = 0;
    int nLayers = 0, redoTop = 0;
    float fade = 1.0f;
    Layer layers[kMaxLayers];
};
} // namespace cr

class CarouselProcessor : public juce::AudioProcessor
{
public:
    CarouselProcessor();
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Carousel"; }
    bool acceptsMidi() const override { return true; }       // footswitches
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 600.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // UI -> audio
    void push (int action);

    // audio -> UI
    std::atomic<int> uiState { cr::Empty }, uiSlot { 0 }, uiLayers { 0 }, uiRedo { 0 }, uiFreeChunks { 0 };
    std::atomic<float> uiPos { 0 }, uiLenSec { 0 }, uiRecSec { 0 }, uiInLevel { 0 };
    std::atomic<uint32_t> uiSlotHas { 0 };
    std::atomic<bool> uiWaitingForBar { false }, uiLinked { false }, uiArmed { false }, uiSectionRec { false };
    double getSampleRateSafe() const { return sr; }

    // Export the current slot's loop (all layers mixed) to a WAV. Message thread only.
    juce::File exportCurrentLoop();

private:
    int meterNow = 0;

    void ensurePool();
    void handleAction (int a);
    void handleMidi (const juce::MidiMessage&);
    void checkTriggerParams();

    // memory pool
    int allocChunk();
    void freeChunk (int c) { if (freeTop < (int) freeList.size()) freeList[(size_t) freeTop++] = c; }
    void freeLayer (cr::Layer& l) { for (int i = 0; i < l.nChunks; ++i) freeChunk (l.chunk[i]); l.nChunks = 0; }
    void clearSlot (int s);
    inline float* chunkPtr (int c) { return pool.data() + (size_t) c * (size_t) chunkLen * 2; }
    float readLayer (cr::Layer& l, int64_t len, double pos, int ch);
    void writeLayer (cr::Layer& l, int64_t len, int64_t idx, float a, float b);

    // transport helpers
    void startRecording();
    void closeLoop (bool force = false);
    bool startOverdub();
    void stopSlot (bool instant);
    void selectSlot (int s);

    double sr = 44100, bpm = 120;
    bool transportPlaying = false;
    int chunkLen = 44100;
    std::vector<float> pool;
    std::vector<int> freeList;
    int freeTop = 0;
    std::vector<cr::Slot> slots;
    int cur = 0;
    bool bandActive = false, armed = false, sectionRec = false;
    juce::SpinLock poolLock;

    juce::AbstractFifo fifo { 256 };
    std::array<int, 256> queue {};
    float lastTrig[6] {};
    bool ccDown[16] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CarouselProcessor)
};
