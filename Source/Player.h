#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include "BandBus.h"

// ---------------------------------------------------------------------------
// PLAYER: a drummer that follows Live's transport and plays the kit.
// Styles x sections, with fills, endings, intensity, looseness and swing.
// Patterns are 16 steps (sixteenth notes) of one 4/4 bar.
// Velocity characters: X x o m - g  ('.' = rest)
// ---------------------------------------------------------------------------
namespace tt
{
enum PlayerStyle { StSixties = 0, StMotorik, StKraut, StCrazyHorse, StPowerPop, StBallad, NumStyles };
enum PlayerSection { SecVerse = 0, SecChorus, SecBridge, NumSections };
enum FillEvery { FillOff = 0, Fill4, Fill8, Fill16 };

inline const char* styleName (int s)
{
    static const char* n[] = { "Sixties Pop (Ringo)", "Motorik", "Kraut Funk (Can)", "Crazy Horse", "Power Pop", "Ballad" };
    return n[s];
}
inline const char* sectionName (int s)
{
    static const char* n[] = { "Verse", "Chorus", "Bridge" };
    return n[s];
}

// Key switches on any MIDI channel (an octave above the drums)
constexpr int kKeyVerse = 60, kKeyChorus = 62, kKeyBridge = 64, kKeyFill = 65, kKeyEnding = 67;

enum Lane { LK = 0, LS, LSS, LH, LO, LRD, LB, LC, LT1, LT2, LT3, LF, NumLanes };
constexpr int laneNote[NumLanes] = { 36, 38, 37, 42, 46, 51, 53, 49, 50, 48, 45, 43 };

struct Groove
{
    const char* lane[NumLanes] {};
};

struct StyleDef
{
    Groove sec[NumSections];
    Groove extra[NumSections];   // ghost notes / extra kicks added as Intensity rises
    Groove fill[2];              // second half of a bar (steps 8-15 are used)
};

inline Groove G (std::initializer_list<std::pair<Lane, const char*>> l)
{
    Groove g;
    for (auto& p : l) g.lane[p.first] = p.second;
    return g;
}

inline const std::array<StyleDef, NumStyles>& styles()
{
    static const std::array<StyleDef, NumStyles> s = []
    {
        std::array<StyleDef, NumStyles> d {};
        const char* H8 = "x.o.x.o.x.o.x.o.";
        const char* BB = "....x.......x...";
        const char* BBX = "....X.......X...";

        d[StSixties].sec[SecVerse]  = G ({ { LH, H8 }, { LK, "x.....x.x......." }, { LS, BB } });
        d[StSixties].sec[SecChorus] = G ({ { LO, H8 }, { LK, "x.....x.x.x....." }, { LS, BB } });
        d[StSixties].sec[SecBridge] = G ({ { LRD, H8 }, { LB, "x.......x......." }, { LK, "x.......x......." }, { LS, BB } });
        d[StSixties].extra[SecVerse]  = G ({ { LS, "..........g....g" }, { LK, "...........x...." } });
        d[StSixties].extra[SecChorus] = G ({ { LS, ".......g......g." }, { LK, "...x............" } });
        d[StSixties].extra[SecBridge] = G ({ { LS, "..............g." } });
        d[StSixties].fill[0] = G ({ { LS, "........x.xx...." }, { LT1, "............x..." }, { LT2, ".............x.." }, { LF, "..............xx" }, { LK, "........x......." } });
        d[StSixties].fill[1] = G ({ { LS, "........xoxoxoxx" }, { LK, "........x......." } });

        d[StMotorik].sec[SecVerse]  = G ({ { LH, H8 }, { LK, "x.......x.x....." }, { LS, BB } });
        d[StMotorik].sec[SecChorus] = G ({ { LH, "x...x...x...x..." }, { LO, "..o...o...o...o." }, { LK, "x.......x.x....." }, { LS, BB } });
        d[StMotorik].sec[SecBridge] = G ({ { LRD, H8 }, { LK, "x...x...x...x..." }, { LS, BB } });
        d[StMotorik].extra[SecVerse]  = G ({ { LH, ".-.-.-.-.-.-.-.-" } });
        d[StMotorik].extra[SecChorus] = G ({ { LK, "..........x....." }, { LS, "..............g." } });
        d[StMotorik].extra[SecBridge] = G ({ { LS, "..............g." } });
        d[StMotorik].fill[0] = G ({ { LS, "........xoxoxoxx" }, { LK, "........x......." } });
        d[StMotorik].fill[1] = G ({ { LS, "........xo......" }, { LT1, "..........xo...." }, { LT2, "............xo.." }, { LF, "..............xx" }, { LK, "........x......." } });

        d[StKraut].sec[SecVerse]  = G ({ { LH, "x-o-x-o-x-o-x-o-" }, { LK, "x..x..x...x..x.." }, { LS, "....x.......x..g" } });
        d[StKraut].sec[SecChorus] = G ({ { LH, "x-o-x-o-x-o-x-.-" }, { LO, "..............o." }, { LK, "x..x..x...x..x.." }, { LS, "....x..g....x.g." } });
        d[StKraut].sec[SecBridge] = G ({ { LF, "x..x..x...x..x.." }, { LT2, "..x...x.x...x..." }, { LK, "x.......x......." }, { LS, "....o.......o..." }, { LH, "..-...-...-...-." } });
        d[StKraut].extra[SecVerse]  = G ({ { LS, ".g.....g.g......" } });
        d[StKraut].extra[SecChorus] = G ({ { LS, ".g.......g......" }, { LK, ".......x........" } });
        d[StKraut].extra[SecBridge] = G ({ { LS, ".g....g....g...." } });
        d[StKraut].fill[0] = G ({ { LS, "........xgxgxoxx" }, { LT2, "..............x." }, { LK, "........x..x...." } });
        d[StKraut].fill[1] = G ({ { LF, "........x.xx.x.x" }, { LT2, ".........x..x.x." }, { LK, "........x......." } });

        d[StCrazyHorse].sec[SecVerse]  = G ({ { LRD, H8 }, { LK, "x.......x......." }, { LS, BBX } });
        d[StCrazyHorse].sec[SecChorus] = G ({ { LO, H8 }, { LK, "x.....x.x......." }, { LS, BBX } });
        d[StCrazyHorse].sec[SecBridge] = G ({ { LO, ".o.ox.o.x.o.x.o." }, { LC, "x..............." }, { LK, "x.....x.x.x....." }, { LS, BBX } });
        d[StCrazyHorse].extra[SecVerse]  = G ({ { LK, "..........x....." }, { LS, "...............g" } });
        d[StCrazyHorse].extra[SecChorus] = G ({ { LK, "...........x...." }, { LC, "........x......." } });
        d[StCrazyHorse].extra[SecBridge] = G ({ { LS, "...............g" } });
        d[StCrazyHorse].fill[0] = G ({ { LS, "........x.o.x..." }, { LT2, "..........x....." }, { LT3, "...........x...." }, { LF, "............x.xx" }, { LK, "........x......." } });
        d[StCrazyHorse].fill[1] = G ({ { LS, "........X...X.xx" }, { LF, "..........x...x." }, { LC, "........x......." }, { LK, "........x...x..." } });

        d[StPowerPop].sec[SecVerse]  = G ({ { LH, H8 }, { LK, "x.x.....x.x....." }, { LS, BB } });
        d[StPowerPop].sec[SecChorus] = G ({ { LO, H8 }, { LK, "x.x...x.x.x....." }, { LS, BB } });
        d[StPowerPop].sec[SecBridge] = G ({ { LF, H8 }, { LK, "x.......x......." }, { LS, "............x..." } });
        d[StPowerPop].extra[SecVerse]  = G ({ { LK, "...x.......x...." }, { LS, ".......g.......g" } });
        d[StPowerPop].extra[SecChorus] = G ({ { LS, ".......g.......g" } });
        d[StPowerPop].extra[SecBridge] = G ({ { LS, "....g.......g..." } });
        d[StPowerPop].fill[0] = G ({ { LS, "........xoxoxoxo" }, { LK, "........x...x..." } });
        d[StPowerPop].fill[1] = G ({ { LS, "........xo......" }, { LT1, "..........xo...." }, { LT2, "............xo.." }, { LF, "..............xx" }, { LK, "........x......." } });

        d[StBallad].sec[SecVerse]  = G ({ { LH, "o.-.o.-.o.-.o.-." }, { LK, "x.........x....." }, { LSS, "....o.......o..." } });
        d[StBallad].sec[SecChorus] = G ({ { LRD, H8 }, { LK, "x.........x....." }, { LS, BB } });
        d[StBallad].sec[SecBridge] = G ({ { LF, "x.......x......." }, { LK, "x.......x......." }, { LB, "x...x...x...x..." } });
        d[StBallad].extra[SecVerse]  = G ({ { LK, ".......x........" } });
        d[StBallad].extra[SecChorus] = G ({ { LK, ".......x........" }, { LS, "..............g." } });
        d[StBallad].extra[SecBridge] = G ({ { LF, "......o.......o." } });
        d[StBallad].fill[0] = G ({ { LS, "........x.xx.xxx" }, { LT1, "...........x...." }, { LF, "..............x." } });
        d[StBallad].fill[1] = G ({ { LS, "........x...xoxx" }, { LF, "..........x....." }, { LK, "........x......." } });
        return d;
    }();
    return s;
}

class GroovePlayer
{
public:
    struct Settings
    {
        bool on = false;
        int style = 0, section = 0, fillEvery = Fill8;
        float intensity = 0.5f, looseness = 0.3f, swing = 0.0f, variation = 0.5f;
        const band::Leader* link = nullptr;   // set when a Bandleader is running the song
    };

