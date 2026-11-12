#include "ParlourEditor.h"

using namespace pk;
using ttui::font;
namespace col = ttui::col;

namespace
{
const juce::Colour kPlum { 0xffc583d6 };
const char* kSectionNames[3] = { "VERSE", "CHORUS", "BRIDGE" };
const char* kPc[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
}

ttui::Knob* ParlourEditor::knob (const char* id, const char* label, bool small)
{
    auto* k = new ttui::Knob (label, small, this);
    k->attach (proc.apvts, id);
    owned.add (k); addAndMakeVisible (k);
    return k;
}

juce::ComboBox* ParlourEditor::combo (const char* id, const char* label)
{
    auto* c = new juce::ComboBox();
    if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id))) c->addItemList (ch->choices, 1);
    cAtt.add (new APVTS::ComboBoxAttachment (proc.apvts, id, *c));
    owned.add (c); addAndMakeVisible (c);
    if (label != nullptr) labelled.push_back ({ c, label });
    return c;
}

ParlourEditor::ParlourEditor (ParlourProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    lnf.setAccent (kPlum);
    setLookAndFeel (&lnf);
    for (int i = 0; i < 3; ++i)
    {
        sectionButtons[i].setButtonText (kSectionNames[i]);
        sectionButtons[i].onClick = [this, i]
        {
            if (auto* prm = proc.apvts.getParameter ("section")) { prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i)); prm->endChangeGesture(); }
            if (auto* sg = proc.apvts.getParameter ("song")) { sg->beginChangeGesture(); sg->setValueNotifyingHost (1.0f); sg->endChangeGesture(); }
        };
        addAndMakeVisible (sectionButtons[i]);
    }
    instBox = combo ("instrument", "INSTRUMENT"); feelBox = combo ("feel", "FEEL"); followBox = combo ("follow", "CHORDS FROM");
    sitBox = combo ("sit_out", "WITH THE BAND"); regBox = combo ("register", "REGISTER"); lhBox = combo ("left_hand", "LEFT HAND");
    stopBox = combo ("band_stop", "WHEN THE BAND STOPS");
    keyBox = combo ("key", "KEY"); chordBarsBox = combo ("chord_bars", "BARS PER CHORD"); songBox = combo ("song", "SONG"); secBarsBox = combo ("section_bars", "SECTION LENGTH");
    progBox[0] = combo ("prog_verse", nullptr); progBox[1] = combo ("prog_chorus", nullptr); progBox[2] = combo ("prog_bridge", nullptr);
    rotorBox = combo ("rotor", "ROTARY");
    playerKnobs = { knob ("busy", "BUSY"), knob ("colour", "COLOUR"), knob ("dynamics", "DYNAMICS"), knob ("human", "HUMAN"), knob ("swing", "SWING"), knob ("length", "LENGTH") };
    soundKnobs = { knob ("motion", "MOTION", true), knob ("chorus", "CHORUS", true), knob ("lofi", "LO-FI", true),
                   knob ("tone", "TONE", true), knob ("room", "ROOM", true), knob ("out", "OUTPUT", true) };
    dragMidi = std::make_unique<mc::DragButton> (proc.capture, "Parlour", juce::Colour (0xffc583d6));
    addAndMakeVisible (*dragMidi);
    setSize (1040, 700);
    startTimerHz (25);
}

ParlourEditor::~ParlourEditor() { setLookAndFeel (nullptr); }

void ParlourEditor::timerCallback()
{
    auto v = [&] (const char* id) { return (int) proc.apvts.getRawParameterValue (id)->load(); };
    const bool band = v ("follow") == FBand;
    const bool selfAuto = band && v ("song") == 0;
    const int sec = selfAuto && proc.uiTransport.load() ? proc.uiSection.load() : v ("section");
    for (int i = 0; i < 3; ++i) { sectionButtons[i].setToggleState (i == sec, juce::dontSendNotification); sectionButtons[i].setEnabled (band); }
    for (auto* cb : { keyBox, progBox[0], progBox[1], progBox[2], chordBarsBox, songBox, secBarsBox }) cb->setEnabled (band && ! proc.uiLinked.load());
    feelBox->setEnabled (v ("follow") != FPlay);
    rotorBox->setEnabled (v ("instrument") == IOrgan);

    juce::String st;
    for (auto* id : { "follow", "key", "prog_verse", "prog_chorus", "prog_bridge", "instrument", "feel", "sit_out" }) st << v (id) << ",";
    st << (int) proc.uiLinked.load() << (int) proc.samplesLoaded();
    if (st != lastStatic) { lastStatic = st; repaint(); return; }
    int64_t dyn = proc.uiChord.load() * 131 + proc.uiSection.load() * 17 + (proc.uiTransport.load() ? 3 : 0) + proc.uiStep.load() * 7919;
    for (int i = 0; i < 4; ++i) dyn = dyn * 1000003 + proc.uiNotes[i].load();
    if (dyn != lastDyn) { lastDyn = dyn; repaint (nowArea.getUnion (keysArea)); }
}

void ParlourEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    auto header = getLocalBounds().removeFromTop (60);
    g.setColour (col::panel); g.fillRect (header);
    g.setColour (kPlum); g.fillRect (header.removeFromBottom (2));
    g.setColour (col::cream); g.setFont (font (28.0f, true));
    g.drawText ("PARLOUR", 20, 8, 300, 30, juce::Justification::centredLeft);
    g.setColour (kPlum); g.setFont (font (11.0f, true));
    g.drawText ("KEYS PLAYER", 22, 38, 300, 14, juce::Justification::centredLeft);
    if (! proc.samplesLoaded()) { g.setColour (col::red); g.drawText ("samples missing", 320, 20, 200, 20, juce::Justification::centredLeft); }
    const int follow = (int) proc.apvts.getRawParameterValue ("follow")->load();
    const int inst = (int) proc.apvts.getRawParameterValue ("instrument")->load();
    g.setColour (col::muted); g.setFont (font (12.0f, true));
    g.drawText (juce::String (instrumentName (inst)).toUpperCase() + (usesSamples (inst) ? "  -  REAL SAMPLES" : "  -  MODELLED"), header.withTrimmedRight (214), juce::Justification::centredRight);

    // ---- what it's playing
    g.setColour (col::panel); g.fillRoundedRectangle (nowArea.toFloat(), 8.0f);
    auto na = nowArea.reduced (16, 10);
    g.setColour (kPlum); g.setFont (font (13.0f, true));
    g.drawText ("NOW PLAYING", na.removeFromTop (20), juce::Justification::centredLeft);
    const int ch = proc.uiChord.load();
    g.setColour (col::muted); g.setFont (font (11.0f, true));
    g.drawText (follow == FPlay ? "YOU'RE PLAYING" : follow == FMidiChords ? "CHORD (from MIDI)" : proc.uiLinked.load() ? "CHORD (from Bandleader)" : "CHORD (its own song)",
                na.removeFromTop (16), juce::Justification::centredLeft);
    g.setColour (ch >= 0 ? col::cream : col::muted); g.setFont (font (46.0f, true));
    g.drawText (ch >= 0 ? juce::String (kPc[ch & 15]) + band::qualitySuffix (ch >> 4) : juce::String ("--"), na.removeFromTop (54), juce::Justification::centredLeft);
    g.setColour (col::muted); g.setFont (font (12.0f));
    juce::String status;
    if (follow == FPlay) status = "Play it from your KeyStep or a MIDI clip (sustain pedal works).";
    else if (proc.apvts.getRawParameterValue ("sit_out")->load() > 0.5f) status = "Sitting out this style. Set WITH THE BAND to Plays along.";
    else if (! proc.uiTransport.load()) status = "Press play in Live - it comps along to the tempo.";
    else status = juce::String (feelName ((int) proc.apvts.getRawParameterValue ("feel")->load())) + "  -  " + kSectionNames[juce::jlimit (0, 2, proc.uiSection.load())];
    g.drawFittedText (status, na.removeFromTop (34), juce::Justification::topLeft, 2, 1.0f);

    // ---- the keyboard: C2 to C7
    g.setColour (col::bg.brighter (0.03f)); g.fillRoundedRectangle (keysArea.toFloat(), 6.0f);
    auto ka = keysArea.reduced (10, 12).toFloat();
    const int lo = 36, hi = 96;
    int whites = 0; for (int n = lo; n <= hi; ++n) { const int pc = n % 12; if (! (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10)) ++whites; }
    const float ww = ka.getWidth() / (float) whites;
    auto on = [&] (int n) { return (proc.uiNotes[n >> 5].load() >> (n & 31)) & 1u; };
    int wi = 0;
    for (int n = lo; n <= hi; ++n)
    {
        const int pc = n % 12; if (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10) continue;
        juce::Rectangle<float> k (ka.getX() + wi * ww, ka.getY(), ww - 1.5f, ka.getHeight());
        g.setColour (on (n) ? kPlum : col::cream.withAlpha (0.9f)); g.fillRoundedRectangle (k, 2.0f);
        if (pc == 0) { g.setColour (col::muted); g.setFont (font (9.0f)); g.drawText ("C" + juce::String (n / 12 - 1), k.removeFromBottom (14), juce::Justification::centred); }
        ++wi;
    }
    wi = 0;
    for (int n = lo; n <= hi; ++n)
    {
        const int pc = n % 12;
        const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        if (! black) { ++wi; continue; }
        juce::Rectangle<float> k (ka.getX() + wi * ww - ww * 0.32f, ka.getY(), ww * 0.62f, ka.getHeight() * 0.6f);
        g.setColour (on (n) ? kPlum.brighter (0.2f) : col::bg); g.fillRoundedRectangle (k, 2.0f);
    }

    // ---- panels
    g.setColour (col::panel); g.fillRoundedRectangle (playerPanel.toFloat(), 8.0f);
    g.setColour (kPlum); g.setFont (font (13.0f, true));
    g.drawText ("THE PLAYER", playerPanel.reduced (16, 10).removeFromTop (18), juce::Justification::centredLeft);
    g.setColour (col::panel); g.fillRoundedRectangle (soundPanel.toFloat(), 8.0f);
    g.setColour (kPlum);
    g.drawText ("THE SOUND", soundPanel.reduced (16, 10).removeFromTop (18), juce::Justification::centredLeft);
    g.setColour (col::muted); g.setFont (font (11.0f, true));
    for (auto& [cb, t] : labelled) g.drawText (t, cb->getBounds().translated (0, -15).withHeight (14), juce::Justification::centredLeft);
    const int key = (int) proc.apvts.getRawParameterValue ("key")->load();
    const char* names[] = { "VERSE: ", "CHORUS: ", "BRIDGE: " };
    for (int i = 0; i < 3; ++i)
        g.drawText (juce::String (names[i]) + ut::progressionChords (progBox[i]->getSelectedItemIndex(), key), progBox[i]->getBounds().translated (0, -15).withHeight (14), juce::Justification::centredLeft);
    g.setColour (kPlum);
    g.drawText (proc.uiLinked.load() ? "SONG: FOLLOWING BANDLEADER" : "WHEN IT PLAYS BY ITSELF", keyBox->getBounds().translated (0, -36).withHeight (16).withWidth (500), juce::Justification::centredLeft);

    g.setColour (col::muted); g.setFont (font (12.0f));
    g.drawFittedText ("Put Parlour on a MIDI track and press play: it comps the Bandleader's chords (or its own song) in the feel you pick. "
                      "Set Chords From to MIDI chords to follow chords you hold on your KeyStep, or Play it to play it yourself. "
                      "MOTION is tremolo on the electric pianos and vibes, the rotating speaker on the organ. Leave LEFT HAND off when Undertow is playing bass.",
                      legendArea, juce::Justification::topLeft, 3, 1.0f);
}

