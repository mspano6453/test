// Offline test for genre Styles: pick a style in Bandleader, check every plugin took it,
// check a reopened set doesn't re-apply it, and render a short demo of each style.
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Bandleader/BandleaderProcessor.h"
#include "../Source/PluginProcessor.h"
#include "../Undertow/UndertowProcessor.h"
#include "../Loon/LoonProcessor.h"
#include "../Schaltwerk/SchaltwerkProcessor.h"
#include "../TapeTwin/TapeTwinProcessor.h"
#include "../Lathe/LatheProcessor.h"
#include "../Parlour/ParlourProcessor.h"
#include "../Ether/EtherProcessor.h"
#include "../Chamber/ChamberProcessor.h"
#include "../Jangle/JangleProcessor.h"
#include "../Harmonia/HarmoniaProcessor.h"

struct Head : juce::AudioPlayHead
{
    double ppq = 0, bpm = 120; bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override { PositionInfo p; p.setIsPlaying (playing); p.setPpqPosition (ppq); p.setBpm (bpm); return p; }
};
static float val (juce::AudioProcessorValueTreeState& s, const char* id) { return s.getParameter (id)->convertFrom0to1 (s.getParameter (id)->getValue()); }
static void setP (juce::AudioProcessorValueTreeState& s, const char* id, float v) { auto* p = s.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); }

