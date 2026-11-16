#include "SwirlProcessor.h"
#include "SwirlEditor.h"

namespace
{
constexpr double kTwoPi = 6.283185307179586;
juce::String pct (float v, int) { return juce::String (juce::roundToInt (v * 100)) + "%"; }
juce::String hz (float v, int) { return juce::String (v, 2) + " Hz"; }
juce::String dB1 (float v, int) { return juce::String (v, 1) + " dB"; }
float tap (const std::vector<float>& d, int w, double delay)
{
    const int sz = (int) d.size();
    double rp = (double) w - delay; while (rp < 0) rp += sz;
    const int a = (int) rp % sz; const float f = (float) (rp - std::floor (rp));
    return d[(size_t) a] + (d[(size_t) ((a + 1) % sz)] - d[(size_t) a]) * f;
}
struct Preset { const char* name; float back; int len; float phase, phRate, phFb, rotary; int speed; float flange, flRate, flFb, mix; };
const Preset kPresets[] = {
    { "Backwards Guitar",   1.0f, 1, 0.0f, 0.3f, 0.3f, 0.0f, 0, 0.0f, 0.15f, 0.3f, 0.7f },
    { "Spinning Speaker",   0.0f, 1, 0.0f, 0.3f, 0.3f, 1.0f, 1, 0.0f, 0.15f, 0.3f, 1.0f },
    { "Slow Rotary Vocal",  0.0f, 1, 0.0f, 0.3f, 0.3f, 0.8f, 0, 0.0f, 0.15f, 0.3f, 1.0f },
    { "Tape Phasing Drums", 0.0f, 1, 0.0f, 0.3f, 0.3f, 0.0f, 0, 0.9f, 0.08f, 0.55f, 1.0f },
    { "Wide Sweep",         0.0f, 1, 0.9f, 0.2f, 0.6f, 0.0f, 0, 0.5f, 0.1f, 0.4f, 1.0f },
    { "Everything Swims",   0.35f, 2, 0.6f, 0.25f, 0.4f, 0.6f, 0, 0.4f, 0.12f, 0.3f, 0.8f },
    { "Reverse Ghost",      0.6f, 3, 0.4f, 0.15f, 0.3f, 0.0f, 0, 0.0f, 0.15f, 0.3f, 0.45f },
    { "Jet Plane",          0.0f, 1, 0.0f, 0.3f, 0.3f, 0.0f, 0, 1.0f, 0.05f, 0.8f, 1.0f },
};
}

juce::StringArray SwirlProcessor::presetNames() { juce::StringArray a; for (auto& p : kPresets) a.add (p.name); return a; }

juce::AudioProcessorValueTreeState::ParameterLayout SwirlProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> ps;
    auto F = [&] (const char* id, const char* name, juce::NormalisableRange<float> r, float def, juce::String (*fmt) (float, int))
    { ps.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, r, def, juce::AudioParameterFloatAttributes().withStringFromValueFunction (fmt))); };
    auto C = [&] (const char* id, const char* name, juce::StringArray items, int def)
    { ps.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def)); };
    F ("back", "Backwards", { 0, 1, 0.001f }, 0, pct);
    C ("back_len", "Backwards Length", { "1/2 beat", "1 beat", "2 beats", "1 bar" }, 1);
    F ("phase", "Phase", { 0, 1, 0.001f }, 0, pct);
    F ("phase_rate", "Phase Rate", { 0.03f, 4.0f, 0.001f, 0.4f }, 0.3f, hz);
    F ("phase_fb", "Phase Feedback", { 0, 0.9f, 0.001f }, 0.3f, pct);
    F ("rotary", "Rotary", { 0, 1, 0.001f }, 0.8f, pct);
    C ("rotary_speed", "Rotary Speed", { "Slow", "Fast" }, 0);
    F ("flange", "Tape Flange", { 0, 1, 0.001f }, 0, pct);
    F ("flange_rate", "Flange Rate", { 0.02f, 2.0f, 0.001f, 0.4f }, 0.15f, hz);
    F ("flange_fb", "Flange Feedback", { 0, 0.9f, 0.001f }, 0.3f, pct);
    F ("mix", "Mix", { 0, 1, 0.001f }, 1.0f, pct);
    F ("out", "Output", { -24, 6, 0.1f }, 0, dB1);
    return { ps.begin(), ps.end() };
}

