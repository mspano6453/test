// Offline test: Bandleader + Tea Towel + Undertow in one "Live set", following one song.
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Bandleader/BandleaderProcessor.h"
#include "../Undertow/UndertowProcessor.h"
#include "../Source/PluginProcessor.h"

struct Head : juce::AudioPlayHead
{
    double ppq = 0, bpm = 120; bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override { PositionInfo p; p.setIsPlaying (playing); p.setPpqPosition (ppq); p.setBpm (bpm); return p; }
};

static void setP (juce::AudioProcessor& p, juce::AudioProcessorValueTreeState& s, const char* id, float v)
{
    juce::ignoreUnused (p);
    auto* prm = s.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 44100, bpm = 120; const int bs = 256;
    const double spb = sr * 60 / bpm;
    const auto outDir = juce::File::getCurrentWorkingDirectory();

    BandleaderProcessor lead; UndertowProcessor bass; TeaTowelProcessor drums;
    Head head;
    for (juce::AudioProcessor* p : { (juce::AudioProcessor*) &lead, (juce::AudioProcessor*) &bass, (juce::AudioProcessor*) &drums })
    {
        p->setPlayConfigDetails (p == &bass ? 2 : 0, 2, sr, bs);
        p->prepareToPlay (sr, bs);
        p->setPlayHead (&head);
    }
    setP (lead, lead.apvts, "section_bars", 0);   // 4-bar sections so the test moves along

    const int bars = 40, total = (int) (spb * 4 * bars) / bs * bs;
    juce::AudioBuffer<float> mix (2, total), b1 (2, bs), b2 (2, bs), b3 (2, bs);
    std::vector<int> leadSec (bars, -1), drumSec (bars, -1), bassSec (bars, -1);
    std::vector<double> drumE (bars, 0), bassE (bars, 0);
    std::vector<double> kicks, bassNotes;
    std::vector<int> bassRootOk (bars, -1);
    juce::Random order (3);
    struct Cmd { double atBar; BandleaderProcessor::Cmd c; const char* what; };
    const Cmd cmds[] = { { 6.3, BandleaderProcessor::CNext, "NEXT" }, { 13.4, BandleaderProcessor::CDrop, "DROP" },
                         { 16.2, BandleaderProcessor::CAllIn, "ALL IN" }, { 22.1, BandleaderProcessor::CFill, "FILL" },
                         { 27.5, BandleaderProcessor::CBridge, "BRIDGE (manual)" } };
    size_t nextCmd = 0; double bassCpu = 0;
    // Live runs plugins while stopped too: a moment of that before pressing play
    head.playing = false;
    for (int i = 0; i < 20; ++i)
    {
        juce::MidiBuffer m; b1.clear(); b2.clear(); b3.clear();
        bass.processBlock (b2, m); m.clear(); drums.processBlock (b3, m); m.clear(); lead.processBlock (b1, m);
    }
    head.playing = true;
    for (int pos = 0; pos < total; pos += bs)
    {
        head.ppq = pos / spb;
        const double barF = head.ppq / 4;
        if (nextCmd < std::size (cmds) && barF >= cmds[nextCmd].atBar) { lead.command (cmds[nextCmd].c); ++nextCmd; }
        juce::MidiBuffer m1, m2, m3;
        b1.clear(); b2.clear(); b3.clear();
        // Live doesn't promise any order between tracks: shuffle it
        int ord[3] = { 0, 1, 2 };
        for (int i = 2; i > 0; --i) std::swap (ord[i], ord[order.nextInt (i + 1)]);
        for (int o : ord)
        {
            if (o == 0) lead.processBlock (b1, m1);
            if (o == 1) { const auto t0 = juce::Time::getHighResolutionTicks(); bass.processBlock (b2, m2); bassCpu += juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0); }
            if (o == 2) drums.processBlock (b3, m3);
        }
        const int bar = (int) barF;
        if (bar < bars)
        {
            if (std::fmod (barF, 1.0) > 0.4 && std::fmod (barF, 1.0) < 0.6)
            { leadSec[(size_t) bar] = lead.uiSection.load(); drumSec[(size_t) bar] = drums.player.uiSection.load(); bassSec[(size_t) bar] = bass.uiSection.load(); }
            drumE[(size_t) bar] += b3.getRMSLevel (0, 0, bs);
            bassE[(size_t) bar] += b2.getRMSLevel (0, 0, bs);
        }
        for (const auto e : m3) if (e.getMessage().isNoteOn() && e.getMessage().getNoteNumber() == 36) kicks.push_back ((pos + e.samplePosition) / spb);
        for (const auto e : m2) if (e.getMessage().isNoteOn())
        {
            const double q = (pos + e.samplePosition) / spb;
            bassNotes.push_back (q);
            const int b = (int) std::floor ((q + 0.1) / 4);
            if (b < bars && std::fmod (q + 0.1, 4.0) < 0.35 && bassRootOk[(size_t) b] < 0)
            {
                band::Leader L; band::readLeader (L);
                int r; bool mn; L.chordAt (b, r, mn);
                bassRootOk[(size_t) b] = e.getMessage().getNoteNumber() % 12 == r ? 1 : 0;
            }
        }
        for (int c = 0; c < 2; ++c) { mix.copyFrom (c, pos, b3, c, 0, bs); mix.addFrom (c, pos, b2, c, 0, bs); }
    }
    const char* sn = "VCB";
    std::printf ("bar  lead drums bass   drumLvl bassLvl  bassRoot\n");
    int agree = 0, rootOk = 0, rootN = 0;
    for (int b = 0; b < bars; ++b)
    {
        auto c = [&] (int v) { return v >= 0 ? sn[v] : '-'; };
        const bool ag = leadSec[(size_t) b] == drumSec[(size_t) b] && leadSec[(size_t) b] == bassSec[(size_t) b];
        agree += ag;
        if (bassRootOk[(size_t) b] >= 0) { ++rootN; rootOk += bassRootOk[(size_t) b]; }
        std::printf ("%3d   %c    %c     %c    %7.3f %7.3f   %s%s\n", b, c (leadSec[(size_t) b]), c (drumSec[(size_t) b]), c (bassSec[(size_t) b]),
                     drumE[(size_t) b] / 344, bassE[(size_t) b] / 344, bassRootOk[(size_t) b] < 0 ? "-" : bassRootOk[(size_t) b] ? "ok" : "WRONG", ag ? "" : "  <- disagree");
    }
    // how many bass notes land within 30 ms of a kick
    int withKick = 0;
    for (double q : bassNotes) for (double k : kicks) if (std::abs (q - k) * 0.5 < 0.03) { ++withKick; break; }
    std::printf ("sections agree %d/%d bars; bass on chord root at downbeats %d/%d; %zu kicks, %zu bass notes, %d bass notes with a kick\n",
                 agree, bars, rootOk, rootN, kicks.size(), bassNotes.size(), withKick);
    std::printf ("CPU Undertow %.2f%% of real time\n", 100.0 * bassCpu / (total / sr));
    std::printf ("present: teatowel %d undertow %d bandleader %d\n", band::isPresent (band::WTeaTowel), band::isPresent (band::WUndertow), band::isPresent (band::WBandleader));

    {
        auto f = outDir.getChildFile ("band_test.wav"); f.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0));
        w->writeFromAudioSampleBuffer (mix, 0, total);
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed (lead.createEditor());
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    auto pf = outDir.getChildFile ("bandleader_ui.png"); pf.deleteFile();
    juce::FileOutputStream os (pf); juce::PNGImageFormat().writeImageToStream (img, os);
    return 0;
}
