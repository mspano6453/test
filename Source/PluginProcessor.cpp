#include "BandBus.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

using namespace tt;

namespace
{
juce::String pctText (float v, int)  { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
juce::String dbText (float v, int)   { return juce::String (v, 1) + " dB"; }
juce::String stText (float v, int)   { return (v > 0 ? "+" : "") + juce::String (v, 1) + " st"; }
juce::String panText (float v, int)
{
    if (v < -0.005f) return juce::String (juce::roundToInt (-v * 100.0f)) + "L";
    if (v >  0.005f) return juce::String (juce::roundToInt ( v * 100.0f)) + "R";
    return "C";
}

struct Hit { int piece; int artic; float noteMul; };

bool mapNote (int note, Hit& h)
{
    switch (note)
    {
        case 35: case 36:             h = { Kick, Normal, 1.0f };  return true;
        case 38:                      h = { Snare, Normal, 1.0f }; return true;
        case 40:                      h = { Snare, Rim, 1.0f };    return true;
        case 37:                      h = { Snare, SideStick, 1.0f }; return true;
        case 50:                      h = { RackTom, Normal, 1.12f }; return true;
        case 48:                      h = { RackTom, Normal, 1.0f };  return true;
        case 47:                      h = { RackTom, Normal, 0.89f }; return true;
        case 45:                      h = { RackTom, Normal, 0.79f }; return true;
        case 43:                      h = { FloorTom, Normal, 1.0f }; return true;
        case 41:                      h = { FloorTom, FloorLow, 1.0f }; return true;
        case 42:                      h = { HiHat, HatClosed, 1.0f }; return true;
        case 44:                      h = { HiHat, HatPedal, 1.0f };  return true;
        case 46:                      h = { HiHat, HatOpen, 1.0f };   return true;
        case 51: case 59:             h = { Ride, Normal, 1.0f };     return true;
        case 53:                      h = { Ride, RideBell, 1.0f };   return true;
        case 49: case 52: case 55: case 57: h = { Crash, Normal, 1.0f }; return true;
        case 39:                      h = { Perc, Clap, 1.0f };       return true;
        case 54:                      h = { Perc, Tamb, 1.0f };       return true;
        case 69:                      h = { Perc, TambShake, 1.0f };  return true;
        case 70: case 82:             h = { Perc, Shaker, 1.0f };     return true;
        case 56:                      h = { Perc, Cowbell, 1.0f };    return true;
        case 62:                      h = { Perc, CongaHi, 1.06f };   return true;
        case 63:                      h = { Perc, CongaHi, 1.0f };    return true;
        case 64:                      h = { Perc, CongaLo, 1.0f };    return true;
        case 75:                      h = { Perc, Claves, 1.0f };     return true;
        default: return false;
    }
}
} // namespace

// ---------------------------------------------------------------------------
juce::AudioProcessorValueTreeState::ParameterLayout TeaTowelProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> ps;
    const auto& def = presets()[0];

