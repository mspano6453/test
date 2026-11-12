#include "LoonProcessor.h"
#include "../Source/BandBus.h"
#include "LoonEditor.h"

using namespace ln;

namespace
{
juce::String pct (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
juce::String dB1 (float v, int) { return juce::String (v, 1) + " dB"; }
const juce::StringArray& onOff() { static const juce::StringArray s { "Off", "On" }; return s; }

struct Scene { const char* name; std::vector<std::pair<const char*, float>> v; };
const std::vector<Scene>& scenes()
{
    static const std::vector<Scene> s {
        { "Northwoods Night", { { "mood", MWarm }, { "weather", 0.3f }, { "crickets", 0.55f }, { "loons", 0.4f }, { "wind", 0.2f }, { "creek", 0.15f },
                                { "cloud_amt", 0.5f }, { "cloud_pitch", CPOctUp }, { "shimmer", 0.4f } } },
        { "Lake Fog",         { { "mood", MDreamy }, { "weather", 0.25f }, { "wind", 0.3f }, { "loons", 0.25f }, { "crickets", 0.0f }, { "birds", 0.0f },
                                { "cloud_amt", 0.6f }, { "cloud_pitch", CPOctaves }, { "shimmer", 0.6f }, { "bright", 0.35f }, { "wobble", 0.4f } } },
        { "Summer Storm",     { { "mood", MDark }, { "weather", 0.8f }, { "rain", 0.7f }, { "thunder", 0.6f }, { "wind", 0.6f }, { "crickets", 0.0f },
                                { "birds", 0.0f }, { "loons", 0.0f }, { "cloud_amt", 0.4f }, { "cloud_pitch", CPOctDown }, { "shimmer", 0.25f } } },
        { "Spring Morning",   { { "mood", MHopeful }, { "weather", 0.4f }, { "birds", 0.55f }, { "creek", 0.4f }, { "wind", 0.15f }, { "crickets", 0.0f },
                                { "loons", 0.0f }, { "bright", 0.7f }, { "cloud_amt", 0.4f }, { "cloud_pitch", CPFifth } } },
        { "Eerie Hollow",     { { "mood", MEerie }, { "weather", 0.35f }, { "wind", 0.5f }, { "loons", 0.3f }, { "crickets", 0.1f }, { "birds", 0.0f },
                                { "wobble", 0.6f }, { "shimmer", 0.2f }, { "bright", 0.3f }, { "cloud_amt", 0.5f }, { "cloud_pitch", CPOctDown } } },
        { "Guitar Clouds (no nature)", { { "nature_lvl", 0.0f }, { "cloud_amt", 0.75f }, { "cloud_swell", 0.85f }, { "cloud_pitch", CPOctUp },
                                { "gen_lvl", 0.5f }, { "shimmer", 0.5f }, { "gtr_space", 0.45f } } },
    };
    return s;
}
}

juce::AudioProcessorValueTreeState::ParameterLayout LoonProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> ps;
    auto F = [&] (const char* id, const char* name, float def, juce::String (*fmt) (float, int) = pct, juce::NormalisableRange<float> r = { 0, 1, 0.001f })
    {
        ps.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, r, def,
                                                                   juce::AudioParameterFloatAttributes().withStringFromValueFunction (fmt)));
    };
    auto C = [&] (const char* id, const char* name, juce::StringArray items, int def)
    { ps.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def)); };

    juce::StringArray moods; for (int m = 0; m < NumMoods; ++m) moods.add (moodName (m));
    juce::StringArray roots; for (int r = 0; r < 12; ++r) roots.add (pcName (r));
    C ("gen_on", "Generator", onOff(), 1);
    C ("mood", "Mood", moods, MWarm);
    C ("root", "Key", roots, 9);
    C ("follow", "Follow Input Key", onOff(), 1);
    C ("band_link", "Follow Bandleader", { "Off", "Key only", "Key + chords" }, 2);
    C ("journey", "Journey", { "Off", "10 min", "30 min", "60 min" }, 0);
    F ("weather", "Weather", 0.3f);
    F ("evolve", "Evolve", 0.4f);
    F ("bright", "Brightness", 0.5f);
    F ("pads", "Pads", 0.7f);
    F ("bells", "Bells", 0.45f);
    F ("glass", "Glass", 0.35f);
    F ("drone", "Drone", 0.5f);
    F ("gen_lvl", "Generator Level", 0.7f);

    F ("cloud_amt", "Cloud", 0.5f);
    F ("cloud_den", "Cloud Density", 0.5f);
    F ("cloud_size", "Grain Size", 0.5f);
    F ("cloud_spray", "Spray", 0.5f);
    F ("cloud_swell", "Swell", 0.6f);
    C ("cloud_pitch", "Cloud Pitch", { "Unison", "Octave up", "Octave down", "Fifth", "Octaves" }, CPOctUp);
    C ("freeze", "Freeze", onOff(), 0);
    F ("dry", "Guitar (dry)", 1.0f);
    F ("gtr_space", "Guitar into Space", 0.3f);

    F ("wind", "Wind", 0.2f); F ("rain", "Rain", 0.0f); F ("creek", "Creek", 0.0f); F ("crickets", "Crickets", 0.3f);
    F ("birds", "Birds", 0.0f); F ("loons", "Loons", 0.2f); F ("thunder", "Thunder", 0.0f); F ("nature_lvl", "Nature Level", 0.7f);

    F ("v_size", "Space Size", 0.7f); F ("v_mix", "Space", 0.6f); F ("shimmer", "Shimmer", 0.35f); F ("wobble", "Tape Wobble", 0.2f);
    F ("out", "Output", -3.0f, dB1, { -24, 6, 0.1f });
    ps.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "take_style", 1 }, "Take Style From Bandleader", juce::StringArray { "Off", "On" }, 1));
    return { ps.begin(), ps.end() };
}

