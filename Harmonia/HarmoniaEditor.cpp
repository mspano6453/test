#include "HarmoniaEditor.h"

using namespace hm;
using ttui::font;
namespace col = ttui::col;

namespace
{
const juce::Colour kRose { 0xffe48aa6 };
const char* kSectionNames[3] = { "VERSE", "CHORUS", "BRIDGE" };
const char* kPc[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
const char* kVowel[NumVowels] = { "OOH", "OH", "AAH", "EE", "MMM" };
const char* kPart[4] = { "TOP", "2ND", "3RD", "LOW" };
}

ttui::Knob* HarmoniaEditor::knob (const char* id, const char* label, bool small)
{
    auto* k = new ttui::Knob (label, small, this);
    k->attach (proc.apvts, id);
    owned.add (k); addAndMakeVisible (k);
    return k;
}

juce::ComboBox* HarmoniaEditor::combo (const char* id, const char* label)
{
    auto* c = new juce::ComboBox();
    if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id))) c->addItemList (ch->choices, 1);
    cAtt.add (new APVTS::ComboBoxAttachment (proc.apvts, id, *c));
    owned.add (c); addAndMakeVisible (c);
    if (label != nullptr) labelled.push_back ({ c, label });
    return c;
}

HarmoniaEditor::HarmoniaEditor (HarmoniaProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    lnf.setAccent (kRose);
    setLookAndFeel (&lnf);
    groupBox = combo ("group", "SINGERS"); feelBox = combo ("feel", "FEEL"); partsBox = combo ("parts", "PARTS"); partBox = combo ("part", "WHAT THEY SING");
    followBox = combo ("follow", "CHORDS FROM"); sitBox = combo ("sit_out", "WITH THE BAND"); regBox = combo ("register", "REGISTER");
    keyBox = combo ("key", "KEY"); chordBarsBox = combo ("chord_bars", "BARS PER CHORD"); songBox = combo ("song", "SONG");
    secBarsBox = combo ("section_bars", "SECTION LENGTH"); secBox = combo ("section", "SECTION");
    progBox[0] = combo ("prog_verse", "VERSE"); progBox[1] = combo ("prog_chorus", "CHORUS"); progBox[2] = combo ("prog_bridge", "BRIDGE");
    singKnobs = { knob ("busy", "BUSY"), knob ("dynamics", "DYNAMICS"), knob ("human", "HUMAN"), knob ("vibrato", "VIBRATO"),
                  knob ("ensemble", "ENSEMBLE"), knob ("swell", "SWELL") };
    soundKnobs = { knob ("breath", "BREATH", true), knob ("bright", "BRIGHT", true), knob ("room", "ROOM", true),
                   knob ("width", "WIDTH", true), knob ("out", "OUTPUT", true) };
    dragMidi = std::make_unique<mc::DragButton> (proc.capture, "Harmonia", kRose);
    addAndMakeVisible (*dragMidi);
    setSize (1000, 690);
    startTimerHz (25);
}

HarmoniaEditor::~HarmoniaEditor() { setLookAndFeel (nullptr); }

void HarmoniaEditor::timerCallback()
{
    auto v = [&] (const char* id) { return (int) proc.apvts.getRawParameterValue (id)->load(); };
    const bool band = v ("follow") == FBand;
    for (auto* cb : { keyBox, progBox[0], progBox[1], progBox[2], chordBarsBox, songBox, secBarsBox, secBox }) cb->setEnabled (band && ! proc.uiLinked.load());
    juce::String st;
    for (auto* id : { "follow", "group", "feel", "parts", "sit_out" }) st << v (id) << ",";
    st << (int) proc.uiLinked.load();
    if (st != lastStatic) { lastStatic = st; repaint(); return; }
    int64_t dyn = proc.uiChord.load() * 131 + proc.uiSection.load() * 17 + (proc.uiTransport.load() ? 3 : 0) + proc.uiStep.load() * 7919 + proc.uiVowel.load();
    for (int i = 0; i < 4; ++i) dyn = dyn * 131 + proc.uiPartNote[i].load();
    dyn = dyn * 7 + (int) (proc.uiLevel.load() * 20.0f);
    if (dyn != lastDyn) { lastDyn = dyn; repaint (nowArea.getUnion (stackArea)); }
}

void HarmoniaEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    auto header = getLocalBounds().removeFromTop (60);
    g.setColour (col::panel); g.fillRect (header);
    g.setColour (kRose); g.fillRect (header.removeFromBottom (2));
    g.setColour (col::cream); g.setFont (font (28.0f, true));
    g.drawText ("HARMONIA", 20, 8, 300, 30, juce::Justification::centredLeft);
    g.setColour (kRose); g.setFont (font (11.0f, true));
    g.drawText ("BACKING VOCALS", 22, 38, 300, 14, juce::Justification::centredLeft);
    if (proc.uiLinked.load()) { g.setColour (juce::Colour (0xff7fbf6a)); g.setFont (font (12.0f, true)); g.drawText ("FOLLOWING BANDLEADER", getWidth() - 420, 22, 200, 16, juce::Justification::centredRight); }

    g.setColour (col::panel); g.fillRoundedRectangle (nowArea.toFloat(), 8.0f);
    auto na = nowArea.reduced (18, 12);
    g.setColour (kRose); g.setFont (font (13.0f, true));
    g.drawText ("NOW", na.removeFromTop (18), juce::Justification::centredLeft);
    const int ch = proc.uiChord.load();
    const bool on = proc.uiTransport.load();
    g.setColour (col::cream); g.setFont (font (46.0f, true));
    g.drawText (ch >= 0 && on ? juce::String (kPc[ch & 15]) + band::qualitySuffix (ch >> 4) : juce::String ("-"), na.removeFromTop (54), juce::Justification::centredLeft);
    g.setFont (font (14.0f, true)); g.setColour (col::muted);
    g.drawText (on ? juce::String (kSectionNames[juce::jlimit (0, 2, proc.uiSection.load())]) + "   -   \"" + kVowel[juce::jlimit (0, NumVowels - 1, proc.uiVowel.load())] + "\""
                   : juce::String ("Press play in Live"), na.removeFromTop (20), juce::Justification::centredLeft);
    {
        auto r = na.removeFromTop (22).withTrimmedTop (8);
        g.setColour (col::line); g.fillRoundedRectangle (r.toFloat().withHeight (6), 3.0f);
        g.setColour (kRose); g.fillRoundedRectangle (r.toFloat().withHeight (6).withWidth ((float) r.getWidth() * juce::jmin (1.0f, proc.uiLevel.load() * 1.6f)), 3.0f);
    }

    // the stack: each part's note, high to low
    g.setColour (col::panel); g.fillRoundedRectangle (stackArea.toFloat(), 8.0f);
    {
        auto r = stackArea.reduced (24, 18);
        g.setColour (kRose); g.setFont (font (13.0f, true));
        g.drawText ("THE STACK", r.removeFromTop (18), juce::Justification::centredLeft);
        r.removeFromTop (6);
        const int parts = 2 + (int) proc.apvts.getRawParameterValue ("parts")->load();
        const float lo = 40, hi = 84;
        for (int s = 0; s < 9; ++s) { const float y = (float) r.getBottom() - (float) r.getHeight() * (float) s / 8.0f; g.setColour (col::line); g.drawHorizontalLine ((int) y, (float) r.getX() + 60, (float) r.getRight()); }
        const float colW = (float) (r.getWidth() - 60) / 4.0f;
        for (int p = 0; p < 4; ++p)
        {
            const float x = (float) r.getX() + 60 + colW * (float) p;
            g.setColour (p < parts ? col::cream : col::line); g.setFont (font (11.0f, true));
            g.drawText (kPart[p == parts - 1 ? 3 : p], (int) x, r.getY() - 2, (int) colW, 14, juce::Justification::centred);
            const int nn = proc.uiPartNote[p].load();
            if (nn < 0 || p >= parts) continue;
            const float y = (float) r.getBottom() - (float) r.getHeight() * juce::jlimit (0.0f, 1.0f, ((float) nn - lo) / (hi - lo));
            g.setColour (kRose); g.fillRoundedRectangle (x + 14, y - 8, colW - 28, 16, 8.0f);
            g.setColour (col::bg); g.setFont (font (11.0f, true));
            g.drawText (juce::String (kPc[nn % 12]) + juce::String (nn / 12 - 1), (int) x + 14, (int) y - 8, (int) colW - 28, 16, juce::Justification::centred);
        }
    }

    for (auto* r : { &singPanel, &soundPanel, &songPanel }) { g.setColour (col::panel); g.fillRoundedRectangle (r->toFloat(), 8.0f); }
    g.setColour (kRose); g.setFont (font (13.0f, true));
    g.drawText ("THE SINGERS", singPanel.reduced (18, 10).removeFromTop (18), juce::Justification::centredLeft);
    g.drawText ("THE SOUND", soundPanel.reduced (18, 10).removeFromTop (18), juce::Justification::centredLeft);
    g.drawText (proc.uiLinked.load() ? "THE SONG (Bandleader runs it)" : "THE SONG (on its own)", songPanel.reduced (18, 10).removeFromTop (18), juce::Justification::centredLeft);
    g.setFont (font (11.0f, true)); g.setColour (col::muted);
    for (auto& [cb, label] : labelled) g.drawText (label, cb->getBounds().translated (0, -15).withHeight (14), juce::Justification::centredLeft);
    g.setFont (font (12.0f));
    g.drawFittedText ("The singers come in at the verse (not in the count-in or the intro) and hold the last chord of a big ending. "
                      "The voices are modelled, not sampled: no words, just oohs, aahs, hums and doo-wops.",
                      legendArea, juce::Justification::topLeft, 2);
}