    // UI / key-switch requests (any thread)
    std::atomic<bool> requestFill { false }, requestEnding { false };
    // Read by the UI
    std::atomic<int> uiSection { 0 }, uiStep { -1 }, uiBar { 0 };
    std::atomic<bool> uiFillComing { false }, uiEnded { false }, uiRunning { false };

    void prepare (double newSr) { sr = newSr; reset(); }

    void reset()
    {
        pendingCount = 0;
        lastStep = -1;
        section = -1;
        ended = false;
        fillThisBar = false;
        fillNextBar = false;
        endingStage = 0;
        barInSection = 0;
        uiStep = -1;
        uiRunning = false;
    }

    // Handles key switches in `midi`, then adds the drummer's hits for this block to `midi`
    // (for the kit) and `out` (MIDI output, for recording). `now` = absolute sample time of block start.
    void process (juce::MidiBuffer& midi, juce::MidiBuffer& out, int numSamples, int64_t now,
                  const Settings& s, juce::AudioPlayHead* playHead)
    {
        // key switches
        for (const auto m : midi)
        {
            const auto msg = m.getMessage();
            if (! msg.isNoteOn()) continue;
            switch (msg.getNoteNumber())
            {
                case kKeyVerse:  keySection = SecVerse;  sectionFromKey = true; break;
                case kKeyChorus: keySection = SecChorus; sectionFromKey = true; break;
                case kKeyBridge: keySection = SecBridge; sectionFromKey = true; break;
                case kKeyFill:   requestFill = true; break;
                case kKeyEnding: requestEnding = true; break;
                default: break;
            }
        }
        if (s.section != lastParamSection) { lastParamSection = s.section; sectionFromKey = false; }
        const int wanted = sectionFromKey ? keySection : s.section;

        bool playing = false;
        double ppq = 0, bpm = 120;
        if (playHead != nullptr)
            if (auto pos = playHead->getPosition())
            {
                playing = pos->getIsPlaying();
                if (auto p = pos->getPpqPosition()) ppq = *p;
                if (auto b = pos->getBpm()) bpm = *b;
            }

        if (! s.on || ! playing)
        {
            if (uiRunning) reset();
            flush (midi, out, numSamples, now);
            return;
        }
        uiRunning = true;

        const double ppqPerSample = bpm / 60.0 / sr;
        const double ppqEnd = ppq + ppqPerSample * numSamples;
        const double stepLenSamples = 0.25 / ppqPerSample;

        // a jump in the timeline (loop or relocate) restarts the phrase
        if (lastStep >= 0 && (std::floor (ppq * 4.0) < lastStep || std::floor (ppq * 4.0) > lastStep + 2))
        {
            barInSection = 0;
            fillThisBar = fillNextBar = false;
        }

        for (int64_t k = (int64_t) std::ceil (ppq * 4.0 - 1.0e-9); (double) k * 0.25 < ppqEnd; ++k)
        {
            const double stepPpq = (double) k * 0.25;
            const int offset = juce::jlimit (0, numSamples - 1, (int) ((stepPpq - ppq) / ppqPerSample));
            const int step = (int) (((k % 16) + 16) % 16);
            const int64_t bar = (k - step) / 16;
            lastStep = k;
            int w = wanted;
            // following the Bandleader: this bar's section on the downbeat, the coming one after it
            if (s.link != nullptr) w = s.link->sectionAt (step == 0 ? bar : bar + 1);
            playStep (step, now + offset, stepLenSamples, s, w, bar);
        }

        flush (midi, out, numSamples, now);
    }

private:
    struct Pending { int64_t t; uint8_t note, vel; bool on; };
    static constexpr int kMaxPending = 512;
    Pending pending[kMaxPending];
    int pendingCount = 0;

