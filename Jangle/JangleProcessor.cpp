#include <juce_audio_formats/juce_audio_formats.h>
#include "JangleProcessor.h"
#include "JangleEditor.h"
#include "JangleData.h"

using namespace jg;

namespace
{
juce::String pct (float v, int) { return juce::String (juce::roundToInt (v * 100)) + "%"; }
juce::String dB1 (float v, int) { return juce::String (v, 1) + " dB"; }
juce::StringArray guitarNames() { juce::StringArray a; for (int i = 0; i < NumGuitars; ++i) a.add (guitarName (i)); return a; }
juce::StringArray feelNames() { juce::StringArray a; for (int i = 0; i < NumFeels; ++i) a.add (feelName (i)); return a; }

// a chord from held MIDI keys (lowest note leans the root)
void chordFromHeld (const std::array<bool, 128>& held, int& root, int& q)
{
    int mask = 0, lowest = -1;
    for (int n = 0; n < 128; ++n) if (held[(size_t) n]) { mask |= 1 << (n % 12); if (lowest < 0) lowest = n; }
    root = -1; q = 0;
    if (mask == 0) return;
    int best = -100;
    for (int r = 0; r < 12; ++r)
        for (int qq = 0; qq < band::NumQualities; ++qq)
        {
            int t[4]; const int nt = band::qualityTones (qq, t); int s = 0, tm = 0;
            for (int i = 0; i < nt; ++i) { const int pc = (r + t[i]) % 12; tm |= 1 << pc; s += (mask >> pc) & 1 ? 3 : -2; }
            for (int pc = 0; pc < 12; ++pc) if (((mask >> pc) & 1) && ! ((tm >> pc) & 1)) s -= 2;
            if (r == lowest % 12) s += 2;
            if (nt == 3) s += 1;
            if (s > best) { best = s; root = r; q = qq; }
        }
}
}

juce::AudioProcessorValueTreeState::ParameterLayout JangleProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> ps;
    auto F = [&] (const char* id, const char* name, juce::NormalisableRange<float> r, float def, juce::String (*fmt) (float, int))
    { ps.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, r, def, juce::AudioParameterFloatAttributes().withStringFromValueFunction (fmt))); };
    auto C = [&] (const char* id, const char* name, juce::StringArray items, int def)
    { ps.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def)); };
    C ("guitar", "Guitar", guitarNames(), 0);
    C ("feel", "Feel", feelNames(), 0);
    C ("follow", "Chords From", { "Band (Bandleader / own song)", "MIDI chords", "Play it (MIDI keys)" }, 0);
    C ("sit_out", "With The Band", { "Plays along", "Sits out" }, 0);
    C ("position", "Chord Shapes", { "Open chords", "Barre chords", "Up the neck" }, 0);
    C ("key", "Key", { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 9);
    C ("prog_verse", "Verse Chords", ut::progressionNames(), 0);
    C ("prog_chorus", "Chorus Chords", ut::progressionNames(), 2);
    C ("prog_bridge", "Bridge Chords", ut::progressionNames(), 5);
    C ("chord_bars", "Bars Per Chord", { "1", "2", "4" }, 0);
    C ("song", "Song", { "Auto song", "Manual" }, 0);
    C ("section_bars", "Section Length", { "4 bars", "8 bars", "16 bars", "12 bars" }, 1);
    C ("section", "Section", { "Verse", "Chorus", "Bridge" }, 0);
    F ("busy", "Busy", { 0, 1, 0.001f }, 0.5f, pct);
    F ("strum", "Strum Speed", { 0, 1, 0.001f }, 0.5f, pct);
    F ("dynamics", "Dynamics", { 0, 1, 0.001f }, 0.65f, pct);
    F ("human", "Human", { 0.1f, 1, 0.001f }, 0.6f, pct);
    F ("swing", "Swing", { 0, 0.5f, 0.001f }, 0, pct);
    F ("length", "Let It Ring", { 0.3f, 1.6f, 0.001f }, 1.0f, pct);
    F ("drive", "Drive", { 0, 1, 0.001f }, 0.35f, pct);
    F ("tone", "Tone", { 0, 1, 0.001f }, 0.5f, pct);
    F ("trem", "Tremolo", { 0, 1, 0.001f }, 0, pct);
    F ("chorus", "Chorus", { 0, 1, 0.001f }, 0, pct);
    F ("room", "Room", { 0, 1, 0.001f }, 0.25f, pct);
    F ("width", "Width", { 0, 1, 0.001f }, 0.6f, pct);
    F ("out", "Output", { -24, 6, 0.1f }, -3, dB1);
    C ("take_style", "Take Style From Bandleader", { "Off", "On" }, 1);
    return { ps.begin(), ps.end() };
}

