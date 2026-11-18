#include "ChamberProcessor.h"
#include "ChamberEditor.h"
#include "ChamberData.h"

using namespace ch;

namespace
{
juce::String pct (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
juce::String bip (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
juce::String dB1 (float v, int) { return juce::String (v, 1) + " dB"; }
const int kMajor[7] = { 0, 2, 4, 5, 7, 9, 11 }, kMinor[7] = { 0, 2, 3, 5, 7, 8, 10 };
const float kPan[4] = { -0.72f, -0.3f, 0.32f, 0.74f };      // violins left, cellos right: the 60s way
}

juce::AudioProcessorValueTreeState::ParameterLayout ChamberProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> ps;
    auto F = [&] (const char* id, const char* name, juce::NormalisableRange<float> r, float def, juce::String (*fmt) (float, int))
    {
        ps.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, r, def,
                                                                   juce::AudioParameterFloatAttributes().withStringFromValueFunction (fmt)));
    };
    auto C = [&] (const char* id, const char* name, juce::StringArray items, int def)
    { ps.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def)); };
    juce::StringArray ens, feels;
    for (int i = 0; i < NumEnsembles; ++i) ens.add (ensembleName (i));
    for (int i = 0; i < NumFeels; ++i) feels.add (feelName (i));
    C ("ensemble", "Ensemble", ens, 0);
    C ("feel", "Arrangement", feels, 1);
    C ("follow", "Chords From", { "The band", "MIDI chords", "Play it (MIDI)" }, 0);
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
    C ("timpani", "Timpani", { "Off", "On" }, 0);
    C ("play_art", "Played Articulation", { "Long (arco)", "Short (spiccato)", "Pizzicato", "Tremolo" }, 0);
    F ("busy", "Busy", { 0, 1, 0.001f }, 0.5f, pct);
    F ("dynamics", "Dynamics", { 0, 1, 0.001f }, 0.6f, pct);
    F ("swell", "Swell (bowing)", { 0, 1, 0.001f }, 0.35f, pct);
    F ("human", "Human", { 0.1f, 1, 0.001f }, 0.6f, pct);
    F ("swing", "Swing", { 0, 0.5f, 0.001f }, 0, pct);
    F ("length", "Note Length", { 0.3f, 1.5f, 0.001f }, 1.0f, pct);
    F ("warp", "Warp (wobbly orchestra)", { 0, 1, 0.001f }, 0, pct);
    F ("tape", "Tape", { 0, 1, 0.001f }, 0, pct);
    F ("width", "Width", { 0, 1, 0.001f }, 0.9f, pct);
    F ("room", "Room", { 0, 1, 0.001f }, 0.3f, pct);
    F ("tone", "Tone", { -1, 1, 0.001f }, 0, bip);
    F ("out", "Output", { -24, 6, 0.1f }, -3, dB1);
    ps.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "take_style", 1 }, "Take Style From Bandleader", juce::StringArray { "Off", "On" }, 1));
    return { ps.begin(), ps.end() };
}

ChamberProcessor::ChamberProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Chamber", createLayout())
{
    band::available();
    queue.reserve (2048); bar.reserve (96);
    for (auto& p : phrase) p.reserve (40);
    loadSamples();
    renderer = std::make_unique<mc::SongRenderer> (*this, band::WChamber, mc::SongRenderer::partFileName (6, "Orchestra"),
                                                   [] { return std::make_unique<ChamberProcessor>(); },
                                                   [] (juce::AudioProcessor& p) { mc::setChoice (p, "follow", 0); });
}

// names: <code>_<art>_<midi>_<layer>[_L<loop start>]_flac   (timpani: tim_<midi>_<layer>_flac)
void ChamberProcessor::loadSamples()
{
    juce::FlacAudioFormat flac;
    for (int r = 0; r < ChamberData::namedResourceListSize; ++r)
    {
        const juce::String res (ChamberData::namedResourceList[r]);
        juce::StringArray b; b.addTokens (res, "_", "");
        if (b.size() < 4) continue;
        int instr = -1;
        for (int i = 0; i < NumInstr; ++i) if (b[0] == instrCode (i)) instr = i;
        if (instr < 0) continue;
        int art = ASus, idx = 1;
        if (instr != TIM) { for (int a = 0; a < NumArts; ++a) if (b[1] == artCode (a)) art = a; idx = 2; }
        Zone z;
        z.root = b[idx].getIntValue(); z.layer = b[idx + 1].getIntValue();
        if (b[idx + 2].startsWith ("L")) z.loopStart = b[idx + 2].substring (1).getIntValue();
        int size = 0;
        const char* data = ChamberData::getNamedResource (res.toRawUTF8(), size);
        if (data == nullptr) continue;
        std::unique_ptr<juce::AudioFormatReader> reader (flac.createReaderFor (new juce::MemoryInputStream (data, (size_t) size, false), true));
        if (reader == nullptr) continue;
        const int len = (int) reader->lengthInSamples;
        juce::AudioBuffer<float> tmp ((int) juce::jmax (1u, reader->numChannels), len);
        reader->read (&tmp, 0, len, 0, true, true);
        z.sr = reader->sampleRate;
        z.d.assign (tmp.getReadPointer (0), tmp.getReadPointer (0) + len);
        z.d.push_back (z.loopStart >= 0 ? z.d[(size_t) z.loopStart] : 0.0f); z.d.push_back (0);
        if (z.loopStart >= len - 4) z.loopStart = -1;
        lib.zones[instr][art].push_back (std::move (z));
        ++lib.count;
    }
}

void ChamberProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& v : voices) { v.prepare (sr); v.kill(); }
    rack.prepare (sr);
    queue.clear(); held.fill (false);
    lastStep = -1; lastPpqEnd = -1; wasPlaying = false; curBar = -1;
    rng.seed ((uint32_t) juce::Time::getMillisecondCounter() | 1u);
}

void ChamberProcessor::readParams()
{
    auto g = [&] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    ensemble = (int) g ("ensemble"); feel = (int) g ("feel"); follow = (int) g ("follow"); sitOut = g ("sit_out") > 0.5f;
    key = (int) g ("key"); progs[0] = (int) g ("prog_verse"); progs[1] = (int) g ("prog_chorus"); progs[2] = (int) g ("prog_bridge");
    chordBars = 1 << (int) g ("chord_bars"); autoSong = g ("song") < 0.5f; sectionBars = ut::sectionBarsFor ((int) g ("section_bars"));
    section = (int) g ("section"); reg = (int) g ("register"); timpani = g ("timpani") > 0.5f; playArt = (int) g ("play_art");
    busy = g ("busy"); dynamics = g ("dynamics"); swell = g ("swell"); human = g ("human"); swing = g ("swing"); lengthK = g ("length");
    warp = g ("warp"); width = g ("width");
    ensembleParts (ensemble, parts);
    if (ensemble != lastEnsemble) { if (lastEnsemble >= 0) releaseParts (31); lastEnsemble = ensemble; }
    Rack::Params p;
    p.room = g ("room"); p.tone = g ("tone"); p.width = width; p.out = juce::Decibels::decibelsToGain (g ("out"));
    p.tape = juce::jmax (g ("tape"), ensembleIsTape (ensemble) ? 0.5f : 0.0f);
    rack.set (p);
    for (auto& v : voices) v.warp = warp;
}

int ChamberProcessor::fitTo (int instr, int note) const
{
    int lo = 0, hi = 127; instrRange (instr, lo, hi);
    while (note < lo) note += 12;
    while (note > hi) note -= 12;
    return note;
}

// four-part voicing: the root in the bass, the upper three in close position, moving as little as possible
void ChamberProcessor::voiceChord (int root, int q)
{
    int t[4]; const int nt = band::qualityTones (q, t);
    int up[3];
    if (nt == 4) { up[0] = t[1]; up[1] = t[2]; up[2] = t[3]; } else { up[0] = t[0]; up[1] = t[1]; up[2] = t[2]; }
    const int centers[3] = { 62, 67, 74 };
    const int c = centers[juce::jlimit (0, 2, reg)];
    int best[3] { 0, 0, 0 }; float bestCost = 1e9f;
    for (int mask = 0; mask < 8; ++mask)
    {
        int ns[3];
        for (int i = 0; i < 3; ++i)
        {
            int p = root + up[i];
            while (p < c - 7) p += 12;
            while (p >= c + 5) p -= 12;
            if ((mask >> i) & 1) p += 12;
            ns[i] = p;
        }
        std::sort (ns, ns + 3);
        if (ns[2] - ns[0] > 12) continue;
        float cost = 0;
        if (lastRoot >= 0) for (int i = 0; i < 3; ++i) cost += (float) std::abs (ns[2 - i] - voicing[i]);
        else cost = (float) std::abs ((ns[0] + ns[2]) / 2 - c);
        if (cost < bestCost) { bestCost = cost; std::copy (ns, ns + 3, best); }
    }
    voicing[0] = best[2]; voicing[1] = best[1]; voicing[2] = best[0];
    int b = root; while (b < 36) b += 12; while (b > 50) b -= 12;
    voicing[3] = b;
    lastRoot = root; lastQ = q;
}

