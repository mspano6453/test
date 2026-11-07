#include "CarouselEditor.h"

using namespace cr;
using ttui::font;
namespace col = ttui::col;

namespace
{
const juce::Colour kOrange { 0xffe2682f };
const juce::Colour kGreen  { 0xff6fbf5a };
const juce::Colour kRedRec { 0xffd8352c };

juce::String mmss (float sec)
{
    const int s = (int) sec;
    return juce::String (s / 60) + ":" + juce::String (s % 60).paddedLeft ('0', 2) + "." + juce::String ((int) ((sec - (float) s) * 10.0f));
}
}

ttui::Knob* CarouselEditor::knob (const char* id, const char* label)
{
    auto* k = new ttui::Knob (label, false, this);
    k->attach (proc.apvts, id);
    owned.add (k); addAndMakeVisible (k);
    return k;
}

juce::ComboBox* CarouselEditor::combo (const char* id)
{
    auto* c = new juce::ComboBox();
    if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id))) c->addItemList (ch->choices, 1);
    cAtt.add (new APVTS::ComboBoxAttachment (proc.apvts, id, *c));
    owned.add (c); addAndMakeVisible (c);
    return c;
}

juce::TextButton* CarouselEditor::toggleButton (const char* id, const char* label)
{
    auto* b = new juce::TextButton (label);
    b->setClickingTogglesState (true);
    bAtt.add (new APVTS::ButtonAttachment (proc.apvts, id, *b));
    owned.add (b); addAndMakeVisible (b);
    return b;
}

CarouselEditor::CarouselEditor (CarouselProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    lnf.setAccent (kOrange);
    setLookAndFeel (&lnf);

    recButton.onClick  = [this] { proc.push (ARecPlayDub); };
    stopButton.onClick = [this] { proc.push (AStop); };
    undoButton.onClick = [this] { proc.push (AUndo); };
    redoButton.onClick = [this] { proc.push (ARedo); };
    for (auto* b : { &recButton, &stopButton, &undoButton, &redoButton }) addAndMakeVisible (*b);

    for (int i = 0; i < kSlots; ++i)
    {
        slotButtons[i].setButtonText (juce::String (i + 1));
        slotButtons[i].onClick = [this, i] { proc.push (ASelect0 + i); };
        addAndMakeVisible (slotButtons[i]);
    }
    clearButton.onClick = [this] { proc.push (AClear); };
    clearAllButton.onClick = [this]
    {
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Carousel", "Clear all 10 loops?", "Clear all", "Cancel", this,
                                            juce::ModalCallbackFunction::create ([this] (int r) { if (r == 1) proc.push (AClearAll); }));
    };
    exportButton.onClick = [this]
    {
        auto f = proc.exportCurrentLoop();
        if (f.existsAsFile()) { lastExport = "Saved: Documents/Carousel Loops/" + f.getFileName(); f.revealToUser(); }
        else lastExport = "Nothing to export yet (record a loop first).";
        repaint (lastExportArea);
    };
    for (auto* b : { &clearButton, &clearAllButton, &exportButton }) addAndMakeVisible (*b);

    levelK = knob ("loop_lvl", "LOOP LEVEL");
    dryK = knob ("dry", "INPUT");
    decayK = knob ("decay", "FADE");
    revB = toggleButton ("reverse", "REVERSE");
    halfB = toggleButton ("half", "HALF SPEED");
    syncB = toggleButton ("sync", "SNAP TO TEMPO");
    stopBox = combo ("stop_mode");
    playBox = combo ("play_mode");
    snapBox = combo ("snap");

    setSize (1040, 680);
    startTimerHz (30);
}

CarouselEditor::~CarouselEditor() { setLookAndFeel (nullptr); }

void CarouselEditor::timerCallback()
{
    const int st = proc.uiState.load();
    recButton.setButtonText (proc.uiArmed.load() ? "WAITING FOR SECTION..." : proc.uiSectionRec.load() ? "RECORDING SECTION" : st == Empty ? "REC" : st == Recording ? (proc.uiWaitingForBar.load() ? "FINISHING BAR..." : "PLAY") : st == Overdubbing ? "PLAY" : st == Playing ? "OVERDUB" : "PLAY");
    const uint32_t has = proc.uiSlotHas.load();
    const int cur = proc.uiSlot.load();
    for (int i = 0; i < kSlots; ++i)
    {
        slotButtons[i].setToggleState (i == cur, juce::dontSendNotification);
        slotButtons[i].setButtonText (juce::String (i + 1) + ((has >> i) & 1u ? " *" : ""));
    }
    undoButton.setEnabled (proc.uiLayers.load() > 1 || st == Recording);
    redoButton.setEnabled (proc.uiRedo.load() > 0);
    exportButton.setEnabled (st != Recording && st != Empty);
    inShown = juce::jmax (proc.uiInLevel.load(), inShown * 0.88f);
    repaint();
}

void CarouselEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    auto header = getLocalBounds().removeFromTop (60);
    g.setColour (col::panel); g.fillRect (header);
    g.setColour (kOrange); g.fillRect (header.removeFromBottom (2));
    g.setColour (col::cream); g.setFont (font (28.0f, true));
    g.drawText ("CAROUSEL", 20, 8, 300, 30, juce::Justification::centredLeft);
    g.setColour (kOrange); g.setFont (font (11.0f, true));
    g.drawText ("STEREO LOOPER", 22, 38, 300, 14, juce::Justification::centredLeft);

    // ---- the loop ring
    const int st = proc.uiState.load();
    const auto c = ringArea.toFloat().getCentre();
    const float r = juce::jmin (ringArea.getWidth(), ringArea.getHeight()) * 0.5f - 10.0f;
    g.setColour (col::panel); g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    juce::Path track; track.addCentredArc (c.x, c.y, r - 14, r - 14, 0, 0, juce::MathConstants<float>::twoPi, true);
    g.setColour (col::line); g.strokePath (track, juce::PathStrokeType (14.0f));

    const juce::Colour stateCol = st == Recording ? kRedRec : st == Overdubbing ? kOrange : (st == Playing || st == StoppingEnd || st == StoppingFade) ? kGreen : col::muted;
    if (st == Recording)
    {
        // recording: the ring pulses and fills by seconds
        const float sec = proc.uiRecSec.load();
        const float frac = std::fmod (sec, 4.0f) / 4.0f;
        juce::Path arc; arc.addCentredArc (c.x, c.y, r - 14, r - 14, 0, 0, frac * juce::MathConstants<float>::twoPi, true);
        g.setColour (stateCol); g.strokePath (arc, juce::PathStrokeType (14.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else if (st != Empty)
    {
        const float frac = proc.uiPos.load();
        juce::Path arc; arc.addCentredArc (c.x, c.y, r - 14, r - 14, 0, 0, frac * juce::MathConstants<float>::twoPi, true);
        g.setColour (stateCol.withAlpha (st == Stopped ? 0.35f : 1.0f));
        g.strokePath (arc, juce::PathStrokeType (14.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        // layer dots around the inside
        const int layers = proc.uiLayers.load();
        for (int l = 0; l < layers; ++l)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) l / 24.0f;
            const auto p = c.getPointOnCircumference (r - 40, a);
            g.setColour (l == 0 ? col::cream : kOrange);
            g.fillEllipse (p.x - 4, p.y - 4, 8, 8);
        }
    }
    // input level glow on the rim
    g.setColour (kOrange.withAlpha (juce::jlimit (0.0f, 0.6f, inShown * 2.0f)));
    g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 3.0f);

    g.setColour (stateCol); g.setFont (font (30.0f, true));
    g.drawText (stateName (st), juce::Rectangle<float> (r * 2, 36).withCentre (c.translated (0, -34)), juce::Justification::centred);
    g.setColour (col::cream); g.setFont (font (22.0f, true));
    const float len = st == Recording ? proc.uiRecSec.load() : proc.uiLenSec.load();
    g.drawText (st == Empty ? juce::String ("press REC") : mmss (len), juce::Rectangle<float> (r * 2, 28).withCentre (c.translated (0, 6)), juce::Justification::centred);
    g.setColour (col::muted); g.setFont (font (13.0f));
    juce::String info = "Loop " + juce::String (proc.uiSlot.load() + 1);
    if (st != Empty && st != Recording) info << "  -  " << proc.uiLayers.load() << (proc.uiLayers.load() == 1 ? " layer" : " layers");
    if (proc.apvts.getRawParameterValue ("reverse")->load() > 0.5f) info << "  -  REV";
    if (proc.apvts.getRawParameterValue ("half")->load() > 0.5f) info << "  -  1/2";
    g.drawText (info, juce::Rectangle<float> (r * 2, 20).withCentre (c.translated (0, 36)), juce::Justification::centred);

    // ---- side panel
    g.setColour (col::panel); g.fillRoundedRectangle (sidePanel.toFloat(), 8.0f);
    g.setColour (kOrange); g.setFont (font (13.0f, true));
    auto sp = sidePanel.reduced (14, 10);
    g.drawText ("LOOP BANK", sp.removeFromTop (18), juce::Justification::centredLeft);
    g.setColour (col::muted); g.setFont (font (11.0f, true));
    for (auto* cb : { stopBox, playBox, snapBox })
    {
        const char* t = cb == stopBox ? "STOP MODE" : cb == playBox ? "PLAY MODE" : "SNAP TO";
        g.drawText (t, cb->getBounds().translated (0, -16).withHeight (14), juce::Justification::centredLeft);
    }

    // memory
    const float freeSec = (float) proc.uiFreeChunks.load();
    g.setColour (col::muted); g.setFont (font (11.0f, true));
    g.drawText ("MEMORY LEFT  " + juce::String ((int) freeSec / 60) + " min " + juce::String ((int) freeSec % 60) + " s",
                memArea.translated (0, -16).withHeight (14), juce::Justification::centredLeft);
    g.setColour (col::line); g.fillRoundedRectangle (memArea.toFloat(), 3.0f);
    g.setColour (kOrange); g.fillRoundedRectangle (memArea.toFloat().withWidth ((float) memArea.getWidth() * freeSec / (float) kPoolSeconds), 3.0f);

    g.setColour (col::cream); g.setFont (font (12.0f));
    g.drawFittedText (lastExport, lastExportArea, juce::Justification::centredLeft, 2, 1.0f);

    g.setColour (col::muted); g.setFont (font (12.0f));
    g.drawFittedText ("FOOTSWITCH (FCB1010 or any MIDI controller): route a MIDI track to Carousel and send notes "
                      "C3 rec/play/dub, D3 stop, E3 undo, F3 redo, G3 reverse, A3 half speed, B3 next loop, C4 previous, D4 clear - "
                      "or CCs 80-88 in the same order. Or use Live's MIDI Map on the 'Footswitch' parameters.",
                      legendArea, juce::Justification::topLeft, 4, 1.0f);
}

void CarouselEditor::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (60);
    r.reduce (18, 14);
    legendArea = r.removeFromBottom (62);
    r.removeFromBottom (8);

    auto left = r.removeFromLeft ((int) (r.getWidth() * 0.5f));
    r.removeFromLeft (16);
    sidePanel = r;

    auto buttons = left.removeFromBottom (120);
    ringArea = left;
    recButton.setBounds (buttons.removeFromTop (58).reduced (4));
    const int bw = buttons.getWidth() / 3;
    stopButton.setBounds (buttons.removeFromLeft (bw).reduced (4));
    undoButton.setBounds (buttons.removeFromLeft (bw).reduced (4));
    redoButton.setBounds (buttons.reduced (4));

    auto sp = sidePanel.reduced (14, 10);
    sp.removeFromTop (22);
    auto slotsRow1 = sp.removeFromTop (34), slotsRow2 = sp.removeFromTop (34);
    const int sw = slotsRow1.getWidth() / 5;
    for (int i = 0; i < 5; ++i) slotButtons[i].setBounds (slotsRow1.removeFromLeft (sw).reduced (3));
    for (int i = 5; i < 10; ++i) slotButtons[i].setBounds (slotsRow2.removeFromLeft (sw).reduced (3));
    sp.removeFromTop (10);
    auto tog = sp.removeFromTop (34);
    const int tw = tog.getWidth() / 3;
    revB->setBounds (tog.removeFromLeft (tw).reduced (3));
    halfB->setBounds (tog.removeFromLeft (tw).reduced (3));
    syncB->setBounds (tog.reduced (3));
    sp.removeFromTop (8);
    auto knobs = sp.removeFromTop (104);
    const int kw = knobs.getWidth() / 3;
    levelK->setBounds (knobs.removeFromLeft (kw));
    dryK->setBounds (knobs.removeFromLeft (kw));
    decayK->setBounds (knobs);
    sp.removeFromTop (22);
    auto combos = sp.removeFromTop (26);
    const int cw = combos.getWidth() / 3;
    stopBox->setBounds (combos.removeFromLeft (cw).reduced (3, 0));
    playBox->setBounds (combos.removeFromLeft (cw).reduced (3, 0));
    snapBox->setBounds (combos.reduced (3, 0));
    sp.removeFromTop (30);
    memArea = sp.removeFromTop (8);
    sp.removeFromTop (12);
    auto actions = sp.removeFromTop (32);
    exportButton.setBounds (actions.removeFromLeft (actions.getWidth() / 2).reduced (3, 0));
    clearButton.setBounds (actions.removeFromLeft (actions.getWidth() / 2).reduced (3, 0));
    clearAllButton.setBounds (actions.reduced (3, 0));
    lastExportArea = sp.removeFromTop (34);
}