JangleProcessor::JangleProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Jangle", createLayout())
{
    band::available();
    queue.reserve (2048); bar.reserve (64);
    outMidi.ensureSize (2048);
    loadSamples();
    renderer = std::make_unique<mc::SongRenderer> (*this, band::WJangle, mc::SongRenderer::partFileName (4, "Rhythm Guitar"),
                                                   [] { return std::make_unique<JangleProcessor>(); },
                                                   [] (juce::AudioProcessor& p) { mc::setChoice (p, "follow", 0); });
}

// names: <set>_<midi>_<layer>_flac   (sets: ac steel acoustic, cl clean, mu palm mutes, sh hollow body, sc dead-string scratches)
void JangleProcessor::loadSamples()
{
    juce::FlacAudioFormat flac;
    const char* prefixes[NumSets] = { "ac_", "cl_", "mu_", "sh_", "sc_" };
    for (int i = 0; i < JangleData::namedResourceListSize; ++i)
    {
        const juce::String res (JangleData::namedResourceList[i]);
        int set = -1;
        for (int s = 0; s < NumSets; ++s) if (res.startsWith (prefixes[s])) set = s;
        if (set < 0) continue;
        juce::StringArray bits; bits.addTokens (res, "_", "");
        if (bits.size() < 3) continue;
        int size = 0;
        const char* data = JangleData::getNamedResource (res.toRawUTF8(), size);
        if (data == nullptr) continue;
        std::unique_ptr<juce::AudioFormatReader> reader (flac.createReaderFor (new juce::MemoryInputStream (data, (size_t) size, false), true));
        if (reader == nullptr) continue;
        const int len = (int) reader->lengthInSamples;
        juce::AudioBuffer<float> tmp (1, len);
        reader->read (&tmp, 0, len, 0, true, false);
        Zone z;
        z.root = bits[1].getIntValue(); z.layer = bits[2].getIntValue(); z.sr = reader->sampleRate;
        z.d.assign (tmp.getReadPointer (0), tmp.getReadPointer (0) + len);
        z.d.push_back (0); z.d.push_back (0);
        lib.sets[set].zones.push_back (std::move (z));
        ++loaded;
    }
}

void JangleProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    for (auto& v : voices) { v.prepare (sr); v.kill(); }
    rig.prepare (sr, juce::jmax (512, samplesPerBlock));
    const size_t n = (size_t) juce::jmax (512, samplesPerBlock);
    tmpL.assign (n, 0.0f); tmpR.assign (n, 0.0f);
    queue.clear();
    lastStep = -1; lastPpqEnd = -1; wasPlaying = false; curBar = -1;
    held.fill (false); lastRoot = -1;
    rng.seed ((uint32_t) juce::Time::getMillisecondCounter() | 1u);
}

void JangleProcessor::readParams()
{
    auto g = [&] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    guitar = (int) g ("guitar"); feel = (int) g ("feel"); follow = (int) g ("follow"); position = (int) g ("position");
    key = (int) g ("key"); progs[0] = (int) g ("prog_verse"); progs[1] = (int) g ("prog_chorus"); progs[2] = (int) g ("prog_bridge");
    chordBars = 1 << (int) g ("chord_bars"); autoSong = g ("song") < 0.5f; sectionBars = ut::sectionBarsFor ((int) g ("section_bars"));
    section = (int) g ("section");
    busy = g ("busy"); strumSpeed = g ("strum"); dynamics = g ("dynamics"); human = g ("human"); swing = g ("swing"); lengthK = g ("length");
    Rig::Params p;
    p.guitar = guitar; p.drive = g ("drive"); p.tone = g ("tone"); p.trem = g ("trem"); p.chorus = g ("chorus");
    p.room = g ("room"); p.width = g ("width");
    // every guitar (and the busier feels) about as loud as the others
    static const float trim[NumGuitars] = { 0.85f, 0.95f, 0.9f, 1.25f, 0.6f, 0.33f, 1.8f };
    const float feelTrim = (feel == FPower || feel == FChug) ? 0.75f : 1.0f;
    p.out = juce::Decibels::decibelsToGain (g ("out")) * 1.6f * trim[juce::jlimit (0, NumGuitars - 1, guitar)] * feelTrim;
    rig.set (p);
}

