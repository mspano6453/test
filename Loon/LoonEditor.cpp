#include "LoonEditor.h"
#include <map>

using namespace ln;
using ttui::font;

namespace
{
const juce::Colour kBg    { 0xff0d1317 };
const juce::Colour kPanel { 0xff151e24 };
const juce::Colour kLine  { 0xff253139 };
const juce::Colour kTeal  { 0xff63b8b4 };
const juce::Colour kText  { 0xffe3ece9 };
const juce::Colour kMuted { 0xff7f9194 };
const juce::Colour kGold  { 0xffd9b46a };
}

ttui::Knob* LoonEditor::knob (const char* id, const char* label, int pg)
{
    auto* k = new ttui::Knob (label, false, this);
    k->attach (proc.apvts, id);
    owned.add (k); addAndMakeVisible (k); pageComps[pg].push_back (k); byId[id] = k;
    return k;
}

juce::ComboBox* LoonEditor::combo (const char* id, int pg)
{
    auto* c = new juce::ComboBox();
    if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id))) c->addItemList (ch->choices, 1);
    cAtt.add (new APVTS::ComboBoxAttachment (proc.apvts, id, *c));
    owned.add (c); addAndMakeVisible (c); pageComps[pg].push_back (c); byId[id] = c;
    return c;
}

juce::ToggleButton* LoonEditor::toggle (const char* id, const char* label, int pg)
{
    auto* t = new juce::ToggleButton (label);
    t->setColour (juce::ToggleButton::textColourId, kText);
    t->setColour (juce::ToggleButton::tickColourId, kTeal);
    t->setColour (juce::ToggleButton::tickDisabledColourId, kMuted);
    bAtt.add (new APVTS::ButtonAttachment (proc.apvts, id, *t));
    owned.add (t); addAndMakeVisible (t); pageComps[pg].push_back (t); byId[id] = t;
    return t;
}

LoonEditor::LoonEditor (LoonProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    lnf.setAccent (kTeal);
    setLookAndFeel (&lnf);

    const char* names[] = { "SCENE", "GUITAR", "NATURE", "SPACE" };
    for (int i = 0; i < 4; ++i)
    {
        tabs[i].setButtonText (names[i]);
        tabs[i].setRadioGroupId (88);
        tabs[i].setClickingTogglesState (true);
        tabs[i].onClick = [this, i] { showPage (i); };
        addAndMakeVisible (tabs[i]);
    }
    tabs[0].setToggleState (true, juce::dontSendNotification);

    for (int i = 0; i < p.getNumPrograms(); ++i) sceneBox.addItem (p.getProgramName (i), i + 1);
    sceneBox.setSelectedId (p.getCurrentProgram() + 1, juce::dontSendNotification);
    sceneBox.onChange = [this] { proc.setCurrentProgram (sceneBox.getSelectedId() - 1); };
    addAndMakeVisible (sceneBox);

    // SCENE
    toggle ("gen_on", "Generator on", 0);
    toggle ("follow", "Follow my playing", 0);
    combo ("mood", 0); combo ("root", 0); combo ("journey", 0);
    knob ("weather", "WEATHER", 0); knob ("evolve", "EVOLVE", 0); knob ("bright", "BRIGHTNESS", 0);
    knob ("pads", "PADS", 0); knob ("bells", "BELLS", 0); knob ("glass", "GLASS", 0); knob ("drone", "DRONE", 0); knob ("gen_lvl", "LEVEL", 0);

    // GUITAR
    freezeButton.setClickingTogglesState (true);
    freezeAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "freeze", freezeButton);
    addAndMakeVisible (freezeButton); pageComps[1].push_back (&freezeButton);
    combo ("cloud_pitch", 1);
    knob ("cloud_amt", "CLOUD", 1); knob ("cloud_swell", "SWELL", 1); knob ("cloud_den", "DENSITY", 1);
    knob ("cloud_size", "GRAIN SIZE", 1); knob ("cloud_spray", "SPRAY", 1);
    knob ("dry", "GUITAR DRY", 1); knob ("gtr_space", "INTO SPACE", 1);

    // NATURE
    for (auto id : { "wind", "rain", "creek", "crickets", "birds", "loons", "thunder", "nature_lvl" })
    {
        juce::String lbl (id); if (lbl == "nature_lvl") lbl = "level";
        knob (id, lbl.toUpperCase().toRawUTF8(), 2);
    }
    // SPACE
    knob ("v_size", "SIZE", 3); knob ("v_mix", "SPACE", 3); knob ("shimmer", "SHIMMER", 3); knob ("wobble", "TAPE WOBBLE", 3); knob ("out", "OUTPUT", 3);

    setSize (1060, 680);
    showPage (0);
    startTimerHz (30);
}

