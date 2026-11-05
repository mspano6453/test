#include "TapeTwinEditor.h"

using namespace ttui;
using namespace tw;

namespace
{
const char* knobLabel (int p)
{
    static const char* n[] = { "DRIVE", "TIME", "SYNC", "FEEDBACK", "WOW", "FLUTTER", "AGE", "SPREAD",
                               "ECHO MIX", "DOUBLE", "TIME", "WOBBLE", "OUTPUT" };
    return n[p];
}
}

TapeTwinEditor::TapeTwinEditor (TapeTwinProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    for (int i = 0; i < NumParams; ++i)
    {
        if (i == Sync) continue;
        knobs[i] = std::make_unique<Knob> (knobLabel (i), false, this);
        knobs[i]->attach (p.apvts, paramId (i));
        addAndMakeVisible (*knobs[i]);
    }
    syncBox.addItemList (syncNames(), 1);
    syncAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, paramId (Sync), syncBox);
    addAndMakeVisible (syncBox);

    for (int i = 0; i < p.getNumPrograms(); ++i) presetBox.addItem (p.getProgramName (i), i + 1);
    presetBox.setSelectedId (p.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    setSize (900, 600);
    startTimerHz (30);
}

TapeTwinEditor::~TapeTwinEditor() { setLookAndFeel (nullptr); }

void TapeTwinEditor::timerCallback()
{
    const float wow = proc.apvts.getRawParameterValue (paramId (Wow))->load();
    wowPhase += 0.1f;
    reelAngle += 0.06f * (1.0f + 0.25f * wow * std::sin (wowPhase));
    echoGlow = juce::jmax (proc.echoLevel.load() * 3.0f, echoGlow * 0.85f);
    knobs[Time]->setEnabled ((int) proc.apvts.getRawParameterValue (paramId (Sync))->load() == 0);
    repaint (deck);
}

void TapeTwinEditor::drawReel (juce::Graphics& g, juce::Point<float> c, float r, float angle)
{
    g.setColour (juce::Colour (0xff5a4634));            // tape pack
    g.fillEllipse (c.x - r * 0.78f, c.y - r * 0.78f, r * 1.56f, r * 1.56f);
    g.setColour (juce::Colour (0xffb9bec4));            // metal flange
    g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 2.5f);
    g.setColour (juce::Colour (0xff2a2622));
    for (int k = 0; k < 3; ++k)
    {
        const float a = angle + juce::MathConstants<float>::twoPi * (float) k / 3.0f;
        juce::Path hole;
        hole.addPieSegment (c.x - r * 0.62f, c.y - r * 0.62f, r * 1.24f, r * 1.24f, a, a + 0.75f, 0.35f);
        g.fillPath (hole);
    }
    g.setColour (juce::Colour (0xffd2d5d9));
    g.fillEllipse (c.x - r * 0.16f, c.y - r * 0.16f, r * 0.32f, r * 0.32f);
}

void TapeTwinEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    auto header = getLocalBounds().removeFromTop (56);
    g.setColour (col::panel);
    g.fillRect (header);
    g.setColour (col::red);
    g.fillRect (header.removeFromBottom (2));
    g.setColour (col::amber);
    g.setFont (font (26.0f, true));
    g.drawText ("TAPE TWIN", 20, 8, 260, 28, juce::Justification::centredLeft);
    g.setColour (col::muted);
    g.setFont (font (11.0f));
    g.drawText ("TAPE ECHO + DOUBLE TRACKER", 21, 34, 260, 14, juce::Justification::centredLeft);

    // tape deck
    g.setColour (col::panel);
    g.fillRoundedRectangle (deck.toFloat(), 8.0f);
    const float r = (float) deck.getHeight() * 0.38f;
    const auto lc = juce::Point<float> ((float) deck.getX() + deck.getWidth() * 0.22f, (float) deck.getCentreY() - 6.0f);
    const auto rc = juce::Point<float> ((float) deck.getX() + deck.getWidth() * 0.78f, (float) deck.getCentreY() - 6.0f);
    // tape path past the heads
    const float headY = (float) deck.getBottom() - 22.0f;
    const float headX = (float) deck.getCentreX();
    g.setColour (juce::Colour (0xff3b2e22));
    juce::Path tape;
    tape.startNewSubPath (lc.x, lc.y + r * 0.78f);
    tape.lineTo (headX - 120.0f, headY);
    tape.lineTo (headX + 120.0f, headY);
    tape.lineTo (rc.x, rc.y + r * 0.78f);
    g.strokePath (tape, juce::PathStrokeType (3.0f));
    drawReel (g, lc, r, reelAngle);
    drawReel (g, rc, r, reelAngle * 1.1f);

    // record head + three playback heads (glow with the echoes)
    const char* headNames[] = { "REC", "PB 1", "PB 2", "PB 3" };
    for (int h = 0; h < 4; ++h)
    {
        const float hx = headX - 90.0f + 60.0f * (float) h;
        auto hr = juce::Rectangle<float> (hx - 12.0f, headY - 6.0f, 24.0f, 18.0f);
        g.setColour (juce::Colour (0xff8d9096));
        g.fillRoundedRectangle (hr, 3.0f);
        if (h > 0)
        {
            g.setColour (col::amber.withAlpha (juce::jlimit (0.0f, 1.0f, echoGlow * (1.0f - 0.25f * (float) (h - 1)))));
            g.fillRoundedRectangle (hr.reduced (4.0f), 2.0f);
        }
        g.setColour (col::muted);
        g.setFont (font (9.0f, true));
        g.drawText (headNames[h], hr.translated (0, -16).toNearestInt(), juce::Justification::centred);
    }

    g.setColour (col::cream);
    g.setFont (font (13.0f, true));
    g.drawText (juce::String (juce::roundToInt (proc.currentDelayMs.load())) + " ms",
                deck.withTrimmedTop (10).withHeight (20), juce::Justification::centred);

    // knob groups
    const char* titles[] = { "ECHO", "TAPE", "DOUBLE TRACK (ADT)", "OUTPUT" };
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (col::panel);
        g.fillRoundedRectangle (groups[i].toFloat(), 8.0f);
        g.setColour (col::amber);
        g.setFont (font (14.0f, true));
        g.drawText (titles[i], groups[i].reduced (14, 8).removeFromTop (18), juce::Justification::topLeft);
    }
}

void TapeTwinEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (56).reduced (16, 12);
    presetBox.setBounds (header.removeFromRight (240));

    r.reduce (20, 16);
    deck = r.removeFromTop (190);
    r.removeFromTop (14);

    auto row1 = r.removeFromTop ((r.getHeight() - 14) / 2);
    r.removeFromTop (14);
    auto row2 = r;
    groups[0] = row1.removeFromLeft (row1.getWidth() / 2).withTrimmedRight (7);
    groups[1] = row1.withTrimmedLeft (7);
    groups[2] = row2.removeFromLeft (row2.getWidth() * 60 / 100).withTrimmedRight (7);
    groups[3] = row2.withTrimmedLeft (7);

    auto place = [] (juce::Rectangle<int> area, std::initializer_list<juce::Component*> comps)
    {
        area = area.reduced (10, 8).withTrimmedTop (22);
        const int w = area.getWidth() / (int) comps.size();
        for (auto* c : comps) c->setBounds (area.removeFromLeft (w).reduced (4, 0));
    };
    {
        auto a = groups[0].reduced (10, 8).withTrimmedTop (22);
        const int w = a.getWidth() / 4;
        knobs[Time]->setBounds (a.removeFromLeft (w).reduced (4, 0));
        syncBox.setBounds (a.removeFromLeft (w).reduced (4, 0).withSizeKeepingCentre (w - 8, 26));
        knobs[Feedback]->setBounds (a.removeFromLeft (w).reduced (4, 0));
        knobs[EchoMix]->setBounds (a.reduced (4, 0));
    }
    place (groups[1], { knobs[Wow].get(), knobs[Flutter].get(), knobs[Age].get(), knobs[Drive].get() });
    place (groups[2], { knobs[AdtMix].get(), knobs[AdtTime].get(), knobs[AdtWobble].get() });
    place (groups[3], { knobs[Spread].get(), knobs[Output].get() });
}
