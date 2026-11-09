// Renders every Ether instrument in each role, playing its own song.
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Ether/EtherProcessor.h"

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
    int bad = 0;
    const char* roles[] = { "Melody", "Drone", "Swoops" };
    for (int inst = 0; inst < et::NumInstruments; ++inst)
        for (int role = 0; role < 3; ++role)
        {
            const juce::String name = juce::String (et::instrumentName (inst)) + " - " + roles[role];
            if (only.isNotEmpty() && ! name.containsIgnoreCase (only)) continue;
            EtherProcessor p;
            p.setPlayConfigDetails (0, 2, sr, bs); p.prepareToPlay (sr, bs);
            Head head; head.bpm = 96; p.setPlayHead (&head);
            setP (p.apvts, "instrument", (float) inst); setP (p.apvts, "role", (float) role); setP (p.apvts, "section_bars", 0);
            setP (p.apvts, "key", 4);
            const double spb = sr * 60 / head.bpm;
            const int total = (int) (spb * 4 * 12) / bs * bs;
            juce::AudioBuffer<float> out (2, total), block (2, bs);
            for (int pos = 0; pos < total; pos += bs)
            {
                block.clear(); head.ppq = pos / spb;
                juce::MidiBuffer m; p.processBlock (block, m);
                for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, block, ch, 0, bs);
            }
            bool nan = false;
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < total; ++i) if (! std::isfinite (out.getSample (ch, i))) nan = true;
            const float rms = out.getRMSLevel (0, 0, total), peak = out.getMagnitude (0, total);
            if (nan || rms < 0.005f || peak > 0.95f) ++bad;
            std::printf ("%-34s rms %.3f peak %.2f %s\n", name.toRawUTF8(), rms, peak, nan ? "NAN!" : "");
            if (role == 0 || only.isNotEmpty())
            {
                juce::WavAudioFormat wav;
                auto f = outDir.getChildFile ("ether_" + name.replaceCharacters (" -", "__") + ".wav"); f.deleteFile();
                std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0));
                w->writeFromAudioSampleBuffer (out, 0, total);
            }
        }
    {
        EtherProcessor px; px.setPlayConfigDetails (0, 2, sr, bs); px.prepareToPlay (sr, bs);
        std::unique_ptr<juce::AudioProcessorEditor> ed (px.createEditor());
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto pf = outDir.getChildFile ("ether_ui.png"); pf.deleteFile();
        juce::FileOutputStream os (pf); juce::PNGImageFormat().writeImageToStream (img, os);
    }
    std::printf ("%s\n", bad ? "SOME FAILED" : "ALL OK");
    return bad ? 1 : 0;
}