int ChamberProcessor::scaleNote (int deg) const
{
    const int k = led ? leader.key : key;
    const int o = (int) std::floor (deg / 7.0), d = deg - o * 7;
    const int centers[3] = { 67, 72, 79 };
    return centers[juce::jlimit (0, 2, reg)] - 5 + (k % 12) + (minorKey ? kMinor : kMajor)[d] + 12 * o;
}

int ChamberProcessor::sectionAt (int64_t b, int& in, bool& lastBar) const
{
    lastBar = false;
    if (led) { int64_t x = 0; const int s = leader.sectionAt (b, &x); in = (int) x; lastBar = leader.nextStartBar == b + 1; return s; }
    if (autoSong) { const auto sp = ut::songPosition (b, sectionBars); in = sp.barIn; lastBar = sp.lastBar; return sp.section; }
    in = (int) (((b % 8) + 8) % 8);
    return section;
}

void ChamberProcessor::chordAt (int64_t b, int& root, int& q) const
{
    if (follow == FMidiChords) { root = midiRoot; q = midiQ; return; }
    if (led) { bool m = false; leader.chordAt (b, root, m); q = leader.qualityAt (b); return; }
    int in = 0; bool lb = false;
    const int sec = sectionAt (b, in, lb);
    const auto& ps = ut::progressions();
    const auto& prog = ps[(size_t) juce::jlimit (0, (int) ps.size() - 1, progs[sec])].chords;
    const auto& pc = prog[(size_t) ((in / juce::jmax (1, chordBars)) % (int) prog.size())];
    root = (key + pc.off) % 12; q = pc.quality();
}

// a two-bar tune for the countermelody / unison hook: (step, scale degree)
void ChamberProcessor::writePhrase (int sec)
{
    Rng r; r.seed (seeds[sec] ^ (uint32_t) (ensemble * 977 + feel * 31));
    auto& ph = phrase[sec]; ph.clear();
    const float b = juce::jlimit (0.0f, 1.0f, busy + (sec == 1 ? 0.12f : 0.0f));
    static const int slow[] = { 0, 8, 12, 16, 24 }, mid[] = { 0, 4, 6, 8, 12, 16, 20, 22, 24, 28 }, fast[] = { 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 24, 26, 28, 30 };
    // three-beat bars (12 steps): 3/4 and 6/8 rhythms over two bars
    static const int slow3[] = { 0, 8, 12, 20 }, mid3[] = { 0, 4, 8, 12, 16, 20, 22 }, fast3[] = { 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22 };
    static const int slow6[] = { 0, 6, 12, 18 }, mid6[] = { 0, 4, 6, 10, 12, 16, 18, 22 }, fast6[] = { 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22 };
    const int* rh; int n;
    if (meterNow == band::M44) { rh = b < 0.35f ? slow : (b < 0.7f ? mid : fast); n = b < 0.35f ? 5 : (b < 0.7f ? 10 : 15); }
    else if (meterNow == band::M34) { rh = b < 0.35f ? slow3 : (b < 0.7f ? mid3 : fast3); n = b < 0.35f ? 4 : (b < 0.7f ? 7 : 12); }
    else { rh = b < 0.35f ? slow6 : (b < 0.7f ? mid6 : fast6); n = b < 0.35f ? 4 : (b < 0.7f ? 8 : 12); }
    int deg = sec == 1 ? 4 : 2;
    for (int i = 0; i < n; ++i)
    {
        if (i > 0) { const float x = r.u(); deg += x < 0.4f ? -1 : x < 0.78f ? 1 : (r.chance (0.5f) ? 2 : -2); deg = juce::jlimit (-2, 9, deg); }
        ph.push_back ({ rh[i], deg });
    }
    if (! ph.empty()) ph.back().second = sec == 1 ? 4 : 0;
}

