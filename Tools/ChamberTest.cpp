// Renders Chamber in every arrangement and ensemble, and writes the DRAG MIDI clip for each.
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Chamber/ChamberProcessor.h"

struct Head : juce::AudioPlayHead
{
    double ppq = 0, bpm = 100; bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override { PositionInfo p; p.setIsPlaying (playing); p.setPpqPosition (ppq); p.setBpm (bpm); return p; }
};
static void setP (juce::AudioProcessorValueTreeState& s, const char* id, float v) { auto* p = s.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); }

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 44100; const int bs = 256;
    const auto outDir = juce::File::getCurrentWorkingDirectory();
    const juce::String only = argc > 1 ? juce::String (argv[1]) : juce::String();
    struct Case { int ens, feel; float bpm; int prog; bool timp; float warp; };
    const Case cases[] = {
        { ch::EStrings, ch::FStaccato, 136, 3, false, 0 }, { ch::EQuartet, ch::FLong, 76, 9, false, 0 }, { ch::EStrings, ch::FDescend, 92, 7, false, 0 },
        { ch::EStrings, ch::FPizz, 110, 10, false, 0 }, { ch::EStrings, ch::FTremolo, 84, 3, true, 0 }, { ch::EStrings, ch::FRise, 96, 8, true, 0 },
        { ch::EStrings, ch::FRiff, 80, 7, false, 0 }, { ch::EBrass, ch::FFanfare, 112, 8, true, 0 }, { ch::EWinds, ch::FCounter, 100, 10, false, 0 },
        { ch::EStringsHorns, ch::FUnison, 96, 0, false, 0 }, { ch::EBrass, ch::FBaroque, 104, 8, false, 0 }, { ch::ETapeStrings, ch::FLong, 88, 0, false, 0 },
        { ch::ETapeFlutes, ch::FCounter, 92, 19, false, 0 }, { ch::EStrings, ch::FLong, 92, 0, true, 0.8f } };
    int bad = 0;
    for (const auto& c : cases)
    {
        const juce::String name = juce::String (ch::ensembleName (c.ens)) + " - " + ch::feelName (c.feel) + (c.warp > 0 ? " (warped)" : "");
        if (only.isNotEmpty() && ! name.containsIgnoreCase (only)) continue;
        ChamberProcessor p;
        p.setPlayConfigDetails (0, 2, sr, bs); p.prepareToPlay (sr, bs);
        Head head; head.bpm = c.bpm; p.setPlayHead (&head);
        setP (p.apvts, "ensemble", (float) c.ens); setP (p.apvts, "feel", (float) c.feel); setP (p.apvts, "prog_verse", (float) c.prog);
        setP (p.apvts, "prog_chorus", (float) c.prog); setP (p.apvts, "key", 2); setP (p.apvts, "section_bars", 0);
        setP (p.apvts, "timpani", c.timp ? 1.0f : 0.0f); setP (p.apvts, "warp", c.warp);
        const double spb = sr * 60 / c.bpm;
        const int total = (int) (spb * 4 * 10) / bs * bs;
        juce::AudioBuffer<float> out (2, total), block (2, bs);
        int midiOns = 0, midiOffs = 0;
        for (int pos = 0; pos < total; pos += bs)
        {
            block.clear(); head.ppq = pos / spb;
            juce::MidiBuffer m; p.processBlock (block, m);
            for (const auto ev : m) { if (ev.getMessage().isNoteOn()) ++midiOns; else if (ev.getMessage().isNoteOff()) ++midiOffs; }
            for (int chn = 0; chn < 2; ++chn) out.copyFrom (chn, pos, block, chn, 0, bs);
        }
        bool nan = false;
        for (int chn = 0; chn < 2; ++chn) for (int i = 0; i < total; ++i) if (! std::isfinite (out.getSample (chn, i))) nan = true;
        const float rms = out.getRMSLevel (0, 0, total), peak = out.getMagnitude (0, total);
        // the MIDI clip the DRAG MIDI button would hand to Live
        auto clip = p.capture.makeFile (8, "Chamber");
        int clipNotes = 0; for (auto* e : *clip.getTrack (0)) if (e->message.isNoteOn()) ++clipNotes;
        auto mf = outDir.getChildFile ("chamber_" + name.replaceCharacters (" -()+", "_____") + ".mid"); mf.deleteFile();
        { juce::FileOutputStream os (mf); clip.writeTo (os, 1); }
        if (nan || rms < 0.005f || peak > 0.99f || midiOns == 0 || clipNotes == 0) ++bad;
        std::printf ("%-44s rms %.3f peak %.2f  midi out %d on / %d off  clip(8 bars) %d notes %s samples=%d\n", name.toRawUTF8(), rms, peak, midiOns, midiOffs, clipNotes, nan ? "NAN!" : "", (int) p.samplesLoaded());
        juce::WavAudioFormat wav;
        auto f = outDir.getChildFile ("chamber_" + name.replaceCharacters (" -()+", "_____") + ".wav"); f.deleteFile();
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0));
        w->writeFromAudioSampleBuffer (out, 0, total);
    }
    {
        ChamberProcessor px; px.setPlayConfigDetails (0, 2, sr, bs); px.prepareToPlay (sr, bs);
        std::unique_ptr<juce::AudioProcessorEditor> ed (px.createEditor());
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto pf = outDir.getChildFile ("chamber_ui.png"); pf.deleteFile();
        juce::FileOutputStream os (pf); juce::PNGImageFormat().writeImageToStream (img, os);
    }
    std::printf ("%s\n", bad ? "SOME FAILED" : "ALL OK");
    return bad ? 1 : 0;
}
