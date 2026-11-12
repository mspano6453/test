#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>
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

// a rolling record of the plugin's MIDI output, stamped in beats (Live's song position)
class Capture
{
public:
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
        std::vector<E> ev; ev.reserve (n);
        for (uint32_t k = end - n; k != end; ++k) ev.push_back (ring[k % kN]);
        juce::MidiFile file; file.setTicksPerQuarterNote (960);
        juce::MidiMessageSequence seq;
        seq.addEvent (juce::MidiMessage::textMetaEvent (3, trackName), 0);
        seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) (60000000.0 / juce::jmax (20.0f, lastBpm.load()))), 0);
        seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);
        if (ev.empty()) { file.addTrack (seq); return file; }
        double last = 0; for (auto& e : ev) last = juce::jmax (last, e.ppq);
        const double endBar = std::ceil ((last + 0.001) / 4.0) * 4.0;
        double from = bars > 0 ? endBar - bars * 4.0 : std::floor (takeStart.load() / 4.0) * 4.0;
        if (bars <= 0 && from > last) from = endBar - 16.0;
        // pair the note-ons with their note-offs
        struct Open { double t; int vel; bool on = false; };
        Open open[16][128];
        auto put = [&] (int ch, int note, double t0, double t1, int vel)
        {
            if (t1 <= from || t0 >= endBar) return;
            t0 = juce::jmax (t0, from); t1 = juce::jmin (juce::jmax (t1, t0 + 1.0 / 32.0), endBar);
            seq.addEvent (juce::MidiMessage::noteOn (ch, note, (juce::uint8) juce::jlimit (1, 127, vel)), (t0 - from) * 960.0);
            seq.addEvent (juce::MidiMessage::noteOff (ch, note), (t1 - from) * 960.0);
        };
        for (auto& e : ev)
        {
            const int ch = juce::jlimit (1, 16, (int) e.ch) - 1;
            auto& o = open[ch][e.note & 127];
            if (o.on) { put (ch + 1, e.note, o.t, e.ppq, o.vel); o.on = false; }
            if (e.type == 1 && e.vel > 0) { o.on = true; o.t = e.ppq; o.vel = e.vel; }
        }
        for (int ch = 0; ch < 16; ++ch) for (int nn = 0; nn < 128; ++nn) if (open[ch][nn].on) put (ch + 1, nn, open[ch][nn].t, endBar, open[ch][nn].vel);   // still sounding: hold to the end of the clip
        seq.updateMatchedPairs();
        seq.sort();
        file.addTrack (seq);
        return file;
    }

    // writes the clip to Documents\DEMOCRAZY\MIDI and returns the file
    juce::File writeFile (int bars, const juce::String& pluginName) const
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("DEMOCRAZY").getChildFile ("MIDI");
        dir.createDirectory();
        const auto stamp = juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S");
        auto f = dir.getChildFile (pluginName + " " + (bars > 0 ? juce::String (bars) + " bars " : juce::String ("take ")) + stamp + ".mid");
        auto file = makeFile (bars, pluginName);
        f.deleteFile();
        juce::FileOutputStream os (f);
        if (os.openedOk()) { file.writeTo (os, 1); os.flush(); }
        return f;
    }

private:
    struct E { double ppq; uint8_t type, note, vel, ch; };
    static constexpr uint32_t kN = 16384;
    E ring[kN] {};
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
} // namespace mc
