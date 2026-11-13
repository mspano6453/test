#include "CarouselProcessor.h"
#include "../Source/SongForm.h"
#include "CarouselEditor.h"
#include "../Source/BandBus.h"

using namespace cr;

namespace
{
juce::String pct (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
juce::String dB1 (float v, int) { return v <= -35.9f ? juce::String ("-inf") : juce::String (v, 1) + " dB"; }
const juce::StringArray& onOff() { static const juce::StringArray s { "Off", "On" }; return s; }
const char* kTrigIds[6] = { "t_rec", "t_stop", "t_undo", "t_redo", "t_next", "t_clear" };
const int kTrigActions[6] = { ARecPlayDub, AStop, AUndo, ARedo, ANext, AClear };
}

juce::AudioProcessorValueTreeState::ParameterLayout CarouselProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> ps;
    auto F = [&] (const char* id, const char* name, juce::NormalisableRange<float> r, float def, juce::String (*fmt) (float, int))
    {
        ps.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, r, def,
                                                                   juce::AudioParameterFloatAttributes().withStringFromValueFunction (fmt)));
    };
    auto C = [&] (const char* id, const char* name, juce::StringArray items, int def)
    { ps.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def)); };

    F ("loop_lvl", "Loop Level", { -36, 6, 0.1f }, 0, dB1);
    F ("dry", "Input (dry)", { 0, 1, 0.001f }, 1, pct);
    F ("decay", "Fade (older layers)", { 0, 0.5f, 0.001f }, 0, pct);
    C ("reverse", "Reverse", onOff(), 0);
    C ("half", "Half Speed", onOff(), 0);
    C ("stop_mode", "Stop Mode", { "Instant", "Fade out", "End of loop" }, 0);
    C ("play_mode", "Play Mode", { "Loop", "Once" }, 0);
    C ("sync", "Snap to Live Tempo", onOff(), 1);
    C ("snap", "Snap To", { "Beat", "Bar" }, 1);
    C ("band_rec", "Record Whole Sections (Bandleader)", onOff(), 1);
    C ("band_slots", "Slots Follow Sections (Bandleader)", onOff(), 0);
    // footswitch targets for Live's MIDI Map mode (press = jump above 50%)
    const char* trigNames[6] = { "Footswitch: Rec/Play/Dub", "Footswitch: Stop", "Footswitch: Undo", "Footswitch: Redo", "Footswitch: Next Loop", "Footswitch: Clear" };
    for (int i = 0; i < 6; ++i) C (kTrigIds[i], trigNames[i], onOff(), 0);
    return { ps.begin(), ps.end() };
}

CarouselProcessor::CarouselProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Carousel", createLayout())
{
    band::available();   // open the band link here (message thread), not on the audio thread
    slots.resize (kSlots);
}

bool CarouselProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet(), in = l.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return in == out;
}

void CarouselProcessor::ensurePool()
{
    const int newChunk = (int) std::lround (sr);
    if (newChunk == chunkLen && ! pool.empty()) return;
    const juce::SpinLock::ScopedLockType sl (poolLock);
    chunkLen = newChunk;
    pool.assign ((size_t) chunkLen * 2 * kPoolSeconds, 0.0f);
    freeList.resize (kPoolSeconds);
    freeTop = 0;
    for (int c = kPoolSeconds - 1; c >= 0; --c) freeList[(size_t) freeTop++] = c;
    for (auto& s : slots) { s.state = Empty; s.length = 0; s.nLayers = s.redoTop = 0; for (auto& l : s.layers) l.nChunks = 0; }
}

void CarouselProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    ensurePool();
    for (int i = 0; i < 6; ++i) lastTrig[i] = apvts.getRawParameterValue (kTrigIds[i])->load();   // don't fire on load
}

// ---------------------------------------------------------------------------
// memory
// ---------------------------------------------------------------------------
int CarouselProcessor::allocChunk() { return freeTop > 0 ? freeList[(size_t) --freeTop] : -1; }

void CarouselProcessor::clearSlot (int s)
{
    auto& sl = slots[(size_t) s];
    for (int l = 0; l < kMaxLayers; ++l) freeLayer (sl.layers[l]);
    sl.state = Empty; sl.length = 0; sl.syncTarget = 0; sl.pos = 0; sl.nLayers = sl.redoTop = 0; sl.fade = 1;
}

