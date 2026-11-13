#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "BandBus.h"
#include <algorithm>
#include <vector>

// ---------------------------------------------------------------------------
// The song form. Bandleader's SONG FORM settings (arrangement, count-in, intro,
// breakdown, key change, ending) become a list of parts, each so many bars long.
// The same code runs in Bandleader (to run the song live) and in every player
// when it renders the whole song to a MIDI file, so they all agree bar by bar.
// ---------------------------------------------------------------------------
namespace band
{
struct FormDef { const char* name; std::vector<int> secs; };
inline const std::vector<FormDef>& forms()
{
    static const std::vector<FormDef> f = {
        { "V C V C B C",       { Verse, Chorus, Verse, Chorus, Bridge, Chorus } },
        { "V C V C B C C",     { Verse, Chorus, Verse, Chorus, Bridge, Chorus, Chorus } },
        { "V C V C C (short)", { Verse, Chorus, Verse, Chorus, Chorus } },
        { "V V C V C B C C",   { Verse, Verse, Chorus, Verse, Chorus, Bridge, Chorus, Chorus } },
        { "V V B V (AABA)",    { Verse, Verse, Bridge, Verse } },
        { "V C V C V C",       { Verse, Chorus, Verse, Chorus, Verse, Chorus } },
        { "Verse only (jam)",  { Verse } },
    };
    return f;
}

struct Seg { int sec = Verse, part = PNone, key = 0; int64_t start = 0, bars = 8; };
struct Song
{
    std::vector<Seg> segs;
    int64_t length = 0;      // bars, one pass
    int64_t loopFrom = 0;    // where a looping song goes back to (after the count-in and intro)
    bool loops = true;
};

inline Song buildSong (const FormSpec& f)
{
    Song s;
    const auto& fd = forms()[(size_t) std::max (0, std::min ((int) forms().size() - 1, (int) f.form))];
    auto bars = [&] (int sec) { return (int64_t) std::max (1, std::min (64, (int) f.secBars[sec])); };
    std::vector<Seg> body;
    for (int sec : fd.secs) { Seg g; g.sec = sec; g.bars = bars (sec); g.part = (f.breakdown & 2) && sec == Bridge ? PHalf : PNone; body.push_back (g); }
    // the breakdown before the last chorus: the chorus chords, half-time, half as long
    int lastChorus = -1;
    for (int i = 0; i < (int) body.size(); ++i) if (body[(size_t) i].sec == Chorus) lastChorus = i;
    if ((f.breakdown & 1) && lastChorus >= 0)
    {
        Seg b; b.sec = Chorus; b.part = PHalf; b.bars = std::max<int64_t> (2, bars (Chorus) / 2);
        body.insert (body.begin() + lastChorus, b);
        ++lastChorus;
    }
    // the key change: the last run of choruses (and everything after them) go up
    if (f.keyUp != 0)
    {
        int from = lastChorus >= 0 ? lastChorus : (int) body.size() - 1;
        while (from > 0 && body[(size_t) from - 1].sec == Chorus && body[(size_t) from - 1].part == PNone) --from;
        for (int i = from; i < (int) body.size(); ++i) body[(size_t) i].key = f.keyUp;
    }
    const int lastKey = body.empty() ? 0 : body.back().key;
    if (f.ending == 2) { Seg o; o.sec = Chorus; o.part = POutro; o.key = lastKey; o.bars = bars (Chorus) * 2; body.push_back (o); }
    if (f.ending == 1) { Seg e; e.sec = body.empty() ? Chorus : body.back().sec; e.part = PEnd; e.key = lastKey; e.bars = 2; body.push_back (e); }
    if (f.countIn > 0) { Seg c; c.sec = Verse; c.part = PCount; c.bars = f.countIn; s.segs.push_back (c); }
    if (f.intro > 0)   { Seg c; c.sec = Verse; c.part = PIntro; c.bars = f.intro; s.segs.push_back (c); }
    const size_t bodyFrom = s.segs.size();
    for (auto& b : body) s.segs.push_back (b);
    int64_t at = 0;
    for (auto& g : s.segs) { g.start = at; at += g.bars; }
    s.length = at;
    s.loopFrom = bodyFrom < s.segs.size() ? s.segs[bodyFrom].start : 0;
    s.loops = f.ending == 0;
    return s;
}

// which part holds this bar (-1: the song is over); `offset` is added to the part's start for a looping song
inline int segAt (const Song& s, int64_t bar, int64_t& offset)
{
    offset = 0;
    if (s.segs.empty()) return -1;
    if (bar < 0) bar = 0;
    if (bar >= s.length)
    {
        if (! s.loops) return -1;
        const int64_t loopLen = std::max<int64_t> (1, s.length - s.loopFrom);
        const int64_t b = s.loopFrom + (bar - s.loopFrom) % loopLen;
        offset = bar - b;
        bar = b;
    }
    for (int i = 0; i < (int) s.segs.size(); ++i)
        if (bar < s.segs[(size_t) i].start + s.segs[(size_t) i].bars) return i;
    return (int) s.segs.size() - 1;
}

// set the Leader's section and song-form ranges for this bar from the song
inline void applySong (Leader& L, const Song& s, int64_t bar)
{
    L.countFrom = L.countTo = L.introFrom = L.introTo = L.halfFrom = L.halfTo = L.outroFrom = L.outroTo = kNever;
    L.endBar = L.overFrom = kNever;
    L.keyShift = 0; L.keyFrom = L.keyTo = kNever;
    L.nextStartBar = kNever;
    int64_t off = 0;
    const int i = segAt (s, bar, off);
    if (i < 0)
    {
        const auto& last = s.segs.back();
        L.curSection = last.sec; L.curStartBar = last.start;
        L.overFrom = s.length;
        L.endBar = last.part == PEnd ? last.start : kNever;
        if (last.part == POutro) { L.outroFrom = last.start; L.outroTo = last.start + last.bars; }
        L.keyShift = last.key; L.keyFrom = last.start; L.keyTo = kNever;
        return;
    }
    struct W { Seg g; int64_t a; };
    W win[3]; int n = 0;
    win[n++] = { s.segs[(size_t) i], s.segs[(size_t) i].start + off };
    for (int k = 1; k < 3; ++k)
    {
        int j = i + k; int64_t o = off;
        if (j >= (int) s.segs.size())
        {
            if (! s.loops) break;
            int first = 0; while (first < (int) s.segs.size() && s.segs[(size_t) first].start < s.loopFrom) ++first;
            j = first + (j - (int) s.segs.size()); o = off + (s.length - s.loopFrom);
            if (j >= (int) s.segs.size()) break;
        }
        win[n++] = { s.segs[(size_t) j], s.segs[(size_t) j].start + o };
    }
    const auto& cur = win[0];
    L.curSection = cur.g.sec; L.curStartBar = cur.a;
    if (n > 1)
    {
        L.nextSection = win[1].g.sec; L.nextStartBar = win[1].a;
        L.fillBar = cur.g.part == PCount ? -kNever : win[1].a - 1;
    }
    else { L.overFrom = cur.a + cur.g.bars; L.fillBar = -kNever; }
    auto widen = [] (int64_t& from, int64_t& to, int64_t a, int64_t b) { if (from >= kNever) { from = a; to = b; } else { from = std::min (from, a); to = std::max (to, b); } };
    for (int k = 0; k < n; ++k)
    {
        const auto& w = win[k];
        const int64_t a = w.a, b = w.a + w.g.bars;
        switch (w.g.part)
        {
            case PCount: widen (L.countFrom, L.countTo, a, b); break;
            case PIntro: widen (L.introFrom, L.introTo, a, b); break;
            case PHalf:  widen (L.halfFrom, L.halfTo, a, b); break;
            case POutro: widen (L.outroFrom, L.outroTo, a, b); if (k == n - 1 && ! s.loops) L.overFrom = b; break;
            case PEnd:   L.endBar = a; L.overFrom = b; break;
            default: break;
        }
    }
    // a key change running through these parts
    for (int k = 0; k < n; ++k)
        if (win[k].g.key != 0)
        {
            L.keyShift = win[k].g.key; L.keyFrom = win[k].a; L.keyTo = win[k].a + win[k].g.bars;
            for (int m = k + 1; m < n && win[m].g.key == L.keyShift; ++m) L.keyTo = win[m].a + win[m].g.bars;
            if (! s.loops && L.keyTo >= s.length) L.keyTo = kNever;   // stays up to the end
            break;
        }
}

// the meter the band plays in: Bandleader's if it's running, otherwise Live's time signature
inline int meterFrom (const juce::Optional<juce::AudioPlayHead::PositionInfo>& pos)
{
    if (pos.hasValue()) if (auto ts = pos->getTimeSignature()) return meterFromSig (ts->numerator, ts->denominator);
    return M44;
}

// the host's meter for this block
inline int hostMeter (juce::AudioPlayHead* ph)
{
    if (ph == nullptr) return M44;
    return meterFrom (ph->getPosition());
}

// a real fade-out over the outro: the plugin's whole output goes down with it
struct Fade
{
    float g = 1.0f;
    void apply (juce::AudioBuffer<float>& buf, int n, bool led, const Leader& L, juce::AudioPlayHead* ph)
    {
        float target = 1.0f;
        if (led && L.outroFrom < kNever && ph != nullptr)
            if (auto pos = ph->getPosition()) if (auto p = pos->getPpqPosition()) target = L.fadeAt (*p / meterBarPpq (L.meter));
        if (target >= 1.0f && g >= 1.0f) return;
        for (int c = 0; c < buf.getNumChannels(); ++c) buf.applyGainRamp (c, 0, n, g, target);
        g = target;
    }
};
} // namespace band
