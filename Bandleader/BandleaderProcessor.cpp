#include "../Source/BandBus.h"
#include "BandleaderProcessor.h"
#include "BandleaderEditor.h"

using namespace bl;

namespace
{
const juce::StringArray& onOff() { static const juce::StringArray s { "Off", "On" }; return s; }
const char* kTrigIds[kNumCmds] = { "t_next", "t_fill", "t_drop", "t_allin", "t_verse", "t_chorus", "t_bridge", "t_end", "t_half", "t_keyup", "t_learn",
                                   "t_strange_up", "t_strange_down", "t_mutate", "t_loop", "t_tape", "t_sketch_next", "t_sketch_prev" };
juce::StringArray progMenu() { auto a = ut::progressionNames(); a.add ("Learned"); return a; }
juce::StringArray formMenu() { juce::StringArray a; for (auto& f : band::forms()) a.add (f.name); return a; }

// shift every bar number the song gave the Leader by `d`
void shiftLeader (band::Leader& L, int64_t d)
{
    if (d == 0) return;
    auto sh = [d] (int64_t& v) { if (v < band::kNever && v > -band::kNever) v += d; };
    sh (L.curStartBar); sh (L.nextStartBar); sh (L.fillBar);
    sh (L.countFrom); sh (L.countTo); sh (L.introFrom); sh (L.introTo); sh (L.halfFrom); sh (L.halfTo);
    sh (L.outroFrom); sh (L.outroTo); sh (L.endBar); sh (L.overFrom); sh (L.keyFrom); sh (L.keyTo);
}

// the best chord for a bar of held keyboard notes (any of the qualities the band knows)
void bestChord (const float* chroma, const float* bass, int& root, int& q)
{
    float best = -1e9f; root = -1; q = 0;
    float total = 0; for (int i = 0; i < 12; ++i) total += chroma[i];
    if (total <= 0) return;
    for (int r = 0; r < 12; ++r)
        for (int qq = 0; qq < band::NumQualities; ++qq)
        {
            int t[4]; const int n = band::qualityTones (qq, t);
            bool in[12] {}; float s = 0;
            for (int i = 0; i < n; ++i) { in[(r + t[i]) % 12] = true; s += chroma[(r + t[i]) % 12] / total; }
            for (int i = 0; i < 12; ++i) if (! in[i]) s -= 0.6f * chroma[i] / total;
            // every chord tone should really be there
            for (int i = 0; i < n; ++i) if (chroma[(r + t[i]) % 12] < 0.15f * total / (float) n) s -= 0.25f;
            s += 0.25f * bass[r] - 0.04f * (float) (n - 3) - (qq == band::QSus4 || qq == band::QDim || qq == band::QMin7b5 ? 0.03f : 0.0f);
            if (s > best) { best = s; root = r; q = qq; }
        }
}
}

juce::AudioProcessorValueTreeState::ParameterLayout BandleaderProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> ps;
    auto C = [&] (const char* id, const char* name, juce::StringArray items, int def)
    { ps.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def)); };
    C ("style", "Style", band::styleNames(), 0);
    C ("key", "Key", { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 9);
    C ("prog_verse", "Verse Chords", progMenu(), 0);
    C ("prog_chorus", "Chorus Chords", progMenu(), 2);
    C ("prog_bridge", "Bridge Chords", progMenu(), 5);
    C ("chord_bars", "Bars Per Chord", { "1", "2", "4" }, 0);
    C ("song", "Song", { "Auto song", "Manual" }, 0);
    C ("section_bars", "Section Length", { "4 bars", "8 bars", "16 bars", "12 bars", "As learned" }, 1);
    C ("section", "Section", { "Verse", "Chorus", "Bridge" }, 0);
    ps.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "energy", 1 }, "Energy", juce::NormalisableRange<float> (0, 1, 0.001f), 0.5f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100)) + "%"; })));
    const char* trigNames[kNumCmds] = { "Footswitch: Next Section", "Footswitch: Fill", "Footswitch: Drop Out", "Footswitch: All In",
                                        "Footswitch: Verse", "Footswitch: Chorus", "Footswitch: Bridge",
                                        "Footswitch: End Song", "Footswitch: Half-Time", "Footswitch: Key Up", "Footswitch: Learn" , "Footswitch: Strange Up", "Footswitch: Strange Down", "Footswitch: Mutate", "Footswitch: Loop Section",
                                        "Footswitch: Tape Stop", "Footswitch: Next Sketch", "Footswitch: Previous Sketch" };
    for (int i = 0; i < kNumCmds; ++i) C (kTrigIds[i], trigNames[i], onOff(), 0);
    // the song form
    C ("meter", "Time Signature", { "Follow Live", "4/4", "3/4", "6/8" }, 0);
    C ("form", "Arrangement", formMenu(), 0);
    C ("count_in", "Count-In", { "Off", "1 bar", "2 bars" }, 0);
    C ("intro", "Intro", { "Off", "2 bars", "4 bars", "8 bars" }, 0);
    C ("breakdown", "Breakdown", { "Off", "Before last chorus", "Half-time bridge", "Both" }, 0);
    C ("key_up", "Key Change", { "Off", "Last chorus +1", "Last chorus +2", "Last chorus +3" }, 0);
    C ("ending", "Ending", { "Keep going", "Big ending", "Fade out" }, 0);
    ps.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "strange", 1 }, "Strange", juce::NormalisableRange<float> (0, 1, 0.001f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100)) + "%"; })));
    return { ps.begin(), ps.end() };
}

