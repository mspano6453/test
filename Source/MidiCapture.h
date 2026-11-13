#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "SongForm.h"
#include <atomic>
#include <vector>

// ---------------------------------------------------------------------------
// Getting the notes out. Every player plugin:
//  1. sends what it plays out of its MIDI output (so Live can record it from
//     another MIDI track: MIDI From = this track, then pick the plugin), and
//  2. remembers the last few minutes of what it played, so the DRAG MIDI
//     button can hand Live a ready-made clip (.mid file) by drag and drop.
// ---------------------------------------------------------------------------
namespace mc
{
// note-offs for notes we started with a known length (the audio thread's helper)
class NoteOut
{
public:
    NoteOut() { act.reserve (64); }
    void on (juce::MidiBuffer& out, int pos, int64_t now, int note, float vel, double lenSec, double sr, int ch = 1)
    {
        if (note < 0 || note > 127) return;
        off (out, pos, note, ch);
        out.addEvent (juce::MidiMessage::noteOn (ch, note, (juce::uint8) juce::jlimit (1, 127, (int) std::round (vel * 127.0f))), pos);
        if (act.size() < 64) act.push_back ({ note, ch, lenSec > 0 ? now + (int64_t) (lenSec * sr) : -1 });
    }
    void off (juce::MidiBuffer& out, int pos, int note, int ch = 1)
    {
        for (size_t i = 0; i < act.size(); ++i)
            if (act[i].note == note && act[i].ch == ch) { out.addEvent (juce::MidiMessage::noteOff (ch, note), pos); act.erase (act.begin() + (long) i); return; }
    }
    void allOff (juce::MidiBuffer& out, int pos)
    {
        for (auto& a : act) out.addEvent (juce::MidiMessage::noteOff (a.ch, a.note), pos);
        act.clear();
    }
    // emit the note-offs that are due at this sample
    void tick (juce::MidiBuffer& out, int pos, int64_t now)
    {
        for (size_t i = 0; i < act.size();)
            if (act[i].off >= 0 && act[i].off <= now) { out.addEvent (juce::MidiMessage::noteOff (act[i].ch, act[i].note), pos); act.erase (act.begin() + (long) i); }
            else ++i;
    }
    bool any() const { return ! act.empty(); }
private:
    struct A { int note, ch; int64_t off; };
    std::vector<A> act;
};

// one note event, stamped in beats (quarter notes from the start of the song)
struct NoteEv { double ppq; uint8_t type, note, vel, ch; };   // type 1 = on, 0 = off

inline void timeSig (juce::MidiMessageSequence& seq, int meter)
{
    if (meter == band::M34) seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (3, 4), 0);
    else if (meter == band::M68) seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (6, 8), 0);
    else seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);
}

// notes between `from` and `to` (in beats) as a MIDI file: 960 ticks a beat, tempo and meter set,
// note-ons paired with their note-offs, anything still sounding held to the end
inline juce::MidiFile notesToFile (const std::vector<NoteEv>& ev, double from, double to, double bpm, int meter, const juce::String& trackName)
{
    juce::MidiFile file; file.setTicksPerQuarterNote (960);
    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::textMetaEvent (3, trackName), 0);
    seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) (60000000.0 / juce::jmax (20.0, bpm))), 0);
    timeSig (seq, meter);
    struct Open { double t; int vel; bool on = false; };
    std::vector<Open> open (16 * 128);
    auto put = [&] (int ch, int note, double t0, double t1, int vel)
    {
        if (t1 <= from || t0 >= to) return;
        t0 = juce::jmax (t0, from); t1 = juce::jmin (juce::jmax (t1, t0 + 1.0 / 32.0), to);
        seq.addEvent (juce::MidiMessage::noteOn (ch, note, (juce::uint8) juce::jlimit (1, 127, vel)), (t0 - from) * 960.0);
        seq.addEvent (juce::MidiMessage::noteOff (ch, note), (t1 - from) * 960.0);
    };
    for (auto& e : ev)
    {
        const int ch = juce::jlimit (1, 16, (int) e.ch) - 1;
        auto& o = open[(size_t) (ch * 128 + (e.note & 127))];
        if (o.on) { put (ch + 1, e.note, o.t, e.ppq, o.vel); o.on = false; }
        if (e.type == 1 && e.vel > 0) { o.on = true; o.t = e.ppq; o.vel = e.vel; }
    }
    for (int ch = 0; ch < 16; ++ch) for (int nn = 0; nn < 128; ++nn) { auto& o = open[(size_t) (ch * 128 + nn)]; if (o.on) put (ch + 1, nn, o.t, to, o.vel); }
    seq.updateMatchedPairs();
    seq.sort();
    file.addTrack (seq);
    return file;
}

