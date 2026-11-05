#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

// Shared look for Tea Towel and Tape Twin: 60s studio console - charcoal, cream, amber
namespace ttui
{
namespace col
{
    const juce::Colour bg     { 0xff1b1916 };
    const juce::Colour panel  { 0xff25221e };
    const juce::Colour panel2 { 0xff302b25 };
    const juce::Colour line   { 0xff3d3731 };
    const juce::Colour cream  { 0xffe9dfc7 };
    const juce::Colour amber  { 0xffe0a33a };
    const juce::Colour muted  { 0xff8c8273 };
    const juce::Colour red    { 0xffb5452f };
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos,
                           float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
};

using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

class Knob : public juce::Component
{
public:
    Knob (const juce::String& name, bool isSmall, juce::Component* popupParent);
    void attach (juce::AudioProcessorValueTreeState& s, const juce::String& id);
    void resized() override;
    juce::Slider slider;
    juce::Label label;
private:
    bool small;
    std::unique_ptr<SliderAttachment> att;
};

juce::Font font (float size, bool bold = false);
} // namespace ttui
