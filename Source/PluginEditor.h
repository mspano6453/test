#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "UiKit.h"

namespace ttui
{
// ---------------------------------------------------------------------------
class KitView : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit KitView (TeaTowelProcessor&);
    std::function<void (int)> onSelect;
    std::function<void()> onSampleChanged;
    void setSelected (int p) { selected = p; repaint(); }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void fileDragMove (const juce::StringArray&, int x, int y) override;
    void fileDragExit (const juce::StringArray&) override { dropTarget = -1; repaint(); }
    void filesDropped (const juce::StringArray&, int x, int y) override;

private:
    void timerCallback() override;
    int pieceAt (juce::Point<float>) const;
    float damp (int p) const;
    void drawCymbal (juce::Graphics&, juce::Rectangle<float> box, float relH, float standBottom);
    void drawDrum (juce::Graphics&, juce::Rectangle<float> shell, float headH, float towel, bool snare);
    void drawKick (juce::Graphics&, juce::Point<float> c, float r, float towel);

    TeaTowelProcessor& proc;
    int selected = 0, hover = -1, dropTarget = -1;
    float flash[tt::NumPieces] {};
    juce::Rectangle<float> box[tt::NumPieces];
    juce::Point<float> kickCentre;
    float kickR = 0, floorY = 0;
};

// ---------------------------------------------------------------------------
class MixerView : public juce::Component
{
public:
    MixerView (TeaTowelProcessor&, juce::Component* popupParent);
    std::function<void (int)> onSelect;
    void setSelected (int p);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    struct Strip
    {
        std::unique_ptr<juce::TextButton> name;
        std::unique_ptr<Knob> pan, room;
        std::unique_ptr<juce::Slider> fader;
        std::unique_ptr<SliderAttachment> faderAtt;
    };
    Strip strips[tt::NumPieces];
    int selected = 0;
};

// ---------------------------------------------------------------------------
class EditView : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    EditView (TeaTowelProcessor&, juce::Component* popupParent);
    std::function<void (int)> onSelect;
    void setPiece (int p);
    void refreshSample();
    void paint (juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray&, int, int) override;
private:
    TeaTowelProcessor& proc;
    int piece = 0;
    std::unique_ptr<juce::TextButton> pieceButtons[tt::NumPieces];
    std::unique_ptr<Knob> knobs[tt::NumPieceParams];
    juce::TextButton loadButton { "Load sample..." }, clearButton { "Use built-in" }, playButton { "Play" };
    juce::Label sampleLabel;
    std::unique_ptr<juce::FileChooser> chooser;
};

// ---------------------------------------------------------------------------
class FxView : public juce::Component, private juce::Timer
{
public:
    FxView (TeaTowelProcessor&, juce::Component* popupParent);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    TeaTowelProcessor& proc;
    std::unique_ptr<Knob> knobs[tt::NumMasterParams];
    juce::ComboBox kitTypeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> kitTypeAtt;
    juce::Rectangle<int> meterArea;
    float grShown = 0;
};
// ---------------------------------------------------------------------------
class PlayerView : public juce::Component, private juce::Timer
{
public:
    PlayerView (TeaTowelProcessor&, juce::Component* popupParent);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void setSection (int s);
    TeaTowelProcessor& proc;
    juce::ToggleButton onButton { "Play along with Live's transport" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAtt;
    juce::ComboBox styleBox, fillBox, swellBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> styleAtt, fillAtt, swellAtt;
    juce::TextButton sectionButtons[tt::NumSections];
    juce::TextButton fillButton { "FILL" }, endingButton { "ENDING" };
    std::unique_ptr<Knob> intensity, looseness, swing, variation;
    juce::Rectangle<int> lightsArea, statusArea;
    int shownStep = -2, shownSection = -1;
};
} // namespace ttui

// ---------------------------------------------------------------------------
class TeaTowelEditor : public juce::AudioProcessorEditor
{
public:
    explicit TeaTowelEditor (TeaTowelProcessor&);
    ~TeaTowelEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void showPage (int page);
    void select (int piece);

    ttui::LookAndFeel lnf;
    TeaTowelProcessor& proc;
    juce::TextButton tabKit { "KIT" }, tabEdit { "EDIT" }, tabFx { "FX" }, tabPlayer { "PLAYER" };
    juce::ComboBox presetBox;
    ttui::KitView kit;
    ttui::MixerView mixer;
    ttui::EditView edit;
    ttui::FxView fx;
    ttui::PlayerView player;
    int page = 0;

    std::unique_ptr<mc::DragButton> dragMidi;   // drag what it played into Live as a clip
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TeaTowelEditor)
};
