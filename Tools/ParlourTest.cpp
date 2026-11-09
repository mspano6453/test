// Renders every Parlour instrument in a feel that suits it, playing its own song.
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Parlour/ParlourProcessor.h"

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
    struct Case { int inst, feel; float bpm; int prog; float colour; };
    const Case cases[] = {
        { pk::IPiano, pk::KDetroit, 116, 8, 0.2f }, { pk::IEPiano, pk::KLofi, 84, 24, 0.8f }, { pk::IFMPiano, pk::KBallad, 76, 9, 0.3f },
        { pk::IOrgan, pk::KSoul8, 104, 13, 0.3f }, { pk::IClav, pk::KStabs, 100, 29, 0.5f }, { pk::IStrings, pk::KPhillyStr, 110, 25, 0.5f },
        { pk::IBrass, pk::KStabs, 112, 10, 0.3f }, { pk::IPad, pk::KPads, 96, 0, 0.4f }, { pk::IVibes, pk::KSoul8, 92, 24, 0.7f },
        { pk::IGlock, pk::KBells, 140, 3, 0.2f }, { pk::ILofiPiano, pk::KLofi, 82, 24, 0.9f },
        { pk::IPiano, pk::KHouse, 122, 26, 0.6f }, { pk::IPad, pk::KArp, 104, 4, 0.2f }, { pk::IPiano, pk::KRock8, 132, 13, 0.1f } };
    int bad = 0;
    for (const auto& c : cases)
    {
        const juce::String name = juce::String (pk::instrumentName (c.inst)) + " - " + pk::feelName (c.feel);
        if (only.isNotEmpty() && ! name.containsIgnoreCase (only)) continue;
        ParlourProcessor p;
        p.setPlayConfigDetails (0, 2, sr, bs); p.prepareToPlay (sr, bs);
        Head head; head.bpm = c.bpm; p.setPlayHead (&head);
        setP (p.apvts, "instrument", (float) c.inst); setP (p.apvts, "feel", (float) c.feel); setP (p.apvts, "prog_verse", (float) c.prog);
        setP (p.apvts, "prog_chorus", (float) c.prog); setP (p.apvts, "colour", c.colour); setP (p.apvts, "key", 2);
        setP (p.apvts, "section_bars", 0); setP (p.apvts, "left_hand", c.feel == pk::KDetroit || c.feel == pk::KRock8 ? 1.0f : 0.0f);
        const double spb = sr * 60 / c.bpm;
        const int total = (int) (spb * 4 * 8) / bs * bs;
        juce::AudioBuffer<float> out (2, total), block (2, bs);
        bool nan = false;
        for (int pos = 0; pos < total; pos += bs)
        {
            block.clear(); head.ppq = pos / spb;
            juce::MidiBuffer m; p.processBlock (block, m);
            for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, block, ch, 0, bs);
        }
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < total; ++i) if (! std::isfinite (out.getSample (ch, i))) nan = true;
        const float rms = out.getRMSLevel (0, 0, total), peak = out.getMagnitude (0, total);
        if (nan || rms < 0.01f || peak > 0.99f) ++bad;
        std::printf ("%-40s rms %.3f peak %.2f %s samples=%d\n", name.toRawUTF8(), rms, peak, nan ? "NAN!" : "", (int) p.samplesLoaded());
        juce::WavAudioFormat wav;
        auto f = outDir.getChildFile ("parlour_" + name.replaceCharacters (" -/'", "____") + ".wav");
        f.deleteFile();
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0));
        w->writeFromAudioSampleBuffer (out, 0, total);
    }
    {
        ParlourProcessor px; px.setPlayConfigDetails (0, 2, sr, bs); px.prepareToPlay (sr, bs);
        std::unique_ptr<juce::AudioProcessorEditor> ed (px.createEditor());
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto pf = outDir.getChildFile ("parlour_ui.png"); pf.deleteFile();
        juce::FileOutputStream os (pf); juce::PNGImageFormat().writeImageToStream (img, os);
    }
    std::printf ("%s\n", bad ? "SOME FAILED" : "ALL OK");
    return bad ? 1 : 0;
}
