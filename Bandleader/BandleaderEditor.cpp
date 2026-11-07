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
    setSize (980, 650);
    startTimerHz (20);
}

BandleaderEditor::~BandleaderEditor() { setLookAndFeel (nullptr); }

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
    g.drawText ("RUNS THE TEA TOWEL BAND", 22, 38, 400, 14, juce::Justification::centredLeft);

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
    const juce::String chord = ch >= 0 ? juce::String (ut::pcName (ch & 15)) + ((ch & 16) ? "m" : "") : "";
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
            auto b = fa.withX (fa.getX() + i * w).withWidth (w).reduced (3, 0).withHeight (46);
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
    g.drawText ("KEY AND CHORDS", songArea.reduced (18, 12).removeFromTop (18), juce::Justification::centredLeft);
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
    struct M { const char* name; band::Who who; bool follows; };
    const M members[] = { { "Tea Towel (drums)", band::WTeaTowel, true }, { "Undertow (bass)", band::WUndertow, true },
                          { "Loon", band::WLoon, false }, { "Schaltwerk", band::WSchaltwerk, false }, { "Carousel", band::WCarousel, false },
                          { "Tape Twin", band::WTapeTwin, false }, { "Lathe", band::WLathe, false } };
    for (auto& m : members)
    {
        auto r = ba.removeFromTop (24);
        const bool here = band::isPresent (m.who);
        g.setColour (here ? kGreen : col::line); g.fillEllipse (r.removeFromLeft (14).withSizeKeepingCentre (10, 10).toFloat());
        r.removeFromLeft (6);
        g.setColour (m.follows ? col::cream : col::muted); g.setFont (font (13.0f, m.follows));
        g.drawText (m.name, r.removeFromLeft (150), juce::Justification::centredLeft);
        g.setColour (col::muted); g.setFont (font (11.0f));
        g.drawText (m.follows ? (here ? "following" : "not in this set") : "joins in a later update", r, juce::Justification::centredLeft);
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
    g.drawFittedText ("Put Bandleader on its own MIDI track (it makes no sound) and press play. Tea Towel and Undertow follow it automatically. "
                      "Send the footswitch to this track, or MIDI Map the \"Footswitch:\" parameters.",
                      legendArea, juce::Justification::topLeft, 3);
}

void BandleaderEditor::resized()
{
    auto r = getLocalBounds();
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
    formArea = r.removeFromTop (130);
    {
        auto sb = formArea.reduced (18, 12).removeFromBottom (34);
        const int w = sb.getWidth() / 3;
        for (int i = 0; i < 3; ++i) secB[i].setBounds (sb.removeFromLeft (w).reduced (4, 0));
    }
    r.removeFromTop (12);
    songArea = r;
    auto sa = songArea.reduced (18, 12);
    sa.removeFromTop (40);
    auto row1 = sa.removeFromTop (26);
    energyK->setBounds (sa.withTrimmedTop (8).removeFromRight (110).withHeight (100));
    const int w4 = row1.getWidth() / 4;
    for (int i = 0; i < 4; ++i) labelled[(size_t) i].first->setBounds (row1.removeFromLeft (w4).reduced (4, 0));
    sa.removeFromTop (24);
    auto row2 = sa.removeFromTop (26).withTrimmedRight (120);
    const int w3 = row2.getWidth() / 3;
    for (int i = 0; i < 3; ++i) progBox[i]->setBounds (row2.removeFromLeft (w3).reduced (4, 0));
}
