// Offline test for Lathe: runs a WAV through the chain with the Assistant, checks loudness and peaks,
// and saves UI snapshots.  Usage: LatheTest in.wav out.wav [target 0-2]
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Lathe/LatheProcessor.h"
#include "../Lathe/LatheEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 3) return 1;
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (cwd.getChildFile (argv[1])));
    if (! rd) { std::printf ("can't read input\n"); return 1; }
    const double sr = rd->sampleRate; const int bs = 512;
    const int len = (int) rd->lengthInSamples;
    juce::AudioBuffer<float> src (2, len); rd->read (&src, 0, len, 0, true, true);

    LatheProcessor p;
    p.setPlayConfigDetails (2, 2, sr, bs);
    p.prepareToPlay (sr, bs);
    const int target = argc > 3 ? std::atoi (argv[3]) : 0;

    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    p.startAssistant (target);

    // loop the song until the assistant finishes, then render one clean pass
    juce::AudioBuffer<float> block (2, bs);
    int pos = 0, ticks = 0;
    while (p.assistPhase.load() != 3 && ticks < 20000)
    {
        for (int c = 0; c < 2; ++c) block.copyFrom (c, 0, src, c, pos, juce::jmin (bs, len - pos));
        juce::MidiBuffer m; p.processBlock (block, m);
        pos += bs; if (pos + bs > len) pos = 0;
        if (++ticks % 4 == 0) p.assistantTick();
    }
    std::printf ("assistant: %s (gain %.1f dB)\n", p.assistMessage.toRawUTF8(), p.apvts.getRawParameterValue ("mx_gain")->load());
    for (int b = 0; b < lt::NumEqBands; ++b)
        std::printf ("  %-10s %s %6.1f Hz %+5.1f dB\n", lt::eqBandName (b), p.apvts.getRawParameterValue (lt::eqId (b, "on"))->load() > 0.5f ? "on " : "off",
                     p.apvts.getRawParameterValue (lt::eqId (b, "f"))->load(), p.apvts.getRawParameterValue (lt::eqId (b, "g"))->load());

    p.resetLoudness = true;
    juce::AudioBuffer<float> out (2, len);
    float peak = 0; bool bad = false;
    for (int i = 0; i < len; i += bs)
    {
        const int n = juce::jmin (bs, len - i);
        juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, i, n);
        for (int c = 0; c < 2; ++c) view.copyFrom (c, 0, src, c, i, n);
        juce::MidiBuffer m; p.processBlock (view, m);
    }
    for (int c = 0; c < 2; ++c) for (int i = 0; i < len; ++i) { const float x = out.getSample (c, i); if (! std::isfinite (x)) bad = true; peak = std::max (peak, std::abs (x)); }
    std::printf ("output: integrated %.1f LUFS (target %.0f), sample peak %.2f dBFS, finite=%s\n",
                 p.outLoud.integrated.load(), lt::targetLufs (target), juce::Decibels::gainToDecibels (peak), bad ? "NO" : "yes");

    juce::WavAudioFormat wav;
    auto of = cwd.getChildFile (argv[2]); of.deleteFile();
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (of), sr, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (out, 0, len);

    // UI snapshots: EQ page and Maximizer page
    auto* le = dynamic_cast<LatheEditor*> (ed.get());
    auto snap = [&] (const char* name)
    {
        auto img = ed->createComponentSnapshot (ed->getLocalBounds());
        juce::File f (cwd.getChildFile (name)); f.deleteFile();
        juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
    };
    (void) le;
    snap ("lathe_eq.png");
    return bad ? 2 : 0;
}
