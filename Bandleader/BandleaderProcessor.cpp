#include "BandleaderProcessor.h"
#include "BandleaderEditor.h"

using namespace bl;

namespace
{
const juce::StringArray& onOff() { static const juce::StringArray s { "Off", "On" }; return s; }
const char* kTrigIds[7] = { "t_next", "t_fill", "t_drop", "t_allin", "t_verse", "t_chorus", "t_bridge" };
}

juce::AudioProcessorValueTreeState::ParameterLayout BandleaderProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> ps;
    auto C = [&] (const char* id, const char* name, juce::StringArray items, int def)
    { ps.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def)); };
    C ("key", "Key", { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 9);
    C ("prog_verse", "Verse Chords", ut::progressionNames(), 0);
    C ("prog_chorus", "Chorus Chords", ut::progressionNames(), 2);
    C ("prog_bridge", "Bridge Chords", ut::progressionNames(), 5);
    C ("chord_bars", "Bars Per Chord", { "1", "2", "4" }, 0);
    C ("song", "Song", { "Auto song", "Manual" }, 0);
    C ("section_bars", "Section Length", { "4 bars", "8 bars", "16 bars" }, 1);
    C ("section", "Section", { "Verse", "Chorus", "Bridge" }, 0);
    ps.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "energy", 1 }, "Energy", juce::NormalisableRange<float> (0, 1, 0.001f), 0.5f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100)) + "%"; })));
    const char* trigNames[7] = { "Footswitch: Next Section", "Footswitch: Fill", "Footswitch: Drop Out", "Footswitch: All In",
                                 "Footswitch: Verse", "Footswitch: Chorus", "Footswitch: Bridge" };
    for (int i = 0; i < 7; ++i) C (kTrigIds[i], trigNames[i], onOff(), 0);
    return { ps.begin(), ps.end() };
}

BandleaderProcessor::BandleaderProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Bandleader", createLayout())
{
}

int BandleaderProcessor::sectionBarsNow() const { return 4 << (int) apvts.getRawParameterValue ("section_bars")->load(); }

// a command lands on the next bar line (or the one after, if this bar is nearly over)
void BandleaderProcessor::handle (Cmd c, int64_t bar, double frac)
{
    const int64_t at = bar + (frac > 0.75 ? 2 : 1);
    auto goTo = [&] (int sec, bool advanceForm)
    {
        if (advanceForm) pendingForm = (formIndex + 1) % 6;
        else pendingForm = -1;
        L.nextSection = sec; L.nextStartBar = at; L.fillBar = at - 1;
        if (L.dropStart < at && L.dropEnd > at) L.dropEnd = at;   // a section change brings everyone back
    };
    auto setManual = [&] (int sec)
    {
        if (auto* p = apvts.getParameter ("song")) p->setValueNotifyingHost (1.0f);
        if (auto* p = apvts.getParameter ("section")) p->setValueNotifyingHost (p->convertTo0to1 ((float) sec));
        goTo (sec, false);
    };
    switch (c)
    {
        case CNext:
            if (apvts.getRawParameterValue ("song")->load() < 0.5f)
            {
                if (pendingForm >= 0) formIndex = pendingForm;   // already heading somewhere: go one further
                goTo (kForm[(formIndex + 1) % 6], true);
            }
            else
            {
                const int s = (L.curSection + 1) % band::NumSec;
                if (auto* p = apvts.getParameter ("section")) p->setValueNotifyingHost (p->convertTo0to1 ((float) s));
                goTo (s, false);
            }
            break;
        case CFill:   L.fillBar = frac < 0.5 ? bar : bar + 1; break;
        case CDrop:   L.dropStart = at; L.dropEnd = band::kNever; break;
        case CAllIn:  if (L.dropStart < band::kNever) { L.dropEnd = at; L.fillBar = at - 1; } break;
        case CVerse:  setManual (band::Verse); break;
        case CChorus: setManual (band::Chorus); break;
        case CBridge: setManual (band::Bridge); break;
        default: break;
    }
}

void BandleaderProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();
    auto g = [&] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    // the song's harmony
    L.key = (int) g ("key");
    L.chordBars = 1 << (int) g ("chord_bars");
    const char* progIds[3] = { "prog_verse", "prog_chorus", "prog_bridge" };
    for (int s = 0; s < band::NumSec; ++s)
    {
        const auto& p = ut::progressions()[(size_t) juce::jlimit (0, (int) ut::progressions().size() - 1, (int) g (progIds[s]))].chords;
        L.progLen[s] = juce::jmin ((int) p.size(), band::kMaxProg);
        for (int i = 0; i < L.progLen[s]; ++i) { L.progOff[s][i] = (int8_t) p[(size_t) i].off; L.progMinor[s][i] = p[(size_t) i].minor ? 1 : 0; }
    }
    L.energy = g ("energy");
    const bool autoSong = g ("song") < 0.5f;
    const int secBars = sectionBarsNow();

    double ppq = 0, bpm = 120; bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto p = pos->getPpqPosition()) ppq = *p; else playing = false;
        }
    const int64_t bar = (int64_t) std::floor (ppq / 4.0);
    const double frac = ppq / 4.0 - (double) bar;

    // where are we? (start of play, or the playhead jumped)
    // (while stopped, keep the song parked where play will start, so followers start in the right place)
    if (! playing || ! wasPlaying || ppq < lastPpqEnd - 0.02 || ppq > lastPpqEnd + 0.5)
    {
        if (autoSong)
        {
            const auto sp = ut::songPosition (bar, secBars);
            L.curSection = sp.section; L.curStartBar = bar - sp.barIn;
            const int64_t len = (int64_t) secBars * 6;
            formIndex = (int) ((((bar % len) + len) % len) / secBars);
        }
        else { L.curSection = (int) g ("section"); L.curStartBar = bar; }
        L.nextStartBar = band::kNever; L.fillBar = -band::kNever; pendingForm = -1;
        L.dropStart = L.dropEnd = band::kNever;
    }
    wasPlaying = playing;
    L.playing = playing ? 1 : 0;
    lastPpqEnd = ppq + buffer.getNumSamples() * bpm / 60.0 / juce::jmax (8000.0, getSampleRate());

    // commands: footswitch notes / CCs, mapped parameters, the screen
    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        int idx = -1;
        if (msg.isNoteOn())
        {
            const int notes[7] = { kNext, kFill, kDrop, kAllIn, kVerse, kChorus, kBridge };
            for (int i = 0; i < 7; ++i) if (msg.getNoteNumber() == notes[i]) idx = i;
        }
        else if (msg.isController() && msg.getControllerNumber() >= kCCBase && msg.getControllerNumber() < kCCBase + 7)
        {
            const int i = msg.getControllerNumber() - kCCBase;
            const int v = msg.getControllerValue();
            if (v >= 64 && lastCC[(size_t) i] < 64) idx = i;
            lastCC[(size_t) i] = v;
        }
        if (idx >= 0) handle ((Cmd) (idx + 1), bar, frac);
    }
    midi.clear();
    for (int i = 0; i < 7; ++i)
    {
        const float v = g (kTrigIds[i]);
        if (v > 0.5f && lastTrig[(size_t) i] <= 0.5f) handle ((Cmd) (i + 1), bar, frac);
        lastTrig[(size_t) i] = v;
    }
    if (const int c = uiCmd.exchange (0)) handle ((Cmd) c, bar, frac);

    // manual: follow the Section parameter (automation, MIDI Map)
    if (! autoSong && playing)
    {
        const int want = (int) g ("section");
        const int pending = L.nextStartBar < band::kNever ? L.nextSection : L.curSection;
        if (want != pending) handle ((Cmd) (CVerse + want), bar, frac);
    }

    if (playing)
    {
        // a scheduled change arrives
        if (bar >= L.nextStartBar)
        {
            L.curSection = L.nextSection; L.curStartBar = L.nextStartBar; L.nextStartBar = band::kNever;
            if (pendingForm >= 0) { formIndex = pendingForm; pendingForm = -1; }
        }
        if (L.dropEnd <= bar) L.dropStart = L.dropEnd = band::kNever;
        // auto song: line up the next part one bar ahead, with a fill into it
        if (autoSong && L.nextStartBar >= band::kNever && bar >= L.curStartBar + secBars - 1)
        {
            pendingForm = (formIndex + 1) % 6;
            L.nextSection = kForm[pendingForm];
            L.nextStartBar = L.curStartBar + secBars;
            if (L.nextStartBar <= bar) L.nextStartBar = bar + 1;
            L.fillBar = L.nextStartBar - 1;
        }
    }
    band::writeLeader (L);

    uiBusOk = band::available();
    uiPlaying = playing;
    uiSection = L.curSection;
    uiNextSection = L.nextStartBar < band::kNever ? L.nextSection : -1;
    uiBarsToNext = L.nextStartBar < band::kNever ? (int) (L.nextStartBar - bar) : -1;
    uiBarIn = (int) juce::jmax<int64_t> (0, bar - L.curStartBar) + 1;
    uiFormIndex = formIndex;
    uiOut = playing && L.isOut (bar);
    uiDropComing = playing && L.dropStart > bar && L.dropStart < band::kNever;
    uiFillComing = playing && (L.fillBar == bar || L.fillBar == bar + 1);
    int r; bool mn; L.chordAt (playing ? bar : 0, r, mn);
    uiChord = r | (mn ? 16 : 0);
}

void BandleaderProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void BandleaderProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* BandleaderProcessor::createEditor() { return new BandleaderEditor (*this); }

#ifndef BANDLEADER_NO_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BandleaderProcessor(); }
#endif
