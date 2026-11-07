#include "UndertowProcessor.h"
#include "UndertowEditor.h"
#include "BinaryData.h"

using namespace ut;

namespace
{
juce::String pct (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
juce::String dB1 (float v, int) { return juce::String (v, 1) + " dB"; }
juce::String bip (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + (v > 0.005f ? "% ahead" : v < -0.005f ? "% behind" : "%"); }
const juce::StringArray& onOff() { static const juce::StringArray s { "Off", "On" }; return s; }

inline uint32_t hash (uint64_t x)
{
    x ^= x >> 33; x *= 0xff51afd7ed558ccdULL; x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL; x ^= x >> 33;
    return (uint32_t) x;
}
inline float hashU (uint64_t x) { return (float) (hash (x) & 0xffffff) / 16777216.0f; }
}

juce::AudioProcessorValueTreeState::ParameterLayout UndertowProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> ps;
    auto F = [&] (const char* id, const char* name, juce::NormalisableRange<float> r, float def, juce::String (*fmt) (float, int))
    {
        ps.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, r, def,
                                                                   juce::AudioParameterFloatAttributes().withStringFromValueFunction (fmt)));
    };
    auto C = [&] (const char* id, const char* name, juce::StringArray items, int def)
    { ps.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def)); };

    C ("mode", "Mode", { "Bass Player", "Play It (MIDI keys)", "Guitar to Bass: Octave", "Guitar to Bass: Tracked" }, 0);
    C ("follow", "Chords From", { "Self (plays by itself)", "Guitar (sidechain)", "MIDI chords" }, 0);
    C ("key", "Key", { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 9);
    C ("prog_verse", "Verse Chords", progressionNames(), 0);
    C ("prog_chorus", "Chorus Chords", progressionNames(), 2);
    C ("prog_bridge", "Bridge Chords", progressionNames(), 5);
    C ("chord_bars", "Bars Per Chord", { "1", "2", "4" }, 0);
    C ("song", "Song", { "Auto song", "Manual" }, 0);
    C ("section_bars", "Section Length", { "4 bars", "8 bars", "16 bars" }, 1);
    C ("feel", "Feel", { "Pump", "Melodic", "Gothic", "Octaves", "Walk" }, 0);
    C ("section", "Section", { "Verse", "Chorus", "Bridge" }, 0);
    C ("fill_now", "Footswitch: Fill", onOff(), 0);
    C ("register", "Register", { "Low (E-D#)", "Mid (A-G#)", "High (octave up)" }, 0);
    C ("band_stop", "When The Band Stops", { "Stop with the band", "Keep playing" }, 0);
    F ("variation", "Variation", { 0, 1, 0.001f }, 0.4f, pct);
    F ("fills", "Fills", { 0, 1, 0.001f }, 0.5f, pct);
    F ("hook", "Hook (high melody)", { 0, 1, 0.001f }, 0.5f, pct);
    F ("density", "Busy", { 0, 1, 0.001f }, 0.55f, pct);
    F ("push", "Push", { -1, 1, 0.001f }, 0.25f, bip);
    F ("human", "Human", { 0.2f, 1, 0.001f }, 0.6f, pct);
    F ("swing", "Swing", { 0, 0.5f, 0.001f }, 0, pct);
    F ("length", "Note Length", { 0.3f, 1.3f, 0.001f }, 0.9f, pct);
    C ("octave", "Octave", { "-2", "-1", "0", "+1" }, 2);
    F ("sens", "Guitar Sensitivity", { 0, 1, 0.001f }, 0.5f, pct);
    F ("guitar_dry", "Guitar Blend", { 0, 1, 0.001f }, 0, pct);
    // the rig
    F ("comp", "Compressor", { 0, 1, 0.001f }, 0.4f, pct);
    F ("grit", "Grit", { 0, 1, 0.001f }, 0.2f, pct);
    F ("ch_mix", "Chorus", { 0, 1, 0.001f }, 0.45f, pct);
    F ("ch_rate", "Chorus Rate", { 0, 1, 0.001f }, 0.3f, pct);
    F ("ch_depth", "Chorus Depth", { 0, 1, 0.001f }, 0.55f, pct);
    F ("fl_mix", "Flanger", { 0, 1, 0.001f }, 0, pct);
    F ("fl_rate", "Flanger Rate", { 0, 1, 0.001f }, 0.2f, pct);
    F ("fl_fb", "Flanger Feedback", { 0, 1, 0.001f }, 0.55f, pct);
    F ("bass_db", "Bass", { -12, 12, 0.1f }, 2, dB1);
    F ("mid_db", "Mid", { -12, 12, 0.1f }, 1.5f, dB1);
    F ("treble_db", "Treble", { -12, 12, 0.1f }, 2.5f, dB1);
    C ("bright", "Bright", onOff(), 1);
    F ("out", "Output", { -24, 6, 0.1f }, -3, dB1);
    return { ps.begin(), ps.end() };
}

