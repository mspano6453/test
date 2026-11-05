// Offline test: renders a groove through the plugin and checks the output.
// Usage: RenderDemo <out.wav> [preset index]
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include <random>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/PluginEditor.h"
#include "../Source/PluginProcessor.h"

struct Ev { double t; int note; float vel; };

static std::vector<Ev> makeGroove (double bpm, std::mt19937& rng)
{
    std::vector<Ev> ev;
    std::normal_distribution<double> jitter (0.0, 0.004);
    std::uniform_real_distribution<float> vj (-0.06f, 0.06f);
    const double s16 = 60.0 / bpm / 4.0;
    auto add = [&] (int bar, int step, int note, float vel)
    { ev.push_back ({ 20.8 + (bar * 16 + step) * s16 + jitter (rng), note, juce::jlimit (0.05f, 1.0f, vel + vj (rng)) }); };

    // isolated hits first: one of each drum
    const int solo[] = { 36, 38, 37, 48, 43, 42, 46, 51, 53, 49 };
    for (int i = 0; i < 10; ++i) ev.push_back ({ 0.25 + i * 2.0, solo[i], 0.85f });

    for (int bar = 0; bar < 8; ++bar)
    {
        const int b = bar;
        const bool fill = bar == 3 || bar == 7;
        const bool ride = bar >= 4 && bar <= 6;
        if (bar == 0 || bar == 4) add (b, 0, 49, 0.9f);

        for (int s = 0; s < 16; s += 2)
        {
            if (fill && s >= 8) break;
            if (ride) add (b, s, 51, s % 4 == 0 ? 0.75f : 0.55f);
            else if (s == 14) add (b, s, 46, 0.7f);                    // loose open hat on the "and" of 4
            else add (b, s, 42, s % 4 == 0 ? 0.75f : 0.5f);
        }
        add (b, 0, 36, 0.95f);
        add (b, 6, 36, 0.7f);
        add (b, 8, 36, 0.9f);
        add (b, 4, 38, 0.9f);
        if (! fill) { add (b, 12, 38, 0.95f); add (b, 15, 38, 0.25f); }

        if (fill)   // tom fill, falling to the floor tom
        {
            const int notes[] = { 38, 38, 48, 48, 48, 45, 43, 43 };
            for (int k = 0; k < 8; ++k) add (b, 8 + k, notes[k], 0.65f + 0.04f * (float) k);
        }
    }
    add (8, 0, 49, 0.95f);
    add (8, 0, 36, 0.95f);
    return ev;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outFile (juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "demo.wav"));

    TeaTowelProcessor proc;
    const double sr = 48000.0;
    const int bs = 256;
    proc.setPlayConfigDetails (0, 2, sr, bs);
    proc.prepareToPlay (sr, bs);
    if (argc > 2) proc.setCurrentProgram (std::atoi (argv[2]));

    if (std::getenv ("TT_SNAPSHOT") != nullptr)   // debug: save the three UI pages as PNGs
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        const char* names[] = { "kit", "edit", "fx" };
        for (int page = 0; page < 3; ++page)
        {
            juce::Component* tabs[3] {};
            int ti = 0;
            for (auto* c : ed->getChildren())
                if (auto* b = dynamic_cast<juce::TextButton*> (c))
                    if (b->getRadioGroupId() == 1001 && ti < 3) tabs[ti++] = b;
            if (auto* b = dynamic_cast<juce::TextButton*> (tabs[page])) { b->setToggleState (true, juce::dontSendNotification); if (b->onClick) b->onClick(); }
            auto img = ed->createComponentSnapshot (ed->getLocalBounds());
            juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (juce::String ("ui_") + names[page] + ".png"));
            f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat().writeImageToStream (img, os);
        }
        return 0;
    }
    if (const char* off = std::getenv ("TT_ZERO"))   // debug: zero listed params, e.g. TT_ZERO=m_roomlvl,m_squash
        for (auto id : juce::StringArray::fromTokens (off, ",", ""))
            if (auto* prm = proc.apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (0.0f));

    std::mt19937 rng (7);
    auto events = makeGroove (96.0, rng);
    std::sort (events.begin(), events.end(), [] (auto& a, auto& b) { return a.t < b.t; });
    const double total = events.back().t + 3.0;
    const int totalSamples = (int) (total * sr);

    juce::AudioBuffer<float> out (2, totalSamples);
    juce::AudioBuffer<float> block (2, bs);
    size_t next = 0;
    for (int pos = 0; pos < totalSamples; pos += bs)
    {
        const int n = juce::jmin (bs, totalSamples - pos);
        juce::MidiBuffer midi;
        while (next < events.size() && (int) (events[next].t * sr) < pos + n)
        {
            const int at = juce::jmax (0, (int) (events[next].t * sr) - pos);
            midi.addEvent (juce::MidiMessage::noteOn (10, events[next].note, events[next].vel), at);
            ++next;
        }
        block.setSize (2, n, false, false, true);
        proc.processBlock (block, midi);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, block, c, 0, n);
    }

    // sanity: no NaN/inf, sensible peak, and per-hit peaks for the solo section
    bool bad = false;
    float peak = 0;
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < totalSamples; ++i)
        {
            const float x = out.getSample (c, i);
            if (! std::isfinite (x)) bad = true;
            peak = juce::jmax (peak, std::abs (x));
        }
    std::printf ("samples=%d peak=%.3f (%.1f dBFS) finite=%s\n", totalSamples, peak,
                 juce::Decibels::gainToDecibels (peak), bad ? "NO" : "yes");
    const char* names[] = { "kick", "snare", "sidestick", "rack tom", "floor tom", "hat closed", "hat open", "ride", "ride bell", "crash" };
    for (int i = 0; i < 10; ++i)
    {
        const int s0 = (int) ((0.25 + i * 2.0) * sr), s1 = s0 + (int) (1.9 * sr);
        float pk = 0; int last = s0;
        for (int k = s0; k < s1; ++k)
        {
            const float x = juce::jmax (std::abs (out.getSample (0, k)), std::abs (out.getSample (1, k)));
            pk = juce::jmax (pk, x);
        }
        for (int k = s0; k < s1; ++k)
            if (juce::jmax (std::abs (out.getSample (0, k)), std::abs (out.getSample (1, k))) > pk * 0.03f) last = k;
        std::printf ("  %-11s peak %6.1f dB   audible ~%4.0f ms\n", names[i], juce::Decibels::gainToDecibels (pk),
                     (last - s0) * 1000.0 / sr);
    }

    outFile.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (outFile), sr, 2, 24, {}, 0));
    if (w == nullptr) { std::printf ("could not write wav\n"); return 1; }
    w->writeFromAudioSampleBuffer (out, 0, totalSamples);
    std::printf ("wrote %s\n", outFile.getFullPathName().toRawUTF8());
    return bad ? 2 : 0;
}
