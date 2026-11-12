#include "BandleaderEditor.h"

using ttui::font;
namespace col = ttui::col;

namespace
{
const juce::Colour kGold { 0xffd8b04a };
const juce::Colour kGreen { 0xff7fbf6a };
const char* kSecNames[3] = { "VERSE", "CHORUS", "BRIDGE" };
}

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
    for (auto* b : { &nextB, &fillB, &dropB, &allInB }) addAndMakeVisible (*b);
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
    keyBox = combo ("key", "KEY");
    combo ("chord_bars", "BARS PER CHORD");
    combo ("song", "SONG");
    combo ("section_bars", "SECTION LENGTH");
    progBox[0] = combo ("prog_verse", "VERSE");
    progBox[1] = combo ("prog_chorus", "CHORUS");
    progBox[2] = combo ("prog_bridge", "BRIDGE");
    energyK = new ttui::Knob ("ENERGY", false, this);
    energyK->attach (proc.apvts, "energy");
    owned.add (energyK); addAndMakeVisible (energyK);
    dragMidi = std::make_unique<mc::DragButton> (proc.capture, "Bandleader Chords", juce::Colour (0xffd9b45a));
    addAndMakeVisible (*dragMidi);
    setSize (980, 740);
    startTimerHz (12);
}

BandleaderEditor::~BandleaderEditor() { setLookAndFeel (nullptr); }

void BandleaderEditor::timerCallback()
{
    updateButtons();
    juce::String st;
    for (auto* id : { "style", "key", "prog_verse", "prog_chorus", "prog_bridge", "chord_bars", "song", "section_bars" })
        st << (int) proc.apvts.getRawParameterValue (id)->load() << ",";
    if (st != lastStatic) { lastStatic = st; repaint(); return; }
    const int64_t dyn = proc.uiSection.load() * 7 + proc.uiNextSection.load() * 131 + proc.uiBarsToNext.load() * 1031 + proc.uiBarIn.load() * 10007
                      + proc.uiFormIndex.load() * 100003 + proc.uiChord.load() * 1000003LL + (proc.uiPlaying.load() ? 1 : 0)
                      + (proc.uiOut.load() ? 2 : 0) + (proc.uiDropComing.load() ? 4 : 0) + (proc.uiFillComing.load() ? 8 : 0) * 77;
    if (dyn != lastDyn) { lastDyn = dyn; repaint (nowArea); repaint (formArea); }
    if (++tick % 12 == 0) repaint (bandArea);    // who's listening: once a second is plenty
}

