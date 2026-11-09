#include "EtherEditor.h"

using namespace et;
using ttui::font;
namespace col = ttui::col;

namespace
{
const juce::Colour kGlow { 0xff7fe0a8 };     // valve-green
const char* kPc[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
juce::String noteName (int n) { return n < 0 ? juce::String ("--") : juce::String (kPc[n % 12]) + juce::String (n / 12 - 1); }
}

ttui::Knob* EtherEditor::knob (const char* id, const char* label, bool small)
{
    auto* k = new ttui::Knob (label, small, this);
    k->attach (proc.apvts, id);
    owned.add (k); addAndMakeVisible (k);
    return k;
}

juce::ComboBox* EtherEditor::combo (const char* id, const char* label)
{
    auto* c = new juce::ComboBox();
    if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id))) c->addItemList (ch->choices, 1);
    cAtt.add (new APVTS::ComboBoxAttachment (proc.apvts, id, *c));
    owned.add (c); addAndMakeVisible (c);
    if (label != nullptr) labelled.push_back ({ c, label });
    return c;
}

EtherEditor::EtherEditor (EtherProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    lnf.setAccent (kGlow);
    setLookAndFeel (&lnf);
    instBox = combo ("instrument", "INSTRUMENT"); roleBox = combo ("role", "PLAYS"); followBox = combo ("follow", "NOTES FROM");
    sitBox = combo ("sit_out", "WITH THE BAND"); regBox = combo ("register", "REGISTER");
    keyBox = combo ("key", "KEY"); chordBarsBox = combo ("chord_bars", "BARS PER CHORD"); songBox = combo ("song", "SONG"); secBarsBox = combo ("section_bars", "SECTION LENGTH");
    progBox[0] = combo ("prog_verse", nullptr); progBox[1] = combo ("prog_chorus", nullptr); progBox[2] = combo ("prog_bridge", nullptr);
    playerKnobs = { knob ("busy", "BUSY"), knob ("wander", "WANDER"), knob ("repeat", "REPEAT"), knob ("glide", "GLIDE"), knob ("vibrato", "VIBRATO") };
    soundKnobs = { knob ("echo", "TAPE ECHO", true), knob ("space", "SPACE", true), knob ("age", "AGE", true), knob ("out", "OUTPUT", true) };
    setSize (980, 640);
    startTimerHz (30);
}

EtherEditor::~EtherEditor() { setLookAndFeel (nullptr); }

void EtherEditor::timerCallback()
{
    const bool band = proc.apvts.getRawParameterValue ("follow")->load() < 0.5f;
    for (auto* cb : { keyBox, progBox[0], progBox[1], progBox[2], chordBarsBox, songBox, secBarsBox }) cb->setEnabled (band && ! proc.uiLinked.load());
    roleBox->setEnabled (band);
    juce::String st;
    for (auto* id : { "follow", "key", "prog_verse", "prog_chorus", "prog_bridge", "instrument", "role", "sit_out" }) st << (int) proc.apvts.getRawParameterValue (id)->load() << ",";
    st << (int) proc.uiLinked.load();
    if (st != lastStatic) { lastStatic = st; repaint(); }
    levelShown = juce::jmax (proc.uiLevel.load(), levelShown * 0.9f);
    phase += 0.08f;
    repaint (scopeArea);
}

void EtherEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    auto header = getLocalBounds().removeFromTop (60);
    g.setColour (col::panel); g.fillRect (header);
    g.setColour (kGlow); g.fillRect (header.removeFromBottom (2));
    g.setColour (col::cream); g.setFont (font (28.0f, true));
    g.drawText ("ETHER", 20, 8, 300, 30, juce::Justification::centredLeft);
    g.setColour (kGlow); g.setFont (font (11.0f, true));
    g.drawText ("EARLY ELECTRONIC INSTRUMENTS", 22, 38, 300, 14, juce::Justification::centredLeft);
    const int inst = (int) proc.apvts.getRawParameterValue ("instrument")->load();
    g.setColour (col::muted); g.setFont (font (12.0f, true));
    g.drawText (juce::String (instrumentName (inst)).toUpperCase() + "  -  " + instrumentBlurb (inst), header.withTrimmedRight (20), juce::Justification::centredRight);

    // ---- the scope: a glowing trace whose pitch follows the note, plus the trail of recent notes
    g.setColour (juce::Colour (0xff0d1410)); g.fillRoundedRectangle (scopeArea.toFloat(), 8.0f);
    auto sa = scopeArea.reduced (14, 12).toFloat();
    g.setColour (kGlow.withAlpha (0.08f));
    for (int i = 1; i < 8; ++i) g.drawHorizontalLine ((int) (sa.getY() + sa.getHeight() * i / 8), sa.getX(), sa.getRight());
    for (int i = 1; i < 16; ++i) g.drawVerticalLine ((int) (sa.getX() + sa.getWidth() * i / 16), sa.getY(), sa.getBottom());
    const int note = proc.uiNote.load();
    const float amp = juce::jlimit (0.0f, 1.0f, levelShown * 2.2f);
    juce::Path wave;
    const float cycles = note >= 0 ? 1.5f + (float) (note - 36) / 12.0f : 1.0f;
    for (int x = 0; x <= (int) sa.getWidth(); x += 2)
    {
        const float t = (float) x / sa.getWidth();
        float s = std::sin (t * cycles * juce::MathConstants<float>::twoPi + phase);
        if (inst == IStylus || inst == IReed) s = s > 0 ? 0.8f : -0.8f;
        else if (inst == IWire) s = std::fmod (t * cycles + phase * 0.16f, 1.0f) * 2 - 1;
        const float y = sa.getCentreY() - s * amp * sa.getHeight() * 0.32f;
        if (x == 0) wave.startNewSubPath (sa.getX() + (float) x, y); else wave.lineTo (sa.getX() + (float) x, y);
    }
    g.setColour (kGlow.withAlpha (0.25f)); g.strokePath (wave, juce::PathStrokeType (6.0f));
    g.setColour (kGlow); g.strokePath (wave, juce::PathStrokeType (1.8f));
    g.setColour (col::cream); g.setFont (font (34.0f, true));
    g.drawText (noteName (note), sa.removeFromTop (40).removeFromLeft (160), juce::Justification::centredLeft);
    g.setColour (kGlow.withAlpha (0.7f)); g.setFont (font (11.0f, true));
    const int follow = (int) proc.apvts.getRawParameterValue ("follow")->load();
    juce::String st = follow == FPlay ? "PLAY IT FROM YOUR KEYS (it glides between held notes)"
                    : proc.apvts.getRawParameterValue ("sit_out")->load() > 0.5f ? "SITTING OUT THIS STYLE"
                    : ! proc.uiTransport.load() ? "PRESS PLAY IN LIVE"
                    : proc.uiLinked.load() ? "FOLLOWING BANDLEADER" : "PLAYING ITS OWN SONG";
    g.drawText (st, scopeArea.reduced (14, 12).removeFromBottom (14), juce::Justification::centredLeft);

    g.setColour (col::panel); g.fillRoundedRectangle (playerPanel.toFloat(), 8.0f);
    g.setColour (kGlow); g.setFont (font (13.0f, true));
    g.drawText ("THE PLAYER", playerPanel.reduced (16, 10).removeFromTop (18), juce::Justification::centredLeft);
    g.setColour (col::panel); g.fillRoundedRectangle (soundPanel.toFloat(), 8.0f);
    g.setColour (kGlow);
    g.drawText ("THE ROOM", soundPanel.reduced (16, 10).removeFromTop (18), juce::Justification::centredLeft);
    g.setColour (col::muted); g.setFont (font (11.0f, true));
    for (auto& [cb, t] : labelled) g.drawText (t, cb->getBounds().translated (0, -15).withHeight (14), juce::Justification::centredLeft);
    const int key = (int) proc.apvts.getRawParameterValue ("key")->load();
    const char* names[] = { "VERSE: ", "CHORUS: ", "BRIDGE: " };
    for (int i = 0; i < 3; ++i)
        g.drawText (juce::String (names[i]) + ut::progressionChords (progBox[i]->getSelectedItemIndex(), key), progBox[i]->getBounds().translated (0, -15).withHeight (14), juce::Justification::centredLeft);
    g.setColour (kGlow);
    g.drawText (proc.uiLinked.load() ? "SONG: FOLLOWING BANDLEADER" : "WHEN IT PLAYS BY ITSELF", keyBox->getBounds().translated (0, -36).withHeight (16).withWidth (500), juce::Justification::centredLeft);

    g.setColour (col::muted); g.setFont (font (12.0f));
    g.drawFittedText ("Put Ether on a MIDI track and press play: it writes melodies over the Bandleader's chords, holds a drone, or adds swoops and bloops. "
                      "REPEAT keeps a tune coming back (hooky); WANDER lets it leap. Set Notes From to Play it and it's yours: hold a key, press another, and it glides.",
                      legendArea, juce::Justification::topLeft, 3, 1.0f);
}

void EtherEditor::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (60);
    r.reduce (18, 14);
    legendArea = r.removeFromBottom (48);
    r.removeFromBottom (8);
    scopeArea = r.removeFromTop (150);
    r.removeFromTop (12);
    playerPanel = r.removeFromLeft ((int) (r.getWidth() * 0.68f));
    r.removeFromLeft (14);
    soundPanel = r;

    auto pp = playerPanel.reduced (16, 10);
    pp.removeFromTop (40);
    auto row1 = pp.removeFromTop (26);
    const int w5 = row1.getWidth() / 5;
    for (auto* cb : { instBox, roleBox, followBox, regBox }) cb->setBounds (row1.removeFromLeft (w5).reduced (3, 0));
    sitBox->setBounds (row1.reduced (3, 0));
    pp.removeFromTop (42);
    auto row3 = pp.removeFromTop (26);
    const int qw = row3.getWidth() / 4;
    keyBox->setBounds (row3.removeFromLeft (qw).reduced (3, 0));
    chordBarsBox->setBounds (row3.removeFromLeft (qw).reduced (3, 0));
    songBox->setBounds (row3.removeFromLeft (qw).reduced (3, 0));
    secBarsBox->setBounds (row3.reduced (3, 0));
    pp.removeFromTop (20);
    auto row4 = pp.removeFromTop (26);
    const int cw = row4.getWidth() / 3;
    for (int i = 0; i < 3; ++i) progBox[i]->setBounds ((i < 2 ? row4.removeFromLeft (cw) : row4).reduced (3, 0));
    pp.removeFromTop (10);
    const int kw = pp.getWidth() / (int) playerKnobs.size();
    for (auto* k : playerKnobs) k->setBounds (pp.removeFromLeft (kw));

    auto sp = soundPanel.reduced (12, 10);
    sp.removeFromTop (28);
    for (int rowI = 0; rowI < 2; ++rowI)
    {
        auto kr = sp.removeFromTop (100);
        const int w = kr.getWidth() / 2;
        for (int c = 0; c < 2; ++c) soundKnobs[(size_t) (rowI * 2 + c)]->setBounds (kr.removeFromLeft (w));
    }
}
