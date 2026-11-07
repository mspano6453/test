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
        Head head; head.bpm = sd.bpm; head.playing = false;
        struct P { juce::AudioProcessor* p; int ins; };
        const P all[] = { { &lead, 0 }, { &drums, 0 }, { &bass, 2 }, { &loon, 2 }, { &sw, 2 }, { &tape, 2 }, { &lathe, 2 } };
        for (auto& x : all) { x.p->setPlayConfigDetails (x.ins, 2, sr, bs); x.p->prepareToPlay (sr, bs); x.p->setPlayHead (&head); }
        setP (loon.apvts, "follow", 0);

        // pick the style; the followers take it from the bus
        setP (lead.apvts, "style", (float) styleIndex);
        lead.timerCallback();
        { juce::AudioBuffer<float> b (2, bs); juce::MidiBuffer m; lead.processBlock (b, m); }
        for (auto* f : { &drums.styleFollower, &bass.styleFollower, &loon.styleFollower, &sw.styleFollower, &tape.styleFollower, &lathe.styleFollower }) f->check();
        const bool took = (int) val (drums.apvts, "p_style") == sd.dStyle && (int) val (bass.apvts, "feel") == sd.bFeel
                       && std::abs (val (loon.apvts, "v_mix") - sd.lSpace) < 0.01f && std::abs (val (tape.apvts, "wow") - sd.tWow) < 0.01f
                       && lathe.getCurrentProgram() == sd.lathe && (int) val (lead.apvts, "key") == sd.key;

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
        const int bars = 16, total = (int) (spb * 4 * bars) / bs * bs;
        juce::AudioBuffer<float> mix (2, total), b (2, bs), bd (2, bs), bb (2, bs), bl (2, bs), bsw (2, bs);
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
            band::Leader L; band::readLeader (L);
            for (int i = 0; i < bs; ++i)
            {
                const double q = (pos + i) / spb;
                if (std::abs (q * 2 - std::round (q * 2)) * spb / 2 < 0.5)
                {
                    int r; bool mn; L.chordAt ((int64_t) (q / 4), r, mn);
                    const int notes[4] = { 40 + (r + 8) % 12, 52 + r, 52 + r + (mn ? 3 : 4), 52 + r + 7 };
                    for (int s = 0; s < 4; ++s) strings[(size_t) s].pluck (440.0 * std::pow (2.0, (notes[s] - 69) / 12.0), sr, 0.12f, rnd);
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
            }
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
        std::printf ("%-18s %3.0f bpm  took style %s  %s drums %.3f bass %.3f guitar+fx %.3f peak %.2f %s\n", sd.name, sd.bpm, took ? "yes" : "NO",
                     keptTweak ? "" : "RE-APPLIED ON RELOAD!", drumE / nb, bassE / nb, loonE / nb, mix.getMagnitude (0, total), nan ? "NAN!" : "");
        auto f = outDir.getChildFile (juce::String ("style_") + juce::String (sd.name).replaceCharacters (" /", "__") + ".wav"); f.deleteFile();
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
