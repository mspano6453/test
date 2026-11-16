#include "BandleaderEditor.h"

using ttui::font;
namespace col = ttui::col;

namespace
{
const juce::Colour kGold { 0xffd8b04a };
const juce::Colour kGreen { 0xff7fbf6a };
const char* kSecNames[3] = { "VERSE", "CHORUS", "BRIDGE" };
const char* kSecShort[3] = { "V", "C", "B" };

struct Member { const char* name; band::Who who; const char* does; };
const Member kMembers[] = { { "Tea Towel", band::WTeaTowel, "drums" },
                            { "Undertow", band::WUndertow, "bass" },
                            { "Jangle", band::WJangle, "rhythm guitar" },
                            { "Parlour", band::WParlour, "keys" },
                            { "Chamber", band::WChamber, "strings, brass, woodwinds" },
                            { "Harmonia", band::WHarmonia, "backing vocals" },
                            { "Ether", band::WEther, "early electronic" },
                            { "Schaltwerk", band::WSchaltwerk, "drum box, Strumchord" },
                            { "Loon", band::WLoon, "ambience in key" },
                            { "Carousel", band::WCarousel, "looper" },
                            { "Tape Twin", band::WTapeTwin, "echo throws" },
                            { "Swirl", band::WSwirl, "backwards, phase, rotary" },
                            { "Lathe", band::WLathe, "mastering" } };
}

// drags every .mid file of the last render into Live at once
class BandleaderEditor::SongDrag : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit SongDrag (BandleaderProcessor& p) : proc (p)
    {
        setTooltip ("Drag into Live's arrangement: every part of the rendered song, one MIDI file each.");
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }
    void paint (juce::Graphics& g) override
    {
        const bool ready = files().size() > 0;
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (kGold.withAlpha (ready ? (hover ? 0.4f : 0.25f) : 0.06f)); g.fillRoundedRectangle (r, 5.0f);
        g.setColour (ready ? kGold : col::line); g.drawRoundedRectangle (r, 5.0f, 1.2f);
        g.setFont (font (12.0f, true));
        g.drawText (ready ? "DRAG THE SONG (" + juce::String (files().size()) + " PARTS)" : juce::String ("DRAG THE SONG"), r, juce::Justification::centred);
    }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging || e.getDistanceFromDragStart() < 6) return;
        const auto f = files();
        if (f.isEmpty()) return;
        dragging = true;
        juce::DragAndDropContainer::performExternalDragDropOfFiles (f, false, this, [this] { dragging = false; });
    }
    void mouseUp (const juce::MouseEvent&) override { dragging = false; if (proc.renderFolder.isDirectory()) proc.renderFolder.revealToUser(); }
    juce::StringArray files() const
    {
        juce::StringArray a;
        if (proc.renderFolder.isDirectory())
            for (auto& f : proc.renderFolder.findChildFiles (juce::File::findFiles, false, "*.mid")) a.add (f.getFullPathName());
        a.sort (true);
        return a;
    }
private:
    BandleaderProcessor& proc;
    bool hover = false, dragging = false;
};

juce::ComboBox* BandleaderEditor::combo (const char* id, const char* label)
{
    auto* c = new juce::ComboBox();
    if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id))) c->addItemList (ch->choices, 1);
    cAtt.add (new juce::AudioProcessorValueTreeState::ComboBoxAttachment (proc.apvts, id, *c));
    owned.add (c); addAndMakeVisible (c);
    labelled.push_back ({ c, label });
    return c;
}

