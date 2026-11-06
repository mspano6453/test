#include "SchaltwerkEditor.h"

using namespace swui;
using namespace sw;
using ttui::font;

namespace
{
const juce::Colour kBg     { 0xff101010 };
const juce::Colour kPanel  { 0xff1b1b1b };
const juce::Colour kLine   { 0xff2e2e2e };
const juce::Colour kRed    { 0xffd8312a };
const juce::Colour kText   { 0xffe8e8e8 };
const juce::Colour kMuted  { 0xff8a8a8a };
}

// ===========================================================================
// Step grid
// ===========================================================================
juce::Rectangle<float> StepGrid::cell (int row, int step) const
{
    const float w = (float) (getWidth() - labelW()) / 16.0f;
    const float h = (float) getHeight() / NumDrums;
    return { (float) labelW() + w * (float) step + 2.0f + (step / 4) * 1.0f, h * (float) row + 2.0f, w - 4.0f, h - 4.0f };
}

void StepGrid::paint (juce::Graphics& g)
{
    const int cur = proc.currentStep.load();
    const float h = (float) getHeight() / NumDrums;
    for (int r = 0; r < NumDrums; ++r)
    {
        auto lab = juce::Rectangle<float> (0, h * (float) r + 2.0f, (float) labelW() - 8.0f, h - 4.0f);
        g.setColour (r == selectedRow ? kRed.withAlpha (0.25f) : kPanel);
        g.fillRoundedRectangle (lab, 3.0f);
        if (flash[r] > 0.01f) { g.setColour (kRed.withAlpha (0.6f * flash[r])); g.fillRoundedRectangle (lab.withWidth (6.0f), 2.0f); }
        g.setColour (r == selectedRow ? kText : kMuted);
        g.setFont (font (12.0f, true));
        g.drawText (juce::String (drumName (r)).toUpperCase(), lab.reduced (12, 0), juce::Justification::centredLeft);

        for (int s = 0; s < 16; ++s)
        {
            const auto c = cell (r, s);
            const int v = proc.steps[r][s].load();
            juce::Colour col = (s / 4) % 2 == 0 ? kPanel : kPanel.brighter (0.05f);
            if (v == 1) col = kRed.withAlpha (0.7f);
            if (v == 2) col = kRed.brighter (0.25f);
            g.setColour (col);
            g.fillRoundedRectangle (c, 2.0f);
            if (s == cur) { g.setColour (juce::Colours::white.withAlpha (0.18f)); g.fillRoundedRectangle (c, 2.0f); }
            if (v == 2) { g.setColour (kText); g.fillEllipse (c.withSizeKeepingCentre (5, 5)); }
        }
    }
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    paintValue = -1;
    const float h = (float) getHeight() / NumDrums;
    const int r = juce::jlimit (0, NumDrums - 1, (int) (e.position.y / h));
    if (e.position.x < (float) labelW())
    {
        selectedRow = r;
        if (onSelectRow) onSelectRow (r);
        proc.keyboardState.noteOn (1, kDrumBaseNote + r, 0.9f);
        proc.keyboardState.noteOff (1, kDrumBaseNote + r, 0.0f);
        repaint();
        return;
    }
    for (int s = 0; s < 16; ++s)
        if (cell (r, s).expanded (2).contains (e.position))
        {
            const int v = proc.steps[r][s].load();
            // click cycles off -> on -> accent -> off; shift-click sets accent; right-click clears
            int nv = e.mods.isPopupMenu() ? 0 : e.mods.isShiftDown() ? 2 : (v + 1) % 3;
            proc.steps[r][s].store ((uint8_t) nv);
            paintValue = nv;
            selectedRow = r;
            if (onSelectRow) onSelectRow (r);
            repaint();
            return;
        }
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (paintValue < 0) return;
    const float h = (float) getHeight() / NumDrums;
    const int r = juce::jlimit (0, NumDrums - 1, (int) (e.position.y / h));
    for (int s = 0; s < 16; ++s)
        if (cell (r, s).contains (e.position)) { proc.steps[r][s].store ((uint8_t) paintValue); repaint(); }
}

// ===========================================================================
// Omnichord chord buttons
// ===========================================================================
juce::Rectangle<float> ChordPad::button (int type, int root) const
{
    const float lw = 70.0f;
    const float w = ((float) getWidth() - lw) / 12.0f;
    const float h = ((float) getHeight() - 24.0f) / 3.0f;
    // the real thing staggers its rows; so do we
    const float stagger = (float) type * w * 0.25f;
    return { lw + w * (float) root + 3.0f + stagger * 0.0f, 24.0f + h * (float) type + 4.0f, w - 6.0f, h - 8.0f };
}