void ParlourEditor::resized()
{
    auto r = getLocalBounds();
    dragMidi->setBounds (getWidth() - 196, 14, 178, 32);
    r.removeFromTop (60);
    r.reduce (18, 14);
    legendArea = r.removeFromBottom (48);
    r.removeFromBottom (8);
    auto top = r.removeFromTop (170);
    nowArea = top.removeFromLeft (300);
    top.removeFromLeft (12);
    keysArea = top;
    r.removeFromTop (12);
    playerPanel = r.removeFromLeft ((int) (r.getWidth() * 0.64f));
    r.removeFromLeft (14);
    soundPanel = r;

    auto pp = playerPanel.reduced (16, 10);
    pp.removeFromTop (40);
    auto row1 = pp.removeFromTop (26);
    const int cw = row1.getWidth() / 3;
    instBox->setBounds (row1.removeFromLeft (cw).reduced (3, 0));
    feelBox->setBounds (row1.removeFromLeft (cw).reduced (3, 0));
    followBox->setBounds (row1.reduced (3, 0));
    pp.removeFromTop (22);
    auto row2 = pp.removeFromTop (26);
    const int qw = row2.getWidth() / 4;
    regBox->setBounds (row2.removeFromLeft (qw).reduced (3, 0));
    lhBox->setBounds (row2.removeFromLeft (qw).reduced (3, 0));
    sitBox->setBounds (row2.removeFromLeft (qw).reduced (3, 0));
    stopBox->setBounds (row2.reduced (3, 0));
    pp.removeFromTop (42);
    auto row3 = pp.removeFromTop (26);
    keyBox->setBounds (row3.removeFromLeft (qw).reduced (3, 0));
    chordBarsBox->setBounds (row3.removeFromLeft (qw).reduced (3, 0));
    songBox->setBounds (row3.removeFromLeft (qw).reduced (3, 0));
    secBarsBox->setBounds (row3.reduced (3, 0));
    pp.removeFromTop (20);
    auto row4 = pp.removeFromTop (26);
    for (int i = 0; i < 3; ++i) progBox[i]->setBounds ((i < 2 ? row4.removeFromLeft (cw) : row4).reduced (3, 0));
    pp.removeFromTop (8);
    auto row5 = pp.removeFromTop (30);
    const int bw = row5.getWidth() / 3;
    for (auto& b : sectionButtons) b.setBounds (row5.removeFromLeft (bw).reduced (3, 0));
    pp.removeFromTop (6);
    auto knobs = pp;
    const int kw = knobs.getWidth() / (int) playerKnobs.size();
    for (auto* k : playerKnobs) k->setBounds (knobs.removeFromLeft (kw));

    auto sp = soundPanel.reduced (12, 10);
    sp.removeFromTop (28);
    for (int rowI = 0; rowI < 2; ++rowI)
    {
        auto kr = sp.removeFromTop (92);
        const int w = kr.getWidth() / 3;
        for (int c = 0; c < 3; ++c) soundKnobs[(size_t) (rowI * 3 + c)]->setBounds (kr.removeFromLeft (w));
    }
    sp.removeFromTop (24);
    rotorBox->setBounds (sp.removeFromTop (26).withWidth (sp.getWidth() / 2).reduced (3, 0));
}