SwirlProcessor::SwirlProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true).withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Swirl", createLayout())
{
    band::available();
}

void SwirlProcessor::loadPreset (int i)
{
    if (i < 0 || i >= (int) (sizeof (kPresets) / sizeof (kPresets[0]))) return;
    const auto& p = kPresets[i];
    auto set = [&] (const char* id, float v) { if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v)); };
    set ("back", p.back); set ("back_len", (float) p.len); set ("phase", p.phase); set ("phase_rate", p.phRate); set ("phase_fb", p.phFb);
    set ("rotary", p.rotary); set ("rotary_speed", (float) p.speed); set ("flange", p.flange); set ("flange_rate", p.flRate); set ("flange_fb", p.flFb); set ("mix", p.mix);
}

void SwirlProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& r : rec) r.assign ((size_t) (sr * 8.5) + 16, 0.0f);     // room for a reversed bar at slow tempos
    for (auto& r : rot) r.assign ((size_t) (sr * 0.02) + 16, 0.0f);
    for (auto& r : fl)  r.assign ((size_t) (sr * 0.03) + 16, 0.0f);
    recW = rotW = flW = 0; revK = 0; revStart = 0;
    for (auto& c : apZ) for (auto& z : c) z = 0;
    phFb[0] = phFb[1] = flFb[0] = flFb[1] = 0;
}

void SwirlProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    midi.clear();
    for (int c = 2; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, buffer.getNumSamples());
    if (buffer.getNumChannels() < 2 || rec[0].empty()) return;
    band::markPresent (band::WSwirl);
    bench.begin (buffer, band::WSwirl, true);
    uiLinked = bench.active || band::isPresent (band::WBandleader);
    processCore (buffer);
    bench.end (buffer, true);
}