LoonProcessor::LoonProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Loon", createLayout())
{
    band::available();   // open the band link here (message thread), not on the audio thread
    gen.midiOut = [this] (int note, float vel, bool)
    {
        if (pendingCount + 2 > 128) return;
        pending[pendingCount++] = { sampleClock, note, true, vel };
        pending[pendingCount++] = { sampleClock + (int64_t) (sr * 1.5), note, false, 0 };
        uiLastNote = note;
        uiNoteCounter.fetch_add (1);
    };
}

bool LoonProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet(), in = l.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo()) return false;
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono() || in.isDisabled();
}

void LoonProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    key.prepare (sr); gen.prepare (sr); cloud.prepare (sr); nature.prepare (sr); space.prepare (sr);
    for (auto* b : { &mono, &gL, &gR, &sL, &sR }) b->assign ((size_t) juce::jmax (512, samplesPerBlock), 0.0f);
    pendingCount = 0;
}

void LoonProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if ((int) mono.size() < n) for (auto* b : { &mono, &gL, &gR, &sL, &sR }) b->resize ((size_t) n);
    midi.clear();

    const int inCh = getTotalNumInputChannels();
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    if (inCh == 0) { buffer.clear(); }
    else if (inCh == 1) std::copy (L, L + n, R);
    for (int i = 0; i < n; ++i) mono[(size_t) i] = 0.5f * (L[i] + R[i]);

    // ---- listen
    const bool follow = v ("follow") > 0.5f;
    key.push (mono.data(), n);
    uiInput = key.inputLevel.load();
    const bool hearing = key.hasLocked() && key.inputLevel.load() > 0.003f && key.confidence.load() > 0.25f;
    if (follow && hearing) { followedRoot = key.root.load(); followedMinor = key.minor.load(); }

    // ---- journey: a slow arc of brightness, density and key over 10-60 minutes
    const int j = (int) v ("journey");
    const double jLen = j == 1 ? 600.0 : j == 2 ? 1800.0 : j == 3 ? 3600.0 : 0.0;
    double jPh = 0;
    if (jLen > 0) { journeyTime += n / sr; jPh = std::fmod (journeyTime, jLen) / jLen; }
    uiJourney = (float) jPh;
    const float jMod = jLen > 0 ? (float) std::sin (kTwoPi * jPh) : 0.0f;

    // ---- the band: a Bandleader in the set gives the key (and the chord of the moment)
    band::markPresent (band::WLoon);
    band::Leader leader;
    const int linkMode = (int) v ("band_link");
    bool led = linkMode > 0 && band::readLeader (leader);
    const bool bandWaiting = led && ! leader.playing;   // a Bandleader runs the show: wait for play
    double ppq = 0, bpm = 120; bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            if (auto p = pos->getPpqPosition()) ppq = *p;
            if (auto b = pos->getBpm()) bpm = *b;
        }
    led = led && playing && leader.playing;
    uiLinked = led;
    int bandRoot = 9; bool bandMinor = true; float sectionLift = 0, dropLift = 0;
    const int64_t bar = (int64_t) std::floor (ppq / 4.0);
    if (led)
    {
        // the song's key: the tonic chord (offset 0) tells major from minor
        bandRoot = leader.key; bandMinor = true;
        for (int s = 0; s < band::NumSec; ++s)
            for (int i = 0; i < leader.progLen[s] && i < band::kMaxProg; ++i)
                if (leader.progOff[s][i] == 0) bandMinor = band::qualityIsMinor (leader.progMinor[s][i]);
        if (linkMode == 2) { int r; bool m; leader.chordAt (bar, r, m); bandRoot = r; bandMinor = m; }
        const int sec = leader.sectionAt (bar);
        sectionLift = sec == band::Chorus ? 1.0f : sec == band::Bridge ? -0.6f : 0.0f;
        dropLift = leader.isOut (bar) ? 1.0f : 0.0f;          // the band stops: Loon fills the space
    }
    const double sm = 1.0 - std::exp (-(double) n / (2.0 * sr));   // ~2 s glide
    liftS += (float) ((sectionLift - liftS) * sm);
    waitGain += (float) (((bandWaiting ? 0.0f : 1.0f) - waitGain) * (1.0 - std::exp (-(double) n / (0.8 * sr))));
    dropS += (float) ((dropLift - dropS) * sm);

    Generator::Params gp;
    gp.on = v ("gen_on") > 0.5f;
    gp.mood = (int) v ("mood");
    const bool useFollow = follow && (hearing || uiFollowing.load());
    gp.root = useFollow ? followedRoot : (int) v ("root");
    gp.minor = useFollow ? followedMinor != 0 : (gp.mood == MDark || gp.mood == MEerie);
    if (jLen > 0 && ! useFollow) gp.root = (gp.root + 5 * (int) (jPh * 3.0)) % 12;   // moves round the circle of fourths
    gp.weather = juce::jlimit (0.0f, 1.0f, v ("weather") + 0.2f * jMod);
    gp.evolve = v ("evolve");
    gp.brightness = juce::jlimit (0.0f, 1.0f, v ("bright") + 0.25f * jMod);
    gp.padLvl = v ("pads"); gp.bellLvl = v ("bells"); gp.glassLvl = v ("glass"); gp.droneLvl = v ("drone");
    if (led) { gp.root = bandRoot; gp.minor = bandMinor; }
    // choruses swell and brighten, bridges thin out and darken
    gp.brightness = juce::jlimit (0.0f, 1.0f, gp.brightness + 0.15f * liftS + 0.2f * dropS);
    gp.weather = juce::jlimit (0.0f, 1.0f, gp.weather + 0.1f * dropS);
    uiRoot = gp.root; uiMinor = gp.minor ? 1 : 0; uiFollowing = useFollow && ! led;

    // ---- generate
    std::fill (gL.begin(), gL.begin() + n, 0.0f); std::fill (gR.begin(), gR.begin() + n, 0.0f);
    gen.render (gL.data(), gR.data(), n, gp, v ("gen_lvl") * (1.0f + 0.25f * liftS + 0.35f * dropS) * waitGain);

    // dip under Tea Towel's kick (no sidechain routing needed)
    if (led)
    {
        const double ppqPerSample = bpm / 60.0 / sr;
        const float rel = (float) std::exp (-1.0 / (0.16 * sr));
        for (int i = 0; i < n; ++i)
        {
            const double q = ppq + i * ppqPerSample;
            const int64_t k = (int64_t) std::floor (q * 4.0);
            if (k != lastDuckStep)
            {
                lastDuckStep = k;
                const int64_t b = (k - (((k % 16) + 16) % 16)) / 16;
                uint16_t kicks = 0;
                if (band::readKicks (b, kicks) && ((kicks >> (int) (((k % 16) + 16) % 16)) & 1u)) duck = 1.0f;
            }
            const float g = 1.0f - 0.33f * duck;      // about -3.5 dB at the hit
            gL[(size_t) i] *= g; gR[(size_t) i] *= g;
            duck *= rel;
        }
    }

    Cloud::Params cp;
    cp.amount = v ("cloud_amt"); cp.density = v ("cloud_den"); cp.size = v ("cloud_size"); cp.spray = v ("cloud_spray");
    cp.swell = v ("cloud_swell"); cp.pitch = (int) v ("cloud_pitch"); cp.freeze = v ("freeze") > 0.5f;
    cloud.process (mono.data(), gL.data(), gR.data(), n, cp);

    // ---- guitar: dry, plus a send into the space
    const float dry = v ("dry"), send = v ("gtr_space");
    for (int i = 0; i < n; ++i) { sL[(size_t) i] = L[i] * send; sR[(size_t) i] = R[i] * send; L[i] *= dry; R[i] *= dry; }

    space.process (gL.data(), gR.data(), sL.data(), sR.data(), L, R, n, v ("v_size"), v ("v_mix"), v ("shimmer"), v ("wobble"));

    Nature::Params np;
    const float w = gp.weather;
    np.wind = v ("wind") * (0.4f + w); np.rain = v ("rain") * (0.3f + w); np.creek = v ("creek");
    np.crickets = v ("crickets"); np.birds = v ("birds"); np.loons = v ("loons"); np.thunder = v ("thunder") * w;
    nature.process (L, R, n, np, v ("nature_lvl") * waitGain);

    // ---- output + gentle safety limiter
    const float g = juce::Decibels::decibelsToGain (v ("out"));
    for (int c = 0; c < 2; ++c)
    {
        float* x = c == 0 ? L : R;
        for (int i = 0; i < n; ++i)
        {
            float y = x[i] * g;
            const float a = std::abs (y);
            if (a > 0.8f) y = std::copysign (0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f), y);
            x[i] = y;
        }
    }

    // ---- MIDI out: the notes it played
    int wpos = 0;
    for (int i = 0; i < pendingCount; ++i)
    {
        const auto& p = pending[i];
        if (p.t < sampleClock + n)
        {
            const int off = (int) juce::jlimit<int64_t> (0, n - 1, p.t - sampleClock);
            midi.addEvent (p.on ? juce::MidiMessage::noteOn (1, p.note, juce::jlimit (0.05f, 1.0f, p.vel))
                                : juce::MidiMessage::noteOff (1, p.note), off);
        }
        else pending[wpos++] = p;
    }
    pendingCount = wpos;
    capture.recordWithHost (midi, getPlayHead(), getSampleRate(), n);
    sampleClock += n;
}

