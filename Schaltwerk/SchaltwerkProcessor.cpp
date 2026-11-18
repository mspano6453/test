#include "../Source/SongForm.h"
#include "SchaltwerkProcessor.h"
#include "SchaltwerkEditor.h"
#include "../Source/BandBus.h"

using namespace sw;

namespace
{
juce::String pct (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
juce::String dB1 (float v, int) { return juce::String (v, 1) + " dB"; }
juce::String st (float v, int)  { return (v > 0 ? "+" : "") + juce::String (v, 1) + " st"; }
juce::String hz (float v, int)  { return v >= 1000 ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; }
juce::String sec (float v, int) { return v < 1 ? juce::String (juce::roundToInt (v * 1000)) + " ms" : juce::String (v, 2) + " s"; }
juce::String mult (float v, int) { return juce::String (v, 2) + "x"; }
juce::String cents (float v, int) { return juce::String (v, 1) + " ct"; }
juce::String panT (float v, int) { return v < -0.005f ? juce::String (juce::roundToInt (-v * 100)) + "L" : v > 0.005f ? juce::String (juce::roundToInt (v * 100)) + "R" : "C"; }

const juce::StringArray& onOff() { static const juce::StringArray s { "Off", "On" }; return s; }
}

const std::vector<Pattern>& sw::patterns()
{
    //                    Kick                 Clank               Snare               Clap                Zap                 TomLo               Hat                 TomHi               Clave               Cowbell             OpenHat             Steam
    static const std::vector<Pattern> p {
        { "Robot March",   { "x...x...x...x...", "......x.......x.", "....X.......X...", "................", "................", "................", "x.x.x.x.x.x.x.x.", "................", "................", "................", "................", "................" } },
        { "Motorway",      { "x.......x.x.....", "................", "....X.......X...", "................", "................", "................", "XxxxXxxxXxxxXxxx", "................", "..x...x...x...x.", "................", "................", "................" } },
        { "Steel Train",   { "x..x..x.x..x..x.", "X...X...X...X...", "....x.......x...", "................", "................", "................", "..x...x...x...x.", "................", "................", "................", "................", "x..............." } },
        { "Metal Factory", { "x...x...x...x...", "..X...X...X...X.", "....X.......X...", "................", "..............x.", "................", "................", "................", "................", "x..x..x..x..x.x.", "................", "................" } },
        { "Pocket Beat",   { "x.....x...x.....", "................", "....X..x....X...", "................", "X...............", "................", "x.x.x.x.x.x.x.x.", "................", ".x..x..x..x...x.", "................", "................", "................" } },
        { "Radio Pulse",   { "x...x...x...x...", "x.......x.......", "........X.......", "................", "................", "................", "................", "................", "................", "................", "..x...x...x...x.", "x..............." } },
        { "Tom Machine",   { "x.......x.......", "................", "....X.......X...", "................", "................", "..x..x....x..x..", "x.x.x.x.x.x.x.x.", "x..x..x.x..x..x.", "................", "................", "................", "................" } },
        { "Clap Parade",   { "x...x...x...x...", "................", "................", "....X.......X..x", "................", "................", "..x...x...x...x.", "................", "................", "................", "................", "................" } },
        { "Empty",         { "................", "................", "................", "................", "................", "................", "................", "................", "................", "................", "................", "................" } },
    };
    return p;
}

juce::AudioProcessorValueTreeState::ParameterLayout SchaltwerkProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> ps;
    auto F = [&] (const juce::String& id, const juce::String& name, juce::NormalisableRange<float> r, float def, juce::String (*fmt) (float, int))
    {
        ps.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, r, def,
                                                                   juce::AudioParameterFloatAttributes().withStringFromValueFunction (fmt)));
    };
    auto C = [&] (const juce::String& id, const juce::String& name, juce::StringArray items, int def)
    { ps.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def)); };

    const float panDef[NumDrums] = { 0, 0.3f, 0, -0.2f, 0.4f, -0.35f, -0.3f, 0.35f, 0.5f, -0.5f, -0.3f, 0.2f };
    for (int d = 0; d < NumDrums; ++d)
    {
        const juce::String nm = drumName (d);
        F (dId (d, "tune"), nm + " Tune", { -12, 12, 0.1f }, 0, st);
        F (dId (d, "dec"), nm + " Decay", juce::NormalisableRange<float> (0.25f, 3.0f, 0.01f, 0.6f), 1, mult);
        F (dId (d, "tone"), nm + " Tone", { 0, 1, 0.001f }, 0.5f, pct);
        F (dId (d, "lvl"), nm + " Level", { -36, 6, 0.1f }, d == DKick ? 0.0f : -3.0f, dB1);
        F (dId (d, "pan"), nm + " Pan", { -1, 1, 0.01f }, panDef[d], panT);
    }
    C ("seq_on", "Sequencer", onOff(), 1);
    C ("band_link", "Follow Bandleader", onOff(), 1);
    F ("seq_swing", "Swing", { 0, 1, 0.001f }, 0, pct);
    F ("drum_send", "Drums Echo Send", { 0, 1, 0.001f }, 0.1f, pct);

    C ("sy_mode", "Robot Mode", { "Synth", "Vowel Choir", "Vocoder" }, 0);
    F ("sy_wave", "Robot Wave (Saw-Pulse)", { 0, 1, 0.001f }, 0.3f, pct);
    F ("sy_pw", "Robot Pulse Width", { 0.05f, 0.5f, 0.001f }, 0.35f, pct);
    F ("sy_det", "Robot Detune", { 0, 30, 0.1f }, 7, cents);
    juce::NormalisableRange<float> cut (60, 16000, 1); cut.setSkewForCentre (1200);
    F ("sy_cut", "Robot Cutoff", cut, 1800, hz);
    F ("sy_res", "Robot Resonance", { 0, 1, 0.001f }, 0.35f, pct);
    F ("sy_env", "Robot Filter Env", { 0, 1, 0.001f }, 0.45f, pct);
    juce::NormalisableRange<float> tm (0.001f, 4.0f, 0.001f); tm.setSkewForCentre (0.3f);
    F ("sy_a", "Robot Attack", tm, 0.01f, sec);
    F ("sy_d", "Robot Decay", tm, 0.35f, sec);
    F ("sy_s", "Robot Sustain", { 0, 1, 0.001f }, 0.7f, pct);
    F ("sy_r", "Robot Release", tm, 0.3f, sec);
    juce::NormalisableRange<float> gl (0, 1, 0.001f); gl.setSkewForCentre (0.15f);
    F ("sy_glide", "Robot Glide (0 = poly)", gl, 0, sec);
    F ("sy_vowel", "Robot Vowel", { 0, 1, 0.001f }, 0.25f, pct);
    F ("sy_shift", "Robot Formant Shift", { 0.6f, 1.6f, 0.001f }, 1, mult);
    F ("sy_sib", "Robot Sibilance", { 0, 1, 0.001f }, 0.3f, pct);
    F ("sy_lvl", "Robot Level", { -36, 6, 0.1f }, -4, dB1);
    F ("sy_send", "Robot Echo Send", { 0, 1, 0.001f }, 0.25f, pct);

    juce::StringArray roots; for (int i = 0; i < 12; ++i) roots.add (noteName (i));
    C ("om_root", "Strumchord Root", roots, 0);
    C ("om_type", "Strumchord Chord", { "Major", "Minor", "7th" }, 0);
    C ("om_hold", "Strumchord Hold", onOff(), 0);
    F ("om_chord", "Strumchord Chord Level", { 0, 1, 0.001f }, 0.5f, pct);
    F ("om_tone", "Strumchord Chord Tone", { 0, 1, 0.001f }, 0.4f, pct);
    F ("om_bass", "Strumchord Bass", { 0, 1, 0.001f }, 0.5f, pct);
    F ("om_harp", "Strumchord Harp Level", { 0, 1, 0.001f }, 0.7f, pct);
    F ("om_decay", "Strumchord Harp Decay", { 0, 1, 0.001f }, 0.45f, pct);
    F ("om_vib", "Strumchord Vibrato", { 0, 1, 0.001f }, 0.3f, pct);
    C ("om_oct", "Strumchord Harp Octave", { "-1", "0", "+1" }, 1);
    C ("om_auto", "Strumchord Auto Strum", { "Off", "1/4", "1/8", "1/16" }, 0);
    F ("om_send", "Strumchord Echo Send", { 0, 1, 0.001f }, 0.3f, pct);

    C ("ec_time", "Echo Time", { "1/4", "1/8 dotted", "1/8", "1/16" }, 1);
    F ("ec_fb", "Echo Feedback", { 0, 0.9f, 0.001f }, 0.4f, pct);
    F ("ec_mix", "Echo Level", { 0, 1, 0.001f }, 0.6f, pct);
    F ("out", "Output", { -24, 6, 0.1f }, -3, dB1);
    ps.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "take_style", 1 }, "Take Style From Bandleader", juce::StringArray { "Off", "On" }, 1));
    return { ps.begin(), ps.end() };
}