    double sr = 44100.0;
    int64_t lastStep = -1;
    int section = -1, keySection = 0, lastParamSection = -1;
    bool sectionFromKey = false;
    bool ended = false, fillThisBar = false, fillNextBar = false;
    int endingStage = 0;          // 0 none, 1 ending fill this bar, 2 final hit next bar
    int barInSection = 0;
    int fillAlt = 0;
    uint32_t rng = 0x1234567u;

    float rand01() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (float) (rng & 0xffffff) / 16777216.0f; }
    float gauss() { return (rand01() + rand01() + rand01() - 1.5f) * 1.41f; }

    static float velOf (char c)
    {
        switch (c)
        {
            case 'X': return 120; case 'x': return 102; case 'o': return 82;
            case 'm': return 66;  case '-': return 52;  case 'g': return 34; default: return 0;
        }
    }

    void push (int64_t t, int note, float vel)
    {
        if (pendingCount + 2 > kMaxPending) return;
        const uint8_t v = (uint8_t) juce::jlimit (1, 127, (int) std::lround (vel));
        pending[pendingCount++] = { t, (uint8_t) note, v, true };
        pending[pendingCount++] = { t + (int64_t) (0.05 * sr), (uint8_t) note, 0, false };
    }

    void flush (juce::MidiBuffer& midi, juce::MidiBuffer& out, int numSamples, int64_t now)
    {
        int w = 0;
        for (int i = 0; i < pendingCount; ++i)
        {
            const auto& p = pending[i];
            if (p.t < now + numSamples)
            {
                const int off = (int) juce::jlimit<int64_t> (0, numSamples - 1, p.t - now);
                const auto m = p.on ? juce::MidiMessage::noteOn (10, p.note, (juce::uint8) p.vel)
                                    : juce::MidiMessage::noteOff (10, p.note);
                if (p.on) midi.addEvent (m, off);
                out.addEvent (m, off);
            }
            else pending[w++] = p;
        }
        pendingCount = w;
    }