BandleaderProcessor::BandleaderProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                                       .withInput ("Sidechain (guitar)", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Bandleader", createLayout())
{
    band::available();   // open the band link here (message thread), not on the audio thread
    lastStyleParam = (int) apvts.getRawParameterValue ("style")->load();
    songShared.segs.reserve (64); songAudio.segs.reserve (64);
    rebuildSong();
    renderer = std::make_unique<mc::SongRenderer> (*this, band::WBandleader, mc::SongRenderer::partFileName (0, "Chords"),
                                                   [] { return std::make_unique<BandleaderProcessor>(); },
                                                   [] (juce::AudioProcessor& p) { mc::setChoice (p, "song", 0); });
    startTimerHz (5);
}

BandleaderProcessor::~BandleaderProcessor() { renderer.reset(); stopTimer(); cancelPendingUpdate(); }

void BandleaderProcessor::prepareToPlay (double sr, int bs)
{
    chordOut.ensureSize (2048);
    ears.prepare (sr);
    mono.assign ((size_t) juce::jmax (512, bs), 0.0f);
}

bool BandleaderProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    const auto in = l.getMainInputChannelSet();
    return in.isDisabled() || in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

band::FormSpec BandleaderProcessor::currentSpec() const
{
    auto g = [&] (const char* id) { return (int) apvts.getRawParameterValue (id)->load(); };
    band::FormSpec f;
    f.form = g ("form");
    const int sb = g ("section_bars");
    const auto lc = learnedCopy();
    for (int s = 0; s < band::NumSec; ++s)
        f.secBars[s] = sb == 4 ? (lc.has[s] && lc.bars[s] > 0 ? lc.bars[s] : 8) : ut::sectionBarsFor (sb);
    static const int ci[] = { 0, 1, 2 }, in[] = { 0, 2, 4, 8 };
    f.countIn = ci[juce::jlimit (0, 2, g ("count_in"))];
    f.intro = in[juce::jlimit (0, 3, g ("intro"))];
    f.breakdown = g ("breakdown");
    f.keyUp = g ("key_up");
    f.ending = g ("ending");
    return f;
}

void BandleaderProcessor::rebuildSong()
{
    const auto f = currentSpec();
    juce::String k; k << f.form << "," << f.secBars[0] << "," << f.secBars[1] << "," << f.secBars[2] << "," << f.countIn << "," << f.intro << ","
                      << f.breakdown << "," << f.keyUp << "," << f.ending;
    if (k == lastSpecKey) return;
    lastSpecKey = k;
    auto s = band::buildSong (f);
    {
        const juce::SpinLock::ScopedLockType l (songLock);
        songShared.segs.clear();
        for (auto& g : s.segs) if (songShared.segs.size() < 64) songShared.segs.push_back (g);
        songShared.length = s.length; songShared.loopFrom = s.loopFrom; songShared.loops = s.loops;
    }
    ++songVersion;
}

// a command lands on the next bar line (or the one after, if this bar is nearly over)
void BandleaderProcessor::handle (Cmd c, int64_t bar, double frac)
{
    const int64_t at = bar + (frac > 0.75 ? 2 : 1);
    const bool autoSong = apvts.getRawParameterValue ("song")->load() < 0.5f;
    if (learnState != 0)
    {
        // while learning: NEXT moves on to the next part, LEARN finishes, a section button jumps to it
        if (c == CNext) { if (learnState == 2) { learnSwitchAt = frac < 0.3 ? bar : bar + 1; if (learnSwitchAt <= accBar) learnSwitch(); } else learnSwitch(); }
        else if (c == CLearn)
        {
            // the bar just played (or the one being played, if DONE came late in it) still counts
            if (accBar > -band::kNever && (frac >= 0.3 || accBar < bar)) learnFinalize (accBar);
            accBar = bar;
            if (learnState != 0) learnFinish (true);
        }
        else if (c == CLearnCancel) learnFinish (false);
        else if (c >= CVerse && c <= CBridge) { learnFinish (true); learnState = 1; learnSec = c - CVerse; lbHas[learnSec] = false; lb[learnSec].bars = 0; lastRoot = -1; }
        return;
    }
    auto goTo = [&] (int sec)
    {
        L.nextSection = sec; L.nextStartBar = at; L.fillBar = at - 1;
        if (L.dropStart < at && L.dropEnd > at) L.dropEnd = at;   // a section change brings everyone back
    };
    switch (c)
    {
        case CNext:
            if (autoSong)
            {
                // move the song on so that the next part starts at the coming bar line
                int64_t off = 0;
                const int i = band::segAt (songAudio, bar - shift, off);
                if (i >= 0)
                {
                    int64_t ns = songAudio.segs[(size_t) i].start + songAudio.segs[(size_t) i].bars + off;
                    if (! songAudio.loops && i + 1 >= (int) songAudio.segs.size()) break;
                    shift = at - ns;
                    if (L.dropStart < at && L.dropEnd > at) L.dropEnd = at;
                }
            }
            else
            {
                const int s = (L.curSection + 1) % band::NumSec;
                postParams (s, false);
                goTo (s);
            }
            break;
        case CFill:   manualFill = frac < 0.5 ? bar : bar + 1; break;
        case CDrop:   L.dropStart = at; L.dropEnd = band::kNever; break;
        case CAllIn:  if (L.dropStart < band::kNever) { L.dropEnd = at; manualFill = at - 1; } break;
        case CVerse: case CChorus: case CBridge:
            postParams (c - CVerse, true);
            if (autoSong) { L.curSection = c - CVerse; L.curStartBar = bar; }
            goTo (c - CVerse);
            break;
        case CEnd:
            endOverride = frac < 0.5 ? bar + 1 : bar + 2;
            manualFill = endOverride - 1;
            break;
        case CHalf:
            if (halfFrom < band::kNever && (halfTo >= band::kNever || halfTo > bar)) { halfTo = at; }
            else { halfFrom = at; halfTo = band::kNever; }
            break;
        case CKeyUp:
            keyOverride = (keyOverride + 1) % 12; keyOverrideFrom = at; manualFill = at - 1;
            break;
        case CStrangeUp:   pendingStrange = juce::jmin (1.0f, apvts.getRawParameterValue ("strange")->load() + 0.1f); triggerAsyncUpdate(); break;
        case CStrangeDown: pendingStrange = juce::jmax (0.0f, apvts.getRawParameterValue ("strange")->load() - 0.1f); triggerAsyncUpdate(); break;
        case CMutate:      strangeSeed = strangeSeed * 1664525u + 1013904223u; ++mutateStamp; pendingMutate = true; triggerAsyncUpdate(); break;
        case CLoop:        looping = ! looping; loopLen = 0; break;
        case CTapeStop:    ++tapeStamp; break;
        case CSketchNext:  pendingSketchStep = 1; triggerAsyncUpdate(); break;
        case CSketchPrev:  pendingSketchStep = -1; triggerAsyncUpdate(); break;
        case CLearn:
            learnState = 1; learnSec = band::Verse; lastRoot = -1;
            for (int s = 0; s < band::NumSec; ++s) { lbHas[s] = false; lb[s].bars = 0; }
            held.fill (false);
            break;
        default: break;
    }
}

// ---------------------------------------------------------------------------
// Learning: every bar, the chord heard most (guitar through the sidechain, or
// notes held on a keyboard) goes into the part being learned.
void BandleaderProcessor::learnFinalize (int64_t b)
{
    int root = -1, q = 0;
    float chromaSum = 0; for (float v : accChroma) chromaSum += v;
    if (chromaSum > 0) bestChord (accChroma, accBass, root, q);
    else
    {
        float best = 0; int bi = -1;
        for (int i = 0; i < 24; ++i) if (accVotes[i] > best) { best = accVotes[i]; bi = i; }
        if (bi >= 0) { root = bi / 2; q = (bi % 2) ? band::QMin : band::QMaj; }
    }
    const bool heard = root >= 0;
    if (learnState == 1 && heard) learnState = 2;   // the part starts with the first bar you play
    if (learnState == 2)
    {
        if (! heard) { root = lastRoot; q = lastQ; }
        if (root >= 0)
        {
            auto& buf = lb[learnSec];
            if (buf.bars < 64) { buf.root[buf.bars] = (uint8_t) root; buf.q[buf.bars] = (int8_t) q; ++buf.bars; }
            lbHas[learnSec] = true;
            lastRoot = root; lastQ = q;
        }
        if (b + 1 >= learnSwitchAt) learnSwitch();
    }
    uiLearnHeard = heard ? root | (q << 4) : -1;
}

void BandleaderProcessor::learnSwitch()
{
    learnSwitchAt = band::kNever;
    if (learnSec >= band::Bridge) { learnFinish (true); return; }
    ++learnSec;
    lb[learnSec].bars = 0; lbHas[learnSec] = false;
    learnState = 2;     // keep going: the next part starts right away
}

void BandleaderProcessor::learnFinish (bool keep)
{
    if (keep && ! learnPending.load())
    {
        for (int s = 0; s < band::NumSec; ++s) { pendingLb[s] = lb[s]; pendingHas[s] = lbHas[s] && lb[s].bars > 0; }
        learnPending = true;
        triggerAsyncUpdate();
    }
    learnState = 0; learnSwitchAt = band::kNever;
}

void BandleaderProcessor::applyLearned (const LearnBuf* bufs, const bool* has)
{
    // the key: the verse's first chord; if only other parts were learned this time, keep the key there is
    Learned nl = learnedCopy();
    bool before = false; for (int s = 0; s < band::NumSec; ++s) before = before || (nl.has[s] && ! has[s]);
    int key = -1;
    if (has[band::Verse]) key = bufs[band::Verse].root[0];
    else if (before) key = (int) apvts.getRawParameterValue ("key")->load();
    else for (int s = 0; s < band::NumSec && key < 0; ++s) if (has[s]) key = bufs[s].root[0];
    if (key < 0) return;
    if (has[band::Verse] && before)
    {
        // a new key for the verse: move the parts learned before along with it
        const int oldKey = (int) apvts.getRawParameterValue ("key")->load();
        for (int s = 0; s < band::NumSec; ++s)
            if (nl.has[s] && ! has[s]) for (int i = 0; i < nl.bars[s]; ++i) nl.off[s][i] = (int8_t) (((nl.off[s][i] + oldKey - key) % 12 + 12) % 12);
    }
    for (int s = 0; s < band::NumSec; ++s)
    {
        if (! has[s]) continue;
        nl.has[s] = true;
        nl.bars[s] = juce::jmin (band::kMaxProg, bufs[s].bars);
        for (int i = 0; i < nl.bars[s]; ++i) { nl.off[s][i] = (int8_t) (((bufs[s].root[i] - key) % 12 + 12) % 12); nl.q[s][i] = bufs[s].q[i]; }
    }
    { const juce::SpinLock::ScopedLockType l (learnLock); learned = nl; }
    saveLearned();
    band::setParam (apvts, "key", (float) key);
    for (int s = 0; s < band::NumSec; ++s)
        if (has[s]) band::setParam (apvts, s == 0 ? "prog_verse" : s == 1 ? "prog_chorus" : "prog_bridge", (float) learnedIndex());
    band::setParam (apvts, "section_bars", 4.0f);
    band::setParam (apvts, "chord_bars", 0.0f);
    band::setParam (apvts, "song", 0.0f);
    band::setParam (apvts, "style", 0.0f);   // it's your song now, not a style's
    lastStyleParam = 0; styleIdx = -1;
    rebuildSong();
}

void BandleaderProcessor::saveLearned()
{
    const auto lc = learnedCopy();
    juce::String s;
    for (int sec = 0; sec < band::NumSec; ++sec)
    {
        if (sec > 0) s << ";";
        if (! lc.has[sec]) continue;
        for (int i = 0; i < lc.bars[sec]; ++i) s << (i ? "," : "") << (int) lc.off[sec][i] << "." << (int) lc.q[sec][i];
    }
    apvts.state.setProperty ("learned", s, nullptr);
}

void BandleaderProcessor::loadLearned()
{
    Learned nl;
    const auto parts = juce::StringArray::fromTokens (apvts.state.getProperty ("learned", "").toString(), ";", "");
    for (int sec = 0; sec < juce::jmin ((int) band::NumSec, parts.size()); ++sec)
    {
        const auto chords = juce::StringArray::fromTokens (parts[sec], ",", "");
        for (auto& c : chords)
        {
            if (c.isEmpty() || nl.bars[sec] >= band::kMaxProg) continue;
            nl.off[sec][nl.bars[sec]] = (int8_t) juce::jlimit (0, 11, c.upToFirstOccurrenceOf (".", false, false).getIntValue());
            nl.q[sec][nl.bars[sec]] = (int8_t) juce::jlimit (0, band::NumQualities - 1, c.fromFirstOccurrenceOf (".", false, false).getIntValue());
            ++nl.bars[sec];
        }
        nl.has[sec] = nl.bars[sec] > 0;
    }
    { const juce::SpinLock::ScopedLockType l (learnLock); learned = nl; }
}

juce::String BandleaderProcessor::learnedChords (int sec, int key) const
{
    const auto lc = learnedCopy();
    if (sec < 0 || sec >= band::NumSec || ! lc.has[sec]) return "(nothing learned yet)";
    juce::String s;
    for (int i = 0; i < lc.bars[sec]; ++i) s << ut::pcName (key + lc.off[sec][i]) << band::qualitySuffix (lc.q[sec][i]) << " ";
    return s.trim();
}

// ---------------------------------------------------------------------------
void BandleaderProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    const int n = buffer.getNumSamples();
    auto g = [&] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    double ppq = 0, bpm = 120; bool playing = false; int hostMeter = band::M44;
    if (auto* ph = getPlayHead())
    {
        const auto pos = ph->getPosition();
        if (pos.hasValue())
        {
            playing = pos->getIsPlaying();
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto p = pos->getPpqPosition()) ppq = *p; else playing = false;
        }
        hostMeter = band::meterFrom (pos);
    }
    const int meterSel = (int) g ("meter");
    const int meter = meterSel == 0 ? hostMeter : meterSel - 1;
    const double bp = band::meterBarPpq (meter);
    L.meter = meter;
    capture.setMeter (meter);

    // the guitar (sidechain) for learning; the plugin passes nothing through
    if ((learnState != 0 || tunerOn.load()) && getTotalNumInputChannels() > 0 && (int) mono.size() >= n)
    {
        const int ch = juce::jmin (2, getTotalNumInputChannels());
        for (int i = 0; i < n; ++i) { float v = 0; for (int c = 0; c < ch; ++c) v += buffer.getSample (c, i); mono[(size_t) i] = v / (float) ch; }
        if (learnState != 0) ears.push (mono.data(), n);
        if (tunerOn.load())
        {
            int w = tunerW.load();
            for (int i = 0; i < n; ++i) { tunerBuf[w] = mono[(size_t) i]; w = (w + 1) % kTunerN; }
            tunerW = w;
        }
    }
    buffer.clear();

    // the song's harmony
    const auto lc = learnedCopy();
    L.key = (int) g ("key");
    L.chordBars = 1 << (int) g ("chord_bars");
    const char* progIds[3] = { "prog_verse", "prog_chorus", "prog_bridge" };
    for (int s = 0; s < band::NumSec; ++s)
    {
        const int pi = (int) g (progIds[s]);
        if (pi >= learnedIndex() && lc.has[s])
        {
            L.progLen[s] = juce::jmax (1, lc.bars[s]); L.chordBarsSec[s] = 1;
            for (int i = 0; i < L.progLen[s]; ++i) { L.progOff[s][i] = lc.off[s][i]; L.progMinor[s][i] = lc.q[s][i]; }
        }
        else
        {
            const auto& p = ut::progressions()[(size_t) juce::jlimit (0, (int) ut::progressions().size() - 1, pi)].chords;
            L.progLen[s] = juce::jmin ((int) p.size(), band::kMaxProg); L.chordBarsSec[s] = L.chordBars;
            for (int i = 0; i < L.progLen[s]; ++i) { L.progOff[s][i] = (int8_t) p[(size_t) i].off; L.progMinor[s][i] = (int8_t) p[(size_t) i].quality(); }
        }
    }
    // STRANGE: swap chords for rare ones (the home chord is worked out from the song as written, first)
    L.tonicQ = band::QMaj;
    for (int s = 0, found = 0; s < band::NumSec && ! found; ++s)
        for (int i = 0; i < L.progLen[s]; ++i) if (L.progOff[s][i] == 0) { L.tonicQ = L.progMinor[s][i]; found = 1; break; }
    const int tonicKeep = L.tonicQ;
    L.strange = g ("strange");
    if (L.strange > 0.0f)
        for (int s = 0; s < band::NumSec; ++s)
            for (int i = 0; i < L.progLen[s]; ++i)
            {
                int o = 0, q = 0;
                band::strangeChord (L.progOff[s][i], L.progMinor[s][i], L.strange, strangeSeed, s, i, o, q);
                L.progOff[s][i] = (int8_t) o; L.progMinor[s][i] = (int8_t) q;
            }
    L.mutateStamp = mutateStamp; L.tapeStamp = tapeStamp;
    // the home chord (the last chord of a big ending): the key's own chord, as the song plays it
    L.tonicQ = band::QMaj;
    for (int s = 0, found = 0; s < band::NumSec && ! found; ++s)
        for (int i = 0; i < L.progLen[s]; ++i) if (L.progOff[s][i] == 0) { L.tonicQ = L.progMinor[s][i]; found = 1; break; }
    L.tonicQ = tonicKeep;
    if (L.tonicQ == band::QDom7) L.tonicQ = band::QMaj;
    L.energy = g ("energy");
    L.benched = benched.load();
    L.style = styleIdx.load(); L.styleStamp = styleStamp.load();
    L.suggestedBpm = L.style >= 0 ? band::styles()[(size_t) L.style].bpm : 0.0f;
    const bool autoSong = g ("song") < 0.5f;

    // take the latest song from the message thread
    if (songVersionAudio != songVersion.load())
    {
        const juce::SpinLock::ScopedTryLockType tl (songLock);
        if (tl.isLocked())
        {
            songAudio.segs.clear();
            for (auto& sg : songShared.segs) songAudio.segs.push_back (sg);
            songAudio.length = songShared.length; songAudio.loopFrom = songShared.loopFrom; songAudio.loops = songShared.loops;
            songVersionAudio = songVersion.load();
        }
    }

    const int64_t bar = band::barOfPpq (ppq, meter);
    const double frac = ppq / bp - (double) bar;

    // where are we? (start of play, or the playhead jumped)
    if (! playing || ! wasPlaying || ppq < lastPpqEnd - 0.02 || ppq > lastPpqEnd + 0.5)
    {
        shift = 0; loopLen = 0;
        endOverride = band::kNever; manualFill = -band::kNever;
        halfFrom = halfTo = band::kNever; keyOverride = 0; keyOverrideFrom = band::kNever;
        manualCountFrom = manualCountTo = manualIntroFrom = manualIntroTo = band::kNever;
        if (! autoSong)
        {
            L.curSection = (int) g ("section"); L.curStartBar = bar;
            const auto f = currentSpec();
            if (f.countIn > 0) { manualCountFrom = bar; manualCountTo = bar + f.countIn; }
            if (f.intro > 0) { manualIntroFrom = manualCountTo < band::kNever ? manualCountTo : bar; manualIntroTo = manualIntroFrom + f.intro; }
            if (manualIntroTo < band::kNever) L.curStartBar = manualIntroTo; else if (manualCountTo < band::kNever) L.curStartBar = manualCountTo;
        }
        L.nextStartBar = band::kNever; L.fillBar = -band::kNever;
        L.dropStart = L.dropEnd = band::kNever;
    }
    wasPlaying = playing;
    L.playing = playing ? 1 : 0;
    lastPpqEnd = ppq + n * bpm / 60.0 / juce::jmax (8000.0, getSampleRate());

    // commands: footswitch notes / CCs, mapped parameters, the screen; held notes for learning
    // (while learning from a keyboard, a footswitch note that's part of a chord is music, not a command)
    int noteOnsThisBlock = 0;
    for (const auto m : midi) if (m.getMessage().isNoteOn()) ++noteOnsThisBlock;
    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        int idx = -1;
        if (msg.isNoteOn())
        {
            for (int i = 0; i < kNumCmds; ++i) if (msg.getNoteNumber() == kNotes[i]) idx = i;
            if (learnState != 0 && idx >= 0)
            {
                bool othersHeld = false;
                for (int nn = 0; nn < 128; ++nn) if (held[(size_t) nn] && nn != msg.getNoteNumber()) othersHeld = true;
                if (othersHeld || noteOnsThisBlock > 1) idx = -1;
            }
            if (learnState != 0 && idx < 0) held[(size_t) msg.getNoteNumber()] = true;
        }
        else if (msg.isNoteOff()) held[(size_t) msg.getNoteNumber()] = false;
        else if (msg.isController() && msg.getControllerNumber() >= kCCBase && msg.getControllerNumber() < kCCBase + kNumCmds)
        {
            const int i = msg.getControllerNumber() - kCCBase;
            const int v = msg.getControllerValue();
            if (v >= 64 && lastCC[(size_t) i] < 64) idx = i;
            lastCC[(size_t) i] = v;
        }
        else if (msg.isAllNotesOff()) held.fill (false);
        if (idx >= 0) handle ((Cmd) (idx + 1), bar, frac);
    }
    midi.clear();
    for (int i = 0; i < kNumCmds; ++i)
    {
        const float v = g (kTrigIds[i]);
        if (v > 0.5f && lastTrig[(size_t) i] <= 0.5f) handle ((Cmd) (i + 1), bar, frac);
        lastTrig[(size_t) i] = v;
    }
    if (const int c = uiCmd.exchange (0)) handle ((Cmd) c, bar, frac);

    // manual: follow the Section parameter (automation, MIDI Map)
    if (! autoSong && playing && ! paramsPending.load() && learnState == 0)
    {
        const int want = (int) g ("section");
        const int pending = L.nextStartBar < band::kNever ? L.nextSection : L.curSection;
        if (want != pending) handle ((Cmd) (CVerse + want), bar, frac);
    }

    // ---- the song for this bar
    const auto dropS = L.dropStart, dropE = L.dropEnd;
    const bool useAuto = autoSong && ! (paramsPending.load() && pendingManual.load());
    if (useAuto && ! songAudio.segs.empty())
    {
        // PRACTICE LOOP: the part you're in plays again and again until you let it go
        if (looping && playing)
        {
            int64_t off = 0;
            if (loopLen <= 0)
            {
                const int i = band::segAt (songAudio, bar - shift, off);
                if (i >= 0) { loopStart = songAudio.segs[(size_t) i].start + off; loopLen = songAudio.segs[(size_t) i].bars; }
            }
            if (loopLen > 0) while (bar - shift >= loopStart + loopLen) shift += loopLen;
        }
        band::applySong (L, songAudio, bar - shift);
        shiftLeader (L, shift);
        if (looping && loopLen > 0)
        {
            // the next part is this part again (so the fills and the sections still line up)
            L.nextSection = L.curSection; L.nextStartBar = loopStart + loopLen + shift; L.fillBar = L.nextStartBar - 1;
            L.endBar = L.overFrom = band::kNever; L.outroFrom = L.outroTo = band::kNever;
        }
    }
    else
    {
        L.countFrom = manualCountFrom; L.countTo = manualCountTo; L.introFrom = manualIntroFrom; L.introTo = manualIntroTo;
        L.halfFrom = L.halfTo = L.outroFrom = L.outroTo = band::kNever; L.endBar = L.overFrom = band::kNever;
        L.keyShift = 0; L.keyFrom = L.keyTo = band::kNever;
        if (playing)
        {
            if (bar >= L.nextStartBar) { L.curSection = L.nextSection; L.curStartBar = L.nextStartBar; L.nextStartBar = band::kNever; }
            if (L.introTo < band::kNever && bar == L.introTo - 1 && L.fillBar < bar) L.fillBar = bar;
        }
    }
    L.dropStart = dropS; L.dropEnd = dropE;
    if (L.dropEnd <= bar) L.dropStart = L.dropEnd = band::kNever;
    if (manualFill == bar || manualFill == bar + 1) L.fillBar = manualFill;
    if (halfFrom < band::kNever) { if (halfTo <= bar) halfFrom = halfTo = band::kNever; else { L.halfFrom = halfFrom; L.halfTo = halfTo; } }
    if (keyOverride != 0)
    {
        const int planned = (bar >= L.keyFrom && bar < L.keyTo) ? L.keyShift : 0;
        L.keyShift = planned + keyOverride; L.keyFrom = keyOverrideFrom; L.keyTo = band::kNever;
    }
    if (endOverride < band::kNever)
    {
        L.endBar = endOverride; L.overFrom = endOverride + 2;
        L.outroFrom = L.outroTo = band::kNever;
        if (L.nextStartBar >= endOverride) L.nextStartBar = band::kNever;
    }

    // ---- learning: freeze the song on the part being learned, the band lays out
    if (learnState != 0)
    {
        if (bar != accBar)
        {
            if (playing && accBar > -band::kNever) learnFinalize (accBar);
            std::fill (std::begin (accChroma), std::end (accChroma), 0.0f);
            std::fill (std::begin (accBass), std::end (accBass), 0.0f);
            std::fill (std::begin (accVotes), std::end (accVotes), 0.0f);
            accBar = bar;
        }
        if (playing && learnState != 0)
        {
            int lowest = -1;
            for (int nn = 0; nn < 128; ++nn) if (held[(size_t) nn]) { accChroma[nn % 12] += (float) n; if (lowest < 0) lowest = nn; }
            if (lowest >= 0) accBass[lowest % 12] += (float) n;
            if (ears.playing()) { const auto c = ears.current(); if (c.valid()) accVotes[c.root * 2 + (c.minor ? 1 : 0)] += (float) n; }
        }
        if (learnState != 0)
        {
            L.learning = 1;
            L.curSection = learnSec; L.curStartBar = bar; L.nextStartBar = band::kNever; L.fillBar = -band::kNever;
            L.countFrom = L.countTo = L.introFrom = L.introTo = L.halfFrom = L.halfTo = L.outroFrom = L.outroTo = band::kNever;
            L.endBar = L.overFrom = band::kNever;
        }
        else L.learning = 0;
    }
    else { L.learning = 0; accBar = -band::kNever; }
    band::writeLeader (L);

    // MIDI out: the song's chords, one held chord per change (root low, the chord above) - drag or record them into a clip
    {
        juce::MidiBuffer& out = chordOut;
        out.clear();
        const double spb = getSampleRate() * 60.0 / juce::jmax (20.0, bpm);
        auto chordOff = [&] (int pos) { for (int nn : chordNotes) out.addEvent (juce::MidiMessage::noteOff (1, nn), pos); chordNotes.clear(); };
        auto emit = [&] (int64_t b, int pos)
        {
            if (L.isOut (b) || L.isCountIn (b) || L.isOver (b)) { if (! chordNotes.empty()) chordOff (pos); lastChordKey = -1; return; }
            if (L.isEnding (b) && ! L.isFinalHit (b)) return;     // the last chord rings on
            int r = 0; bool m = false; L.chordAt (b, r, m); const int q = L.qualityAt (b);
            const int key = r | (q << 4) | (L.isFinalHit (b) ? 1 << 10 : 0);
            if (key == lastChordKey) return;
            chordOff (pos);
            int t[4]; const int nt = band::qualityTones (q, t);
            chordNotes.push_back (48 + r);
            for (int i = 0; i < nt; ++i) chordNotes.push_back (60 + (r + t[i]) % 12);
            for (int nn : chordNotes) out.addEvent (juce::MidiMessage::noteOn (1, nn, (juce::uint8) 90), pos);
            lastChordKey = key;
        };
        if (playing)
        {
            if (lastChordKey < 0) emit (bar, 0);
            const double ppqEnd = ppq + n / spb;
            for (int64_t b = bar + 1; (double) b * bp < ppqEnd; ++b) emit (b, juce::jlimit (0, n - 1, (int) (((double) b * bp - ppq) * spb)));
        }
        else if (! chordNotes.empty() || lastChordKey >= 0) { chordOff (0); lastChordKey = -1; }
        midi.swapWith (out);
        capture.recordBlock (midi, ppq, playing, bpm, getSampleRate(), n);
    }

    uiBusOk = band::available();
    uiPlaying = playing;
    uiBar = bar;
    uiMeter = meter; uiHostMeter = hostMeter;
    uiSection = L.curSection;
    uiNextSection = L.nextStartBar < band::kNever ? L.nextSection : -1;
    uiBarsToNext = L.nextStartBar < band::kNever ? (int) (L.nextStartBar - bar) : -1;
    uiBarIn = (int) juce::jmax<int64_t> (0, bar - L.curStartBar) + 1;
    {
        int64_t off = 0;
        uiFormIndex = autoSong && ! songAudio.segs.empty() ? band::segAt (songAudio, bar - shift, off) : -1;
    }
    uiPart = L.isCountIn (bar) ? band::PCount : L.introStage (bar) ? band::PIntro : L.isEnding (bar) ? band::PEnd
           : (L.outroFrom <= bar && bar < L.outroTo) ? band::POutro : L.isHalf (bar) ? band::PHalf : band::PNone;
    uiKeyShift = (bar >= L.keyFrom && bar < L.keyTo) ? L.keyShift : 0;
    uiOut = playing && L.isOut (bar);
    uiOver = playing && L.isOver (bar);
    uiEndComing = playing && L.endBar < band::kNever && L.endBar > bar;
    uiHalfOn = halfFrom < band::kNever;
    uiDropComing = playing && L.dropStart > bar && L.dropStart < band::kNever;
    uiFillComing = playing && (L.fillBar == bar || L.fillBar == bar + 1);
    uiLearnState = learnState; uiLearnSec = learnSec; uiLearnBars = lb[juce::jlimit (0, 2, learnSec)].bars;
    int r; bool mn; L.chordAt (playing ? bar : 0, r, mn);
    uiChord = r | (L.qualityAt (playing ? bar : 0) << 4);   juce::ignoreUnused (mn);
    uiLooping = looping;
    {
        // the chord chart: this chord and the next four changes
        const int64_t b0 = playing ? bar : 0;
        int n2 = 0, last = -2;
        for (int64_t b = b0; b < b0 + 48 && n2 < 5; ++b)
        {
            if (L.isOver (b)) break;
            int rr = 0; bool mm = false; L.chordAt (b, rr, mm);
            const int c = L.isCountIn (b) ? -1 : (rr | (L.qualityAt (b) << 4));
            if (c != last || b == b0) { uiChartChord[n2] = c; uiChartBars[n2] = (int) (b - b0); uiChartSec[n2] = L.sectionAt (b); ++n2; last = c; }
        }
        for (; n2 < 5; ++n2) { uiChartChord[n2] = -2; uiChartBars[n2] = 0; }
    }
}

void BandleaderProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    saveLearned();
    apvts.state.setProperty ("benched", (juce::int64) benched.load(), nullptr);
    apvts.state.setProperty ("strangeSeed", (juce::int64) strangeSeed, nullptr);
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void BandleaderProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            // a reopened set keeps its style without re-applying it to everyone
            lastStyleParam = (int) apvts.getRawParameterValue ("style")->load();
            styleIdx = lastStyleParam - 1;
            styleStamp = (uint32_t) (juce::int64) apvts.state.getProperty ("styleStamp", 0);
            loadLearned();
            benched = (uint32_t) (juce::int64) apvts.state.getProperty ("benched", 0);
            strangeSeed = (uint32_t) (juce::int64) apvts.state.getProperty ("strangeSeed", 1);
            if (strangeSeed == 0) strangeSeed = 1;
            lastSpecKey = {};
            rebuildSong();
        }
}

// ---------------------------------------------------------------------------
// Styles: watched on the message thread. Picking one sets Bandleader's own song
// settings and stamps it, and every plugin in the set takes it from the bus.
void BandleaderProcessor::timerCallback()
{
    rebuildSong();
    const int st = (int) apvts.getRawParameterValue ("style")->load();
    if (st == lastStyleParam) return;
    lastStyleParam = st;
    if (st <= 0) { styleIdx = -1; return; }      // Custom: keep everything as it is
    applyStyle (st - 1);
}