SchaltwerkProcessor::SchaltwerkProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                                       .withInput ("Sidechain (vocoder)", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Schaltwerk", createLayout())
{
    band::available();   // open the band link here (message thread), not on the audio thread
    loadPattern (0);
    renderer = std::make_unique<mc::SongRenderer> (*this, band::WSchaltwerk, mc::SongRenderer::partFileName (2, "Drum Box"),
                                                   [] { return std::make_unique<SchaltwerkProcessor>(); },
                                                   [] (juce::AudioProcessor& p) { mc::setChoice (p, "band_link", 1); });
}

bool SchaltwerkProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    const auto in = l.getMainInputChannelSet();
    return in.isDisabled() || in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void SchaltwerkProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    capOut.ensureSize (2048);
    sr = sampleRate;
    drums.prepare (sr); synth.prepare (sr); omni.prepare (sr); echo.prepare (sr);
    const size_t n = (size_t) juce::jmax (512, samplesPerBlock);
    for (auto* v : { &sideL, &sideR, &sendL, &sendR, &scratch }) v->assign (n, 0.0f);
    keyboardState.reset();
    lastStepIndex = -1;
}

// ---------------------------------------------------------------------------
void SchaltwerkProcessor::loadPattern (int index)
{
    const auto& p = patterns()[(size_t) juce::jlimit (0, (int) patterns().size() - 1, index)];
    for (int d = 0; d < NumDrums; ++d)
        for (int s = 0; s < 16; ++s)
        {
            const char c = p.rows[d][s];
            steps[d][s].store ((uint8_t) (c == 'X' ? 2 : c == 'x' ? 1 : 0));
        }
}

