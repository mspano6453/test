// Scripted test for Carousel: record with tempo snap, overdub, undo, redo, reverse, half speed, slots, clear, export.
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Carousel/CarouselProcessor.h"
#include "../Carousel/CarouselEditor.h"

struct Head : juce::AudioPlayHead
{
    double ppq = 0, bpm = 120;
    juce::Optional<PositionInfo> getPosition() const override { PositionInfo p; p.setIsPlaying (true); p.setPpqPosition (ppq); p.setBpm (bpm); return p; }
};

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const double sr = 48000; const int bs = 256;
    CarouselProcessor p;
    p.setPlayConfigDetails (2, 2, sr, bs);
    p.prepareToPlay (sr, bs);
    Head head; p.setPlayHead (&head);
    const int freeAtStart = (int) cr::kPoolSeconds;

    struct Cue { double t; int action; const char* label; };
    const Cue cues[] = {
        { 0.50, cr::ARecPlayDub, "rec" }, { 4.30, cr::ARecPlayDub, "close (should snap to 2 bars = 4.00 s)" },
        { 9.00, cr::ARecPlayDub, "overdub" }, { 13.20, cr::ARecPlayDub, "play" },
        { 15.0, cr::AUndo, "undo" }, { 17.0, cr::ARedo, "redo" },
        { 19.0, cr::AToggleReverse, "reverse" }, { 23.0, cr::AToggleReverse, "forward" },
        { 25.0, cr::AToggleHalf, "half speed" }, { 29.0, cr::AToggleHalf, "normal" },
        { 31.0, cr::ANext, "slot 2" }, { 31.5, cr::ARecPlayDub, "rec slot 2" }, { 33.6, cr::ARecPlayDub, "close slot 2 (snap 1 bar = 2.00 s)" },
        { 37.0, cr::AStop, "stop" }, { 38.0, cr::APrev, "back to slot 1" }, { 38.5, cr::ARecPlayDub, "play slot 1" },
    };
    const int total = (int) (sr * 42);
    juce::AudioBuffer<float> out (2, total), block (2, bs);
    size_t ci = 0; bool bad = false;
    double ph1 = 0, ph2 = 0;
    for (int pos = 0; pos < total; pos += bs)
    {
        const int n = juce::jmin (bs, total - pos);
        const double t = pos / sr;
        while (ci < std::size (cues) && cues[ci].t <= t)
        {
            p.push (cues[ci].action);
            juce::MidiBuffer m; block.setSize (2, 1, false, false, true); block.clear(); p.processBlock (block, m);   // apply now
            std::printf ("%5.2fs  %-40s -> %-9s len %.3f s, layers %d, free %d s\n", t, cues[ci].label, cr::stateName (p.uiState.load()),
                         p.uiLenSec.load(), p.uiLayers.load(), p.uiFreeChunks.load());
            ++ci;
        }
        block.setSize (2, n, false, false, true);
        for (int i = 0; i < n; ++i)
        {
            // input: a "guitar" arpeggio while recording slot 1, a different note while overdubbing, silence otherwise
            const double tt = (pos + i) / sr;
            float x = 0;
            if (tt >= 0.5 && tt < 4.0) { ph1 += 220.0 * std::pow (2.0, ((int) (tt * 4) % 4) * 4 / 12.0) / sr; x = 0.3f * (float) std::sin (6.2831853 * ph1) * (float) std::exp (-std::fmod (tt, 0.25) * 8); }
            if (tt >= 9.0 && tt < 13.0) { ph2 += 659.0 / sr; x = 0.2f * (float) std::sin (6.2831853 * ph2) * (float) std::exp (-std::fmod (tt, 0.5) * 6); }
            if (tt >= 31.5 && tt < 33.5) { ph2 += 110.0 / sr; x = 0.3f * (float) std::sin (6.2831853 * ph2); }
            block.setSample (0, i, x); block.setSample (1, i, x);
        }
        juce::MidiBuffer m;
        p.processBlock (block, m);
        head.ppq += n / sr * 2.0;
        for (int c = 0; c < 2; ++c) { out.copyFrom (c, pos, block, c, 0, n); for (int i = 0; i < n; ++i) if (! std::isfinite (block.getSample (c, i))) bad = true; }
    }
    std::printf ("peak %.2f finite=%s\n", out.getMagnitude (0, total), bad ? "NO" : "yes");
    for (int s = 0; s < 42; s += 2) std::printf ("%s%2d:%.3f", s % 14 == 0 ? "\n  rms " : "  ", s, out.getRMSLevel (0, (int) (s * sr), (int) (2 * sr)));
    std::printf ("\n");

    auto f = p.exportCurrentLoop();
    std::printf ("export: %s (%lld bytes)\n", f.getFullPathName().toRawUTF8(), (long long) f.getSize());

    // MIDI footswitch + clear-all frees all memory
    { juce::MidiBuffer m; m.addEvent (juce::MidiMessage::controllerEvent (1, 88, 127), 0); block.setSize (2, bs, false, false, true); block.clear(); p.processBlock (block, m); }
    std::printf ("after CC88 (clear) on slot %d: state %s\n", p.uiSlot.load() + 1, cr::stateName (p.uiState.load()));
    p.push (cr::AClearAll); { juce::MidiBuffer m; p.processBlock (block, m); }
    std::printf ("after clear all: free %d of %d s\n", p.uiFreeChunks.load(), freeAtStart);

    juce::WavAudioFormat wav;
    auto wf = cwd.getChildFile ("carousel_test.wav"); wf.deleteFile();
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (wf), sr, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (out, 0, total);

    // UI snapshot with a loop playing
    p.push (cr::ARecPlayDub); { juce::MidiBuffer m; for (int k = 0; k < 400; ++k) { block.clear(); for (int i = 0; i < bs; ++i) block.setSample (0, i, 0.2f * (float) std::sin (i * 0.05)); p.processBlock (block, m); } }
    p.push (cr::ARecPlayDub); { juce::MidiBuffer m; for (int k = 0; k < 300; ++k) { block.clear(); p.processBlock (block, m); } }
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    if (auto* t = dynamic_cast<juce::Timer*> (ed.get())) t->timerCallback();
    auto img = ed->createComponentSnapshot (ed->getLocalBounds());
    auto pf = cwd.getChildFile ("carousel_ui.png"); pf.deleteFile();
    juce::FileOutputStream os (pf); juce::PNGImageFormat().writeImageToStream (img, os);
    return bad ? 2 : 0;
}