void BandleaderEditor::updateButtons()
{
    const bool playing = proc.uiPlaying.load();
    const int sec = proc.uiSection.load(), next = proc.uiNextSection.load();
    for (int i = 0; i < 3; ++i) secB[i].setToggleState (playing && (i == sec || i == next), juce::dontSendNotification);
    dropB.setToggleState (proc.uiOut.load() || proc.uiDropComing.load(), juce::dontSendNotification);
    fillB.setToggleState (proc.uiFillComing.load(), juce::dontSendNotification);
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

    // ---- now
    g.setColour (col::panel); g.fillRoundedRectangle (nowArea.toFloat(), 8.0f);
    auto na = nowArea.reduced (18, 12);
    g.setColour (kGold); g.setFont (font (13.0f, true));
    g.drawText ("NOW", na.removeFromTop (18), juce::Justification::centredLeft);
    g.setColour (playing ? col::cream : col::muted); g.setFont (font (44.0f, true));
    g.drawText (playing ? juce::String (kSecNames[juce::jlimit (0, 2, sec)]) : juce::String ("STOPPED"), na.removeFromTop (50), juce::Justification::centredLeft);
    g.setFont (font (15.0f));
    const int ch = proc.uiChord.load();
    const juce::String chord = ch >= 0 ? juce::String (ut::pcName (ch & 15)) + band::qualitySuffix (ch >> 4) : "";
    juce::String line = playing ? "bar " + juce::String (proc.uiBarIn.load()) + "   -   chord " + chord : "Press play in Live and the band starts.";
    g.drawText (line, na.removeFromTop (22), juce::Justification::centredLeft);
    g.setFont (font (15.0f, true));
    juce::String coming;
    if (proc.uiOut.load()) { g.setColour (col::red); coming = "BAND DROPPED OUT - press ALL IN"; }
    else if (proc.uiDropComing.load()) { g.setColour (col::red); coming = "dropping out next bar"; }
    else if (playing && next >= 0) { g.setColour (kGold); coming = "-> " + juce::String (kSecNames[next]) + (toNext <= 1 ? " next bar" : " in " + juce::String (toNext) + " bars") + "  (fill)"; }
    else if (proc.uiFillComing.load()) { g.setColour (kGold); coming = "fill coming"; }
    g.drawText (coming, na.removeFromTop (22), juce::Justification::centredLeft);

    // ---- song form
    g.setColour (col::panel); g.fillRoundedRectangle (formArea.toFloat(), 8.0f);
    auto fa = formArea.reduced (18, 12);
    const bool autoSong = proc.apvts.getRawParameterValue ("song")->load() < 0.5f;
    g.setColour (kGold); g.setFont (font (13.0f, true));
    g.drawText (autoSong ? "THE SONG (auto - loops)" : "THE SONG (manual - you call the sections)", fa.removeFromTop (18), juce::Justification::centredLeft);
    fa.removeFromTop (8);
    if (autoSong)
    {
        const int w = fa.getWidth() / 6;
        for (int i = 0; i < 6; ++i)
        {
            auto b = fa.withX (fa.getX() + i * w).withWidth (w).reduced (3, 0).withHeight (36);
            const bool on = playing && i == proc.uiFormIndex.load();
            g.setColour (on ? kGold : col::panel2); g.fillRoundedRectangle (b.toFloat(), 5.0f);
            g.setColour (on ? col::bg : col::cream); g.setFont (font (13.0f, true));
            g.drawText (kSecNames[bl::kForm[i]], b, juce::Justification::centred);
        }
    }
    else
    {
        g.setColour (col::muted); g.setFont (font (13.0f));
        g.drawFittedText ("Pick VERSE / CHORUS / BRIDGE, or stomp NEXT SECTION. Changes land on the next bar with a fill.", fa.removeFromTop (46), juce::Justification::centredLeft, 2);
    }

    // ---- song settings labels
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
            g.drawText ("Suggested tempo: " + juce::String (juce::roundToInt (sd.bpm)) + " BPM (set it in Live)", ta.removeFromTop (16), juce::Justification::centredLeft);
            g.setColour (col::muted); g.setFont (font (12.0f));
            g.drawFittedText (sd.blurb, ta, juce::Justification::topLeft, 2, 1.0f);
        }
        else
        {
            g.setColour (col::muted); g.setFont (font (12.0f));
            g.drawFittedText ("Pick a style and the whole band changes: drums, bass, Loon, Tape Twin, Schaltwerk and Lathe move their knobs to suit it. Tweak anything after.",
                              ta, juce::Justification::topLeft, 2, 1.0f);
        }
    }
    const int key = keyBox->getSelectedItemIndex();
    g.setFont (font (11.0f, true));
    for (auto& [cb, label] : labelled)
    {
        juce::String t = label;
        for (int i = 0; i < 3; ++i) if (cb == progBox[i]) t << ": " << ut::progressionChords (cb->getSelectedItemIndex(), juce::jmax (0, key));
        g.setColour (col::muted);
        g.drawText (t, cb->getBounds().translated (0, -15).withHeight (14), juce::Justification::centredLeft);
    }

    // ---- the band
    g.setColour (col::panel); g.fillRoundedRectangle (bandArea.toFloat(), 8.0f);
    auto ba = bandArea.reduced (18, 12);
    g.setColour (kGold); g.setFont (font (13.0f, true));
    g.drawText ("THE BAND", ba.removeFromTop (18), juce::Justification::centredLeft);
    ba.removeFromTop (6);
    struct M { const char* name; band::Who who; const char* does; };
    const M members[] = { { "Tea Towel", band::WTeaTowel, "drums: sections, fills, drop-outs" },
                          { "Undertow", band::WUndertow, "bass: chords, locks to the kick" },
                          { "Loon", band::WLoon, "key + chords, swells, ducks the kick" },
                          { "Schaltwerk", band::WSchaltwerk, "Strumchord plays the chords" },
                          { "Carousel", band::WCarousel, "records whole sections" },
                          { "Tape Twin", band::WTapeTwin, "echo throws into new sections" },
                          { "Lathe", band::WLathe, "loudness per section" },
                          { "Parlour", band::WParlour, "keys: comps the chords" },
                          { "Ether", band::WEther, "early electronic melody" },
                          { "Chamber", band::WChamber, "strings, brass, woodwinds" } };
    for (auto& m : members)
    {
        auto r = ba.removeFromTop (18);
        const bool here = band::isPresent (m.who);
        g.setColour (here ? kGreen : col::line); g.fillEllipse (r.removeFromLeft (14).withSizeKeepingCentre (10, 10).toFloat());
        r.removeFromLeft (6);
        g.setColour (here ? col::cream : col::muted); g.setFont (font (13.0f, true));
        g.drawText (m.name, r.removeFromLeft (84), juce::Justification::centredLeft);
        g.setColour (col::muted); g.setFont (font (11.0f));
        g.drawText (m.does, r, juce::Justification::centredLeft);
    }
    if (! proc.uiBusOk.load()) { g.setColour (col::red); g.drawText ("Couldn't open the band link on this computer.", ba.removeFromTop (20), juce::Justification::centredLeft); }

    ba.removeFromTop (18);
    g.setColour (kGold); g.setFont (font (13.0f, true));
    g.drawText ("FOOTSWITCH (MIDI Captain)", ba.removeFromTop (18), juce::Justification::centredLeft);
    ba.removeFromTop (4);
    const char* fs[][2] = { { "C2  /  CC 90", "Next section" }, { "D2  /  CC 91", "Fill" }, { "E2  /  CC 92", "Drop out" }, { "F2  /  CC 93", "All in" },
                            { "G2  /  CC 94", "Verse" }, { "A2  /  CC 95", "Chorus" }, { "B2  /  CC 96", "Bridge" } };
    for (auto& f : fs)
    {
        auto r = ba.removeFromTop (20);
        g.setColour (col::muted); g.setFont (font (12.0f)); g.drawText (f[0], r.removeFromLeft (110), juce::Justification::centredLeft);
        g.setColour (col::cream); g.drawText (f[1], r, juce::Justification::centredLeft);
    }

    g.setColour (col::muted); g.setFont (font (12.0f));
    g.drawFittedText ("Put Bandleader on its own MIDI track (it makes no sound) and press play. The rest of the suite follows it automatically (green dot = listening). "
                      "Send the footswitch to this track, or MIDI Map the \"Footswitch:\" parameters.",
                      legendArea, juce::Justification::topLeft, 3);
}