    for (int p = 0; p < NumPieces; ++p)
    {
        for (int k = 0; k < NumPieceParams; ++k)
        {
            juce::NormalisableRange<float> range (0.0f, 1.0f, 0.001f);
            juce::AudioParameterFloatAttributes attr;
            switch (k)
            {
                case Tune: range = { -12.0f, 12.0f, 0.1f }; attr = attr.withStringFromValueFunction (stText); break;
                case Vol:  range = { -48.0f, 6.0f, 0.1f };  range.setSkewForCentre (-12.0f);
                           attr = attr.withStringFromValueFunction (dbText); break;
                case Pan:  range = { -1.0f, 1.0f, 0.01f };  attr = attr.withStringFromValueFunction (panText); break;
                default:   attr = attr.withStringFromValueFunction (pctText); break;
            }
            ps.push_back (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { pid (p, k), 1 },
                juce::String (pieceName (p)) + " " + pieceParamName (k),
                range, def.piece[p][k], attr));
        }
    }

    for (int k = 0; k < NumMasterParams; ++k)
    {
        juce::NormalisableRange<float> range (0.0f, 1.0f, 0.001f);
        juce::AudioParameterFloatAttributes attr = juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctText);
        if (k == Width)  range = { 0.0f, 1.5f, 0.01f };
        if (k == Output) { range = { -24.0f, 12.0f, 0.1f }; attr = attr.withStringFromValueFunction (dbText); }
        ps.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { masterId (k), 1 }, juce::String ("Master ") + masterName (k),
            range, def.master[k], attr));
    }
    // Player
    ps.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { PID::on, 1 }, "Player On", juce::StringArray { "Off", "On" }, 0));
    ps.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "sit_out", 1 }, "With The Band", juce::StringArray { "Plays along", "Sits out" }, 0));
    ps.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "band_link", 1 }, "Follow Bandleader", juce::StringArray { "Off", "On" }, 1));
    {
        juce::StringArray st; for (int i = 0; i < NumStyles; ++i) st.add (styleName (i));
        ps.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { PID::style, 1 }, "Player Style", st, 0));
        juce::StringArray se; for (int i = 0; i < NumSections; ++i) se.add (sectionName (i));
        ps.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { PID::section, 1 }, "Player Section", se, 0));
        ps.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { PID::fillEvery, 1 }, "Player Fill Every",
                                                                    juce::StringArray { "Off", "4 bars", "8 bars", "16 bars" }, 2));
    }
    auto pctAttr = juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctText);
    ps.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { PID::intensity, 1 }, "Player Intensity", juce::NormalisableRange<float> (0, 1, 0.001f), 0.55f, pctAttr));
    ps.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { PID::looseness, 1 }, "Player Looseness", juce::NormalisableRange<float> (0, 1, 0.001f), 0.3f, pctAttr));
    ps.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { PID::swing, 1 }, "Player Swing", juce::NormalisableRange<float> (0, 1, 0.001f), 0.0f, pctAttr));
    ps.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { PID::variation, 1 }, "Player Variation", juce::NormalisableRange<float> (0, 1, 0.001f), 0.5f, pctAttr));

    {
        juce::StringArray kt; for (int i = 0; i < NumKitTypes; ++i) kt.add (kitTypeName (i));
        ps.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "kit_type", 1 }, "Kit Sound", kt, 0));
    }
    ps.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "take_style", 1 }, "Take Style From Bandleader", juce::StringArray { "Off", "On" }, 1));
    return { ps.begin(), ps.end() };
}

TeaTowelProcessor::TeaTowelProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TeaTowel", createLayout())
{
    band::available();   // open the band link here (message thread), not on the audio thread
    for (int p = 0; p < NumPieces; ++p)
    {
        hitFlash[(size_t) p].store (0.0f);
        for (int k = 0; k < NumPieceParams; ++k)
            pp[p][k] = apvts.getRawParameterValue (pid (p, k));
    }
    for (int k = 0; k < NumMasterParams; ++k)
        mp[k] = apvts.getRawParameterValue (masterId (k));

    formatManager.registerBasicFormats();
    loadBuiltInKit();
    for (int t = 0; t < 3; ++t) buildMachineKit (t + 1, machineBanks[t]);
    const char* pids[] = { PID::on, PID::style, PID::section, PID::fillEvery, PID::intensity, PID::looseness, PID::swing, PID::variation };
    for (int i = 0; i < 8; ++i) playerParams[i] = apvts.getRawParameterValue (pids[i]);
    playerOut.ensureSize (4096);
    scratch.resize (512);
    renderer = std::make_unique<mc::SongRenderer> (*this, band::WTeaTowel, mc::SongRenderer::partFileName (1, "Drums"),
                                                   [] { return std::make_unique<TeaTowelProcessor>(); },
                                                   [] (juce::AudioProcessor& p) { mc::setChoice (p, "band_link", 1); }, true, false);
}

bool TeaTowelProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void TeaTowelProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    engine.prepare (sr);
    roomBuf.setSize (2, juce::jmax (samplesPerBlock, 512) * 2);
    reverb.reset();
    reverb.setSampleRate (sr);
    gateReverb.reset(); gateReverb.setSampleRate (sr);
    gateBuf.setSize (2, juce::jmax (samplesPerBlock, 512) * 2);
    gateEnv.assign ((size_t) juce::jmax (samplesPerBlock, 512) * 2, 0.0f);
    gateHold = 0; gateGain = 0; crushCount = 0; crushHold[0] = crushHold[1] = 0; crackle = 0;
    for (int c = 0; c < 2; ++c) { crushLp[c].reset(); vinylLp[c].reset(); vinylHp[c].reset(); smashLow[c].reset(); }
    smashEnv = 0;
    for (int c = 0; c < 2; ++c)
    {
        lowCut[c].reset(); thudEq[c].reset(); tapeLp[c].reset(); dcBlock[c].reset();
        lowCut[c].highpass (sr, 30.0);
        dcBlock[c].highpass (sr, 15.0);
    }
    grState = 0.0f;
    keyboardState.reset();
    player.prepare (sr);
}

