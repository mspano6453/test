#pragma once
#include "LoonProcessor.h"
#include "../Source/UiKit.h"
#include <map>

class LoonEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LoonEditor (LoonProcessor&);
    ~LoonEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;
    void timerCallback() override;
    void showPage (int);
    ttui::Knob* knob (const char* id, const char* label, int page);
    juce::ComboBox* combo (const char* id, int page);
    juce::ToggleButton* toggle (const char* id, const char* label, int page);
    void layoutRow (juce::Rectangle<int> r, const std::vector<juce::Component*>& comps, int w);

    ttui::LookAndFeel lnf;
    LoonProcessor& proc;
    juce::TextButton tabs[4];
    juce::ComboBox sceneBox;
    int page = 0;

    juce::OwnedArray<juce::Component> owned;
    juce::OwnedArray<APVTS::ComboBoxAttachment> cAtt;
    juce::OwnedArray<APVTS::ButtonAttachment> bAtt;
    std::vector<juce::Component*> pageComps[4];
    std::map<juce::String, juce::Component*> byId;

    juce::TextButton freezeButton { "FREEZE" };
    std::unique_ptr<APVTS::ButtonAttachment> freezeAtt;

    struct Bubble { float x, y, r, life; int kind; };
    std::vector<Bubble> bubbles;
    uint32_t lastCounter = 0;
    float inputShown = 0;
    juce::Rectangle<int> sky, panelA, panelB, panelC, meterArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoonEditor)
};