void ChordPad::paint (juce::Graphics& g)
{
    const char* typeNames[] = { "MAJOR", "MINOR", "7TH" };
    const bool on = proc.chordSounding.load();
    const int curRoot = (int) proc.apvts.getRawParameterValue ("om_root")->load();
    const int curType = (int) proc.apvts.getRawParameterValue ("om_type")->load();
    g.setFont (font (11.0f, true));
    for (int r = 0; r < 12; ++r)
    {
        const auto b = button (0, r);
        g.setColour (kMuted);
        g.drawText (noteName (r), juce::Rectangle<float> (b.getX(), 2.0f, b.getWidth(), 18.0f), juce::Justification::centred);
    }
    for (int t = 0; t < NumChordTypes; ++t)
    {
        const auto b0 = button (t, 0);
        g.setColour (kMuted);
        g.drawText (typeNames[t], juce::Rectangle<float> (0, b0.getY(), 64.0f, b0.getHeight()), juce::Justification::centredLeft);
        for (int r = 0; r < 12; ++r)
        {
            const auto b = button (t, r);
            const bool lit = on && r == curRoot && t == curType;
            g.setColour (lit ? kRed : juce::Colour (0xff2a2a2a));
            g.fillRoundedRectangle (b, 4.0f);
            g.setColour (lit ? kRed.brighter (0.4f) : kLine.brighter (0.3f));
            g.drawRoundedRectangle (b, 4.0f, 1.0f);
            g.setColour (lit ? kText : kMuted);
            g.drawText (juce::String (noteName (r)) + (t == ChMinor ? "m" : t == ChSeventh ? "7" : ""), b, juce::Justification::centred);
        }
    }
}

void ChordPad::mouseDown (const juce::MouseEvent& e)
{
    for (int t = 0; t < NumChordTypes; ++t)
        for (int r = 0; r < 12; ++r)
            if (button (t, r).contains (e.position)) { downType = t; downRoot = r; proc.pressChord (r, t); repaint(); return; }
}

void ChordPad::mouseUp (const juce::MouseEvent&)
{
    if (downType >= 0) proc.releaseChord();
    downType = downRoot = -1;
    repaint();
}

// ===========================================================================
// Strumplate
// ===========================================================================
void StrumPlate::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0b0b0b));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (kLine.brighter (0.2f));
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
    const int lit = proc.lastString.load();
    for (int s = 0; s < kStrings; ++s)
    {
        const float y = r.getBottom() - (r.getHeight() - 20.0f) * ((float) s + 0.5f) / kStrings - 10.0f;
        g.setColour (s == lit ? kRed : kMuted.withAlpha (0.5f));
        g.fillRect (r.getX() + 12.0f, y - (s == lit ? 1.5f : 0.75f), r.getWidth() - 24.0f, s == lit ? 3.0f : 1.5f);
    }
    g.setColour (kMuted);
    g.setFont (font (10.0f, true));
    g.drawText ("STRUM", r.removeFromTop (18), juce::Justification::centred);
}

void StrumPlate::mouseDrag (const juce::MouseEvent& e)
{
    const float frac = 1.0f - juce::jlimit (0.0f, 0.999f, (e.position.y - 10.0f) / ((float) getHeight() - 20.0f));
    proc.uiStrum = juce::jlimit (0, kStrings - 1, (int) (frac * kStrings));
    repaint();
}

// ===========================================================================
// Editor
// ===========================================================================
ttui::Knob* SchaltwerkEditor::knob (const juce::String& id, const juce::String& label, bool small)
{
    auto* k = knobs.add (new ttui::Knob (label, small, this));
    k->attach (proc.apvts, id);
    addAndMakeVisible (k);
    return k;
}

juce::ComboBox* SchaltwerkEditor::combo (const juce::String& id)
{
    auto* c = combos.add (new juce::ComboBox());
    if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id))) c->addItemList (ch->choices, 1);
    comboAtts.add (new swui::APVTS::ComboBoxAttachment (proc.apvts, id, *c));
    addAndMakeVisible (c);
    return c;
}

juce::ToggleButton* SchaltwerkEditor::toggle (const juce::String& id, const juce::String& label)
{
    auto* t = toggles.add (new juce::ToggleButton (label));
    t->setColour (juce::ToggleButton::textColourId, kText);
    t->setColour (juce::ToggleButton::tickColourId, kRed);
    t->setColour (juce::ToggleButton::tickDisabledColourId, kMuted);
    toggleAtts.add (new swui::APVTS::ButtonAttachment (proc.apvts, id, *t));
    addAndMakeVisible (t);
    return t;
}

