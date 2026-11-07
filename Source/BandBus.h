#pragma once
#include <atomic>
#include <cstdint>
#include <cstring>
#include <limits>

// ---------------------------------------------------------------------------
// The band bus: a small block of shared memory on this computer that lets the
// Tea Towel suite plugins follow one song. Bandleader writes the song (key,
// chords, section, fills, drop-outs, energy); Tea Towel writes its kick
// pattern; everyone marks themselves present. Everything is optional: if the
// bus can't be opened, or no Bandleader is running, each plugin works alone.
// Bars are counted from Live's song position (bar = beat / 4), so every plugin
// agrees on bar numbers without talking.
// ---------------------------------------------------------------------------
namespace band
{
enum Who { WTeaTowel = 0, WUndertow, WLoon, WSchaltwerk, WCarousel, WTapeTwin, WLathe, WBandleader, NumWho };
enum Sec { Verse = 0, Chorus, Bridge, NumSec };
constexpr int kMaxProg = 16;
constexpr int64_t kNever = std::numeric_limits<int64_t>::max() / 4;

struct Leader
{
    int64_t heartbeatMs = 0;
    int32_t playing = 0;
    int32_t key = 9;                       // 0 = C
    int32_t chordBars = 1;
    int32_t progLen[NumSec] { 4, 4, 2 };
    int8_t progOff[NumSec][kMaxProg] {};   // semitones above the key
    int8_t progMinor[NumSec][kMaxProg] {};
    int32_t curSection = 0;
    int64_t curStartBar = 0;
    int32_t nextSection = 0;
    int64_t nextStartBar = kNever;
    int64_t fillBar = -kNever;             // the band fills at the end of this bar
    int64_t dropStart = kNever, dropEnd = kNever;
    float energy = 0.5f;
    int32_t style = -1;                    // genre style (index into band::styles()), -1 = custom
    uint32_t styleStamp = 0;               // changes every time a style is (re)applied
    float suggestedBpm = 0;

    // ---- helpers every follower uses
    int sectionAt (int64_t bar, int64_t* barIn = nullptr) const
    {
        if (bar >= nextStartBar) { if (barIn) *barIn = bar - nextStartBar; return nextSection; }
        if (barIn) *barIn = bar - curStartBar < 0 ? 0 : bar - curStartBar;
        return curSection;
    }
    void chordAt (int64_t bar, int& root, bool& minor) const
    {
        int64_t in = 0;
        const int s = sectionAt (bar, &in);
        const int len = progLen[s] < 1 ? 1 : (progLen[s] > kMaxProg ? kMaxProg : progLen[s]);
        const int i = (int) ((in / (chordBars < 1 ? 1 : chordBars)) % len);
        root = ((key + progOff[s][i]) % 12 + 12) % 12;
        minor = progMinor[s][i] != 0;
    }
    bool isOut (int64_t bar) const { return bar >= dropStart && bar < dropEnd; }
    bool fillAt (int64_t bar) const { return bar == fillBar; }
};

struct Drums
{
    int64_t heartbeatMs = 0;
    int64_t bar[4] { -kNever, -kNever, -kNever, -kNever };
    uint16_t kick[4] {};                   // bit i = kick on 16th i of that bar
};

// Opens (or creates) the shared block. Safe to call from any thread; cheap after the first call.
bool available();
void writeLeader (const Leader&);
// true if a Bandleader is running (heard from in the last 1.5 s); fills `out`
bool readLeader (Leader& out);
void publishKicks (int64_t bar, uint16_t mask);
bool readKicks (int64_t bar, uint16_t& mask);     // true if a drummer published this bar recently
void markPresent (Who);
bool isPresent (Who);
int64_t nowMs();
} // namespace band
