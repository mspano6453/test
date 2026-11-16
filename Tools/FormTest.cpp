// Offline tests for the song form (count-in, intro, breakdown, key change, ending, 3/4 and 6/8),
// for teaching Bandleader a song (keyboard and guitar), and for rendering the whole song to MIDI files.
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Bandleader/BandleaderProcessor.h"
#include "../Source/PluginProcessor.h"
#include "../Undertow/UndertowProcessor.h"
#include "../Parlour/ParlourProcessor.h"
#include "../Chamber/ChamberProcessor.h"
#include "../Jangle/JangleProcessor.h"
#include "../Harmonia/HarmoniaProcessor.h"

namespace
{
struct Head : juce::AudioPlayHead
{
    double ppq = 0, bpm = 120; bool playing = true; int num = 4, den = 4;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p; p.setIsPlaying (playing); p.setPpqPosition (ppq); p.setBpm (bpm);
        p.setTimeSignature (juce::AudioPlayHead::TimeSignature { num, den });
        return p;
    }
};
void setP (juce::AudioProcessorValueTreeState& s, const char* id, float v) { auto* p = s.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); }
float val (juce::AudioProcessorValueTreeState& s, const char* id) { return s.getParameter (id)->convertFrom0to1 (s.getParameter (id)->getValue()); }
int fails = 0;
void check (bool ok, const juce::String& what) { std::printf ("  [%s] %s\n", ok ? " ok " : "FAIL", what.toRawUTF8()); if (! ok) ++fails; }

struct KS
{
    std::vector<float> d; int i = 0;
    void pluck (double f, double sr, float amp, juce::Random& r) { d.assign ((size_t) std::max (2.0, sr / f), 0.0f); for (auto& x : d) x = (r.nextFloat() * 2 - 1) * amp; i = 0; }
    float tick() { if (d.empty()) return 0; const size_t n = d.size(), j = (size_t) (i + 1) % n; const float y = d[(size_t) i]; d[(size_t) i] = 0.996f * 0.5f * (d[(size_t) i] + d[j]); i = (int) j; return y; }
};

