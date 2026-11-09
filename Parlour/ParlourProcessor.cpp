#include "ParlourProcessor.h"
#include "ParlourEditor.h"
#include "ParlourData.h"

using namespace pk;

namespace
{
juce::String pct (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
juce::String dB1 (float v, int) { return juce::String (v, 1) + " dB"; }
juce::String bip (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
}

juce::AudioProcessorValueTreeState::ParameterLayout ParlourProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> ps;
    auto F = [&] (const char* id, const char* name, juce::NormalisableRange<float> r, float def, juce::String (*fmt) (float, int))
    {
        ps.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, r, def,
                                                                   juce::AudioParameterFloatAttributes().withStringFromValueFunction (fmt)));
    };
    auto C = [&] (const char* id, const char* name, juce::StringArray items, int def)
    { ps.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def)); };

    C ("instrument", "Instrument", instrumentNames(), 0);
    C ("feel", "Feel", feelNames(), 1);
    C ("follow", "Chords From", { "Band (Bandleader / own song)", "MIDI chords", "Play it (MIDI keys)" }, 0);
    C ("sit_out", "With The Band", { "Plays along", "Sits out" }, 0);
    C ("key", "Key", { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 9);
    C ("prog_verse", "Verse Chords", ut::progressionNames(), 0);
    C ("prog_chorus", "Chorus Chords", ut::progressionNames(), 2);
    C ("prog_bridge", "Bridge Chords", ut::progressionNames(), 5);
    C ("chord_bars", "Bars Per Chord", { "1", "2", "4" }, 0);
    C ("song", "Song", { "Auto song", "Manual" }, 0);
    C ("section_bars", "Section Length", { "4 bars", "8 bars", "16 bars", "12 bars" }, 1);
    C ("section", "Section", { "Verse", "Chorus", "Bridge" }, 0);
    C ("register", "Register", { "Low", "Middle", "High" }, 1);
    C ("left_hand", "Left Hand", { "Off (the bass has it)", "On" }, 0);
    C ("band_stop", "When The Band Stops", { "Stop with the band", "Keep playing" }, 0);
    F ("busy", "Busy", { 0, 1, 0.001f }, 0.5f, pct);
    F ("colour", "Colour (7ths, 9ths)", { 0, 1, 0.001f }, 0.3f, pct);
    F ("dynamics", "Dynamics", { 0, 1, 0.001f }, 0.65f, pct);
    F ("human", "Human", { 0.1f, 1, 0.001f }, 0.6f, pct);
    F ("swing", "Swing", { 0, 0.5f, 0.001f }, 0, pct);
    F ("length", "Note Length", { 0.3f, 1.5f, 0.001f }, 1.0f, pct);
    // the sound
    F ("motion", "Motion (tremolo / rotary)", { 0, 1, 0.001f }, 0.3f, pct);
    C ("rotor", "Rotary Speed", { "Slow", "Fast" }, 0);
    F ("chorus", "Chorus", { 0, 1, 0.001f }, 0.1f, pct);
    F ("lofi", "Lo-fi", { 0, 1, 0.001f }, 0, pct);
    F ("tone", "Tone", { -1, 1, 0.001f }, 0, bip);
    F ("room", "Room", { 0, 1, 0.001f }, 0.2f, pct);
    F ("out", "Output", { -24, 6, 0.1f }, -3, dB1);
    ps.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "take_style", 1 }, "Take Style From Bandleader", juce::StringArray { "Off", "On" }, 1));
    return { ps.begin(), ps.end() };
}

ParlourProcessor::ParlourProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parlour", createLayout())
{
    band::available();
    queue.reserve (1024); bar.reserve (64);
    loadSamples();
}