LoonEditor::~LoonEditor() { setLookAndFeel (nullptr); }

void LoonEditor::showPage (int pg)
{
    page = pg;
    for (int i = 0; i < 4; ++i) for (auto* c : pageComps[i]) c->setVisible (i == pg);
    resized();
    repaint();
}

void LoonEditor::timerCallback()
{
    const uint32_t c = proc.uiNoteCounter.load();
    if (c != lastCounter)
    {
        lastCounter = c;
        const int note = proc.uiLastNote.load();
        const float x = juce::jmap ((float) juce::jlimit (30, 96, note), 30.0f, 96.0f, 0.05f, 0.95f);
        bubbles.push_back ({ x, 0.95f, note < 60 ? 22.0f : note < 72 ? 14.0f : 9.0f, 1.0f, note < 60 ? 0 : note < 72 ? 1 : 2 });
    }
    for (auto& b : bubbles) { b.y -= 0.0025f; b.life -= 0.004f; }
    bubbles.erase (std::remove_if (bubbles.begin(), bubbles.end(), [] (const Bubble& b) { return b.life <= 0 || b.y < 0; }), bubbles.end());
    inputShown = juce::jmax (proc.uiInput.load(), inputShown * 0.9f);
    repaint();
}

void LoonEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    auto header = getLocalBounds().removeFromTop (60);
    g.setColour (kTeal);
    g.fillRect (header.removeFromBottom (2));
    g.setColour (kText);
    g.setFont (font (30.0f, true));
    g.drawText ("LOON", 22, 8, 160, 32, juce::Justification::centredLeft);
    g.setColour (kTeal);
    g.setFont (font (11.0f, true));
    g.drawText ("AMBIENT GENERATOR", 24, 38, 300, 14, juce::Justification::centredLeft);

    const juce::String keyText = juce::String (pcName (proc.uiRoot.load())) + (proc.uiMinor.load() ? " minor" : " major");
    const juce::String keyNote = proc.uiLinked.load() ? "following Bandleader" : proc.uiFollowing.load() ? "following your playing" : "set key";

    auto panel = [&] (juce::Rectangle<int> a, const juce::String& t)
    {
        if (a.isEmpty()) return;
        g.setColour (kPanel); g.fillRoundedRectangle (a.toFloat(), 8.0f);
        g.setColour (kTeal); g.setFont (font (13.0f, true));
        g.drawText (t, a.reduced (14, 8).removeFromTop (18), juce::Justification::centredLeft);
    };

    if (page == 0)
    {
        // sky: each note it plays floats up and fades
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0a1a24), (float) sky.getX(), (float) sky.getY(),
                                                 juce::Colour (0xff1c2b2f), (float) sky.getX(), (float) sky.getBottom(), false));
        g.fillRoundedRectangle (sky.toFloat(), 8.0f);
        for (auto& b : bubbles)
        {
            const juce::Colour c = b.kind == 0 ? kTeal : b.kind == 1 ? kGold : kText;
            const float x = (float) sky.getX() + b.x * (float) sky.getWidth(), y = (float) sky.getY() + b.y * (float) sky.getHeight();
            g.setColour (c.withAlpha (0.12f * b.life)); g.fillEllipse (x - b.r * 2, y - b.r * 2, b.r * 4, b.r * 4);
            g.setColour (c.withAlpha (0.55f * b.life)); g.fillEllipse (x - b.r * 0.5f, y - b.r * 0.5f, b.r, b.r);
        }
        g.setColour (kText); g.setFont (font (26.0f, true));
        g.drawText (keyText, sky.reduced (18, 12).removeFromTop (30), juce::Justification::topLeft);
        g.setColour (kMuted); g.setFont (font (12.0f));
        g.drawText (keyNote, sky.reduced (18, 12).withTrimmedTop (32).removeFromTop (16), juce::Justification::topLeft);
        if (proc.uiJourney.load() > 0)
        {
            auto jb = sky.reduced (18, 14).removeFromBottom (4).toFloat();
            g.setColour (kLine); g.fillRoundedRectangle (jb, 2);
            g.setColour (kTeal); g.fillRoundedRectangle (jb.withWidth (jb.getWidth() * proc.uiJourney.load()), 2);
        }
        panel (panelA, "THE MUSIC");
        panel (panelB, "LAYERS");
        g.setColour (kMuted); g.setFont (font (11.0f, true));
        for (auto id : { "mood", "root", "journey" })
            if (auto* c = byId[id]) g.drawText (juce::String (id).toUpperCase() == "ROOT" ? "KEY (when not following)" : juce::String (id).toUpperCase(),
                                                c->getBounds().translated (0, -16).withHeight (14), juce::Justification::centredLeft);
    }
    else if (page == 1)
    {
        panel (panelA, "CLOUD - your playing, stretched into a slow granular haze");
        panel (panelB, "GUITAR");
        g.setColour (kMuted); g.setFont (font (11.0f, true));
        if (auto* c = byId["cloud_pitch"]) g.drawText ("CLOUD PITCH", c->getBounds().translated (0, -16).withHeight (14), juce::Justification::centredLeft);
        // input meter + key
        g.setColour (kLine); g.fillRoundedRectangle (meterArea.toFloat(), 3.0f);
        const float lv = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (inputShown) + 60.0f) / 60.0f);
        g.setColour (kTeal); g.fillRoundedRectangle (meterArea.toFloat().withWidth ((float) meterArea.getWidth() * lv), 3.0f);
        g.setColour (kMuted); g.setFont (font (11.0f, true));
        g.drawText ("INPUT", meterArea.translated (0, -16).withHeight (14), juce::Justification::centredLeft);
        g.setColour (kText); g.setFont (font (15.0f, true));
        g.drawText ("Hearing: " + keyText, meterArea.translated (0, 12).withHeight (22), juce::Justification::centredLeft);
        g.setColour (kMuted); g.setFont (font (12.0f));
        g.drawFittedText ("Put LOON on your guitar track. Play a few chords and it finds your key (SCENE > Follow my playing) - "
                          "the pads and bells move with you. FREEZE holds the cloud so you can solo over it. "
                          "SWELL makes the cloud rise when you play and fade when you stop.",
                          panelB.reduced (14, 10).withTrimmedTop (240), juce::Justification::topLeft, 8, 1.0f);
    }
    else if (page == 2)
    {
        panel (panelA, "NATURE - synthesized, never loops");
        g.setColour (kMuted); g.setFont (font (12.0f));
        g.drawFittedText ("WEATHER on the SCENE page pushes wind and rain harder and lets the thunder in.",
                          panelA.reduced (14, 10).removeFromBottom (24), juce::Justification::left, 1, 1.0f);
    }
    else
    {
        panel (panelA, "SPACE - huge reverb, octave-up shimmer, and tape wobble");
        g.setColour (kMuted); g.setFont (font (12.0f));
        g.drawFittedText ("Everything Loon makes goes through this space. Your guitar goes in too, as much as GUITAR > INTO SPACE sends. "
                          "Loon also sends the notes it plays out as MIDI, for hosts that can route it to another instrument.",
                          panelA.reduced (14, 10).removeFromBottom (40), juce::Justification::topLeft, 2, 1.0f);
    }
}