inline bool writeMidi (const juce::MidiFile& mf, const juce::File& f)
{
    f.deleteFile();
    juce::FileOutputStream os (f);
    if (! os.openedOk()) return false;
    mf.writeTo (os, 1); os.flush();
    return true;
}

inline juce::File midiFolder()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("DEMOCRAZY").getChildFile ("MIDI");
    dir.createDirectory();
    return dir;
}

// a rolling record of the plugin's MIDI output, stamped in beats (Live's song position)
class Capture
{
public:
    void setMeter (int m) { meter.store (m, std::memory_order_relaxed); }
    // audio thread: call once per block with the MIDI the plugin is sending out
    void recordBlock (const juce::MidiBuffer& out, double ppqStart, bool playing, double bpm, double sr, int numSamples)
    {
        const double spb = sr * 60.0 / juce::jmax (20.0, bpm);
        if (! playing) ppqStart = freePpq;               // stopped: keep counting so played notes still land in order
        for (const auto m : out)
        {
            const auto& msg = m.getMessage();
            if (! (msg.isNoteOn() || msg.isNoteOff())) continue;
            const uint32_t i = w.load (std::memory_order_relaxed);
            ring[i % kN] = { ppqStart + m.samplePosition / spb, (uint8_t) (msg.isNoteOn() ? 1 : 0), (uint8_t) msg.getNoteNumber(),
                             (uint8_t) msg.getVelocity(), (uint8_t) msg.getChannel() };
            w.store (i + 1, std::memory_order_release);
        }
        lastBpm.store ((float) bpm);
        freePpq = (playing ? ppqStart : freePpq) + numSamples / spb;
        if (playing && ppqStart < lastPpq.load() - 1.0) takeStart.store (ppqStart);   // jumped back: a new take
        if (playing && ! wasPlaying) takeStart.store (ppqStart);
        wasPlaying = playing;
        lastPpq.store (freePpq);
    }

    // the same, reading Live's position from the play head
    void recordWithHost (const juce::MidiBuffer& out, juce::AudioPlayHead* ph, double sr, int numSamples)
    {
        double ppq = 0, bpm = lastBpm.load(); bool pl = false;
        if (ph != nullptr)
            if (auto pos = ph->getPosition())
            {
                pl = pos->getIsPlaying();
                if (auto q = pos->getPpqPosition()) ppq = *q; else pl = false;
                if (auto b = pos->getBpm()) bpm = *b;
            }
        recordBlock (out, ppq, pl, bpm, sr, numSamples);
    }

    int eventCount() const { return (int) juce::jmin ((uint32_t) kN, w.load()); }

    // message thread: the last `bars` bars (0 = this whole take) as a MIDI file
    juce::MidiFile makeFile (int bars, const juce::String& trackName) const
    {
        const uint32_t end = w.load (std::memory_order_acquire);
        const uint32_t n = juce::jmin ((uint32_t) kN, end);
        std::vector<NoteEv> ev; ev.reserve (n);
        for (uint32_t k = end - n; k != end; ++k) ev.push_back (ring[k % kN]);
        const int m = meter.load();
        const double bp = band::meterBarPpq (m);
        if (ev.empty()) return notesToFile (ev, 0, bp, lastBpm.load(), m, trackName);
        double last = 0; for (auto& e : ev) last = juce::jmax (last, e.ppq);
        const double endBar = std::ceil ((last + 0.001) / bp) * bp;
        double from = bars > 0 ? endBar - bars * bp : std::floor (takeStart.load() / bp) * bp;
        if (bars <= 0 && from > last) from = endBar - 4 * bp;
        return notesToFile (ev, from, endBar, lastBpm.load(), m, trackName);
    }

    // writes the clip to Documents\DEMOCRAZY\MIDI and returns the file
    juce::File writeFile (int bars, const juce::String& pluginName) const
    {
        auto dir = midiFolder();
        const auto stamp = juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S");
        auto f = dir.getChildFile (pluginName + " " + (bars > 0 ? juce::String (bars) + " bars " : juce::String ("take ")) + stamp + ".mid");
        writeMidi (makeFile (bars, pluginName), f);
        return f;
    }

private:
    using E = NoteEv;
    static constexpr uint32_t kN = 32768;
    E ring[kN] {};
    std::atomic<int> meter { 0 };
    std::atomic<uint32_t> w { 0 };
    std::atomic<double> lastPpq { 0 }, takeStart { 0 };
    std::atomic<float> lastBpm { 120 };
    double freePpq = 0;
    bool wasPlaying = false;
};