SchaltwerkEditor::SchaltwerkEditor (SchaltwerkProcessor& p)
    : AudioProcessorEditor (&p), proc (p), grid (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard),
      chordPad (p), strum (p)
{
    lnf.setAccent (kRed);
    setLookAndFeel (&lnf);

    for (auto* t : { &tabRhythm, &tabRobot, &tabOmni })
    {
        t->setRadioGroupId (77);
        t->setClickingTogglesState (true);
        addAndMakeVisible (*t);
    }
    tabRhythm.onClick = [this] { showPage (0); };
    tabRobot.onClick = [this] { showPage (1); };
    tabOmni.onClick = [this] { showPage (2); };
    tabRhythm.setToggleState (true, juce::dontSendNotification);

    // ---- RHYTHM
    for (int i = 0; i < p.getNumPrograms(); ++i) patternBox.addItem (p.getProgramName (i), i + 1);
    patternBox.setSelectedId (p.getCurrentProgram() + 1, juce::dontSendNotification);
    patternBox.onChange = [this] { proc.setCurrentProgram (patternBox.getSelectedId() - 1); grid.repaint(); };
    addAndMakeVisible (patternBox);
    clearButton.onClick = [this] { proc.clearPattern(); grid.repaint(); };
    addAndMakeVisible (clearButton);
    addAndMakeVisible (grid);
    grid.onSelectRow = [this] (int r) { selectRow (r); };
    seqToggle = toggle ("seq_on", "Play pattern with Live");
    swingKnob = knob ("seq_swing", "SWING", true);
    drumSendKnob = knob ("drum_send", "ECHO", true);
    const char* rowLabels[5] = { "TUNE", "DECAY", "TONE", "LEVEL", "PAN" };
    for (int i = 0; i < 5; ++i)
    {
        rowKnobs[i] = std::make_unique<ttui::Knob> (rowLabels[i], false, this);
        if (i == 0 || i == 4) rowKnobs[i]->slider.getProperties().set ("bipolar", true);
        addAndMakeVisible (*rowKnobs[i]);
    }
    rhythmComps = { &patternBox, &clearButton, &grid, seqToggle, swingKnob, drumSendKnob };
    for (auto& k : rowKnobs) rhythmComps.push_back (k.get());
    selectRow (0);

    // ---- ROBOT
    modeBox = combo ("sy_mode");
    const std::pair<const char*, const char*> rk[] = {
        { "sy_wave", "SAW / PULSE" }, { "sy_pw", "WIDTH" }, { "sy_det", "DETUNE" },
        { "sy_cut", "CUTOFF" }, { "sy_res", "RESONANCE" }, { "sy_env", "ENV AMT" },
        { "sy_a", "ATTACK" }, { "sy_d", "DECAY" }, { "sy_s", "SUSTAIN" }, { "sy_r", "RELEASE" },
        { "sy_glide", "GLIDE" }, { "sy_vowel", "VOWEL" }, { "sy_shift", "FORMANT" }, { "sy_sib", "SIBILANCE" },
        { "sy_lvl", "LEVEL" }, { "sy_send", "ECHO" } };
    for (auto& k : rk) robotKnobs.push_back (knob (k.first, k.second));
    keyboard.setAvailableRange (48, 96);
    keyboard.setLowestVisibleKey (48);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, kRed);
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, kRed.withAlpha (0.3f));
    addAndMakeVisible (keyboard);
    robotComps = { modeBox, &keyboard };
    for (auto* k : robotKnobs) robotComps.push_back (k);

    // ---- OMNICHORD
    addAndMakeVisible (chordPad);
    addAndMakeVisible (strum);
    omniCtl = { knob ("om_chord", "CHORDS"), knob ("om_tone", "TONE"), knob ("om_bass", "BASS"), knob ("om_harp", "HARP"),
                knob ("om_decay", "SUSTAIN"), knob ("om_vib", "VIBRATO"), knob ("om_send", "ECHO") };
    omniCtl.push_back (combo ("om_oct"));
    omniCtl.push_back (combo ("om_auto"));
    omniCtl.push_back (toggle ("om_hold", "Hold chord"));
    omniComps = { &chordPad, &strum };
    for (auto* c : omniCtl) omniComps.push_back (c);

    // ---- shared echo / output bar
    echoCtl = { combo ("ec_time"), knob ("ec_fb", "REPEATS", true), knob ("ec_mix", "ECHO LVL", true), knob ("out", "OUTPUT", true) };

    setSize (1100, 740);
    showPage (0);
    startTimerHz (30);
}

