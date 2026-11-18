#include "EtherProcessor.h"
#include "EtherEditor.h"

using namespace et;

namespace
{
juce::String pct (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
juce::String dB1 (float v, int) { return juce::String (v, 1) + " dB"; }
const int kMajor[7] = { 0, 2, 4, 5, 7, 9, 11 }, kMinor[7] = { 0, 2, 3, 5, 7, 8, 10 };
}

juce::AudioProcessorValueTreeState::ParameterLayout EtherProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> ps;
    auto F = [&] (const char* id, const char* name, juce::NormalisableRange<float> r, float def, juce::String (*fmt) (float, int))
    {
        ps.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, r, def,
                                                                   juce::AudioParameterFloatAttributes().withStringFromValueFunction (fmt)));
    };
    auto C = [&] (const char* id, const char* name, juce::StringArray items, int def)
    { ps.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def)); };
    juce::StringArray insts; for (int i = 0; i < NumInstruments; ++i) insts.add (instrumentName (i));
    C ("instrument", "Instrument", insts, 0);
    C ("role", "Plays", { "Melody", "Drone", "Swoops & Effects" }, 0);
    C ("follow", "Notes From", { "The band", "Play it (MIDI)" }, 0);
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
    F ("busy", "Busy", { 0, 1, 0.001f }, 0.45f, pct);
    F ("wander", "Wander", { 0, 1, 0.001f }, 0.5f, pct);
    F ("repeat", "Repeat (hooky)", { 0, 1, 0.001f }, 0.65f, pct);
    F ("glide", "Glide", { 0, 1, 0.001f }, 0.35f, pct);
    F ("vibrato", "Vibrato", { 0, 1, 0.001f }, 0.5f, pct);
    F ("echo", "Tape Echo", { 0, 1, 0.001f }, 0.3f, pct);
    F ("space", "Space", { 0, 1, 0.001f }, 0.4f, pct);
    F ("age", "Age", { 0, 1, 0.001f }, 0.25f, pct);
    F ("out", "Output", { -24, 6, 0.1f }, -6, dB1);
    ps.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "take_style", 1 }, "Take Style From Bandleader", juce::StringArray { "Off", "On" }, 1));
    return { ps.begin(), ps.end() };
}

EtherProcessor::EtherProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Ether", createLayout())
{
    band::available();
    queue.reserve (512);
    outMidi.ensureSize (2048);
    for (auto& p : phrase) p.reserve (40);
    answer.reserve (40);
    for (auto& t : uiTrail) t = -1;
    renderer = std::make_unique<mc::SongRenderer> (*this, band::WEther, mc::SongRenderer::partFileName (8, "Ether"),
                                                   [] { return std::make_unique<EtherProcessor>(); },
                                                   [] (juce::AudioProcessor& p) { mc::setChoice (p, "follow", 0); });
}

void EtherProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& v : voices) { v.prepare (sr); v.kill(); }
    rack.prepare (sr);
    queue.clear(); held.fill (false);
    lastStep = -1; lastPpqEnd = -1; wasPlaying = false; curBar = -1; lastSection = -1;
    rng.seed ((uint32_t) juce::Time::getMillisecondCounter() | 1u);
}

void EtherProcessor::readParams()
{
    auto g = [&] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    instrument = (int) g ("instrument"); role = (int) g ("role"); follow = (int) g ("follow"); sitOut = g ("sit_out") > 0.5f;
    key = (int) g ("key"); progs[0] = (int) g ("prog_verse"); progs[1] = (int) g ("prog_chorus"); progs[2] = (int) g ("prog_bridge");
    chordBars = 1 << (int) g ("chord_bars"); autoSong = g ("song") < 0.5f; sectionBars = ut::sectionBarsFor ((int) g ("section_bars"));
    section = (int) g ("section"); reg = (int) g ("register");
    busy = g ("busy"); wander = g ("wander"); repeat = g ("repeat"); glide = g ("glide"); vibrato = g ("vibrato");
    if (instrument != lastInstrument) { for (auto& v : voices) v.kill(); lastInstrument = instrument; }
    for (auto& v : voices) v.vibrato = vibrato;
    Rack::Params p;
    p.echo = g ("echo"); p.space = g ("space"); p.age = g ("age"); p.out = juce::Decibels::decibelsToGain (g ("out"));
    p.echoTime = (float) (60.0 / juce::jmax (30.0, bpmNow) * 0.75);     // a dotted eighth, the classic
    rack.set (p);
}