// ---------------------------------------------------------------------------
void TeaTowelProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
    {
        Hit h;
        if (! mapNote (m.getNoteNumber(), h)) return;
        const float vel = m.getFloatVelocity();
        const KitBank& kb = kitType > 0 ? machineBanks[kitType - 1] : bank;
        engine.noteOn (h.piece, h.artic, h.noteMul, vel, settings[h.piece], kb, blockSamples[h.piece]);
        // the gated reverb opens on every snare and clap
        if (h.piece == Snare || (h.piece == Perc && h.artic == Clap) || h.piece == RackTom || h.piece == FloorTom)
            gateHold = (int) (0.24 * sr);
        auto& flash = hitFlash[(size_t) h.piece];
        flash.store (juce::jmax (flash.load(), vel));
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        engine.allOff();
    }
}

void TeaTowelProcessor::renderSegment (juce::AudioBuffer<float>& out, int start, int end)
{
    float* L = out.getWritePointer (0);
    float* R = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;
    float* rL = roomBuf.getWritePointer (0);
    float* rR = roomBuf.getWritePointer (1);
    float* gL = gateBuf.getWritePointer (0);
    float* gR = gateBuf.getWritePointer (1);
    // gate envelope for this stretch (open ~240 ms after a snare, then slams shut)
    for (int i = start; i < end; ++i)
    {
        if (gateHold > 0) { --gateHold; gateGain += (1.0f - gateGain) * 0.2f; }
        else gateGain *= 0.993f;
        gateEnv[(size_t) i] = gateGain;
    }

    while (start < end)
    {
        const int n = juce::jmin (end - start, (int) scratch.size());
        for (int p = 0; p < NumPieces; ++p)
        {
            std::fill (scratch.begin(), scratch.begin() + n, 0.0f);
            engine.render (p, scratch.data(), n);
            const auto& s = settings[p];
            const float gl = s.gain * s.panL, gr = s.gain * s.panR, rs = s.room;
            const float gs = (p == Snare || p == RackTom || p == FloorTom || p == Perc) ? 1.0f : 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float x = scratch[(size_t) i];
                const float l = x * gl, r = x * gr;
                L[start + i] += l;
                if (R != nullptr) R[start + i] += r;
                rL[start + i] += l * rs;
                rR[start + i] += r * rs;
                gL[start + i] += l * gs;
                gR[start + i] += r * gs;
            }
        }
        start += n;
    }
}

void TeaTowelProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (roomBuf.getNumSamples() < n) roomBuf.setSize (2, n, false, false, true);
    roomBuf.clear (0, n);
    if (gateBuf.getNumSamples() < n) gateBuf.setSize (2, n, false, false, true);
    if ((int) gateEnv.size() < n) gateEnv.resize ((size_t) n);
    gateBuf.clear (0, n);
    kitType = juce::jlimit (0, NumKitTypes - 1, (int) apvts.getRawParameterValue ("kit_type")->load());

    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    // The drummer adds its hits for this block (and keeps a copy for MIDI out)
    {
        GroovePlayer::Settings ps;
        ps.on = playerParams[0]->load() > 0.5f;
        ps.style = (int) playerParams[1]->load();
        ps.section = (int) playerParams[2]->load();
        ps.fillEvery = (int) playerParams[3]->load();
        ps.intensity = playerParams[4]->load();
        ps.looseness = playerParams[5]->load();
        ps.swing = playerParams[6]->load();
        ps.variation = playerParams[7]->load();
        // a Bandleader in the set runs the song: section, fills, drop-outs, energy
        band::markPresent (band::WTeaTowel);
        band::Leader leader;
        const bool led = apvts.getRawParameterValue ("band_link")->load() > 0.5f && band::readLeader (leader);
        uiLinked = led;
        ps.meter = led ? leader.meter : band::hostMeter (getPlayHead());
        uiMeter = ps.meter;
        capture.setMeter (ps.meter);
        fadeLed = led; fadeLeader = leader;
        if (led)
        {
            ps.link = &leader;
            ps.on = apvts.getRawParameterValue ("sit_out")->load() < 0.5f && ! leader.off (band::WTeaTowel);     // a style can leave the drums out
            ps.intensity = juce::jlimit (0.0f, 1.0f, ps.intensity + (leader.energy - 0.5f) * 0.8f);
        }
        playerOut.clear();
        player.process (midi, playerOut, n, sampleCounter, ps, getPlayHead());
    }

    // Snapshot controls once per block
    for (int p = 0; p < NumPieces; ++p)
    {
        auto& s = settings[p];
        s.tuneRatio = std::pow (2.0f, pp[p][Tune]->load() / 12.0f);
        s.damp  = pp[p][Damp]->load();
        s.decay = pp[p][Decay]->load();
        s.tone  = pp[p][Tone]->load();
        s.gain  = juce::Decibels::decibelsToGain (pp[p][Vol]->load(), -47.9f);
        const float pan = pp[p][Pan]->load();
        const float a = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        s.panL = std::cos (a) * juce::MathConstants<float>::sqrt2;
        s.panR = std::sin (a) * juce::MathConstants<float>::sqrt2;
        s.room = pp[p][Room]->load();
        blockSamples[p] = std::atomic_load (&samples[p]);
    }

    int pos = 0;
    for (const auto meta : midi)
    {
        const int ev = juce::jlimit (0, n, meta.samplePosition);
        renderSegment (buffer, pos, ev);
        handleMidi (meta.getMessage());
        pos = ev;
    }
    renderSegment (buffer, pos, n);

    // ---- Room (a small, dark, heavily damped space) -----------------------
    const float roomLvl = mp[RoomLevel]->load();
    juce::Reverb::Parameters rp;
    rp.roomSize = 0.15f + 0.6f * mp[RoomSize]->load();
    rp.damping = 0.65f;
    rp.wetLevel = 1.0f;
    rp.dryLevel = 0.0f;
    rp.width = 0.7f;
    reverb.setParameters (rp);
    reverb.processStereo (roomBuf.getWritePointer (0), roomBuf.getWritePointer (1), n);

    // ---- 80s gated reverb: a big bright room, chopped off after each snare --
    const float gateLvl = mp[GateVerb]->load();
    if (gateLvl > 0.001f)
    {
        juce::Reverb::Parameters gp;
        gp.roomSize = 0.92f; gp.damping = 0.15f; gp.wetLevel = 1.0f; gp.dryLevel = 0.0f; gp.width = 1.0f;
        gateReverb.setParameters (gp);
        gateReverb.processStereo (gateBuf.getWritePointer (0), gateBuf.getWritePointer (1), n);
        float* a = gateBuf.getWritePointer (0); float* b = gateBuf.getWritePointer (1);
        for (int i = 0; i < n; ++i) { const float e = gateEnv[(size_t) i] * gateLvl * 2.2f; a[i] *= e; b[i] *= e; }
    }

    // ---- Master bus: Thud -> Squash -> Tape -> Width -> Output ---------------
    const float thud   = mp[Thud]->load();
    const float squash = mp[Squash]->load();
    const float tape   = mp[Tape]->load();
    const float width  = mp[Width]->load();
    const float outG   = juce::Decibels::decibelsToGain (mp[Output]->load());

    for (int c = 0; c < 2; ++c)
    {
        thudEq[c].peak (sr, 95.0, 0.8, 9.0 * thud);
        tapeLp[c].lowpass (sr, 18000.0 - 8000.0 * tape);
    }

    const float thr = -6.0f - 30.0f * squash;
    const float ratio = 2.0f + 6.0f * squash;
    const float knee = 10.0f;
    const float makeup = -thr * (1.0f - 1.0f / ratio) * 0.35f;
    const float atk = std::exp (-1.0f / (0.006f * (float) sr));
    const float rel = std::exp (-1.0f / (0.18f * (float) sr));
    const bool useComp = squash > 0.005f;

    const float drive = 1.0f + tape * 5.0f;
    const float bias = 0.15f * tape;
    const float tb = std::tanh (bias);
    const float tapeNorm = 1.0f / (drive * (1.0f - tb * tb));
    const bool useTape = tape > 0.001f;
    const float crunch = mp[Crunch]->load(), vinyl = mp[Vinyl]->load(), smash = mp[Smash]->load();
    for (int c = 0; c < 2; ++c) smashLow[c].lowpass (sr, 9000.0);
    const float crunchMix = juce::jmin (1.0f, crunch * 1.6f);
    const int crushEvery = 1 + (int) std::round (crunch * 2.0f * (float) (sr / 44100.0));     // 44 -> ~15 kHz
    const float crushSteps = std::pow (2.0f, 15.0f - 7.0f * crunch);                           // 16 -> ~9 bits
    const float crackleChance = vinyl * 12.0f / (float) sr;
    for (int c = 0; c < 2; ++c)
    {
        crushLp[c].lowpass (sr, 18000.0 - 9000.0 * crunch);
        vinylLp[c].lowpass (sr, 16000.0 - 7000.0 * vinyl);
        vinylHp[c].highpass (sr, 20.0 + 60.0 * vinyl);
    }

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    const float* rL = roomBuf.getReadPointer (0);
    const float* rR = roomBuf.getReadPointer (1);
    float maxGr = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        float l = L[i] + rL[i] * roomLvl * 1.5f;
        float r = (R != nullptr ? R[i] : L[i]) + rR[i] * roomLvl * 1.5f;
        if (gateLvl > 0.001f) { l += gateBuf.getSample (0, i); r += gateBuf.getSample (1, i); }

        l = thudEq[0].process (lowCut[0].process (l));
        r = thudEq[1].process (lowCut[1].process (r));

        if (useComp)
        {
            const float lvl = juce::jmax (std::abs (l), std::abs (r)) + 1.0e-9f;
            const float over = 20.0f * std::log10 (lvl) - thr;
            float target = 0.0f;
            if (over > knee * 0.5f)       target = (1.0f / ratio - 1.0f) * over;
            else if (over > -knee * 0.5f) target = (1.0f / ratio - 1.0f) * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);
            grState = target < grState ? target + atk * (grState - target)
                                       : target + rel * (grState - target);
            const float g = juce::Decibels::decibelsToGain (grState + makeup);
            l *= g; r *= g;
            maxGr = juce::jmin (maxGr, grState);
        }

        if (useTape)
        {
            l = (std::tanh (drive * l + bias) - tb) * tapeNorm;
            r = (std::tanh (drive * r + bias) - tb) * tapeNorm;
            l = tapeLp[0].process (l);
            r = tapeLp[1].process (r);
        }
        // Smash: a blown-out drum bus - slammed by a brutal compressor, driven into distortion, mixed in under the kit
        if (smash > 0.001f)
        {
            const float lvl = juce::jmax (std::abs (l), std::abs (r));
            smashEnv += (lvl > smashEnv ? 0.25f : 0.00025f) * (lvl - smashEnv);
            const float g = 0.12f / juce::jmax (0.12f, smashEnv);          // squeeze everything to the same loudness
            const float dl = std::tanh (smashLow[0].process (l) * g * (4.0f + 10.0f * smash)) * 0.45f;
            const float dr = std::tanh (smashLow[1].process (r) * g * (4.0f + 10.0f * smash)) * 0.45f;
            l = (l * (1.0f - 0.35f * smash) + dl * smash) * (1.0f - 0.42f * smash);   // keep the level where it was
            r = (r * (1.0f - 0.35f * smash) + dr * smash) * (1.0f - 0.42f * smash);
        }
        // Crunch: an old sampler's grit (lower sample rate, fewer bits)
        if (crunch > 0.001f)
        {
            if (++crushCount >= crushEvery) { crushCount = 0; crushHold[0] = std::round (l * crushSteps) / crushSteps; crushHold[1] = std::round (r * crushSteps) / crushSteps; }
            l = crushLp[0].process (l + (crushHold[0] - l) * crunchMix);
            r = crushLp[1].process (r + (crushHold[1] - r) * crunchMix);
        }
        // Vinyl: hiss and crackle, a little band-limited
        if (vinyl > 0.001f)
        {
            vinylRng ^= vinylRng << 13; vinylRng ^= vinylRng >> 17; vinylRng ^= vinylRng << 5;
            const float u = (float) (vinylRng & 0xffffff) / 16777216.0f;
            if (u < crackleChance) crackle = (u < crackleChance * 0.5f ? 1.0f : -1.0f) * (0.3f + 0.7f * u / crackleChance);
            crackle *= 0.55f;
            const float hiss = (u - 0.5f) * 0.004f;
            l = vinylLp[0].process (vinylHp[0].process (l)) * (1.0f - 0.15f * vinyl) + (hiss + crackle * 0.05f) * vinyl;
            r = vinylLp[1].process (vinylHp[1].process (r)) * (1.0f - 0.15f * vinyl) + (hiss + crackle * 0.05f) * vinyl;
        }
        l = dcBlock[0].process (l);
        r = dcBlock[1].process (r);

        const float m = 0.5f * (l + r), s = 0.5f * (l - r) * width;
        l = (m + s) * outG;
        r = (m - s) * outG;

        if (R != nullptr) { L[i] = l; R[i] = r; }
        else              { L[i] = 0.5f * (l + r); }
    }
    gainReductionDb.store (maxGr);
    fade.apply (buffer, n, fadeLed, fadeLeader, getPlayHead());   // Bandleader's fade-out
    midi.swapWith (playerOut);     // MIDI out = only what the Player played
    capture.recordWithHost (midi, getPlayHead(), sr, n);
    sampleCounter += n;

    for (auto& s : blockSamples) s.reset();
}

