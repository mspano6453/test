#include "HarmoniaProcessor.h"
#include "HarmoniaEditor.h"

using namespace hm;

namespace
{
juce::String pct (float v, int) { return juce::String (juce::roundToInt (v * 100)) + "%"; }
juce::String dB1 (float v, int) { return juce::String (v, 1) + " dB"; }
juce::StringArray feelNames() { juce::StringArray a; for (int i = 0; i < NumFeels; ++i) a.add (feelName (i)); return a; }
juce::StringArray groupNames() { juce::StringArray a; for (int i = 0; i < NumGroups; ++i) a.add (groupName (i)); return a; }

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

juce::AudioProcessorValueTreeState::ParameterLayout HarmoniaProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> ps;
    auto F = [&] (const char* id, const char* name, juce::NormalisableRange<float> r, float def, juce::String (*fmt) (float, int))
    { ps.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, r, def, juce::AudioParameterFloatAttributes().withStringFromValueFunction (fmt))); };
    auto C = [&] (const char* id, const char* name, juce::StringArray items, int def)
    { ps.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def)); };
    C ("group", "Singers", groupNames(), 0);
    C ("feel", "Feel", feelNames(), 0);
    C ("follow", "Chords From", { "Band (Bandleader / own song)", "MIDI chords", "Play it (MIDI keys)" }, 0);
    C ("sit_out", "With The Band", { "Plays along", "Sits out" }, 0);
    C ("parts", "Parts", { "2 parts", "3 parts", "4 parts" }, 1);
    C ("register", "Register", { "Low", "Middle", "High" }, 1);
    C ("key", "Key", { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 9);
    C ("prog_verse", "Verse Chords", ut::progressionNames(), 0);
    C ("prog_chorus", "Chorus Chords", ut::progressionNames(), 2);
    C ("prog_bridge", "Bridge Chords", ut::progressionNames(), 5);
    C ("chord_bars", "Bars Per Chord", { "1", "2", "4" }, 0);
    C ("song", "Song", { "Auto song", "Manual" }, 0);
    C ("section_bars", "Section Length", { "4 bars", "8 bars", "16 bars", "12 bars" }, 1);
    C ("section", "Section", { "Verse", "Chorus", "Bridge" }, 0);
    F ("busy", "Busy", { 0, 1, 0.001f }, 0.45f, pct);
    F ("dynamics", "Dynamics", { 0, 1, 0.001f }, 0.65f, pct);
    F ("human", "Human", { 0.1f, 1, 0.001f }, 0.6f, pct);
    F ("vibrato", "Vibrato", { 0, 1, 0.001f }, 0.45f, pct);
    F ("breath", "Breath", { 0, 1, 0.001f }, 0.3f, pct);
    F ("ensemble", "Ensemble", { 0, 1, 0.001f }, 0.5f, pct);
    F ("swell", "Swell", { 0, 1, 0.001f }, 0.4f, pct);
    F ("bright", "Bright", { 0, 1, 0.001f }, 0.45f, pct);
    F ("room", "Room", { 0, 1, 0.001f }, 0.45f, pct);
    F ("width", "Width", { 0, 1, 0.001f }, 0.7f, pct);
    F ("out", "Output", { -24, 6, 0.1f }, -3, dB1);
    C ("take_style", "Take Style From Bandleader", { "Off", "On" }, 1);
    return { ps.begin(), ps.end() };
}

HarmoniaProcessor::HarmoniaProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Harmonia", createLayout())
{
    band::available();
    queue.reserve (1024); bar.reserve (32);
    outMidi.ensureSize (2048);
    renderer = std::make_unique<mc::SongRenderer> (*this, band::WHarmonia, mc::SongRenderer::partFileName (7, "Backing Vocals"),
                                                   [] { return std::make_unique<HarmoniaProcessor>(); },
                                                   [] (juce::AudioProcessor& p) { mc::setChoice (p, "follow", 0); });
}

void HarmoniaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    for (int i = 0; i < kNumSingers; ++i) singers[i].prepare (sr, 0x9e3779b9u * (uint32_t) (i + 1));
    room.prepare (sr, juce::jmax (512, samplesPerBlock));
    const size_t n = (size_t) juce::jmax (512, samplesPerBlock);
    tmpL.assign (n, 0.0f); tmpR.assign (n, 0.0f);
    queue.clear();
    lastStep = -1; lastPpqEnd = -1; wasPlaying = false; curBar = -1; singing = false;
    for (auto& p : partNote) p = -1;
    held.fill (false);
    rng.seed ((uint32_t) juce::Time::getMillisecondCounter() | 1u);
}

void HarmoniaProcessor::readParams()
{
    auto g = [&] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    group = (int) g ("group"); feel = (int) g ("feel"); follow = (int) g ("follow"); parts = 2 + (int) g ("parts"); reg = (int) g ("register");
    key = (int) g ("key"); progs[0] = (int) g ("prog_verse"); progs[1] = (int) g ("prog_chorus"); progs[2] = (int) g ("prog_bridge");
    chordBars = 1 << (int) g ("chord_bars"); autoSong = g ("song") < 0.5f; sectionBars = ut::sectionBarsFor ((int) g ("section_bars"));
    section = (int) g ("section");
    busy = g ("busy"); dynamics = g ("dynamics"); human = g ("human"); vibrato = g ("vibrato"); breath = g ("breath");
    ensemble = g ("ensemble"); swell = g ("swell"); bright = g ("bright"); roomAmt = g ("room"); width = g ("width");
    // every group and feel about as loud as the others
    static const float groupTrim[NumGroups] = { 1.0f, 1.1f, 1.05f, 0.85f };
    const float feelTrim = feel == FBaBa ? 1.6f : feel == FGospel ? 1.25f : feel == FAnswers ? 1.3f : 1.0f;
    outGain = juce::Decibels::decibelsToGain (g ("out")) * 0.55f * groupTrim[juce::jlimit (0, NumGroups - 1, group)] * feelTrim;
}

int HarmoniaProcessor::sectionAt (int64_t b, int& barIn, bool& lastBar) const
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

void HarmoniaProcessor::chordAt (int64_t b, int& root, int& q) const
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

// all the parts sing the chord (in close harmony, moving as little as they can)
void HarmoniaProcessor::singChord (int64_t at, int root, int q, const Hit& h, bool legato)
{
    const int centers[3] = { 57, 64, 70 };
    int center = centers[juce::jlimit (0, 2, reg)];
    if (group == GGirls) center += 3;
    if (group == GMen) center -= 7;
    int notes[kMaxParts];
    voiceParts (root, q, parts, center, partNote, partNote[0] >= 0, notes);
    const double sixteenth = sr * 15.0 / juce::jmax (30.0, bpmNow);
    const float lenSec = h.len < 0 ? -1.0f : (float) (h.len * sixteenth / sr);
    const bool pad = h.len < 0;
    const float attack = h.swell ? (float) (sixteenth * 4 / sr * (0.5 + swell)) : pad ? 0.06f + 0.5f * swell : 0.018f;
    const float energy = led ? (leader.energy - 0.5f) * 0.2f : 0.0f;
    for (int p = 0; p < parts; ++p)
    {
        const float v = juce::jlimit (0.05f, 1.0f, h.vel * (0.55f + 0.6f * dynamics) * (p == 0 ? 1.0f : 0.86f) + rng.n() * 0.05f * human + energy);
        schedule ({ at + (int64_t) (rng.u() * 0.012 * human * sr), 1, p, notes[p], v, lenSec, h.syl, legato, attack });
        partNote[p] = notes[p];
    }
    singing = true; lastSyl = h.syl;
}

