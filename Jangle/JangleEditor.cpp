#include "JangleEditor.h"

using namespace jg;
using ttui::font;
namespace col = ttui::col;

namespace
{
const juce::Colour kCopper { 0xffd98a4e };
const char* kSectionNames[3] = { "VERSE", "CHORUS", "BRIDGE" };
const char* kPc[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
}

ttui::Knob* JangleEditor::knob (const char* id, const char* label, bool small)
{
    auto* k = new ttui::Knob (label, small, this);
    k->attach (proc.apvts, id);
    owned.add (k); addAndMakeVisible (k);
    return k;
}

juce::ComboBox* JangleEditor::combo (const char* id, const char* label)
{
    auto* c = new juce::ComboBox();
    if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id))) c->addItemList (ch->choices, 1);
    cAtt.add (new APVTS::ComboBoxAttachment (proc.apvts, id, *c));
    owned.add (c); addAndMakeVisible (c);
    if (label != nullptr) labelled.push_back ({ c, label });
    return c;
}

JangleEditor::JangleEditor (JangleProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    lnf.setAccent (kCopper);
    setLookAndFeel (&lnf);
    guitarBox = combo ("guitar", "GUITAR"); feelBox = combo ("feel", "FEEL"); followBox = combo ("follow", "CHORDS FROM");
    sitBox = combo ("sit_out", "WITH THE BAND"); posBox = combo ("position", "CHORD SHAPES"); leadBox = combo ("lead_fills", "LEAD ANSWERS");
    keyBox = combo ("key", "KEY"); chordBarsBox = combo ("chord_bars", "BARS PER CHORD"); songBox = combo ("song", "SONG");
    secBarsBox = combo ("section_bars", "SECTION LENGTH"); secBox = combo ("section", "SECTION");
    progBox[0] = combo ("prog_verse", "VERSE"); progBox[1] = combo ("prog_chorus", "CHORUS"); progBox[2] = combo ("prog_bridge", "BRIDGE");
    playerKnobs = { knob ("busy", "BUSY"), knob ("strum", "STRUM SPEED"), knob ("dynamics", "DYNAMICS"), knob ("human", "HUMAN"),
                    knob ("swing", "SWING"), knob ("length", "LET IT RING") };
    soundKnobs = { knob ("drive", "DRIVE", true), knob ("tone", "TONE", true), knob ("trem", "TREMOLO", true), knob ("chorus", "CHORUS", true),
                   knob ("room", "ROOM", true), knob ("width", "WIDTH", true), knob ("out", "OUTPUT", true) };
    dragMidi = std::make_unique<mc::DragButton> (proc.capture, "Jangle", kCopper);
    addAndMakeVisible (*dragMidi);
    setSize (1000, 690);
    startTimerHz (25);
}

JangleEditor::~JangleEditor() { setLookAndFeel (nullptr); }

void JangleEditor::timerCallback()
{
    auto v = [&] (const char* id) { return (int) proc.apvts.getRawParameterValue (id)->load(); };
    const bool band = v ("follow") == FBand;
    for (auto* cb : { keyBox, progBox[0], progBox[1], progBox[2], chordBarsBox, songBox, secBarsBox, secBox }) cb->setEnabled (band && ! proc.uiLinked.load());
    feelBox->setEnabled (v ("follow") != FPlay);
    juce::String st;
    for (auto* id : { "follow", "key", "prog_verse", "prog_chorus", "prog_bridge", "guitar", "feel", "sit_out", "position" }) st << v (id) << ",";
    st << (int) proc.uiLinked.load() << (int) proc.samplesLoaded();
    if (st != lastStatic) { lastStatic = st; repaint(); return; }
    int64_t dyn = proc.uiChord.load() * 131 + proc.uiSection.load() * 17 + (proc.uiTransport.load() ? 3 : 0) + proc.uiStep.load() * 7919 + proc.uiSteps.load();
    for (int i = 0; i < 6; ++i) dyn = dyn * 31 + proc.uiFret[i].load() + (int) (proc.uiString[i].load() * 8.0f) * 100;
    if (dyn != lastDyn) { lastDyn = dyn; repaint (nowArea.getUnion (neckArea)); }
}

void JangleEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    auto header = getLocalBounds().removeFromTop (60);
    g.setColour (col::panel); g.fillRect (header);
    g.setColour (kCopper); g.fillRect (header.removeFromBottom (2));
    g.setColour (col::cream); g.setFont (font (28.0f, true));
    g.drawText ("JANGLE", 20, 8, 300, 30, juce::Justification::centredLeft);
    g.setColour (kCopper); g.setFont (font (11.0f, true));
    g.drawText ("RHYTHM GUITAR", 22, 38, 300, 14, juce::Justification::centredLeft);
    if (proc.uiLinked.load()) { g.setColour (juce::Colour (0xff7fbf6a)); g.setFont (font (12.0f, true)); g.drawText ("FOLLOWING BANDLEADER", getWidth() - 420, 22, 200, 16, juce::Justification::centredRight); }

    // ---- now: the chord, the section, the beat
    g.setColour (col::panel); g.fillRoundedRectangle (nowArea.toFloat(), 8.0f);
    auto na = nowArea.reduced (18, 12);
    g.setColour (kCopper); g.setFont (font (13.0f, true));
    g.drawText ("NOW", na.removeFromTop (18), juce::Justification::centredLeft);
    const int ch = proc.uiChord.load();
    g.setColour (col::cream); g.setFont (font (46.0f, true));
    g.drawText (ch >= 0 && proc.uiTransport.load() ? juce::String (kPc[ch & 15]) + band::qualitySuffix (ch >> 4) : juce::String ("-"), na.removeFromTop (54), juce::Justification::centredLeft);
    g.setFont (font (14.0f, true)); g.setColour (col::muted);
    g.drawText (proc.uiTransport.load() ? juce::String (kSectionNames[juce::jlimit (0, 2, proc.uiSection.load())]) : juce::String ("Press play in Live"), na.removeFromTop (20), juce::Justification::centredLeft);
    {
        // the beat: one light per 16th, the beats brighter
        const int steps = proc.uiSteps.load(), cur = proc.uiStep.load();
        auto r = na.removeFromTop (22).withTrimmedTop (6);
        const float w = (float) r.getWidth() / 16.0f;
        for (int s = 0; s < steps; ++s)
        {
            const bool beat = steps == 12 ? s % 4 == 0 : s % 4 == 0;
            g.setColour (s == cur ? kCopper : (beat ? col::line.brighter (0.4f) : col::line));
            g.fillRoundedRectangle (juce::Rectangle<float> ((float) r.getX() + w * (float) s + 1.0f, (float) r.getY(), w - 2.0f, 8.0f), 2.0f);
        }
    }
    if (! proc.samplesLoaded()) { g.setColour (col::red); g.drawText ("Guitar samples missing", na.removeFromTop (18), juce::Justification::centredLeft); }

    // ---- the neck: six strings, the shape being played
    g.setColour (col::panel); g.fillRoundedRectangle (neckArea.toFloat(), 8.0f);
    {
        auto r = neckArea.reduced (28, 22).toFloat();
        int lo = 99, hi = 0;
        for (int i = 0; i < 6; ++i) { const int f = proc.uiFret[i].load(); if (f > 0) { lo = juce::jmin (lo, f); hi = juce::jmax (hi, f); } }
        const int first = lo == 99 || hi <= 5 ? 0 : juce::jmax (0, lo - 1);
        const int frets = 6;
        const float fw = r.getWidth() / (float) frets, sh = r.getHeight() / 5.0f;
        g.setColour (juce::Colour (0xff3a2a1e)); g.fillRoundedRectangle (r.expanded (0, 8), 4.0f);
        for (int f = 0; f <= frets; ++f)
        {
            g.setColour (f == 0 && first == 0 ? col::cream : col::line.brighter (0.3f));
            g.fillRect (r.getX() + fw * (float) f - (f == 0 ? 2.0f : 1.0f), r.getY() - 6, f == 0 && first == 0 ? 5.0f : 2.0f, r.getHeight() + 12);
        }
        for (int f : { 3, 5, 7, 9, 12 }) if (f > first && f <= first + frets)
        { g.setColour (col::line.brighter (0.15f)); g.fillEllipse (r.getX() + fw * ((float) (f - first) - 0.5f) - 5, r.getCentreY() - 5, 10, 10); }
        g.setColour (col::muted); g.setFont (font (11.0f));
        if (first > 0) g.drawText (juce::String (first + 1) + "fr", (int) r.getX() - 4, (int) r.getBottom() + 6, 60, 14, juce::Justification::centredLeft);
        for (int s = 0; s < 6; ++s)
        {
            const float y = r.getBottom() - sh * (float) s;      // low E at the bottom
            const float hit = proc.uiString[s].load();
            g.setColour (col::cream.withAlpha (0.45f + 0.5f * juce::jmin (1.0f, hit)));
            g.fillRect (r.getX(), y - (0.6f + (5 - s) * 0.25f), r.getWidth(), 1.2f + (5 - s) * 0.5f);
            const int f = proc.uiFret[s].load();
            if (f < 0) { g.setColour (col::muted); g.drawText ("x", (int) r.getX() - 22, (int) y - 8, 14, 16, juce::Justification::centred); }
            else if (f == 0) { g.setColour (col::cream); g.drawEllipse (r.getX() - 20, y - 6, 12, 12, 1.5f); }
            else
            {
                const float x = r.getX() + fw * ((float) (f - first) - 0.5f);
                g.setColour (kCopper.withMultipliedBrightness (0.8f + 0.4f * juce::jmin (1.0f, hit))); g.fillEllipse (x - 9, y - 9, 18, 18);
            }
        }
    }

    // ---- panels
    for (auto* r : { &playerPanel, &soundPanel, &songPanel }) { g.setColour (col::panel); g.fillRoundedRectangle (r->toFloat(), 8.0f); }
    g.setColour (kCopper); g.setFont (font (13.0f, true));
    g.drawText ("THE PLAYER", playerPanel.reduced (18, 10).removeFromTop (18), juce::Justification::centredLeft);
    g.drawText ("THE SOUND", soundPanel.reduced (18, 10).removeFromTop (18), juce::Justification::centredLeft);
    g.drawText (proc.uiLinked.load() ? "THE SONG (Bandleader runs it)" : "THE SONG (on its own)", songPanel.reduced (18, 10).removeFromTop (18), juce::Justification::centredLeft);
    g.setFont (font (11.0f, true)); g.setColour (col::muted);
    for (auto& [cb, label] : labelled) g.drawText (label, cb->getBounds().translated (0, -15).withHeight (14), juce::Justification::centredLeft);
    g.setFont (font (12.0f));
    g.drawFittedText ("With a Bandleader in the set, Jangle strums its chords and follows the song: count-in, intro, breakdowns, the ending. "
                      "On its own it plays its own song, follows chords held on a MIDI keyboard, or you play it from the keys.",
                      legendArea, juce::Justification::topLeft, 2);
}