void HarmoniaEditor::resized()
{
    auto r = getLocalBounds();
    dragMidi->setBounds (getWidth() - 196, 14, 178, 32);
    r.removeFromTop (60);
    r.reduce (18, 14);
    legendArea = r.removeFromBottom (32);
    r.removeFromBottom (6);
    auto top = r.removeFromTop (190);
    nowArea = top.removeFromLeft (320);
    top.removeFromLeft (14);
    stackArea = top;
    r.removeFromTop (12);
    auto left = r.removeFromLeft (r.getWidth() * 55 / 100);
    r.removeFromLeft (12);
    singPanel = left;
    auto right = r;
    soundPanel = right.removeFromTop (right.getHeight() / 2 - 6);
    right.removeFromTop (12);
    songPanel = right;

    auto pa = singPanel.reduced (18, 10); pa.removeFromTop (38);
    auto row = pa.removeFromTop (26);
    const int w3 = row.getWidth() / 3, wq = row.getWidth() / 4;
    groupBox->setBounds (row.removeFromLeft (wq).reduced (4, 0)); feelBox->setBounds (row.removeFromLeft (wq).reduced (4, 0));
    partBox->setBounds (row.removeFromLeft (wq).reduced (4, 0)); partsBox->setBounds (row.reduced (4, 0));
    pa.removeFromTop (22);
    row = pa.removeFromTop (26);
    followBox->setBounds (row.removeFromLeft (w3).reduced (4, 0)); sitBox->setBounds (row.removeFromLeft (w3).reduced (4, 0)); regBox->setBounds (row.reduced (4, 0));
    pa.removeFromTop (10);
    const int kw = pa.getWidth() / 6;
    for (auto* k : singKnobs) k->setBounds (pa.removeFromLeft (kw).withHeight (juce::jmin (pa.getHeight(), 110)));

    auto sa = soundPanel.reduced (18, 10); sa.removeFromTop (22);
    const int sw = sa.getWidth() / 5;
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
