#include "FuzzboxProcessor.h"
#include "FuzzboxEditor.h"

namespace
{
constexpr double kTwoPi = 6.283185307179586;
juce::String pct (float v, int) { return juce::String (juce::roundToInt (v * 100)) + "%"; }
juce::String hz (float v, int) { return juce::String (v, 2) + " Hz"; }
juce::String dB1 (float v, int) { return juce::String (v, 1) + " dB"; }

struct Preset { const char* name; bool on[5]; float wah, wahRange, fuzz, fuzzTone, fuzzLvl, oct, octLvl, trem, tremRate, spring, springDecay, drip; int wahMode, tremSync; };
const Preset kPresets[] = {
    // name                 wah    fuzz   oct    trem   spring   wah  rng   fuzz tone  lvl   oct  octL trem rate  sprg dec  drip  wmode tsync
    { "Buzzing Lead",       { false, true,  false, false, true  }, 0.5f, 0.7f, 0.75f, 0.55f, 0.6f, 0.6f, 0.5f, 0.5f, 5.0f, 0.18f, 0.4f, 0.4f, 0, 0 },
    { "Octave Scream",      { false, true,  true,  false, false }, 0.5f, 0.7f, 0.9f,  0.6f,  0.55f, 0.85f, 0.6f, 0.5f, 5.0f, 0.2f, 0.4f, 0.4f, 0, 0 },
    { "Wah Rhythm",         { true,  false, false, false, false }, 0.5f, 0.8f, 0.5f,  0.5f,  0.6f, 0.5f, 0.5f, 0.5f, 5.0f, 0.2f, 0.4f, 0.4f, 1, 0 },
    { "Wah into Fuzz",      { true,  true,  false, false, false }, 0.5f, 0.8f, 0.8f,  0.5f,  0.55f, 0.5f, 0.5f, 0.5f, 5.0f, 0.2f, 0.4f, 0.4f, 0, 0 },
    { "Surf Tank",          { false, false, false, false, true  }, 0.5f, 0.7f, 0.5f,  0.5f,  0.6f, 0.5f, 0.5f, 0.5f, 5.0f, 0.55f, 0.6f, 0.85f, 0, 0 },
    { "Tremolo Twang",      { false, false, false, true,  true  }, 0.5f, 0.7f, 0.5f,  0.5f,  0.6f, 0.5f, 0.5f, 0.7f, 5.5f, 0.3f, 0.5f, 0.5f, 0, 2 },
    { "Sweeping Psych",     { true,  true,  false, false, true  }, 0.5f, 0.9f, 0.7f,  0.45f, 0.55f, 0.5f, 0.5f, 0.5f, 5.0f, 0.3f, 0.5f, 0.5f, 2, 0 },
    { "Choppy Fuzz",        { false, true,  false, true,  false }, 0.5f, 0.7f, 0.85f, 0.55f, 0.55f, 0.5f, 0.5f, 1.0f, 6.0f, 0.2f, 0.4f, 0.4f, 0, 3 },
    { "The Whole Board",    { true,  true,  true,  true,  true  }, 0.5f, 0.8f, 0.8f,  0.5f,  0.5f, 0.6f, 0.45f, 0.45f, 5.0f, 0.3f, 0.5f, 0.5f, 1, 2 },
};
}

juce::StringArray FuzzboxProcessor::presetNames() { juce::StringArray a; for (auto& p : kPresets) a.add (p.name); return a; }

