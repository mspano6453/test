#include "UndertowEditor.h"

using namespace ut;
using ttui::font;
namespace col = ttui::col;

namespace
{
const juce::Colour kTeal { 0xff4fb3bf };
const juce::Colour kPink { 0xffd9677f };
const char* kSectionNames[3] = { "VERSE", "CHORUS", "BRIDGE" };
}

ttui::Knob* UndertowEditor::knob (const char* id, const char* label, bool small)
{
    auto* k = new ttui::Knob (label, small, this);
    k->attach (proc.apvts, id);
    owned.add (k); addAndMakeVisible (k);
    return k;
}

juce::ComboBox* UndertowEditor::combo (const char* id)
{
    auto* c = new juce::ComboBox();
    if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id))) c->addItemList (ch->choices, 1);
    cAtt.add (new APVTS::ComboBoxAttachment (proc.apvts, id, *c));
    owned.add (c); addAndMakeVisible (c);
    return c;
}

UndertowEditor::UndertowEditor (UndertowProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    lnf.setAccent (kTeal);
    setLookAndFeel (&lnf);

    for (int i = 0; i < 3; ++i)
    {
        sectionButtons[i].setButtonText (kSectionNames[i]);
        sectionButtons[i].setClickingTogglesState (false);
        sectionButtons[i].onClick = [this, i]
        {
            if (auto* prm = proc.apvts.getParameter ("section"))
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i));
                prm->endChangeGesture();
            }
            if (auto* sg = proc.apvts.getParameter ("song"))   // picking a section by hand = manual song
            {
                sg->beginChangeGesture(); sg->setValueNotifyingHost (1.0f); sg->endChangeGesture();
            }
        };
        addAndMakeVisible (sectionButtons[i]);
    }
    fillButton.onClick = [this] { proc.uiFill = true; fillFlash = 12; };
    newLineButton.onClick = [this] { proc.uiNewLine = true; };
    brightButton.setClickingTogglesState (true);
    bAtt.add (new APVTS::ButtonAttachment (proc.apvts, "bright", brightButton));
    for (auto* b : { &fillButton, &newLineButton, &brightButton }) addAndMakeVisible (*b);

    modeBox = combo ("mode"); followBox = combo ("follow"); feelBox = combo ("feel");
    keyBox = combo ("key"); progBox[0] = combo ("prog_verse"); progBox[1] = combo ("prog_chorus"); progBox[2] = combo ("prog_bridge");
    chordBarsBox = combo ("chord_bars"); songBox = combo ("song"); secBarsBox = combo ("section_bars");
    regBox = combo ("register"); octBox = combo ("octave"); stopBox = combo ("band_stop");

    playerKnobs = { knob ("variation", "VARIATION"), knob ("fills", "FILLS"), knob ("hook", "HOOK"), knob ("density", "BUSY"),
                    knob ("push", "PUSH"), knob ("human", "HUMAN"), knob ("swing", "SWING"), knob ("length", "LENGTH") };
    rigKnobs = { knob ("comp", "COMP", true), knob ("grit", "GRIT", true), knob ("ch_mix", "CHORUS", true), knob ("ch_rate", "CH RATE", true),
                 knob ("ch_depth", "CH DEPTH", true), knob ("fl_mix", "FLANGER", true), knob ("fl_rate", "FL RATE", true), knob ("fl_fb", "FL FEEDBK", true),
                 knob ("bass_db", "BASS", true), knob ("mid_db", "MID", true), knob ("treble_db", "TREBLE", true), knob ("out", "OUTPUT", true) };
    guitarKnobs = { knob ("sens", "SENSITIVITY", true), knob ("guitar_dry", "GTR BLEND", true) };

    setSize (1080, 760);
    startTimerHz (30);
}

UndertowEditor::~UndertowEditor() { setLookAndFeel (nullptr); }