// ---- voices: one for the monophonic instruments (with glide), six for the choir
void EtherProcessor::noteOn (int note, float vel, float len, float glideSec)
{
    if (note < 0 || note > 127) return;
    const float gl = glideSec >= 0 ? glideSec : glide * glide * 0.6f;
    if (! isPoly (instrument)) { voices[0].start (instrument, note, vel, len, gl, rng); }
    else
    {
        Voice* v = nullptr;
        for (auto& x : voices) if (x.active() && x.note == note) { x.release(); }
        for (auto& x : voices) if (! x.active()) { v = &x; break; }
        if (v == nullptr) for (auto& x : voices) if (v == nullptr || x.age > v->age) v = &x;
        v->start (instrument, note, vel, len, 0, rng);
    }
    uiNote = note;
    uiTrail[trailPos] = note; trailPos = (trailPos + 1) % 32;
}
void EtherProcessor::noteOff (int note)
{
    for (auto& v : voices) if (v.active() && v.note == note && v.held) v.release();
}
void EtherProcessor::allOff() { for (auto& v : voices) v.release(); }

// ---- the song (the same form as the rest of the band)
int EtherProcessor::sectionAt (int64_t b, int& barIn, bool& lastBar) const
{
    lastBar = false;
    if (led) { int64_t in = 0; const int s = leader.sectionAt (b, &in); barIn = (int) in; lastBar = leader.nextStartBar == b + 1; return s; }
    if (autoSong) { const auto sp = ut::songPosition (b, sectionBars); barIn = sp.barIn; lastBar = sp.lastBar; return sp.section; }
    barIn = (int) (((b % 8) + 8) % 8);
    return section;
}
void EtherProcessor::chordAt (int64_t b, int& root, int& q) const
{
    if (led) { bool m = false; leader.chordAt (b, root, m); q = leader.qualityAt (b); return; }
    int barIn = 0; bool lb = false;
    const int sec = sectionAt (b, barIn, lb);
    const auto& ps = ut::progressions();
    const auto& prog = ps[(size_t) juce::jlimit (0, (int) ps.size() - 1, progs[sec])].chords;
    const auto& pc = prog[(size_t) ((barIn / juce::jmax (1, chordBars)) % (int) prog.size())];
    root = (key + pc.off) % 12; q = pc.quality();
}

// scale degree -> pitch; strong beats are pulled onto a chord tone
int EtherProcessor::pitchFor (int deg, int root, int q) const
{
    const int centers[3] = { 55, 67, 79 };
    const int k = led ? leader.key : key;
    const int o = (int) std::floor (deg / 7.0), d = deg - o * 7;
    int p = centers[juce::jlimit (0, 2, reg)] - 7 + ((k % 12) + (minorKey ? kMinor : kMajor)[d]) + 12 * o;
    juce::ignoreUnused (root, q);
    return p;
}