void LoonEditor::layoutRow (juce::Rectangle<int> r, const std::vector<juce::Component*>& comps, int w)
{
    for (auto* c : comps) c->setBounds (r.removeFromLeft (w).reduced (4, 0));
}

void LoonEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (60).reduced (16, 14);
    sceneBox.setBounds (header.removeFromRight (230));
    header.removeFromRight (16);
    for (int i = 3; i >= 0; --i) { tabs[i].setBounds (header.removeFromRight (90)); header.removeFromRight (6); }

    auto c = r.reduced (16, 14);
    panelA = panelB = panelC = {};

    if (page == 0)
    {
        sky = c.removeFromTop (220);
        c.removeFromTop (12);
        panelA = c.removeFromLeft (c.getWidth() / 2 - 6);
        c.removeFromLeft (12);
        panelB = c;
        auto a = panelA.reduced (14, 10).withTrimmedTop (26);
        auto top = a.removeFromTop (26);
        byId["gen_on"]->setBounds (top.removeFromLeft (160));
        byId["follow"]->setBounds (top.removeFromLeft (200));
        a.removeFromTop (24);
        auto combos = a.removeFromTop (26);
        const int cw = combos.getWidth() / 3;
        for (auto id : { "mood", "root", "journey" }) byId[id]->setBounds (combos.removeFromLeft (cw).reduced (4, 0));
        a.removeFromTop (14);
        layoutRow (a.removeFromTop (110), { byId["weather"], byId["evolve"], byId["bright"] }, a.getWidth() / 3);
        auto b = panelB.reduced (14, 10).withTrimmedTop (30);
        auto row1 = b.removeFromTop (110);
        layoutRow (row1, { byId["pads"], byId["bells"], byId["glass"] }, row1.getWidth() / 3);
        b.removeFromTop (10);
        auto row2 = b.removeFromTop (110);
        layoutRow (row2, { byId["drone"], byId["gen_lvl"] }, row2.getWidth() / 3);
    }
    else if (page == 1)
    {
        panelA = c.removeFromLeft ((int) (c.getWidth() * 0.62f));
        c.removeFromLeft (12);
        panelB = c;
        auto a = panelA.reduced (14, 10).withTrimmedTop (30);
        auto top = a.removeFromTop (70);
        freezeButton.setBounds (top.removeFromLeft (200).reduced (0, 6));
        top.removeFromLeft (24);
        byId["cloud_pitch"]->setBounds (top.removeFromLeft (180).withSizeKeepingCentre (180, 26).translated (0, 8));
        a.removeFromTop (16);
        const int kw = a.getWidth() / 3;
        layoutRow (a.removeFromTop (110), { byId["cloud_amt"], byId["cloud_swell"], byId["cloud_den"] }, kw);
        a.removeFromTop (10);
        layoutRow (a.removeFromTop (110), { byId["cloud_size"], byId["cloud_spray"] }, kw);
        auto b = panelB.reduced (14, 10).withTrimmedTop (30);
        layoutRow (b.removeFromTop (110), { byId["dry"], byId["gtr_space"] }, b.getWidth() / 2);
        meterArea = b.withTrimmedTop (34).removeFromTop (8);
    }
    else if (page == 2)
    {
        panelA = c.removeFromTop (300);
        auto a = panelA.reduced (14, 10).withTrimmedTop (30);
        layoutRow (a.removeFromTop (120), { byId["wind"], byId["rain"], byId["creek"], byId["crickets"], byId["birds"], byId["loons"], byId["thunder"], byId["nature_lvl"] },
                   a.getWidth() / 8);
    }
    else
    {
        panelA = c.removeFromTop (260);
        auto a = panelA.reduced (14, 10).withTrimmedTop (30);
        layoutRow (a.removeFromTop (120), { byId["v_size"], byId["v_mix"], byId["shimmer"], byId["wobble"], byId["out"] }, a.getWidth() / 5);
    }
}
