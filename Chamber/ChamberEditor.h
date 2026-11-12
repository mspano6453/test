#pragma once
#include "ChamberProcessor.h"
#include "../Source/UiKit.h"

class ChamberEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ChamberEditor (ChamberProcessor&);
    ~ChamberEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;
    void timerCallback() override;
    ttui::Knob* knob (const char* id, const char* label, bool small = false);
    juce::ComboBox* combo (const char* id, const char* label);

    ttui::LookAndFeel lnf;
    ChamberProcessor& proc;
    juce::OwnedArray<juce::Component> owned;
    juce::OwnedArray<APVTS::ComboBoxAttachment> cAtt;
    std::vector<std::pair<juce::ComboBox*, juce::String>> labelled;
    juce::ComboBox *ensBox = nullptr, *feelBox = nullptr, *followBox = nullptr, *sitBox = nullptr, *regBox = nullptr, *timBox = nullptr, *artBox = nullptr;
    juce::ComboBox *keyBox = nullptr, *progBox[3] {}, *chordBarsBox = nullptr, *songBox = nullptr, *secBarsBox = nullptr;
    std::vector<ttui::Knob*> playerKnobs, soundKnobs;
    std::unique_ptr<mc::DragButton> drag;
    juce::Rectangle<int> scoreArea, playerPanel, soundPanel, legendArea, statusArea;
    juce::String lastStatic, status; int64_t lastDyn = 0; int statusTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChamberEditor)
};
