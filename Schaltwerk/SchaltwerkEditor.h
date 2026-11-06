#pragma once
#include "SchaltwerkProcessor.h"
#include "../Source/UiKit.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace swui
{
using APVTS = juce::AudioProcessorValueTreeState;

class StepGrid : public juce::Component
{
public:
    explicit StepGrid (SchaltwerkProcessor& p) : proc (p) {}
    std::function<void (int row)> onSelectRow;
    int selectedRow = 0;
    float flash[sw::NumDrums] {};
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
private:
    juce::Rectangle<float> cell (int row, int step) const;
    int labelW() const { return 110; }
    SchaltwerkProcessor& proc;
    int paintValue = -1;
};

class ChordPad : public juce::Component
{
public:
    explicit ChordPad (SchaltwerkProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    juce::Rectangle<float> button (int type, int root) const;
    SchaltwerkProcessor& proc;
    int downType = -1, downRoot = -1;
};

class StrumPlate : public juce::Component
{
public:
    explicit StrumPlate (SchaltwerkProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override { mouseDrag (e); }
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { proc.uiStrum = -1; }
private:
    SchaltwerkProcessor& proc;
};
} // namespace swui

class SchaltwerkEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SchaltwerkEditor (SchaltwerkProcessor&);
    ~SchaltwerkEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void showPage (int p);
    void selectRow (int row);
    ttui::Knob* knob (const juce::String& id, const juce::String& label, bool small = false);
    juce::ComboBox* combo (const juce::String& id);
    juce::ToggleButton* toggle (const juce::String& id, const juce::String& label);
    void placeRow (juce::Rectangle<int> area, std::initializer_list<juce::Component*> comps);

    ttui::LookAndFeel lnf;
    SchaltwerkProcessor& proc;
    juce::TextButton tabRhythm { "RHYTHM" }, tabRobot { "ROBOT" }, tabOmni { "OMNICHORD" };
    int page = 0;

    juce::OwnedArray<ttui::Knob> knobs;
    juce::OwnedArray<juce::ComboBox> combos;
    juce::OwnedArray<juce::ToggleButton> toggles;
    juce::OwnedArray<swui::APVTS::ComboBoxAttachment> comboAtts;
    juce::OwnedArray<swui::APVTS::ButtonAttachment> toggleAtts;

    // rhythm page
    swui::StepGrid grid;
    juce::ComboBox patternBox;
    juce::TextButton clearButton { "Clear" };
    std::unique_ptr<ttui::Knob> rowKnobs[5];
    juce::ToggleButton* seqToggle = nullptr;
    ttui::Knob *swingKnob = nullptr, *drumSendKnob = nullptr;
    std::vector<juce::Component*> rhythmComps, robotComps, omniComps, sharedComps;

    // robot page
    juce::MidiKeyboardComponent keyboard;
    juce::ComboBox* modeBox = nullptr;
    std::vector<ttui::Knob*> robotKnobs;

    // omnichord page
    swui::ChordPad chordPad;
    swui::StrumPlate strum;
    std::vector<juce::Component*> omniCtl;

    // shared bottom bar
    std::vector<juce::Component*> echoCtl;

    juce::Rectangle<int> rowPanel, robotPanel, omniPanel, bottomBar, helpArea, rhythmHelp, robotHelp, omniHelp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SchaltwerkEditor)
};
