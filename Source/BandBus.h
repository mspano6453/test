#pragma once
#include <atomic>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>
#include <limits>

// ---------------------------------------------------------------------------
// The band bus: a small block of shared memory on this computer that lets the
// DEMOCRAZY plugins follow one song. Bandleader writes the song (key,
// chords, section, fills, drop-outs, energy); Tea Towel writes its kick
// pattern; everyone marks themselves present. Everything is optional: if the
// bus can't be opened, or no Bandleader is running, each plugin works alone.
// Bars are counted from Live's song position (bar = beats / beats-per-bar of the
// song's meter), so every plugin agrees on bar numbers without talking.
// ---------------------------------------------------------------------------
namespace band
{
enum Who { WTeaTowel = 0, WUndertow, WLoon, WSchaltwerk, WCarousel, WTapeTwin, WLathe, WBandleader, WParlour, WEther, WChamber, WJangle, WHarmonia, WSwirl, NumWho };
enum Sec { Verse = 0, Chorus, Bridge, NumSec };
constexpr int kMaxProg = 32;
// chord qualities carried on the bus (progMinor holds one of these)
enum Quality { QMaj = 0, QMin, QDom7, QMaj7, QMin7, QDim, QSus4, QMin7b5, NumQualities };
inline bool qualityIsMinor (int q) { return q == QMin || q == QMin7 || q == QDim || q == QMin7b5; }
inline const char* qualitySuffix (int q) { static const char* s[] = { "", "m", "7", "maj7", "m7", "dim", "sus4", "m7b5" }; return (q >= 0 && q < NumQualities) ? s[q] : ""; }
// chord tones as semitones above the root (up to 4)
inline int qualityTones (int q, int* out)
{
    static const int t[NumQualities][4] = { { 0, 4, 7, -1 }, { 0, 3, 7, -1 }, { 0, 4, 7, 10 }, { 0, 4, 7, 11 }, { 0, 3, 7, 10 }, { 0, 3, 6, -1 }, { 0, 5, 7, -1 }, { 0, 3, 6, 10 } };
    const int qq = (q >= 0 && q < NumQualities) ? q : 0; int n = 0;
    for (int i = 0; i < 4; ++i) if (t[qq][i] >= 0) out[n++] = t[qq][i];
    return n;
}
constexpr int64_t kNever = std::numeric_limits<int64_t>::max() / 4;

// ---- the meter. Every bar is counted in 16th notes: 16 in 4/4, 12 in 3/4 and 6/8.
// 3/4 has three beats of four 16ths; 6/8 has two beats (dotted quarters) of six.
// 5/4 and 7/8 are "odd": every player writes its 4/4 part and plays it through a step map.
// 5/4 = three beats then two (4/4's first three beats, then its last two); 7/8 = 2+2+3 eighths (4/4 less its last eighth).
enum Meter { M44 = 0, M34, M68, M54, M78, NumMeters };
inline int meterSteps (int m)      { return m == M44 ? 16 : m == M54 ? 20 : m == M78 ? 14 : 12; }
inline double meterBarPpq (int m)  { return meterSteps (m) / 4.0; }       // quarter notes per bar
inline int meterBeats (int m)      { return m == M44 ? 4 : m == M34 ? 3 : m == M68 ? 2 : m == M54 ? 5 : 3; }
inline int meterBeatSteps (int m)  { return m == M68 ? 6 : 4; }
inline bool meterTriple (int m)    { return m == M34 || m == M68; }
inline bool meterOdd (int m)       { return m == M54 || m == M78; }
// the 4/4 step a bar of this meter plays at step s (the same step in 4/4, 3/4 and 6/8)
inline int oddSrc (int m, int s)   { return m == M54 ? (s < 12 ? s : s - 4) : s; }
// the meter a player writes its part in: odd meters write 4/4
inline int writeMeter (int m)      { return meterOdd (m) ? M44 : m; }
inline int patternSteps (int m)    { return meterSteps (writeMeter (m)); }
inline const char* meterName (int m) { static const char* n[] = { "4/4", "3/4", "6/8", "5/4", "7/8" }; return (m >= 0 && m < NumMeters) ? n[m] : "4/4"; }
inline int meterFromSig (int num, int den)
{
    if (num == 3 && den == 4) return M34;
    if ((num == 6 && den == 8) || (num == 3 && den == 8)) return M68;
    if (num == 5 && den == 4) return M54;
    if (num == 7 && den == 8) return M78;
    return M44;
}
// a part written as 4/4 hits over `bars` bars (each 16 steps) moved onto the odd meter's steps.
// Works for anything with an int `step` (and a float `len` in 16ths, which is kept).
template <typename H> void oddRemap (std::vector<H>& hits, int m, int bars = 1)
{
    if (! meterOdd (m)) return;
    const int ns = meterSteps (m);
    std::vector<H> out; out.reserve (hits.size() * 2);
    for (const auto& h : hits)
    {
        const int b = h.step / 16, s = h.step % 16;
        if (b >= bars) continue;
        if (m == M54)
        {
            if (s < 12) { H x = h; x.step = b * ns + s; out.push_back (x); }
            if (s >= 8) { H x = h; x.step = b * ns + s + 4; out.push_back (x); }
        }
        else if (s < ns) { H x = h; x.step = b * ns + s; out.push_back (x); }
    }
    hits.swap (out);
}
inline int64_t barOfPpq (double ppq, int m) { return (int64_t) std::floor (ppq / meterBarPpq (m) + 1.0e-9); }

// ---- song parts the Bandleader marks on top of verse / chorus / bridge
enum Part { PNone = 0, PCount, PIntro, PHalf, POutro, PEnd, NumParts };

struct Leader
{
    int64_t heartbeatMs = 0;
    int32_t playing = 0;
    int32_t key = 9;                       // 0 = C
    int32_t chordBars = 1;                 // (kept for older code; chordBarsSec is what counts)
    int32_t chordBarsSec[NumSec] { 1, 1, 1 };
    int32_t progLen[NumSec] { 4, 4, 2 };
    int8_t progOff[NumSec][kMaxProg] {};   // semitones above the key
    int8_t progMinor[NumSec][kMaxProg] {};   // chord quality (band::Quality), not just minor
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
    int32_t meter = M44;
    // the song form: ranges of bars [from, to)
    int64_t countFrom = kNever, countTo = kNever;     // count-in: the drummer clicks, everyone else waits
    int64_t introFrom = kNever, introTo = kNever;     // intro: keys and guitar first half, the rest join
    int64_t halfFrom = kNever, halfTo = kNever;       // half-time breakdown
    int64_t outroFrom = kNever, outroTo = kNever;     // fade-out
    int64_t endBar = kNever;                          // the final chord lands on this bar's downbeat
    int64_t overFrom = kNever;                        // the song is over from this bar on: silence
    int32_t keyShift = 0; int64_t keyFrom = kNever, keyTo = kNever;   // key change
    int32_t tonicQ = 0;                               // the quality of the last chord (the home chord)
    uint32_t benched = 0;                             // one bit per band member (Who) that Bandleader has switched off
    bool off (int who) const { return ((benched >> who) & 1u) != 0; }
    int32_t learning = 0;                             // Bandleader is learning the song: the band lays out
    uint32_t mutateStamp = 0;                         // MUTATE: every player rewrites its part a little
    uint32_t tapeStamp = 0;                           // TAPE STOP footswitch: Lathe stops the tape (or starts it again)
    float strange = 0;                                // how many chords Bandleader swapped for rare ones (0..1)