void JangleEditor::resized()
{
    auto r = getLocalBounds();
    dragMidi->setBounds (getWidth() - 196, 14, 178, 32);
    r.removeFromTop (60);
    r.reduce (18, 14);
    legendArea = r.removeFromBottom (32);
    r.removeFromBottom (6);
    auto top = r.removeFromTop (190);
    nowArea = top.removeFromLeft (300);
    top.removeFromLeft (14);
    neckArea = top;
    r.removeFromTop (12);
    auto left = r.removeFromLeft (r.getWidth() * 55 / 100);
    r.removeFromLeft (12);
    playerPanel = left;
    auto right = r;
    soundPanel = right.removeFromTop (right.getHeight() / 2 - 6);
    right.removeFromTop (12);
    songPanel = right;

    auto pa = playerPanel.reduced (18, 10); pa.removeFromTop (38);
    auto row = pa.removeFromTop (26);
    const int w3 = row.getWidth() / 3;
    guitarBox->setBounds (row.removeFromLeft (w3).reduced (4, 0)); feelBox->setBounds (row.removeFromLeft (w3).reduced (4, 0)); posBox->setBounds (row.reduced (4, 0));
    pa.removeFromTop (22);
    row = pa.removeFromTop (26);
    followBox->setBounds (row.removeFromLeft (w3).reduced (4, 0)); leadBox->setBounds (row.removeFromLeft (w3).reduced (4, 0)); sitBox->setBounds (row.reduced (4, 0));
    pa.removeFromTop (10);
    const int kw = pa.getWidth() / 6;
    for (auto* k : playerKnobs) k->setBounds (pa.removeFromLeft (kw).withHeight (juce::jmin (pa.getHeight(), 110)));

    auto sa = soundPanel.reduced (18, 10); sa.removeFromTop (22);
    const int sw = sa.getWidth() / 7;
    for (auto* k : soundKnobs) k->setBounds (sa.removeFromLeft (sw).withHeight (juce::jmin (sa.getHeight(), 90)));

    auto so = songPanel.reduced (18, 10); so.removeFromTop (38);
    row = so.removeFromTop (24);
    const int w4 = row.getWidth() / 4;
    keyBox->setBounds (row.removeFromLeft (w4).reduced (3, 0)); chordBarsBox->setBounds (row.removeFromLeft (w4).reduced (3, 0));
    songBox->setBounds (row.removeFromLeft (w4).reduced (3, 0)); secBarsBox->setBounds (row.reduced (3, 0));
    so.removeFromTop (20);
    row = so.removeFromTop (24);
    for (int i = 0; i < 3; ++i) progBox[i]->setBounds (row.removeFromLeft (w4).reduced (3, 0));
    secBox->setBounds (row.reduced (3, 0));
}