BandleaderEditor::BandleaderEditor (BandleaderProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    lnf.setAccent (kGold);
    setLookAndFeel (&lnf);
    nextB.onClick  = [this] { proc.command (BandleaderProcessor::CNext); };
    fillB.onClick  = [this] { proc.command (BandleaderProcessor::CFill); };
    dropB.onClick  = [this] { proc.command (BandleaderProcessor::CDrop); };
    allInB.onClick = [this] { proc.command (BandleaderProcessor::CAllIn); };
    endB.onClick   = [this] { proc.command (BandleaderProcessor::CEnd); };
    halfB.onClick  = [this] { proc.command (BandleaderProcessor::CHalf); };
    keyUpB.onClick = [this] { proc.command (BandleaderProcessor::CKeyUp); };
    learnB.onClick = [this] { proc.command (BandleaderProcessor::CLearn); };
    learnNextB.onClick = [this] { proc.command (BandleaderProcessor::CNext); };
    learnCancelB.onClick = [this] { proc.command (BandleaderProcessor::CLearnCancel); };
    renderB.onClick = [this] { proc.startRender(); repaint(); };
    openB.onClick = [this] { if (proc.renderFolder.isDirectory()) proc.renderFolder.revealToUser(); else mc::midiFolder().revealToUser(); };
    for (auto* b : { &nextB, &fillB, &dropB, &allInB, &endB, &halfB, &keyUpB, &learnB, &learnNextB, &learnCancelB, &renderB, &openB }) addAndMakeVisible (*b);
    for (int i = 0; i < 3; ++i)
    {
        secB[i].setButtonText (kSecNames[i]);
        secB[i].onClick = [this, i] { proc.command ((BandleaderProcessor::Cmd) (BandleaderProcessor::CVerse + i)); };
        addAndMakeVisible (secB[i]);
    }
    // the style menu, grouped under pack headings (headings don't count as items, so the parameter index still lines up)
    styleBox.addItem ("Custom (no style)", 1);
    {
        juce::String pack;
        int id = 2;
        for (auto& sd : band::styles())
        {
            if (pack != sd.pack) { pack = sd.pack; styleBox.addSectionHeading (pack.toUpperCase()); }
            styleBox.addItem (sd.name, id++);
        }
    }
    styleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "style", styleBox);
    addAndMakeVisible (styleBox);
    applyAgainB.onClick = [this] { proc.reapplyStyle(); };
    addAndMakeVisible (applyAgainB);
    // a random style: the whole band re-dresses as something you didn't pick
    surpriseB.setTooltip ("Picks a style at random.");
    surpriseB.onClick = [this]
    {
        const int n = (int) band::styles().size();
        const int cur = (int) proc.apvts.getRawParameterValue ("style")->load();
        int pick = 1 + juce::Random::getSystemRandom().nextInt (n);
        if (pick == cur) pick = 1 + (pick % n);
        band::setParam (proc.apvts, "style", (float) pick);
    };
    addAndMakeVisible (surpriseB);
    // [0..3] song settings, [4..6] chords, [7..13] the form
    keyBox = combo ("key", "KEY");
    combo ("chord_bars", "BARS PER CHORD");
    combo ("song", "SONG");
    combo ("section_bars", "SECTION LENGTH");
    progBox[0] = combo ("prog_verse", "VERSE");
    progBox[1] = combo ("prog_chorus", "CHORUS");
    progBox[2] = combo ("prog_bridge", "BRIDGE");
    combo ("form", "ARRANGEMENT");
    combo ("meter", "TIME SIGNATURE");
    combo ("count_in", "COUNT-IN");
    combo ("intro", "INTRO");
    combo ("breakdown", "BREAKDOWN");
    combo ("key_up", "KEY CHANGE");
    combo ("ending", "ENDING");
    energyK = new ttui::Knob ("ENERGY", false, this);
    energyK->attach (proc.apvts, "energy");
    owned.add (energyK); addAndMakeVisible (energyK);
    dragMidi = std::make_unique<mc::DragButton> (proc.capture, "Bandleader Chords", juce::Colour (0xffd9b45a));
    addAndMakeVisible (*dragMidi);
    dragSong = std::make_unique<SongDrag> (proc);
    addAndMakeVisible (*dragSong);
    setSize (1200, 900);
    startTimerHz (12);
}

BandleaderEditor::~BandleaderEditor() { setLookAndFeel (nullptr); }

