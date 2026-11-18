#pragma once
#include "FuzzboxProcessor.h"
#include "../Source/UiKit.h"

class FuzzboxEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FuzzboxEditor (FuzzboxProcessor&);
    ~FuzzboxEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    using APVTS = juce::AudioProcessorValueTreeState;
    void timerCallback() override { repaint(); }
    ttui::Knob* knob (const char* id, const char* label);
    juce::ComboBox* combo (const char* id);
    ttui::LookAndFeel lnf;
    FuzzboxProcessor& proc;
    juce::OwnedArray<juce::Component> owned;
    juce::OwnedArray<APVTS::ComboBoxAttachment> cAtt;
    juce::OwnedArray<APVTS::ButtonAttachment> bAtt;
    juce::ComboBox presetBox;
    juce::TextButton stomp[FuzzboxProcessor::NumPedals];
    std::vector<juce::Component*> parts[FuzzboxProcessor::NumPedals];
    ttui::Knob* outK = nullptr;
    juce::Rectangle<int> box[FuzzboxProcessor::NumPedals];
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FuzzboxEditor)
};
