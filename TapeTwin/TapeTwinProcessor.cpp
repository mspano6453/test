#include "TapeTwinProcessor.h"
#include "TapeTwinEditor.h"
#include "../Source/BandBus.h"

using namespace tw;

namespace
{
juce::String pct (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
juce::String ms (float v, int)  { return juce::String (juce::roundToInt (v)) + " ms"; }
juce::String db (float v, int)  { return juce::String (v, 1) + " dB"; }
}

juce::AudioProcessorValueTreeState::ParameterLayout TapeTwinProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> ps;
    const auto& d = presets()[0].v;
    auto add = [&] (int p, const juce::String& name, juce::NormalisableRange<float> r, juce::String (*fmt) (float, int))
    {
        ps.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { paramId (p), 1 }, name, r, d[p],
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (fmt)));
    };
    juce::NormalisableRange<float> timeRange (30.0f, 2000.0f, 1.0f);
    timeRange.setSkewForCentre (350.0f);

    add (Drive, "Drive", { 0, 1, 0.001f }, pct);
    add (Time, "Echo Time", timeRange, ms);
    ps.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { paramId (Sync), 1 }, "Echo Sync", syncNames(), (int) d[Sync]));
    add (Feedback, "Feedback", { 0, 0.98f, 0.001f }, pct);
    add (Wow, "Wow", { 0, 1, 0.001f }, pct);
    add (Flutter, "Flutter", { 0, 1, 0.001f }, pct);
    add (Age, "Tape Age", { 0, 1, 0.001f }, pct);
    add (Spread, "Spread", { 0, 1, 0.001f }, pct);
    add (EchoMix, "Echo Mix", { 0, 1, 0.001f }, pct);
    add (AdtMix, "Double Mix", { 0, 1, 0.001f }, pct);
    add (AdtTime, "Double Time", { 5, 60, 0.1f }, ms);
    add (AdtWobble, "Double Wobble", { 0, 1, 0.001f }, pct);
    add (Output, "Output", { -24, 12, 0.1f }, db);
    ps.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "band_throws", 1 }, "Section Throws (Bandleader)", juce::StringArray { "Off", "On" }, 1));
    return { ps.begin(), ps.end() };
}

TapeTwinProcessor::TapeTwinProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TapeTwin", createLayout())
{
    for (int p = 0; p < NumParams; ++p) prm[p] = apvts.getRawParameterValue (paramId (p));
}

bool TapeTwinProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return l.getMainInputChannelSet() == out;
}

void TapeTwinProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (int c = 0; c < 2; ++c)
    {
        echo[c].setSize ((int) (sr * 2.3));
        adt[c].setSize ((int) (sr * 0.1));
        fbLp[c].z = fbHp[c].z = inLp[c].z = 0;
    }
    smoothDelay = 0.0;
}

void TapeTwinProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int chans = juce::jmin (2, buffer.getNumChannels());
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);
    if (chans == 0) return;

    const float drive = prm[Drive]->load(), fb = prm[Feedback]->load();
    const float wow = prm[Wow]->load(), flut = prm[Flutter]->load(), age = prm[Age]->load();
    const float spread = prm[Spread]->load(), echoMix = prm[EchoMix]->load();
    const float adtMix = prm[AdtMix]->load(), adtTime = prm[AdtTime]->load(), adtWob = prm[AdtWobble]->load();
    const float outG = juce::Decibels::decibelsToGain (prm[Output]->load());
    const int sync = (int) prm[Sync]->load();

    double delayMs = prm[Time]->load();
    if (sync > 0)
    {
        double bpm = 120.0;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto b = pos->getBpm()) bpm = *b;
        delayMs = syncBeats (sync) * 60000.0 / juce::jmax (30.0, bpm);
    }
    delayMs = juce::jlimit (20.0, 2000.0, delayMs);

    // the band: an echo "throw" on the last beat before each section change or drop-out
    band::markPresent (band::WTapeTwin);
    band::Leader leader;
    double ppq = 0, bpmNow = 120; bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            if (auto p = pos->getPpqPosition()) ppq = *p;
            if (auto b = pos->getBpm()) bpmNow = *b;
        }
    const bool led = playing && apvts.getRawParameterValue ("band_throws")->load() > 0.5f && band::readLeader (leader) && leader.playing;
    uiLinked = led;
    double throwStart = -1e18, throwEnd = -1e18;     // in beats
    if (led)
    {
        int64_t change = band::kNever;
        if (leader.nextStartBar < band::kNever) change = leader.nextStartBar;
        if (leader.dropStart < band::kNever && leader.dropStart < change && leader.dropStart * 4 > ppq - 1) change = leader.dropStart;
        if (change < band::kNever) { throwEnd = (double) change * 4.0; throwStart = throwEnd - 1.0; }
    }
    const double ppqPerSample = bpmNow / 60.0 / sr;
    const float throwDecay = (float) std::exp (-1.0 / (sr * 60.0 / juce::jmax (30.0, bpmNow) * 3.0));   // rings out over a few beats
    currentDelayMs = (float) delayMs;
    const double target = delayMs * 0.001 * sr;
    if (smoothDelay <= 0.0) smoothDelay = target;
    const double glide = 1.0 - std::exp (-1.0 / (0.25 * sr));   // tape speeds up/slows down when you turn Time

    for (int c = 0; c < 2; ++c)
    {
        fbLp[c].lp (sr, 9000.0 * std::pow (0.25, (double) age));
        fbHp[c].lp (sr, 80.0 + 250.0 * age);
    }

    const float g = 1.0f + 4.0f * drive;
    const float driveMakeup = 1.0f + 0.3f * drive;
    const double twoPi = juce::MathConstants<double>::twoPi;
    const float adtNorm = 1.0f / (1.0f + 0.45f * adtMix);
    float peak = 0.0f;

    float* io[2] = { buffer.getWritePointer (0), chans > 1 ? buffer.getWritePointer (1) : buffer.getWritePointer (0) };

    for (int i = 0; i < n; ++i)
    {
        // slow random drift for wow and the double-tracker
        if (--driftCounter <= 0)
        {
            driftCounter = (int) (sr * 0.35);
            driftTarget = rand01() * 2.0f - 1.0f;
            for (auto& t : adtDriftTarget) t = rand01() * 2.0f - 1.0f;
        }
        drift += (driftTarget - drift) * 0.00004f;
        for (int c = 0; c < 2; ++c) adtDrift[c] += (adtDriftTarget[c] - adtDrift[c]) * 0.00004f;

        wowPh += 0.55 / sr; if (wowPh > 1) wowPh -= 1;
        wowPh2 += 0.23 / sr; if (wowPh2 > 1) wowPh2 -= 1;
        flPh += 6.8 / sr; if (flPh > 1) flPh -= 1;
        flPh2 += 11.3 / sr; if (flPh2 > 1) flPh2 -= 1;
        const double wowMod = (0.6 * std::sin (twoPi * wowPh) + 0.25 * std::sin (twoPi * wowPh2) + 0.4 * drift) * wow * 4.0;
        const double flMod = (std::sin (twoPi * flPh) + 0.4 * std::sin (twoPi * flPh2)) * flut * 0.3;
        const double modSamples = (wowMod + flMod) * 0.001 * sr;

        smoothDelay += (target - smoothDelay) * glide;

        const double q = ppq + i * ppqPerSample;
        const bool inThrow = q >= throwStart && q < throwEnd;
        if (inThrow) throwEnv = 1.0f; else throwEnv *= throwDecay;
        const float fbNow = fb + (juce::jmax (fb, 0.72f) - fb) * throwEnv;
        const float mixNow = echoMix + (juce::jmax (echoMix, 0.55f) - echoMix) * throwEnv;
        const float sendNow = inThrow ? 1.6f : 1.0f;

        float dry[2], e[2];
        for (int c = 0; c < 2; ++c)
        {
            float x = io[c][i];
            if (drive > 0.001f) x = std::tanh (g * x) / g * driveMakeup;
            dry[c] = x;
            const double d = smoothDelay * (c == 1 ? 1.0 + 0.04 * spread : 1.0) + modSamples * (c == 0 ? 1.0 : 0.85);
            e[c] = fbHp[c].hpProcess (fbLp[c].lpProcess (echo[c].read (d)));
        }

        for (int c = 0; c < 2; ++c)
        {
            const float cross = (1.0f - 0.5f * spread) * e[c] + 0.5f * spread * e[1 - c];
            float w = dry[c] * sendNow + fbNow * cross;
            w = std::tanh (1.2f * w) / 1.2f;                            // tape saturates in the loop
            w += (rand01() - 0.5f) * 0.0006f * age;                     // a little hiss on old tape
            echo[c].write (w);

            adt[c].write (dry[c]);
            const double ad = (adtTime * (c == 0 ? 1.0 : 1.31) + adtWob * (2.5 * std::sin (twoPi * adtPh[c]) + 1.5 * adtDrift[c])) * 0.001 * sr;
            adtPh[c] += (0.31 + 0.11 * c) / sr; if (adtPh[c] > 1) adtPh[c] -= 1;
            const float a = adt[c].read (juce::jmax (2.0, ad));

            float y = (dry[c] + adtMix * 0.9f * a) * adtNorm + mixNow * e[c];
            peak = juce::jmax (peak, std::abs (e[c]));
            io[c][i] = y * outG;
        }
        if (chans == 1) io[0][i] = 0.5f * (io[0][i] + io[1][i]);
    }
    echoLevel.store (peak);
}

void TapeTwinProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets().size())) return;
    currentProgram = index;
    for (int p = 0; p < NumParams; ++p)
        if (auto* a = apvts.getParameter (paramId (p)))
            a->setValueNotifyingHost (a->convertTo0to1 (presets()[(size_t) index].v[p]));
}

const juce::String TapeTwinProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, (int) presets().size()) ? juce::String (presets()[(size_t) index].name) : juce::String();
}

void TapeTwinProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void TapeTwinProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) state.getProperty ("program", 0);
            apvts.replaceState (state);
        }
}

juce::AudioProcessorEditor* TapeTwinProcessor::createEditor() { return new TapeTwinEditor (*this); }

#ifndef TAPETWIN_NO_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TapeTwinProcessor(); }
#endif