// Real instruments from the Versilian Community Sample Library (CC0): upright piano ("pn_"),
// vibraphone ("vb_") and glockenspiel ("gl_"), each with a soft and a hard layer
void ParlourProcessor::loadSamples()
{
    juce::FlacAudioFormat flac;
    const char* prefixes[3] = { "pn_", "vb_", "gl_" };
    for (int i = 0; i < ParlourData::namedResourceListSize; ++i)
    {
        const juce::String res (ParlourData::namedResourceList[i]);
        int set = -1;
        for (int s = 0; s < 3; ++s) if (res.startsWith (prefixes[s])) set = s;
        if (set < 0) continue;
        juce::StringArray bits; bits.addTokens (res, "_", "");
        if (bits.size() < 3) continue;
        int size = 0;
        const char* data = ParlourData::getNamedResource (res.toRawUTF8(), size);
        if (data == nullptr) continue;
        std::unique_ptr<juce::AudioFormatReader> reader (flac.createReaderFor (new juce::MemoryInputStream (data, (size_t) size, false), true));
        if (reader == nullptr) continue;
        const int len = (int) reader->lengthInSamples;
        juce::AudioBuffer<float> tmp (2, len);
        reader->read (&tmp, 0, len, 0, true, true);
        Zone z;
        z.root = bits[1].getIntValue(); z.layer = bits[2].getIntValue(); z.sr = reader->sampleRate;
        z.l.assign (tmp.getReadPointer (0), tmp.getReadPointer (0) + len);
        z.r.assign (tmp.getReadPointer (reader->numChannels > 1 ? 1 : 0), tmp.getReadPointer (reader->numChannels > 1 ? 1 : 0) + len);
        for (int k = 0; k < 2; ++k) { z.l.push_back (0); z.r.push_back (0); }
        lib.sets[set].zones.push_back (std::move (z));
        ++loaded;
    }
}

void ParlourProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    for (auto& v : voices) { v.prepare (sr); v.kill(); }
    rack.prepare (sr, samplesPerBlock);
    const size_t n = (size_t) juce::jmax (512, samplesPerBlock);
    tmpL.assign (n, 0.0f); tmpR.assign (n, 0.0f);
    queue.clear();
    lastStep = -1; lastPpqEnd = -1; wasPlaying = false; curBar = -1;
    held.fill (false); sustained.fill (false);
    voicing = {}; lastChord = {};
    rng.seed ((uint32_t) juce::Time::getMillisecondCounter() | 1u);
}

void ParlourProcessor::readParams()
{
    auto g = [&] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    instrument = (int) g ("instrument"); feel = (int) g ("feel"); follow = (int) g ("follow");
    key = (int) g ("key"); progs[0] = (int) g ("prog_verse"); progs[1] = (int) g ("prog_chorus"); progs[2] = (int) g ("prog_bridge");
    chordBars = 1 << (int) g ("chord_bars"); autoSong = g ("song") < 0.5f; sectionBars = ut::sectionBarsFor ((int) g ("section_bars"));
    section = (int) g ("section"); reg = (int) g ("register"); leftHand = g ("left_hand") > 0.5f; stopWithBand = g ("band_stop") < 0.5f;
    busy = g ("busy"); colour = g ("colour"); dynamics = g ("dynamics"); human = g ("human"); swing = g ("swing"); lengthK = g ("length");
    if (instrument != lastInstrument) { if (lastInstrument >= 0) allOff(); lastInstrument = instrument; }
    Rack::Params p;
    p.instrument = instrument; p.motion = g ("motion"); p.fastRotor = g ("rotor") > 0.5f; p.chorus = g ("chorus");
    p.lofi = g ("lofi"); p.tone = g ("tone"); p.room = g ("room"); p.out = juce::Decibels::decibelsToGain (g ("out"));
    rack.set (p);
}

// ---- voices
void ParlourProcessor::noteOn (int note, float vel, float len)
{
    if (note < 0 || note > 127) return;
    // the same key again: let the old one go (a real key can't be struck twice at once)
    for (auto& v : voices) if (v.active() && v.note == note && ! v.held) v.kill();
    for (auto& v : voices) if (v.active() && v.note == note) v.release();
    Voice* free = nullptr;
    for (auto& v : voices) if (! v.active()) { free = &v; break; }
    if (free == nullptr)
    {
        // steal the oldest released voice, else the oldest one
        for (auto& v : voices) if (! v.held && (free == nullptr || v.age > free->age)) free = &v;
        if (free == nullptr) for (auto& v : voices) if (free == nullptr || v.age > free->age) free = &v;
    }
    free->start (instrument, note, juce::jlimit (0.05f, 1.0f, vel), len, &lib, rng);
}
void ParlourProcessor::noteOff (int note) { for (auto& v : voices) if (v.active() && v.note == note && v.held) v.release(); }
void ParlourProcessor::allOff() { for (auto& v : voices) v.release(); }