    void playStep (int step, int64_t t, double stepLen, const Settings& s, int wanted, int64_t bar)
    {
        uiStep = step;
        const auto& st = styles()[(size_t) juce::jlimit (0, NumStyles - 1, s.style)];

        if (step == 0)
        {
            // new bar
            bool sectionStart = false;
            if (section < 0) { section = wanted; barInSection = 0; sectionStart = true; ended = false; }
            if (endingStage == 2)
            {
                // the final hit, then silence until something changes
                hit (laneNote[LC], 124, t, s, LC);
                hit (laneNote[LK], 124, t, s, LK);
                hit (laneNote[LF], 110, t, s, LF);
                endingStage = 0;
                ended = true;
                uiEnded = true;
                fillThisBar = false;
                return;
            }
            if (ended)
            {
                if (wanted != section || requestFill.exchange (false)) { ended = false; uiEnded = false; section = wanted; barInSection = 0; sectionStart = true; }
                else return;
            }
            if (wanted != section) { section = wanted; barInSection = 0; sectionStart = true; }
            else if (! sectionStart) ++barInSection;
            if (s.link != nullptr)
            {
                int64_t in = 0; s.link->sectionAt (bar, &in); barInSection = (int) in;
                const bool out = s.link->isOut (bar);
                if (wasOut && ! out) sectionStart = true;      // everybody back in: crash
                wasOut = outBar = out;
                if (s.link->fillAt (bar)) fillNextBar = true;  // the band fills at the end of this bar
            }
            else outBar = wasOut = false;

            fillThisBar = fillNextBar;
            const bool requested = fillNextBar;
            fillNextBar = false;
            const int every = s.fillEvery == Fill4 ? 4 : s.fillEvery == Fill8 ? 8 : s.fillEvery == Fill16 ? 16 : 0;
            const int phraseLen = every > 0 ? every : 8;
            barsLeftInPhrase = phraseLen - 1 - (barInSection % phraseLen);
            if (every > 0 && barsLeftInPhrase == 0) fillThisBar = true;
            if (endingStage == 1) { fillThisBar = true; endingStage = 2; }
            if (fillThisBar) chooseFill (s, ! requested && endingStage == 0);

            // the drummer's feel drifts a little from bar to bar
            const float V = s.variation;
            energy += (1.0f - energy) * 0.25f + gauss() * 0.035f * V;
            energy = juce::jlimit (0.88f, 1.12f, energy);
            timeBiasMs = juce::jlimit (-3.0f, 3.0f, timeBiasMs * 0.85f + gauss() * 0.7f * (0.3f + V));
            hatAccent = 1.0f + (rand01() - 0.3f) * 0.35f * (0.3f + V);

            uiSection = section;
            uiBar = barInSection + 1;

            if (sectionStart && (barInSection == 0 || s.link != nullptr) && ! outBar)
                hit (laneNote[LC], 112 + 10 * s.intensity, t, s, LC);

            // tell the bass player where the kicks are (this bar, and a guess at the next)
            {
                auto mask = [&] (int sec, bool fill)
                {
                    uint16_t m = 0;
                    const Groove& gg = st.sec[juce::jlimit (0, NumSections - 1, sec)];
                    for (int i = 0; i < 16; ++i)
                    {
                        const bool inF = fill && i >= fillStart;
                        const char* lane = inF ? (useGenFill ? nullptr : st.fill[fillAlt].lane[LK]) : gg.lane[LK];
                        const char c = inF && useGenFill ? genFill[LK][i] : (lane != nullptr ? lane[i] : '.');
                        if (c != '.') m |= (uint16_t) (1u << i);
                    }
                    return m;
                };
                const uint16_t thisBar = outBar ? (fillThisBar ? (uint16_t) (mask (section, true) & (0xFFFFu << fillStart)) : (uint16_t) 0) : mask (section, fillThisBar);
                band::publishKicks (bar, thisBar);
                const int nextSec = s.link != nullptr ? s.link->sectionAt (bar + 1) : section;
                band::publishKicks (bar + 1, s.link != nullptr && s.link->isOut (bar + 1) ? (uint16_t) 0 : mask (nextSec, false));
            }

            pendingSectionStart = sectionStart;
        }

        if (ended || section < 0) return;
        // the Bandleader called a fill after this bar had started: take it if there's room
        if (s.link != nullptr && step > 0 && step <= 8 && ! fillThisBar && s.link->fillAt (bar)) { fillThisBar = true; chooseFill (s, false); }
        if (outBar && ! (fillThisBar && step >= fillStart && s.link != nullptr && ! s.link->isOut (bar + 1))) return;   // dropped out (but play the fill that brings everyone back)

        // requests land on the second half of the current bar if there's time, otherwise the next bar
        if (requestFill.exchange (false)) { if (step < 8) { fillThisBar = true; chooseFill (s, false); } else fillNextBar = true; }
        if (requestEnding.exchange (false))
        {
            if (step < 8) { fillThisBar = true; chooseFill (s, false); endingStage = 2; }
            else          { endingStage = 1; }
        }
        // a section change coming at the next bar gets a fill if there's room
        if (step == 8 && wanted != section && ! fillThisBar) { fillThisBar = true; chooseFill (s, false); }

        uiFillComing = fillThisBar || fillNextBar || endingStage > 0;

        const bool inFill = fillThisBar && step >= fillStart;
        nowInFill = inFill;
        const Groove& g = st.sec[section];
        const Groove& ex = st.extra[section];
        const float V = s.variation;

        const float build = (barsLeftInPhrase == 0 || fillThisBar) ? 1.05f : 1.0f;
        const float velScale = (0.78f + 0.34f * s.intensity) * energy * build;
        const float swingDelay = (step % 2 == 1) ? (float) (s.swing * 0.333 * stepLen) : 0.0f;

        for (int l = 0; l < NumLanes; ++l)
        {
            if (step == 0 && pendingSectionStart && (l == LH || l == LO || l == LRD) && ! inFill) continue;  // crash replaces cymbal
            char c;
            int lane = l;
            if (inFill)
            {
                c = useGenFill ? genFill[l][step] : (st.fill[fillAlt].lane[l] != nullptr ? st.fill[fillAlt].lane[l][step] : '.');
                // keep time on the hats/ride through a short fill's first steps
                if (c == '.' && step < 8 && g.lane[l] != nullptr && (l == LH || l == LRD || l == LO)) c = g.lane[l][step];
            }
            else
            {
                c = g.lane[l] != nullptr ? g.lane[l][step] : '.';
                if (c == '.' && ex.lane[l] != nullptr && ex.lane[l][step] != '.' && rand01() < (s.intensity - 0.35f) * 1.6f)
                    c = ex.lane[l][step];

                // little things a real drummer does that a loop never would
                if (l == LH && c != '.' && step % 4 == 2 && rand01() < 0.03f * V) c = '.';                  // drops a weak hat
                if (l == LH && c != '.' && step == 14 && rand01() < 0.07f * V) lane = LO;                    // opens the hat
                if (l == LS && c == '.' && (step % 2 == 1) && rand01() < 0.05f * V * (0.4f + s.intensity)) c = 'g';   // ghost note
                if (l == LK && c == '.' && (step == 3 || step == 7 || step == 10 || step == 14)
                    && g.lane[LS] != nullptr && g.lane[LS][step] == '.' && rand01() < 0.025f * V) c = 'o';  // extra kick
                if (l == LS && c == '.' && step == 15 && barsLeftInPhrase == 0 && rand01() < 0.4f * V) c = '-';  // pickup into the next phrase
            }
            if (c == '.') continue;
            float v = velOf (c) * velScale * (1.0f + gauss() * 0.04f);
            if (l == LH || l == LO || l == LRD) v *= (step % 4 == 0) ? hatAccent : 1.0f / hatAccent;
            if (s.intensity < 0.25f && (l == LH || l == LRD) && (step % 4) == 2) v *= 0.7f;
            hit (laneNote[lane], v, t + (int64_t) swingDelay, s, l);
        }
        if (step == 15) pendingSectionStart = false;
    }