void EtherProcessor::writePhrase (int sec, bool fresh)
{
    if (fresh) seeds[sec] = seeds[sec] * 1664525u + 1013904223u;
    Rng r; r.seed (seeds[sec] ^ (uint32_t) (instrument * 7919));
    auto& ph = phrase[sec];
    ph.clear();
    const float b = juce::jlimit (0.0f, 1.0f, busy + (sec == 1 ? 0.12f : 0.0f) - (sec == 2 ? 0.25f : 0.0f));
    // rhythms: long singing notes when it's sparse, a tune with pickups when it's busy
    static const int sparse[][2] = { { 0, 6 }, { 8, 4 }, { 12, 4 }, { 16, 12 }, { 28, 4 } };
    static const int medium[][2] = { { 0, 3 }, { 3, 1 }, { 4, 4 }, { 8, 2 }, { 10, 2 }, { 12, 4 }, { 16, 6 }, { 22, 2 }, { 24, 6 }, { 30, 2 } };
    static const int busyR[][2] = { { 0, 2 }, { 2, 2 }, { 4, 1 }, { 5, 1 }, { 6, 2 }, { 8, 4 }, { 12, 2 }, { 14, 2 }, { 16, 2 }, { 18, 2 }, { 20, 2 }, { 22, 2 }, { 24, 6 }, { 30, 2 } };
    // three-beat bars: 24 steps over two bars (3/4 in fours, 6/8 in sixes)
    static const int sparse3[][2] = { { 0, 8 }, { 8, 4 }, { 12, 8 }, { 20, 4 } };
    static const int medium3[][2] = { { 0, 4 }, { 4, 2 }, { 6, 2 }, { 8, 4 }, { 12, 6 }, { 18, 2 }, { 20, 4 } };
    static const int busy3[][2] = { { 0, 2 }, { 2, 2 }, { 4, 2 }, { 6, 1 }, { 7, 1 }, { 8, 4 }, { 12, 2 }, { 14, 2 }, { 16, 2 }, { 18, 2 }, { 20, 4 } };
    static const int sparse6[][2] = { { 0, 6 }, { 6, 6 }, { 12, 10 }, { 22, 2 } };
    static const int medium6[][2] = { { 0, 3 }, { 3, 1 }, { 4, 2 }, { 6, 4 }, { 10, 2 }, { 12, 6 }, { 18, 2 }, { 20, 2 }, { 22, 2 } };
    static const int busy6[][2] = { { 0, 2 }, { 2, 2 }, { 4, 2 }, { 6, 2 }, { 8, 2 }, { 10, 2 }, { 12, 4 }, { 16, 2 }, { 18, 4 }, { 22, 2 } };
    auto& rhythm = rhythmScratch; rhythm.clear();     // (kept between calls: nothing is allocated while playing)
    auto take = [&] (const auto& tbl) { for (auto& x : tbl) rhythm.push_back ({ x[0], x[1] }); };
    if (meterNow == band::M34)      { if (b < 0.35f) take (sparse3); else if (b < 0.7f) take (medium3); else take (busy3); }
    else if (meterNow == band::M68) { if (b < 0.35f) take (sparse6); else if (b < 0.7f) take (medium6); else take (busy6); }
    else if (b < 0.35f) take (sparse);
    else if (b < 0.7f) take (medium);
    else take (busyR);
    // leave a breath sometimes
    for (size_t i = 1; i + 1 < rhythm.size(); ++i) if (r.chance (0.12f + (1.0f - b) * 0.1f)) { rhythm.erase (rhythm.begin() + (long) i); }
    int deg = r.chance (0.5f) ? 4 : (r.chance (0.5f) ? 2 : 0);
    if (sec == 1) deg += 2;     // the chorus sings higher
    for (size_t i = 0; i < rhythm.size(); ++i)
    {
        Note16 n; n.step = rhythm[i].first; n.len = rhythm[i].second;
        if (i > 0)
        {
            const float x = r.u();
            const int leap = 2 + r.i (2 + (int) (wander * 3));
            deg += x < 0.38f ? -1 : x < 0.7f ? 1 : x < 0.7f + wander * 0.2f ? (r.chance (0.5f) ? leap : -leap) : 0;
            deg = juce::jlimit (-3, 11, deg);
        }
        // long notes and downbeats land on the triad (0, 2, 4 steps from the tonic)
        if ((strongStep (n.step) || n.len >= 4) && ((deg % 7) + 7) % 7 != 0 && ((deg % 7) + 7) % 7 != 2 && ((deg % 7) + 7) % 7 != 4) deg += r.chance (0.5f) ? 1 : -1;
        n.deg = deg; n.vel = n.step % band::meterBeatSteps (meterNow) == 0 ? 0.85f : 0.7f;
        ph.push_back (n);
    }
    if (! ph.empty()) { ph.back().deg = sec == 1 ? 7 : 0; ph.back().len = juce::jmax (ph.back().len, 3); }
    // the answer: same start, a different ending that comes home
    answer = ph;
    int d2 = answer.size() > 2 ? answer[answer.size() / 2].deg : 0;
    for (size_t i = answer.size() / 2; i < answer.size(); ++i) { d2 += r.chance (0.6f) ? -1 : 1; answer[i].deg = d2; }
    if (! answer.empty()) answer.back().deg = 0;
}