UndertowProcessor::UndertowProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                                       .withInput ("Sidechain (guitar)", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Undertow", createLayout())
{
    for (auto& m : uiMotif) m = -1;
    loadSamples();
}

bool UndertowProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    const auto in = l.getMainInputChannelSet();
    return in.isDisabled() || in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

// Real picked bass: Karoryfer "Black And Blue Basses" (CC0), the pick + bridge pickup bass
void UndertowProcessor::loadSamples()
{
    auto bank = std::make_shared<Bank>();
    juce::FlacAudioFormat flac;
    int loaded = 0;
    for (int note = kLowSample; note <= kHighSample; ++note)
        for (int layer = 0; layer < kLayers; ++layer)
            for (int rr = 0; rr < kRR; ++rr)
            {
                const juce::String name = "b_" + juce::String (note) + "_" + juce::String (layer) + "_" + juce::String (rr) + "_flac";
                int size = 0;
                const char* data = BinaryData::getNamedResource (name.toRawUTF8(), size);
                if (data == nullptr) continue;
                std::unique_ptr<juce::AudioFormatReader> reader (flac.createReaderFor (new juce::MemoryInputStream (data, (size_t) size, false), true));
                if (reader == nullptr) continue;
                const int len = (int) reader->lengthInSamples;
                juce::AudioBuffer<float> tmp ((int) juce::jmax (1u, reader->numChannels), len);
                reader->read (&tmp, 0, len, 0, true, true);
                auto s = std::make_shared<Sample>();
                s->data.assign (tmp.getReadPointer (0), tmp.getReadPointer (0) + len);
                s->data.push_back (0); s->data.push_back (0);
                s->sr = reader->sampleRate;
                bank->s[(size_t) (note - kLowSample)][(size_t) layer][(size_t) rr] = s;
                ++loaded;
            }
    if (loaded > 0) sampler.setBank (bank);
}

void UndertowProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    sampler.prepare (sr); ears.prepare (sr); tracker.prepare (sr); octave.prepare (sr); rig.prepare (sr);
    const size_t n = (size_t) juce::jmax (512, samplesPerBlock);
    sc.assign (n, 0.0f); mono.assign (n, 0.0f); tmpR.assign (n, 0.0f);
    events.reserve (512);
    keyboardState.reset();
    lastStep = -1; lastPpqEnd = -1; wasPlaying = false;
    midiOutNote = -1; midiOffIn = -1; keysNote = -1;
    held.fill (false);
    lastFillParam = apvts.getRawParameterValue ("fill_now")->load();
    baseFeel = -1;
}

