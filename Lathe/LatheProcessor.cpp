#include "../Source/SongForm.h"
#include "LatheProcessor.h"
#include "LatheEditor.h"
#include "../Source/BandBus.h"

using namespace lt;

namespace
{
juce::String pct (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
juce::String dB1 (float v, int) { return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB"; }
juce::String hz (float v, int)  { return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; }
juce::String msT (float v, int) { return juce::String (v, v < 10 ? 1 : 0) + " ms"; }
juce::String ratioT (float v, int) { return juce::String (v, 1) + ":1"; }
juce::String q2 (float v, int)  { return juce::String (v, 2); }
juce::String wid (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }

const float kEqFreq[NumEqBands] = { 25, 90, 250, 800, 2500, 5000, 10000, 20000 };
const float kEqQ[NumEqBands]    = { 0.707f, 0.707f, 0.8f, 0.8f, 0.9f, 0.9f, 0.707f, 0.707f };

struct PresetDef { const char* name; std::vector<std::pair<juce::String, float>> values; };

const std::vector<PresetDef>& presetDefs()
{
    static const std::vector<PresetDef> p {
        { "Clean Streaming (-14)", {
            { modOnId (MEq), 1 }, { modOnId (MDyn), 1 }, { modOnId (MMax), 1 },
            { dyId (0, "ratio"), 1.6f }, { dyId (1, "ratio"), 1.4f }, { dyId (2, "ratio"), 1.4f },
            { "mx_gain", 4.0f }, { "mx_ceil", -1.0f } } },
        { "Warm Analog", {
            { modOnId (MEq), 1 }, { modOnId (MTape), 1 }, { modOnId (MDyn), 1 }, { modOnId (MExciter), 1 }, { modOnId (MMax), 1 },
            { "tp_drive", 0.45f }, { "tp_bump", 2.0f }, { "tp_warmth", 0.45f }, { exId (0, "amt"), 0.25f }, { exId (1, "amt"), 0.15f },
            { exId (2, "amt"), 0.0f }, { dyId (0, "ratio"), 2.0f }, { dyId (1, "ratio"), 1.6f }, { "mx_gain", 5.0f } } },
        { "Loud Rock", {
            { modOnId (MEq), 1 }, { modOnId (MTape), 1 }, { modOnId (MDyn), 1 }, { modOnId (MExciter), 1 }, { modOnId (MImager), 1 }, { modOnId (MMax), 1 },
            { "tp_drive", 0.35f }, { dyId (0, "thr"), -22.0f }, { dyId (0, "ratio"), 2.5f }, { dyId (1, "thr"), -20.0f }, { dyId (1, "ratio"), 2.2f },
            { dyId (2, "thr"), -22.0f }, { dyId (2, "ratio"), 2.0f }, { exId (1, "amt"), 0.25f }, { exId (2, "amt"), 0.3f },
            { imId (2), 1.2f }, { "mx_gain", 9.0f }, { "mx_ceil", -1.0f }, { "mx_rel", 80.0f } } },
        { "Gentle Folk", {
            { modOnId (MEq), 1 }, { modOnId (MTape), 1 }, { modOnId (MDyn), 1 }, { modOnId (MMax), 1 },
            { "tp_drive", 0.2f }, { "tp_warmth", 0.25f }, { dyId (0, "ratio"), 1.3f }, { dyId (1, "ratio"), 1.3f }, { dyId (2, "ratio"), 1.3f },
            { "mx_gain", 3.0f }, { "mx_rel", 250.0f } } },
        { "Sixties Mono-ish", {
            { modOnId (MEq), 1 }, { modOnId (MTape), 1 }, { modOnId (MDyn), 1 }, { modOnId (MImager), 1 }, { modOnId (MMax), 1 },
            { "tp_speed", 0 }, { "tp_drive", 0.5f }, { "tp_warmth", 0.6f }, { "tp_bump", 2.5f },
            { eqId (EqHighCut, "on"), 1 }, { eqId (EqHighCut, "f"), 14000.0f }, { imId (0), 0.0f }, { imId (1), 0.55f }, { imId (2), 0.7f },
            { dyId (1, "ratio"), 2.0f }, { "mx_gain", 4.0f } } },
        { "Flat (start here)", {} },
        // the 90s psych-pop master: everything pushed hard into tape and a pumping compressor
        { "Blown Out (90s psych)", {
            { modOnId (MEq), 1 }, { modOnId (MTape), 1 }, { modOnId (MDyn), 1 }, { modOnId (MExciter), 1 }, { modOnId (MImager), 1 }, { modOnId (MMax), 1 },
            { "tp_drive", 0.8f }, { "tp_bump", 3.0f }, { "tp_warmth", 0.5f },
            { dyId (0, "thr"), -28.0f }, { dyId (0, "ratio"), 4.0f }, { dyId (0, "att"), 25.0f }, { dyId (0, "rel"), 120.0f },
            { dyId (1, "thr"), -30.0f }, { dyId (1, "ratio"), 4.5f }, { dyId (1, "att"), 8.0f }, { dyId (1, "rel"), 90.0f },
            { dyId (2, "thr"), -26.0f }, { dyId (2, "ratio"), 3.0f },
            { exId (1, "amt"), 0.45f }, { exId (2, "amt"), 0.35f }, { imId (2), 1.3f },
            { "mx_gain", 10.0f }, { "mx_ceil", -1.0f }, { "mx_rel", 60.0f } } },
    };
    return p;
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout LatheProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> ps;
    auto F = [&] (const juce::String& id, const juce::String& name, juce::NormalisableRange<float> r, float def, juce::String (*fmt) (float, int))
    {
        ps.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, r, def,
                                                                   juce::AudioParameterFloatAttributes().withStringFromValueFunction (fmt)));
    };
    // on/off switches are two-item choices: they save and restore cleanly in every host
    auto B = [&] (const juce::String& id, const juce::String& name, bool def)
    { ps.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, juce::StringArray { "Off", "On" }, def ? 1 : 0)); };
    auto C = [&] (const juce::String& id, const juce::String& name, juce::StringArray items, int def)
    { ps.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def)); };
    auto freqRange = [] (float lo, float hi) { juce::NormalisableRange<float> r (lo, hi, 0.1f); r.setSkewForCentre (std::sqrt (lo * hi)); return r; };

    F ("in_gain", "Input Gain", { -18, 18, 0.1f }, 0, dB1);
    F ("out_gain", "Output Gain", { -18, 6, 0.1f }, 0, dB1);
    C ("dither", "Dither", { "Off", "16-bit", "24-bit" }, 0);
    C ("tape_stop", "Tape Stop", { "Off", "At the end of the song", "Footswitch only" }, 1);

    const bool defOn[NumModules] = { true, false, false, true, false, false, true };
    for (int m = 0; m < NumModules; ++m) B (modOnId (m), juce::String (moduleName (m)) + " On", defOn[m]);

    // Equalizer
    B ("eq_steep", "EQ Steep Cuts", true);
    for (int b = 0; b < NumEqBands; ++b)
    {
        const juce::String nm = juce::String ("EQ ") + eqBandName (b);
        B (eqId (b, "on"), nm + " On", b == EqLowCut);
        F (eqId (b, "f"), nm + " Freq", freqRange (20, 20000), kEqFreq[b], hz);
        F (eqId (b, "g"), nm + " Gain", { -12, 12, 0.1f }, 0, dB1);
        F (eqId (b, "q"), nm + " Q", juce::NormalisableRange<float> (0.2f, 8.0f, 0.01f, 0.5f), kEqQ[b], q2);
    }
    // Dynamic EQ
    const float deF[2] = { 280, 3500 };
    for (int b = 0; b < DynamicEq::kBands; ++b)
    {
        const juce::String nm = "Dyn EQ " + juce::String (b + 1);
        B (deId (b, "on"), nm + " On", true);
        F (deId (b, "f"), nm + " Freq", freqRange (40, 16000), deF[b], hz);
        F (deId (b, "q"), nm + " Q", juce::NormalisableRange<float> (0.3f, 8.0f, 0.01f, 0.5f), 1.4f, q2);
        F (deId (b, "thr"), nm + " Threshold", { -60, 0, 0.1f }, -28, dB1);
        F (deId (b, "range"), nm + " Range", { 0, 12, 0.1f }, 4, dB1);
    }
    // Dynamics
    F ("dy_x1", "Dynamics Low/Mid", freqRange (40, 1000), 120, hz);
    F ("dy_x2", "Dynamics Mid/High", freqRange (1000, 12000), 2500, hz);
    const juce::String bandNm[3] = { "Low", "Mid", "High" };
    for (int b = 0; b < 3; ++b)
    {
        const juce::String nm = "Dynamics " + bandNm[b];
        F (dyId (b, "thr"), nm + " Threshold", { -48, 0, 0.1f }, -18, dB1);
        F (dyId (b, "ratio"), nm + " Ratio", juce::NormalisableRange<float> (1, 10, 0.01f, 0.5f), 1.5f, ratioT);
        F (dyId (b, "att"), nm + " Attack", juce::NormalisableRange<float> (0.1f, 200, 0.1f, 0.4f), b == 0 ? 40.0f : 20.0f, msT);
        F (dyId (b, "rel"), nm + " Release", juce::NormalisableRange<float> (10, 1000, 1, 0.5f), b == 0 ? 250.0f : 150.0f, msT);
        F (dyId (b, "gain"), nm + " Gain", { -12, 12, 0.1f }, 0, dB1);
    }
    // Exciter
    F ("ex_x1", "Exciter Low/Mid", freqRange (60, 1500), 300, hz);
    F ("ex_x2", "Exciter Mid/High", freqRange (1500, 12000), 4000, hz);
    F ("ex_mix", "Exciter Mix", { 0, 1, 0.001f }, 1, pct);
    const int exModeDef[3] = { ExWarm, ExTube, ExTape };
    const float exAmtDef[3] = { 0.0f, 0.2f, 0.25f };
    for (int b = 0; b < 3; ++b)
    {
        F (exId (b, "amt"), "Exciter " + bandNm[b] + " Amount", { 0, 1, 0.001f }, exAmtDef[b], pct);
        C (exId (b, "mode"), "Exciter " + bandNm[b] + " Mode", { "Tape", "Tube", "Warm", "Retro" }, exModeDef[b]);
    }
    // Imager
    F ("im_x1", "Imager Low/Mid", freqRange (60, 1500), 250, hz);
    F ("im_x2", "Imager Mid/High", freqRange (1500, 12000), 4000, hz);
    for (int b = 0; b < 3; ++b) F (imId (b), "Imager " + bandNm[b] + " Width", { 0, 2, 0.01f }, 1, wid);
    F ("im_stereoize", "Imager Stereoize", { 0, 1, 0.001f }, 0, pct);
    // Tape
    F ("tp_drive", "Tape Drive", { 0, 1, 0.001f }, 0.3f, pct);
    C ("tp_speed", "Tape Speed", { "15 ips", "30 ips" }, 1);
    F ("tp_bump", "Tape Low Bump", { 0, 4, 0.01f }, 1.5f, dB1);
    F ("tp_warmth", "Tape Warmth", { 0, 1, 0.001f }, 0.3f, pct);
    F ("tp_mix", "Tape Mix", { 0, 1, 0.001f }, 1, pct);
    // Maximizer
    F ("mx_gain", "Maximizer Gain", { 0, 20, 0.1f }, 0, dB1);
    F ("mx_ceil", "Maximizer Ceiling", { -6, 0, 0.1f }, -1, dB1);
    F ("mx_rel", "Maximizer Release", juce::NormalisableRange<float> (10, 1000, 1, 0.5f), 120, msT);
    B ("mx_tp", "Maximizer True Peak", true);

    ps.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "take_style", 1 }, "Take Style From Bandleader", juce::StringArray { "Off", "On" }, 1));
    return { ps.begin(), ps.end() };
}