// ---------------------------------------------------------------------------
void TeaTowelProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets().size())) return;
    currentProgram = index;
    const auto& pr = presets()[(size_t) index];
    auto set = [this] (const juce::String& id, float v)
    {
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    };
    for (int p = 0; p < NumPieces; ++p)
        for (int k = 0; k < NumPieceParams; ++k)
            set (pid (p, k), pr.piece[p][k]);
    for (int k = 0; k < NumMasterParams; ++k)
        set (masterId (k), pr.master[k]);
    set ("kit_type", (float) pr.kitType);
}

const juce::String TeaTowelProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, (int) presets().size()))
        return presets()[(size_t) index].name;
    return {};
}

// ---------------------------------------------------------------------------
bool TeaTowelProcessor::loadSample (int piece, const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples < 2) return false;

    const int len = (int) juce::jmin<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 20.0));
    const int chans = (int) juce::jmax (1u, reader->numChannels);
    juce::AudioBuffer<float> tmp (chans, len);
    reader->read (&tmp, 0, len, 0, true, true);

    auto data = std::make_shared<SampleData>();
    data->mono.setSize (1, len);
    data->mono.clear();
    for (int c = 0; c < chans; ++c)
        data->mono.addFrom (0, 0, tmp, c, 0, len, 1.0f / (float) chans);
    data->sampleRate = reader->sampleRate;
    data->name = file.getFileNameWithoutExtension();

    std::shared_ptr<const SampleData> newData = data;
    auto old = std::atomic_exchange (&samples[piece], newData);
    if (old != nullptr) retired.push_back (std::move (old));   // freed off the audio thread

    const juce::ScopedLock sl (pathLock);
    samplePaths[piece] = file.getFullPathName();
    return true;
}