// ---- the song
int ParlourProcessor::sectionAt (int64_t b, int& barIn, bool& lastBar) const
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

KChord ParlourProcessor::chordAt (int64_t b) const
{
    if (follow == FMidiChords) return midiChord;
    if (led) { int r = 0; bool m = false; leader.chordAt (b, r, m); return { r, leader.qualityAt (b) }; }
    int barIn = 0; bool lb = false;
    const int sec = sectionAt (b, barIn, lb);
    const auto& ps = ut::progressions();
    const auto& prog = ps[(size_t) juce::jlimit (0, (int) ps.size() - 1, progs[sec])].chords;
    const auto& pc = prog[(size_t) ((barIn / juce::jmax (1, chordBars)) % (int) prog.size())];
    return { (key + pc.off) % 12, pc.quality() };
}

void ParlourProcessor::playStep (int64_t k, int64_t at)
{
    const int64_t b = k / 16;
    const int s16 = (int) (((k % 16) + 16) % 16);
    if (b != curBar)
    {
        curBar = b;
        bool lastBar = false;
        barSection = sectionAt (b, barInSec, lastBar);
        writeBar (feel, barSection, barInSec, busy, rng, bar);
        const bool fill = (led && leader.fillAt (b)) || (! led && follow == FBand && autoSong && lastBar);
        if (fill && filledBar != b) { writeFill (feel, rng, bar); filledBar = b; }
        uiSection = barSection;
    }
    uiStep = s16;
    // the band dropped out: let go, and stay quiet
    const bool out = led && leader.isOut (b);
    const bool midiSilent = follow == FMidiChords && ! midiChord.valid();
    if (out || midiSilent)
    {
        if (! bandOut) { schedule ({ at, 3, 0, 0, 0 }); bandOut = true; }
        return;
    }
    bandOut = false;

    const KChord c = chordAt (b);
    if (! c.valid()) return;
    uiChord = c.root | (c.q << 4);
    const bool changed = ! (c == lastChord);
    const bool holds = feelHolds (feel);
    const int centers[3] = { 55, 62, 69 };
    int center = centers[juce::jlimit (0, 2, reg)];
    if (instrument == IGlock) center = juce::jmax (center + 17, 79);
    else if (instrument == IVibes) center = juce::jmax (center + 4, 64);
    if (feel == KBells) center += 12;

    const double sixteenth = sr * 15.0 / juce::jmax (30.0, bpmNow);
    const bool strike = [&] { for (auto& h : bar) if (h.step == s16) return true; return false; }();

    // a held chord that changes under you: move to the new one now
    if (holds && changed && ! strike && lastChord.valid())
    {
        schedule ({ at, 3, 0, 0, 0 });
        voicing = voiceChord (c, colour, center, voicing);
        for (int i = 0; i < voicing.n; ++i) schedule ({ at + (int64_t) i * 2, 1, voicing.notes[i], 0.62f, -1.0f });
        lastChord = c;
        return;
    }
    if (! strike) return;
    if (changed || voicing.n == 0) { voicing = voiceChord (c, colour, center, voicing); lastChord = c; }

    const float energy = led ? (leader.energy - 0.5f) * 0.25f : 0.0f;
    const float secLift = barSection == 1 ? 0.06f : (barSection == 2 ? -0.05f : 0.0f);
    // timing: swing on the off 16ths, a little looseness, the lazy lo-fi drag
    double offMs = rng.n() * (1.5 + 5.0 * human);
    if (s16 % 2 == 1) offMs += swing * sixteenth / sr * 1000.0 * 0.66;
    if (feel == KLofi) offMs += 14.0 + 8.0 * human;
    const bool percussive = ! (instrument == IOrgan || instrument == IStrings || instrument == IPad || instrument == IBrass);
    const double strumMs = feel == KLofi ? 9.0 + 6.0 * human : (percussive ? 0.8 + 3.0 * human : 0.3);

    for (const auto& h : bar)
    {
        if (h.step != s16) continue;
        if (holds && h.step == 0 && ! changed && h.kind == HChord && barInSec % 4 != 0) continue;   // same chord: keep holding it
        const float vel = juce::jlimit (0.08f, 1.0f, h.vel * (0.55f + 0.6f * dynamics) + rng.n() * 0.06f * human + energy + secLift);
        float lenSec = (float) (h.len * lengthK * sixteenth / sr) * (1.0f + rng.n() * 0.06f * human);
        if (holds && h.len >= 15.0f) lenSec = -1.0f;    // pads hold until the chord changes
        const int64_t t0 = at + (int64_t) (offMs * sr / 1000.0);
        auto on = [&] (int note, float v, double extraMs) { schedule ({ juce::jmax (sampleClock, t0 + (int64_t) (extraMs * sr / 1000.0)), 1, note, v, lenSec }); };
        int lh = c.root + 36; while (lh < (reg == 0 ? 31 : 36)) lh += 12; while (lh > (reg == 0 ? 42 : 47)) lh -= 12;
        if (holds && lenSec < 0) schedule ({ t0 - 1 < sampleClock ? sampleClock : t0 - 1, 3, 0, 0, 0 });
        switch (h.kind)
        {
            case HChord:
                if (leftHand) on (lh, vel * 0.95f, 0);
                for (int i = 0; i < voicing.n; ++i) on (voicing.notes[i], vel * (i == voicing.n - 1 ? 1.0f : 0.9f), strumMs * i);
                break;
            case HChordNoLeft:
                for (int i = 0; i < voicing.n; ++i) on (voicing.notes[i], vel * (i == voicing.n - 1 ? 1.0f : 0.9f), strumMs * i);
                break;
            case HLeft:
                if (leftHand || feel == KBallad || feel == KRock8) on (lh + (h.index % 2 == 1 ? (feel == KRock8 ? 12 : 7) : 0), vel, 0);
                break;
            case HNote:
            {
                if (voicing.n == 0) break;
                const int idx = h.index;
                const int note = voicing.notes[idx % voicing.n] + 12 * (idx / voicing.n);
                on (note, vel, 0);
                break;
            }
            default:  // HTopOct
                if (voicing.n > 0) on (voicing.notes[juce::jmax (0, voicing.n - 1 - h.index)] + 12, vel, 0);
                break;
        }
    }
}