void BandleaderEditor::timerCallback()
{
    updateButtons();
    juce::String st;
    for (auto* id : { "style", "key", "prog_verse", "prog_chorus", "prog_bridge", "chord_bars", "song", "section_bars", "form", "meter", "count_in", "intro", "breakdown", "key_up", "ending" })
        st << (int) proc.apvts.getRawParameterValue (id)->load() << ",";
    st << proc.apvts.state.getProperty ("learned", "").toString();
    if (st != lastStatic) { lastStatic = st; repaint(); return; }
    const int64_t dyn = proc.uiSection.load() * 7 + proc.uiNextSection.load() * 131 + proc.uiBarsToNext.load() * 1031 + proc.uiBarIn.load() * 10007
                      + proc.uiFormIndex.load() * 100003 + proc.uiChord.load() * 1000003LL + (proc.uiPlaying.load() ? 1 : 0)
                      + (proc.uiOut.load() ? 2 : 0) + (proc.uiDropComing.load() ? 4 : 0) + (proc.uiFillComing.load() ? 8 : 0) * 77
                      + proc.uiPart.load() * 31 + proc.uiKeyShift.load() * 997 + proc.uiMeter.load() * 3 + proc.uiHostMeter.load() * 5
                      + proc.uiLearnState.load() * 13 + proc.uiLearnSec.load() * 17 + proc.uiLearnBars.load() * 19 + proc.uiLearnHeard.load() * 23
                      + (proc.uiEndComing.load() ? 29 : 0) + (proc.uiOver.load() ? 37 : 0);
    if (dyn != lastDyn) { lastDyn = dyn; repaint (nowArea); repaint (formArea); repaint (learnArea); }
    if (++tick % 6 == 0) { repaint (bandArea); repaint (renderArea); dragSong->repaint(); }
}

void BandleaderEditor::updateButtons()
{
    const bool playing = proc.uiPlaying.load();
    const int sec = proc.uiSection.load(), next = proc.uiNextSection.load();
    for (int i = 0; i < 3; ++i) secB[i].setToggleState (playing && (i == sec || i == next), juce::dontSendNotification);
    dropB.setToggleState (proc.uiOut.load() || proc.uiDropComing.load(), juce::dontSendNotification);
    fillB.setToggleState (proc.uiFillComing.load(), juce::dontSendNotification);
    endB.setToggleState (proc.uiEndComing.load(), juce::dontSendNotification);
    halfB.setToggleState (proc.uiHalfOn.load(), juce::dontSendNotification);
    const bool learning = proc.uiLearnState.load() != 0;
    learnB.setButtonText (learning ? "DONE" : "LEARN THE SONG");
    learnB.setToggleState (learning, juce::dontSendNotification);
    learnNextB.setEnabled (learning); learnCancelB.setEnabled (learning);
}

void BandleaderEditor::paintForm (juce::Graphics& g, juce::Rectangle<int> r)
{
    // the song as a strip of parts; the one playing lights up
    const auto song = proc.currentSong();
    if (song.segs.empty() || song.length <= 0) return;
    const bool autoSong = proc.apvts.getRawParameterValue ("song")->load() < 0.5f;
    const int cur = autoSong && proc.uiPlaying.load() ? proc.uiFormIndex.load() : -1;
    const float w = (float) r.getWidth() / (float) song.length;
    for (int i = 0; i < (int) song.segs.size(); ++i)
    {
        const auto& sg = song.segs[(size_t) i];
        auto b = juce::Rectangle<float> ((float) r.getX() + w * (float) sg.start, (float) r.getY(), w * (float) sg.bars, (float) r.getHeight()).reduced (1.5f, 0);
        juce::Colour c = sg.sec == band::Chorus ? kGold.darker (0.35f) : sg.sec == band::Bridge ? juce::Colour (0xff6a7fb5) : col::panel2.brighter (0.25f);
        juce::String label = kSecShort[sg.sec];
        switch (sg.part)
        {
            case band::PCount: c = col::line; label = "COUNT"; break;
            case band::PIntro: c = col::panel2.brighter (0.1f); label = "INTRO"; break;
            case band::PHalf:  c = c.darker (0.4f); label = juce::String ("1/2 ") + kSecShort[sg.sec]; break;
            case band::POutro: c = c.withAlpha (0.55f); label = "FADE"; break;
            case band::PEnd:   c = col::red.darker (0.2f); label = "END"; break;
            default: break;
        }
        if (sg.key != 0) label << " +" << sg.key;
        g.setColour (i == cur ? col::cream : c); g.fillRoundedRectangle (b, 4.0f);
        g.setColour (i == cur ? col::bg : col::cream.withAlpha (0.85f)); g.setFont (font (b.getWidth() > 40 ? 11.0f : 9.0f, true));
        g.drawFittedText (label, b.toNearestInt(), juce::Justification::centred, 1, 0.6f);
    }
    if (song.loops) { g.setColour (col::muted); g.setFont (font (11.0f)); g.drawText ("then round again", r.translated (0, r.getHeight() + 2).withHeight (14), juce::Justification::centredRight); }
}