void SchaltwerkProcessor::clearPattern()
{
    for (auto& row : steps) for (auto& c : row) c.store (0);
}

juce::String SchaltwerkProcessor::patternToString() const
{
    juce::String s;
    for (auto& row : steps) for (auto& c : row) s << juce::String ((int) c.load());
    return s;
}

void SchaltwerkProcessor::patternFromString (const juce::String& s)
{
    if (s.length() != NumDrums * 16) return;
    for (int i = 0; i < NumDrums * 16; ++i)
        steps[i / 16][i % 16].store ((uint8_t) juce::jlimit (0, 2, (int) (s[i] - '0')));
}

void SchaltwerkProcessor::setCurrentProgram (int i)
{
    if (! juce::isPositiveAndBelow (i, getNumPrograms())) return;
    currentProgram = i;
    loadPattern (i);
}

const juce::String SchaltwerkProcessor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, getNumPrograms()) ? juce::String (patterns()[(size_t) i].name) : juce::String();
}

void SchaltwerkProcessor::pressChord (int root, int type)
{
    auto set = [this] (const char* id, float v)
    {
        if (auto* p = apvts.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (v)); p->endChangeGesture(); }
    };
    set ("om_root", (float) root);
    set ("om_type", (float) type);
    uiChordGate = 1;
}
void SchaltwerkProcessor::releaseChord() { uiChordGate = 0; }