void ChamberProcessor::writeBar (int sec, int in, bool fillBar)
{
    bar.clear();
    const int N = bs;                                   // steps a bar: 16, or 12 in 3/4 and 6/8
    const int beat = band::meterBeatSteps (meterNow);
    const bool chorus = sec == 1, bridge = sec == 2;
    const float b = juce::jlimit (0.0f, 1.0f, busy + (chorus ? 0.15f : 0.0f) - (bridge ? 0.15f : 0.0f));
    auto add = [&] (int s, int partsMask, int art, float len, float vel, int kind = KChord, int arg = 0) { bar.push_back ({ s, partsMask, art, len, vel, kind, arg }); };
    const float lv = chorus ? 0.78f : (bridge ? 0.55f : 0.65f);
    auto phraseHits = [&] (int partsMask, int kind)
    {
        for (size_t i = 0; i < phrase[sec].size(); ++i)
        {
            const int st = phrase[sec][i].first - (in % 2) * N;
            if (st >= 0 && st < N) add (st, partsMask, ASus, (float) ((i + 1 < phrase[sec].size() ? phrase[sec][i + 1].first : 2 * N) - phrase[sec][i].first), lv + 0.1f, kind, (int) i);
        }
    };
    switch (feel)
    {
        case FLong: add (0, 15, ASus, (float) N, lv); break;
        case FStaccato:
            for (int s = 0; s < N; s += 2)
            {
                if (b < 0.4f && s % beat != 0 && meterNow != band::M68) continue;
                add (s, 15, AShort, 1.0f, s % (2 * beat) == 0 ? 0.88f : (s % beat == 0 ? 0.78f : 0.66f));
            }
            if (chorus && b > 0.5f) add (N - 1, 15, AShort, 0.7f, 0.6f);
            break;
        case FCounter:
            add (0, 14, ASus, (float) N, lv * 0.75f);
            phraseHits (1, KMelody);
            break;
        case FDescend:
            add (0, 7, ASus, (float) N, lv * 0.85f);
            { const int line[4] = { 0, -1, -2, -3 }, line2[4] = { -4, -5, -3, -1 };
              const bool second = chordBars > 1 && (in % chordBars) % 2 == 1;
              const int steps = N / beat;
              for (int i = 0; i < steps; ++i) add (i * beat, 8, ASus, (float) beat, lv + 0.05f, KBass, second ? line2[i] : line[i]); }
            break;
        case FPizz:
        {
            static const int order[8] = { 8, 4, 2, 1, 8, 2, 4, 1 };
            for (int i = 0; i < N / 2; ++i) if (! (b < 0.35f && i % 2 == 1)) add (i * 2, order[i], APizz, 2.0f, (i * 2) % beat == 0 ? 0.85f : 0.72f);
            if (b > 0.6f) for (int s = beat; s < N; s += beat) add (s, 15, APizz, 2.0f, 0.7f);
            break;
        }
        case FTremolo: add (0, 15, ATrem, (float) N, lv); break;
        case FRise: add (0, 15, ASus, (float) N, lv * 0.7f); break;     // the rise itself is played in playStep
        case FRiff:
        {
            add (0, 3, ASus, (float) N, lv * 0.7f);
            static const int riffMaj[8] = { 0, 0, 7, 5, 0, 0, 10, 7 }, riffMin[8] = { 0, 3, 7, 3, 0, 10, 7, 5 };
            static const int riff3Maj[6] = { 0, 7, 5, 0, 10, 7 }, riff3Min[6] = { 0, 3, 7, 0, 10, 7 };
            const bool m = band::qualityIsMinor (curQ);
            for (int i = 0; i < N / 2; ++i)
                add (i * 2, 12, AShort, 1.3f, (i * 2) % beat == 0 ? 0.88f : 0.74f, KRiff, N == 16 ? (m ? riffMin : riffMaj)[i] : (m ? riff3Min : riff3Maj)[i]);
            break;
        }
        case FFanfare:
            add (0, 15, AShort, 1.5f, 0.92f); add (3, 15, AShort, 0.8f, 0.7f); add (4, 15, ASus, 3.0f, 0.85f);
            if (b > 0.4f && N == 16) { add (8, 15, AShort, 1.0f, 0.75f); add (10, 15, AShort, 1.0f, 0.75f); }
            add (N - 4, 15, ASus, 4.0f, 0.88f);
            break;
        case FUnison:
            phraseHits (15, KUnison);
            break;
        default:  // FBaroque
        {
            for (int s = 0; s < N; s += beat) add (s, 14, AShort, 1.6f, 0.66f);
            static const int up[16] = { 0, 1, 2, 3, 4, 3, 2, 1, 0, 1, 2, 3, 4, 5, 6, 7 };
            const bool fast = b >= 0.45f;
            for (int s = 0; s < N; s += fast ? 1 : 2) add (s, 1, AShort, fast ? 0.9f : 1.8f, s % beat == 0 ? 0.82f : 0.7f, KRun, up[s]);
            break;
        }
    }
    if (fillBar && feel != FRise)
    {
        // into the next section: a quick run up in the top part
        bar.erase (std::remove_if (bar.begin(), bar.end(), [N] (const Hit& h) { return h.step >= N - 4 && (h.parts & 1); }), bar.end());
        for (int i = 0; i < 4; ++i) add (N - 4 + i, 1, AShort, 0.9f, 0.72f + 0.05f * (float) i, KRun, 2 + i);
    }
}