// ---------------------------------------------------------------------------
void formTest (int meterSel, const char* label)
{
    std::printf ("\n== SONG FORM in %s\n", label);
    const double sr = 44100; const int bs = 256;
    BandleaderProcessor lead; TeaTowelProcessor drums; UndertowProcessor bass; ParlourProcessor keys; JangleProcessor gtr; HarmoniaProcessor vox; ChamberProcessor orch;
    Head head; head.bpm = 120; head.playing = false;
    if (meterSel == 2) { head.num = 3; head.den = 4; } else if (meterSel == 3) { head.num = 6; head.den = 8; }
    struct P { juce::AudioProcessor* p; int ins; const char* name; };
    const P all[] = { { &lead, 2, "lead" }, { &drums, 0, "drums" }, { &bass, 2, "bass" }, { &keys, 0, "keys" }, { &gtr, 0, "guitar" }, { &vox, 0, "vox" }, { &orch, 0, "orch" } };
    for (auto& x : all) { x.p->setPlayConfigDetails (x.ins, 2, sr, bs); x.p->prepareToPlay (sr, bs); x.p->setPlayHead (&head); }
    setP (lead.apvts, "meter", (float) meterSel);
    setP (lead.apvts, "key", 0); setP (lead.apvts, "prog_verse", 10); setP (lead.apvts, "prog_chorus", 8); setP (lead.apvts, "prog_bridge", 11);
    setP (lead.apvts, "section_bars", 0);    // 4 bars
    setP (lead.apvts, "count_in", 1); setP (lead.apvts, "intro", 2); setP (lead.apvts, "breakdown", 1); setP (lead.apvts, "key_up", 2); setP (lead.apvts, "ending", 1);
    setP (orch.apvts, "sit_out", 0);
    lead.timerCallback();
    const auto song = lead.currentSong();
    std::printf ("  the song: %d bars:", (int) song.length);
    for (auto& g : song.segs) std::printf (" [%s%s%s x%d]", g.part == band::PCount ? "count" : g.part == band::PIntro ? "intro" : g.part == band::PEnd ? "END" : g.part == band::PHalf ? "half " : "",
                                           g.part == band::PNone || g.part == band::PHalf ? (g.sec == 0 ? "V" : g.sec == 1 ? "C" : "B") : "", g.key ? (" +" + juce::String (g.key)).toRawUTF8() : "", (int) g.bars);
    std::printf ("\n");
    const int meter = meterSel == 1 ? band::M44 : meterSel == 2 ? band::M34 : band::M68;
    const double barQ = band::meterBarPpq (meter);
    const double spb = sr * 60.0 / head.bpm;
    const int bars = (int) song.length + 3;
    const int total = (int) (spb * barQ * bars) / bs * bs;
    // note-ons per player per bar, and the Bandleader's chord roots per bar
    std::vector<std::array<int, 7>> ons ((size_t) bars + 1);
    std::vector<int> drumOther ((size_t) bars + 1, 0), chordRoot ((size_t) bars + 1, -1);
    std::vector<double> lastOn (7, -1);
    juce::AudioBuffer<float> b (2, bs), mix (2, total);
    head.playing = true;
    bool nan = false;
    for (int pos = 0; pos < total; pos += bs)
    {
        head.ppq = pos / spb;
        for (int k = 0; k < 7; ++k)
        {
            b.clear(); juce::MidiBuffer m;
            all[k].p->processBlock (b, m);
            if (k > 0) for (int c = 0; c < 2; ++c) mix.addFrom (c, pos, b, c, 0, bs, 0.7f);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < bs; ++i) if (! std::isfinite (b.getSample (c, i))) nan = true;
            for (const auto ev : m)
            {
                const auto& msg = ev.getMessage();
                if (! msg.isNoteOn()) continue;
                const double q = head.ppq + ev.samplePosition / spb;
                const int bar = (int) std::floor (q / barQ + 0.03 * head.bpm / 60.0 / barQ);   // (a note pushed up to 30 ms ahead of the bar line belongs to the new bar)
                if (bar < 0 || bar > bars) continue;
                ++ons[(size_t) bar][(size_t) k];
                lastOn[(size_t) k] = q;
                if (k == 1 && msg.getNoteNumber() != 37) ++drumOther[(size_t) bar];
                if (k == 0 && msg.getNoteNumber() < 60 && chordRoot[(size_t) bar] < 0) chordRoot[(size_t) bar] = msg.getNoteNumber() % 12;
            }
        }
    }
    if (std::getenv ("FORM_TABLE"))
        for (int i = 0; i <= bars; ++i)
            std::printf ("    bar %2d  lead %2d drums %2d (other %2d) bass %2d keys %2d gtr %3d vox %2d orch %2d  root %d\n", i, ons[(size_t) i][0], ons[(size_t) i][1], drumOther[(size_t) i],
                         ons[(size_t) i][2], ons[(size_t) i][3], ons[(size_t) i][4], ons[(size_t) i][5], ons[(size_t) i][6], chordRoot[(size_t) i]);
    auto sum = [&] (int from, int to, int k) { int n = 0; for (int i = from; i < to; ++i) n += ons[(size_t) i][(size_t) k]; return n; };
    // where the parts are
    int64_t verseAt = -1, halfAt = -1, lastChorusAt = -1, endAt = -1;
    for (auto& g : song.segs)
    {
        if (g.part == band::PNone && g.sec == band::Verse && verseAt < 0) verseAt = g.start;
        if (g.part == band::PHalf && halfAt < 0) halfAt = g.start;
        if (g.part == band::PNone && g.sec == band::Chorus && g.key != 0 && lastChorusAt < 0) lastChorusAt = g.start;
        if (g.part == band::PEnd) endAt = g.start;
    }
    check (ons[0][1] > 0 && drumOther[0] == 0, "count-in bar: the drummer clicks sticks only (" + juce::String (ons[0][1]) + " clicks)");
    check (sum (0, 1, 2) + sum (0, 1, 3) + sum (0, 1, 4) + sum (0, 1, 5) + sum (0, 1, 6) == 0, "count-in bar: nobody else plays");
    check (sum (1, 3, 3) > 0 && sum (1, 3, 4) > 0, "intro, first half: keys and guitar play");
    check (sum (1, 3, 1) == 0 && sum (1, 3, 2) == 0 && sum (1, 3, 5) == 0, "intro, first half: drums, bass and singers wait");
    check (sum (3, 5, 1) > 0 && sum (3, 5, 2) > 0 && sum (3, 5, 5) == 0, "intro, second half: drums and bass join, singers still wait");
    check (verseAt == 5 && sum ((int) verseAt, (int) verseAt + 4, 5) > 0 && sum ((int) verseAt, (int) verseAt + 4, 6) > 0, "verse: the singers and the orchestra come in");
    if (halfAt >= 0)
    {
        const int dHalf = sum ((int) halfAt, (int) halfAt + 2, 1), dBefore = sum ((int) halfAt - 2, (int) halfAt, 1);
        check (dHalf < dBefore, "breakdown: the drums thin out (" + juce::String (dHalf) + " hits vs " + juce::String (dBefore) + " before)");
        check (sum ((int) halfAt, (int) halfAt + 2, 2) <= 2, "breakdown: the bass plays one long note a bar");
    }
    // chorus "I - V - vi - IV" in C, up 2 -> D A Bm G
    check (lastChorusAt > 0 && chordRoot[(size_t) lastChorusAt] == 2, "key change: the last chorus starts on D (heard " + juce::String (lastChorusAt > 0 ? chordRoot[(size_t) lastChorusAt] : -1) + ")");
    check (endAt > 0 && ons[(size_t) endAt][1] > 0 && ons[(size_t) endAt][2] > 0 && ons[(size_t) endAt][3] > 0 && ons[(size_t) endAt][4] > 0 && ons[(size_t) endAt][5] > 0,
           "big ending: everyone hits the last chord together");
    check (endAt > 0 && chordRoot[(size_t) endAt] == 2, "big ending: on the home chord (D after the key change)");
    int after = 0; for (int i = (int) endAt + 1; i <= bars; ++i) for (int k = 0; k < 7; ++k) after += ons[(size_t) i][(size_t) k];
    check (after == 0, "after the last chord: silence (" + juce::String (after) + " notes)");
    check (! nan, "no NaN in any output");
    // the meter: every drum hit lands inside a bar of the right length (bars counted in this meter)
    const int sw = meter == band::M44 ? 16 : 12;
    int offGrid = 0;
    for (int i = (int) verseAt; i < (int) verseAt + 4; ++i) if (ons[(size_t) i][1] == 0) ++offGrid;
    check (offGrid == 0, juce::String ("drums play every bar of the verse in ") + band::meterName (meter) + " (" + juce::String (sw) + " steps a bar)");
    auto f = juce::File::getCurrentWorkingDirectory().getChildFile (juce::String ("form_") + label + ".wav"); f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (mix, 0, total);
    std::printf ("  peak %.2f\n", mix.getMagnitude (0, total));
}