int JangleProcessor::sectionAt (int64_t b, int& barIn, bool& lastBar) const
{
    lastBar = false;
    if (led)
    {
        int64_t in = 0; const int s = leader.sectionAt (b, &in);
        barIn = (int) in; lastBar = leader.nextStartBar == b + 1;
        return s;
    }
    if (autoSong) { const auto sp = ut::songPosition (b, sectionBars); barIn = sp.barIn; lastBar = sp.lastBar; return sp.section; }
    barIn = (int) (((b % 8) + 8) % 8);
    return section;
}

void JangleProcessor::chordAt (int64_t b, int& root, int& q) const
{
    if (follow == FMidiChords) { root = midiRoot; q = midiQ; return; }
    if (led) { bool m = false; leader.chordAt (b, root, m); q = leader.qualityAt (b); return; }
    int barIn = 0; bool lb = false;
    const int sec = sectionAt (b, barIn, lb);
    const auto& ps = ut::progressions();
    const auto& prog = ps[(size_t) juce::jlimit (0, (int) ps.size() - 1, progs[sec])].chords;
    const auto& pc = prog[(size_t) ((barIn / juce::jmax (1, chordBars)) % (int) prog.size())];
    root = (key + pc.off) % 12; q = pc.quality();
}

Shape JangleProcessor::shapeFor (int root, int q) const
{
    if (feelUsesPower (feel) || (guitar == GFuzz && feel != FRing && feel != FWash && feel != FArpeggio && feel != FPicked)) return powerShape (root, position);
    return chordShape (root, q, position);
}

// ---- the right hand
void JangleProcessor::startString (const Ev& e)
{
    // one note per string: the last one on this string stops
    for (auto& v : voices) if (v.active() && v.string == e.string) { v.choke (0.015f); v.string = -1; }
    Voice* fv = nullptr;
    for (auto& v : voices) if (! v.active()) { fv = &v; break; }
    if (fv == nullptr) for (auto& v : voices) if (v.string < 0 && (fv == nullptr || v.age > fv->age)) fv = &v;
    if (fv == nullptr) for (auto& v : voices) if (fv == nullptr || v.age > fv->age) fv = &v;
    const auto& set = lib.sets[e.set];
    const Zone* z = nullptr;
    if (e.set == SetScratch) { for (auto& zz : set.zones) if (zz.root == e.note && (z == nullptr || rng.chance (0.5f))) z = &zz; if (z == nullptr && ! set.zones.empty()) z = &set.zones[(size_t) rng.i ((int) set.zones.size())]; }
    else z = set.pick (e.note, e.vel, rng);
    if (z == nullptr) return;
    const int str = e.string % 6;
    const float pan = e.string >= 12 ? 0.0f : (-0.3f + 0.12f * (float) str) * (e.string >= 6 ? -1.0f : 1.0f);
    const double bright = e.set == SetMute ? 0.55 : (guitar == GAcoustic ? 0.85 + 0.15 * e.vel : 0.6 + 0.4 * e.vel);
    fv->start (z, e.set == SetScratch ? z->root : e.note, e.vel, e.len, pan, (e.string >= 6 && e.string < 12) ? 4.0f : rng.n() * 2.0f, bright);
    fv->string = e.string;
    uiString[(size_t) str] = e.vel;
}