void ChamberProcessor::startNote (const Ev& e)
{
    // the same part on the same note: let the old one go first
    for (auto& v : voices) if (v.active() && v.note == e.note && v.part == e.part) v.release();
    Voice* free = nullptr;
    for (auto& v : voices) if (! v.active()) { free = &v; break; }
    if (free == nullptr) { for (auto& v : voices) if (! v.held && (free == nullptr || v.age > free->age)) free = &v; }
    if (free == nullptr) for (auto& v : voices) if (free == nullptr || v.age > free->age) free = &v;
    // every player is a touch out of tune with the others when WARP is up
    const float detune = (warp > 0.001f ? rng.n() * 22.0f * warp : rng.n() * 3.0f) + (ensembleIsTape (ensemble) ? rng.n() * 6.0f : 0.0f);
    free->start (lib, e.instr, e.art, e.note, e.vel, e.len, e.attack, detune, ensembleIsTape (ensemble), rng);
    free->part = e.part;
    free->pan = e.part < 4 ? kPan[e.part] : 0.0f;
    free->gain = e.instr == TIM ? 1.1f : 1.0f;
    if (e.glide != 0) free->glide (e.glide, e.glideSec);
}

void ChamberProcessor::releaseParts (int mask)
{
    for (auto& v : voices) if (v.active() && v.held && ((mask >> v.part) & 1)) v.release();
}