// ---------------------------------------------------------------------------
void learnKeys()
{
    std::printf ("\n== LEARN THE SONG from a keyboard (4/4)\n");
    const double sr = 44100; const int bs = 256;
    BandleaderProcessor lead; Head head; head.playing = false;
    lead.setPlayConfigDetails (2, 2, sr, bs); lead.prepareToPlay (sr, bs); lead.setPlayHead (&head);
    setP (lead.apvts, "meter", 1);
    lead.timerCallback();
    const double spb = sr * 60.0 / head.bpm;
    // verse: C Am F G | chorus: F G C C | bridge: Am F  (chords on a keyboard, C4 and up)
    struct B { int root; bool minor; };
    const B verse[] = { { 0, false }, { 9, true }, { 5, false }, { 7, false } }, chorus[] = { { 5, false }, { 7, false }, { 0, false }, { 0, false } }, bridge[] = { { 9, true }, { 5, false } };
    std::vector<B> plan;
    for (auto& x : verse) plan.push_back (x);
    for (auto& x : chorus) plan.push_back (x);
    for (auto& x : bridge) plan.push_back (x);
    juce::AudioBuffer<float> buf (2, bs);
    lead.command (BandleaderProcessor::CLearn);
    { juce::MidiBuffer m; buf.clear(); lead.processBlock (buf, m); }
    head.playing = true;
    const int total = (int) (spb * 4 * 12);
    int lastBar = -1; std::vector<int> heldNotes;
    for (int pos = 0; pos < total; pos += bs)
    {
        head.ppq = pos / spb;
        const int bar = (int) (head.ppq / 4.0);
        juce::MidiBuffer m;
        if (bar != lastBar)
        {
            for (int nn : heldNotes) m.addEvent (juce::MidiMessage::noteOff (1, nn), 0);
            heldNotes.clear();
            if (bar < (int) plan.size())
            {
                const auto& c = plan[(size_t) bar];
                const int r = 60 + c.root;
                for (int nn : { r - 12, r, r + (c.minor ? 3 : 4), r + 7 }) { m.addEvent (juce::MidiMessage::noteOn (1, nn, (juce::uint8) 90), 2); heldNotes.push_back (nn); }
            }
            // the footswitch (CC 90 = next section, CC 100 = learn/done) just after the bar line
            if (bar == 4 || bar == 8) { m.addEvent (juce::MidiMessage::controllerEvent (1, 90, 127), 20); }
            if (bar == 10) m.addEvent (juce::MidiMessage::controllerEvent (1, 100, 127), 20);
            lastBar = bar;
        }
        else { m.addEvent (juce::MidiMessage::controllerEvent (1, 90, 0), 0); m.addEvent (juce::MidiMessage::controllerEvent (1, 100, 0), 0); }
        buf.clear(); lead.processBlock (buf, m);
    }
    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
    const int key = (int) val (lead.apvts, "key");
    check (key == 0, "key: C (" + juce::String (key) + ")");
    check (lead.learnedChords (0, key) == "C Am F G", "verse learned: " + lead.learnedChords (0, key));
    check (lead.learnedChords (1, key) == "F G C C", "chorus learned: " + lead.learnedChords (1, key));
    check (lead.learnedChords (2, key) == "Am F", "bridge learned: " + lead.learnedChords (2, key));
    check ((int) val (lead.apvts, "prog_verse") == lead.learnedIndex() && (int) val (lead.apvts, "section_bars") == 4, "the chord menus and section length switched to Learned");
    const auto f = lead.currentSpec();
    check (f.secBars[0] == 4 && f.secBars[1] == 4 && f.secBars[2] == 2, "the song uses the learned lengths (" + juce::String (f.secBars[0]) + "/" + juce::String (f.secBars[1]) + "/" + juce::String (f.secBars[2]) + ")");
    // and it survives saving the set
    juce::MemoryBlock mb; lead.getStateInformation (mb);
    BandleaderProcessor lead2; lead2.setStateInformation (mb.getData(), (int) mb.getSize());
    check (lead2.learnedChords (0, 0) == "C Am F G", "learned chords are saved with the set");
}

