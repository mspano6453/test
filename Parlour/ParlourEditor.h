#pragma once
#include "ParlourProcessor.h"
#include "../Source/UiKit.h"

class ParlourEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ParlourEditor (ParlourProcessor&);
    ~ParlourEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;
    void timerCallback() override;
    ttui::Knob* knob (const char* id, const char* label, bool small = false);
    juce::ComboBox* combo (const char* id, const char* label);

    ttui::LookAndFeel lnf;
    ParlourProcessor& proc;
    juce::OwnedArray<juce::Component> owned;
    juce::OwnedArray<APVTS::ComboBoxAttachment> cAtt;
    std::vector<std::pair<juce::ComboBox*, juce::String>> labelled;
    juce::TextButton sectionButtons[3];
    juce::ComboBox *instBox = nullptr, *feelBox = nullptr, *followBox = nullptr, *sitBox = nullptr, *regBox = nullptr, *lhBox = nullptr, *stopBox = nullptr;
    juce::ComboBox *keyBox = nullptr, *progBox[3] {}, *chordBarsBox = nullptr, *songBox = nullptr, *secBarsBox = nullptr, *rotorBox = nullptr;
    std::vector<ttui::Knob*> playerKnobs, soundKnobs;
    juce::Rectangle<int> nowArea, keysArea, playerPanel, soundPanel, legendArea;
    juce::String lastStatic; int64_t lastDyn = 0;

    std::unique_ptr<mc::DragButton> dragMidi;   // drag what it played into Live as a clip
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParlourEditor)
};