    bool pendingSectionStart = false, outBar = false, wasOut = false;
    bool nowInFill = false, useGenFill = false;
    int fillStart = 8, barsLeftInPhrase = 7;
    float energy = 1.0f, timeBiasMs = 0.0f, hatAccent = 1.0f;
    char genFill[NumLanes][16] {};

    // Pick this bar's fill: one of the style's own fills, or one made up on the spot.
    // Auto fills are sometimes just the last beat; requested ones always take half a bar.
    void chooseFill (const Settings& s, bool mayBeShort)
    {
        const float V = s.variation;
        fillStart = (mayBeShort && rand01() < 0.35f * V) ? 12 : 8;
        const float r = rand01();
        if (r < 1.0f - 0.6f * V) { useGenFill = false; fillAlt = rand01() < 0.5f ? 0 : 1; return; }

        useGenFill = true;
        for (auto& lane : genFill) for (auto& c : lane) c = '.';
        // snare -> rack tom(s) -> floor tom, sixteenths or eighths
        const int lanesSeq[] = { LS, LS, LT1, LT2, LT2, LT3, LF, LF };
        const bool sixteenths = rand01() < 0.45f + 0.4f * s.intensity;
        const int len = 16 - fillStart;
        for (int i = 0; i < len; ++i)
        {
            const int step = fillStart + i;
            if (! sixteenths && (i % 2 == 1)) continue;
            if (sixteenths && i > 0 && i < len - 1 && rand01() < 0.15f * V) continue;   // leave a gap now and then
            const int lane = lanesSeq[juce::jlimit (0, 7, (int) ((float) i / (float) len * 8.0f + (rand01() - 0.5f) * 1.5f))];
            genFill[lane][step] = i >= len - 2 ? 'x' : (i < 2 ? 'o' : (rand01() < 0.5f ? 'x' : 'o'));
        }
        genFill[LK][fillStart] = 'x';
        if (rand01() < 0.5f) genFill[LK][15] = 'o';
    }

