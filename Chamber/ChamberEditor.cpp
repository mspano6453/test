#include "ChamberEditor.h"

using namespace ch;
using ttui::font;
namespace col = ttui::col;

namespace
{
const juce::Colour kGold { 0xffd9b45a };
const char* kPc[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
juce::String noteName (int n) { return n < 0 ? juce::String ("-") : juce::String (kPc[n % 12]) + juce::String (n / 12 - 1); }
const char* instrName (int i)
{
    static const char* n[] = { "Violins", "Violas", "Cellos", "Solo Violin", "Trumpet", "French Horn", "Trombone", "Flute", "Oboe", "Clarinet", "Bassoon", "Timpani" };
    return n[juce::jlimit (0, (int) NumInstr - 1, i)];
}
}

ttui::Knob* ChamberEditor::knob (const char* id, const char* label, bool small)
{
    auto* k = new ttui::Knob (label, small, this);
    k->attach (proc.apvts, id);
    owned.add (k); addAndMakeVisible (k);
    return k;
}

juce::ComboBox* ChamberEditor::combo (const char* id, const char* label)
{
    auto* c = new juce::ComboBox();
    if (auto* chp = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id))) c->addItemList (chp->choices, 1);
    cAtt.add (new APVTS::ComboBoxAttachment (proc.apvts, id, *c));
    owned.add (c); addAndMakeVisible (c);
    if (label != nullptr) labelled.push_back ({ c, label });
    return c;
}

ChamberEditor::ChamberEditor (ChamberProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    lnf.setAccent (kGold);
    setLookAndFeel (&lnf);
    ensBox = combo ("ensemble", "ENSEMBLE"); feelBox = combo ("feel", "ARRANGEMENT"); followBox = combo ("follow", "CHORDS FROM");
    sitBox = combo ("sit_out", "WITH THE BAND"); regBox = combo ("register", "REGISTER"); timBox = combo ("timpani", "TIMPANI");
    artBox = combo ("play_art", "WHEN YOU PLAY IT");
    keyBox = combo ("key", "KEY"); chordBarsBox = combo ("chord_bars", "BARS PER CHORD"); songBox = combo ("song", "SONG"); secBarsBox = combo ("section_bars", "SECTION LENGTH");
    progBox[0] = combo ("prog_verse", nullptr); progBox[1] = combo ("prog_chorus", nullptr); progBox[2] = combo ("prog_bridge", nullptr);
    playerKnobs = { knob ("busy", "BUSY"), knob ("dynamics", "DYNAMICS"), knob ("swell", "SWELL"), knob ("human", "HUMAN"), knob ("swing", "SWING"), knob ("length", "LENGTH") };
    soundKnobs = { knob ("warp", "WARP", true), knob ("tape", "TAPE", true), knob ("width", "WIDTH", true),
                   knob ("room", "ROOM", true), knob ("tone", "TONE", true), knob ("out", "OUTPUT", true) };
    drag = std::make_unique<mc::DragButton> (proc.capture, "Chamber", kGold);
    drag->onMessage = [this] (const juce::String& m) { status = m; statusTicks = 150; repaint (statusArea); };
    addAndMakeVisible (*drag);
    setSize (1040, 700);
    startTimerHz (20);
}

ChamberEditor::~ChamberEditor() { setLookAndFeel (nullptr); }

void ChamberEditor::timerCallback()
{
    auto v = [&] (const char* id) { return (int) proc.apvts.getRawParameterValue (id)->load(); };
    const bool band = v ("follow") == FBand;
    for (auto* cb : { keyBox, progBox[0], progBox[1], progBox[2], chordBarsBox, songBox, secBarsBox }) cb->setEnabled (band && ! proc.uiLinked.load());
    feelBox->setEnabled (v ("follow") != FPlay && v ("follow") != FGuitar);
    artBox->setEnabled (v ("follow") == FPlay || v ("follow") == FGuitar);
    juce::String st;
    for (auto* id : { "follow", "key", "prog_verse", "prog_chorus", "prog_bridge", "ensemble", "feel", "sit_out" }) st << v (id) << ",";
    st << (int) proc.uiLinked.load() << (int) proc.samplesLoaded();
    if (st != lastStatic) { lastStatic = st; repaint(); return; }
    int64_t dyn = proc.uiChord.load() * 131 + proc.uiSection.load() * 17 + (proc.uiTransport.load() ? 3 : 0);
    for (int i = 0; i < 5; ++i) dyn = dyn * 211 + proc.uiPartNote[i].load();
    if (dyn != lastDyn) { lastDyn = dyn; repaint (scoreArea); }
    if (statusTicks > 0 && --statusTicks == 0) { status.clear(); repaint (statusArea); }
}

void ChamberEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    auto header = getLocalBounds().removeFromTop (60);
    g.setColour (col::panel); g.fillRect (header);
    g.setColour (kGold); g.fillRect (header.removeFromBottom (2));
    g.setColour (col::cream); g.setFont (font (28.0f, true));
    g.drawText ("CHAMBER", 20, 8, 300, 30, juce::Justification::centredLeft);
    g.setColour (kGold); g.setFont (font (11.0f, true));
    g.drawText ("60s ORCHESTRA", 22, 38, 300, 14, juce::Justification::centredLeft);
    if (! proc.samplesLoaded()) { g.setColour (col::red); g.drawText ("samples missing", 320, 20, 200, 20, juce::Justification::centredLeft); }
    const int ens = (int) proc.apvts.getRawParameterValue ("ensemble")->load();
    g.setColour (col::muted); g.setFont (font (12.0f, true));
    g.drawText (juce::String (ensembleName (ens)).toUpperCase() + "  -  REAL RECORDED ORCHESTRA", header.withTrimmedRight (20), juce::Justification::centredRight);

    // ---- the score: the four parts and what each is playing
    g.setColour (col::panel); g.fillRoundedRectangle (scoreArea.toFloat(), 8.0f);
    auto sa = scoreArea.reduced (16, 10);
    auto left = sa.removeFromLeft (230);
    g.setColour (kGold); g.setFont (font (13.0f, true));
    g.drawText ("THE SCORE", left.removeFromTop (20), juce::Justification::centredLeft);
    const int chd = proc.uiChord.load();
    g.setColour (chd >= 0 ? col::cream : col::muted); g.setFont (font (44.0f, true));
    g.drawText (chd >= 0 ? juce::String (kPc[chd & 15]) + band::qualitySuffix (chd >> 4) : juce::String ("--"), left.removeFromTop (56), juce::Justification::centredLeft);
    g.setColour (col::muted); g.setFont (font (12.0f));
    const int follow = (int) proc.apvts.getRawParameterValue ("follow")->load();
    const char* secs[3] = { "VERSE", "CHORUS", "BRIDGE" };
    juce::String s1 = follow == FPlay ? "Play it from your keys: each note goes to the part that plays that range best."
                    : follow == FGuitar ? juce::String ("Following your guitar: route it to the sidechain (Audio From) and play single notes. ") + (proc.uiGuitarNote.load() >= 0 ? "Hearing " + juce::MidiMessage::getMidiNoteName (proc.uiGuitarNote.load(), true, true, 3) : juce::String ("Listening..."))
                    : proc.apvts.getRawParameterValue ("sit_out")->load() > 0.5f ? "Sitting out this style. Set WITH THE BAND to Plays along."
                    : ! proc.uiTransport.load() ? "Press play in Live."
                    : juce::String (feelName ((int) proc.apvts.getRawParameterValue ("feel")->load())) + "  -  " + secs[juce::jlimit (0, 2, proc.uiSection.load())]
                      + (proc.uiLinked.load() ? "  (following Bandleader)" : "  (its own song)");
    g.drawFittedText (s1, left.removeFromTop (44), juce::Justification::topLeft, 3, 1.0f);
    // staves
    int ps[4]; ensembleParts (ens, ps);
    const int rows = 5;
    const float rh = (float) sa.getHeight() / rows;
    for (int p = 0; p < rows; ++p)
    {
        auto row = juce::Rectangle<float> ((float) sa.getX(), sa.getY() + p * rh, (float) sa.getWidth(), rh).reduced (0, 3);
        g.setColour (col::bg.brighter (0.04f)); g.fillRoundedRectangle (row, 4.0f);
        g.setColour (col::muted.withAlpha (0.25f));
        for (int l = 1; l < 5; ++l) g.drawHorizontalLine ((int) (row.getY() + row.getHeight() * l / 5), row.getX() + 140, row.getRight() - 8);
        const int note = proc.uiPartNote[p].load();
        g.setColour (note >= 0 ? kGold : col::muted); g.setFont (font (12.0f, true));
        g.drawText (p < 4 ? juce::String (instrName (ps[p])) : juce::String ("Timpani"), row.withWidth (130).withTrimmedLeft (10), juce::Justification::centredLeft);
        if (note >= 0)
        {
            const float x = row.getX() + 150 + (row.getWidth() - 170) * juce::jlimit (0.0f, 1.0f, (note - 30) / 70.0f);
            g.setColour (kGold); g.fillEllipse (x - 7, row.getCentreY() - 5, 14, 10);
            g.setColour (col::cream); g.setFont (font (11.0f)); g.drawText (noteName (note), (int) x + 10, (int) row.getY(), 50, (int) row.getHeight(), juce::Justification::centredLeft);
        }
    }

    g.setColour (col::panel); g.fillRoundedRectangle (playerPanel.toFloat(), 8.0f);
    g.setColour (kGold); g.setFont (font (13.0f, true));
    g.drawText ("THE ARRANGER", playerPanel.reduced (16, 10).removeFromTop (18), juce::Justification::centredLeft);
    g.setColour (col::panel); g.fillRoundedRectangle (soundPanel.toFloat(), 8.0f);
    g.setColour (kGold);
    g.drawText ("THE ROOM", soundPanel.reduced (16, 10).removeFromTop (18), juce::Justification::centredLeft);
    g.setColour (col::muted); g.setFont (font (11.0f, true));
    for (auto& [cb, t] : labelled) g.drawText (t, cb->getBounds().translated (0, -15).withHeight (14), juce::Justification::centredLeft);
    const int key = (int) proc.apvts.getRawParameterValue ("key")->load();
    const char* names[] = { "VERSE: ", "CHORUS: ", "BRIDGE: " };
    for (int i = 0; i < 3; ++i)
        g.drawText (juce::String (names[i]) + ut::progressionChords (progBox[i]->getSelectedItemIndex(), key), progBox[i]->getBounds().translated (0, -15).withHeight (14), juce::Justification::centredLeft);
    g.setColour (kGold);
    g.drawText (proc.uiLinked.load() ? "SONG: FOLLOWING BANDLEADER" : "WHEN IT PLAYS BY ITSELF", keyBox->getBounds().translated (0, -36).withHeight (16).withWidth (500), juce::Justification::centredLeft);
    g.setColour (status.isNotEmpty() ? kGold : col::muted); g.setFont (font (11.0f));
    g.drawFittedText (status.isNotEmpty() ? status : juce::String ("Drag onto a MIDI track to get the parts as a clip (one MIDI channel per part)."), statusArea, juce::Justification::centredLeft, 2, 1.0f);

    g.setColour (col::muted); g.setFont (font (12.0f));
    g.drawFittedText ("Put Chamber on a MIDI track and press play: it arranges the Bandleader's chords for strings, brass or woodwinds. "
                      "SWELL is how softly the bows come in. WARP detunes and wobbles the players for that blown-out 90s orchestra; TAPE puts it all on old tape. "
                      "WIDTH at 0 is a mono 60s mix; up high the violins sit left and the cellos right, like the old records.",
                      legendArea, juce::Justification::topLeft, 3, 1.0f);
}