float CarouselProcessor::readLayer (Layer& l, int64_t len, double pos, int ch)
{
    const int64_t i0 = (int64_t) std::floor (pos);
    const float fr = (float) (pos - (double) i0);
    auto at = [&] (int64_t i) -> float
    {
        i %= len; if (i < 0) i += len;
        const int k = (int) (i / chunkLen);
        if (k >= l.nChunks || ! l.init[k]) return 0.0f;
        return chunkPtr (l.chunk[k])[(size_t) (i % chunkLen) * 2 + (size_t) ch];
    };
    const float a = at (i0);
    return fr > 0.0001f ? a + fr * (at (i0 + 1) - a) : a;
}

void CarouselProcessor::writeLayer (Layer& l, int64_t len, int64_t idx, float a, float b)
{
    idx %= len; if (idx < 0) idx += len;
    const int k = (int) (idx / chunkLen);
    if (k >= l.nChunks) return;
    float* p = chunkPtr (l.chunk[k]);
    if (! l.init[k]) { std::fill (p, p + (size_t) chunkLen * 2, 0.0f); l.init[k] = 1; }
    const size_t o = (size_t) (idx % chunkLen) * 2;
    p[o] += a; p[o + 1] += b;
}

// ---------------------------------------------------------------------------
// transport
// ---------------------------------------------------------------------------
void CarouselProcessor::startRecording()
{
    clearSlot (cur);
    auto& s = slots[(size_t) cur];
    s.layers[0].gain = 1.0f;
    s.nLayers = s.redoTop = 1;
    s.state = Recording;
}

void CarouselProcessor::closeLoop (bool force)
{
    auto& s = slots[(size_t) cur];
    if (s.length < (int64_t) (0.1 * sr)) { clearSlot (cur); return; }
    const bool sync = apvts.getRawParameterValue ("sync")->load() > 0.5f;
    if (sync && ! force && s.syncTarget == 0)
    {
        const double unitBeats = apvts.getRawParameterValue ("snap")->load() > 0.5f ? band::meterBarPpq (meterNow) : 1.0;
        const double unitLen = unitBeats * 60.0 / juce::jmax (30.0, bpm) * sr;
        const int64_t units = juce::jmax<int64_t> (1, (int64_t) std::llround ((double) s.length / unitLen));
        const int64_t target = (int64_t) std::llround ((double) units * unitLen);
        if (target > s.length) { s.syncTarget = target; return; }      // keep recording up to the bar line
        // trim to the bar line and give back unused chunks
        s.length = target;
        auto& base = s.layers[0];
        const int need = (int) ((target + chunkLen - 1) / chunkLen);
        while (base.nChunks > need) freeChunk (base.chunk[--base.nChunks]);
    }
    s.syncTarget = 0;
    s.state = Playing;
    s.pos = 0;
    s.fade = 1;
}

bool CarouselProcessor::startOverdub()
{
    auto& s = slots[(size_t) cur];
    if (s.nLayers >= kMaxLayers) return false;
    // a new overdub throws away anything that was undone
    for (int l = s.nLayers; l < s.redoTop; ++l) freeLayer (s.layers[l]);
    s.redoTop = s.nLayers;
    auto& nl = s.layers[s.nLayers];
    const int need = (int) ((s.length + chunkLen - 1) / chunkLen);
    if (freeTop < need) return false;
    nl.nChunks = 0;
    for (int i = 0; i < need; ++i) { nl.chunk[nl.nChunks] = allocChunk(); nl.init[nl.nChunks] = 0; ++nl.nChunks; }
    nl.gain = 1.0f;
    ++s.nLayers;
    s.redoTop = s.nLayers;
    s.state = Overdubbing;
    return true;
}

void CarouselProcessor::stopSlot (bool instant)
{
    auto& s = slots[(size_t) cur];
    if (s.state == Recording) { closeLoop(); if (s.state == Playing) s.state = Stopped; return; }
    if (s.state != Playing && s.state != Overdubbing && s.state != StoppingFade && s.state != StoppingEnd) return;
    const int mode = instant ? 0 : (int) apvts.getRawParameterValue ("stop_mode")->load();
    if (mode == 0) { s.state = Stopped; s.pos = 0; }
    else if (mode == 1) s.state = StoppingFade;
    else s.state = StoppingEnd;
}