void HarmoniaProcessor::playStep (int64_t k, int64_t at)
{
    const int64_t b = k / bs;
    const int s = (int) (((k % bs) + bs) % bs);
    if (b != curBar)
    {
        curBar = b;
        bool lastBar = false;
        barSection = sectionAt (b, barInSec, lastBar);
        writeBar (feel, barSection, barInSec, busy, meterNow, rng, bar);
        if (led && leader.isHalf (b)) { bar.clear(); bar.push_back ({ 0, -1.0f, 0.66f, { VOo, ONone }, true }); }   // the breakdown: a soft "ooh"
        uiSection = barSection;
    }
    uiStep = s;
    // the last chord: everybody, "aah", held
    if (led && leader.isEnding (b))
    {
        if (leader.isFinalHit (b) && s == 0)
        {
            int r = 0, q = 0; chordAt (b, r, q);
            if (r >= 0) singChord (at, r, q, { 0, (float) (bs * 2) * 0.95f, 0.9f, { VAh, ONone }, false }, singing);
        }
        return;
    }
    const bool out = (led && leader.rests (b, false, true)) || (follow == FMidiChords && midiRoot < 0);
    if (out) { if (! bandOut) { schedule ({ at, 3, 0, 0, 0, 0, {}, false, 0 }); bandOut = true; singing = false; } return; }
    bandOut = false;
    int r = 0, q = 0; chordAt (b, r, q);
    if (r < 0) return;
    const bool changed = r != lastRoot || q != lastQ;
    lastRoot = r; lastQ = q;
    uiChord = r | (q << 4);
    bool any = false; for (auto& h : bar) if (h.step == s) any = true;
    // a held "ooh" that the chord moves under: the parts glide to the new chord
    if (changed && ! any && singing && s != 0 && ! bar.empty() && bar.front().len < 0)
    {
        singChord (at, r, q, bar.front(), true);
        return;
    }
    if (! any) return;
    for (const auto& h : bar)
    {
        if (h.step != s) continue;
        // a pad that's already holding this chord and syllable keeps holding it
        if (h.len < 0 && singing && ! changed && h.syl.vowel == lastSyl.vowel && barInSec % 4 != 0 && ! h.swell) continue;
        const bool legato = h.len < 0 && singing && h.syl.onset == ONone;
        singChord (at, r, q, h, legato);
        uiVowel = h.syl.vowel;
    }
}

