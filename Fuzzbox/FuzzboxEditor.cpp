#include "FuzzboxEditor.h"
using ttui::font;
namespace col = ttui::col;
namespace
{
const char* kNames[] = { "WAH", "FUZZ", "OCTAVE", "TREMOLO", "SPRING" };
const juce::Colour kPaint[] = { juce::Colour (0xff3a3f47), juce::Colour (0xff9b3b2c), juce::Colour (0xff3f6e8a), juce::Colour (0xffc9a227), juce::Colour (0xff5e7f4f) };
const char* kKeys[] = { "C3", "D3", "E3", "F3", "G3" };
}

ttui::Knob* FuzzboxEditor::knob (const char* id, const char* label)
{
    auto* k = new ttui::Knob (label, true, this); k->attach (proc.apvts, id); owned.add (k); addAndMakeVisible (k); return k;
}
juce::ComboBox* FuzzboxEditor::combo (const char* id)
{
    auto* c = new juce::ComboBox();
    if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id))) c->addItemList (ch->choices, 1);
    cAtt.add (new APVTS::ComboBoxAttachment (proc.apvts, id, *c)); owned.add (c); addAndMakeVisible (c); return c;
}

FuzzboxEditor::FuzzboxEditor (FuzzboxProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    lnf.setAccent (juce::Colour (0xffe0763a)); setLookAndFeel (&lnf);
    presetBox.addItemList (FuzzboxProcessor::presetNames(), 1);
    presetBox.setTextWhenNothingSelected ("Pedalboards...");
    presetBox.onChange = [this] { if (presetBox.getSelectedItemIndex() >= 0) proc.loadPreset (presetBox.getSelectedItemIndex()); };
    addAndMakeVisible (presetBox);
    for (int i = 0; i < FuzzboxProcessor::NumPedals; ++i)
    {
        stomp[i].setButtonText ("STOMP");
        stomp[i].setClickingTogglesState (true);
        stomp[i].setTooltip (juce::String ("On / off. Footswitch: MIDI note ") + kKeys[i] + ", or CC " + juce::String (80 + i));
        bAtt.add (new APVTS::ButtonAttachment (proc.apvts, FuzzboxProcessor::onId (i), stomp[i]));
        addAndMakeVisible (stomp[i]);
    }
    parts[0] = { combo ("wah_mode"), knob ("wah_pos", "PEDAL"), knob ("wah_range", "RANGE") };
    parts[1] = { knob ("fuzz", "FUZZ"), knob ("fuzz_tone", "TONE"), knob ("fuzz_lvl", "LEVEL") };
    parts[2] = { knob ("oct", "OCTAVE"), knob ("oct_lvl", "LEVEL") };
    parts[3] = { combo ("trem_sync"), combo ("trem_shape"), knob ("trem", "DEPTH"), knob ("trem_rate", "SPEED") };
    parts[4] = { knob ("spring", "MIX"), knob ("spring_decay", "DECAY"), knob ("spring_drip", "DRIP") };
    outK = knob ("out", "OUTPUT");
    setSize (1120, 470);
    startTimerHz (20);
}
FuzzboxEditor::~FuzzboxEditor() { setLookAndFeel (nullptr); }

void FuzzboxEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1714));
    auto header = getLocalBounds().removeFromTop (60);
    g.setColour (col::panel); g.fillRect (header);
    g.setColour (juce::Colour (0xffe0763a)); g.fillRect (header.removeFromBottom (2));
    g.setColour (col::cream); g.setFont (font (28.0f, true)); g.drawText ("FUZZBOX", 20, 8, 300, 30, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xffe0763a)); g.setFont (font (11.0f, true)); g.drawText ("A 60s PEDALBOARD", 22, 38, 300, 14, juce::Justification::centredLeft);
    // the patch cable running through the board
    g.setColour (juce::Colour (0xff121010));
    g.fillRect (box[0].getX() - 14, box[0].getY() + 40, box[4].getRight() - box[0].getX() + 28, 6);
    for (int i = 0; i < FuzzboxProcessor::NumPedals; ++i)
    {
        const bool on = proc.apvts.getRawParameterValue (FuzzboxProcessor::onId (i))->load() > 0.5f;
        auto b = box[i].toFloat();
        g.setColour (kPaint[i]); g.fillRoundedRectangle (b, 10.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f)); g.drawRoundedRectangle (b.reduced (1), 10.0f, 2.0f);
        g.setColour (col::cream); g.setFont (font (18.0f, true));
        g.drawText (kNames[i], box[i].reduced (14, 10).removeFromTop (24), juce::Justification::centredLeft);
        // the LED
        auto led = juce::Rectangle<float> (b.getRight() - 30, b.getY() + 14, 14, 14);
        g.setColour (on ? juce::Colour (0xffff3b30) : juce::Colour (0xff3a1210)); g.fillEllipse (led);
        if (on) { g.setColour (juce::Colour (0x55ff3b30)); g.fillEllipse (led.expanded (5)); }
        g.setColour (col::cream.withAlpha (0.6f)); g.setFont (font (10.0f));
        g.drawText (juce::String ("footswitch ") + kKeys[i], box[i].reduced (14, 8).removeFromBottom (14), juce::Justification::centred);
    }
    // the wah's rocker position and the tremolo's pulse
    {
        auto w = box[0].reduced (18).removeFromBottom (110).removeFromTop (12).toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.3f)); g.fillRoundedRectangle (w, 4.0f);
        g.setColour (col::cream); g.fillRoundedRectangle (w.withWidth (juce::jmax (6.0f, w.getWidth() * proc.uiWah.load())), 4.0f);
    }
    g.setColour (col::muted); g.setFont (font (12.0f));
    g.drawFittedText ("Put it on the guitar track. The order is the classic one: wah, fuzz, octave, tremolo, spring. MIDI Captain: send notes C3-G3 to stomp the pedals, "
                      "CC 11 (or CC 4) from an expression pedal works the wah.", getLocalBounds().removeFromBottom (36).reduced (18, 4), juce::Justification::centredLeft, 2);
}

void FuzzboxEditor::resized()
{
    auto r = getLocalBounds();
    auto head = r.removeFromTop (60);
    presetBox.setBounds (head.removeFromRight (240).reduced (14, 16));
    outK->setBounds (head.removeFromRight (80).reduced (2));
    r.reduce (18, 14); r.removeFromBottom (32);
    const int w = (r.getWidth() - 4 * 14) / 5;
    for (int i = 0; i < FuzzboxProcessor::NumPedals; ++i)
    {
        box[i] = r.removeFromLeft (w); r.removeFromLeft (14);
        auto b = box[i].reduced (12, 10); b.removeFromTop (34); b.removeFromBottom (16);
        stomp[i].setBounds (b.removeFromBottom (44).withSizeKeepingCentre (110, 40));
        b.removeFromBottom (8);
        // combos on top, knobs two to a row below
        std::vector<juce::Component*> knobs;
        for (auto* c : parts[i])
        {
            if (dynamic_cast<juce::ComboBox*> (c)) { c->setBounds (b.removeFromTop (26)); b.removeFromTop (6); }
            else knobs.push_back (c);
        }
        const int rows = ((int) knobs.size() + 1) / 2;
        const int kh = juce::jmin (100, b.getHeight() / juce::jmax (1, rows));
        for (size_t k = 0; k < knobs.size(); k += 2)
        {
            auto row = b.removeFromTop (kh);
            if (k + 1 < knobs.size()) { knobs[k]->setBounds (row.removeFromLeft (row.getWidth() / 2)); knobs[k + 1]->setBounds (row); }
            else knobs[k]->setBounds (row.withSizeKeepingCentre (row.getWidth() / 2, row.getHeight()));
        }
    }
}