// The DRAG MIDI button. Drag it onto a Live track to drop in a clip of what
// the plugin just played. Click the number to choose how many bars.
class DragButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    DragButton (const Capture& c, juce::String pluginName, juce::Colour accent) : cap (c), name (std::move (pluginName)), col (accent)
    {
        setTooltip ("Drag onto a MIDI track in Live to get a clip of what " + name + " just played. Click the number to change how many bars.");
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }
    std::function<void (const juce::String&)> onMessage;   // the editor can show a status line

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (col.withAlpha (hover ? 0.32f : 0.18f)); g.fillRoundedRectangle (r, 5.0f);
        g.setColour (col); g.drawRoundedRectangle (r, 5.0f, 1.2f);
        auto barsR = r.removeFromRight (r.getWidth() * 0.36f);
        g.drawVerticalLine ((int) barsR.getX(), r.getY() + 3, r.getBottom() - 3);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText ("DRAG MIDI", r, juce::Justification::centred);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (bars[bi] > 0 ? juce::String (bars[bi]) + " BARS" : juce::String ("TAKE"), barsR, juce::Justification::centred);
    }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override { dragged = false; onBars = e.x > getWidth() * 0.64f; }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragged) return;
        if (onBars) { bi = (bi + 1) % 5; repaint(); return; }
        // a click saves the clip and says where it went
        auto f = cap.writeFile (bars[bi], name);
        if (onMessage) onMessage ("Saved " + f.getFileName() + " in Documents\\DEMOCRAZY\\MIDI");
        else f.revealToUser();      // no status line on this screen: show the file in Explorer
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragged || onBars || e.getDistanceFromDragStart() < 6) return;
        dragged = true;
        if (cap.eventCount() == 0) { if (onMessage) onMessage ("Nothing played yet - press play first."); return; }
        auto f = cap.writeFile (bars[bi], name);
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
    }
    juce::File saveNow() const { return cap.writeFile (bars[bi], name); }   // for tests
    void setBarsIndex (int i) { bi = juce::jlimit (0, 4, i); repaint(); }

private:
    const Capture& cap;
    juce::String name;
    juce::Colour col;
    static constexpr int bars[5] = { 8, 16, 32, 4, 0 };
    int bi = 0;
    bool hover = false, dragged = false, onBars = false;
};

// ---------------------------------------------------------------------------
// RENDER SONG. Bandleader posts a job on the band bus; each player's renderer
// sees it, makes a private copy of its plugin (same settings), and runs that
// copy through the whole song as fast as the computer can, with the song form
// worked out bar by bar. What the copy plays is written as one .mid file.
// ---------------------------------------------------------------------------
inline std::atomic<int>& makingOffline() { static std::atomic<int> n { 0 }; return n; }

class FakeHead : public juce::AudioPlayHead
{
public:
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setIsPlaying (playing);
        p.setPpqPosition (ppq);
        p.setBpm (bpm);
        p.setTimeInSamples (samples);
        p.setTimeInSeconds ((double) samples / sr);
        const int m = meter;
        p.setTimeSignature (juce::AudioPlayHead::TimeSignature { m == band::M44 ? 4 : m == band::M34 ? 3 : 6, m == band::M68 ? 8 : 4 });
        const double bp = band::meterBarPpq (m);
        p.setPpqPositionOfLastBarStart (std::floor (ppq / bp) * bp);
        p.setBarCount ((int64_t) std::floor (ppq / bp));
        return p;
    }
    bool playing = false; double ppq = 0, bpm = 120, sr = 48000; int64_t samples = 0; int meter = 0;
};

class SongRenderer : private juce::Timer, private juce::Thread, private juce::AsyncUpdater
{
public:
    using Make = std::function<std::unique_ptr<juce::AudioProcessor>()>;
    using Prep = std::function<void (juce::AudioProcessor&)>;
    // who: which band member this is. file: the name of its part ("Drums", "Bass"...).
    // prep: set the copy up to follow the band (not the guitar or MIDI in). kicks: this is the
    // drummer (others wait for its kicks), or a player who locks to them.
    SongRenderer (juce::AudioProcessor& p, band::Who w, juce::String file, Make mk, Prep pr = {}, bool isDrummer = false, bool locksToKicks = false)
        : juce::Thread ("DEMOCRAZY render"), self (p), who (w), name (std::move (file)), make (std::move (mk)), prep (std::move (pr)),
          drummer (isDrummer), lock (locksToKicks)
    {
        if (makingOffline().load() > 0) return;     // the private copy doesn't render itself
        band::RenderJob j;
        if (band::readRender (j)) lastStamp = j.stamp;   // a job from before we existed isn't ours
        startTimerHz (4);
    }
    ~SongRenderer() override { stopTimer(); stopThread (4000); cancelPendingUpdate(); inst.reset(); }