SchaltwerkEditor::~SchaltwerkEditor() { setLookAndFeel (nullptr); }

void SchaltwerkEditor::selectRow (int r)
{
    grid.selectedRow = r;
    const char* ids[5] = { "tune", "dec", "tone", "lvl", "pan" };
    for (int i = 0; i < 5; ++i) rowKnobs[i]->attach (proc.apvts, dId (r, ids[i]));
    repaint (rowPanel);
}

void SchaltwerkEditor::showPage (int pg)
{
    page = pg;
    for (auto* c : rhythmComps) c->setVisible (pg == 0);
    for (auto* c : robotComps) c->setVisible (pg == 1);
    for (auto* c : omniComps) c->setVisible (pg == 2);
    helpArea = pg == 0 ? rhythmHelp : pg == 1 ? robotHelp : omniHelp;
    repaint();
}

void SchaltwerkEditor::timerCallback()
{
    for (int d = 0; d < NumDrums; ++d)
        grid.flash[d] = juce::jmax (proc.drumFlash[d].exchange (0.0f), grid.flash[d] * 0.8f);
    if (page == 0) grid.repaint();
    if (page == 2) { chordPad.repaint(); strum.repaint(); }
}

void SchaltwerkEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    auto header = getLocalBounds().removeFromTop (60);
    g.setColour (kRed);
    g.fillRect (header.removeFromBottom (3));
    g.setColour (kText);
    g.setFont (font (30.0f, true));
    g.drawText ("SCHALTWERK", 22, 8, 320, 32, juce::Justification::centredLeft);
    g.setColour (kRed);
    g.setFont (font (11.0f, true));
    g.drawText ("RHYTHMUS  -  ROBOTER  -  OMNICHORD", 24, 38, 400, 14, juce::Justification::centredLeft);

    auto panel = [&] (juce::Rectangle<int> a, const juce::String& title)
    {
        g.setColour (kPanel);
        g.fillRect (a);
        g.setColour (kRed);
        g.fillRect (a.withHeight (2));
        g.setColour (kText);
        g.setFont (font (13.0f, true));
        g.drawText (title, a.reduced (12, 6).removeFromTop (18), juce::Justification::centredLeft);
    };

    if (page == 0)
    {
        panel (rowPanel, juce::String (drumName (grid.selectedRow)).toUpperCase() + "  (MIDI " + juce::String (kDrumBaseNote + grid.selectedRow) + ")");
        g.setColour (kMuted);
        g.setFont (font (12.0f));
        g.drawFittedText ("Click a step to add a hit, click again for an accent, again to clear. Shift-click = accent, right-click = clear, drag to paint. "
                          "Click a drum's name to hear it. Drums also play from MIDI notes C1 to B1.",
                          helpArea, juce::Justification::topLeft, 3, 1.0f);
    }
    else if (page == 1)
    {
        panel (robotPanel, "ROBOTER");
        g.setColour (kMuted);
        g.setFont (font (12.0f));
        g.drawFittedText ("Play from C2 up. SYNTH is a classic analog lead or pad (turn up GLIDE for a single sliding voice). "
                          "VOWEL CHOIR sings through vowel formants - sweep VOWEL. VOCODER makes it talk: in Live, open this device's "
                          "Sidechain section, pick your vocal or mic track, then hold chords while it plays.",
                          helpArea, juce::Justification::topLeft, 4, 1.0f);
        g.setColour (kText);
        g.setFont (font (12.0f, true));
        const char* groups[] = { "OSCILLATORS", "FILTER", "ENVELOPE", "VOICE" };
        const int gx[] = { 0, 3, 6, 10 };
        for (int i = 0; i < 4; ++i)
        {
            auto* k = robotKnobs[(size_t) gx[i]];
            g.drawText (groups[i], k->getX(), k->getY() - 18, 200, 16, juce::Justification::centredLeft);
        }
    }
    else
    {
        panel (omniPanel, "OMNICHORD");
        g.setColour (kMuted);
        g.setFont (font (12.0f));
        g.setColour (kMuted);
        g.setFont (font (11.0f, true));
        const char* lbl[] = { "HARP OCTAVE", "AUTO STRUM" };
        for (int i = 0; i < 2; ++i)
        {
            auto b = omniCtl[(size_t) (7 + i)]->getBounds();
            g.drawText (lbl[i], b.translated (0, -20).withHeight (16), juce::Justification::centredLeft);
        }
        g.setFont (font (12.0f));
        g.drawFittedText ("Hold a chord button, then sweep the strumplate (or your keyboard's mod wheel / touch strip). "
                          "From MIDI: notes 24-35 play major chords, 12-23 minor, 0-11 sevenths. AUTO STRUM arpeggiates in time with Live.",
                          helpArea, juce::Justification::topLeft, 3, 1.0f);
    }

    g.setColour (kPanel);
    g.fillRect (bottomBar);
    g.setColour (kMuted);
    g.setFont (font (11.0f, true));
    g.drawText ("ECHO", bottomBar.reduced (16, 0).removeFromLeft (60), juce::Justification::centredLeft);
}