void UndertowProcessor::readParams()
{
    auto g = [&] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    mode = (int) g ("mode"); follow = (int) g ("follow"); feel = (int) g ("feel"); section = (int) g ("section");
    key = (int) g ("key"); progs[0] = (int) g ("prog_verse"); progs[1] = (int) g ("prog_chorus"); progs[2] = (int) g ("prog_bridge");
    chordBars = 1 << (int) g ("chord_bars"); autoSong = g ("song") < 0.5f; sectionBars = 4 << (int) g ("section_bars");
    reg = (int) g ("register"); stopWithBand = g ("band_stop") < 0.5f; keysOct = (int) g ("octave") - 2;
    variation = g ("variation"); fills = g ("fills"); hook = g ("hook"); density = g ("density");
    push = g ("push"); human = g ("human"); swing = g ("swing"); lengthK = g ("length");
    sens = g ("sens"); guitarDry = g ("guitar_dry");
    const float f = g ("fill_now");
    if (f > 0.5f && lastFillParam <= 0.5f) fillPending = true;
    lastFillParam = f;

    RigParams p;
    p.comp = g ("comp"); p.grit = g ("grit");
    p.chorusMix = g ("ch_mix"); p.chorusRate = g ("ch_rate"); p.chorusDepth = g ("ch_depth");
    p.flangerMix = g ("fl_mix"); p.flangerRate = g ("fl_rate"); p.flangerFb = g ("fl_fb");
    p.bassDb = g ("bass_db"); p.midDb = g ("mid_db"); p.trebleDb = g ("treble_db"); p.bright = g ("bright") > 0.5f;
    p.outGain = juce::Decibels::decibelsToGain (g ("out"));
    rig.update (p);
}

// ---------------------------------------------------------------------------
void UndertowProcessor::handleMidi (const juce::MidiMessage& m, std::vector<Ev>& evs, int pos)
{
    if (m.isNoteOn())
    {
        const int nn = m.getNoteNumber();
        if (mode == MPlayer && nn >= kKeyVerse && nn <= kKeyFill)
        {
            if (nn == kKeyFill) fillPending = true;
            else
            {
                if (auto* p = apvts.getParameter ("section")) p->setValueNotifyingHost (p->convertTo0to1 ((float) (nn - kKeyVerse)));
                if (auto* p = apvts.getParameter ("song")) p->setValueNotifyingHost (1.0f);   // you're steering now
            }
            return;
        }
        held[(size_t) nn] = true;
        if (mode == MKeys)
        {
            keysNote = nn;
            evs.push_back ({ pos, 1, juce::jlimit (0, 127, nn + keysOct * 12), m.getFloatVelocity(), -1.0f });
        }
    }
    else if (m.isNoteOff())
    {
        const int nn = m.getNoteNumber();
        held[(size_t) nn] = false;
        if (mode == MKeys && nn == keysNote) { evs.push_back ({ pos, 2, 0, 0, 0 }); keysNote = -1; }
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        held.fill (false);
        evs.push_back ({ pos, 2, 0, 0, 0 });
    }
}

// which chord of the built-in progression is playing at 16th number k
ut::Chord UndertowProcessor::selfChordAt (int64_t k) const
{
    const int64_t bar = k / 16;
    int sec = section; int64_t barIn = bar;
    if (autoSong) { const auto sp = songPosition (bar, sectionBars); sec = sp.section; barIn = sp.barIn; }
    const auto& prog = progressions()[(size_t) juce::jlimit (0, (int) progressions().size() - 1, progs[sec])].chords;
    const auto& pc = prog[(size_t) ((barIn / chordBars) % (int64_t) prog.size())];
    return { (key + pc.off) % 12, pc.minor };
}

ut::Chord UndertowProcessor::currentChord() const
{
    if (follow == FollowMidi) return midiChord;
    if (follow == FollowSelf) return selfChord;
    return ears.current();
}

int UndertowProcessor::pitchFor (Chord c, const Step& s) const
{
    int root = 28 + ((c.root - 4 + 12) % 12);                 // E1 .. D#2
    if (reg == 1) root = 33 + ((c.root - 9 + 12) % 12);       // A1 .. G#2
    if (reg == 2) root += 12;
    int p = root + harmony.semisFor (c, s.deg) + s.semis + keysOct * 12;
    while (p > 74) p -= 12;
    while (p < 23) p += 12;
    return p;
}