void ChamberProcessor::playStep (int64_t k, int64_t at)
{
    const int64_t b = k / bs;
    const int s16 = (int) (((k % bs) + bs) % bs);
    const int N = bs;
    if (b != curBar)
    {
        curBar = b;
        bool lastBar = false;
        const int prevSec = barSec;
        barSec = sectionAt (b, barIn, lastBar);
        uiSection = barSec;
        if (led) minorKey = band::qualityIsMinor (leader.progMinor[0][0]);
        else { const auto& pr = ut::progressions()[(size_t) juce::jlimit (0, (int) ut::progressions().size() - 1, progs[0])].chords; minorKey = ! pr.empty() && band::qualityIsMinor (pr[0].quality()); }
        if (phrase[barSec].empty() || (barIn == 0 && prevSec != barSec)) writePhrase (barSec);
        chordAt (b, curRoot, curQ);
        const bool fill = (led && leader.fillAt (b)) || (! led && autoSong && lastBar);
        writeBar (barSec, barIn, fill);
        // the breakdown: everyone holds the chord
        if (led && leader.isHalf (b)) { bar.clear(); bar.push_back ({ 0, 15, ASus, (float) N, 0.6f, KChord, 0 }); }
        // the rise: the last bar before a new section (two bars when sections are long)
        const bool riseBar = feel == FRise && (fill || (! led && autoSong && lastBar));
        if (riseBar && ! (led && leader.rests (b, false, true)) && ! (led && leader.isEnding (b)))
        {
            rising = true;
            releaseParts (15);
            const float riseSec = (float) (60.0 / bpmNow * N / 4.0);
            for (int p = 0; p < 4; ++p)
                for (int dbl = 0; dbl < 2; ++dbl)
                {
                    int lo = 0, hi = 0; instrRange (parts[p], lo, hi);
                    const int startN = lo + rng.i (7) + dbl * 5;
                    const float up = (float) (juce::jmin (hi, startN + 26) - startN) * (0.8f + 0.2f * rng.u());
                    schedule ({ at + (int64_t) (rng.u() * 0.1 * sr), 1, p, parts[p], ASus, startN, 0.75f + 0.2f * dynamics, riseSec * 1.02f, riseSec * 0.85f, up, riseSec });
                }
            if (timpani) for (int r = 0; r < N / 2; ++r) schedule ({ at + (int64_t) ((N / 2 + r) * sr * 15.0 / bpmNow), 1, 4, TIM, ASus, 41 + ((curRoot - 5 + 12) % 12), 0.4f + 0.06f * r, 0.4f, 0.002f });
        }
        else if (rising) { rising = false; schedule ({ at, 3, 15, 0, 0, 0, 0, 0 }); }
        if (timpani && barIn == 0 && b > 0 && ! (led && (leader.rests (b, false, true) || leader.isEnding (b))))
        {
            int tn = 41 + ((curRoot - 5 + 12) % 12); while (tn > 55) tn -= 12;
            schedule ({ at, 1, 4, TIM, ASus, tn, 0.8f + 0.15f * dynamics, 2.5f, 0.002f });
        }
        if (fill && timpani && feel != FRise)
            for (int r = 0; r < 4; ++r) schedule ({ at + (int64_t) ((N - 4 + r) * sr * 15.0 / bpmNow), 1, 4, TIM, ASus, 41 + ((curRoot - 5 + 12) % 12), 0.5f + 0.1f * r, 0.4f, 0.002f });
    }
    if (rising) return;
    // the song's last chord: every part holds it, the timpani lands it
    if (led && leader.isEnding (b))
    {
        if (leader.isFinalHit (b) && s16 == 0)
        {
            int root = curRoot, q = curQ;
            chordAt (b, root, q);
            if (root >= 0)
            {
                schedule ({ at, 3, 31, 0, 0, 0, 0, 0 });
                voiceChord (root, q);
                const float lenSec = (float) (2.0 * N * 15.0 / juce::jmax (30.0, bpmNow) * 0.95);
                for (int p = 0; p < 4; ++p) schedule ({ at + 2 + p * 30, 1, p, parts[p], ASus, fitTo (parts[p], voicing[p]), 0.82f, lenSec, 0.02f });
                if (timpani) { int tn = 41 + ((root - 5 + 12) % 12); while (tn > 55) tn -= 12; schedule ({ at, 1, 4, TIM, ASus, tn, 0.95f, 3.0f, 0.002f }); }
                lastRoot = root; lastQ = q;
            }
        }
        return;
    }
    if (led && leader.rests (b, false, true)) { if (! outNow) { schedule ({ at, 3, 31, 0, 0, 0, 0, 0 }); outNow = true; } return; }
    outNow = false;
    if (follow == FMidiChords && midiRoot < 0) return;
    int root = curRoot, q = curQ;
    chordAt (b, root, q);
    if (root < 0) return;
    uiChord = root | (q << 4);
    const bool changed = root != lastRoot || q != lastQ;
    const bool holdFeel = feel == FLong || feel == FTremolo || feel == FRise || feel == FCounter || feel == FDescend || feel == FRiff;
    bool any = false; for (auto& h : bar) if (h.step == s16) any = true;
    if (changed)
    {
        voiceChord (root, q);
        // a held chord moves with the harmony, mid-bar too
        if (holdFeel && ! any && s16 != 0)
        {
            const int mask = feel == FCounter ? 14 : (feel == FDescend ? 7 : (feel == FRiff ? 3 : 15));
            schedule ({ at, 3, mask, 0, 0, 0, 0, 0 });
            for (int p = 0; p < 4; ++p) if ((mask >> p) & 1)
                schedule ({ at + 2 + p * 30, 1, p, parts[p], feel == FTremolo ? ATrem : ASus, fitTo (parts[p], voicing[p]), 0.6f, -1.0f, 0.08f });
            return;
        }
    }
    if (! any) return;

    const double sixteenth = sr * 15.0 / juce::jmax (30.0, bpmNow);
    const float energy = led ? (leader.energy - 0.5f) * 0.25f : 0.0f;
    double offMs = rng.n() * (2.0 + 6.0 * human);
    if (s16 % 2 == 1 && meterNow != band::M68) offMs += swing * sixteenth / sr * 1000.0 * 0.66;
    for (const auto& h : bar)
    {
        if (h.step != s16) continue;
        const bool holds = h.len >= (float) N - 1.0f && (h.art == ASus || h.art == ATrem);
        if (holds && ! changed && barIn % 4 != 0 && h.kind == KChord) continue;     // same chord: the bows keep going
        const float vel = juce::jlimit (0.08f, 1.0f, h.vel * (0.55f + 0.6f * dynamics) + rng.n() * 0.05f * human + energy);
        const float lenSec = holds ? -1.0f : (float) (h.len * lengthK * sixteenth / sr);
        const float attack = h.art == ASus || h.art == ATrem ? 0.015f + swell * (holds ? 0.9f : 0.25f) : (h.art == APizz ? 0.002f : 0.004f);
        const int64_t t0 = juce::jmax (sampleClock, at + (int64_t) (offMs * sr / 1000.0));
        if (holds) schedule ({ t0 - 1 < sampleClock ? sampleClock : t0 - 1, 3, h.parts, 0, 0, 0, 0, 0 });
        for (int p = 0; p < 4; ++p)
        {
            if (! ((h.parts >> p) & 1)) continue;
            int note = voicing[p];
            switch (h.kind)
            {
                case KMelody: case KUnison:
                {
                    const auto& ph = phrase[barSec];
                    int n = scaleNote (ph[(size_t) juce::jlimit (0, (int) ph.size() - 1, h.arg)].second);
                    if (ph[(size_t) juce::jlimit (0, (int) ph.size() - 1, h.arg)].first % 8 == 0)
                    {
                        // land on the chord on the strong beats
                        int t[4]; const int nt = band::qualityTones (q, t); int best = n, bd = 99;
                        for (int o = -1; o <= 1; ++o) for (int i = 0; i < nt; ++i) { const int c = root + t[i] + 12 * ((n - root) / 12 + o); if (std::abs (c - n) < bd) { bd = std::abs (c - n); best = c; } }
                        n = best;
                    }
                    note = h.kind == KUnison ? n - 12 * p + (p == 3 ? 12 : 0) : n;
                    break;
                }
                case KBass: note = voicing[3] + h.arg; break;
                case KRiff: note = (p == 3 ? voicing[3] : voicing[3] + 12) + h.arg; break;
                case KRun: note = scaleNote (h.arg); break;
                default: break;
            }
            note = fitTo (parts[p], note);
            schedule ({ t0 + (int64_t) (p * (0.002 + 0.004 * human) * sr), 1, p, parts[p], h.art, note, vel * (p == 0 ? 1.0f : 0.92f), lenSec, attack });
        }
    }
}