struct KS
{
    std::vector<float> d; int i = 0;
    void pluck (double f, double sr, float amp, juce::Random& r) { d.assign ((size_t) std::max (2.0, sr / f), 0.0f); for (auto& x : d) x = (r.nextFloat() * 2 - 1) * amp; i = 0; }
    float tick() { if (d.empty()) return 0; const size_t n = d.size(), j = (size_t) (i + 1) % n; const float y = d[(size_t) i]; d[(size_t) i] = 0.997f * 0.5f * (d[(size_t) i] + d[j]); i = (int) j; return y; }
};

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 44100; const int bs = 256;
    const auto outDir = juce::File::getCurrentWorkingDirectory();
    const juce::String only = argc > 1 ? juce::String (argv[1]) : juce::String();

    int styleIndex = 0;
    for (const auto& sd : band::styles())
    {
        ++styleIndex;
        if (only.isNotEmpty() && ! juce::String (sd.name).containsIgnoreCase (only)) continue;
        BandleaderProcessor lead; TeaTowelProcessor drums; UndertowProcessor bass; LoonProcessor loon; SchaltwerkProcessor sw; TapeTwinProcessor tape; LatheProcessor lathe;
        ParlourProcessor keys; EtherProcessor ether; ChamberProcessor orch; JangleProcessor gtrP; HarmoniaProcessor vox;
        Head head; head.bpm = sd.bpm; head.playing = false;
        struct P { juce::AudioProcessor* p; int ins; };
        const P all[] = { { &lead, 0 }, { &drums, 0 }, { &bass, 2 }, { &loon, 2 }, { &sw, 2 }, { &tape, 2 }, { &lathe, 2 }, { &keys, 0 }, { &ether, 0 }, { &orch, 0 }, { &gtrP, 0 }, { &vox, 0 } };
        for (auto& x : all) { x.p->setPlayConfigDetails (x.ins, 2, sr, bs); x.p->prepareToPlay (sr, bs); x.p->setPlayHead (&head); }
        setP (loon.apvts, "follow", 0);

        // pick the style; the followers take it from the bus
        setP (lead.apvts, "style", (float) styleIndex);
        lead.timerCallback();
        { juce::AudioBuffer<float> b (2, bs); juce::MidiBuffer m; lead.processBlock (b, m); }
        for (auto* f : { &drums.styleFollower, &bass.styleFollower, &loon.styleFollower, &sw.styleFollower, &tape.styleFollower, &lathe.styleFollower, &keys.styleFollower, &ether.styleFollower, &orch.styleFollower, &gtrP.styleFollower, &vox.styleFollower }) f->check();
        const bool took = (int) val (drums.apvts, "p_style") == sd.dStyle && (int) val (bass.apvts, "feel") == sd.bFeel
                       && std::abs (val (loon.apvts, "v_mix") - sd.lSpace) < 0.01f && std::abs (val (tape.apvts, "wow") - sd.tWow) < 0.01f
                       && lathe.getCurrentProgram() == sd.lathe && (int) val (lead.apvts, "key") == sd.key
                       && (int) val (bass.apvts, "bass_type") == sd.uBass
                       && ((int) val (keys.apvts, "sit_out") == (sd.kOn ? 0 : 1)) && (! sd.kOn || (int) val (keys.apvts, "instrument") == sd.kInst)
                       && ((int) val (ether.apvts, "sit_out") == (sd.eRole > 0 ? 0 : 1)) && (sd.eRole <= 0 || (int) val (ether.apvts, "instrument") == sd.eInst)
                       && ((int) val (orch.apvts, "sit_out") == (sd.cOn ? 0 : 1)) && (! sd.cOn || (int) val (orch.apvts, "ensemble") == sd.cEns)
                       && ((int) val (bass.apvts, "sit_out") == (sd.uOn ? 0 : 1))
                       && ((int) val (gtrP.apvts, "sit_out") == (sd.gOn ? 0 : 1)) && (! sd.gOn || (int) val (gtrP.apvts, "guitar") == sd.gInst)
                       && ((int) val (vox.apvts, "sit_out") == (sd.vOn ? 0 : 1)) && (! sd.vOn || (int) val (vox.apvts, "feel") == sd.vFeel)
                       && (int) val (lead.apvts, "meter") == (sd.meter == 0 ? 0 : sd.meter + 1);

        // a reopened set must not re-apply the style over your tweaks
        bool keptTweak = true;
        if (styleIndex == 1 || only.isNotEmpty())
        {
            setP (bass.apvts, "grit", 0.99f);
            juce::MemoryBlock mb; bass.getStateInformation (mb);
            UndertowProcessor bass2; bass2.setStateInformation (mb.getData(), (int) mb.getSize());
            bass2.styleFollower.check();
            keptTweak = val (bass2.apvts, "grit") > 0.98f;
            bass.styleFollower.check();
            keptTweak = keptTweak && val (bass.apvts, "grit") > 0.98f;
            setP (bass.apvts, "grit", sd.bGrit);
        }

        // play 16 bars: a fake guitar strums the song's chords into Loon and Tape Twin
        head.playing = true;
        const double spb = sr * 60.0 / sd.bpm;
        const double barQ = band::meterBarPpq (sd.meter);
        const int bars = 16, total = (int) (spb * barQ * bars) / bs * bs;
        juce::AudioBuffer<float> mix (2, total), b (2, bs), bd (2, bs), bb (2, bs), bl (2, bs), bsw (2, bs), bk (2, bs), be (2, bs), bo (2, bs), bg (2, bs), bv (2, bs);
        double gtrE = 0, voxE = 0;
        const juce::String pk (sd.pack);
        const float gtr = (sd.gOn || pk == "Hip-Hop" || pk == "Electronic" || pk == "Early Electronic" || pk == "60s Chamber Pop") ? 0.0f : (pk == "Soul" || pk == "80s") ? 0.05f : 0.12f;
        double keysE = 0, etherE = 0, orchE = 0;
        std::vector<KS> strings (4); juce::Random rnd (5);
        double drumE = 0, bassE = 0, loonE = 0; bool nan = false;
        for (int pos = 0; pos < total; pos += bs)
        {
            head.ppq = pos / spb;
            juce::MidiBuffer m;
            b.clear(); lead.processBlock (b, m); m.clear();
            bd.clear(); drums.processBlock (bd, m); m.clear();
            bb.clear(); bass.processBlock (bb, m); m.clear();
            bsw.clear(); sw.processBlock (bsw, m); m.clear();
            bk.clear(); keys.processBlock (bk, m); m.clear();
            be.clear(); ether.processBlock (be, m); m.clear();
            bo.clear(); orch.processBlock (bo, m); m.clear();
            bg.clear(); gtrP.processBlock (bg, m); m.clear();
            bv.clear(); vox.processBlock (bv, m); m.clear();
            band::Leader L; band::readLeader (L);
            for (int i = 0; i < bs; ++i)
            {
                const double q = (pos + i) / spb;
                if (std::abs (q * 2 - std::round (q * 2)) * spb / 2 < 0.5)
                {
                    int r; bool mn; L.chordAt (band::barOfPpq (q, L.meter), r, mn);
                    const int notes[4] = { 40 + (r + 8) % 12, 52 + r, 52 + r + (mn ? 3 : 4), 52 + r + 7 };
                    for (int s = 0; s < 4; ++s) if (gtr > 0) strings[(size_t) s].pluck (440.0 * std::pow (2.0, (notes[s] - 69) / 12.0), sr, gtr, rnd);
                }
                float g = 0; for (auto& s : strings) g += s.tick();
                bl.setSample (0, i, g); bl.setSample (1, i, g);
            }
            loon.processBlock (bl, m); m.clear();
            tape.processBlock (bl, m); m.clear();
            for (int c = 0; c < 2; ++c)
            {
                mix.copyFrom (c, pos, bd, c, 0, bs); mix.applyGain (c, pos, bs, 0.9f);
                mix.addFrom (c, pos, bb, c, 0, bs, 0.9f);
                mix.addFrom (c, pos, bl, c, 0, bs, 0.6f);
                mix.addFrom (c, pos, bsw, c, 0, bs, 0.6f);
                mix.addFrom (c, pos, bk, c, 0, bs, 0.8f);
                mix.addFrom (c, pos, be, c, 0, bs, 0.7f);
                mix.addFrom (c, pos, bo, c, 0, bs, 0.9f);
                mix.addFrom (c, pos, bg, c, 0, bs, 0.8f);
                mix.addFrom (c, pos, bv, c, 0, bs, 0.8f);
            }
            gtrE += bg.getRMSLevel (0, 0, bs); voxE += bv.getRMSLevel (0, 0, bs);
            keysE += bk.getRMSLevel (0, 0, bs); etherE += be.getRMSLevel (0, 0, bs); orchE += bo.getRMSLevel (0, 0, bs);
            drumE += bd.getRMSLevel (0, 0, bs); bassE += bb.getRMSLevel (0, 0, bs); loonE += bl.getRMSLevel (0, 0, bs);
        }
        // master through Lathe
        for (int pos = 0; pos < total; pos += bs)
        {
            juce::AudioBuffer<float> blk (mix.getArrayOfWritePointers(), 2, pos, bs);
            juce::MidiBuffer m; lathe.processBlock (blk, m);
        }
        for (int c = 0; c < 2; ++c) for (int i = 0; i < total; ++i) if (! std::isfinite (mix.getSample (c, i))) nan = true;
        const int nb = total / bs;
        std::printf ("%-20s %3.0f bpm %s took %s  %s drums %.3f bass %.3f fx %.3f keys %.3f ether %.3f orch %.3f gtr %.3f vox %.3f peak %.2f %s\n", sd.name, sd.bpm, band::meterName (sd.meter), took ? "yes" : "NO",
                     keptTweak ? "" : "RE-APPLIED ON RELOAD!", drumE / nb, bassE / nb, loonE / nb, keysE / nb, etherE / nb, orchE / nb, gtrE / nb, voxE / nb, mix.getMagnitude (0, total), nan ? "NAN!" : "");
        // what each plugin's DRAG MIDI button would hand to Live (8 bars)
        if (juce::String (sd.name) == "Space Bubble Pop" || juce::String (sd.name) == "Synth Rock" || juce::String (sd.name) == "Soul Ballad 6/8" || juce::String (sd.name) == "Country Waltz")
        {
            const std::pair<const mc::Capture*, const char*> caps[] = { { &drums.capture, "TeaTowel" }, { &bass.capture, "Undertow" }, { &keys.capture, "Parlour" },
                                                                        { &ether.capture, "Ether" }, { &orch.capture, "Chamber" }, { &lead.capture, "Bandleader" },
                                                                        { &loon.capture, "Loon" }, { &sw.capture, "Schaltwerk" }, { &gtrP.capture, "Jangle" }, { &vox.capture, "Harmonia" } };
            for (auto& [c, nm] : caps)
            {
                auto clip = c->makeFile (8, nm);
                int notes = 0; for (auto* e : *clip.getTrack (0)) if (e->message.isNoteOn()) ++notes;
                auto mf = outDir.getChildFile (juce::String ("midi_") + juce::String (sd.name).replaceCharacters (" /", "__") + "_" + nm + ".mid"); mf.deleteFile();
                { juce::FileOutputStream os (mf); clip.writeTo (os, 1); }
                std::printf ("    DRAG MIDI %-10s %d notes in the last 8 bars\n", nm, notes);
            }
        }
        auto f = outDir.getChildFile (juce::String ("style_") + juce::String (sd.name).replaceCharacters (" /&", "___") + ".wav"); f.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0));
        w->writeFromAudioSampleBuffer (mix, 0, total);
        if (only.isNotEmpty())
        {
            std::unique_ptr<juce::AudioProcessorEditor> ed (lead.createEditor());
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            auto pf = outDir.getChildFile ("bandleader_ui.png"); pf.deleteFile();
            juce::FileOutputStream os (pf); juce::PNGImageFormat().writeImageToStream (img, os);
        }
    }
    return 0;
}