void learnGuitar()
{
    std::printf ("\n== LEARN THE SONG from a guitar (sidechain)\n");
    const double sr = 44100; const int bs = 256;
    BandleaderProcessor lead; Head head; head.playing = false; head.bpm = 100;
    lead.setPlayConfigDetails (2, 2, sr, bs); lead.prepareToPlay (sr, bs); lead.setPlayHead (&head);
    setP (lead.apvts, "meter", 1);
    lead.timerCallback();
    const double spb = sr * 60.0 / head.bpm;
    // verse: E A D A (strummed open chords), then DONE
    const int shapes[4][6] = { { 40, 47, 52, 56, 59, 64 }, { 45, 52, 57, 61, 64, -1 }, { 50, 57, 62, 66, -1, -1 }, { 45, 52, 57, 61, 64, -1 } };
    std::vector<KS> str (6); juce::Random rnd (3);
    juce::AudioBuffer<float> buf (2, bs);
    lead.command (BandleaderProcessor::CLearn);
    { juce::MidiBuffer m; buf.clear(); lead.processBlock (buf, m); }
    head.playing = true;
    const int total = (int) (spb * 4 * 6);
    int64_t s = 0;
    for (int pos = 0; pos < total; pos += bs)
    {
        head.ppq = pos / spb;
        const int bar = (int) (head.ppq / 4.0);
        juce::MidiBuffer m;
        if (bar == 4 && std::fmod (head.ppq, 4.0) < bs / spb) m.addEvent (juce::MidiMessage::controllerEvent (1, 100, 127), 10);
        for (int i = 0; i < bs; ++i, ++s)
        {
            const double q = (pos + i) / spb;
            // a strum on every beat
            if (bar < 4 && std::abs (q - std::round (q)) * spb < 0.5)
            {
                for (int k = 0; k < 6; ++k) if (shapes[bar][k] > 0) str[(size_t) k].pluck (440.0 * std::pow (2.0, (shapes[bar][k] - 69) / 12.0), sr, 0.2f, rnd);
            }
            float v = 0; for (auto& x : str) v += x.tick();
            buf.setSample (0, i, v); buf.setSample (1, i, v);
        }
        lead.processBlock (buf, m);
    }
    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
    const int key = (int) val (lead.apvts, "key");
    check (lead.learnedChords (0, key) == "E A D A", "verse learned from the guitar: " + lead.learnedChords (0, key) + " (key " + ut::pcName (key) + ")");
}