LatheProcessor::LatheProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Lathe", createLayout())
{
    band::available();   // open the band link here (message thread), not on the audio thread
    const int def[NumModules] = { MEq, MDynEq, MTape, MDyn, MExciter, MImager, MMax };
    for (int i = 0; i < NumModules; ++i) order[(size_t) i].store (def[i]);
}

bool LatheProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && l.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void LatheProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    for (auto& b : tsBuf) b.assign ((size_t) (sr * 6.0), 0.0f);
    tsW = 0; tsState = 0; tsRead = 0; tsRate = 1;
    eq.prepare (sr); deq.prepare (sr); tape.prepare (sr); dyn.prepare (sr);
    ex.prepare (sr); im.prepare (sr); mx.prepare (sr);
    inLoud.prepare (sr); outLoud.prepare (sr);
    scratch.assign ((size_t) juce::jmax (512, samplesPerBlock) * 2, 0.0f);
    setLatencySamples (mx.latency());
}

void LatheProcessor::setOrder (const std::array<int, NumModules>& o)
{
    for (int i = 0; i < NumModules; ++i) order[(size_t) i].store (o[(size_t) i]);
}

void LatheProcessor::readParams()
{
    auto v = [this] (const juce::String& id) { return apvts.getRawParameterValue (id)->load(); };
    eq.steepCuts = v ("eq_steep") > 0.5f;
    for (int b = 0; b < NumEqBands; ++b)
    {
        eq.bands[b].on = v (eqId (b, "on")) > 0.5f;
        eq.bands[b].freq = v (eqId (b, "f"));
        eq.bands[b].gain = v (eqId (b, "g"));
        eq.bands[b].q = v (eqId (b, "q"));
    }
    eq.update();
    for (int b = 0; b < DynamicEq::kBands; ++b)
    {
        deq.bands[b].on = v (deId (b, "on")) > 0.5f;
        deq.bands[b].freq = v (deId (b, "f"));
        deq.bands[b].q = v (deId (b, "q"));
        deq.bands[b].thresholdDb = v (deId (b, "thr"));
        deq.bands[b].rangeDb = v (deId (b, "range"));
    }
    dyn.x1 = v ("dy_x1"); dyn.x2 = v ("dy_x2");
    for (int b = 0; b < 3; ++b)
    {
        dyn.bands[b] = { v (dyId (b, "thr")), v (dyId (b, "ratio")), v (dyId (b, "att")), v (dyId (b, "rel")), v (dyId (b, "gain")) };
        ex.amount[b] = v (exId (b, "amt"));
        ex.mode[b] = (int) v (exId (b, "mode"));
        im.width[b] = v (imId (b));
    }
    ex.x1 = v ("ex_x1"); ex.x2 = v ("ex_x2"); ex.mix = v ("ex_mix");
    im.x1 = v ("im_x1"); im.x2 = v ("im_x2"); im.stereoize = v ("im_stereoize");
    tape.drive = v ("tp_drive"); tape.speed30 = v ("tp_speed") > 0.5f; tape.bumpDb = v ("tp_bump");
    tape.warmth = v ("tp_warmth"); tape.mix = v ("tp_mix");
    mx.gainDb = v ("mx_gain"); mx.ceilingDb = v ("mx_ceil"); mx.releaseMs = v ("mx_rel"); mx.truePeak = v ("mx_tp") > 0.5f;
}

void LatheProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    bench.begin (buffer, band::WLathe, true);     // Bandleader's on/off light for this plugin
    processCore (buffer, midi);
    bench.end (buffer, true);
}

void LatheProcessor::processCore (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || n == 0) return;
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    readParams();
    if (resetLoudness.exchange (false)) { inLoud.reset(); outLoud.reset(); for (auto& e : secEnergy) e = 0; for (auto& c : secCount) c = 0; for (auto& u : uiSecLufs) u = -70.0f; }

    const float inG = dbToGain (apvts.getRawParameterValue ("in_gain")->load());
    if (std::abs (inG - 1.0f) > 1.0e-6f) buffer.applyGain (inG);

    inPeak.store (juce::jmax (buffer.getMagnitude (0, 0, n), buffer.getMagnitude (1, 0, n)));
    inLoud.process (L, R, n);

    if (assistCapture.load())
    {
        if ((int) scratch.size() < n) scratch.resize ((size_t) n);
        for (int i = 0; i < n; ++i) scratch[(size_t) i] = 0.5f * (L[i] + R[i]);
        assistFifo.push (scratch.data(), n);
    }

    for (int k = 0; k < NumModules; ++k)
    {
        const int m = order[(size_t) k].load();
        const bool on = apvts.getRawParameterValue (modOnId (m))->load() > 0.5f;
        if (m == MMax && ! on) { mx.gainDb = 0; mx.ceilingDb = 0; }    // keep the latency constant
        if (! on && m != MMax) continue;
        switch (m)
        {
            case MEq:      eq.process (L, R, n); break;
            case MDynEq:   deq.process (L, R, n); break;
            case MTape:    tape.process (L, R, n); break;
            case MDyn:     dyn.process (L, R, n); break;
            case MExciter: ex.process (L, R, n); break;
            case MImager:  im.process (L, R, n); break;
            case MMax:     mx.process (L, R, n); break;
            default: break;
        }
    }
    for (int b = 0; b < 3; ++b) dynGr[b].store (apvts.getRawParameterValue (modOnId (MDyn))->load() > 0.5f ? dyn.grMeter[b] : 0.0f);
    for (int b = 0; b < 2; ++b) dynEqCut[b].store (apvts.getRawParameterValue (modOnId (MDynEq))->load() > 0.5f ? deq.cut[b] : 0.0f);
    maxGr.store (apvts.getRawParameterValue (modOnId (MMax))->load() > 0.5f ? mx.grDb : 0.0f);

    const float outG = dbToGain (apvts.getRawParameterValue ("out_gain")->load());
    if (std::abs (outG - 1.0f) > 1.0e-6f) buffer.applyGain (outG);

    const int bits = (int) apvts.getRawParameterValue ("dither")->load();
    if (bits > 0) { dither.process (L, n, bits == 1 ? 16 : 24); dither.process (R, n, bits == 1 ? 16 : 24); }

    outLoud.process (L, R, n);
    tapeStopBlock (L, R, n);

    // the band: loudness of each section, so you can see whether the chorus really lifts
    band::markPresent (band::WLathe);
    {
        band::Leader leader;
        double ppq = 0; bool playing = false;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
            {
                playing = pos->getIsPlaying();
                if (auto p = pos->getPpqPosition()) ppq = *p;
            }
        const bool led = playing && band::readLeader (leader) && leader.playing;
        uiLinked = led;
        secClock += n;
        if (led && secClock >= (int64_t) (0.1 * sr))       // one 400 ms momentary reading every 100 ms
        {
            secClock = 0;
            const int64_t bar = band::barOfPpq (ppq, leader.meter);
            const float m = outLoud.momentary.load();
            if (m > -50.0f && ! leader.isOut (bar))
            {
                const int sec = leader.sectionAt (bar);
                secEnergy[sec] += std::pow (10.0, (m + 0.691) / 10.0);
                secCount[sec] += 1;
                uiSecLufs[sec] = (float) (-0.691 + 10.0 * std::log10 (secEnergy[sec] / secCount[sec]));
            }
        }
    }
    float pk = 0, tp = 0;
    for (int i = 0; i < n; ++i) pk = juce::jmax (pk, std::abs (L[i]), std::abs (R[i]));
    for (int i = 2; i < n - 1; ++i)
    {
        tp = juce::jmax (tp, std::abs ((-L[i - 2] + 9 * L[i - 1] + 9 * L[i] - L[i + 1]) * 0.0625f));
        tp = juce::jmax (tp, std::abs ((-R[i - 2] + 9 * R[i - 1] + 9 * R[i] - R[i + 1]) * 0.0625f));
    }
    outPeak.store (pk);
    outTruePeak.store (juce::jmax (tp, pk));

    // analysis feeds for the UI
    if ((int) scratch.size() < n * 2) scratch.resize ((size_t) n * 2);
    for (int i = 0; i < n; ++i) scratch[(size_t) i] = 0.5f * (L[i] + R[i]);
    spectrumFifo.push (scratch.data(), n);
    for (int i = 0; i < n; ++i) { scratch[(size_t) (2 * i)] = L[i]; scratch[(size_t) (2 * i + 1)] = R[i]; }
    scopeFifo.push (scratch.data(), n * 2);
}

