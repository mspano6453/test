// Offline test for Undertow: a plucked-string "guitar" strums a progression on the sidechain;
// the bass should hear the chords and play along. Also MIDI chords, guitar-to-bass tracked and octave.
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Undertow/UndertowProcessor.h"

struct Head : juce::AudioPlayHead
{
    double ppq = 0, bpm = 120; bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override { PositionInfo p; p.setIsPlaying (playing); p.setPpqPosition (ppq); p.setBpm (bpm); return p; }
};

struct KS   // Karplus-Strong string
{
    std::vector<float> d; int i = 0; float last = 0, decay = 0.996f;
    void pluck (double f, double sr, float amp, juce::Random& r)
    {
        d.assign ((size_t) std::max (2.0, sr / f), 0.0f);
        for (auto& x : d) x = (r.nextFloat() * 2 - 1) * amp;
        i = 0;
    }
    float tick()
    {
        if (d.empty()) return 0;
        const size_t n = d.size(), j = (size_t) (i + 1) % n;
        const float y = d[(size_t) i];
        d[(size_t) i] = decay * 0.5f * (d[(size_t) i] + d[j]);
        i = (int) j;
        return y;
    }
};

static void setP (UndertowProcessor& p, const char* id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

static void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, (unsigned) b.getNumChannels(), 24, {}, 0));
    w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

