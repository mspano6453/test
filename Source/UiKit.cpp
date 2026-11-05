#include "UiKit.h"

using namespace ttui;

juce::Font ttui::font (float size, bool bold)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

// ===========================================================================
// LookAndFeel: 60s studio console - charcoal, cream, amber
// ===========================================================================
ttui::LookAndFeel::LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, col::bg);
    setColour (juce::Label::textColourId, col::cream);
    setColour (juce::TextButton::buttonColourId, col::panel2);
    setColour (juce::TextButton::buttonOnColourId, col::amber);
    setColour (juce::TextButton::textColourOffId, col::cream);
    setColour (juce::TextButton::textColourOnId, col::bg);
    setColour (juce::ComboBox::backgroundColourId, col::panel2);
    setColour (juce::ComboBox::textColourId, col::cream);
    setColour (juce::ComboBox::outlineColourId, col::line);
    setColour (juce::ComboBox::arrowColourId, col::amber);
    setColour (juce::PopupMenu::backgroundColourId, col::panel);
    setColour (juce::PopupMenu::textColourId, col::cream);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::amber);
    setColour (juce::PopupMenu::highlightedTextColourId, col::bg);
    setColour (juce::BubbleComponent::backgroundColourId, col::panel2);
    setColour (juce::BubbleComponent::outlineColourId, col::amber);
    setColour (juce::TooltipWindow::textColourId, col::cream);
}

void ttui::LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float start, float end, juce::Slider& s)
{
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float arcW = juce::jmax (2.0f, r * 0.09f);
    const float angle = start + pos * (end - start);

    juce::Path track;
    track.addCentredArc (c.x, c.y, r - arcW, r - arcW, 0.0f, start, end, true);
    g.setColour (col::line);
    g.strokePath (track, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const float from = bipolar ? (start + end) * 0.5f : start;
    juce::Path val;
    val.addCentredArc (c.x, c.y, r - arcW, r - arcW, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setColour (col::amber);
    g.strokePath (val, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Bakelite knob
    const float kr = (r - arcW) * 0.74f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a443d), c.x, c.y - kr,
                                             juce::Colour (0xff141210), c.x, c.y + kr, false));
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2, 1.0f);

    juce::Path ptr;
    ptr.addRoundedRectangle (-1.5f, -kr * 0.92f, 3.0f, kr * 0.55f, 1.5f);
    g.setColour (col::cream);
    g.fillPath (ptr, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

void ttui::LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float minPos, float maxPos, juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, minPos, maxPos, style, s);
        return;
    }
    const float cx = (float) x + (float) w * 0.5f;
    const auto track = juce::Rectangle<float> (cx - 2.5f, (float) y + 6.0f, 5.0f, (float) h - 12.0f);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (track, 2.5f);
    g.setColour (col::amber.withAlpha (0.85f));
    g.fillRoundedRectangle (track.withTop (pos), 2.5f);

    // tick marks
    g.setColour (col::muted.withAlpha (0.5f));
    for (int i = 0; i <= 10; ++i)
    {
        const float ty = track.getY() + track.getHeight() * (float) i / 10.0f;
        g.drawHorizontalLine ((int) ty, cx - 12.0f, cx - 6.0f);
        g.drawHorizontalLine ((int) ty, cx + 6.0f, cx + 12.0f);
    }

    const auto cap = juce::Rectangle<float> (cx - 13.0f, pos - 8.0f, 26.0f, 16.0f);
    g.setGradientFill (juce::ColourGradient (col::cream, cap.getX(), cap.getY(),
                                             juce::Colour (0xffb9ae95), cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 3.0f);
    g.setColour (col::bg);
    g.drawHorizontalLine ((int) pos, cap.getX() + 3.0f, cap.getRight() - 3.0f);
}

void ttui::LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                             bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    juce::Colour c = b.getToggleState() ? col::amber : col::panel2;
    if (over) c = c.brighter (0.12f);
    if (down) c = c.darker (0.15f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (b.getToggleState() ? col::amber.brighter (0.3f) : col::line);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

// ===========================================================================
Knob::Knob (const juce::String& name, bool isSmall, juce::Component* popupParent) : small (isSmall)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setPopupDisplayEnabled (true, true, popupParent);
    slider.setMouseDragSensitivity (180);
    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (font (small ? 11.0f : 13.0f, ! small));
    label.setColour (juce::Label::textColourId, small ? col::muted : col::cream);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (slider);
    addAndMakeVisible (label);
}

void Knob::attach (juce::AudioProcessorValueTreeState& s, const juce::String& id)
{
    att.reset();
    att = std::make_unique<SliderAttachment> (s, id, slider);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromBottom (small ? 14 : 20));
    slider.setBounds (r);
}