// ---------------------------------------------------------------------------
int LoonProcessor::getNumPrograms() { return (int) scenes().size(); }
const juce::String LoonProcessor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, getNumPrograms()) ? juce::String (scenes()[(size_t) i].name) : juce::String();
}
void LoonProcessor::setCurrentProgram (int i)
{
    if (! juce::isPositiveAndBelow (i, getNumPrograms())) return;
    currentProgram = i;
    static const char* keep[] = { "follow", "root", "dry", "out", "freeze", "gen_on" };
    for (auto* p : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            bool skip = false;
            for (auto* k : keep) if (r->getParameterID() == k) skip = true;
            if (! skip) r->setValueNotifyingHost (r->getDefaultValue());
        }
    for (auto& kv : scenes()[(size_t) i].v)
        if (auto* p = apvts.getParameter (kv.first)) p->setValueNotifyingHost (p->convertTo0to1 (kv.second));
}

void LoonProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto st = apvts.copyState();
    st.setProperty ("program", currentProgram, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}
void LoonProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto st = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) st.getProperty ("program", 0);
            apvts.replaceState (st);
        }
}

void LoonProcessor::applyStyle (const band::StyleDef& s)
{
    band::setParam (apvts, "gen_on", s.lGen > 0.01f ? 1.0f : 0.0f);
    band::setParam (apvts, "gen_lvl", s.lGen);
    band::setParam (apvts, "mood", (float) s.lMood);
    band::setParam (apvts, "pads", s.lPads);
    band::setParam (apvts, "v_mix", s.lSpace);
    band::setParam (apvts, "v_size", s.lSize);
    band::setParam (apvts, "shimmer", s.lShimmer);
    band::setParam (apvts, "cloud_amt", s.lCloud);
    band::setParam (apvts, "wobble", s.lWobble);
    band::setParam (apvts, "gtr_space", s.lGtr);
    band::setParam (apvts, "dry", 1.0f);
    band::setParam (apvts, "nature_lvl", 0.0f);     // no crickets in the rock band
}

juce::AudioProcessorEditor* LoonProcessor::createEditor() { return new LoonEditor (*this); }

#ifndef LOON_NO_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LoonProcessor(); }
#endif