// timing feel in ms for 16th number k: a little jitter, a slow drift, the push, the swing
float UndertowProcessor::offsetMs (int64_t k) const
{
    const uint64_t s = (uint64_t) runSeed << 32;
    const float jit = (hashU (s ^ (uint64_t) k) + hashU (s ^ ((uint64_t) k * 7919u + 1)) - 1.0f) * (2.0f + 7.0f * human);
    const int64_t seg = k / 16; const float fr = (float) (k % 16) / 16.0f;
    const float a = hashU (s ^ (uint64_t) (seg * 104729 + 5)) * 2 - 1, b = hashU (s ^ (uint64_t) ((seg + 1) * 104729 + 5)) * 2 - 1;
    const float drift = (a + (b - a) * (fr * fr * (3 - 2 * fr))) * (1.5f + 5.0f * human);
    const float pushMs = -push * 9.0f - (activeSection == SChorus ? 2.0f : 0.0f);
    return jit + drift + pushMs;
}

void UndertowProcessor::rebuildBase (int sec, bool fresh)
{
    if (fresh) seeds[sec] = seeds[sec] * 1664525u + 1013904223u + (uint32_t) rng.i (1000);
    base[sec] = MotifWriter::write (feel, sec, seeds[sec], hook, density);
}

// the start of a 2-bar pass: decide how this time round will go
void UndertowProcessor::newPass (int64_t k)
{
    if (baseFeel != feel || std::abs (baseHook - hook) > 0.04f || std::abs (baseDensity - density) > 0.04f)
    {
        for (int s = 0; s < NumSections; ++s) rebuildBase (s, false);
        baseFeel = feel; baseHook = hook; baseDensity = density;
    }
    const int bar = barsInSection;
    if (bar > 0 && bar % 8 == 0 && rng.chance (variation * 0.35f)) rebuildBase (activeSection, true);   // the line evolves
    inst = MotifWriter::vary (base[activeSection], rng, variation, density);
    // phrase-end fills: end of every 4 bars sometimes, end of 8 bars more often
    const bool phraseEnd4 = (bar % 4) == 2, phraseEnd8 = (bar % 8) == 6;
    if ((phraseEnd8 && rng.chance (fills * 0.9f)) || (phraseEnd4 && rng.chance (fills * 0.45f)))
        MotifWriter::fill (inst, rng, feel, rng.chance (fills * (phraseEnd8 ? 0.7f : 0.3f)));
    juce::ignoreUnused (k);
    publishMotif();
}

void UndertowProcessor::publishMotif()
{
    const Chord c = currentChord().valid() ? currentChord() : Chord { 4, true };
    for (int i = 0; i < kSteps; ++i)
    {
        const auto& s = inst.s[(size_t) i];
        uiMotif[i] = s.on ? pitchFor (c, s) + (s.ghost ? 1000 : 0) : -1;
    }
}