void BandleaderEditor::paintLearn (juce::Graphics& g, juce::Rectangle<int> r)
{
    g.setColour (kGold); g.setFont (font (13.0f, true));
    g.drawText ("LEARN THE SONG", r.removeFromTop (18), juce::Justification::centredLeft);
    r.removeFromTop (4);
    const int ls = proc.uiLearnState.load();
    const int key = juce::jmax (0, keyBox->getSelectedItemIndex());
    auto text = r.withTrimmedLeft (420);
    if (ls != 0)
    {
        const int sec = juce::jlimit (0, 2, proc.uiLearnSec.load());
        const int heard = proc.uiLearnHeard.load();
        g.setColour (col::red); g.setFont (font (15.0f, true));
        g.drawText (ls == 1 ? juce::String ("READY: play the ") + kSecNames[sec] + " (it starts on your first chord)"
                            : juce::String ("LEARNING THE ") + kSecNames[sec] + " - bar " + juce::String (proc.uiLearnBars.load() + 1),
                    text.removeFromTop (20), juce::Justification::centredLeft);
        g.setColour (col::cream); g.setFont (font (13.0f));
        g.drawText (heard >= 0 ? "hearing " + juce::String (ut::pcName (heard & 15)) + band::qualitySuffix (heard >> 4) : juce::String ("listening..."),
                    text.removeFromTop (18), juce::Justification::centredLeft);
        g.setColour (col::muted); g.setFont (font (12.0f));
        g.drawFittedText ("Stomp NEXT SECTION (or NEXT PART) at the end of each part. DONE when you're finished.", text, juce::Justification::topLeft, 2);
        return;
    }
    g.setFont (font (12.0f));
    for (int s = 0; s < 3; ++s)
    {
        auto row = text.removeFromTop (17);
        g.setColour (col::muted); g.drawText (kSecNames[s], row.removeFromLeft (62), juce::Justification::centredLeft);
        g.setColour (col::cream); g.drawText (proc.learnedChords (s, key), row, juce::Justification::centredLeft);
    }
    g.setColour (col::muted);
    g.drawFittedText ("Guitar into the sidechain, or a keyboard on this track. Press LEARN, play the verse, NEXT, the chorus, NEXT, the bridge.",
                      text, juce::Justification::topLeft, 2);
}

void BandleaderEditor::paintRender (juce::Graphics& g, juce::Rectangle<int> r)
{
    g.setColour (kGold); g.setFont (font (13.0f, true));
    g.drawText ("RENDER THE SONG TO CLIPS", r.removeFromTop (18), juce::Justification::centredLeft);
    r.removeFromTop (44);
    g.setFont (font (12.0f));
    if (proc.renderStamp == 0)
    {
        g.setColour (col::muted);
        g.drawFittedText ("Every player writes its whole part of the song (the form above, at Live's tempo) as a MIDI file. "
                          "Then drag them all into Live's arrangement.", r, juce::Justification::topLeft, 4);
        return;
    }
    int busy = 0, done = 0, failed = 0;
    juce::String line;
    for (auto& m : kMembers)
    {
        const int st = band::renderState (m.who);
        if (st == band::RBusy) ++busy; else if (st == band::RDone) ++done; else if (st == band::RFailed) ++failed;
        if (st == band::RDone) line << m.name << "  ";
    }
    const int chords = band::renderState (band::WBandleader);
    if (chords == band::RDone) ++done; else if (chords == band::RBusy) ++busy;
    g.setColour (busy > 0 ? kGold : kGreen); g.setFont (font (13.0f, true));
    g.drawText (busy > 0 ? "Rendering... " + juce::String (done) + " done" : juce::String ("Done: ") + juce::String (done) + " parts" + (failed ? " (" + juce::String (failed) + " failed)" : ""),
                r.removeFromTop (18), juce::Justification::centredLeft);
    g.setColour (col::muted); g.setFont (font (11.0f));
    g.drawFittedText (line.trim(), r.removeFromTop (30), juce::Justification::topLeft, 2);
    g.drawFittedText (proc.renderFolder.getFileName(), r.removeFromTop (16), juce::Justification::topLeft, 1);
}

void BandleaderEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    auto header = getLocalBounds().removeFromTop (60);
    g.setColour (col::panel); g.fillRect (header);
    g.setColour (kGold); g.fillRect (header.removeFromBottom (2));
    g.setColour (col::cream); g.setFont (font (28.0f, true));
    g.drawText ("BANDLEADER", 20, 8, 400, 30, juce::Justification::centredLeft);
    g.setColour (kGold); g.setFont (font (11.0f, true));
    g.drawText ("RUNS THE DEMOCRAZY BAND", 22, 38, 400, 14, juce::Justification::centredLeft);

    const bool playing = proc.uiPlaying.load();
    const int sec = proc.uiSection.load(), next = proc.uiNextSection.load(), toNext = proc.uiBarsToNext.load();
    const int part = proc.uiPart.load();

    // ---- now
    g.setColour (col::panel); g.fillRoundedRectangle (nowArea.toFloat(), 8.0f);
    auto na = nowArea.reduced (18, 12);
    na.removeFromRight (410);
    g.setColour (kGold); g.setFont (font (13.0f, true));
    {
        auto t = na.removeFromTop (18);
        g.drawText ("NOW", t, juce::Justification::centredLeft);
        const int m = proc.uiMeter.load(), hm = proc.uiHostMeter.load();
        g.setColour (col::cream);
        g.drawText (band::meterName (m), t, juce::Justification::centredRight);
        if (m != hm && proc.apvts.getRawParameterValue ("meter")->load() > 0.5f)
        { g.setColour (col::red); g.setFont (font (11.0f, true)); g.drawText (juce::String ("Live is in ") + band::meterName (hm) + ": set it to " + band::meterName (m), t.withTrimmedRight (40), juce::Justification::centredRight); }
    }
    juce::String big = "STOPPED";
    if (playing)
    {
        big = kSecNames[juce::jlimit (0, 2, sec)];
        if (part == band::PCount) big = "COUNT-IN";
        else if (part == band::PIntro) big = "INTRO";
        else if (part == band::PHalf) big = juce::String ("HALF-TIME ") + kSecNames[juce::jlimit (0, 2, sec)];
        else if (part == band::POutro) big = "FADE-OUT";
        else if (part == band::PEnd) big = "THE END";
        if (proc.uiOver.load()) big = "SONG'S OVER";
        if (proc.uiLearnState.load() != 0) big = "LEARNING";
    }
    g.setColour (playing ? col::cream : col::muted); g.setFont (font (40.0f, true));
    g.drawFittedText (big, na.removeFromTop (48), juce::Justification::centredLeft, 1);
    g.setFont (font (15.0f));
    const int ch = proc.uiChord.load();
    const juce::String chord = ch >= 0 ? juce::String (ut::pcName (ch & 15)) + band::qualitySuffix (ch >> 4) : "";
    juce::String line = playing ? "bar " + juce::String (proc.uiBarIn.load()) + "   -   chord " + chord : "Press play in Live and the band starts.";
    if (playing && proc.uiKeyShift.load() != 0) line << "   (key up " << proc.uiKeyShift.load() << ")";
    g.drawText (line, na.removeFromTop (22), juce::Justification::centredLeft);
    g.setFont (font (15.0f, true));
    juce::String coming;
    if (proc.uiOut.load()) { g.setColour (col::red); coming = "BAND DROPPED OUT - press ALL IN"; }
    else if (proc.uiDropComing.load()) { g.setColour (col::red); coming = "dropping out next bar"; }
    else if (proc.uiEndComing.load()) { g.setColour (col::red); coming = "ending the song..."; }
    else if (playing && next >= 0) { g.setColour (kGold); coming = "-> " + juce::String (kSecNames[next]) + (toNext <= 1 ? " next bar" : " in " + juce::String (toNext) + " bars") + "  (fill)"; }
    else if (proc.uiFillComing.load()) { g.setColour (kGold); coming = "fill coming"; }
    g.drawText (coming, na.removeFromTop (22), juce::Justification::centredLeft);

    // ---- the song form
    g.setColour (col::panel); g.fillRoundedRectangle (formArea.toFloat(), 8.0f);
    {
        auto fa = formArea.reduced (18, 12);
        const bool autoSong = proc.apvts.getRawParameterValue ("song")->load() < 0.5f;
        g.setColour (kGold); g.setFont (font (13.0f, true));
        g.drawText (autoSong ? "THE SONG FORM (auto)" : "THE SONG FORM (manual - you call the sections)", fa.removeFromTop (18), juce::Justification::centredLeft);
        if (autoSong) paintForm (g, timelineArea);
        else { g.setColour (col::muted); g.setFont (font (12.0f)); g.drawFittedText ("Pick VERSE / CHORUS / BRIDGE, or stomp NEXT SECTION. Count-in and intro still play when you press play.", timelineArea, juce::Justification::centredLeft, 2); }
    }

    // ---- learn
    g.setColour (col::panel); g.fillRoundedRectangle (learnArea.toFloat(), 8.0f);
    paintLearn (g, learnArea.reduced (18, 12));

    // ---- style, key and chords
    g.setColour (col::panel); g.fillRoundedRectangle (songArea.toFloat(), 8.0f);
    g.setColour (kGold); g.setFont (font (13.0f, true));
    g.drawText ("STYLE, KEY AND CHORDS", songArea.reduced (18, 12).removeFromTop (18), juce::Justification::centredLeft);
    {
        const int st = (int) proc.apvts.getRawParameterValue ("style")->load();
        auto ta = styleText;
        if (st > 0)
        {
            const auto& sd = band::styles()[(size_t) (st - 1)];
            g.setColour (col::cream); g.setFont (font (12.0f, true));
            g.drawText ("Suggested tempo: " + juce::String (juce::roundToInt (sd.bpm)) + " BPM" + (sd.meter != 0 ? juce::String ("  -  ") + band::meterName (sd.meter) + " time" : juce::String()) + " (set it in Live)",
                        ta.removeFromTop (16), juce::Justification::centredLeft);
            g.setColour (col::muted); g.setFont (font (12.0f));
            g.drawFittedText (sd.blurb, ta, juce::Justification::topLeft, 2, 1.0f);
        }
        else
        {
            g.setColour (col::muted); g.setFont (font (12.0f));
            g.drawFittedText ("Pick a style and the whole band changes: every plugin moves its knobs to suit it. Tweak anything after.", ta, juce::Justification::topLeft, 2, 1.0f);
        }
    }
    const int key = keyBox->getSelectedItemIndex();
    g.setFont (font (11.0f, true));
    for (auto& [cb, label] : labelled)
    {
        juce::String t = label;
        for (int i = 0; i < 3; ++i)
            if (cb == progBox[i])
                t << ": " << (cb->getSelectedItemIndex() >= proc.learnedIndex() ? proc.learnedChords (i, juce::jmax (0, key)) : ut::progressionChords (cb->getSelectedItemIndex(), juce::jmax (0, key)));
        g.setColour (col::muted);
        g.drawFittedText (t, cb->getBounds().translated (0, -15).withHeight (14), juce::Justification::centredLeft, 1, 0.8f);
    }

    // ---- the band
    g.setColour (col::panel); g.fillRoundedRectangle (bandArea.toFloat(), 8.0f);
    auto ba = bandArea.reduced (18, 12);
    g.setColour (kGold); g.setFont (font (13.0f, true));
    {
        auto t = ba.removeFromTop (18);
        g.drawText ("THE BAND", t, juce::Justification::centredLeft);
        g.setColour (col::muted); g.setFont (font (11.0f));
        g.drawText ("click a light to switch it off / on", t, juce::Justification::centredRight);
    }
    ba.removeFromTop (4);
    for (auto& m : kMembers)
    {
        auto r = ba.removeFromTop (18);
        const bool here = band::isPresent (m.who);
        const bool off = ((proc.benched.load() >> m.who) & 1u) != 0;
        auto dot = r.removeFromLeft (14).withSizeKeepingCentre (10, 10).toFloat();
        if (off) { g.setColour (here ? col::red : col::line); g.drawEllipse (dot.reduced (0.5f), 1.6f); }     // switched off: a hollow ring
        else { g.setColour (here ? kGreen : col::line); g.fillEllipse (dot); }
        r.removeFromLeft (6);
        g.setColour (here && ! off ? col::cream : col::muted); g.setFont (font (13.0f, true));
        g.drawText (m.name, r.removeFromLeft (84), juce::Justification::centredLeft);
        g.setColour (col::muted); g.setFont (font (11.0f));
        g.drawText (m.does, r, juce::Justification::centredLeft);
    }
    if (! proc.uiBusOk.load()) { g.setColour (col::red); g.drawText ("Couldn't open the band link on this computer.", ba.removeFromTop (20), juce::Justification::centredLeft); }
    ba.removeFromTop (10);
    g.setColour (kGold); g.setFont (font (13.0f, true));
    g.drawText ("FOOTSWITCH (MIDI Captain)", ba.removeFromTop (18), juce::Justification::centredLeft);
    ba.removeFromTop (2);
    const char* fs[][2] = { { "C2 / CC 90", "Next section" }, { "D2 / CC 91", "Fill" }, { "E2 / CC 92", "Drop out" }, { "F2 / CC 93", "All in" },
                            { "G2 / CC 94", "Verse" }, { "A2 / CC 95", "Chorus" }, { "B2 / CC 96", "Bridge" }, { "C3 / CC 97", "End the song" },
                            { "D3 / CC 98", "Half-time on / off" }, { "E3 / CC 99", "Key up" }, { "F3 / CC 100", "Learn / done" } };
    for (auto& f : fs)
    {
        auto r = ba.removeFromTop (17);
        g.setColour (col::muted); g.setFont (font (11.5f)); g.drawText (f[0], r.removeFromLeft (96), juce::Justification::centredLeft);
        g.setColour (col::cream); g.drawText (f[1], r, juce::Justification::centredLeft);
    }

    // ---- render
    g.setColour (col::panel); g.fillRoundedRectangle (renderArea.toFloat(), 8.0f);
    paintRender (g, renderArea.reduced (18, 12));

    g.setColour (col::muted); g.setFont (font (12.0f));
    g.drawFittedText ("Put Bandleader on its own MIDI track (it makes no sound) and press play. The rest of the suite follows it (green dot = listening). "
                      "Send the footswitch to this track, or MIDI Map the \"Footswitch:\" parameters.",
                      legendArea, juce::Justification::topLeft, 2);
}