// ---------------------------------------------------------------------------
void SchaltwerkProcessor::readParams()
{
    auto v = [this] (const juce::String& id) { return apvts.getRawParameterValue (id)->load(); };
    for (int d = 0; d < NumDrums; ++d)
    {
        auto& s = drumSet[d];
        s.tune = std::pow (2.0f, v (dId (d, "tune")) / 12.0f);
        s.decay = v (dId (d, "dec"));
        s.tone = v (dId (d, "tone"));
        s.gain = juce::Decibels::decibelsToGain (v (dId (d, "lvl")), -35.9f);
        const float a = (v (dId (d, "pan")) + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        s.panL = std::cos (a) * 1.4142f; s.panR = std::sin (a) * 1.4142f;
    }
    drumSend = v ("drum_send");
    synthP.mode = (int) v ("sy_mode");
    synthP.wave = v ("sy_wave"); synthP.pw = v ("sy_pw"); synthP.detune = v ("sy_det");
    synthP.cutoff = v ("sy_cut"); synthP.reso = v ("sy_res"); synthP.envAmt = v ("sy_env");
    synthP.a = v ("sy_a"); synthP.d = v ("sy_d"); synthP.s = v ("sy_s"); synthP.r = v ("sy_r");
    synthP.glide = v ("sy_glide"); synthP.vowel = v ("sy_vowel"); synthP.formantShift = v ("sy_shift"); synthP.sibilance = v ("sy_sib");
    synthLevel = juce::Decibels::decibelsToGain (v ("sy_lvl"), -35.9f);
    synthSend = v ("sy_send");
    omniP.chordLevel = v ("om_chord"); omniP.chordTone = v ("om_tone"); omniP.bassLevel = v ("om_bass");
    omniP.harpLevel = v ("om_harp"); omniP.harpDecay = v ("om_decay"); omniP.vibrato = v ("om_vib");
    omniP.harpOctave = (int) v ("om_oct") - 1;
    omniP.hold = v ("om_hold") > 0.5f;
    omniSend = v ("om_send");
}

void SchaltwerkProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
    {
        const int n = m.getNoteNumber();
        const float vel = m.getFloatVelocity();
        if (n >= kDrumBaseNote && n < kDrumBaseNote + NumDrums)
        {
            const int d = n - kDrumBaseNote;
            drums.trigger (d, vel, drumSet[d]);
            drumFlash[d].store (vel);
        }
        else if (n >= kSynthLowNote) synth.noteOn (n, vel, synthP);
        else if (n < kDrumBaseNote)
        {
            const int type = n >= 24 ? ChMajor : n >= 12 ? ChMinor : ChSeventh;
            omni.setChord (n % 12, type);
            midiChordKey = n;
        }
    }
    else if (m.isNoteOff())
    {
        const int n = m.getNoteNumber();
        if (n >= kSynthLowNote) synth.noteOff (n, synthP);
        else if (n < kDrumBaseNote && n == midiChordKey) { omni.releaseChord (omniP.hold || uiChordGate.load() != 0); midiChordKey = -1; }
    }
    else if (m.isController() && m.getControllerNumber() == 1)
        omni.strumTo ((int) std::lround (m.getControllerValue() / 127.0 * (kStrings - 1)), 0.85f, omniP);
    else if (m.isController() && m.getControllerNumber() == 119 && m.getChannel() == 16)   // internal: auto-strum tick
    {
        omni.pluck (autoString, 0.75f, omniP);
        autoString = (autoString + 1) % 8;
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff()) synth.allOff();
}

