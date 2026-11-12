#pragma once
#include "EtherProcessor.h"
#include "../Source/UiKit.h"

class EtherEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit EtherEditor (EtherProcessor&);
    ~EtherEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;
    void timerCallback() override;
    ttui::Knob* knob (const char* id, const char* label, bool small = false);
    juce::ComboBox* combo (const char* id, const char* label);

    ttui::LookAndFeel lnf;
    EtherProcessor& proc;
    juce::OwnedArray<juce::Component> owned;
    juce::OwnedArray<APVTS::ComboBoxAttachment> cAtt;
    std::vector<std::pair<juce::ComboBox*, juce::String>> labelled;
    juce::ComboBox *instBox = nullptr, *roleBox = nullptr, *followBox = nullptr, *sitBox = nullptr, *regBox = nullptr;
    juce::ComboBox *keyBox = nullptr, *progBox[3] {}, *chordBarsBox = nullptr, *songBox = nullptr, *secBarsBox = nullptr;
    std::vector<ttui::Knob*> playerKnobs, soundKnobs;
    juce::Rectangle<int> scopeArea, playerPanel, soundPanel, legendArea;
    float phase = 0, levelShown = 0;
    juce::String lastStatic;

    std::unique_ptr<mc::DragButton> dragMidi;   // drag what it played into Live as a clip
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EtherEditor)
};