// ---------------------------------------------------------------------------
// Assistant: listen to the song, nudge the EQ toward a typical master's tonal
// balance, then set the Maximizer to land on the chosen loudness target.
// ---------------------------------------------------------------------------
void LatheProcessor::setParam (const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

void LatheProcessor::startAssistant (int target)
{
    assistTarget = target;
    fft = std::make_unique<juce::dsp::FFT> (12);
    fftBuf.assign (8192, 0.0f);
    fftAccum.assign (2049, 0.0f);
    window.resize (4096);
    for (int i = 0; i < 4096; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * (float) i / 4095.0f);
    pullBuf.assign (1 << 16, 0.0f);
    fftFrames = 0; fillPos = 0; assistSeconds = 0; phase2Seconds = 0;
    float tmp[1024]; while (assistFifo.pull (tmp, 1024) > 0) {}
    resetLoudness = true;
    assistProgress = 0;
    assistMessage = "Listening... play the loudest part of your song.";
    assistPhase = 1;
    assistCapture = true;
}

void LatheProcessor::assistantTick()
{
    const int phase = assistPhase.load();
    if (phase == 0 || phase == 3) return;

    const int got = assistFifo.pull (pullBuf.data(), (int) pullBuf.size());
    const double secs = got / sr;

    if (phase == 1)
    {
        for (int i = 0; i < got; ++i)
        {
            fftBuf[(size_t) fillPos++] = pullBuf[(size_t) i];
            if (fillPos == 4096)
            {
                double e = 0;
                for (int k = 0; k < 4096; ++k) e += fftBuf[(size_t) k] * fftBuf[(size_t) k];
                if (e / 4096.0 > 1.0e-6)    // ignore silence
                {
                    for (int k = 0; k < 4096; ++k) fftBuf[(size_t) k] *= window[(size_t) k];
                    std::fill (fftBuf.begin() + 4096, fftBuf.end(), 0.0f);
                    fft->performFrequencyOnlyForwardTransform (fftBuf.data());
                    for (int k = 0; k <= 2048; ++k) fftAccum[(size_t) k] += fftBuf[(size_t) k] * fftBuf[(size_t) k];
                    ++fftFrames;
                    assistSeconds += 4096.0 / sr;
                }
                fillPos = 0;
            }
        }
        assistProgress = (float) juce::jmin (1.0, assistSeconds / 10.0) * 0.6f;
        if (assistSeconds < 10.0) return;

        // tonal balance vs. a typical modern master (about -4.5 dB/octave above 150 Hz)
        auto regionDb = [this] (double lo, double hi)
        {
            const double binHz = sr / 4096.0;
            double s = 0; int c = 0;
            for (int k = juce::jmax (1, (int) (lo / binHz)); k <= juce::jmin (2048, (int) (hi / binHz)); ++k) { s += fftAccum[(size_t) k]; ++c; }
            return 10.0 * std::log10 (std::max (s / juce::jmax (1, c) / juce::jmax (1, fftFrames), 1.0e-20));
        };
        auto targetDb = [] (double f) { return f > 150 ? -4.5 * std::log2 (f / 1000.0) : -4.5 * std::log2 (150.0 / 1000.0) + 1.0 * std::log2 (150.0 / f); };
        struct Reg { double lo, hi; int band; float maxDb; };
        const Reg regs[] = { { 45, 120, EqLowShelf, 3.0f }, { 180, 350, EqBell1, 2.5f }, { 550, 1200, EqBell2, 2.5f },
                             { 1800, 3500, EqBell3, 2.5f }, { 4000, 7000, EqBell4, 2.5f }, { 8000, 15000, EqHighShelf, 3.0f } };
        double dev[6], mean = 0; int nm = 0;
        for (int r = 0; r < 6; ++r)
        {
            // weight each region's binned power by the region's width so it compares to a per-octave target
            const double centre = std::sqrt (regs[r].lo * regs[r].hi);
            dev[r] = regionDb (regs[r].lo, regs[r].hi) + 10.0 * std::log10 (centre) - targetDb (centre);
            if (r >= 1 && r <= 4) { mean += dev[r]; ++nm; }
        }
        mean /= nm;
        const float eqF[6] = { 90, 250, 800, 2500, 5000, 10000 };
        for (int r = 0; r < 6; ++r)
        {
            const float g = juce::jlimit (-regs[r].maxDb, regs[r].maxDb, (float) (-(dev[r] - mean) * 0.6));
            setParam (eqId (regs[r].band, "on"), 1);
            setParam (eqId (regs[r].band, "f"), eqF[r]);
            setParam (eqId (regs[r].band, "g"), std::round (g * 10.0f) / 10.0f);
        }
        setParam (eqId (EqLowCut, "on"), 1);
        setParam (eqId (EqLowCut, "f"), 25);
        setParam (modOnId (MEq), 1);
        setParam (modOnId (MMax), 1);
        setParam ("mx_ceil", -1.0f);

        const float inL = inLoud.integrated.load();
        const float startGain = juce::jlimit (0.0f, 14.0f, targetLufs (assistTarget) - inL);
        setParam ("mx_gain", startGain);
        resetLoudness = true;
        assistPhase = 2;
        phase2Seconds = 0;
        assistMessage = "Setting loudness... keep playing.";
        return;
    }

    if (phase == 2)
    {
        // measure for 6 s, correct the Maximizer gain, repeat three times (limiting isn't linear)
        const double before = phase2Seconds;
        phase2Seconds += secs;
        assistProgress = 0.6f + 0.4f * (float) juce::jmin (1.0, phase2Seconds / 18.0);
        const int stepBefore = (int) (before / 6.0), stepNow = (int) (phase2Seconds / 6.0);
        if (stepNow > stepBefore)
        {
            const float out = outLoud.integrated.load();
            const float g = apvts.getRawParameterValue ("mx_gain")->load();
            if (out > -60.0f) setParam ("mx_gain", juce::jlimit (0.0f, 20.0f, g + (targetLufs (assistTarget) - out) * 1.1f));
            resetLoudness = true;
        }
        if (stepNow >= 3)
        {
            assistCapture = false;
            assistProgress = 1.0f;
            assistMessage = "Done. Aimed at " + juce::String (targetName (assistTarget)) + " - tweak to taste.";
            assistPhase = 3;
        }
    }
}

// ---------------------------------------------------------------------------
int LatheProcessor::getNumPrograms() { return (int) presetDefs().size(); }

const juce::String LatheProcessor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, getNumPrograms()) ? juce::String (presetDefs()[(size_t) i].name) : juce::String();
}

void LatheProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    currentProgram = index;
    for (auto* p : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
            r->setValueNotifyingHost (r->getDefaultValue());
    for (auto& kv : presetDefs()[(size_t) index].values)
        if (auto* p = apvts.getParameter (kv.first))
            p->setValueNotifyingHost (p->convertTo0to1 (kv.second));
}

void LatheProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    juce::StringArray o;
    for (auto& v : order) o.add (juce::String (v.load()));
    state.setProperty ("order", o.joinIntoString (","), nullptr);
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void LatheProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml (*xml);
    currentProgram = (int) state.getProperty ("program", 0);
    auto o = juce::StringArray::fromTokens (state.getProperty ("order").toString(), ",", "");
    if (o.size() == NumModules)
    {
        std::array<int, NumModules> arr {}; bool seen[NumModules] {}; bool ok = true;
        for (int i = 0; i < NumModules; ++i)
        {
            const int m = o[i].getIntValue();
            if (m < 0 || m >= NumModules || seen[m]) { ok = false; break; }
            seen[m] = true; arr[(size_t) i] = m;
        }
        if (ok) setOrder (arr);
    }
    apvts.replaceState (state);
}

void LatheProcessor::applyStyle (const band::StyleDef& s)
{
    setCurrentProgram (s.lathe);
}

