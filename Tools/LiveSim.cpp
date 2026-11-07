// Live-like stress test: all eight plugins processing at the same time on separate threads
// (the way Live spreads tracks over CPU cores), with their editors open and painting on the
// message thread. Reports the slowest block for each plugin and any stall.
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <condition_variable>
#include <mutex>
#include <thread>
#include "../Bandleader/BandleaderProcessor.h"
#include "../Source/PluginProcessor.h"
#include "../Undertow/UndertowProcessor.h"
#include "../Loon/LoonProcessor.h"
#include "../Schaltwerk/SchaltwerkProcessor.h"
#include "../Carousel/CarouselProcessor.h"
#include "../TapeTwin/TapeTwinProcessor.h"
#include "../Lathe/LatheProcessor.h"

struct Head : juce::AudioPlayHead
{
    std::atomic<double> ppq { 0 }; std::atomic<bool> playing { false }; double bpm = 120;
    juce::Optional<PositionInfo> getPosition() const override
    { PositionInfo p; p.setIsPlaying (playing.load()); p.setPpqPosition (ppq.load()); p.setBpm (bpm); return p; }
};

struct Gate
{
    std::mutex m; std::condition_variable cv; int64_t gen = 0; int done = 0; bool quit = false;
};

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 44100; const int bs = 128;
    const double spb = sr * 60 / 120.0;
    const bool withUi = ! (argc > 1 && juce::String (argv[1]) == "noui");

    BandleaderProcessor lead; TeaTowelProcessor drums; UndertowProcessor bass; LoonProcessor loon; SchaltwerkProcessor sw;
    CarouselProcessor car; TapeTwinProcessor tape; LatheProcessor lathe;
    Head head;
    struct P { juce::AudioProcessor* p; int ins; const char* name; };
    std::vector<P> all = { { &lead, 0, "Bandleader" }, { &drums, 0, "Tea Towel" }, { &bass, 2, "Undertow" }, { &loon, 2, "Loon" },
                           { &sw, 2, "Schaltwerk" }, { &car, 2, "Carousel" }, { &tape, 2, "Tape Twin" }, { &lathe, 2, "Lathe" } };
    for (auto& x : all) { x.p->setPlayConfigDetails (x.ins, 2, sr, bs); x.p->prepareToPlay (sr, bs); x.p->setPlayHead (&head); }

    std::vector<std::unique_ptr<juce::AudioProcessorEditor>> eds;
    if (withUi) for (auto& x : all) eds.emplace_back (x.p->createEditor());

    const int N = (int) all.size();
    Gate gate;
    std::vector<double> worst ((size_t) N, 0), total ((size_t) N, 0);
    std::vector<std::thread> threads;
    for (int t = 0; t < N; ++t)
        threads.emplace_back ([&, t]
        {
            juce::AudioBuffer<float> buf (2, bs);
            juce::MidiBuffer midi;
            juce::Random rnd (t + 1);
            int64_t seen = 0;
            for (;;)
            {
                {
                    std::unique_lock<std::mutex> lk (gate.m);
                    gate.cv.wait (lk, [&] { return gate.quit || gate.gen != seen; });
                    if (gate.quit) return;
                    seen = gate.gen;
                }
                for (int c = 0; c < 2; ++c) for (int i = 0; i < bs; ++i) buf.setSample (c, i, (rnd.nextFloat() - 0.5f) * 0.05f);
                midi.clear();
                const auto t0 = juce::Time::getHighResolutionTicks();
                juce::ScopedNoDenormals nd;
                all[(size_t) t].p->processBlock (buf, midi);
                const double d = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
                total[(size_t) t] += d; worst[(size_t) t] = std::max (worst[(size_t) t], d);
                {
                    std::lock_guard<std::mutex> lk (gate.m);
                    ++gate.done;
                }
                gate.cv.notify_all();
            }
        });

    // the audio "driver": one block at a time, everyone at once, roughly in real time
    std::atomic<bool> finished { false };
    double maxCycle = 0; int overruns = 0; int64_t blocks = 0;
    std::thread driver ([&]
    {
        const int stoppedBlocks = (int) (2.0 * sr / bs), playBlocks = (int) (30.0 * sr / bs);
        const double blockSec = bs / sr;
        auto next = std::chrono::steady_clock::now();
        for (int b = 0; b < stoppedBlocks + playBlocks; ++b)
        {
            const bool play = b >= stoppedBlocks;
            head.playing = play;
            head.ppq = play ? (double) (b - stoppedBlocks) * bs / spb : 0.0;
            if (b == stoppedBlocks + (int) (6.3 * 2 * sr / bs)) lead.command (BandleaderProcessor::CNext);
            const auto t0 = juce::Time::getHighResolutionTicks();
            {
                std::lock_guard<std::mutex> lk (gate.m);
                gate.done = 0; ++gate.gen;
            }
            gate.cv.notify_all();
            {
                std::unique_lock<std::mutex> lk (gate.m);
                if (! gate.cv.wait_for (lk, std::chrono::seconds (3), [&] { return gate.done == N; }))
                { std::printf ("STALL: a plugin didn't finish its block within 3 s (block %d)\n", b); std::fflush (stdout); }
            }
            const double cyc = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
            maxCycle = std::max (maxCycle, cyc);
            if (cyc > blockSec) ++overruns;
            ++blocks;
            next += std::chrono::microseconds ((int64_t) (blockSec * 1e6));
            std::this_thread::sleep_until (next);
        }
        finished = true;
    });

    // the message thread: editors' timers run and they paint, like Live's UI
    int paints = 0; double worstPaint = 0; double paintT[8] {}, paintW[8] {}; int paintN[8] {};
    while (! finished.load())
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (30);
        if (withUi)
            for (size_t ei = 0; ei < eds.size(); ++ei)
            {
                auto& e = eds[ei];
                const auto t0 = juce::Time::getHighResolutionTicks();
                auto img = e->createComponentSnapshot (e->getLocalBounds(), true, 1.0f);
                const double d = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
                worstPaint = std::max (worstPaint, d);
                paintT[ei] += d; paintW[ei] = std::max (paintW[ei], d); ++paintN[ei];
                ++paints;
            }
    }
    driver.join();
    { std::lock_guard<std::mutex> lk (gate.m); gate.quit = true; }
    gate.cv.notify_all();
    for (auto& t : threads) t.join();

    std::printf ("blocks %lld, block length %.2f ms, slowest whole cycle %.2f ms, cycles over budget %d\n",
                 (long long) blocks, bs / sr * 1000, maxCycle * 1000, overruns);
    for (int t = 0; t < N; ++t)
        std::printf ("  %-10s avg %.3f ms  worst %.3f ms\n", all[(size_t) t].name, total[(size_t) t] / (double) blocks * 1000, worst[(size_t) t] * 1000);
    std::printf ("editor paints %d, slowest %.1f ms\n", paints, worstPaint * 1000);
    for (size_t ei = 0; ei < eds.size(); ++ei)
        std::printf ("  %-10s full repaint avg %.1f ms, worst %.1f ms\n", all[ei].name, paintT[ei] / juce::jmax (1, paintN[ei]) * 1000, paintW[ei] * 1000);
    eds.clear();
    return 0;
}