void SwirlProcessor::processCore (juce::AudioBuffer<float>& buffer)
{
    const int n = buffer.getNumSamples();
    auto g = [&] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    const float back = g ("back"), phase = g ("phase"), phRate = g ("phase_rate"), phFbAmt = g ("phase_fb");
    const float rotary = g ("rotary"), flange = g ("flange"), flRate = g ("flange_rate"), flFbAmt = g ("flange_fb");
    const float mix = g ("mix"), out = juce::Decibels::decibelsToGain (g ("out"));
    const bool fast = g ("rotary_speed") > 0.5f;
    double bpm = 120;
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) if (auto b = pos->getBpm()) bpm = *b;
    static const double beats[] = { 0.5, 1.0, 2.0, 4.0 };
    const int wantLen = juce::jlimit (512, (int) rec[0].size() / 2 - 8, (int) (beats[juce::jlimit (0, 3, (int) g ("back_len"))] * 60.0 / juce::jmax (30.0, bpm) * sr));
    const int recSz = (int) rec[0].size(), rotSz = (int) rot[0].size(), flSz = (int) fl[0].size();
    const int fadeN = juce::jmin (revLen / 4, (int) (0.012 * sr));
    float* ch[2] = { buffer.getWritePointer (0), buffer.getWritePointer (1) };

    for (int i = 0; i < n; ++i)
    {
        const float dry[2] = { ch[0][i], ch[1][i] };
        float x[2] = { dry[0], dry[1] };

        // ---- BACKWARDS: every chunk, play the chunk just recorded in reverse
        rec[0][(size_t) recW] = dry[0]; rec[1][(size_t) recW] = dry[1];
        if (revK >= revLen) { revK = 0; revStart = recW; revLen = wantLen; }
        if (back > 0.001f)
        {
            const int rp = ((revStart - 1 - revK) % recSz + recSz) % recSz;
            const int fn = juce::jmax (1, fadeN);
            const float w = revK < fn ? (float) revK / (float) fn : (revK > revLen - fn ? (float) (revLen - revK) / (float) fn : 1.0f);
            for (int c = 0; c < 2; ++c) x[c] = x[c] * (1.0f - back) + rec[c][(size_t) rp] * w * back;
        }
        ++revK; recW = (recW + 1) % recSz;

        // ---- PHASE: six all-pass stages swept by a slow LFO, the two sides a little apart
        if (phase > 0.001f)
        {
            phPh += phRate / sr; if (phPh >= 1.0) phPh -= 1.0;
            for (int c = 0; c < 2; ++c)
            {
                const double lfo = 0.5 + 0.5 * std::sin (kTwoPi * (phPh + c * 0.12));
                const double fc = 250.0 * std::pow (16.0, lfo);
                const float a = (float) ((1.0 - std::tan (juce::MathConstants<double>::pi * fc / sr)) / (1.0 + std::tan (juce::MathConstants<double>::pi * fc / sr)));
                float y = x[c] + phFb[c] * phFbAmt;
                for (int s = 0; s < 6; ++s) { const float o = -a * y + apZ[c][s]; apZ[c][s] = y + a * o; y = o; }
                phFb[c] = y;
                x[c] = x[c] + (y - x[c]) * 0.5f * phase + y * 0.0f;
                x[c] = x[c] * (1.0f - 0.15f * phase);
            }
        }

        // ---- TAPE FLANGE: a copy on a second "machine" whose speed drifts, fed back for the jet sound
        fl[0][(size_t) flW] = x[0] + flFb[0] * flFbAmt; fl[1][(size_t) flW] = x[1] + flFb[1] * flFbAmt;
        if (flange > 0.001f)
        {
            flPh += flRate / sr; if (flPh >= 1.0) flPh -= 1.0;
            for (int c = 0; c < 2; ++c)
            {
                const double tri = 0.5 - 0.5 * std::cos (kTwoPi * (flPh + c * 0.05));
                const float d = tap (fl[c], flW, (0.0002 + 0.006 * tri * tri) * sr);
                flFb[c] = d;
                x[c] = (x[c] + d * flange) * (1.0f - 0.3f * flange);
            }
        }
        else flFb[0] = flFb[1] = 0;
        flW = (flW + 1) % flSz;

        // ---- ROTARY: the horn spins past two microphones (pitch wobble + loudness + the sides opposite)
        rot[0][(size_t) rotW] = x[0]; rot[1][(size_t) rotW] = x[1];
        if (rotary > 0.001f)
        {
            rotRate += ((fast ? 6.6 : 0.8) - rotRate) * (1.0 / (0.9 * sr));       // the motor takes a second to change speed
            rotPh += rotRate / sr; if (rotPh >= 1.0) rotPh -= 1.0;
            const float m = 0.5f * (x[0] + x[1]);
            for (int c = 0; c < 2; ++c)
            {
                const double s = std::sin (kTwoPi * (rotPh + c * 0.5));
                const float d = 0.5f * (tap (rot[0], rotW, (0.004 + 0.0011 * s) * sr) + tap (rot[1], rotW, (0.004 + 0.0011 * s) * sr));
                const float wet = d * (float) (0.72 + 0.28 * std::sin (kTwoPi * (rotPh + c * 0.5) + 1.2));
                x[c] = x[c] + (wet - x[c]) * rotary;
            }
            juce::ignoreUnused (m);
        }
        rotW = (rotW + 1) % rotSz;

        for (int c = 0; c < 2; ++c)
        {
            float y = (dry[c] + (x[c] - dry[c]) * mix) * out;
            if (std::abs (y) > 0.95f) y = std::copysign (0.95f + 0.05f * std::tanh ((std::abs (y) - 0.95f) * 12.0f), y);
            ch[c][i] = std::isfinite (y) ? y : 0.0f;
        }
    }
    uiSpin = (float) rotPh; uiSweep = (float) phPh; uiRev = revLen > 0 ? (float) revK / (float) revLen : 0.0f;
}

void SwirlProcessor::getStateInformation (juce::MemoryBlock& dest) { if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest); }
void SwirlProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size)) if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}
juce::AudioProcessorEditor* SwirlProcessor::createEditor() { return new SwirlEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SwirlProcessor(); }