juce::AudioProcessorValueTreeState::ParameterLayout FuzzboxProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> ps;
    auto F = [&] (const char* id, const char* name, juce::NormalisableRange<float> r, float def, juce::String (*fmt) (float, int))
    { ps.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, r, def, juce::AudioParameterFloatAttributes().withStringFromValueFunction (fmt))); };
    auto C = [&] (const char* id, const char* name, juce::StringArray items, int def)
    { ps.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def)); };
    auto B = [&] (const char* id, const char* name, bool def) { ps.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def)); };
    B ("wah_on", "Wah On", false);
    C ("wah_mode", "Wah", { "Your foot (pedal / CC 11)", "Auto (follows your playing)", "Sweeps in time" }, 0);
    F ("wah_pos", "Wah Pedal", { 0, 1, 0.001f }, 0.5f, pct);
    F ("wah_range", "Wah Range", { 0, 1, 0.001f }, 0.7f, pct);
    B ("fuzz_on", "Fuzz On", true);
    F ("fuzz", "Fuzz", { 0, 1, 0.001f }, 0.75f, pct);
    F ("fuzz_tone", "Fuzz Tone", { 0, 1, 0.001f }, 0.55f, pct);
    F ("fuzz_lvl", "Fuzz Level", { 0, 1, 0.001f }, 0.6f, pct);
    B ("oct_on", "Octave On", false);
    F ("oct", "Octave", { 0, 1, 0.001f }, 0.6f, pct);
    F ("oct_lvl", "Octave Level", { 0, 1, 0.001f }, 0.5f, pct);
    B ("trem_on", "Tremolo On", false);
    F ("trem", "Tremolo Depth", { 0, 1, 0.001f }, 0.5f, pct);
    F ("trem_rate", "Tremolo Speed", { 0.5f, 12.0f, 0.01f, 0.6f }, 5.0f, hz);
    C ("trem_sync", "Tremolo Time", { "Free", "Quarters", "Eighths", "Eighth triplets", "Sixteenths" }, 0);
    C ("trem_shape", "Tremolo Shape", { "Smooth", "Choppy" }, 0);
    B ("spring_on", "Spring On", true);
    F ("spring", "Spring Mix", { 0, 1, 0.001f }, 0.18f, pct);
    F ("spring_decay", "Spring Decay", { 0, 1, 0.001f }, 0.4f, pct);
    F ("spring_drip", "Drip", { 0, 1, 0.001f }, 0.4f, pct);
    F ("out", "Output", { -24, 12, 0.1f }, 0, dB1);
    return { ps.begin(), ps.end() };
}

FuzzboxProcessor::FuzzboxProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true).withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Fuzzbox", createLayout())
{
}

void FuzzboxProcessor::loadPreset (int i)
{
    if (i < 0 || i >= (int) std::size (kPresets)) return;
    const auto& p = kPresets[i];
    auto set = [&] (const char* id, float v) { if (auto* q = apvts.getParameter (id)) q->setValueNotifyingHost (q->convertTo0to1 (v)); };
    for (int k = 0; k < NumPedals; ++k) set (onId (k), p.on[k] ? 1.0f : 0.0f);
    set ("wah_pos", p.wah); set ("wah_range", p.wahRange); set ("wah_mode", (float) p.wahMode);
    set ("fuzz", p.fuzz); set ("fuzz_tone", p.fuzzTone); set ("fuzz_lvl", p.fuzzLvl);
    set ("oct", p.oct); set ("oct_lvl", p.octLvl);
    set ("trem", p.trem); set ("trem_rate", p.tremRate); set ("trem_sync", (float) p.tremSync);
    set ("spring", p.spring); set ("spring_decay", p.springDecay); set ("spring_drip", p.drip);
}