void UndertowProcessor::playStep (int64_t k, int t, std::vector<Ev>& evs)
{
    juce::ignoreUnused (k);
    // the band changes section at a bar line
    const int64_t bar = k / 16;
    const bool selfSong = follow == FollowSelf && autoSong;
    if (motifPos % 16 == 0)
    {
        if (selfSong)
        {
            const auto sp = songPosition (bar, sectionBars);
            if (sp.section != activeSection) { activeSection = sp.section; motifPos = (sp.barIn % 2) * 16; barsInSection = sp.barIn; if (motifPos != 0) newPass (k); }
            barsInSection = sp.barIn;
            if (sp.lastBar) fillPending = true;               // the band fills into the next section
        }
        else if (section != activeSection) { activeSection = section; motifPos = 0; barsInSection = 0; }
    }
    if (follow == FollowSelf)
    {
        const Chord c = selfChordAt (k);
        if (! (c.root == selfChord.root && c.minor == selfChord.minor)) { selfChord = c; harmony.hear (c, 1.0f); }
    }
    if (motifPos == 0) newPass (k);
    if (motifPos % 16 == 0 && motifPos + 16 <= kSteps && fillPending)
    {
        // asked for a fill: put it at the end of this bar
        MotifWriter::fill (inst, rng, feel, rng.chance (0.5f), motifPos + 16);
        fillPending = false;
        publishMotif();
    }
    uiStep = motifPos;
    uiSection = activeSection;

    const Step st = inst.s[(size_t) motifPos];
    if (++motifPos >= kSteps) motifPos = 0;
    if (motifPos % 16 == 0) ++barsInSection;

    const Chord c = currentChord();
    const bool bandOn = follow == FollowSelf || (follow == FollowMidi ? heldChord.valid() : ears.playing());
    if (! c.valid() || (stopWithBand && ! bandOn))
    {
        if (sampler.currentNote >= 0) evs.push_back ({ t, 2, 0, 0, 0 });
        return;
    }
    if (! st.on) return;
    const int pitch = pitchFor (c, st);
    float vel = juce::jlimit (0.15f, 1.0f, st.vel + rng.n() * 0.07f * human);
    const double sixteenth = sr * 15.0 / juce::jmax (30.0, bpmNow);
    float lenSec = (float) (st.len * lengthK * sixteenth / sr) * (1.0f + rng.n() * 0.08f * human);
    if (st.ghost) { vel = juce::jmin (vel, 0.45f); lenSec = juce::jmin (lenSec, 0.07f); }
    evs.push_back ({ t, 1, pitch, vel, lenSec });
    lastStepPlayed = st; lastNoteChord = c; lastNoteAt = sampleClock + t; lastNoteLen = lenSec;
}

void UndertowProcessor::runPlayer (int n, std::vector<Ev>& evs)
{
    double ppq = 0, bpm = 120; bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto p = pos->getPpqPosition()) ppq = *p; else playing = false;
        }
    bpmNow = bpm;
    uiTransport = playing;
    // the guitar has stopped if it's been quiet for about a beat
    ears.silentLimit = juce::jlimit (10, 200, (int) (60.0 / bpm / 0.0116));

    if (! playing)
    {
        if (wasPlaying) { evs.push_back ({ 0, 2, 0, 0, 0 }); uiStep = -1; }
        wasPlaying = false; return;
    }
    const double spb = sr * 60.0 / bpm;
    if (! wasPlaying || ppq < lastPpqEnd - 0.02 || ppq > lastPpqEnd + 0.25)
    {
        // (re)locate: start of a run, or the playhead jumped
        lastStep = (int64_t) std::ceil (ppq * 4.0 - 0.001) - 1;
        const int64_t k = lastStep + 1;
        motifPos = (int) (((k % kSteps) + kSteps) % kSteps);
        barsInSection = (int) ((k / 16) % 8);
        activeSection = section;
        if (follow == FollowSelf)
        {
            if (autoSong)
            {
                const auto sp = songPosition (k / 16, sectionBars);
                activeSection = sp.section; barsInSection = sp.barIn;
                motifPos = (sp.barIn % 2) * 16 + (int) (((k % 16) + 16) % 16);
            }
            selfChord = selfChordAt (k);
            // let the harmony know the key before the first note
            for (int s = 0; s < NumSections; ++s)
                for (auto& pc : progressions()[(size_t) progs[s]].chords) harmony.hear ({ (key + pc.off) % 12, pc.minor }, 0.5f);
        }
        if (! wasPlaying) runSeed = hash ((uint64_t) juce::Time::getHighResolutionTicks());
        newPass (k);
        // newPass assumed bar alignment; keep the current position in the motif
    }
    wasPlaying = true;
    for (int guard = 0; guard < 64; ++guard)
    {
        const int64_t k = lastStep + 1;
        double t = ((double) k / 4.0 - ppq) * spb + offsetMs (k) * 0.001 * sr;
        // swing: the "&" of each beat and the odd 16ths lean late
        if (k % 4 == 2) t += swing * 0.33 * spb / 2.0;
        else if (k % 2 == 1) t += swing * 0.33 * spb / 4.0;
        if (t >= n) break;
        lastStep = k;
        playStep (k, (int) juce::jmax (0.0, t), evs);
    }
    lastPpqEnd = ppq + n / spb;
}

