#pragma once
#include "BandleaderProcessor.h"
#include "../Source/UiKit.h"

class BandleaderEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BandleaderEditor (BandleaderProcessor&);
    ~BandleaderEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    juce::String lastStatic; int64_t lastDyn = 0; int tick = 0;
    void updateButtons();
    juce::ComboBox* combo (const char* id, const char* label);
    void paintForm (juce::Graphics&, juce::Rectangle<int>);
    void paintLearn (juce::Graphics&, juce::Rectangle<int>);
    void paintRender (juce::Graphics&, juce::Rectangle<int>);

    ttui::LookAndFeel lnf;
    BandleaderProcessor& proc;
    juce::OwnedArray<juce::Component> owned;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cAtt;
    std::vector<std::pair<juce::ComboBox*, juce::String>> labelled;
    juce::TextButton nextB { "NEXT SECTION" }, fillB { "FILL" }, dropB { "DROP OUT" }, allInB { "ALL IN" };
    juce::TextButton endB { "END THE SONG" }, halfB { "HALF-TIME" }, keyUpB { "KEY UP" };
    juce::TextButton secB[3];
    juce::TextButton learnB { "LEARN THE SONG" }, learnNextB { "NEXT PART" }, learnCancelB { "CANCEL" };
    juce::TextButton renderB { "RENDER SONG" }, openB { "OPEN FOLDER" };
    juce::ComboBox* progBox[3] {};
    juce::ComboBox* keyBox = nullptr;
    juce::ComboBox styleBox;
    juce::TextButton applyAgainB { "APPLY AGAIN" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> styleAtt;
    juce::Rectangle<int> styleText;
    ttui::Knob* energyK = nullptr;
    juce::Rectangle<int> nowArea, formArea, timelineArea, learnArea, songArea, bandArea, renderArea, legendArea;

    std::unique_ptr<mc::DragButton> dragMidi;   // drag what it played into Live as a clip
    class SongDrag;
    std::unique_ptr<SongDrag> dragSong;         // drag every rendered part into Live at once
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandleaderEditor)
};