void EtherProcessor::playStep (int64_t k, int64_t at)
{
    const int64_t b = k / bs;
    const int s16 = (int) (((k % bs) + bs) % bs);
    int barIn = 0; bool lastBar = false;
    const int sec = sectionAt (b, barIn, lastBar);
    if (b != curBar)
    {
        curBar = b;
        uiSection = sec;
        // what key/mode is the song in? (from the verse's first chord)
        if (led) minorKey = band::qualityIsMinor (leader.progMinor[0][0]);
        else { const auto& pr = ut::progressions()[(size_t) juce::jlimit (0, (int) ut::progressions().size() - 1, progs[0])].chords; minorKey = pr.empty() || band::qualityIsMinor (pr[0].quality()); }
        if (sec != lastSection || phrase[sec].empty()) { writePhrase (sec, phrase[sec].empty() ? false : rng.chance (1.0f - repeat)); }
        else if (barIn > 0 && barIn % 8 == 0 && rng.chance ((1.0f - repeat) * 0.8f)) writePhrase (sec, true);
        // swoop into a new section
        if (role != RDrone && sec != lastSection && lastSection >= 0 && (role == RSwoops || rng.chance (0.4f)) && ! (led && (leader.rests (b, false, true) || leader.isEnding (b))))
        {
            int r = 0, q = 0; chordAt (b, r, q);
            const int top = pitchFor (sec == 1 ? 9 : 7, r, q);
            schedule ({ at, 1, top - 19, 0.7f, 0.05f });
            schedule ({ at + (int64_t) (sr * 0.03), 1, top, 0.85f, (float) (60.0 / bpmNow * 2.0), (float) (60.0 / bpmNow * 0.5) });
        }
        lastSection = sec;
    }
    // the song's last chord: one long note (the whole chord for the choir)
    if (led && leader.isEnding (b))
    {
        if (leader.isFinalHit (b) && s16 == 0 && role != RSwoops)
        {
            int root = 0, q = 0; chordAt (b, root, q);
            schedule ({ at, 3, 0, 0, 0 });
            const int base = pitchFor (0, root, q) - (isPoly (instrument) ? 12 : 0) + ((root - leader.keyAt (b)) % 12 + 12) % 12;
            const float len = (float) (15.0 / juce::jmax (30.0, bpmNow) * bs * 2 * 0.95);
            if (isPoly (instrument)) { int t[4]; const int nt = band::qualityTones (q, t); for (int i = 0; i < nt && i < 3; ++i) schedule ({ at + 2 + i * 40, 1, base + t[i] + (i == 1 ? 12 : 0), 0.8f, len }); }
            else schedule ({ at + 2, 1, base, 0.8f, len });
        }
        return;
    }
    if (led && leader.rests (b, false, role != RDrone)) { if (! outNow) { schedule ({ at, 3, 0, 0, 0 }); outNow = true; } return; }
    outNow = false;
    int root = 0, q = 0; chordAt (b, root, q);
    uiChord = root | (q << 4);
    const double sixteenth = 15.0 / juce::jmax (30.0, bpmNow);
    const int64_t jit = (int64_t) (rng.n() * 0.004 * sr);

    if (role == RDrone)
    {
        // a long tone on each chord: the root (and fifth and third for the choir), gliding for the wand
        const bool change = root != lastRoot;
        const int every = isPoly (instrument) ? 2 : 4;     // the tape choir only has 8 seconds of tape
        if (s16 == 0 && (change || barIn % every == 0))
        {
            lastRoot = root;
            const int base = pitchFor (0, root, q) - (isPoly (instrument) ? 12 : 0) + ((root - (led ? leader.keyAt (b) : key)) % 12 + 12) % 12;
            const int64_t late = std::abs (jit) + 2;   // after the release below, never before it
            const float len = (float) (sixteenth * bs * every * 1.02);
            if (isPoly (instrument))
            {
                schedule ({ at, 3, 0, 0, 0 });
                int t[4]; const int nt = band::qualityTones (q, t);
                for (int i = 0; i < nt && i < 3; ++i) schedule ({ at + late + i * 40, 1, base + t[i] + (i == 1 ? 12 : 0), 0.8f, len });
            }
            else schedule ({ at + late, 1, base, 0.8f, len });
        }
        return;
    }
    if (role == RSwoops)
    {
        // every few bars a long glide between chord tones, and radio bloops in the gaps
        const bool bigBar = barIn % 4 == 0;
        int t[4]; const int nt = band::qualityTones (q, t);
        const int base = pitchFor (0, root, q) + ((root - (led ? leader.keyAt (b) : key)) % 12 + 12) % 12;
        if (s16 == 0 && bigBar)
        {
            schedule ({ at + jit, 1, base - 12, 0.65f, 0.08f });
            schedule ({ at + jit + (int64_t) (sr * 0.06), 1, base + 12 + t[nt - 1], 0.8f, (float) (sixteenth * 12), (float) (sixteenth * 4) });
        }
        else if (s16 == bs / 2 && bigBar && rng.chance (0.6f))
            schedule ({ at + jit, 1, base + t[rng.i (nt)], 0.7f, (float) (sixteenth * 6) });
        else if (! bigBar && rng.chance (0.08f + busy * 0.22f))
            schedule ({ at + jit, 1, base + 12 + t[rng.i (nt)] + (rng.chance (0.3f) ? 12 : 0), 0.6f, (float) (sixteenth * (rng.chance (0.5f) ? 1 : 2)) });
        return;
    }

    // melody: the phrase on bars 1-2 of every four, the answer on bars 3-4; it breathes in the bridge
    if (sec == 2 && barIn % 4 >= 2) return;
    const bool ans = (barIn % 4) >= 2;
    const auto& ph = ans ? answer : phrase[sec];
    const int stepIn = (barIn % 2) * bs + s16;
    const bool half = led && leader.isHalf (b);
    for (const auto& n : ph)
    {
        if (n.step != stepIn) continue;
        if (half && n.step % bs != 0) continue;     // the breakdown: only the downbeats
        int p = pitchFor (n.deg, root, q);
        // on a downbeat, make sure we're on a note of the chord
        if (strongStep (n.step))
        {
            int t[4]; const int nt = band::qualityTones (q, t); int best = p, bd = 99;
            for (int o = -1; o <= 1; ++o) for (int i = 0; i < nt; ++i) { const int c = root + t[i] + 12 * ((p - root) / 12 + o); if (std::abs (c - p) < bd) { bd = std::abs (c - p); best = c; } }
            p = best;
        }
        const float vel = juce::jlimit (0.1f, 1.0f, n.vel + rng.n() * 0.06f + (led ? (leader.energy - 0.5f) * 0.2f : 0.0f));
        schedule ({ at + jit, 1, p, vel, (float) (n.len * sixteenth * 0.95) });
    }
}

