#pragma once
#include "HarmoniaProcessor.h"
#include "../Source/UiKit.h"

class HarmoniaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit HarmoniaEditor (HarmoniaProcessor&);
    ~HarmoniaEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;
    void timerCallback() override;
    ttui::Knob* knob (const char* id, const char* label, bool small = false);
    juce::ComboBox* combo (const char* id, const char* label);

    ttui::LookAndFeel lnf;
    HarmoniaProcessor& proc;
    juce::OwnedArray<juce::Component> owned;
    juce::OwnedArray<APVTS::ComboBoxAttachment> cAtt;
    std::vector<std::pair<juce::ComboBox*, juce::String>> labelled;
    juce::ComboBox *groupBox = nullptr, *feelBox = nullptr, *followBox = nullptr, *sitBox = nullptr, *partsBox = nullptr, *regBox = nullptr;
    juce::ComboBox *keyBox = nullptr, *progBox[3] {}, *chordBarsBox = nullptr, *songBox = nullptr, *secBarsBox = nullptr, *secBox = nullptr;
    std::vector<ttui::Knob*> singKnobs, soundKnobs;
    juce::Rectangle<int> nowArea, stackArea, singPanel, soundPanel, songPanel, legendArea;
    juce::String lastStatic; int64_t lastDyn = 0;
    std::unique_ptr<mc::DragButton> dragMidi;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmoniaEditor)
};
