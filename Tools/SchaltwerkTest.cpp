// Offline test for Schaltwerk: drums from the sequencer, robot synth, Omnichord, vowel choir, vocoder.
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Schaltwerk/SchaltwerkProcessor.h"
#include "../Schaltwerk/SchaltwerkEditor.h"

struct Head : juce::AudioPlayHead
{
    double ppq = 0, bpm = 112;
    juce::Optional<PositionInfo> getPosition() const override
    { PositionInfo p; p.setIsPlaying (true); p.setPpqPosition (ppq); p.setBpm (bpm); return p; }
};

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const double sr = 48000; const int bs = 256;
    SchaltwerkProcessor p;
    p.setPlayConfigDetails (2, 2, sr, bs);
    p.prepareToPlay (sr, bs);
    Head head; p.setPlayHead (&head);
    auto set = [&] (const char* id, float v) { auto* a = p.apvts.getParameter (id); a->setValueNotifyingHost (a->convertTo0to1 (v)); };
    if (std::getenv ("SW_NODRUMS")) set ("seq_on", 0);

    // snapshots
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        int idx = 0;
        for (auto* c : ed->getChildren())
            if (auto* b = dynamic_cast<juce::TextButton*> (c))
                if (b->getRadioGroupId() == 77)
                {
                    b->setToggleState (true, juce::dontSendNotification); if (b->onClick) b->onClick();
                    auto img = ed->createComponentSnapshot (ed->getLocalBounds());
                    auto f = cwd.getChildFile ("sw_ui_" + juce::String (idx++) + ".png"); f.deleteFile();
                    juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
                }
    }

    // sidechain source for the vocoder section
    juce::AudioBuffer<float> side;
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (cwd.getChildFile (argc > 1 ? argv[1] : "tape_in.wav")));
        if (rd) { side.setSize (2, (int) rd->lengthInSamples); rd->read (&side, 0, (int) rd->lengthInSamples, 0, true, true); }
    }

    const double barSec = 4 * 60.0 / head.bpm;
    const int bars = 20;
    const int total = (int) (barSec * bars * sr) + (int) (2 * sr);
    juce::AudioBuffer<float> out (2, total), block (2, bs);
    const int melody[] = { 60, 63, 67, 70, 72, 70, 67, 63 };
    const int chords[][2] = { { 0, 0 }, { 9, 1 }, { 5, 0 }, { 7, 2 } };   // C, Am, F, G7
    int lastBar = -1, lastEighth = -1, lastNote = -1;
    bool bad = false;
    for (int pos = 0; pos < total; pos += bs)
    {
        const int n = juce::jmin (bs, total - pos);
        const double t = pos / sr;
        const int bar = (int) (t / barSec);
        const int eighth = (int) (t / (barSec / 8));
        juce::MidiBuffer midi;
        if (bar != lastBar)
        {
            lastBar = bar;
            if (bar == 4) set ("sy_glide", 0.04f);
            if (bar == 8) { set ("om_auto", 2); }
            if (bar >= 8 && bar < 12) { auto& c = chords[bar - 8]; p.pressChord (c[0], c[1]); }
            if (bar == 12) { p.releaseChord(); set ("om_auto", 0); set ("sy_glide", 0); set ("sy_mode", 1); p.setCurrentProgram (2);
                             for (int k : { 48, 55, 60, 63 }) midi.addEvent (juce::MidiMessage::noteOn (1, k, 0.8f), 0); }
            if (bar == 14) set ("sy_vowel", 0.9f);
            if (bar == 16) { for (int k : { 48, 55, 60, 63 }) midi.addEvent (juce::MidiMessage::noteOff (1, k), 0);
                             set ("sy_mode", 2); for (int k : { 48, 55, 60, 67 }) midi.addEvent (juce::MidiMessage::noteOn (1, k, 0.9f), 0); }
        }
        if (bar >= 4 && bar < 8 && eighth != lastEighth)
        {
            if (lastNote >= 0) midi.addEvent (juce::MidiMessage::noteOff (1, lastNote), 0);
            lastNote = melody[eighth % 8]; midi.addEvent (juce::MidiMessage::noteOn (1, lastNote, 0.85f), 0);
        }
        if (bar == 8 && lastNote >= 0) { midi.addEvent (juce::MidiMessage::noteOff (1, lastNote), 0); lastNote = -1; }
        if (bar >= 10 && bar < 12 && eighth != lastEighth && eighth % 4 == 0)   // hand strums on the strumplate via CC1
            for (int k = 0; k <= 127; k += 9) midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, k), juce::jmin (n - 1, k / 4));
        lastEighth = eighth;

        block.setSize (2, n, false, false, true);
        block.clear();
        if (bar >= 16 && side.getNumSamples() > 0)
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i)
                    block.setSample (c, i, side.getSample (c, (pos + i) % side.getNumSamples()));
        p.processBlock (block, midi);
        head.ppq += n / sr * head.bpm / 60.0;
        for (int c = 0; c < 2; ++c) { out.copyFrom (c, pos, block, c, 0, n); for (int i = 0; i < n; ++i) if (! std::isfinite (block.getSample (c, i))) bad = true; }
    }
    std::printf ("peak %.2f finite=%s\n", out.getMagnitude (0, total), bad ? "NO" : "yes");
    for (int b = 0; b < bars; b += 2)
    {
        const int s0 = (int) (b * barSec * sr), len = (int) (2 * barSec * sr);
        std::printf ("  bars %2d-%2d rms %.3f\n", b + 1, b + 2, out.getRMSLevel (0, s0, len));
    }
    auto f = cwd.getChildFile (argc > 2 ? argv[2] : "schaltwerk_demo.wav"); f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (out, 0, total);
    return bad ? 2 : 0;
}