void UndertowEditor::timerCallback()
{
    const bool selfAuto = proc.apvts.getRawParameterValue ("follow")->load() < 0.5f && proc.apvts.getRawParameterValue ("song")->load() < 0.5f;
    const int sec = selfAuto && proc.uiTransport.load() ? proc.uiSection.load() : (int) proc.apvts.getRawParameterValue ("section")->load();
    for (int i = 0; i < 3; ++i) sectionButtons[i].setToggleState (i == sec, juce::dontSendNotification);
    if (fillFlash > 0) --fillFlash;
    fillButton.setToggleState (fillFlash > 0, juce::dontSendNotification);
    inShown = juce::jmax (proc.uiInput.load(), inShown * 0.85f);
    const int mode = (int) proc.apvts.getRawParameterValue ("mode")->load();
    const bool player = mode == MPlayer;
    for (auto* k : playerKnobs) k->setEnabled (player);
    for (auto* b : { &fillButton, &newLineButton }) b->setEnabled (player);
    for (auto& b : sectionButtons) b.setEnabled (player);
    const bool self = player && proc.apvts.getRawParameterValue ("follow")->load() < 0.5f;
    for (auto* cb : { keyBox, progBox[0], progBox[1], progBox[2], chordBarsBox, songBox, secBarsBox }) cb->setEnabled (self);
    feelBox->setEnabled (player); regBox->setEnabled (player); stopBox->setEnabled (player); followBox->setEnabled (player);
    guitarKnobs[0]->setEnabled (mode == MTracked);
    guitarKnobs[1]->setEnabled (mode == MOctave || mode == MTracked);
    repaint();
}

void UndertowEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    auto header = getLocalBounds().removeFromTop (60);
    g.setColour (col::panel); g.fillRect (header);
    g.setColour (kTeal); g.fillRect (header.removeFromBottom (2));
    g.setColour (col::cream); g.setFont (font (28.0f, true));
    g.drawText ("UNDERTOW", 20, 8, 300, 30, juce::Justification::centredLeft);
    g.setColour (kTeal); g.setFont (font (11.0f, true));
    g.drawText ("POST-PUNK BASS PLAYER", 22, 38, 300, 14, juce::Justification::centredLeft);
    if (! proc.samplesLoaded()) { g.setColour (kPink); g.drawText ("samples missing", 320, 20, 200, 20, juce::Justification::centredLeft); }

    const int mode = (int) proc.apvts.getRawParameterValue ("mode")->load();
    const int follow = (int) proc.apvts.getRawParameterValue ("follow")->load();

    // ---- the ears: what it hears and what it's playing
    g.setColour (col::panel); g.fillRoundedRectangle (earArea.toFloat(), 8.0f);
    auto ea = earArea.reduced (16, 10);
    auto top = ea.removeFromTop (20);
    g.setColour (kTeal); g.setFont (font (13.0f, true));
    g.drawText (mode == MPlayer ? "LISTENING" : mode == MKeys ? "PLAY IT" : "GUITAR TO BASS", top, juce::Justification::centredLeft);

    const int ch = proc.uiChord.load();
    const int note = proc.uiNote.load();
    auto big = ea.removeFromLeft (250);
    if (mode == MPlayer)
    {
        g.setColour (col::muted); g.setFont (font (11.0f, true));
        g.drawText (follow == FollowSelf ? (proc.uiLinked.load() ? "CHORD (from Bandleader)" : "CHORD (playing by itself)") : follow == FollowGuitar ? "CHORD (from your guitar)" : "CHORD (from MIDI)", big.removeFromTop (16), juce::Justification::centredLeft);
        g.setColour (ch >= 0 ? col::cream : col::muted); g.setFont (font (46.0f, true));
        g.drawText (ch >= 0 ? juce::String (pcName (ch & 15)) + ((ch & 16) ? "m" : "") : juce::String ("--"), big.removeFromTop (52), juce::Justification::centredLeft);
    }
    else
    {
        g.setColour (col::muted); g.setFont (font (11.0f, true));
        g.drawText ("BASS NOTE", big.removeFromTop (16), juce::Justification::centredLeft);
        g.setColour (note >= 0 ? col::cream : col::muted); g.setFont (font (46.0f, true));
        g.drawText (mode == MOctave ? juce::String ("8vb") : note >= 0 ? noteName (note) : juce::String ("--"), big.removeFromTop (52), juce::Justification::centredLeft);
    }
    g.setColour (col::muted); g.setFont (font (12.0f));
    juce::String status;
    const bool needsSide = (mode == MPlayer && follow == FollowGuitar) || mode == MOctave || mode == MTracked;
    if (needsSide && ! proc.uiSidechain.load()) status = "Set Sidechain (top of the plugin in Live) to your guitar track.";
    else if (mode == MPlayer)
    {
        if (! proc.uiTransport.load()) status = "Press play in Live - it plays along to the tempo.";
        else if (! proc.uiBandPlaying.load()) status = follow == FollowSelf ? juce::String ("Starting...") : follow == FollowGuitar ? "Waiting for the guitar..." : "Waiting for a chord on the MIDI track...";
        else status = juce::String (proc.uiLinked.load() ? "Following Bandleader: " : "Playing ") + juce::String (note >= 0 ? noteName (note) : "") + "  -  " + kSectionNames[proc.uiSection.load()];
    }
    else if (mode == MKeys) status = "Play it from your KeyStep or a MIDI clip.";
    else if (mode == MTracked) status = "Play single notes on the guitar; it plays them on the bass an octave down.";
    else status = "Your guitar, an octave down, through the bass rig.";
    g.drawFittedText (status, big.removeFromTop (34), juce::Justification::topLeft, 2, 1.0f);

    // input meter
    if (needsSide)
    {
        auto m = big.removeFromTop (8).withWidth (200);
        g.setColour (col::line); g.fillRoundedRectangle (m.toFloat(), 3.0f);
        g.setColour (kTeal); g.fillRoundedRectangle (m.toFloat().withWidth ((float) m.getWidth() * juce::jlimit (0.0f, 1.0f, inShown * 6.0f)), 3.0f);
    }

    // ---- the bass line (2 bars) as a little piano roll
    g.setColour (col::bg.brighter (0.03f)); g.fillRoundedRectangle (rollArea.toFloat(), 6.0f);
    const auto ra = rollArea.reduced (8, 8).toFloat();
    const float cw = ra.getWidth() / (float) kSteps;
    for (int i = 0; i <= kSteps; ++i)
    {
        g.setColour (i % 16 == 0 ? col::muted : i % 4 == 0 ? col::line.brighter (0.15f) : col::line.withAlpha (0.4f));
        g.drawVerticalLine ((int) (ra.getX() + i * cw), ra.getY(), ra.getBottom());
    }
    int lo = 127, hi = 0;
    for (int i = 0; i < kSteps; ++i) { const int v = proc.uiMotif[i].load() % 1000; if (proc.uiMotif[i].load() >= 0) { lo = juce::jmin (lo, v); hi = juce::jmax (hi, v); } }
    if (hi < lo) { lo = 28; hi = 52; }
    lo -= 2; hi += 2;
    const float rowH = ra.getHeight() / (float) juce::jmax (12, hi - lo + 1);
    const int stepNow = proc.uiStep.load();
    for (int i = 0; i < kSteps; ++i)
    {
        const int v = proc.uiMotif[i].load();
        if (v < 0) continue;
        const bool ghost = v >= 1000; const int p = v % 1000;
        int j = i + 1; while (j < kSteps && proc.uiMotif[j].load() < 0) ++j;
        const float w = juce::jmin ((float) (j - i), ghost ? 1.0f : 4.0f) * cw - 2;
        const float y = ra.getBottom() - (float) (p - lo + 1) * rowH;
        g.setColour ((i == stepNow ? col::cream : kTeal).withAlpha (ghost ? 0.4f : 1.0f));
        g.fillRoundedRectangle (ra.getX() + i * cw + 1, y, juce::jmax (3.0f, w), juce::jmax (4.0f, rowH - 1), 2.0f);
    }
    if (stepNow >= 0)
    {
        g.setColour (col::cream.withAlpha (0.35f));
        g.fillRect (ra.getX() + stepNow * cw, ra.getY(), 2.0f, ra.getHeight());
    }
    g.setColour (col::muted); g.setFont (font (10.0f, true));
    g.drawText ("BASS LINE - 2 BARS", rollArea.reduced (10, 4).removeFromTop (12), juce::Justification::topRight);

    // ---- player panel
    g.setColour (col::panel); g.fillRoundedRectangle (playerPanel.toFloat(), 8.0f);
    g.setColour (kTeal); g.setFont (font (13.0f, true));
    g.drawText ("THE PLAYER", playerPanel.reduced (16, 10).removeFromTop (18), juce::Justification::centredLeft);
    g.setColour (col::muted); g.setFont (font (11.0f, true));
    {
        const int key = (int) proc.apvts.getRawParameterValue ("key")->load();
        const char* names[] = { "VERSE: ", "CHORUS: ", "BRIDGE: " };
        for (int i = 0; i < 3; ++i)
        {
            const int pr = progBox[i]->getSelectedItemIndex();
            g.drawText (juce::String (names[i]) + progressionChords (pr, key), progBox[i]->getBounds().translated (0, -15).withHeight (14), juce::Justification::centredLeft);
        }
        g.drawText ("KEY", keyBox->getBounds().translated (0, -15).withHeight (14), juce::Justification::centredLeft);
        g.drawText ("BARS PER CHORD", chordBarsBox->getBounds().translated (0, -15).withHeight (14), juce::Justification::centredLeft);
        g.drawText ("SONG", songBox->getBounds().translated (0, -15).withHeight (14), juce::Justification::centredLeft);
        g.drawText ("SECTION LENGTH", secBarsBox->getBounds().translated (0, -15).withHeight (14), juce::Justification::centredLeft);
        auto selfHdr = keyBox->getBounds().translated (0, -36).withHeight (16).withWidth (500);
        g.setColour (kTeal); g.drawText ("WHEN IT PLAYS BY ITSELF", selfHdr, juce::Justification::centredLeft);
        g.setColour (col::muted);
    }
    for (auto* cb : { modeBox, followBox, feelBox, regBox, octBox, stopBox })
    {
        const char* t = cb == modeBox ? "MODE" : cb == followBox ? "CHORDS FROM" : cb == feelBox ? "FEEL"
                      : cb == regBox ? "REGISTER" : cb == octBox ? "OCTAVE" : "WHEN THE BAND STOPS";
        g.drawText (t, cb->getBounds().translated (0, -15).withHeight (14), juce::Justification::centredLeft);
    }

    // ---- rig panel
    g.setColour (col::panel); g.fillRoundedRectangle (rigPanel.toFloat(), 8.0f);
    g.setColour (kTeal); g.setFont (font (13.0f, true));
    g.drawText ("THE RIG", rigPanel.reduced (16, 10).removeFromTop (18), juce::Justification::centredLeft);
    g.setColour (col::muted); g.setFont (font (11.0f));
    g.drawText ("picked bass > comp > grit > chorus > flanger > amp", rigPanel.reduced (16, 10).removeFromTop (18), juce::Justification::centredRight);
    g.setColour (col::muted); g.setFont (font (11.0f, true));
    g.drawText ("GUITAR INPUT", guitarArea.withHeight (14), juce::Justification::centredLeft);

    g.setColour (col::muted); g.setFont (font (12.0f));
    g.drawFittedText ("Put Undertow on a MIDI track and press play - it plays a whole song by itself (pick the key and chords above). "
                      "Or set Chords From to Guitar and its Sidechain to your guitar track and it plays along with you, or feed it chords from a MIDI clip. Keyswitches (Live note names): C-1 verse, C#-1 chorus, D-1 bridge, D#-1 fill. "
                      "Map 'Footswitch: Fill' or Section to your MIDI Captain. Drag its MIDI out to another track to record the bass line as a clip.",
                      legendArea, juce::Justification::topLeft, 3, 1.0f);
}