void SchaltwerkProcessor::runSequencer (juce::MidiBuffer& midi, int n)
{
    bool playing = false; double ppq = 0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            if (auto p = pos->getPpqPosition()) ppq = *p;
            if (auto b = pos->getBpm()) bpm = *b;
        }
    const bool seqOn = apvts.getRawParameterValue ("seq_on")->load() > 0.5f;
    const int autoRate = (int) apvts.getRawParameterValue ("om_auto")->load();   // 0 off, 1 = 1/4, 2 = 1/8, 3 = 1/16
    // the band: the Strumchord holds the song's chords, the drum box drops out with everyone
    band::markPresent (band::WSchaltwerk);
    band::Leader leader;
    const bool led = playing && apvts.getRawParameterValue ("band_link")->load() > 0.5f && band::readLeader (leader) && leader.playing;
    uiLinked = led;
    if (! led && bandChord) { omni.releaseChord (false); bandChord = false; }
    if (! playing) { currentStep = -1; lastStepIndex = -1; return; }

    const double ppqPerSample = bpm / 60.0 / sr;
    const double ppqEnd = ppq + ppqPerSample * n;
    const float swing = apvts.getRawParameterValue ("seq_swing")->load();
    const int meter = led ? leader.meter : band::hostMeter (getPlayHead());
    const int ns = band::meterSteps (meter);      // 16 steps, or the first 12 in 3/4 and 6/8
    uiSteps = ns;
    capture.setMeter (meter);
    for (int64_t k = (int64_t) std::ceil (ppq * 4.0 - 1e-9); (double) k * 0.25 < ppqEnd; ++k)
    {
        const int step = (int) (((k % ns) + ns) % ns);
        double t = ((double) k * 0.25 - ppq) / ppqPerSample;
        if (step % 2 == 1 && meter != band::M68) t += swing * 0.333 * (0.25 / ppqPerSample);
        const int off = juce::jlimit (0, n - 1, (int) t);
        currentStep = band::oddSrc (meter, step) & 15;
        bool out = false;
        if (led)
        {
            const int64_t bar = (k - step) / ns;
            out = leader.rests (bar, true) || leader.isEnding (bar);     // the drum box: like the drummer
            const bool chordOut = leader.rests (bar, false);              // the chords: in from the intro, held through the ending
            int r; bool mn; leader.chordAt (bar, r, mn);
            if (chordOut) { if (bandChord) { omni.releaseChord (false); bandChord = false; } }
            else if (! bandChord || omni.root() != r || omni.type() != (mn ? 1 : 0)) { omni.setChord (r, mn ? 1 : 0); bandChord = true; }
        }
        if (seqOn && ! out)
            for (int d = 0; d < NumDrums; ++d)
            {
                const int c = steps[d][band::oddSrc (meter, step) & 15].load();
                if (c > 0) midi.addEvent (juce::MidiMessage::noteOn (10, kDrumBaseNote + d, c == 2 ? 1.0f : 0.72f), off);
            }
        const int every = autoRate == 1 ? 4 : autoRate == 2 ? 2 : autoRate == 3 ? 1 : 0;
        if (every > 0 && (k % every) == 0 && omni.isChordOn())
            midi.addEvent (juce::MidiMessage::controllerEvent (16, 119, 0), off);
        lastStepIndex = k;
    }
}

void SchaltwerkProcessor::renderSegment (float* L, float* R, int start, int end)
{
    if (end <= start) return;
    const int n = end - start;
    drums.render (L + start, R + start, sendL.data() + start, sendR.data() + start, n, drumSend);
    synth.render (L + start, R + start, sendL.data() + start, sendR.data() + start,
                  haveSide ? sideL.data() + start : nullptr, haveSide ? sideR.data() + start : nullptr,
                  n, synthP, synthLevel, synthSend, scratch.data());
    omni.render (L + start, R + start, sendL.data() + start, sendR.data() + start, n, omniP, omniSend);
}

void SchaltwerkProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    bench.begin (buffer, band::WSchaltwerk, false);     // Bandleader's on/off light for this plugin
    processCore (buffer, midi);
    bench.end (buffer, false);
}

