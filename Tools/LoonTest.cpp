// Offline test for Loon: a plucked-string "guitar" plays a progression; check key detection,
// levels and stability; also exercise Freeze. Saves a demo and UI snapshots.
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <random>
#include "../Loon/LoonProcessor.h"
#include "../Loon/LoonEditor.h"

struct KS   // Karplus-Strong string
{
    std::vector<float> d; int p = 0; float last = 0;
    void pluck (double f, double sr, std::mt19937& rng)
    {
        d.assign ((size_t) std::max (2.0, sr / f), 0.0f);
        std::uniform_real_distribution<float> u (-1, 1);
        for (auto& x : d) x = u (rng) * 0.5f;
        p = 0;
    }
    float tick() { if (d.empty()) return 0; const float y = d[(size_t) p]; const int q = (p + 1) % (int) d.size(); d[(size_t) p] = 0.996f * 0.5f * (y + d[(size_t) q]); p = q; return y; }
};

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const double sr = 48000; const int bs = 256;
    LoonProcessor p;
    p.setPlayConfigDetails (2, 2, sr, bs);
    p.prepareToPlay (sr, bs);
    if (argc > 2) p.setCurrentProgram (std::atoi (argv[2]));
    auto set = [&] (const char* id, float v) { auto* a = p.apvts.getParameter (id); a->setValueNotifyingHost (a->convertTo0to1 (v)); };

    // Am - F - C - G strummed, 2 s each, for 32 s; then silence (it keeps going); freeze at 20 s
    const int progC[4][6] = { { 45, 52, 57, 60, 64, 69 }, { 41, 48, 53, 57, 60, 65 }, { 48, 52, 55, 60, 64, 67 }, { 43, 47, 50, 55, 59, 67 } };
    const int progE[4][6] = { { 40, 47, 52, 56, 59, 64 }, { 45, 52, 57, 61, 64, 69 }, { 47, 54, 59, 63, 66, 71 }, { 49, 56, 61, 64, 68, 73 } };   // E A B C#m
    const bool modulate = std::getenv ("LOON_MODULATE") != nullptr;
    std::mt19937 rng (5);
    KS strings[6];
    const int total = (int) (sr * 44);
    juce::AudioBuffer<float> out (2, total), block (2, bs);
    bool bad = false;
    std::vector<std::pair<double, juce::String>> keyLog;
    int lastChord = -1;
    for (int pos = 0; pos < total; pos += bs)
    {
        const int n = juce::jmin (bs, total - pos);
        const double t = pos / sr;
        const int chord = t < 32 ? (int) (t / 2.0) % 4 : -1;
        if (chord >= 0 && (int) (t / 2.0) != lastChord)
        {
            lastChord = (int) (t / 2.0);
            const auto& prog = (modulate && t >= 16) ? progE : progC;
            for (int s = 0; s < 6; ++s) strings[s].pluck (440.0 * std::pow (2.0, (prog[chord][s] - 69) / 12.0), sr, rng);
        }
        if (! modulate && t >= 20 && t < 20 + bs / sr) set ("freeze", 1);
        if (! modulate && t >= 30 && t < 30 + bs / sr) set ("freeze", 0);
        block.setSize (2, n, false, false, true);
        for (int i = 0; i < n; ++i)
        {
            float x = 0;
            for (auto& s : strings) x += s.tick();
            x *= 0.25f;
            block.setSample (0, i, x); block.setSample (1, i, x);
        }
        juce::MidiBuffer m;
        p.processBlock (block, m);
        for (int c = 0; c < 2; ++c) { out.copyFrom (c, pos, block, c, 0, n); for (int i = 0; i < n; ++i) if (! std::isfinite (block.getSample (c, i))) bad = true; }
        if (pos % (int) (sr * 4) < bs)
            keyLog.push_back ({ t, juce::String (ln::pcName (p.uiRoot.load())) + (p.uiMinor.load() ? "m" : "") + (p.uiFollowing.load() ? " (following)" : "") });
    }
    std::printf ("peak %.2f finite=%s\nkey over time:", out.getMagnitude (0, total), bad ? "NO" : "yes");
    for (auto& k : keyLog) std::printf ("  %.0fs:%s", k.first, k.second.toRawUTF8());
    std::printf ("\n");
    for (int s = 0; s < 44; s += 4) std::printf ("  %2d-%2ds rms %.3f\n", s, s + 4, out.getRMSLevel (0, (int) (s * sr), (int) (4 * sr)));

    auto f = cwd.getChildFile (argc > 1 ? argv[1] : "loon_demo.wav"); f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (out, 0, total);

    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    int idx = 0;
    for (auto* c : ed->getChildren())
        if (auto* b = dynamic_cast<juce::TextButton*> (c))
            if (b->getRadioGroupId() == 88)
            {
                b->setToggleState (true, juce::dontSendNotification); if (b->onClick) b->onClick();
                auto img = ed->createComponentSnapshot (ed->getLocalBounds());
                auto pf = cwd.getChildFile ("loon_ui_" + juce::String (idx++) + ".png"); pf.deleteFile();
                juce::FileOutputStream os (pf); juce::PNGImageFormat().writeImageToStream (img, os);
            }
    return bad ? 2 : 0;
}
