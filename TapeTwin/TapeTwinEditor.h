#pragma once
#include "TapeTwinProcessor.h"
#include "../Source/UiKit.h"

class TapeTwinEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TapeTwinEditor (TapeTwinProcessor&);
    ~TapeTwinEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawReel (juce::Graphics&, juce::Point<float> c, float r, float angle);

    ttui::LookAndFeel lnf;
    TapeTwinProcessor& proc;
    juce::ComboBox presetBox, syncBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> syncAtt;
    std::unique_ptr<ttui::Knob> knobs[tw::NumParams];
    juce::Rectangle<int> deck, groups[4];
    float reelAngle = 0.0f, echoGlow = 0.0f, wowPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeTwinEditor)
};
