// Offline test: Bandleader with Tea Towel, Loon, Schaltwerk, Carousel, Tape Twin and Lathe in one "set".
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Bandleader/BandleaderProcessor.h"
#include "../Source/PluginProcessor.h"
#include "../Loon/LoonProcessor.h"
#include "../Schaltwerk/SchaltwerkProcessor.h"
#include "../Carousel/CarouselProcessor.h"
#include "../TapeTwin/TapeTwinProcessor.h"
#include "../Lathe/LatheProcessor.h"

struct Head : juce::AudioPlayHead
{
    double ppq = 0, bpm = 120; bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override { PositionInfo p; p.setIsPlaying (playing); p.setPpqPosition (ppq); p.setBpm (bpm); return p; }
};

static void setP (juce::AudioProcessorValueTreeState& s, const char* id, float v)
{
    auto* prm = s.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 44100, bpm = 120; const int bs = 256;
    const double spb = sr * 60 / bpm;

    BandleaderProcessor lead; TeaTowelProcessor drums; LoonProcessor loon; SchaltwerkProcessor sw;
    CarouselProcessor car; TapeTwinProcessor tape; LatheProcessor lathe;
    Head head;
    struct P { juce::AudioProcessor* p; int ins; };
    const P all[] = { { &lead, 0 }, { &drums, 0 }, { &loon, 2 }, { &sw, 2 }, { &car, 2 }, { &tape, 2 }, { &lathe, 2 } };
    for (auto& x : all) { x.p->setPlayConfigDetails (x.ins, 2, sr, bs); x.p->prepareToPlay (sr, bs); x.p->setPlayHead (&head); }
    setP (lead.apvts, "section_bars", 0);        // 4-bar sections
    setP (loon.apvts, "follow", 0);
    setP (tape.apvts, "echo_mix", 0); setP (tape.apvts, "adt_mix", 0); setP (tape.apvts, "feedback", 0); setP (tape.apvts, "drive", 0);
    setP (tape.apvts, "sync", 3);                // 1/8 echoes

    const int bars = 24, total = (int) (spb * 4 * bars) / bs * bs;
    juce::AudioBuffer<float> b0 (2, bs), bD (2, bs), bL (2, bs), bS (2, bs), bC (2, bs), bT (2, bs), bM (2, bs);
    std::vector<double> swRms (bars, 0), tapeWet (bars * 4, 0);
    int loonOk = 0, loonN = 0, swOk = 0, swN = 0;
    bool carPressed = false, dropped = false, back = false;
    float carLen = 0; int carState = -1; float carRecStartBar = -1;
    double ph = 0;
    for (int pos = 0; pos < total; pos += bs)
    {
        head.ppq = pos / spb;
        const double barF = head.ppq / 4;
        if (! carPressed && barF > 1.5) { car.push (cr::ARecPlayDub); carPressed = true; }
        if (! dropped && barF > 9.3) { lead.command (BandleaderProcessor::CDrop); dropped = true; }
        if (! back && barF > 12.2) { lead.command (BandleaderProcessor::CAllIn); back = true; }
        juce::MidiBuffer m;
        b0.clear(); lead.processBlock (b0, m); m.clear();
        bD.clear(); drums.processBlock (bD, m); m.clear();
        bL.clear(); loon.processBlock (bL, m); m.clear();
        bS.clear(); sw.processBlock (bS, m); m.clear();
        // Carousel hears a steady tone; Tape Twin hears a click on every beat
        for (int i = 0; i < bs; ++i)
        {
            const float v = 0.3f * (float) std::sin (ph); ph += 2 * juce::MathConstants<double>::pi * 220 / sr;
            bC.setSample (0, i, v); bC.setSample (1, i, v);
            const double q = (pos + i) / spb;
            const float click = std::fmod (q, 1.0) < 0.01 ? 0.5f : 0.0f;
            bT.setSample (0, i, click); bT.setSample (1, i, click);
        }
        juce::AudioBuffer<float> tIn; tIn.makeCopyOf (bT);
        car.processBlock (bC, m); m.clear();
        tape.processBlock (bT, m); m.clear();
        bM.makeCopyOf (bD); lathe.processBlock (bM, m); m.clear();

        const int bar = (int) barF;
        if (bar >= bars) break;
        band::Leader L; band::readLeader (L);
        int r; bool mn; L.chordAt (bar, r, mn);
        const double inBar = std::fmod (barF, 1.0);
        if (inBar > 0.4 && inBar < 0.45)
        {
            ++loonN; loonOk += loon.uiRoot.load() == r && (loon.uiMinor.load() != 0) == mn;
            if (! L.isOut (bar)) { ++swN; swOk += sw.uiOmniRoot.load() == r && (sw.uiOmniType.load() == 1) == mn; }
        }
        swRms[(size_t) bar] += bS.getRMSLevel (0, 0, bs);
        double wet = 0;
        for (int i = 0; i < bs; ++i) { const float d = bT.getSample (0, i) - tIn.getSample (0, i); wet += d * d; }
        tapeWet[(size_t) juce::jmin (bars * 4 - 1, (int) (head.ppq))] += wet;
        if (car.uiState.load() == cr::Recording && carRecStartBar < 0) carRecStartBar = (float) barF;
        carLen = car.uiLenSec.load(); carState = car.uiState.load();
    }
    std::printf ("Loon follows the chord: %d/%d checks\n", loonOk, loonN);
    std::printf ("Schaltwerk Omnichord on the chord: %d/%d checks\n", swOk, swN);
    std::printf ("Schaltwerk level per bar:");
    for (int b = 0; b < bars; ++b) std::printf (" %.3f", swRms[(size_t) b] / 344);
    std::printf ("\nTape Twin echo per beat (x = section change or drop next bar):\n  ");
    for (int q = 0; q < bars * 4; ++q) std::printf ("%s%.2f ", q % 16 == 0 ? "| " : "", tapeWet[(size_t) q]);
    std::printf ("\nCarousel: recording started at bar %.2f, loop %.3f s (one 4-bar section = 8.000 s), state %s\n", carRecStartBar, carLen, cr::stateName (carState));
    std::printf ("Lathe by section: verse %.1f, chorus %.1f, bridge %.1f LUFS\n", lathe.uiSecLufs[0].load(), lathe.uiSecLufs[1].load(), lathe.uiSecLufs[2].load());
    std::printf ("present:");
    const char* names[] = { "TeaTowel", "Undertow", "Loon", "Schaltwerk", "Carousel", "TapeTwin", "Lathe", "Bandleader" };
    for (int w = 0; w < band::NumWho; ++w) std::printf (" %s=%d", names[w], band::isPresent ((band::Who) w));
    std::printf ("\n");

    std::unique_ptr<juce::AudioProcessorEditor> ed (lead.createEditor());
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    auto pf = juce::File::getCurrentWorkingDirectory().getChildFile ("bandleader_ui.png"); pf.deleteFile();
    juce::FileOutputStream os (pf); juce::PNGImageFormat().writeImageToStream (img, os);
    return 0;
}
