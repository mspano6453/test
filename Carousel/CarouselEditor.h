#pragma once
#include "CarouselProcessor.h"
#include "../Source/UiKit.h"

class CarouselEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit CarouselEditor (CarouselProcessor&);
    ~CarouselEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;
    void timerCallback() override;
    ttui::Knob* knob (const char* id, const char* label);
    juce::ComboBox* combo (const char* id);
    juce::TextButton* toggleButton (const char* id, const char* label);

    ttui::LookAndFeel lnf;
    CarouselProcessor& proc;

    juce::TextButton recButton { "REC / PLAY / DUB" }, stopButton { "STOP" }, undoButton { "UNDO" }, redoButton { "REDO" };
    juce::TextButton slotButtons[cr::kSlots];
    juce::TextButton clearButton { "Clear loop" }, clearAllButton { "Clear all" }, exportButton { "Export loop to WAV" };
    juce::OwnedArray<juce::Component> owned;
    juce::OwnedArray<APVTS::ComboBoxAttachment> cAtt;
    juce::OwnedArray<APVTS::ButtonAttachment> bAtt;
    ttui::Knob *levelK = nullptr, *dryK = nullptr, *decayK = nullptr;
    juce::TextButton *revB = nullptr, *halfB = nullptr, *syncB = nullptr;
    juce::ComboBox *stopBox = nullptr, *playBox = nullptr, *snapBox = nullptr;

    juce::Rectangle<int> ringArea, sidePanel, memArea, legendArea, lastExportArea;
    juce::String lastExport;
    float inShown = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CarouselEditor)
};