// ---------------------------------------------------------------------------
void renderTest()
{
    std::printf ("\n== RENDER SONG to MIDI files (6/8)\n");
    const double sr = 44100; const int bs = 512;
    BandleaderProcessor lead; TeaTowelProcessor drums; UndertowProcessor bass; ParlourProcessor keys; JangleProcessor gtr; HarmoniaProcessor vox; ChamberProcessor orch;
    Head head; head.playing = false; head.bpm = 90; head.num = 6; head.den = 8;
    juce::AudioProcessor* all[] = { &lead, &drums, &bass, &keys, &gtr, &vox, &orch };
    for (auto* p : all) { p->setPlayConfigDetails (p == &lead || p == &bass ? 2 : 0, 2, sr, bs); p->prepareToPlay (sr, bs); p->setPlayHead (&head); }
    setP (lead.apvts, "count_in", 1); setP (lead.apvts, "intro", 1); setP (lead.apvts, "ending", 1); setP (lead.apvts, "key_up", 1);
    setP (lead.apvts, "section_bars", 0);
    setP (orch.apvts, "sit_out", 0);
    lead.timerCallback();
    // a few blocks so everyone is on the bus
    for (int i = 0; i < 8; ++i) for (auto* p : all) { juce::AudioBuffer<float> b (2, bs); juce::MidiBuffer m; p->processBlock (b, m); }
    const auto song = lead.currentSong();
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    check (lead.startRender(), "render posted (" + juce::String ((int) song.length) + " bars, folder " + lead.renderFolder.getFileName() + ")");
    const band::Who who[] = { band::WBandleader, band::WTeaTowel, band::WUndertow, band::WParlour, band::WJangle, band::WHarmonia, band::WChamber };
    bool allDone = false;
    for (int it = 0; it < 1200 && ! allDone; ++it)
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        for (auto* p : all) { juce::AudioBuffer<float> b (2, bs); juce::MidiBuffer m; p->processBlock (b, m); }   // keep the live band on the bus
        allDone = true;
        for (auto w : who) { const int st = band::renderState (w); if (st == band::RIdle || st == band::RBusy) allDone = false; }
    }
    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
    check (allDone, "every player rendered its part in " + juce::String ((juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0, 1) + " s");
    const auto files = lead.renderFolder.findChildFiles (juce::File::findFiles, false, "*.mid");
    check (files.size() == 7, juce::String (files.size()) + " MIDI files");
    const double songQ = song.length * band::meterBarPpq (band::M68);
    for (auto& f : files)
    {
        juce::FileInputStream is (f); juce::MidiFile mf; mf.readFrom (is);
        const auto* tr = mf.getTrack (0);
        int notes = 0, num = 0, den = 0; double last = 0, first = 1e9;
        for (auto* e : *tr)
        {
            if (e->message.isNoteOn()) { ++notes; last = juce::jmax (last, e->message.getTimeStamp()); first = juce::jmin (first, e->message.getTimeStamp()); }
            if (e->message.isTimeSignatureMetaEvent()) e->message.getTimeSignatureInfo (num, den);
        }
        const double lastQ = last / 960.0, firstQ = first / 960.0;
        check (notes > 10 && num == 6 && den == 8 && lastQ <= songQ + 0.01,
               f.getFileName() + ": " + juce::String (notes) + " notes, first at beat " + juce::String (firstQ, 2) + ", last at " + juce::String (lastQ, 1) + " of " + juce::String (songQ, 1)
               + ", " + juce::String (num) + "/" + juce::String (den));
    }
}
}