void HarmoniaProcessor::runPlayer (int n)
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
    {
        const int m = led ? leader.meter : band::hostMeter (getPlayHead());
        if (m != meterNow) { meterNow = m; curBar = -1; }
        bs = band::meterSteps (meterNow); uiSteps = bs;
        capture.setMeter (meterNow);
    }
    if (! playing)
    {
        if (wasPlaying) { schedule ({ sampleClock, 3, 0, 0, 0, 0, {}, false, 0 }); uiStep = -1; singing = false; }
        wasPlaying = false; return;
    }
    const double spb = sr * 60.0 / bpm;
    if (! wasPlaying || ppq < lastPpqEnd - 0.02 || ppq > lastPpqEnd + 0.25)
    {
        lastStep = (int64_t) std::ceil (ppq * 4.0 - 0.001) - 1;
        curBar = -1; lastRoot = -1; singing = false;
        for (auto& p : partNote) p = -1;
        if (wasPlaying) schedule ({ sampleClock, 3, 0, 0, 0, 0, {}, false, 0 });
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

void HarmoniaProcessor::handle (const Ev& e, juce::MidiBuffer& out, int pos)
{
    const int spp = singersPerPart (group);
    const float w = 0.4f + 0.6f * width;
    auto partPan = [&] (int p) { return parts <= 1 ? 0.0f : (-0.5f + (float) p / (float) (parts - 1)) * w; };
    switch (e.type)
    {
        case 1:
        {
            const bool female = partIsFemale (group, e.part, parts);
            for (int i = 0; i < kPerPart; ++i)
            {
                auto& sg = singers[e.part * kPerPart + i];
                if (i >= spp) { if (sg.isActive()) sg.release (0.2); continue; }
                const float det = (spp > 1 ? (i == 0 ? 0.0f : (i == 1 ? 1.0f : -1.0f)) * (4.0f + 10.0f * ensemble) : 0.0f) + rng.n() * 3.0f * human;
                const float dly = i == 0 ? 0.0f : rng.u() * 0.03f * ensemble;
                sg.part = e.part;
                sg.sing (e.note, e.vel * (i == 0 ? 1.0f : 0.8f), e.syl, e.legato, e.attack, female, vibrato * (0.7f + 0.6f * rng.u()), breath, det, dly);
                if (e.len > 0) { /* timed notes stop themselves below */ }
            }
            if (partMidi[e.part] >= 0) out.addEvent (juce::MidiMessage::noteOff (e.part + 1, partMidi[e.part]), pos);
            out.addEvent (juce::MidiMessage::noteOn (e.part + 1, e.note, (juce::uint8) juce::jlimit (1, 127, (int) (e.vel * 127))), pos);
            partMidi[e.part] = e.note;
            uiPartNote[juce::jlimit (0, kMaxParts - 1, e.part)] = e.note;
            if (e.len > 0) schedule ({ e.at + (int64_t) (e.len * sr), 2, e.part, e.note, 0, 0, {}, false, 0 });
            break;
        }
        case 2:
            if (partMidi[e.part] != e.note && e.note >= 0) break;    // a newer note has already taken over
            for (int i = 0; i < kPerPart; ++i) singers[e.part * kPerPart + i].release (0.18);
            if (partMidi[e.part] >= 0) out.addEvent (juce::MidiMessage::noteOff (e.part + 1, partMidi[e.part]), pos);
            partMidi[e.part] = -1; uiPartNote[juce::jlimit (0, kMaxParts - 1, e.part)] = -1;
            break;
        case 3:
            for (int i = 0; i < kMaxParts * kPerPart; ++i) singers[i].release (0.3);
            for (int p = 0; p < kMaxParts; ++p) { if (partMidi[p] >= 0) out.addEvent (juce::MidiMessage::noteOff (p + 1, partMidi[p]), pos); partMidi[p] = -1; uiPartNote[p] = -1; }
            break;
        case 5:
        {
            const int v = playNext; playNext = (playNext + 1) % kPlayVoices;
            auto& sg = singers[kMaxParts * kPerPart + v];
            sg.sing (e.note, e.vel, syllableFor (feel, band::Verse), false, 0.05 + 0.4 * swell, group == GGirls || (group != GMen && e.note >= 60), vibrato, breath, rng.n() * 4.0f, 0);
            playNote[v] = e.note;
            out.addEvent (juce::MidiMessage::noteOn (1, e.note, (juce::uint8) juce::jlimit (1, 127, (int) (e.vel * 127))), pos);
            break;
        }
        case 6:
            for (int v = 0; v < kPlayVoices; ++v) if (playNote[v] == e.note) { singers[kMaxParts * kPerPart + v].release (0.25); playNote[v] = -1; }
            out.addEvent (juce::MidiMessage::noteOff (1, e.note), pos);
            break;
        default: break;
    }
    juce::ignoreUnused (partPan);
}

void HarmoniaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if ((int) tmpL.size() < n) return;
    readParams();
    band::markPresent (band::WHarmonia);
    const bool wasLed = led;
    led = follow != FPlay && band::readLeader (leader);
    if (led && leader.mutateStamp != lastMutate) { lastMutate = leader.mutateStamp; rng.seed (leader.mutateStamp * 2654435761u + 13u); curBar = -1; }
    uiLinked = led;
    if (led != wasLed) lastPpqEnd = -1e9;

    bool chordKeys = false;
    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        const int64_t at = sampleClock + m.samplePosition;
        if (msg.isNoteOn()) { held[(size_t) msg.getNoteNumber()] = true; chordKeys = true; if (follow == FPlay) schedule ({ at, 5, 0, msg.getNoteNumber(), msg.getFloatVelocity(), -1, {}, false, 0 }); }
        else if (msg.isNoteOff()) { held[(size_t) msg.getNoteNumber()] = false; chordKeys = true; if (follow == FPlay) schedule ({ at, 6, 0, msg.getNoteNumber(), 0, 0, {}, false, 0 }); }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff()) { held.fill (false); schedule ({ at, 3, 0, 0, 0, 0, {}, false, 0 }); }
    }
    if (chordKeys) chordFromHeld (held, midiRoot, midiQ);

    const bool sitOut = apvts.getRawParameterValue ("sit_out")->load() > 0.5f || (led && leader.off (band::WHarmonia));
    if (follow != FPlay && ! sitOut) runPlayer (n);
    else { uiTransport = false; if (wasPlaying) { schedule ({ sampleClock, 3, 0, 0, 0, 0, {}, false, 0 }); wasPlaying = false; singing = false; } }

    std::stable_sort (queue.begin(), queue.end(), [] (const Ev& a, const Ev& b) { return a.at < b.at; });
    outMidi.clear();
    float* L = tmpL.data(); float* R = tmpR.data();
    std::fill (L, L + n, 0.0f); std::fill (R, R + n, 0.0f);
    const int64_t end = sampleClock + n;
    int pos = 0;
    const int spp = singersPerPart (group);
    const float w = 0.4f + 0.6f * width;
    auto renderTo = [&] (int to)
    {
        if (to <= pos) return;
        for (int i = 0; i < kNumSingers; ++i)
        {
            if (! singers[i].isActive()) continue;
            float pan = 0.0f;
            if (i < kMaxParts * kPerPart)
            {
                const int p = i / kPerPart, k = i % kPerPart;
                pan = (parts <= 1 ? 0.0f : (-0.5f + (float) p / (float) (parts - 1)) * w) + (spp > 1 ? (k == 0 ? 0.0f : k == 1 ? 0.12f : -0.12f) * w : 0.0f);
            }
            singers[i].render (L + pos, R + pos, to - pos, juce::jlimit (-0.9f, 0.9f, pan));
        }
        pos = to;
    };
    // events can add events (a timed note schedules its own end), so walk by index
    for (size_t e = 0; e < queue.size();)
    {
        if (queue[e].at >= end) { ++e; continue; }
        const Ev ev = queue[e];
        queue.erase (queue.begin() + (long) e);
        const int at = (int) juce::jlimit<int64_t> (0, n - 1, ev.at - sampleClock);
        renderTo (at);
        handle (ev, outMidi, at);
        std::stable_sort (queue.begin(), queue.end(), [] (const Ev& a, const Ev& b) { return a.at < b.at; });
        e = 0;
    }
    renderTo (n);
    // the stack gets louder with more singers: keep it level
    const float norm = 0.55f / std::sqrt ((float) (parts * spp));
    for (int i = 0; i < n; ++i) { L[i] *= norm; R[i] *= norm; }
    room.process (L, R, n, roomAmt, bright, width, outGain * 1.4f);
    float* oL = buffer.getWritePointer (0);
    float* oR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    float pk = 0;
    for (int i = 0; i < n; ++i)
    {
        float l = L[i], r = R[i];
        if (std::abs (l) > 0.9f) l = std::copysign (0.9f + 0.1f * std::tanh ((std::abs (l) - 0.9f) * 10.0f), l);
        if (std::abs (r) > 0.9f) r = std::copysign (0.9f + 0.1f * std::tanh ((std::abs (r) - 0.9f) * 10.0f), r);
        oL[i] = std::isfinite (l) ? l : 0.0f;
        if (oR) oR[i] = std::isfinite (r) ? r : 0.0f;
        pk = juce::jmax (pk, std::abs (l));
    }
    uiLevel = juce::jmax (pk, uiLevel.load() * 0.9f);
    fade.apply (buffer, n, led, leader, getPlayHead());
    midi.swapWith (outMidi);
    {
        double cp = 0; bool cpl = false;
        if (auto* ph = getPlayHead()) if (auto p = ph->getPosition()) { cpl = p->getIsPlaying(); if (auto q = p->getPpqPosition()) cp = *q; else cpl = false; }
        capture.recordBlock (midi, cp, cpl, bpmNow, sr, n);
    }
    sampleClock += n;
}

void HarmoniaProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void HarmoniaProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

void HarmoniaProcessor::applyStyle (const band::StyleDef& s)
{
    band::setParam (apvts, "sit_out", s.vOn ? 0.0f : 1.0f);
    if (! s.vOn) return;
    band::setParam (apvts, "group", (float) s.vVoice);
    band::setParam (apvts, "feel", (float) s.vFeel);
    band::setParam (apvts, "busy", s.vBusy);
    band::setParam (apvts, "room", juce::jlimit (0.2f, 0.8f, 0.3f + s.lSpace * 0.6f));
}

juce::AudioProcessorEditor* HarmoniaProcessor::createEditor() { return new HarmoniaEditor (*this); }

#ifndef HARMONIA_NO_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HarmoniaProcessor(); }
#endif