static double mtof (int m) { return 440.0 * std::pow (2.0, (m - 69) / 12.0); }

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const auto outDir = juce::File::getCurrentWorkingDirectory();
    const double sr = 44100; const int bs = 256; const double bpm = 120;
    const double spb = sr * 60 / bpm;
    juce::Random rnd (7);
    const juce::String only = argc > 1 ? juce::String (argv[1]) : juce::String();

    // ------------------------------------------------------------------ 1. follow the guitar
    struct ChordDef { const char* name; int root; bool minor; std::vector<int> notes; };
    const std::vector<ChordDef> prog = {
        { "Am", 9, true,  { 45, 52, 57, 60, 64 } },     { "F", 5, false, { 41, 48, 53, 57, 60, 65 } },
        { "C", 0, false,  { 48, 52, 55, 60, 64 } },     { "G", 7, false, { 43, 47, 50, 55, 59, 67 } },
        { "Em", 4, true,  { 40, 47, 52, 55, 59, 64 } }, { "D", 2, false, { 50, 57, 62, 66 } } };
    const int order[] = { 0, 1, 2, 3, 0, 1, 2, 3, 4, 2, 5, 0, 4, 2, 5, 0 };

    auto runFollow = [&] (int feel, const char* file, int sectionAt8)
    {
        UndertowProcessor p;
        p.setPlayConfigDetails (2, 2, sr, bs);
        p.prepareToPlay (sr, bs);
        Head head; head.bpm = bpm; p.setPlayHead (&head);
        setP (p, "feel", (float) feel);
        setP (p, "follow", 0);
        const int bars = 16, total = (int) (spb * 4 * bars + sr);
        juce::AudioBuffer<float> out (2, total), block (2, bs);
        std::vector<KS> strings (6);
        int detectedOk = 0, detectedN = 0, rootsOk = 0, notes = 0;
        juce::String log;
        std::vector<int> played; std::vector<int> soundingAtDown (16, -1);
        for (int pos = 0; pos < total; pos += bs)
        {
            const int n = juce::jmin (bs, total - pos);
            block.setSize (2, n, false, false, true); block.clear();
            head.ppq = pos / spb;
            const int bar = (int) (head.ppq / 4);
            if (bar == 8 && sectionAt8 >= 0) setP (p, "section", (float) sectionAt8);
            // guitar: strum eighths (down on beats, up on &s), chord changes every bar
            for (int i = 0; i < n; ++i)
            {
                const double q = (pos + i) / spb;
                const int b = (int) (q / 4);
                const double e = q * 2; const int ei = (int) e;
                if (b < bars && std::abs (e - ei) * spb / 2 < 0.5 && (ei % 2 == 0 || rnd.nextFloat() < 0.6f))
                {
                    const auto& c = prog[(size_t) order[b]];
                    const bool down = ei % 2 == 0;
                    for (size_t s = 0; s < strings.size(); ++s)
                    {
                        if (s < c.notes.size()) strings[s].pluck (mtof (c.notes[s]), sr, down ? 0.18f : 0.1f, rnd);
                        else strings[s].d.clear();   // muted string
                    }
                }
                float g = 0; for (auto& s : strings) g += s.tick();
                block.setSample (0, i, g); block.setSample (1, i, g);
            }
            juce::AudioBuffer<float> guitar; guitar.makeCopyOf (block);
            juce::MidiBuffer midi;
            p.processBlock (block, midi);
            for (const auto m : midi)
                if (m.getMessage().isNoteOn())
                {
                    const int nn = m.getMessage().getNoteNumber();
                    played.push_back (nn);
                    const double q = (pos + m.samplePosition) / spb;
                    // what's sounding just after each downbeat (after any late correction) should be the root
                    const int b = (int) std::floor ((q + 0.15) / 4);
                    const double inBar = q - b * 4;
                    if (b < bars && inBar > -0.15 && inBar < 0.3) { soundingAtDown[(size_t) b] = nn; }
                }
            // check the detected chord mid-bar
            const double q = head.ppq;
            if (std::abs (std::fmod (q, 4.0) - 2.0) < n / spb && (int) (q / 4) < bars)
            {
                const auto& c = prog[(size_t) order[(int) (q / 4)]];
                const int ch = p.uiChord.load();
                ++detectedN;
                const bool ok = ch >= 0 && (ch & 15) == c.root && ((ch & 16) != 0) == c.minor;
                if (ok) ++detectedOk;
                else if (getenv ("UT_DEBUG"))
                {
                    const auto& E = p.debugEars();
                    juce::String d = "  chroma:"; for (int k = 0; k < 12; ++k) d << " " << ut::pcName (k) << juce::String (E.dbgChroma[k], 2);
                    d << "\n  bass:"; for (int k = 0; k < 12; ++k) d << " " << ut::pcName (k) << juce::String (E.dbgBass[k], 2);
                    d << "\n  scores:"; for (int k = 0; k < 24; ++k) d << " " << ut::pcName (k / 2) << (k % 2 ? "m" : "") << juce::String (E.dbgScores[k], 2);
                    d << "\n  peaks:"; for (int k = 0; k < E.dbgN; ++k) d << " " << juce::String (E.dbgPeakF[k], 0) << "/" << juce::String (E.dbgPeakM[k], 2);
                    std::printf ("%s %s\n%s\n", file, c.name, d.toRawUTF8());
                }
                log << c.name << "->" << (ch >= 0 ? juce::String (ut::pcName (ch & 15)) + ((ch & 16) ? "m" : "") : "--") << (ok ? " " : "(x) ");
            }
            for (int ch = 0; ch < 2; ++ch)
            {
                out.copyFrom (ch, pos, block, ch, 0, n);
                out.addFrom (ch, pos, guitar, ch, 0, n, 0.5f);
            }
        }
        juce::String dl;
        for (int b = 0; b < bars; ++b) if (soundingAtDown[(size_t) b] >= 0)
        {
            ++notes; const bool ok = soundingAtDown[(size_t) b] % 12 == prog[(size_t) order[b]].root; rootsOk += ok;
            dl << prog[(size_t) order[b]].name << ":" << ut::noteName (soundingAtDown[(size_t) b]) << (ok ? " " : "(x) ");
        }
        std::printf ("   downbeats: %s\n", dl.toRawUTF8());
        float peak = out.getMagnitude (0, total);
        std::printf ("[%s] chords %d/%d  %s\n   downbeat roots %d/%d  notes %zu  peak %.2f\n", file, detectedOk, detectedN, log.toRawUTF8(), rootsOk, notes, played.size(), peak);
        writeWav (outDir.getChildFile (file), out, sr);
        if (feel == ut::FPump)
        {
            std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            auto pf = outDir.getChildFile ("undertow_ui.png"); pf.deleteFile();
            juce::FileOutputStream os (pf); juce::PNGImageFormat().writeImageToStream (img, os);
        }
    };

    if (only.isEmpty() || only == "follow")
    {
        runFollow (ut::FPump, "undertow_pump_guitar.wav", ut::SChorus);
        runFollow (ut::FMelodic, "undertow_melodic_guitar.wav", ut::SChorus);
        runFollow (ut::FGothic, "undertow_gothic_guitar.wav", ut::SBridge);
    }

    // ------------------------------------------------------------------ 2. MIDI chords, every feel
    if (only.isEmpty() || only == "midi")
    {
        for (int feel = 0; feel < ut::NumFeels; ++feel)
        {
            UndertowProcessor p;
            p.setPlayConfigDetails (2, 2, sr, bs);
            p.prepareToPlay (sr, bs);
            Head head; p.setPlayHead (&head);
            setP (p, "feel", (float) feel); setP (p, "follow", 1);
            const int bars = 8, total = (int) (spb * 4 * bars);
            juce::AudioBuffer<float> out (2, total), block (2, bs);
            int lastBar = -1; std::vector<int> cur; int ok = 0, tot = 0; bool nan = false;
            for (int pos = 0; pos < total; pos += bs)
            {
                const int n = juce::jmin (bs, total - pos);
                block.setSize (2, n, false, false, true); block.clear();
                head.ppq = pos / spb;
                juce::MidiBuffer midi;
                const int bar = (int) (head.ppq / 4);
                if (bar != lastBar)
                {
                    for (int nn : cur) midi.addEvent (juce::MidiMessage::noteOff (1, nn), 0);
                    const auto& c = prog[(size_t) order[bar]];
                    cur = { 48 + c.root, 48 + c.root + (c.minor ? 3 : 4), 48 + c.root + 7 };
                    for (int nn : cur) midi.addEvent (juce::MidiMessage::noteOn (1, nn, (juce::uint8) 100), 0);
                    lastBar = bar;
                }
                p.processBlock (block, midi);
                for (const auto m : midi) if (m.getMessage().isNoteOn())
                {
                    const auto& c = prog[(size_t) order[bar]];
                    const int pc = m.getMessage().getNoteNumber() % 12;
                    const int iv = (pc - c.root + 12) % 12;
                    ++tot; if (iv == 0 || iv == 7 || iv == (c.minor ? 3 : 4)) ++ok;
                }
                for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, block, ch, 0, n);
            }
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < total; ++i) if (! std::isfinite (out.getSample (ch, i))) nan = true;
            std::printf ("[midi %s] notes %d, chord tones %d%%, peak %.2f rms %.3f %s\n", ut::feelName (feel), tot, tot ? ok * 100 / tot : 0,
                         out.getMagnitude (0, total), out.getRMSLevel (0, 0, total), nan ? "NAN!" : "");
            writeWav (outDir.getChildFile (juce::String ("undertow_midi_") + ut::feelName (feel) + ".wav"), out, sr);
        }
    }

    // ------------------------------------------------------------------ 3. guitar to bass: tracked + octave
    if (only.isEmpty() || only == "gtb")
    {
        const int line[] = { 40, 40, 43, 45, 47, 45, 43, 40, 52, 50, 47, 45, 43, 45, 47, 40 };   // E2 riff, eighths at 100 bpm
        for (int mode : { ut::MTracked, ut::MOctave })
        {
            UndertowProcessor p;
            p.setPlayConfigDetails (2, 2, sr, bs);
            p.prepareToPlay (sr, bs);
            Head head; head.playing = false; p.setPlayHead (&head);
            setP (p, "mode", (float) mode);
            const double eighth = sr * 0.3;
            const int total = (int) (eighth * 16 + sr);
            juce::AudioBuffer<float> out (2, total), block (2, bs);
            KS s; int nextNote = 0; std::vector<std::pair<int, double>> got;
            for (int pos = 0; pos < total; pos += bs)
            {
                const int n = juce::jmin (bs, total - pos);
                block.setSize (2, n, false, false, true); block.clear();
                for (int i = 0; i < n; ++i)
                {
                    if (nextNote < 16 && pos + i >= (int) (nextNote * eighth)) { s.pluck (mtof (line[nextNote]), sr, 0.3f, rnd); s.decay = 0.995f; ++nextNote; }
                    const float g = s.tick();
                    block.setSample (0, i, g); block.setSample (1, i, g);
                }
                juce::MidiBuffer midi;
                p.processBlock (block, midi);
                for (const auto m : midi) if (m.getMessage().isNoteOn()) got.push_back ({ m.getMessage().getNoteNumber(), (pos + m.samplePosition) / sr });
                for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, block, ch, 0, n);
            }
            juce::String g; int ok = 0;
            for (auto& [nn, t] : got)
            {
                const int idx = (int) std::floor (t / 0.3);
                const bool good = idx >= 0 && idx < 16 && nn == line[idx] - 12;
                ok += good;
                g << ut::noteName (nn) << "@" << juce::String ((t - idx * 0.3) * 1000, 0) << "ms" << (good ? " " : "(x) ");
            }
            std::printf ("[%s] %zu notes, %d correct: %s\n   peak %.2f\n", mode == ut::MTracked ? "tracked" : "octave", got.size(), ok, g.toRawUTF8(), out.getMagnitude (0, total));
            writeWav (outDir.getChildFile (mode == ut::MTracked ? "undertow_tracked.wav" : "undertow_octave.wav"), out, sr);
        }
    }
    return 0;
}