// ---------------------------------------------------------------------------
void UndertowProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    if ((int) sc.size() < n) { sc.resize ((size_t) n); mono.resize ((size_t) n); tmpR.resize ((size_t) n); }
    readParams();

    // 1. the guitar on the sidechain
    auto side = getBusBuffer (buffer, true, 0);
    const bool haveSide = side.getNumChannels() > 0;
    uiSidechain = haveSide;
    if (haveSide)
    {
        const float k = 1.0f / (float) side.getNumChannels();
        for (int i = 0; i < n; ++i)
        {
            float v = 0;
            for (int c = 0; c < side.getNumChannels(); ++c) v += side.getReadPointer (c)[i];
            sc[(size_t) i] = v * k;
        }
    }
    else std::fill (sc.begin(), sc.begin() + n, 0.0f);
    buffer.clear();

    events.clear();
    // 2. MIDI in (on-screen keyboard too)
    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    for (const auto meta : midi) handleMidi (meta.getMessage(), events, meta.samplePosition);
    midi.clear();
    if (uiFill.exchange (false)) fillPending = true;
    if (uiNewLine.exchange (false)) { rebuildBase (activeSection, true); inst = base[activeSection]; publishMotif(); }

    // chord from held MIDI notes: lowest note is the root, a minor third makes it minor
    {
        int lo = -1; bool min3 = false, maj3 = false;
        for (int nn = 24; nn < 128; ++nn) if (held[(size_t) nn]) { lo = nn; break; }
        if (lo >= 0)
        {
            for (int nn = lo + 1; nn < 128; ++nn)
                if (held[(size_t) nn]) { const int iv = (nn - lo) % 12; min3 |= iv == 3; maj3 |= iv == 4; }
            heldChord = { lo % 12, min3 && ! maj3 };
            if (! (heldChord.root == midiChord.root && heldChord.minor == midiChord.minor)) { midiChord = heldChord; harmony.hear (midiChord, 1.0f); }
        }
        else heldChord = {};
    }

    // 3. ears
    if (haveSide) ears.push (sc.data(), n);
    uiInput = ears.level.load();

    if (mode == MPlayer)
    {
        // a chord change right after we played: fix the note like a real player would
        const int ch = ears.changes.load();
        const Chord c = currentChord();
        const bool changed = follow == FollowGuitar ? ch != lastChordChanges : ! (c.root == lastNoteChord.root && c.minor == lastNoteChord.minor);
        if (follow == FollowGuitar && changed) harmony.hear (c, 1.0f);
        if (follow == FollowGuitar) { if (++harmonyTick % 64 == 0) harmony.hear (c, 0.3f); }
        lastChordChanges = ch;
        if (follow != FollowSelf && changed && c.valid() && lastNoteChord.valid() && lastStepPlayed.on && sampler.currentNote >= 0
            && sampleClock - lastNoteAt < (int64_t) (sr * 0.12))
        {
            const float left = lastNoteLen - (float) (sampleClock - lastNoteAt) / (float) sr;
            if (left > 0.04f) { events.push_back ({ 0, 1, pitchFor (c, lastStepPlayed), lastStepPlayed.vel * 0.92f, left }); lastNoteChord = c; }
        }
        runPlayer (n, events);
    }
    else
    {
        if (wasPlaying) { wasPlaying = false; uiStep = -1; }
        uiTransport = false;
        if (mode == MTracked && haveSide)
        {
            tracker.push (sc.data(), n, sens, [&] (const PitchTracker::Event& e)
            {
                if (e.type == 1) events.push_back ({ e.offset, 1, juce::jlimit (0, 127, e.note - 12 + keysOct * 12), e.vel, -1.0f });
                else events.push_back ({ e.offset, 2, 0, 0, 0 });
            });
        }
    }
    {
        const Chord c = currentChord();
        uiChord = c.valid() ? c.root | (c.minor ? 16 : 0) : -1;
        uiBandPlaying = follow == FollowSelf ? uiTransport.load() : (follow == FollowMidi ? heldChord.valid() : ears.playing());
    }

    // 4. render the bass
    std::stable_sort (events.begin(), events.end(), [] (const Ev& a, const Ev& b) { return a.t < b.t; });
    size_t e = 0;
    for (int i = 0; i < n; ++i)
    {
        while (e < events.size() && events[e].t <= i)
        {
            const auto& ev = events[e++];
            if (midiOutNote >= 0) { midi.addEvent (juce::MidiMessage::noteOff (1, midiOutNote), i); midiOutNote = -1; midiOffIn = -1; }
            if (ev.type == 1)
            {
                sampler.noteOn (ev.note, ev.vel, ev.len, rng.n() * (1.5f + 3.5f * human));
                midi.addEvent (juce::MidiMessage::noteOn (1, ev.note, (juce::uint8) juce::jlimit (1, 127, (int) (ev.vel * 127.0f))), i);
                midiOutNote = ev.note; midiOffIn = ev.len > 0 ? (int) (ev.len * sr) : -1;
                uiNote = ev.note;
            }
            else { sampler.noteOff(); uiNote = -1; }
        }
        if (midiOffIn > 0 && --midiOffIn == 0) { midi.addEvent (juce::MidiMessage::noteOff (1, midiOutNote), i); midiOutNote = -1; midiOffIn = -1; }
        mono[(size_t) i] = sampler.render();
    }
    if (sampler.currentNote < 0 && midiOutNote < 0) uiNote = -1;

    if (mode == MOctave)
        for (int i = 0; i < n; ++i) mono[(size_t) i] += octave.process (sc[(size_t) i]) * 1.8f;

    // 5. the rig
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : tmpR.data();
    rig.process (mono.data(), L, R, n);
    if ((mode == MOctave || mode == MTracked) && guitarDry > 0.001f)
        for (int i = 0; i < n; ++i) { L[i] += sc[(size_t) i] * guitarDry; R[i] += sc[(size_t) i] * guitarDry; }
    // safety: never let a runaway setting blow up the monitors
    for (int i = 0; i < n; ++i)
    {
        if (std::abs (L[i]) > 0.9f) L[i] = std::copysign (0.9f + 0.1f * std::tanh ((std::abs (L[i]) - 0.9f) * 10.0f), L[i]);
        if (std::abs (R[i]) > 0.9f) R[i] = std::copysign (0.9f + 0.1f * std::tanh ((std::abs (R[i]) - 0.9f) * 10.0f), R[i]);
    }
    sampleClock += n;
}

// ---------------------------------------------------------------------------
void UndertowProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("seeds", juce::String (seeds[0]) + "," + juce::String (seeds[1]) + "," + juce::String (seeds[2]), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void UndertowProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto t = juce::ValueTree::fromXml (*xml);
            juce::StringArray s; s.addTokens (t.getProperty ("seeds").toString(), ",", "");
            if (s.size() == 3) for (int i = 0; i < 3; ++i) seeds[i] = (uint32_t) s[i].getLargeIntValue();
            t.removeProperty ("seeds", nullptr);
            apvts.replaceState (t);
            baseFeel = -1;
        }
}

juce::AudioProcessorEditor* UndertowProcessor::createEditor() { return new UndertowEditor (*this); }

#ifndef UNDERTOW_NO_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new UndertowProcessor(); }
#endif