void SchaltwerkEditor::placeRow (juce::Rectangle<int> area, std::initializer_list<juce::Component*> comps)
{
    const int w = area.getWidth() / (int) comps.size();
    for (auto* c : comps) c->setBounds (area.removeFromLeft (w).reduced (4, 0));
}

void SchaltwerkEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (60).reduced (16, 14);
    tabOmni.setBounds (header.removeFromRight (120)); header.removeFromRight (6);
    tabRobot.setBounds (header.removeFromRight (100)); header.removeFromRight (6);
    tabRhythm.setBounds (header.removeFromRight (100));

    bottomBar = r.removeFromBottom (86);
    {
        auto b = bottomBar.reduced (16, 6);
        b.removeFromLeft (60);
        echoCtl[0]->setBounds (b.removeFromLeft (150).withSizeKeepingCentre (150, 26));
        b.removeFromLeft (10);
        for (size_t i = 1; i < echoCtl.size(); ++i) echoCtl[i]->setBounds (b.removeFromLeft (90));
    }

    auto c = r.reduced (16, 12);

    // RHYTHM
    {
        auto a = c;
        auto top = a.removeFromTop (30);
        patternBox.setBounds (top.removeFromLeft (220));
        top.removeFromLeft (10);
        clearButton.setBounds (top.removeFromLeft (80));
        top.removeFromLeft (20);
        seqToggle->setBounds (top.removeFromLeft (230));
        a.removeFromTop (8);
        grid.setBounds (a.removeFromTop (12 * 27));
        a.removeFromTop (10);
        rowPanel = a.removeFromTop (a.getHeight() - 34);
        auto rp = rowPanel.reduced (12, 6).withTrimmedTop (22);
        auto right = rp.removeFromRight (200);
        swingKnob->setBounds (right.removeFromLeft (100));
        drumSendKnob->setBounds (right);
        for (auto& k : rowKnobs) k->setBounds (rp.removeFromLeft (110));
    }
    // ROBOT
    {
        auto a = c;
        robotPanel = a.removeFromTop (a.getHeight() - 150);
        auto rp = robotPanel.reduced (12, 6).withTrimmedTop (24);
        modeBox->setBounds (rp.removeFromTop (28).removeFromLeft (220));
        rp.removeFromTop (26);
        const int kw = rp.getWidth() / 8;
        auto row1 = rp.removeFromTop (rp.getHeight() / 2);
        auto row2 = rp.withTrimmedTop (22);
        row1 = row1.withHeight (juce::jmin (110, row1.getHeight()));
        row2 = row2.withHeight (juce::jmin (110, row2.getHeight()));
        for (int i = 0; i < 8; ++i) robotKnobs[(size_t) i]->setBounds (row1.removeFromLeft (kw).reduced (6, 0));
        for (int i = 8; i < 16; ++i) robotKnobs[(size_t) i]->setBounds (row2.removeFromLeft (kw).reduced (6, 0));
        a.removeFromTop (8);
        robotHelp = a.removeFromTop (54);
        keyboard.setBounds (a.reduced (0, 4));
        keyboard.setKeyWidth ((float) keyboard.getWidth() / 29.0f);
    }
    // OMNICHORD
    {
        auto a = c;
        omniPanel = a.removeFromTop (a.getHeight() - 54);
        auto op = omniPanel.reduced (12, 6).withTrimmedTop (24);
        strum.setBounds (op.removeFromRight (130));
        op.removeFromRight (16);
        chordPad.setBounds (op.removeFromTop (240));
        op.removeFromTop (12);
        auto row = op.removeFromTop (100);
        const int kw = row.getWidth() / 10;
        for (int i = 0; i < 7; ++i) omniCtl[(size_t) i]->setBounds (row.removeFromLeft (kw).reduced (4, 0));
        for (int i = 7; i < 10; ++i) omniCtl[(size_t) i]->setBounds (row.removeFromLeft (kw + 10).withSizeKeepingCentre (kw, 26));
        omniHelp = a.withTrimmedTop (8);
    }
    rhythmHelp = c.withTop (rowPanel.getBottom() + 4);
    helpArea = page == 0 ? rhythmHelp : page == 1 ? robotHelp : omniHelp;
}