void TeaTowelProcessor::clearSample (int piece)
{
    auto old = std::atomic_exchange (&samples[piece], std::shared_ptr<const SampleData>());
    if (old != nullptr) retired.push_back (std::move (old));
    const juce::ScopedLock sl (pathLock);
    samplePaths[piece].clear();
}

juce::String TeaTowelProcessor::builtInName (int piece)
{
    static const char* n[] = { "26\" kick", "Snare", "14\" rack tom", "16\" & 18\" floor toms",
                               "Hi-hats", "24\" ride", "20\" crash", "Claps, tambourine, shaker, cowbell, congas, claves" };
    return n[piece];
}

// Decode the embedded recordings (AVL Drumkits, CC BY-SA 3.0) into the kit bank
void TeaTowelProcessor::loadBuiltInKit()
{
    struct Map { int piece, artic; const char* stem; };
    static const Map maps[] = {
        { Kick, Normal, "kick" },          { Snare, Normal, "snare" },      { Snare, Rim, "snarerim" },
        { Snare, SideStick, "sidestick" }, { RackTom, Normal, "rtom" },     { FloorTom, Normal, "ftom" },
        { FloorTom, FloorLow, "ftomlow" }, { HiHat, HatClosed, "hatclosed" }, { HiHat, HatPedal, "hatpedal" },
        { HiHat, HatOpen, "hatopen" },     { Ride, Normal, "ride" },        { Ride, RideBell, "ridebell" },
        { Crash, Normal, "crash" },
        { Perc, Clap, "perc_clap" },       { Perc, Tamb, "perc_tamb" },     { Perc, TambShake, "perc_tshake" },
        { Perc, Shaker, "perc_shaker" },   { Perc, Cowbell, "perc_cowbell" }, { Perc, CongaHi, "perc_congahi" },
        { Perc, CongaLo, "perc_congalo" }, { Perc, Claves, "perc_claves" } };

    juce::FlacAudioFormat flac;
    for (const auto& m : maps)
    {
        for (int layer = 0; layer < kLayers; ++layer)
        {
            const juce::String resName = juce::String (m.stem) + "_" + juce::String (layer + 1) + "_flac";
            int size = 0;
            const char* data = BinaryData::getNamedResource (resName.toRawUTF8(), size);
            if (data == nullptr) continue;
            std::unique_ptr<juce::AudioFormatReader> reader (
                flac.createReaderFor (new juce::MemoryInputStream (data, (size_t) size, false), true));
            if (reader == nullptr) continue;
            const int len = (int) reader->lengthInSamples;
            const int chans = (int) juce::jmax (1u, reader->numChannels);
            juce::AudioBuffer<float> tmp (chans, len);
            reader->read (&tmp, 0, len, 0, true, true);
            auto sd = std::make_shared<SampleData>();
            sd->mono.setSize (1, len);
            sd->mono.clear();
            for (int c = 0; c < chans; ++c) sd->mono.addFrom (0, 0, tmp, c, 0, len, 1.0f / (float) chans);
            sd->sampleRate = reader->sampleRate;
            sd->name = resName;
            bank.art[m.piece][m.artic].v[(size_t) layer] = sd;
        }
    }
}

