#include "SwirlEditor.h"
using ttui::font;
namespace col = ttui::col;
namespace { const juce::Colour kViolet { 0xffb48ae8 }; const char* kTitles[4] = { "BACKWARDS", "PHASE", "ROTARY", "TAPE FLANGE" }; }

ttui::Knob* SwirlEditor::knob (const char* id, const char* label)
{
    auto* k = new ttui::Knob (label, true, this); k->attach (proc.apvts, id); owned.add (k); addAndMakeVisible (k); return k;
}
juce::ComboBox* SwirlEditor::combo (const char* id)
{
    auto* c = new juce::ComboBox();
    if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id))) c->addItemList (ch->choices, 1);
    cAtt.add (new APVTS::ComboBoxAttachment (proc.apvts, id, *c)); owned.add (c); addAndMakeVisible (c); return c;
}

SwirlEditor::SwirlEditor (SwirlProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    lnf.setAccent (kViolet); setLookAndFeel (&lnf);
    presetBox.addItemList (SwirlProcessor::presetNames(), 1);
    presetBox.setTextWhenNothingSelected ("Presets...");
    presetBox.onChange = [this] { if (presetBox.getSelectedItemIndex() >= 0) proc.loadPreset (presetBox.getSelectedItemIndex()); };
    addAndMakeVisible (presetBox);
    backK = knob ("back", "AMOUNT"); lenBox = combo ("back_len");
    phaseK = knob ("phase", "AMOUNT"); phRateK = knob ("phase_rate", "RATE"); phFbK = knob ("phase_fb", "FEEDBACK");
    rotK = knob ("rotary", "AMOUNT"); speedBox = combo ("rotary_speed");
    flK = knob ("flange", "AMOUNT"); flRateK = knob ("flange_rate", "RATE"); flFbK = knob ("flange_fb", "FEEDBACK");
    mixK = knob ("mix", "MIX"); outK = knob ("out", "OUTPUT");
    setSize (860, 470);
    startTimerHz (30);
}
SwirlEditor::~SwirlEditor() { setLookAndFeel (nullptr); }

void SwirlEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    auto header = getLocalBounds().removeFromTop (60);
    g.setColour (col::panel); g.fillRect (header);
    g.setColour (kViolet); g.fillRect (header.removeFromBottom (2));
    g.setColour (col::cream); g.setFont (font (28.0f, true)); g.drawText ("SWIRL", 20, 8, 300, 30, juce::Justification::centredLeft);
    g.setColour (kViolet); g.setFont (font (11.0f, true)); g.drawText ("PSYCHEDELIC STUDIO TRICKS", 22, 38, 300, 14, juce::Justification::centredLeft);

    // the scope: a spinning horn, a sweeping phase line, the reversing tape
    g.setColour (col::panel); g.fillRoundedRectangle (scope.toFloat(), 8.0f);
    auto v = [&] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
    {
        auto r = scope.reduced (16, 12).toFloat();
        const float w = r.getWidth() / 4.0f;
        // backwards: a tape running right to left
        auto a = r.removeFromLeft (w).reduced (10, 16);
        g.setColour (col::line); g.fillRoundedRectangle (a.withSizeKeepingCentre (a.getWidth(), 6), 3.0f);
        g.setColour (kViolet.withAlpha (0.3f + 0.7f * v ("back"))); g.fillEllipse (a.getRight() - a.getWidth() * proc.uiRev.load() - 6, a.getCentreY() - 6, 12, 12);
        // phase: a moving notch
        a = r.removeFromLeft (w).reduced (10, 10);
        juce::Path ph; const float sweep = 0.5f + 0.5f * std::sin (6.2832f * proc.uiSweep.load());
        for (int i = 0; i <= 40; ++i) { const float t = (float) i / 40.0f; const float y = a.getY() + a.getHeight() * (0.25f + 0.6f * v ("phase") * std::exp (-std::pow ((t - 0.15f - 0.7f * sweep) * 9.0f, 2.0f))); if (i == 0) ph.startNewSubPath (a.getX(), y); else ph.lineTo (a.getX() + a.getWidth() * t, y); }
        g.setColour (kViolet); g.strokePath (ph, juce::PathStrokeType (2.0f));
        // rotary: the horn
        a = r.removeFromLeft (w).reduced (10, 6);
        const float rad = juce::jmin (a.getWidth(), a.getHeight()) * 0.42f, ang = 6.2832f * proc.uiSpin.load();
        g.setColour (col::line); g.drawEllipse (a.getCentreX() - rad, a.getCentreY() - rad, rad * 2, rad * 2, 1.5f);
        g.setColour (kViolet.withAlpha (0.3f + 0.7f * v ("rotary"))); g.drawLine (a.getCentreX(), a.getCentreY(), a.getCentreX() + rad * std::cos (ang), a.getCentreY() + rad * std::sin (ang), 4.0f);
        // flange: two reels
        a = r.reduced (10, 14);
        for (int k = 0; k < 2; ++k) { g.setColour (k == 0 ? col::line.brighter (0.3f) : kViolet.withAlpha (0.3f + 0.7f * v ("flange"))); g.drawEllipse (a.getCentreX() - 34 + k * 36.0f, a.getCentreY() - 16, 32, 32, 2.5f); }
    }
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (col::panel); g.fillRoundedRectangle (panel[i].toFloat(), 8.0f);
        g.setColour (kViolet); g.setFont (font (13.0f, true));
        g.drawText (kTitles[i], panel[i].reduced (14, 10).removeFromTop (18), juce::Justification::centredLeft);
    }
    g.setColour (col::muted); g.setFont (font (12.0f));
    g.drawFittedText ("Put it on a guitar, a vocal, the drums or a whole group. BACKWARDS follows Live's tempo. Switch it off and on from Bandleader's band list.",
                      getLocalBounds().removeFromBottom (34).reduced (18, 4), juce::Justification::centredLeft, 2);
}

void SwirlEditor::resized()
{
    auto r = getLocalBounds();
    auto head = r.removeFromTop (60);
    presetBox.setBounds (head.removeFromRight (240).reduced (14, 16));
    mixK->setBounds (head.removeFromRight (80).reduced (2)); outK->setBounds (head.removeFromRight (80).reduced (2));
    r.reduce (18, 12); r.removeFromBottom (30);
    scope = r.removeFromTop (110); r.removeFromTop (12);
    const int w = (r.getWidth() - 36) / 4;
    for (int i = 0; i < 4; ++i) { panel[i] = r.removeFromLeft (w); r.removeFromLeft (12); }
    auto lay = [] (juce::Rectangle<int> p, std::initializer_list<juce::Component*> cs)
    {
        p = p.reduced (10, 10); p.removeFromTop (26);
        const int h = juce::jmin (96, p.getHeight() / 2);
        int i = 0; auto row = p.removeFromTop (h);
        for (auto* c : cs)
        {
            if (i == 2) row = p.removeFromTop (h);
            auto cell = row.removeFromLeft ((p.getWidth()) / 2);
            if (dynamic_cast<juce::ComboBox*> (c)) c->setBounds (cell.withSizeKeepingCentre (cell.getWidth() - 8, 24)); else c->setBounds (cell);
            ++i;
        }
    };
    lay (panel[0], { backK, lenBox });
    lay (panel[1], { phaseK, phRateK, phFbK });
    lay (panel[2], { rotK, speedBox });
    lay (panel[3], { flK, flRateK, flFbK });
}