void FuzzboxProcessor::prepareToPlay (double s, int block)
{
    sr = s;
    os = std::make_unique<juce::dsp::Oversampling<float>> (2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    os->initProcessing ((size_t) juce::jmax (block, 512));
    setLatencySamples ((int) std::lround (os->getLatencyInSamples()));
    static const double lens[2] = { 0.0373, 0.0411 };
    for (int c = 0; c < 2; ++c) { tank[c].d.assign ((size_t) (sr * 0.1) + 4, 0.0f); tank[c].w = 0; tank[c].lp = tank[c].fb = 0; for (auto& a : tank[c].ap) a = 0; tank[c].mph = c * 0.3; juce::ignoreUnused (lens); }
    for (auto& a : svL) a[0] = a[1] = 0;
    env = 0; tremPh = 0; lfoPh = 0;
}

void FuzzboxProcessor::handleMidi (const juce::MidiMessage& m)
{
    auto toggle = [&] (int p) { if (auto* q = apvts.getParameter (onId (p))) q->setValueNotifyingHost (q->getValue() > 0.5f ? 0.0f : 1.0f); };
    if (m.isNoteOn())
    {
        static const int notes[NumPedals] = { 60, 62, 64, 65, 67 };     // C3 D3 E3 F3 G3
        for (int p = 0; p < NumPedals; ++p) if (m.getNoteNumber() == notes[p]) toggle (p);
    }
    else if (m.isController())
    {
        const int cc = m.getControllerNumber(), v = m.getControllerValue();
        if (cc == 11 || cc == 4) wahPos = (float) v / 127.0f, apvts.getParameter ("wah_pos")->setValueNotifyingHost ((float) v / 127.0f);
        else if (cc >= 80 && cc < 80 + NumPedals) if (auto* q = apvts.getParameter (onId (cc - 80))) q->setValueNotifyingHost (v >= 64 ? 1.0f : 0.0f);
    }
}

void FuzzboxProcessor::processSpring (float* L, float* R, int n, float mix, float decay, float drip)
{
    // a spring tank: a chain of allpasses (the springs' dispersion - the "boing") in a feedback loop
    static const double base[2] = { 0.0317, 0.0353 };
    const float fbG = 0.55f + 0.38f * decay;
    const float apK = 0.55f + 0.2f * drip;
    for (int c = 0; c < 2; ++c)
    {
        auto& t = tank[c];
        float* x = c == 0 ? L : R;
        const int N = (int) t.d.size();
        for (int i = 0; i < n; ++i)
        {
            t.mph += 0.31 / sr; if (t.mph > 1) t.mph -= 1;
            const double dly = (base[c] + 0.0004 * std::sin (kTwoPi * t.mph)) * sr;
            double rp = (double) t.w - dly; while (rp < 0) rp += N;
            const int a = (int) rp % N; const float fr = (float) (rp - std::floor (rp));
            float y = t.d[(size_t) a] + (t.d[(size_t) ((a + 1) % N)] - t.d[(size_t) a]) * fr;
            // dispersion
            for (int k = 0; k < 12; ++k) { const float v = y - apK * t.ap[k]; const float o = t.ap[k] + apK * v; t.ap[k] = v; y = o; }
            t.lp += (y - t.lp) * 0.45f;     // the tank is dark
            const float in = (x[i] + (c == 0 ? R[i] : L[i]) * 0.3f) * 0.5f;
            t.d[(size_t) t.w] = std::tanh (in * (0.6f + 0.6f * drip) + t.lp * fbG);
            t.w = (t.w + 1) % N;
            x[i] = x[i] * (1.0f - mix * 0.5f) + t.lp * mix * 1.6f;
        }
    }
}

void FuzzboxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    for (const auto m : midi) handleMidi (m.getMessage());
    midi.clear();
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2) return;
    // a mono guitar into a stereo track: both sides get it
    if (getTotalNumInputChannels() == 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);
    float* L = buffer.getWritePointer (0); float* R = buffer.getWritePointer (1);
    auto v = [&] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    auto on = [&] (int p) { return v (onId (p)) > 0.5f; };
    uiIn = juce::jmax (buffer.getMagnitude (0, 0, n), uiIn.load() * 0.9f);

    double bpm = 120, ppq = 0; bool playing = false;
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) { if (auto b = pos->getBpm()) bpm = *b; if (auto q = pos->getPpqPosition()) ppq = *q; playing = pos->getIsPlaying(); }

    // ---- WAH
    if (on (PWah))
    {
        const int mode = (int) v ("wah_mode");
        const float range = v ("wah_range");
        const float pedal = v ("wah_pos");
        const float atk = 1.0f - std::exp (-1.0f / (0.004f * (float) sr)), rel = 1.0f - std::exp (-1.0f / (0.12f * (float) sr));
        for (int i = 0; i < n; ++i)
        {
            const float a = std::abs (L[i]) + std::abs (R[i]);
            env += (a > env ? atk : rel) * (a - env);
            float pos = pedal;
            if (mode == 1) pos = juce::jlimit (0.0f, 1.0f, std::sqrt (env * 6.0f));
            else if (mode == 2)
            {
                const double q = playing ? ppq + i * bpm / 60.0 / sr : (lfoPh += 0.5 / sr);
                pos = (float) (0.5 - 0.5 * std::cos (kTwoPi * std::fmod (q / 2.0, 1.0)));     // a sweep every two beats
            }
            wahPos += (pos - wahPos) * 0.002f;
            const float f = 350.0f * std::pow (2.0f, wahPos * (1.2f + 1.6f * range));      // ~350 Hz to 2.2 kHz (more with RANGE)
            const float g = std::tan (juce::MathConstants<float>::pi * juce::jmin (f, (float) sr * 0.45f) / (float) sr);
            const float k = 1.0f / 6.0f;       // a sharp, vocal resonance
            const float a1 = 1.0f / (1.0f + g * (g + k));
            for (int c = 0; c < 2; ++c)
            {
                float& ic1 = svL[c][0]; float& ic2 = svL[c][1];
                float* x = c == 0 ? L : R;
                const float v3 = x[i] - ic2, v1 = a1 * (ic1 + g * v3), v2 = ic2 + g * v1;
                ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
                x[i] = v1 * k * 2.4f + v2 * 0.12f;    // mostly the band, a touch of the low end
            }
        }
        uiWah = wahPos;
    }

    // ---- FUZZ and OCTAVE FUZZ (4x oversampled so the buzz doesn't alias)
    if (on (PFuzz) || on (POct))
    {
        juce::dsp::AudioBlock<float> blk (buffer);
        auto up = os->processSamplesUp (blk);
        const int un = (int) up.getNumSamples();
        const double usr = sr * 4.0;
        const float fz = v ("fuzz"), tone = v ("fuzz_tone"), lvl = v ("fuzz_lvl"), oct = v ("oct"), octLvl = v ("oct_lvl");
        const bool doF = on (PFuzz), doO = on (POct);
        const float pre = std::pow (10.0f, (6.0f + 44.0f * fz) / 20.0f);
        const float hpK = 1.0f - std::exp ((float) (-kTwoPi * 120.0 / usr));
        const float toneK = 1.0f - std::exp ((float) (-kTwoPi * (900.0 + 6000.0 * tone) / usr));
        const float bpK = 1.0f - std::exp ((float) (-kTwoPi * 1200.0 / usr));
        for (int c = 0; c < 2; ++c)
        {
            float* x = up.getChannelPointer ((size_t) c);
            for (int i = 0; i < un; ++i)
            {
                float s = x[i];
                if (doO)
                {
                    // octave up: rectify a mid-heavy copy, then clip it - the ringing, hollow octave
                    octBp[c][0] += (s - octBp[c][0]) * bpK; const float band = s - octBp[c][0];
                    octBp[c][1] += (band - octBp[c][1]) * 0.25f;
                    const float rect = std::abs (octBp[c][1]) * (1.0f + 6.0f * oct);
                    float o = std::tanh (rect * 3.0f) - 0.5f * std::tanh (rect * 0.5f);
                    octLp[c] += (o - octLp[c]) * 0.2f;
                    s = s * (1.0f - 0.5f * oct) + octLp[c] * (0.8f + 0.8f * oct);
                }
                if (doF)
                {
                    // two germanium transistors: lots of gain, a lopsided, spitting clip, and a bit of sag
                    fuzzHp[c] += (s - fuzzHp[c]) * hpK; float y = (s - fuzzHp[c]) * pre;
                    y += 0.18f * fz;                          // the bias: it clips one side first
                    y = y > 0 ? std::tanh (y * 1.3f) : std::tanh (y * 0.75f) * 0.8f;
                    y = std::tanh (y * (1.5f + 2.0f * fz));
                    // dc blocker
                    const float dc = y - dcX[c] + 0.99985f * dcY[c];   // ~4 Hz at the 4x rate dcX[c] = y; dcY[c] = dc; y = dc;
                    toneZ[c] += (y - toneZ[c]) * toneK; y = toneZ[c] * (0.6f + 0.4f * tone) + (y - toneZ[c]) * 0.15f * tone;
                    s = y * (0.05f + 0.35f * lvl);
                }
                else if (doO) s *= 0.4f + 0.9f * octLvl;
                x[i] = s;
            }
        }
        os->processSamplesDown (blk);
    }

    // ---- TREMOLO
    if (on (PTrem))
    {
        const float depth = v ("trem");
        const int sync = (int) v ("trem_sync");
        const bool choppy = v ("trem_shape") > 0.5f;
        const double perBeat[] = { 0, 1, 2, 3, 4 };
        for (int i = 0; i < n; ++i)
        {
            double ph;
            if (sync > 0 && playing) ph = std::fmod ((ppq + i * bpm / 60.0 / sr) * perBeat[sync], 1.0);
            else { tremPh += v ("trem_rate") / sr; tremPh -= std::floor (tremPh); ph = tremPh; }
            float lfo = (float) (0.5 + 0.5 * std::cos (kTwoPi * ph));
            if (choppy) lfo = juce::jlimit (0.0f, 1.0f, (lfo - 0.5f) * 5.0f + 0.5f);
            const float g = 1.0f - depth * (1.0f - lfo);
            L[i] *= g; R[i] *= g;
            if (i == 0) uiTrem = g;
        }
    }

    // ---- SPRING
    if (on (PSpring)) processSpring (L, R, n, v ("spring"), v ("spring_decay"), v ("spring_drip"));

    buffer.applyGain (juce::Decibels::decibelsToGain (v ("out")));
    for (int c = 0; c < 2; ++c) { auto* x = buffer.getWritePointer (c); for (int i = 0; i < n; ++i) x[i] = std::isfinite (x[i]) ? juce::jlimit (-2.0f, 2.0f, x[i]) : 0.0f; }
    uiOut = juce::jmax (buffer.getMagnitude (0, 0, n), uiOut.load() * 0.9f);
}

void FuzzboxProcessor::getStateInformation (juce::MemoryBlock& d) { if (auto x = apvts.copyState().createXml()) copyXmlToBinary (*x, d); }
void FuzzboxProcessor::setStateInformation (const void* data, int size)
{
    if (auto x = getXmlFromBinary (data, size)) if (x->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*x));
}

juce::AudioProcessorEditor* FuzzboxProcessor::createEditor() { return new FuzzboxEditor (*this); }

#ifndef FUZZBOX_NO_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FuzzboxProcessor(); }
#endif