void BandleaderProcessor::applyStyle (int idx)
{
    const auto& s = band::styles()[(size_t) idx];
    band::setParam (apvts, "key", (float) s.key);
    band::setParam (apvts, "prog_verse", (float) s.prog[0]);
    band::setParam (apvts, "prog_chorus", (float) s.prog[1]);
    band::setParam (apvts, "prog_bridge", (float) s.prog[2]);
    band::setParam (apvts, "chord_bars", (float) s.chordBars);
    band::setParam (apvts, "section_bars", (float) s.sectionBars);
    band::setParam (apvts, "energy", s.energy);
    band::setParam (apvts, "song", 0.0f);
    band::setParam (apvts, "meter", s.meter == 0 ? 0.0f : (float) (s.meter + 1));   // 4/4 styles follow Live; three-beat styles say so
    uint32_t stamp = (uint32_t) juce::Random::getSystemRandom().nextInt();
    if (stamp == 0) stamp = 1;
    styleIdx = idx; styleStamp = stamp;
    apvts.state.setProperty ("styleStamp", (juce::int64) stamp, nullptr);
}

void BandleaderProcessor::mutate() { command (CMutate); }

// ---------------------------------------------------------------------------
// The sketchbook: the song as it stands (every setting, the learned chords, the band lights, the
// Strange seed) saved in one of 20 slots inside the set.
void BandleaderProcessor::saveSketch (int slot)
{
    if (slot < 0 || slot >= kSketches) return;
    saveLearned();
    juce::ValueTree sk ("Sketch");
    sk.setProperty ("slot", slot, nullptr);
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (! r->getParameterID().startsWith ("t_")) sk.setProperty (r->getParameterID(), r->getValue(), nullptr);
    sk.setProperty ("learned", apvts.state.getProperty ("learned", ""), nullptr);
    sk.setProperty ("benched", (juce::int64) benched.load(), nullptr);
    sk.setProperty ("strangeSeed", (juce::int64) strangeSeed, nullptr);
    const int st = (int) apvts.getRawParameterValue ("style")->load();
    static const char* keys[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    juce::String nm = st > 0 ? juce::String (band::styles()[(size_t) (st - 1)].name) : juce::String ("My song");
    nm << " in " << keys[juce::jlimit (0, 11, (int) apvts.getRawParameterValue ("key")->load())];
    if (apvts.getRawParameterValue ("strange")->load() > 0.01f) nm << ", strange " << juce::roundToInt (apvts.getRawParameterValue ("strange")->load() * 100) << "%";
    sk.setProperty ("name", nm, nullptr);
    auto book = apvts.state.getOrCreateChildWithName ("Sketches", nullptr);
    for (int i = book.getNumChildren(); --i >= 0;) if ((int) book.getChild (i).getProperty ("slot") == slot) book.removeChild (i, nullptr);
    book.appendChild (sk, nullptr);
    currentSketch = slot;
}

bool BandleaderProcessor::loadSketch (int slot)
{
    auto book = apvts.state.getChildWithName ("Sketches");
    juce::ValueTree sk;
    for (auto c : book) if ((int) c.getProperty ("slot") == slot) sk = c;
    if (! sk.isValid()) return false;
    // the style first (with no re-dress of the band: the sketch keeps whatever was tweaked), then everything else
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (sk.hasProperty (r->getParameterID())) r->setValueNotifyingHost ((float) sk.getProperty (r->getParameterID()));
    lastStyleParam = (int) apvts.getRawParameterValue ("style")->load();
    styleIdx = lastStyleParam - 1;
    apvts.state.setProperty ("learned", sk.getProperty ("learned", ""), nullptr);
    loadLearned();
    benched = (uint32_t) (juce::int64) sk.getProperty ("benched", 0);
    strangeSeed = (uint32_t) (juce::int64) sk.getProperty ("strangeSeed", 1);
    lastSpecKey = {}; rebuildSong();
    currentSketch = slot;
    return true;
}

juce::String BandleaderProcessor::sketchName (int slot) const
{
    auto book = apvts.state.getChildWithName ("Sketches");
    for (auto c : book) if ((int) c.getProperty ("slot") == slot) return c.getProperty ("name").toString();
    return {};
}

void BandleaderProcessor::reapplyStyle()
{
    const int st = (int) apvts.getRawParameterValue ("style")->load();
    if (st > 0) applyStyle (st - 1);
}

// ---------------------------------------------------------------------------
// RENDER SONG: post the job; every player (and Bandleader itself) writes its part
bool BandleaderProcessor::startRender()
{
    rebuildSong();
    const auto f = currentSpec();
    const auto song = band::buildSong (f);
    band::RenderJob j;
    if (! band::readLeader (j.song)) j.song = L;
    j.song.playing = 1; j.song.learning = 0;
    j.song.benched = benched.load();
    j.song.dropStart = j.song.dropEnd = band::kNever;
    j.song.meter = uiMeter.load();
    j.spec = f;
    j.totalBars = (int) song.length;
    double bpm = 120;
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) if (auto b = pos->getBpm()) bpm = *b;
    j.bpm = (float) bpm;
    const int st = (int) apvts.getRawParameterValue ("style")->load();
    juce::String title = st > 0 ? juce::String (band::styles()[(size_t) (st - 1)].name) : juce::String ("My Song");
    title = juce::File::createLegalFileName (title);
    renderFolder = mc::midiFolder().getChildFile ("Songs").getChildFile (juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S ") + title);
    if (! renderFolder.createDirectory()) return false;
    renderFolder.getFullPathName().copyToUTF8 (j.folder, sizeof (j.folder));
    j.stamp = (uint32_t) juce::Random::getSystemRandom().nextInt() | 1u;
    renderStamp = j.stamp;
    renderStartedMs = band::nowMs();
    band::postRender (j);
    return true;
}

// the audio thread never touches parameters directly: it asks the message thread to
void BandleaderProcessor::postParams (int sec, bool manual)
{
    pendingSection = sec; pendingManual = manual; paramsPending = true;
    triggerAsyncUpdate();
}

void BandleaderProcessor::handleAsyncUpdate()
{
    if (const float st = pendingStrange.exchange (-1.0f); st >= 0.0f) band::setParam (apvts, "strange", st);
    if (pendingMutate.exchange (false)) apvts.state.setProperty ("strangeSeed", (juce::int64) strangeSeed, nullptr);
    if (const int step = pendingSketchStep.exchange (0); step != 0)
    {
        // the next (or previous) sketch that has something in it
        const int from = juce::jmax (0, currentSketch.load());
        for (int k = 1; k <= kSketches; ++k)
        {
            const int s = ((from + step * k) % kSketches + kSketches) % kSketches;
            if (loadSketch (s)) break;
        }
    }
    if (paramsPending.load())
    {
        if (pendingManual.load()) if (auto* p = apvts.getParameter ("song")) p->setValueNotifyingHost (1.0f);
        if (auto* p = apvts.getParameter ("section")) p->setValueNotifyingHost (p->convertTo0to1 ((float) pendingSection.load()));
        paramsPending = false;
    }
    if (learnPending.load())
    {
        applyLearned (pendingLb, pendingHas);
        learnPending = false;
    }
}

juce::AudioProcessorEditor* BandleaderProcessor::createEditor() { return new BandleaderEditor (*this); }

#ifndef BANDLEADER_NO_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BandleaderProcessor(); }
#endif