void CarouselProcessor::selectSlot (int n)
{
    n = (n % kSlots + kSlots) % kSlots;
    if (n == cur) return;
    auto& old = slots[(size_t) cur];
    const bool wasRunning = old.state == Playing || old.state == Overdubbing || old.state == StoppingEnd || old.state == StoppingFade;
    if (old.state == Recording) closeLoop();
    if (old.state != Empty) { old.state = Stopped; old.pos = 0; }
    cur = n;
    auto& s = slots[(size_t) cur];
    if (s.state != Empty && wasRunning) { s.state = Playing; s.pos = 0; s.fade = 1; }
}

void CarouselProcessor::handleAction (int a)
{
    auto& s = slots[(size_t) cur];
    auto toggle = [this] (const char* id)
    {
        if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
    };
    if (a >= ASelect0 && a < ASelect0 + kSlots) { selectSlot (a - ASelect0); return; }
    switch (a)
    {
        case ARecPlayDub:
            switch (s.state)
            {
                case Empty:
                    if (bandActive && apvts.getRawParameterValue ("band_rec")->load() > 0.5f) armed = ! armed;   // starts at the next section
                    else startRecording();
                    break;
                case Recording:   if (s.syncTarget == 0) closeLoop(); break;
                case Playing:     startOverdub(); break;
                case Overdubbing: s.state = Playing; break;
                default:          s.state = Playing; s.pos = 0; s.fade = 1; break;    // stopped -> play from the top
            }
            break;
        case AStop:  stopSlot (false); break;
        case AUndo:
            if (s.state == Recording) { clearSlot (cur); break; }
            if (s.state == Overdubbing) s.state = Playing;
            if (s.nLayers > 1) --s.nLayers;
            break;
        case ARedo:  if (s.nLayers < s.redoTop) ++s.nLayers; break;
        case ANext:  selectSlot (cur + 1); break;
        case APrev:  selectSlot (cur - 1); break;
        case AClear: clearSlot (cur); armed = false; break;
        case AClearAll: for (int i = 0; i < kSlots; ++i) clearSlot (i); break;
        case AToggleReverse: toggle ("reverse"); break;
        case AToggleHalf:    toggle ("half"); break;
        default: break;
    }
}

void CarouselProcessor::push (int action)
{
    int s1, z1, s2, z2;
    fifo.prepareToWrite (1, s1, z1, s2, z2);
    if (z1 > 0) queue[(size_t) s1] = action; else if (z2 > 0) queue[(size_t) s2] = action;
    fifo.finishedWrite (z1 + z2);
}

void CarouselProcessor::handleMidi (const juce::MidiMessage& m)
{
    int action = 0;
    if (m.isNoteOn())
    {
        switch (m.getNoteNumber())
        {
            case kNoteRec:   action = ARecPlayDub; break;
            case kNoteStop:  action = AStop; break;
            case kNoteUndo:  action = AUndo; break;
            case kNoteRedo:  action = ARedo; break;
            case kNoteRev:   action = AToggleReverse; break;
            case kNoteHalf:  action = AToggleHalf; break;
            case kNoteNext:  action = ANext; break;
            case kNotePrev:  action = APrev; break;
            case kNoteClear: action = AClear; break;
            default: break;
        }
    }
    else if (m.isController())
    {
        const int idx = m.getControllerNumber() - kFirstCC;
        if (idx >= 0 && idx < 9)
        {
            const bool down = m.getControllerValue() >= 64;
            if (down && ! ccDown[idx])
            {
                static const int map[9] = { ARecPlayDub, AStop, AUndo, ARedo, AToggleReverse, AToggleHalf, ANext, APrev, AClear };
                action = map[idx];
            }
            ccDown[idx] = down;
        }
    }
    if (action != 0) handleAction (action);
}

void CarouselProcessor::checkTriggerParams()
{
    for (int i = 0; i < 6; ++i)
    {
        const float v = apvts.getRawParameterValue (kTrigIds[i])->load();
        if (v > 0.5f && lastTrig[i] <= 0.5f) handleAction (kTrigActions[i]);
        lastTrig[i] = v;
    }
}