void ChamberEditor::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (60);
    r.reduce (18, 14);
    legendArea = r.removeFromBottom (48);
    r.removeFromBottom (8);
    scoreArea = r.removeFromTop (190);
    r.removeFromTop (12);
    playerPanel = r.removeFromLeft ((int) (r.getWidth() * 0.66f));
    r.removeFromLeft (14);
    soundPanel = r;

    auto pp = playerPanel.reduced (16, 10);
    pp.removeFromTop (40);
    auto row1 = pp.removeFromTop (26);
    const int w4 = row1.getWidth() / 4;
    ensBox->setBounds (row1.removeFromLeft (w4).reduced (3, 0));
    feelBox->setBounds (row1.removeFromLeft (w4).reduced (3, 0));
    followBox->setBounds (row1.removeFromLeft (w4).reduced (3, 0));
    sitBox->setBounds (row1.reduced (3, 0));
    pp.removeFromTop (22);
    auto row2 = pp.removeFromTop (26);
    regBox->setBounds (row2.removeFromLeft (w4).reduced (3, 0));
    timBox->setBounds (row2.removeFromLeft (w4).reduced (3, 0));
    artBox->setBounds (row2.removeFromLeft (w4).reduced (3, 0));
    pp.removeFromTop (42);
    auto row3 = pp.removeFromTop (26);
    keyBox->setBounds (row3.removeFromLeft (w4).reduced (3, 0));
    chordBarsBox->setBounds (row3.removeFromLeft (w4).reduced (3, 0));
    songBox->setBounds (row3.removeFromLeft (w4).reduced (3, 0));
    secBarsBox->setBounds (row3.reduced (3, 0));
    pp.removeFromTop (20);
    auto row4 = pp.removeFromTop (26);
    const int cw = row4.getWidth() / 3;
    for (int i = 0; i < 3; ++i) progBox[i]->setBounds ((i < 2 ? row4.removeFromLeft (cw) : row4).reduced (3, 0));
    pp.removeFromTop (6);
    const int kw = pp.getWidth() / (int) playerKnobs.size();
    for (auto* k : playerKnobs) k->setBounds (pp.removeFromLeft (kw));

    auto sp = soundPanel.reduced (12, 10);
    sp.removeFromTop (28);
    for (int rowI = 0; rowI < 2; ++rowI)
    {
        auto kr = sp.removeFromTop (88);
        const int w = kr.getWidth() / 3;
        for (int c = 0; c < 3; ++c) soundKnobs[(size_t) (rowI * 3 + c)]->setBounds (kr.removeFromLeft (w));
    }
    sp.removeFromTop (10);
    drag->setBounds (sp.removeFromTop (34).reduced (4, 0));
    statusArea = sp.removeFromTop (34).reduced (4, 2);
}
