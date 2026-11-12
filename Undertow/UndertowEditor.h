#pragma once
#include "UndertowProcessor.h"
#include "../Source/UiKit.h"

class UndertowEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit UndertowEditor (UndertowProcessor&);
    ~UndertowEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;
    void timerCallback() override;
    ttui::Knob* knob (const char* id, const char* label, bool small = false);
    juce::ComboBox* combo (const char* id);

    ttui::LookAndFeel lnf;
    UndertowProcessor& proc;

    juce::OwnedArray<juce::Component> owned;
    juce::OwnedArray<APVTS::ComboBoxAttachment> cAtt;
    juce::OwnedArray<APVTS::ButtonAttachment> bAtt;
    juce::TextButton sectionButtons[3], fillButton { "FILL" }, newLineButton { "NEW LINE" }, brightButton { "BRIGHT" };
    juce::ComboBox *modeBox = nullptr, *followBox = nullptr, *feelBox = nullptr, *regBox = nullptr, *octBox = nullptr, *stopBox = nullptr, *typeBox = nullptr, *sitBox = nullptr;
    juce::ComboBox *keyBox = nullptr, *progBox[3] {}, *chordBarsBox = nullptr, *songBox = nullptr, *secBarsBox = nullptr;
    std::vector<ttui::Knob*> playerKnobs, rigKnobs, guitarKnobs;
    ttui::Knob* bubbleKnob = nullptr;
    juce::Rectangle<int> bubbleArea;

    juce::Rectangle<int> earArea, rollArea, playerPanel, rigPanel, legendArea, guitarArea;
    float inShown = 0;
    int fillFlash = 0;
    juce::String lastStatic; int64_t lastDyn = 0;

    std::unique_ptr<mc::DragButton> dragMidi;   // drag what it played into Live as a clip
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UndertowEditor)
};