void UndertowEditor::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (60);
    r.reduce (18, 14);
    legendArea = r.removeFromBottom (48);
    r.removeFromBottom (8);

    auto left = r.removeFromLeft ((int) (r.getWidth() * 0.62f));
    r.removeFromLeft (14);
    rigPanel = r;

    earArea = left.removeFromTop (190);
    rollArea = earArea.reduced (16, 10).withTrimmedLeft (270).withTrimmedTop (22);
    left.removeFromTop (12);
    playerPanel = left;

    auto pp = playerPanel.reduced (16, 10);
    pp.removeFromTop (24);
    auto row = pp.removeFromTop (36);
    const int sw = row.getWidth() / 5;
    for (auto& b : sectionButtons) b.setBounds (row.removeFromLeft (sw).reduced (3));
    fillButton.setBounds (row.removeFromLeft (sw).reduced (3));
    newLineButton.setBounds (row.reduced (3));
    pp.removeFromTop (20);
    auto combos = pp.removeFromTop (26);
    const int cw = combos.getWidth() / 3;
    modeBox->setBounds (combos.removeFromLeft (cw).reduced (3, 0));
    followBox->setBounds (combos.removeFromLeft (cw).reduced (3, 0));
    feelBox->setBounds (combos.reduced (3, 0));
    pp.removeFromTop (22);
    auto combos2 = pp.removeFromTop (26);
    regBox->setBounds (combos2.removeFromLeft (cw).reduced (3, 0));
    octBox->setBounds (combos2.removeFromLeft (cw).reduced (3, 0));
    stopBox->setBounds (combos2.reduced (3, 0));
    pp.removeFromTop (40);
    auto combos3 = pp.removeFromTop (26);
    const int qw = combos3.getWidth() / 4;
    keyBox->setBounds (combos3.removeFromLeft (qw).reduced (3, 0));
    chordBarsBox->setBounds (combos3.removeFromLeft (qw).reduced (3, 0));
    songBox->setBounds (combos3.removeFromLeft (qw).reduced (3, 0));
    secBarsBox->setBounds (combos3.reduced (3, 0));
    pp.removeFromTop (20);
    auto combos4 = pp.removeFromTop (26);
    for (int i = 0; i < 3; ++i) progBox[i]->setBounds ((i < 2 ? combos4.removeFromLeft (cw) : combos4).reduced (3, 0));
    pp.removeFromTop (10);
    auto knobs = pp.removeFromTop (juce::jmin (100, pp.getHeight()));
    const int kw = knobs.getWidth() / (int) playerKnobs.size();
    for (auto* k : playerKnobs) k->setBounds (knobs.removeFromLeft (kw));

    auto rp = rigPanel.reduced (12, 10);
    rp.removeFromTop (26);
    for (int rowI = 0; rowI < 3; ++rowI)
    {
        auto kr = rp.removeFromTop (88);
        const int w = kr.getWidth() / 4;
        for (int c = 0; c < 4; ++c) rigKnobs[(size_t) (rowI * 4 + c)]->setBounds (kr.removeFromLeft (w));
    }
    rp.removeFromTop (6);
    brightButton.setBounds (rp.removeFromTop (30).withWidth (rp.getWidth() / 2).reduced (3, 0));
    rp.removeFromTop (12);
    guitarArea = rp.removeFromTop (14 + 88);
    auto ga = guitarArea.withTrimmedTop (14);
    const int gw = ga.getWidth() / 4;
    guitarKnobs[0]->setBounds (ga.removeFromLeft (gw));
    guitarKnobs[1]->setBounds (ga.removeFromLeft (gw));
}