void ParlourProcessor::runPlayer (int n)
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
    if (! playing)
    {
        if (wasPlaying) { schedule ({ sampleClock, 3, 0, 0, 0 }); uiStep = -1; }
        wasPlaying = false; return;
    }
    const double spb = sr * 60.0 / bpm;
    if (! wasPlaying || ppq < lastPpqEnd - 0.02 || ppq > lastPpqEnd + 0.25)
    {
        lastStep = (int64_t) std::ceil (ppq * 4.0 - 0.001) - 1;
        curBar = -1; voicing = {}; lastChord = {};
        if (wasPlaying) schedule ({ sampleClock, 3, 0, 0, 0 });
    }
    wasPlaying = true;
    const double ppqEnd = ppq + n / spb;
    for (int64_t k = lastStep + 1; (double) k / 4.0 < ppqEnd; ++k)
    {
        const double t = ((double) k / 4.0 - ppq) * spb;
        playStep (k, sampleClock + juce::jmax ((int64_t) 0, (int64_t) t));
        lastStep = k;
    }
    lastPpqEnd = ppqEnd;
}

void ParlourProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if ((int) tmpL.size() < n) return;
    readParams();
    band::markPresent (band::WParlour);
    const bool wasLed = led;
    led = follow != FPlay && band::readLeader (leader);
    uiLinked = led;
    if (led != wasLed) lastPpqEnd = -1e9;

    // MIDI in
    bool chordKeys = false;
    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        const int64_t at = sampleClock + m.samplePosition;
        if (msg.isNoteOn())
        {
            held[(size_t) msg.getNoteNumber()] = true; chordKeys = true;
            if (follow == FPlay) schedule ({ at, 1, msg.getNoteNumber(), msg.getFloatVelocity(), -1.0f });
        }
        else if (msg.isNoteOff())
        {
            held[(size_t) msg.getNoteNumber()] = false; chordKeys = true;
            if (follow == FPlay)
            {
                if (sustainPedal) sustained[(size_t) msg.getNoteNumber()] = true;
                else schedule ({ at, 2, msg.getNoteNumber(), 0, 0 });
            }
        }
        else if (msg.isSustainPedalOn()) sustainPedal = true;
        else if (msg.isSustainPedalOff())
        {
            sustainPedal = false;
            for (int i = 0; i < 128; ++i) if (sustained[(size_t) i]) { sustained[(size_t) i] = false; if (! held[(size_t) i]) schedule ({ at, 2, i, 0, 0 }); }
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff()) { held.fill (false); schedule ({ at, 3, 0, 0, 0 }); }
    }
    if (chordKeys) { const auto c = chordFromNotes (held); if (c.valid() || follow == FMidiChords) midiChord = c; }

    const bool sitOut = apvts.getRawParameterValue ("sit_out")->load() > 0.5f;
    if (follow != FPlay && ! sitOut) runPlayer (n);
    else { uiTransport = false; if (wasPlaying) { schedule ({ sampleClock, 3, 0, 0, 0 }); wasPlaying = false; } }
    if (follow == FPlay) { const auto c = chordFromNotes (held); uiChord = c.valid() ? c.root | (c.q << 4) : -1; }

    // render, splitting the block at each event
    std::stable_sort (queue.begin(), queue.end(), [] (const Ev& a, const Ev& b) { return a.at < b.at; });
    float* L = tmpL.data(); float* R = tmpR.data();
    std::fill (L, L + n, 0.0f); std::fill (R, R + n, 0.0f);
    size_t e = 0;
    const int64_t end = sampleClock + n;
    for (int i = 0; i < n; ++i)
    {
        const int64_t now = sampleClock + i;
        while (e < queue.size() && queue[e].at <= now)
        {
            const auto& ev = queue[e++];
            if (ev.type == 1) noteOn (ev.note, ev.vel, ev.len);
            else if (ev.type == 2) noteOff (ev.note);
            else allOff();
        }
        float l = 0, r = 0;
        for (auto& v : voices) v.render (l, r);
        L[i] = l; R[i] = r;
    }
    // keep what's still in the future (strums and swing can land in the next block)
    queue.erase (queue.begin(), queue.begin() + (long) e);
    for (auto& q : queue) if (q.at < end) q.at = end;

    rack.process (L, R, n, rng);
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
    uint32_t bits[4] {};
    for (auto& v : voices) if (v.active() && v.held) bits[v.note >> 5] |= 1u << (v.note & 31);
    for (int i = 0; i < 4; ++i) uiNotes[i] = bits[i];
    sampleClock += n;
}

// ---------------------------------------------------------------------------
void ParlourProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void ParlourProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

void ParlourProcessor::applyStyle (const band::StyleDef& s)
{
    band::setParam (apvts, "sit_out", s.kOn ? 0.0f : 1.0f);
    if (! s.kOn) return;
    band::setParam (apvts, "instrument", (float) s.kInst);
    band::setParam (apvts, "feel", (float) s.kFeel);
    band::setParam (apvts, "busy", s.kBusy);
    band::setParam (apvts, "colour", s.kColour);
    band::setParam (apvts, "register", (float) s.kReg);
    band::setParam (apvts, "motion", s.kMotion);
    band::setParam (apvts, "chorus", s.kChorus);
    band::setParam (apvts, "lofi", s.kLofi);
    band::setParam (apvts, "room", s.kRoom);
    band::setParam (apvts, "swing", s.bSwing);
}

juce::AudioProcessorEditor* ParlourProcessor::createEditor() { return new ParlourEditor (*this); }

#ifndef PARLOUR_NO_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ParlourProcessor(); }
#endif
