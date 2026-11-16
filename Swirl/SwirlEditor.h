#pragma once
#include "SwirlProcessor.h"
#include "../Source/UiKit.h"

class SwirlEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SwirlEditor (SwirlProcessor&);
    ~SwirlEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    using APVTS = juce::AudioProcessorValueTreeState;
    void timerCallback() override { repaint (0, 60, getWidth(), 120); }
    ttui::Knob* knob (const char* id, const char* label);
    juce::ComboBox* combo (const char* id);
    ttui::LookAndFeel lnf;
    SwirlProcessor& proc;
    juce::OwnedArray<juce::Component> owned;
    juce::OwnedArray<APVTS::ComboBoxAttachment> cAtt;
    juce::ComboBox presetBox; juce::ComboBox *lenBox = nullptr, *speedBox = nullptr;
    ttui::Knob *backK, *phaseK, *phRateK, *phFbK, *rotK, *flK, *flRateK, *flFbK, *mixK, *outK;
    juce::Rectangle<int> panel[4], scope;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SwirlEditor)
};