void BandleaderEditor::resized()
{
    auto r = getLocalBounds();
    dragMidi->setBounds (getWidth() - 196, 14, 178, 32);
    r.removeFromTop (60);
    r.reduce (18, 12);
    legendArea = r.removeFromBottom (32);
    r.removeFromBottom (6);
    auto right = r.removeFromRight (360);
    r.removeFromRight (14);
    renderArea = right.removeFromBottom (170);
    right.removeFromBottom (12);
    bandArea = right;
    {
        auto ra = renderArea.reduced (18, 12); ra.removeFromTop (22);
        auto row = ra.removeFromTop (32);
        renderB.setBounds (row.removeFromLeft (150).reduced (2));
        dragSong->setBounds (row.reduced (2));
    }

    nowArea = r.removeFromTop (150);
    {
        auto btns = nowArea.reduced (18, 14).removeFromRight (400);
        const int h = btns.getHeight() / 3;
        auto a = btns.removeFromTop (h), b = btns.removeFromTop (h), c = btns;
        const int w4 = a.getWidth() / 4;
        nextB.setBounds (a.removeFromLeft (w4 * 2).reduced (3)); fillB.setBounds (a.removeFromLeft (w4).reduced (3)); dropB.setBounds (a.reduced (3));
        allInB.setBounds (b.removeFromLeft (w4).reduced (3)); halfB.setBounds (b.removeFromLeft (w4).reduced (3)); keyUpB.setBounds (b.removeFromLeft (w4).reduced (3)); endB.setBounds (b.reduced (3));
        for (int i = 0; i < 3; ++i) secB[i].setBounds (c.removeFromLeft (c.getWidth() / (3 - i)).reduced (3));
    }
    r.removeFromTop (12);
    formArea = r.removeFromTop (196);
    {
        auto fa = formArea.reduced (18, 12); fa.removeFromTop (38);
        auto row1 = fa.removeFromTop (26);
        // labelled: [7] form, [8] meter, [9] count-in, [10] intro, [11] breakdown, [12] key change, [13] ending, [2] song
        labelled[7].first->setBounds (row1.removeFromLeft (row1.getWidth() * 2 / 5).reduced (4, 0));
        const int w3 = row1.getWidth() / 3;
        labelled[8].first->setBounds (row1.removeFromLeft (w3).reduced (4, 0));
        labelled[9].first->setBounds (row1.removeFromLeft (w3).reduced (4, 0));
        labelled[10].first->setBounds (row1.reduced (4, 0));
        fa.removeFromTop (22);
        auto row2 = fa.removeFromTop (26);
        const int w4 = row2.getWidth() / 4;
        labelled[11].first->setBounds (row2.removeFromLeft (w4).reduced (4, 0));
        labelled[12].first->setBounds (row2.removeFromLeft (w4).reduced (4, 0));
        labelled[13].first->setBounds (row2.removeFromLeft (w4).reduced (4, 0));
        labelled[2].first->setBounds (row2.reduced (4, 0));
        fa.removeFromTop (12);
        timelineArea = fa.removeFromTop (30);
    }
    r.removeFromTop (12);
    learnArea = r.removeFromTop (112);
    {
        auto la = learnArea.reduced (18, 12); la.removeFromTop (24);
        auto col1 = la.removeFromLeft (400);
        auto row = col1.removeFromTop (34);
        learnB.setBounds (row.removeFromLeft (170).reduced (3));
        learnNextB.setBounds (row.removeFromLeft (120).reduced (3));
        learnCancelB.setBounds (row.removeFromLeft (100).reduced (3));
    }
    r.removeFromTop (12);
    songArea = r;
    auto sa = songArea.reduced (18, 12);
    sa.removeFromTop (24);
    {
        auto sr = sa.removeFromTop (28);
        styleBox.setBounds (sr.removeFromLeft (280).reduced (4, 0));
        sr.removeFromLeft (8);
        applyAgainB.setBounds (sr.removeFromLeft (130).reduced (4, 0));
        surpriseB.setBounds (sr.removeFromLeft (130).reduced (4, 0));
        sa.removeFromTop (6);
        styleText = sa.removeFromTop (46).reduced (4, 0);
    }
    sa.removeFromTop (16);
    auto row1 = sa.removeFromTop (26);
    energyK->setBounds (sa.withTrimmedTop (4).removeFromRight (100).withHeight (juce::jmin (96, sa.getHeight())));
    row1.removeFromRight (110);
    const int w3 = row1.getWidth() / 3;
    labelled[0].first->setBounds (row1.removeFromLeft (w3).reduced (4, 0));
    labelled[1].first->setBounds (row1.removeFromLeft (w3).reduced (4, 0));
    labelled[3].first->setBounds (row1.reduced (4, 0));
    sa.removeFromTop (22);
    auto row2 = sa.removeFromTop (26).withTrimmedRight (110);
    const int wp = row2.getWidth() / 3;
    for (int i = 0; i < 3; ++i) progBox[i]->setBounds (row2.removeFromLeft (wp).reduced (4, 0));
}

// the rows of THE BAND: click one to switch that plugin off (it sits out, or an effect passes straight through) or back on
int BandleaderEditor::memberAt (juce::Point<int> p) const
{
    auto ba = bandArea.reduced (18, 12);
    ba.removeFromTop (22);
    const int n = (int) (sizeof (kMembers) / sizeof (kMembers[0]));
    if (! ba.withHeight (18 * n).contains (p)) return -1;
    return juce::jlimit (0, n - 1, (p.y - ba.getY()) / 18);
}
void BandleaderEditor::mouseDown (const juce::MouseEvent& e)
{
    const int i = memberAt (e.getPosition());
    if (i < 0) return;
    proc.toggleBench ((int) kMembers[i].who);
    repaint (bandArea);
}
void BandleaderEditor::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (memberAt (e.getPosition()) >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}