void BandleaderEditor::resized()
{
    auto r = getLocalBounds();
    dragMidi->setBounds (getWidth() - 196, 14, 178, 32);
    r.removeFromTop (60);
    r.reduce (18, 14);
    legendArea = r.removeFromBottom (36);
    r.removeFromBottom (8);
    auto right = r.removeFromRight (300);
    r.removeFromRight (14);
    bandArea = right;

    nowArea = r.removeFromTop (150);
    {
        auto btns = nowArea.reduced (18, 14).removeFromRight (300);
        const int h = btns.getHeight() / 2;
        auto top = btns.removeFromTop (h), bot = btns;
        nextB.setBounds (top.removeFromLeft (top.getWidth() / 2).reduced (4));
        fillB.setBounds (top.reduced (4));
        dropB.setBounds (bot.removeFromLeft (bot.getWidth() / 2).reduced (4));
        allInB.setBounds (bot.reduced (4));
    }
    r.removeFromTop (12);
    formArea = r.removeFromTop (140);
    {
        auto sb = formArea.reduced (18, 12).removeFromBottom (34);
        const int w = sb.getWidth() / 3;
        for (int i = 0; i < 3; ++i) secB[i].setBounds (sb.removeFromLeft (w).reduced (4, 0));
    }
    r.removeFromTop (12);
    songArea = r;
    auto sa = songArea.reduced (18, 12);
    sa.removeFromTop (24);
    {
        auto sr = sa.removeFromTop (28);
        styleBox.setBounds (sr.removeFromLeft (260).reduced (4, 0));
        sr.removeFromLeft (8);
        applyAgainB.setBounds (sr.removeFromLeft (130).reduced (4, 0));
        sa.removeFromTop (6);
        styleText = sa.removeFromTop (48).reduced (4, 0);
    }
    sa.removeFromTop (16);
    auto row1 = sa.removeFromTop (26);
    energyK->setBounds (sa.withTrimmedTop (8).removeFromRight (110).withHeight (100));
    const int w4 = row1.getWidth() / 4;
    for (int i = 0; i < 4; ++i) labelled[(size_t) i].first->setBounds (row1.removeFromLeft (w4).reduced (4, 0));
    sa.removeFromTop (24);
    auto row2 = sa.removeFromTop (26).withTrimmedRight (120);
    const int w3 = row2.getWidth() / 3;
    for (int i = 0; i < 3; ++i) progBox[i]->setBounds (row2.removeFromLeft (w3).reduced (4, 0));
}