void EtherProcessor::runPlayer (int n)
{
    double ppq = 0, bpm = 120; bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto p = pos->getPpqPosition()) ppq = *p; else playing = false;
        }
    bpmNow = bpm; blockPpq = ppq; uiTransport = playing;
    {
        const int m = led ? leader.meter : band::hostMeter (getPlayHead());
        if (m != meterNow) { meterNow = m; curBar = -1; for (auto& ph : phrase) ph.clear(); }
        bs = band::meterSteps (meterNow);
        capture.setMeter (meterNow);
    }
    if (! playing) { if (wasPlaying) schedule ({ sampleClock, 3, 0, 0, 0 }); wasPlaying = false; return; }
    const double spb = sr * 60.0 / bpm;
    if (! wasPlaying || ppq < lastPpqEnd - 0.02 || ppq > lastPpqEnd + 0.25)
    {
        lastStep = (int64_t) std::ceil (ppq * 4.0 - 0.001) - 1;
        curBar = -1; lastRoot = -1;
        if (wasPlaying) schedule ({ sampleClock, 3, 0, 0, 0 });
    }
    wasPlaying = true;
    const double ppqEnd = ppq + n / spb;
    for (int64_t k = lastStep + 1; (double) k / 4.0 < ppqEnd; ++k)
    {
        playStep (k, sampleClock + juce::jmax ((int64_t) 0, (int64_t) (((double) k / 4.0 - ppq) * spb)));
        lastStep = k;
    }
    lastPpqEnd = ppqEnd;
}

void EtherProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    buffer.clear();
    readParams();
    band::markPresent (band::WEther);
    const bool wasLed = led;
    led = follow == FBand && band::readLeader (leader);
    if (led && leader.mutateStamp != lastMutate) { lastMutate = leader.mutateStamp; rng.seed (leader.mutateStamp * 2654435761u + 19u); curBar = -1; for (auto& ph : phrase) ph.clear(); }
    uiLinked = led;
    if (led != wasLed) lastPpqEnd = -1e9;

    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        const int64_t at = sampleClock + m.samplePosition;
        if (msg.isNoteOn())
        {
            held[(size_t) msg.getNoteNumber()] = true;
            if (follow == FPlay) schedule ({ at, 1, msg.getNoteNumber(), msg.getFloatVelocity(), -1.0f });
        }
        else if (msg.isNoteOff())
        {
            const int nn = msg.getNoteNumber();
            held[(size_t) nn] = false;
            if (follow == FPlay)
            {
                // monophonic: fall back to the last key still held, gliding there
                int back = -1; for (int i = 127; i >= 0; --i) if (held[(size_t) i]) { back = i; break; }
                if (! isPoly (instrument) && back >= 0 && voices[0].note == nn) schedule ({ at, 1, back, 0.7f, -1.0f });
                else schedule ({ at, 2, nn, 0, 0 });
            }
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff()) { held.fill (false); schedule ({ at, 3, 0, 0, 0 }); }
    }
    if (follow == FBand && ! sitOut && ! (led && leader.off (band::WEther))) runPlayer (n);
    else { uiTransport = false; if (wasPlaying) { schedule ({ sampleClock, 3, 0, 0, 0 }); wasPlaying = false; } }

    std::stable_sort (queue.begin(), queue.end(), [] (const Ev& a, const Ev& b) { return a.at < b.at; });
    outMidi.clear();
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    std::vector<float> tmp;   // only used for mono hosts
    if (R == nullptr) { tmp.assign ((size_t) n, 0.0f); R = tmp.data(); }
    size_t e = 0;
    for (int i = 0; i < n; ++i)
    {
        const int64_t now = sampleClock + i;
        while (e < queue.size() && queue[e].at <= now)
        {
            const auto& ev = queue[e++];
            if (ev.type == 1)
            {
                // a monophonic instrument only has one note at a time on its MIDI out too
                if (! isPoly (instrument)) midiOut.allOff (outMidi, i);
                noteOn (ev.note, ev.vel, ev.len, ev.gl);
                midiOut.on (outMidi, i, now, ev.note, ev.vel, ev.len, sr);
            }
            else if (ev.type == 2) { noteOff (ev.note); midiOut.off (outMidi, i, ev.note); }
            else { allOff(); midiOut.allOff (outMidi, i); }
        }
        midiOut.tick (outMidi, i, now);
        float l = 0, r = 0;
        for (auto& v : voices) v.render (l, r);
        L[i] = l * 0.5f; R[i] = r * 0.5f;
    }
    queue.erase (queue.begin(), queue.begin() + (long) e);
    for (auto& q : queue) if (q.at < sampleClock + n) q.at = sampleClock + n;
    rack.process (L, R, n, rng);
    float peak = 0;
    for (int i = 0; i < n; ++i)
    {
        for (float* x : { &L[i], &R[i] })
        {
            if (! std::isfinite (*x)) *x = 0;
            if (std::abs (*x) > 0.9f) *x = std::copysign (0.9f + 0.1f * std::tanh ((std::abs (*x) - 0.9f) * 10.0f), *x);
        }
        peak = juce::jmax (peak, std::abs (L[i]));
    }
    uiLevel = peak;
    bool any = false; for (auto& v : voices) any = any || (v.active() && v.held);
    if (! any) uiNote = -1;
    fade.apply (buffer, n, led, leader, getPlayHead());   // Bandleader's fade-out
    midi.swapWith (outMidi);
    {
        double cp = 0; bool cpl = false;
        if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) { cpl = pos->getIsPlaying(); if (auto q = pos->getPpqPosition()) cp = *q; else cpl = false; if (auto b = pos->getBpm()) bpmNow = *b; }
        capture.recordBlock (midi, cp, cpl, bpmNow, sr, n);
    }
    sampleClock += n;
}

void EtherProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("seeds", juce::String (seeds[0]) + "," + juce::String (seeds[1]) + "," + juce::String (seeds[2]), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void EtherProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto t = juce::ValueTree::fromXml (*xml);
            juce::StringArray s; s.addTokens (t.getProperty ("seeds").toString(), ",", "");
            if (s.size() == 3) for (int i = 0; i < 3; ++i) seeds[i] = (uint32_t) s[i].getLargeIntValue();
            t.removeProperty ("seeds", nullptr);
            apvts.replaceState (t);
            for (auto& p : phrase) p.clear();
        }
}

void EtherProcessor::applyStyle (const band::StyleDef& s)
{
    band::setParam (apvts, "sit_out", s.eRole > 0 ? 0.0f : 1.0f);
    if (s.eRole <= 0) return;
    band::setParam (apvts, "role", (float) (s.eRole - 1));
    band::setParam (apvts, "instrument", (float) s.eInst);
    band::setParam (apvts, "space", s.eSpace);
}

juce::AudioProcessorEditor* EtherProcessor::createEditor() { return new EtherEditor (*this); }

#ifndef ETHER_NO_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EtherProcessor(); }
#endif
