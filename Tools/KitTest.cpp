// Renders Tea Towel's drummer on each kit (recorded + the three machines) and a few of the new styles.
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/PluginProcessor.h"
struct Head : juce::AudioPlayHead
{
    double ppq = 0, bpm = 120; bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override { PositionInfo p; p.setIsPlaying (playing); p.setPpqPosition (ppq); p.setBpm (bpm); return p; }
};
static void setP (juce::AudioProcessorValueTreeState& s, const char* id, float v) { auto* p = s.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); }
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 44100; const int bs = 256;
    struct Case { const char* name; int style; int preset; double bpm; };
    const Case cases[] = { { "trap_boom", tt::StTrap, 6, 140 }, { "boombap_crunch", tt::StBoomBap, 4, 90 }, { "house_club", tt::StHouse, 7, 124 },
                           { "synthwave_84", tt::StSynthwave, 8, 110 }, { "detroit_soul", tt::StDetroitSoul, 1, 120 }, { "philly_soul", tt::StPhillySoul, 0, 112 },
                           { "gated_powerballad", tt::StPowerBallad, 5, 72 }, { "techno_club", tt::StTechno, 7, 130 }, { "newjack_84", tt::StNewJack, 8, 104 } };
    for (auto& c : cases)
    {
        if (argc > 1 && ! juce::String (c.name).contains (argv[1])) continue;
        TeaTowelProcessor p;
        p.setPlayConfigDetails (0, 2, sr, bs); p.prepareToPlay (sr, bs);
        Head head; head.bpm = c.bpm; p.setPlayHead (&head);
        p.setCurrentProgram (c.preset);
        setP (p.apvts, "p_on", 1); setP (p.apvts, "p_style", (float) c.style);
        if (c.style == tt::StBoomBap || c.style == tt::StNewJack) setP (p.apvts, "p_swing", 0.55f);
        const double spb = sr * 60 / c.bpm;
        const int total = (int) (spb * 4 * 12) / bs * bs;
        juce::AudioBuffer<float> out (2, total), b (2, bs);
        int hits = 0; bool nan = false;
        for (int pos = 0; pos < total; pos += bs)
        {
            head.ppq = pos / spb;
            if (pos == (int) (spb * 16) / bs * bs) setP (p.apvts, "p_section", 1);
            if (pos == (int) (spb * 32) / bs * bs) setP (p.apvts, "p_section", 2);
            juce::MidiBuffer m; b.clear(); p.processBlock (b, m);
            for (const auto e : m) if (e.getMessage().isNoteOn()) ++hits;
            for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, b, ch, 0, bs);
        }
        for (int i = 0; i < total; ++i) if (! std::isfinite (out.getSample (0, i))) nan = true;
        std::printf ("%-18s kit '%s' (%s)  hits %d  peak %.2f  rms %.3f %s\n", c.name, p.getProgramName (c.preset).toRawUTF8(),
                     tt::kitTypeName (tt::presets()[(size_t) c.preset].kitType), hits, out.getMagnitude (0, total), out.getRMSLevel (0, 0, total), nan ? "NAN" : "");
        auto f = juce::File::getCurrentWorkingDirectory().getChildFile (juce::String ("kit_") + c.name + ".wav"); f.deleteFile();
        juce::WavAudioFormat wav; std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0));
        w->writeFromAudioSampleBuffer (out, 0, total);
    }
    return 0;
}