// how loud each guitar and each group of singers is (and a WAV of each to listen to)
void levels()
{
    const double sr = 44100; const int bs = 512;
    auto run = [&] (juce::AudioProcessor& p, const juce::String& name, int meterNum)
    {
        Head head; head.playing = true; head.bpm = 110; head.num = meterNum; head.den = meterNum == 6 ? 8 : 4;
        p.setPlayConfigDetails (0, 2, sr, bs); p.prepareToPlay (sr, bs); p.setPlayHead (&head);
        const double spb = sr * 60.0 / head.bpm; const int total = (int) (spb * 4 * 8) / bs * bs;
        juce::AudioBuffer<float> all (2, total), b (2, bs);
        double e = 0; float pk = 0;
        for (int pos = 0; pos < total; pos += bs)
        {
            head.ppq = pos / spb; b.clear(); juce::MidiBuffer m; p.processBlock (b, m);
            for (int c = 0; c < 2; ++c) all.copyFrom (c, pos, b, c, 0, bs);
            e += b.getRMSLevel (0, 0, bs); pk = juce::jmax (pk, b.getMagnitude (0, bs));
        }
        std::printf ("  %-34s rms %.3f peak %.2f\n", name.toRawUTF8(), e / (total / bs), pk);
        auto f = juce::File::getCurrentWorkingDirectory().getChildFile ("lvl_" + name.replaceCharacters (" /&()", "_____") + ".wav"); f.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0));
        w->writeFromAudioSampleBuffer (all, 0, total);
    };
    std::printf ("\n== LEVELS: Jangle\n");
    for (int g = 0; g < jg::NumGuitars; ++g)
        for (int fl : { 0, 2, 7 })
        {
            JangleProcessor j; setP (j.apvts, "guitar", (float) g); setP (j.apvts, "feel", (float) fl); setP (j.apvts, "key", 4);
            run (j, juce::String ("gtr ") + jg::guitarName (g) + " - " + jg::feelName (fl), 4);
        }
    { JangleProcessor j; setP (j.apvts, "guitar", 0); setP (j.apvts, "feel", 0); run (j, "gtr Steel Acoustic - waltz", 3); }
    { JangleProcessor j; setP (j.apvts, "guitar", 1); setP (j.apvts, "feel", 8); run (j, "gtr Clean - 6-8 arpeggio", 6); }
    std::printf ("\n== LEVELS: Harmonia\n");
    for (int g = 0; g < hm::NumGroups; ++g)
        for (int fl : { 0, 4, 5, 7 })
        {
            HarmoniaProcessor h; setP (h.apvts, "group", (float) g); setP (h.apvts, "feel", (float) fl);
            run (h, juce::String ("vox ") + hm::groupName (g) + " - " + hm::feelName (fl), 4);
        }
}