juce::String TeaTowelProcessor::getSampleName (int piece) const
{
    auto s = std::atomic_load (&samples[piece]);
    return s != nullptr ? s->name : juce::String();
}

void TeaTowelProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    {
        const juce::ScopedLock sl (pathLock);
        for (int p = 0; p < NumPieces; ++p)
            state.setProperty (juce::String ("sample_") + pieceId (p), samplePaths[p], nullptr);
    }
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void TeaTowelProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml (*xml);
    currentProgram = (int) state.getProperty ("program", 0);
    for (int p = 0; p < NumPieces; ++p)
    {
        const juce::String path = state.getProperty (juce::String ("sample_") + pieceId (p)).toString();
        if (path.isNotEmpty() && juce::File::isAbsolutePath (path) && juce::File (path).existsAsFile())
            loadSample (p, juce::File (path));
        else
            clearSample (p);
    }
    apvts.replaceState (state);
}

void TeaTowelProcessor::applyStyle (const band::StyleDef& s)
{
    setCurrentProgram (s.dKit);
    band::setParam (apvts, "sit_out", s.dOn ? 0.0f : 1.0f);
    band::setParam (apvts, PID::style, (float) s.dStyle);
    band::setParam (apvts, PID::intensity, s.dInt);
    band::setParam (apvts, PID::looseness, s.dLoose);
    band::setParam (apvts, PID::swing, s.dSwing);
    band::setParam (apvts, PID::variation, s.dVar);
    band::setParam (apvts, PID::fillEvery, (float) s.dFill);
    if (s.dRoom >= 0) band::setParam (apvts, "m_roomlvl", s.dRoom);
    if (s.dRoomSize >= 0) band::setParam (apvts, "m_roomsize", s.dRoomSize);
    band::setParam (apvts, "m_smash", s.dSmash);
}

juce::AudioProcessorEditor* TeaTowelProcessor::createEditor() { return new TeaTowelEditor (*this); }

#ifndef TEATOWEL_NO_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TeaTowelProcessor(); }
#endif