    static juce::String partFileName (int order, const juce::String& part) { return juce::String (order).paddedLeft ('0', 2) + " " + part; }

private:
    void timerCallback() override
    {
        if (isThreadRunning() || inst != nullptr) return;
        band::RenderJob j;
        if (! band::readRender (j) || j.stamp == 0 || j.stamp == lastStamp) return;
        lastStamp = j.stamp;
        job = j;
        juce::MemoryBlock st;
        self.getStateInformation (st);
        ++makingOffline();
        inst = make();
        --makingOffline();
        if (inst == nullptr) { band::setRenderState (who, band::RFailed); return; }
        inst->setStateInformation (st.getData(), (int) st.getSize());
        if (prep) prep (*inst);
        band::setRenderState (who, band::RBusy);
        startThread();
    }

    void run() override
    {
        bool ok = renderSong();
        if (drummer) band::setRenderKicksDone (true);
        band::setRenderState (who, ok ? band::RDone : band::RFailed);
        triggerAsyncUpdate();
    }
    void handleAsyncUpdate() override { if (! isThreadRunning()) inst.reset(); }

    bool renderSong()
    {
        if (inst == nullptr) return false;
        // the bass waits for the drummer's kicks, so it can lock to them like it does live
        if (lock && band::isPresent (band::WTeaTowel))
            for (int i = 0; i < 600 && ! band::renderKicksDone() && ! threadShouldExit(); ++i) wait (100);
        const double sr = 48000.0; const int bs = 512;
        const int meter = job.song.meter;
        const double bp = band::meterBarPpq (meter);
        const double bpm = juce::jlimit (30.0, 300.0, (double) job.bpm);
        const double songEnd = job.totalBars * bp;
        const double spq = sr * 60.0 / bpm;                    // samples per beat
        FakeHead ph; ph.bpm = bpm; ph.sr = sr; ph.meter = meter;
        inst->setPlayHead (&ph);
        inst->setRateAndBufferSizeDetails (sr, bs);
        inst->prepareToPlay (sr, bs);
        const auto song = band::buildSong (job.spec);
        band::Sandbox sb; sb.active = true; sb.leader = job.song; sb.leader.playing = 1;
        band::enterSandbox (&sb);
        juce::AudioBuffer<float> buf (juce::jmax (2, inst->getTotalNumInputChannels(), inst->getTotalNumOutputChannels()), bs);
        juce::MidiBuffer midi;
        std::vector<NoteEv> ev; ev.reserve (20000);
        const double tail = songEnd + bp * 2.0;                // let the last notes ring and end
        for (double ppq = 0; ppq < tail && ! threadShouldExit(); ppq += bs / spq)
        {
            band::applySong (sb.leader, song, band::barOfPpq (ppq, meter));
            sb.leader.heartbeatMs = band::nowMs();
            ph.playing = true; ph.ppq = ppq; ph.samples = (int64_t) std::llround (ppq * spq);
            buf.clear(); midi.clear();
            inst->processBlock (buf, midi);
            for (const auto m : midi)
            {
                const auto& msg = m.getMessage();
                if (! (msg.isNoteOn() || msg.isNoteOff())) continue;
                ev.push_back ({ ppq + m.samplePosition / spq, (uint8_t) (msg.isNoteOn() ? 1 : 0), (uint8_t) msg.getNoteNumber(),
                                (uint8_t) msg.getVelocity(), (uint8_t) msg.getChannel() });
            }
        }
        // stop: let the plugin let go of anything it's holding
        ph.playing = false; buf.clear(); midi.clear(); inst->processBlock (buf, midi);
        band::enterSandbox (nullptr);
        inst->releaseResources();
        inst->setPlayHead (nullptr);
        if (threadShouldExit()) return false;
        juce::File dir = juce::File (juce::String::fromUTF8 (job.folder));
        if (! dir.createDirectory()) return false;
        auto f = dir.getNonexistentChildFile (name, ".mid", false);
        return writeMidi (notesToFile (ev, 0.0, juce::jmax (bp, songEnd), bpm, meter, name), f);
    }

    juce::AudioProcessor& self;
    band::Who who;
    juce::String name;
    Make make; Prep prep;
    bool drummer, lock;
    uint32_t lastStamp = 0;
    band::RenderJob job;
    std::unique_ptr<juce::AudioProcessor> inst;
};

// set a choice parameter on a private copy (by item index)
inline void setChoice (juce::AudioProcessor& p, const char* id, int index)
{
    for (auto* prm : p.getParameters())
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (prm))
            if (c->getParameterID() == id) { *c = index; return; }
}
inline int getChoice (juce::AudioProcessor& p, const char* id)
{
    for (auto* prm : p.getParameters())
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (prm))
            if (c->getParameterID() == id) return c->getIndex();
    return -1;
}
} // namespace mc