// Bandleader's on/off lights: a player that's switched off stops, the others carry on, and it comes back
void benchTest()
{
    std::printf ("\n== BAND LIGHTS: switch a player off and on from Bandleader\n");
    const double sr = 44100; const int bs = 256;
    BandleaderProcessor lead; UndertowProcessor bass; ParlourProcessor keys; TeaTowelProcessor drums;
    Head head; head.playing = true;
    juce::AudioProcessor* all[] = { &lead, &drums, &bass, &keys };
    for (auto* p : all) { p->setPlayConfigDetails (p == &lead || p == &bass ? 2 : 0, 2, sr, bs); p->prepareToPlay (sr, bs); p->setPlayHead (&head); }
    setP (keys.apvts, "sit_out", 0);
    const double spb = sr * 60.0 / head.bpm;
    int pos = 0;
    auto run = [&] (int bars, int* ons, double* rms)
    {
        for (int k = 0; k < 4; ++k) { ons[k] = 0; rms[k] = 0; }
        const int end = pos + (int) (spb * 4 * bars) / bs * bs;
        for (; pos < end; pos += bs)
        {
            head.ppq = pos / spb;
            for (int k = 0; k < 4; ++k)
            {
                juce::AudioBuffer<float> b (2, bs); b.clear(); juce::MidiBuffer m; all[k]->processBlock (b, m);
                for (const auto ev : m) if (ev.getMessage().isNoteOn()) ++ons[k];
                rms[k] += b.getRMSLevel (0, 0, bs);
            }
        }
    };
    int ons[4]; double rms[4];
    run (4, ons, rms);
    check (ons[1] > 0 && ons[2] > 0 && ons[3] > 0, "everyone on: drums, bass and keys all play");
    lead.toggleBench (band::WUndertow); lead.toggleBench (band::WTeaTowel);
    run (1, ons, rms); run (4, ons, rms);
    check (ons[2] == 0 && ons[1] == 0 && ons[3] > 0, "bass and drums switched off: they stop (" + juce::String (ons[2]) + ", " + juce::String (ons[1]) + " notes), keys keep playing (" + juce::String (ons[3]) + ")");
    check (rms[2] / 600.0 < 0.002, "the bass is silent while off");
    lead.toggleBench (band::WUndertow); lead.toggleBench (band::WTeaTowel);
    run (1, ons, rms); run (4, ons, rms);
    check (ons[1] > 0 && ons[2] > 0, "switched back on: they come back in");
    juce::MemoryBlock mb; lead.toggleBench (band::WParlour); lead.getStateInformation (mb);
    BandleaderProcessor lead2; lead2.setStateInformation (mb.getData(), (int) mb.getSize());
    check (lead2.benched.load() == (1u << band::WParlour), "the lights are saved with the set");
}