    float hash01 (int step, int lane) const
    {
        uint32_t h = (uint32_t) (step * 73856093) ^ (uint32_t) (lane * 19349663) ^ (uint32_t) (barInSection * 83492791);
        h ^= h >> 13; h *= 0x5bd1e995; h ^= h >> 15;
        return (float) (h & 0xffff) / 65536.0f;
    }

    void hit (int note, float vel, int64_t t, const Settings& s, int lane)
    {
        const float L = s.looseness;
        // a few ms behind the grid on average, drifting from bar to bar, rushing a touch in fills
        double lagMs = 3.0 + 8.0 * L + timeBiasMs;
        if (lane == LS) lagMs += 6.0 * L;
        if (lane == LC) lagMs += 4.0 * L;
        if (lane == LK) lagMs -= 1.5;
        if (nowInFill)  lagMs -= 2.5;
        lagMs += gauss() * (1.2 + 6.0 * L);
        lagMs = std::max (0.0, lagMs);
        vel += gauss() * (3.0f + 8.0f * L);
        const int64_t when = t + (int64_t) (lagMs * 0.001 * sr);
        // sloppy flam on hard backbeats when it's really loose
        if (lane == LS && vel > 100 && L > 0.6f && rand01() < L * 0.6f)
            push (when, note, vel * 0.45f), push (when + (int64_t) (0.018 * sr), note, vel);
        else
            push (when, note, vel);
    }
};
} // namespace tt