juce::AudioProcessorEditor* LatheProcessor::createEditor() { return new LatheEditor (*this); }

#ifndef LATHE_NO_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LatheProcessor(); }
#endif

// ---------------------------------------------------------------------------
// TAPE STOP: like a hand on the reel. The whole mix slows down to a stop (at the end of the song, or
// from Bandleader's footswitch) and spins back up when the footswitch is pressed again or play restarts.
void LatheProcessor::tapeStopBlock (float* L, float* R, int n)
{
    if (tsBuf[0].empty()) return;
    const int mode = (int) apvts.getRawParameterValue ("tape_stop")->load();
    band::Leader leader; bool led = false; double ppq = 0; bool playing = false;
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) { playing = pos->getIsPlaying(); if (auto p = pos->getPpqPosition()) ppq = *p; }
    if (mode != 0) led = band::readLeader (leader);
    auto stop = [&] { if (tsState == 0) { tsState = 1; tsRead = (double) tsW; tsRate = 1.0; } };
    auto start = [&] { if (tsState == 1 || tsState == 2) { tsState = 3; tsRead = (double) tsW - 0.25 * sr; tsRate = 0.0; } };
    if (led)
    {
        if (leader.tapeStamp != tsStamp) { if (tsStampSeen) { if (tsState == 0) stop(); else start(); } tsStamp = leader.tapeStamp; }
        if (mode == 1 && playing && leader.endBar < band::kNever && ! tsEndDone)
        {
            const double bp = band::meterBarPpq (leader.meter);
            if (ppq >= (double) leader.endBar * bp + bp * 0.5) { stop(); tsEndDone = true; }
        }
    }
    tsStampSeen = true;
    if (playing && ! tsWasPlaying) { tsEndDone = false; if (tsState == 2 || tsState == 1) start(); }
    tsWasPlaying = playing;
    const int sz = (int) tsBuf[0].size();
    const double decel = 1.0 / (1.3 * sr), accel = 1.0 / (0.45 * sr);
    for (int i = 0; i < n; ++i)
    {
        tsBuf[0][(size_t) tsW] = L[i]; tsBuf[1][(size_t) tsW] = R[i];
        if (tsState != 0)
        {
            float o[2] = { 0, 0 };
            if (tsState == 1) { tsRate -= decel * (0.4 + tsRate); if (tsRate <= 0.0) { tsRate = 0.0; tsState = 2; } }
            else if (tsState == 3)
            {
                double lag = (double) tsW - tsRead; while (lag < 0) lag += sz;
                if (tsRate < 1.0) tsRate = juce::jmin (1.0, tsRate + accel * (0.3 + tsRate));
                else tsRate = juce::jmin (1.6, 1.0 + lag / (0.15 * sr));
                if (tsRate >= 1.0 && lag < 2.0) tsState = 0;
            }
            if (tsState != 2 && tsState != 0)
            {
                double rp = tsRead; while (rp < 0) rp += sz; while (rp >= sz) rp -= sz;
                const int a = (int) rp, b = (a + 1) % sz; const float f = (float) (rp - a);
                const float lvl = (float) juce::jmin (1.0, tsRate * 1.6);       // the sound sinks as it slows
                for (int c = 0; c < 2; ++c) o[c] = (tsBuf[c][(size_t) a] + (tsBuf[c][(size_t) b] - tsBuf[c][(size_t) a]) * f) * lvl;
                tsRead += tsRate;
            }
            if (tsState == 0) { o[0] = L[i]; o[1] = R[i]; }
            L[i] = o[0]; R[i] = o[1];
        }
        tsW = (tsW + 1) % sz;
    }
    uiTapeStopped = tsState == 2;
}