void JangleProcessor::strum (const Hit& h, int64_t at, const Shape& sh, double sixteenth)
{
    float v = juce::jlimit (0.06f, 1.0f, h.vel * (0.55f + 0.6f * dynamics) + rng.n() * 0.06f * human + (led ? (leader.energy - 0.5f) * 0.2f : 0.0f));
    if (barSection == 1) v = juce::jmin (1.0f, v + 0.05f);
    const float lenSec = h.len < 0 ? (float) (sixteenth * bs * 2.2 / sr * lengthK) : (float) (h.len * lengthK * sixteenth / sr);
    const bool muted = guitar == GMuted || (h.stroke == SDown && h.arg == 1);
    const int setId = muted ? SetMute : setFor (guitar);
    std::vector<int> used; for (int s = 0; s < 6; ++s) if (sh.fret[s] >= 0) used.push_back (s);
    if (used.empty()) return;
    const double gapMs = (4.0 + 14.0 * (1.0 - strumSpeed)) * (1.15 - 0.3 * v);
    auto play = [&] (int str, int64_t t, float vel, float len)
    {
        const int note = stringNote (sh, str);
        if (note < 0) return;
        schedule ({ t, 1, str, note, vel, muted ? juce::jmin (len, 0.14f) : len, setId });
        if (guitar == GTwelve && ! muted)    // the twelve-string's paired strings: octaves on the low four, unisons on top
            schedule ({ t + (int64_t) (0.003 * sr), 1, str + 6, note + (str < 4 ? 12 : 0), vel * 0.7f, len, setId });
    };
    switch (h.stroke)
    {
        case SDown:
        {
            // low to high; a palm-muted chug only hits the low strings
            const int top = muted && h.arg == 1 ? juce::jmin ((int) used.size(), 3) : (int) used.size();
            for (int i = 0; i < top; ++i)
                play (used[(size_t) i], at + (int64_t) (i * gapMs * 0.001 * sr * (1.0 + rng.n() * 0.15 * human)), v * (1.0f - 0.03f * (float) i), lenSec);
            uiStroke = 0;
            break;
        }
        case SUp:
        {
            // high to low, the top three or four strings, a little lighter
            const int nStr = juce::jmin ((int) used.size(), (h.vel > 0.75f ? 4 : 3));
            for (int i = 0; i < nStr; ++i)
                play (used[used.size() - 1 - (size_t) i], at + (int64_t) (i * gapMs * 0.8 * 0.001 * sr), v * 0.86f * (1.0f - 0.04f * (float) i), lenSec);
            uiStroke = 1;
            break;
        }
        case SScratch:
            schedule ({ at, 1, 13, 1 + rng.i (4), v * 0.8f, 0.12f, SetScratch });
            for (int s = 0; s < 6; ++s) schedule ({ at, 2, s, 0, 0, 0, 0 });    // the hand lands on the strings
            uiStroke = 2;
            break;
        case SBass:
        {
            int s = used.front();
            if (h.arg == 1 && used.size() > 1) s = used[1];
            play (s, at, v, lenSec);
            break;
        }
        case SPinch:
            play (used.front(), at, v * 0.9f, lenSec);
            play (used.back(), at + 2, v, lenSec);
            break;
        default:   // SPick: a string by its place in the shape (0 = the lowest, 5 = the top)
        {
            const int idx = h.arg <= 0 ? 0 : juce::jlimit (0, (int) used.size() - 1, (int) std::lround ((double) h.arg / 5.0 * (double) (used.size() - 1)));
            play (used[(size_t) idx], at, v, lenSec);
            break;
        }
    }
}

void JangleProcessor::playStep (int64_t k, int64_t at)
{
    const int64_t b = k / bs;
    const int s = (int) (((k % bs) + bs) % bs);
    if (b != curBar)
    {
        curBar = b;
        bool lastBar = false;
        barSection = sectionAt (b, barInSec, lastBar);
        writeBar (feel, barSection, busy, meterNow, rng, bar);
        const bool fill = (led && leader.fillAt (b)) || (! led && follow == FBand && autoSong && lastBar);
        if (fill && filledBar != b) { writeFill (feel, meterNow, rng, bar); filledBar = b; }
        if (led && leader.isHalf (b)) { bar.clear(); bar.push_back ({ 0, SDown, 0.82f, -1.0f }); }   // the breakdown: one big strum a bar
        uiSection = barSection;
    }
    uiStep = s;
    const double sixteenth = sr * 15.0 / juce::jmax (30.0, bpmNow);
    // the song's last chord: one big strum, left to ring
    if (led && leader.isEnding (b))
    {
        if (leader.isFinalHit (b) && s == 0)
        {
            int r = 0, q = 0; chordAt (b, r, q);
            if (r >= 0) { shape = shapeFor (r, q); lastRoot = r; lastQ = q; strum ({ 0, SDown, 0.95f, (float) (bs * 2) / (float) lengthK }, at, shape, sixteenth); }
        }
        return;
    }
    const bool out = (led && leader.rests (b, false)) || (follow == FMidiChords && midiRoot < 0);
    if (out) { if (! bandOut) { schedule ({ at, 3, 0, 0, 0, 0, 0 }); bandOut = true; } return; }
    bandOut = false;

    int r = 0, q = 0; chordAt (b, r, q);
    if (r < 0) return;
    const bool changed = r != lastRoot || q != lastQ;
    if (changed)
    {
        shape = shapeFor (r, q); lastRoot = r; lastQ = q;
        for (int i = 0; i < 6; ++i) uiFret[i] = shape.fret[i];
    }
    uiChord = r | (q << 4);
    bool any = false; for (auto& h : bar) if (h.step == s) any = true;
    // a ringing chord that changes under you: strum the new one
    if (changed && ! any && s != 0 && (feel == FRing || feel == FWash)) strum ({ s, SDown, 0.66f, -1.0f }, at, shape, sixteenth);
    if (! any) return;
    double offMs = rng.n() * (1.5 + 5.0 * human);
    if (s % 2 == 1 && meterNow != band::M68) offMs += swing * sixteenth / sr * 1000.0 * 0.66;
    for (const auto& h : bar)
    {
        if (h.step != s) continue;
        const double late = h.stroke == SUp ? 4.0 * human : 0.0;    // upstrokes sit a hair behind
        strum (h, juce::jmax (sampleClock, at + (int64_t) ((offMs + late) * sr / 1000.0)), shape, sixteenth);
    }
}