void ChamberProcessor::runPlayer (int n)
{
    double ppq = 0, bpm = 120; bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto p = pos->getPpqPosition()) ppq = *p; else playing = false;
        }
    bpmNow = bpm; uiTransport = playing; blockPpq = ppq;
    {
        const int m = led ? leader.meter : band::hostMeter (getPlayHead());
        if (m != meterNow) { meterNow = m; curBar = -1; for (auto& ph : phrase) ph.clear(); }
        bs = band::meterSteps (meterNow);
        capture.setMeter (meterNow);
    }
    if (! playing) { if (wasPlaying) schedule ({ sampleClock, 3, 31, 0, 0, 0, 0, 0 }); wasPlaying = false; return; }
    const double spb = sr * 60.0 / bpm;
    if (! wasPlaying || ppq < lastPpqEnd - 0.02 || ppq > lastPpqEnd + 0.25)
    {
        lastStep = (int64_t) std::ceil (ppq * 4.0 - 0.001) - 1;
        curBar = -1; lastRoot = -1; rising = false;
        if (wasPlaying) schedule ({ sampleClock, 3, 31, 0, 0, 0, 0, 0 });
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

void ChamberProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    buffer.clear();
    readParams();
    band::markPresent (band::WChamber);
    const bool wasLed = led;
    led = follow != FPlay && band::readLeader (leader);
    if (led && leader.mutateStamp != lastMutate) { lastMutate = leader.mutateStamp; rng.seed (leader.mutateStamp * 2654435761u + 17u); curBar = -1; for (auto& ph : phrase) ph.clear(); }
    uiLinked = led;
    if (led != wasLed) lastPpqEnd = -1e9;

    // MIDI in: chords to follow, or notes to play
    bool keysChanged = false;
    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        const int64_t at = sampleClock + m.samplePosition;
        if (msg.isNoteOn())
        {
            held[(size_t) msg.getNoteNumber()] = true; keysChanged = true;
            if (follow == FPlay)
            {
                // the note goes to whichever part of the ensemble plays that range best
                const int nn = msg.getNoteNumber(); int bestP = 0, bd = 999;
                for (int p = 0; p < 4; ++p) { int lo = 0, hi = 0; instrRange (parts[p], lo, hi); const int d = nn < lo ? (lo - nn) * 3 : nn > hi ? (nn - hi) * 3 : std::abs (nn - (lo + hi) / 2) / 4; if (d < bd) { bd = d; bestP = p; } }
                schedule ({ at, 1, bestP, parts[bestP], playArt, nn, msg.getFloatVelocity(), playArt == ASus || playArt == ATrem ? -1.0f : 0.5f, 0.02f + swell * 0.4f });
            }
        }
        else if (msg.isNoteOff())
        {
            held[(size_t) msg.getNoteNumber()] = false; keysChanged = true;
            if (follow == FPlay) schedule ({ at, 2, 0, 0, 0, msg.getNoteNumber(), 0, 0 });
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff()) { held.fill (false); schedule ({ at, 3, 31, 0, 0, 0, 0, 0 }); }
    }
    if (keysChanged && follow == FMidiChords)
    {
        int pcMask = 0, lowest = -1;
        for (int i = 0; i < 128; ++i) if (held[(size_t) i]) { pcMask |= 1 << (i % 12); if (lowest < 0) lowest = i; }
        if (pcMask == 0) midiRoot = -1;
        else
        {
            int bestScore = -100;
            for (int r = 0; r < 12; ++r) for (int q = 0; q < band::NumQualities; ++q)
            {
                int t[4]; const int nt = band::qualityTones (q, t); int sc = 0, tm = 0;
                for (int i = 0; i < nt; ++i) { const int pc = (r + t[i]) % 12; tm |= 1 << pc; sc += (pcMask >> pc) & 1 ? 3 : -2; }
                for (int pc = 0; pc < 12; ++pc) if (((pcMask >> pc) & 1) && ! ((tm >> pc) & 1)) sc -= 2;
                if (r == lowest % 12) sc += 2;
                if (nt == 3) sc += 1;
                if (sc > bestScore) { bestScore = sc; midiRoot = r; midiQ = q; }
            }
        }
    }
    midi.clear();
    if (follow != FPlay && ! sitOut && ! (led && leader.off (band::WChamber))) runPlayer (n);
    else
    {
        double ppq = 0, bpm = 120; bool playing = false;
        if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) { playing = pos->getIsPlaying(); if (auto b = pos->getBpm()) bpm = *b; if (auto p = pos->getPpqPosition()) ppq = *p; }
        bpmNow = bpm; blockPpq = ppq; uiTransport = playing && follow == FPlay;
        if (wasPlaying) { schedule ({ sampleClock, 3, 31, 0, 0, 0, 0, 0 }); wasPlaying = false; }
    }

    std::stable_sort (queue.begin(), queue.end(), [] (const Ev& a, const Ev& b) { return a.at < b.at; });
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : buffer.getWritePointer (0);
    size_t e = 0;
    for (int i = 0; i < n; ++i)
    {
        const int64_t now = sampleClock + i;
        while (e < queue.size() && queue[e].at <= now)
        {
            const auto& ev = queue[e++];
            if (ev.type == 1)
            {
                startNote (ev);
                midiOut.on (midi, i, now, ev.note, ev.vel, ev.len, sr, ev.part + 1);
            }
            else if (ev.type == 2)
            {
                for (auto& v : voices) if (v.active() && v.held && v.note == ev.note) { midiOut.off (midi, i, v.note, v.part + 1); v.release(); }
            }
            else
            {
                for (auto& v : voices) if (v.active() && v.held && ((ev.part >> v.part) & 1)) midiOut.off (midi, i, v.note, v.part + 1);
                releaseParts (ev.part);
            }
        }
        midiOut.tick (midi, i, now);
        float l = 0, r = 0;
        for (auto& v : voices) v.render (l, r);
        L[i] = l * 0.3f; R[i] = r * 0.3f;
    }
    queue.erase (queue.begin(), queue.begin() + (long) e);
    for (auto& q : queue) if (q.at < sampleClock + n) q.at = sampleClock + n;
    rack.process (L, R, n, rng);
    for (int i = 0; i < n; ++i)
        for (float* x : { &L[i], &R[i] })
        {
            if (! std::isfinite (*x)) *x = 0;
            if (std::abs (*x) > 0.9f) *x = std::copysign (0.9f + 0.1f * std::tanh ((std::abs (*x) - 0.9f) * 10.0f), *x);
        }
    fade.apply (buffer, n, led, leader, getPlayHead());   // Bandleader's fade-out
    // what each part is playing, for the screen
    int pn[5] { -1, -1, -1, -1, -1 };
    for (auto& v : voices) if (v.active() && v.held && v.part >= 0 && v.part < 5) pn[v.part] = v.note;
    for (int p = 0; p < 5; ++p) uiPartNote[p] = pn[p];
    {
        double cp = 0; bool cpl = false;
        if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) { cpl = pos->getIsPlaying(); if (auto q = pos->getPpqPosition()) cp = *q; else cpl = false; }
        capture.recordBlock (midi, cp, cpl, bpmNow, sr, n);
    }
    sampleClock += n;
}

void ChamberProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void ChamberProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

void ChamberProcessor::applyStyle (const band::StyleDef& s)
{
    band::setParam (apvts, "sit_out", s.cOn ? 0.0f : 1.0f);
    if (! s.cOn) return;
    band::setParam (apvts, "ensemble", (float) s.cEns);
    band::setParam (apvts, "feel", (float) s.cFeel);
    band::setParam (apvts, "busy", s.cBusy);
    band::setParam (apvts, "warp", s.cWarp);
    band::setParam (apvts, "timpani", s.cTimp ? 1.0f : 0.0f);
    band::setParam (apvts, "room", s.cRoom);
}

juce::AudioProcessorEditor* ChamberProcessor::createEditor() { return new ChamberEditor (*this); }

#ifndef CHAMBER_NO_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ChamberProcessor(); }
#endif