// ---------------------------------------------------------------------------
void CarouselProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nch = buffer.getNumChannels();
    if (nch == 0 || n == 0) return;
    float* L = buffer.getWritePointer (0);
    float* R = nch > 1 ? buffer.getWritePointer (1) : buffer.getWritePointer (0);

    double ppq = 0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            transportPlaying = pos->getIsPlaying();
            if (auto p = pos->getPpqPosition()) ppq = *p;
        }
    // the band: with a Bandleader running, loops can follow the song's sections
    band::markPresent (band::WCarousel);
    band::Leader leader;
    bandActive = transportPlaying && band::readLeader (leader) && leader.playing;
    meterNow = bandActive ? leader.meter : band::hostMeter (getPlayHead());
    const double bpq = band::meterBarPpq (meterNow);
    if (! bandActive) armed = false;
    uiLinked = bandActive;
    int boundaryAt = -1; int boundarySection = 0;
    if (bandActive)
    {
        const double ppqPerSample = bpm / 60.0 / sr;
        const double barPpq = std::ceil (ppq / bpq - 1e-9) * bpq;
        const int off = (int) std::llround ((barPpq - ppq) / ppqPerSample);
        if (off >= 0 && off < n)
        {
            int64_t in = 0;
            boundarySection = leader.sectionAt ((int64_t) std::llround (barPpq / bpq), &in);
            if (in == 0) boundaryAt = off;
        }
    }

    const juce::SpinLock::ScopedLockType lock (poolLock);
    if (pool.empty()) return;

    // controls: UI buttons, footswitch params, MIDI
    {
        int s1, z1, s2, z2;
        const int ready = fifo.getNumReady();
        fifo.prepareToRead (ready, s1, z1, s2, z2);
        for (int i = 0; i < z1; ++i) handleAction (queue[(size_t) (s1 + i)]);
        for (int i = 0; i < z2; ++i) handleAction (queue[(size_t) (s2 + i)]);
        fifo.finishedRead (z1 + z2);
    }
    checkTriggerParams();
    for (const auto meta : midi) handleMidi (meta.getMessage());
    midi.clear();

    const float loopGain = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("loop_lvl")->load(), -35.9f);
    const float dry = apvts.getRawParameterValue ("dry")->load();
    const float decay = apvts.getRawParameterValue ("decay")->load();
    const bool rev = apvts.getRawParameterValue ("reverse")->load() > 0.5f;
    const bool half = apvts.getRawParameterValue ("half")->load() > 0.5f;
    const bool once = apvts.getRawParameterValue ("play_mode")->load() > 0.5f;
    const double rate = (rev ? -1.0 : 1.0) * (half ? 0.5 : 1.0);
    const float fadeStep = (float) (1.0 / (2.0 * sr));

    float inPk = 0;
    auto run = [&] (int from, int to)
    {
    auto& s = slots[(size_t) cur];
    for (int i = from; i < to; ++i)
    {
        const float inL = L[i], inR = nch > 1 ? R[i] : L[i];
        inPk = std::max (inPk, std::max (std::abs (inL), std::abs (inR)));
        float oL = inL * dry, oR = inR * dry;

        if (s.state == Recording)
        {
            auto& base = s.layers[0];
            const int k = (int) (s.length / chunkLen);
            if (k >= base.nChunks)
            {
                const int c = allocChunk();
                if (c < 0) { s.syncTarget = 0; closeLoop (true); }    // out of memory: close what we have
                else { base.chunk[base.nChunks] = c; base.init[base.nChunks] = 1; ++base.nChunks; }
            }
            if (s.state == Recording)
            {
                float* p = chunkPtr (base.chunk[k]);
                const size_t o = (size_t) (s.length % chunkLen) * 2;
                p[o] = inL; p[o + 1] = inR;
                ++s.length;
                if (s.syncTarget > 0 && s.length >= s.syncTarget) { s.syncTarget = 0; s.state = Playing; s.pos = 0; s.fade = 1; }
            }
        }
        else if (s.state == Playing || s.state == Overdubbing || s.state == StoppingFade || s.state == StoppingEnd)
        {
            float lL = 0, lR = 0;
            for (int l = 0; l < s.nLayers; ++l)
            {
                auto& ly = s.layers[l];
                lL += readLayer (ly, s.length, s.pos, 0) * ly.gain;
                lR += readLayer (ly, s.length, s.pos, 1) * ly.gain;
            }
            if (s.state == Overdubbing)
                writeLayer (s.layers[s.nLayers - 1], s.length, (int64_t) std::floor (s.pos), inL * (float) std::abs (rate), inR * (float) std::abs (rate));

            if (s.state == StoppingFade) { s.fade -= fadeStep; if (s.fade <= 0) { s.fade = 1; s.state = Stopped; s.pos = 0; } }
            const float g = loopGain * (s.state == StoppingFade ? s.fade : 1.0f);
            oL += lL * g; oR += lR * g;

            s.pos += rate;
            bool wrapped = false;
            if (s.pos >= (double) s.length) { s.pos -= (double) s.length; wrapped = true; }
            if (s.pos < 0) { s.pos += (double) s.length; wrapped = true; }
            if (wrapped)
            {
                if (s.state == Overdubbing && decay > 0)
                    for (int l = 0; l < s.nLayers - 1; ++l) s.layers[l].gain *= (1.0f - decay);
                if (s.state == StoppingEnd || (once && s.state == Playing)) { s.state = Stopped; s.pos = 0; }
            }
        }

        L[i] = oL;
        if (nch > 1) R[i] = oR;
    }
    };

    if (boundaryAt < 0) run (0, n);
    else
    {
        run (0, boundaryAt);
        // a new section starts right here
        if (apvts.getRawParameterValue ("band_slots")->load() > 0.5f) selectSlot (boundarySection);
        auto& sl = slots[(size_t) cur];
        if (sl.state == Recording && sectionRec) { sl.syncTarget = 0; closeLoop (true); sectionRec = false; }   // exactly one section long
        else if (armed && sl.state == Empty) { startRecording(); sectionRec = true; armed = false; }
        run (boundaryAt, n);
    }
    auto& s = slots[(size_t) cur];

    // UI
    uiState = s.state; uiSlot = cur; uiLayers = s.nLayers; uiRedo = s.redoTop - s.nLayers; uiFreeChunks = freeTop;
    uiPos = s.length > 0 ? (float) (s.pos / (double) s.length) : 0.0f;
    uiLenSec = (float) ((double) s.length / sr);
    uiRecSec = s.state == Recording ? (float) ((double) s.length / sr) : 0.0f;
    uiWaitingForBar = s.state == Recording && s.syncTarget > 0;
    uiArmed = armed; uiSectionRec = s.state == Recording && sectionRec;
    if (s.state != Recording) sectionRec = false;
    uiInLevel = inPk;
    uint32_t has = 0;
    for (int k = 0; k < kSlots; ++k) if (slots[(size_t) k].state != Empty && slots[(size_t) k].length > 0) has |= (1u << k);
    uiSlotHas = has;
}