// a long torture run: the whole band, random block sizes, jumps, stops, tempo, meter and style changes, footswitches
void soak (int minutes)
{
    std::printf ("\n== SOAK: %d minutes of the band being pushed around\n", minutes);
    const double sr = 48000; const int maxBs = 2048;
    BandleaderProcessor lead; TeaTowelProcessor drums; UndertowProcessor bass; ParlourProcessor keys; JangleProcessor gtr; HarmoniaProcessor vox; ChamberProcessor orch;
    juce::AudioProcessor* all[] = { &lead, &drums, &bass, &keys, &gtr, &vox, &orch };
    Head head; head.playing = true; head.bpm = 120;
    for (auto* p : all) { p->setPlayConfigDetails (p == &lead || p == &bass ? 2 : 0, 2, sr, maxBs); p->prepareToPlay (sr, maxBs); p->setPlayHead (&head); }
    setP (orch.apvts, "sit_out", 0); setP (gtr.apvts, "sit_out", 0); setP (vox.apvts, "sit_out", 0);
    juce::Random r (42);
    juce::AudioBuffer<float> b (2, maxBs);
    const int64_t totalSamples = (int64_t) (minutes * 60.0 * sr);
    int64_t done = 0; int events = 0; bool bad = false; float peak = 0;
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    while (done < totalSamples && ! bad)
    {
        const int n = r.nextInt (10) == 0 ? 1 + r.nextInt (31) : 32 + r.nextInt (maxBs - 32);
        // now and then, something happens
        if (r.nextInt (400) == 0)
        {
            ++events;
            switch (r.nextInt (9))
            {
                case 0: head.ppq = r.nextDouble() * 400; break;                        // jump
                case 1: head.playing = ! head.playing; break;                         // stop / start
                case 2: head.bpm = 50 + r.nextInt (150); break;                       // tempo
                case 3: { const int m = r.nextInt (3); head.num = m == 0 ? 4 : m == 1 ? 3 : 6; head.den = m == 2 ? 8 : 4; setP (lead.apvts, "meter", (float) r.nextInt (4)); break; }
                case 4: setP (lead.apvts, "style", (float) r.nextInt ((int) band::styles().size() + 1)); lead.timerCallback(); break;
                case 5: lead.command ((BandleaderProcessor::Cmd) (1 + r.nextInt (10))); break;
                case 6: for (auto* id : { "form", "count_in", "intro", "breakdown", "key_up", "ending" }) setP (lead.apvts, id, (float) r.nextInt (3)); lead.timerCallback(); break;
                case 7: setP (gtr.apvts, "guitar", (float) r.nextInt (jg::NumGuitars)); setP (gtr.apvts, "feel", (float) r.nextInt (jg::NumFeels)); setP (vox.apvts, "feel", (float) r.nextInt (hm::NumFeels)); setP (vox.apvts, "group", (float) r.nextInt (hm::NumGroups)); break;
                default: lead.command (BandleaderProcessor::CLearn); break;
            }
            for (auto* f : { &drums.styleFollower, &bass.styleFollower, &keys.styleFollower, &gtr.styleFollower, &vox.styleFollower, &orch.styleFollower }) f->check();
        }
        for (auto* p : all)
        {
            juce::AudioBuffer<float> blk (b.getArrayOfWritePointers(), 2, n);
            blk.clear();
            if (p == &lead || p == &bass) for (int i = 0; i < n; ++i) { const float v = 0.1f * std::sin ((float) (done + i) * 0.03f); blk.setSample (0, i, v); blk.setSample (1, i, v); }
            juce::MidiBuffer m;
            p->processBlock (blk, m);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) { const float v = blk.getSample (c, i); if (! std::isfinite (v) || std::abs (v) > 4.0f) bad = true; peak = juce::jmax (peak, std::abs (v)); }
        }
        if (head.playing) head.ppq += n / sr * head.bpm / 60.0;
        done += n;
    }
    const double secs = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
    check (! bad, "no NaN, no blow-ups in " + juce::String (minutes) + " minutes (" + juce::String (events) + " random changes, peak " + juce::String (peak, 2) + ")");
    check (secs < minutes * 60.0 * 0.5, "the whole band uses " + juce::String (100.0 * secs / (minutes * 60.0), 1) + "% of one core");
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    if (const char* s = std::getenv ("SOAK")) { soak (juce::jmax (1, juce::String (s).getIntValue())); return fails == 0 ? 0 : 1; }
    if (std::getenv ("LEVELS")) { levels(); return 0; }
    formTest (2, "3-4");
    formTest (3, "6-8");
    formTest (1, "4-4");
    benchTest();
    learnKeys();
    learnGuitar();
    renderTest();
    std::printf ("\n%s (%d failed)\n", fails == 0 ? "ALL PASSED" : "SOME FAILED", fails);
    return fails == 0 ? 0 : 1;
}