void SchaltwerkProcessor::processCore (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if ((int) sendL.size() < n) for (auto* v : { &sideL, &sideR, &sendL, &sendR, &scratch }) v->resize ((size_t) n);

    // grab the vocoder sidechain before the buffer is reused for output
    haveSide = false;
    auto side = getBusBuffer (buffer, true, 0);
    if (side.getNumChannels() > 0)
    {
        std::copy (side.getReadPointer (0), side.getReadPointer (0) + n, sideL.begin());
        const float* r = side.getReadPointer (side.getNumChannels() > 1 ? 1 : 0);
        std::copy (r, r + n, sideR.begin());
        haveSide = true;
    }
    buffer.clear();
    std::fill (sendL.begin(), sendL.begin() + n, 0.0f);
    std::fill (sendR.begin(), sendR.begin() + n, 0.0f);

    readParams();
    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    runSequencer (midi, n);

    // UI chord buttons / automated chord root & type
    const int root = (int) apvts.getRawParameterValue ("om_root")->load();
    const int type = (int) apvts.getRawParameterValue ("om_type")->load();
    const int gate = uiChordGate.load();
    if (gate && (! omni.isChordOn() || omni.root() != root || omni.type() != type)) omni.setChord (root, type);
    if (! gate && lastUiGate && midiChordKey < 0) omni.releaseChord (omniP.hold);
    lastUiGate = gate;

    // UI strumplate
    const int us = uiStrum.load();
    if (us != lastUiStrum)
    {
        if (us >= 0) omni.strumTo (us, 0.85f, omniP); else omni.strumRelease();
        lastUiStrum = us;
    }

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    int pos = 0;
    for (const auto meta : midi)
    {
        const int ev = juce::jlimit (0, n, meta.samplePosition);
        renderSegment (L, R, pos, ev);
        handleMidi (meta.getMessage());
        pos = ev;
    }
    renderSegment (L, R, pos, n);

    const int et = (int) apvts.getRawParameterValue ("ec_time")->load();
    static const double beats[] = { 1.0, 0.75, 0.5, 0.25 };
    echo.process (L, R, sendL.data(), sendR.data(), n, beats[et] * 60.0 / juce::jmax (30.0, bpm),
                  apvts.getRawParameterValue ("ec_fb")->load(), apvts.getRawParameterValue ("ec_mix")->load());

    buffer.applyGain (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("out")->load()));
    // gentle safety limiter: transparent below -2 dBFS, rounds off anything hotter
    for (int c = 0; c < 2; ++c)
    {
        float* x = buffer.getWritePointer (c);
        for (int i = 0; i < n; ++i)
        {
            const float a = std::abs (x[i]);
            if (a > 0.8f) x[i] = std::copysign (0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f), x[i]);
        }
    }
    lastString = omni.lastPlucked;
    chordSounding = omni.isChordOn();
    uiOmniRoot = omni.root(); uiOmniType = omni.type();
    // MIDI out: the drum box's hits (channel 10), so the pattern can be dragged or recorded into a clip
    capOut.clear();
    capNotes.tick (capOut, 0, sampleClock);
    for (const auto meta : midi)
    {
        const auto& msg = meta.getMessage();
        if (msg.isNoteOn() && msg.getChannel() == 10)
            capNotes.on (capOut, meta.samplePosition, sampleClock + meta.samplePosition, msg.getNoteNumber(), msg.getFloatVelocity(), 0.1, getSampleRate(), 10);
    }
    midi.swapWith (capOut);
    capture.recordWithHost (midi, getPlayHead(), getSampleRate(), n);
    sampleClock += n;
}

// ---------------------------------------------------------------------------
void SchaltwerkProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("pattern", patternToString(), nullptr);
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void SchaltwerkProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml (*xml);
    currentProgram = (int) state.getProperty ("program", 0);
    patternFromString (state.getProperty ("pattern").toString());
    apvts.replaceState (state);
}

void SchaltwerkProcessor::applyStyle (const band::StyleDef& s)
{
    // 0 sits out, 1 Strumchord pad, 2 Strumchord strums, 3 drum machine + pad
    const bool drums = s.sRole == 3;
    if (drums && s.sPattern >= 0) loadPattern (s.sPattern);
    band::setParam (apvts, "seq_on", drums ? 1.0f : 0.0f);
    band::setParam (apvts, "om_chord", s.sRole == 1 ? 0.45f : s.sRole == 2 ? 0.15f : s.sRole == 3 ? 0.35f : 0.0f);
    band::setParam (apvts, "om_harp", s.sRole == 2 ? 0.55f : 0.0f);
    band::setParam (apvts, "om_bass", s.sRole == 3 ? 0.3f : 0.0f);
    band::setParam (apvts, "om_auto", s.sRole == 2 ? 2.0f : 0.0f);
}

juce::AudioProcessorEditor* SchaltwerkProcessor::createEditor() { return new SchaltwerkEditor (*this); }

#ifndef SCHALTWERK_NO_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SchaltwerkProcessor(); }
#endif