// ---------------------------------------------------------------------------
juce::File CarouselProcessor::exportCurrentLoop()
{
    juce::AudioBuffer<float> out;
    int slotNum = 0;
    {
        const juce::SpinLock::ScopedLockType lock (poolLock);
        auto& s = slots[(size_t) cur];
        slotNum = cur + 1;
        if (s.length <= 0 || s.state == Recording) return {};
        out.setSize (2, (int) s.length);
        out.clear();
        for (int l = 0; l < s.nLayers; ++l)
        {
            auto& ly = s.layers[l];
            for (int k = 0; k < ly.nChunks; ++k)
            {
                if (! ly.init[k]) continue;
                const float* p = chunkPtr (ly.chunk[k]);
                const int start = k * chunkLen;
                const int cnt = (int) std::min<int64_t> (chunkLen, s.length - start);
                for (int i = 0; i < cnt; ++i)
                {
                    out.addSample (0, start + i, p[(size_t) i * 2] * ly.gain);
                    out.addSample (1, start + i, p[(size_t) i * 2 + 1] * ly.gain);
                }
            }
        }
    }
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Carousel Loops");
    dir.createDirectory();
    auto f = dir.getNonexistentChildFile ("Loop " + juce::String (slotNum) + " " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H%M%S"), ".wav");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0));
    if (w == nullptr) return {};
    w->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
    return f;
}

void CarouselProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    // settings only - loops live in memory (export them to keep them)
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void CarouselProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* CarouselProcessor::createEditor() { return new CarouselEditor (*this); }

#ifndef CAROUSEL_NO_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CarouselProcessor(); }
#endif