    // ---- helpers every follower uses
    int sectionAt (int64_t bar, int64_t* barIn = nullptr) const
    {
        if (bar >= nextStartBar) { if (barIn) *barIn = bar - nextStartBar; return nextSection; }
        if (barIn) *barIn = bar - curStartBar < 0 ? 0 : bar - curStartBar;
        return curSection;
    }
    int keyAt (int64_t bar) const { return ((key + (bar >= keyFrom && bar < keyTo ? keyShift : 0)) % 12 + 12) % 12; }
    int progIndexAt (int64_t bar, int& s) const
    {
        int64_t in = 0;
        s = sectionAt (bar, &in);
        const int len = progLen[s] < 1 ? 1 : (progLen[s] > kMaxProg ? kMaxProg : progLen[s]);
        const int cb = chordBarsSec[s] < 1 ? 1 : chordBarsSec[s];
        return (int) ((in / cb) % len);
    }
    void chordAt (int64_t bar, int& root, bool& minor) const
    {
        if (isEnding (bar)) { root = keyAt (bar); minor = qualityIsMinor (tonicQ); return; }
        int s = 0; const int i = progIndexAt (bar, s);
        root = ((keyAt (bar) + progOff[s][i]) % 12 + 12) % 12;
        minor = qualityIsMinor (progMinor[s][i]);
    }
    int qualityAt (int64_t bar) const
    {
        if (isEnding (bar)) return tonicQ;
        int s = 0; const int i = progIndexAt (bar, s);
        return progMinor[s][i];
    }
    bool isOut (int64_t bar) const { return (bar >= dropStart && bar < dropEnd) || learning != 0; }
    bool fillAt (int64_t bar) const { return bar == fillBar; }
    // ---- the song form
    bool isCountIn (int64_t bar) const { return bar >= countFrom && bar < countTo; }
    bool isHalf (int64_t bar) const    { return bar >= halfFrom && bar < halfTo; }
    bool isEnding (int64_t bar) const  { return bar >= endBar && bar < overFrom; }    // the bars the last chord rings
    bool isFinalHit (int64_t bar) const { return bar == endBar; }
    bool isOver (int64_t bar) const    { return bar >= overFrom; }
    // 0 = not the intro, 1 = its first half (keys and guitar), 2 = its second half (drums and bass join)
    int introStage (int64_t bar) const
    {
        if (bar < introFrom || bar >= introTo) return 0;
        const int64_t len = introTo - introFrom;
        return (len < 2 || bar - introFrom >= len / 2) ? 2 : 1;
    }
    // a player who isn't in the count-in, hasn't come in yet, or the song is over: rests this bar.
    // `rhythm` players (drums, bass) come in halfway through the intro; `late` ones (strings, voices) at the verse
    bool rests (int64_t bar, bool rhythm, bool late = false) const
    {
        if (isCountIn (bar) || isOver (bar) || isOut (bar)) return true;
        const int st = introStage (bar);
        if (st == 1 && (rhythm || late)) return true;
        if (st == 2 && late) return true;
        return false;
    }
    // a fade-out: 1 until the outro, then down to nothing at its end (smooth, by position in beats)
    float fadeAt (double barPos) const
    {
        if (outroFrom >= kNever || barPos < (double) outroFrom) return 1.0f;
        const double len = (double) (outroTo - outroFrom);
        const double x = len > 0 ? (barPos - (double) outroFrom) / len : 1.0;
        if (x >= 1.0) return 0.0f;
        const double g = 1.0 - x;
        return (float) (g * g);
    }
};

struct Drums
{
    int64_t heartbeatMs = 0;
    int64_t bar[4] { -kNever, -kNever, -kNever, -kNever };
    uint16_t kick[4] {};                   // bit i = kick on 16th i of that bar
};

// ---- the song form settings (Bandleader's SONG FORM panel). SongForm.h turns these into bars.
struct FormSpec
{
    int32_t form = 0;                   // which arrangement (see SongForm.h)
    int32_t secBars[NumSec] { 8, 8, 8 };
    int32_t countIn = 0;                // bars
    int32_t intro = 0;                  // bars
    int32_t ending = 0;                 // 0 keep going (loop), 1 big ending, 2 fade out
    int32_t keyUp = 0;                  // semitones up for the last chorus
    int32_t breakdown = 0;              // 0 off, 1 before the last chorus, 2 half-time bridge, 3 both
};

// ---- "Render song": Bandleader posts a job; every player renders its whole part to a .mid
struct RenderJob
{
    uint32_t stamp = 0;                 // changes for every new render
    int32_t totalBars = 0;
    float bpm = 120;
    Leader song;                        // harmony, style, meter (the form's bar ranges are worked out per bar)
    FormSpec spec;
    char folder[1024] {};               // UTF-8 path to write the files into
};
enum RenderState { RIdle = 0, RBusy, RDone, RFailed };
void postRender (const RenderJob&);
bool readRender (RenderJob&);
void setRenderState (Who, int);
int renderState (Who);
void resetRenderStates();
bool renderKicksDone();
void setRenderKicksDone (bool);

// A thread that's rendering a song offline sees this instead of the live bus.
struct Sandbox
{
    Leader leader;
    bool active = false;
};
void enterSandbox (Sandbox*);           // this thread only; nullptr to leave
bool inSandbox();

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