void JangleProcessor::runPlayer (int n)
{
    double ppq = 0, bpm = 120; bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto p = pos->getPpqPosition()) ppq = *p; else playing = false;
        }
    bpmNow = bpm; blockPpq = ppq; blockPlaying = playing;
    uiTransport = playing;
    {
        const int m = led ? leader.meter : band::hostMeter (getPlayHead());
        if (m != meterNow) { meterNow = m; curBar = -1; }
        bs = band::meterSteps (meterNow); uiSteps = bs;
        capture.setMeter (meterNow);
    }
    if (! playing)
    {
        if (wasPlaying) { schedule ({ sampleClock, 3, 0, 0, 0, 0, 0 }); uiStep = -1; }
        wasPlaying = false; return;
    }
    const double spb = sr * 60.0 / bpm;
    if (! wasPlaying || ppq < lastPpqEnd - 0.02 || ppq > lastPpqEnd + 0.25)
    {
        lastStep = (int64_t) std::ceil (ppq * 4.0 - 0.001) - 1;
        curBar = -1; lastRoot = -1;
        if (wasPlaying) schedule ({ sampleClock, 3, 0, 0, 0, 0, 0 });
    }
    wasPlaying = true;
    const double ppqEnd = ppq + n / spb;
    for (int64_t k = lastStep + 1; (double) k / 4.0 < ppqEnd; ++k)
    {
        playStep (k, sampleClock + juce::jmax ((int64_t) 0, (int64_t) (((double) k / 4.0 - ppq) * spb)));
        lastStep = k;
    }
    lastPpqEnd = ppqEnd;
}

void JangleProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if ((int) tmpL.size() < n) return;
    readParams();
    band::markPresent (band::WJangle);
    const bool wasLed = led;
    led = follow != FPlay && band::readLeader (leader);
    uiLinked = led;
    if (led != wasLed) lastPpqEnd = -1e9;

    // MIDI in: chords to follow, or notes to play
    bool chordKeys = false;
    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        const int64_t at = sampleClock + m.samplePosition;
        if (msg.isNoteOn())
        {
            held[(size_t) msg.getNoteNumber()] = true; chordKeys = true;
            if (follow == FPlay)
            {
                playVoice = (playVoice + 1) % 8;
                schedule ({ at, 1, 14 + playVoice, msg.getNoteNumber(), msg.getFloatVelocity(), -1.0f, guitar == GMuted ? SetMute : setFor (guitar) });
            }
        }
        else if (msg.isNoteOff())
        {
            held[(size_t) msg.getNoteNumber()] = false; chordKeys = true;
            if (follow == FPlay) schedule ({ at, 4, 0, msg.getNoteNumber(), 0, 0, 0 });
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff()) { held.fill (false); schedule ({ at, 3, 0, 0, 0, 0, 0 }); }
    }
    if (chordKeys) chordFromHeld (held, midiRoot, midiQ);

    const bool sitOut = apvts.getRawParameterValue ("sit_out")->load() > 0.5f;
    if (follow != FPlay && ! sitOut) runPlayer (n);
    else { uiTransport = false; if (wasPlaying) { schedule ({ sampleClock, 3, 0, 0, 0, 0, 0 }); wasPlaying = false; } }

    // render, splitting the block at each event
    std::stable_sort (queue.begin(), queue.end(), [] (const Ev& a, const Ev& b) { return a.at < b.at; });
    outMidi.clear();
    float* L = tmpL.data(); float* R = tmpR.data();
    std::fill (L, L + n, 0.0f); std::fill (R, R + n, 0.0f);
    size_t e = 0;
    const int64_t end = sampleClock + n;
    int pos = 0;
    auto renderTo = [&] (int to) { if (to > pos) { for (auto& v : voices) v.render (L + pos, R + pos, to - pos); pos = to; } };
    while (e < queue.size() && queue[e].at < end)
    {
        const auto& ev = queue[e++];
        const int at = (int) juce::jlimit<int64_t> (0, n - 1, ev.at - sampleClock);
        renderTo (at);
        midiOut.tick (outMidi, at, sampleClock + at);
        if (ev.type == 1)
        {
            startString (ev);
            if (ev.set != SetScratch) midiOut.on (outMidi, at, sampleClock + at, ev.note, ev.vel, ev.len, sr);
        }
        else if (ev.type == 2) { for (auto& v : voices) if (v.active() && v.string == ev.string) v.choke (0.02f); }
        else if (ev.type == 4) { for (auto& v : voices) if (v.active() && v.string >= 14 && v.note == ev.note) v.choke (0.12f); midiOut.off (outMidi, at, ev.note); }
        else { for (auto& v : voices) v.choke (0.05f); midiOut.allOff (outMidi, at); }
    }
    renderTo (n);
    midiOut.tick (outMidi, n - 1, sampleClock + n - 1);
    // keep what's still in the future (strums and swing can land in the next block)
    queue.erase (queue.begin(), queue.begin() + (long) e);

    rig.process (L, R, n, blockPpq, bpmNow, blockPlaying);
    float* oL = buffer.getWritePointer (0);
    float* oR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    for (int i = 0; i < n; ++i)
    {
        float l = L[i], r = R[i];
        if (std::abs (l) > 0.9f) l = std::copysign (0.9f + 0.1f * std::tanh ((std::abs (l) - 0.9f) * 10.0f), l);
        if (std::abs (r) > 0.9f) r = std::copysign (0.9f + 0.1f * std::tanh ((std::abs (r) - 0.9f) * 10.0f), r);
        oL[i] = std::isfinite (l) ? l : 0.0f;
        if (oR) oR[i] = std::isfinite (r) ? r : 0.0f;
    }
    fade.apply (buffer, n, led, leader, getPlayHead());   // Bandleader's fade-out
    for (auto& u : uiString) u = u.load() * 0.9f;
    midi.swapWith (outMidi);
    {
        double cp = 0; bool cpl = false;
        if (auto* ph = getPlayHead()) if (auto p = ph->getPosition()) { cpl = p->getIsPlaying(); if (auto q = p->getPpqPosition()) cp = *q; else cpl = false; }
        capture.recordBlock (midi, cp, cpl, bpmNow, sr, n);
    }
    sampleClock += n;
}

void JangleProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void JangleProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

void JangleProcessor::applyStyle (const band::StyleDef& s)
{
    band::setParam (apvts, "sit_out", s.gOn ? 0.0f : 1.0f);
    if (! s.gOn) return;
    band::setParam (apvts, "guitar", (float) s.gInst);
    band::setParam (apvts, "feel", (float) s.gFeel);
    band::setParam (apvts, "busy", s.gBusy);
    band::setParam (apvts, "swing", juce::jlimit (0.0f, 0.5f, s.dSwing * 0.6f));
    band::setParam (apvts, "trem", s.gInst == GClean && s.lMood == 1 ? 0.3f : 0.0f);
    band::setParam (apvts, "chorus", s.bChorus > 0.3f ? 0.35f : 0.0f);
    band::setParam (apvts, "room", juce::jlimit (0.1f, 0.7f, 0.15f + s.lSpace * 0.6f));
    band::setParam (apvts, "drive", s.gInst == GFuzz ? 0.6f : s.gInst == GCrunch ? 0.4f : 0.3f);
}

juce::AudioProcessorEditor* JangleProcessor::createEditor() { return new JangleEditor (*this); }

#ifndef JANGLE_NO_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new JangleProcessor(); }
#endif
