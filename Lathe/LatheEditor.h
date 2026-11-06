#pragma once
#include "LatheProcessor.h"
#include "../Source/UiKit.h"

namespace ltui
{
using APVTS = juce::AudioProcessorValueTreeState;

// ---------------------------------------------------------------------------
// Module chain: click to select, click the dot to bypass, drag to reorder
// ---------------------------------------------------------------------------
class ChainView : public juce::Component
{
public:
    explicit ChainView (LatheProcessor&);
    std::function<void (int module)> onSelect;
    int selected = lt::MEq;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    juce::Rectangle<float> tileRect (int slot) const;
    juce::Rectangle<float> dotRect (int slot) const;
    LatheProcessor& proc;
    int dragSlot = -1, dragTo = -1;
    float dragY = 0;
};

// ---------------------------------------------------------------------------
// Generic module page: columns of knobs, menus and switches
// ---------------------------------------------------------------------------
struct Ctl { enum Kind { Knob, Combo, Toggle } kind; juce::String id, label; };
struct Group { juce::String title; std::vector<Ctl> ctls; };

class GenericPanel : public juce::Component
{
public:
    GenericPanel (LatheProcessor&, juce::String title, juce::String blurb, std::vector<Group>, int extraWidth, juce::Component* popupParent);
    std::function<juce::String (int group)> groupSuffix;
    std::function<void (juce::Graphics&, juce::Rectangle<int>)> paintExtra;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    LatheProcessor& proc;
    juce::String title, blurb;
    std::vector<Group> groups;
    int extraWidth;
    juce::OwnedArray<juce::Component> owned;
    juce::OwnedArray<APVTS::SliderAttachment> sAtt;
    juce::OwnedArray<APVTS::ComboBoxAttachment> cAtt;
    juce::OwnedArray<APVTS::ButtonAttachment> bAtt;
    std::vector<std::vector<juce::Component*>> groupComps;
    std::vector<juce::Rectangle<int>> groupAreas;
    juce::Rectangle<int> extraArea;
};

// ---------------------------------------------------------------------------
// Equalizer page: live spectrum + curve with draggable band nodes
// ---------------------------------------------------------------------------
class EqPanel : public juce::Component
{
public:
    EqPanel (LatheProcessor&, juce::Component* popupParent);
    void setSpectrum (const std::vector<float>& db) { spectrum = db; }
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
private:
    void selectBand (int b);
    float xForFreq (float f) const;
    float freqForX (float x) const;
    float yForDb (float db) const;
    float dbForY (float y) const;
    juce::Point<float> nodePos (int b) const;
    float pv (const juce::String& id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    void setP (const juce::String& id, float v);

    LatheProcessor& proc;
    juce::Rectangle<int> graph;
    std::vector<float> spectrum;   // dB per FFT bin
    int band = lt::EqBell2, dragBand = -1;
    juce::TextButton bandButtons[lt::NumEqBands];
    juce::ToggleButton onToggle { "Band on" }, steepToggle { "24 dB/oct cuts" };
    std::unique_ptr<ttui::Knob> freq, gain, q;
    std::unique_ptr<APVTS::ButtonAttachment> onAtt, steepAtt;
};

// ---------------------------------------------------------------------------
class Meters : public juce::Component
{
public:
    Meters (LatheProcessor&, juce::Component* popupParent);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();
private:
    LatheProcessor& proc;
    float inPk = 0, outPk = 0, tpHold = 0;
    juce::TextButton resetButton { "Reset" };
    std::unique_ptr<ttui::Knob> inGain, outGain;
    juce::ComboBox dither;
    std::unique_ptr<APVTS::ComboBoxAttachment> ditherAtt;
};
} // namespace ltui

class LatheEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LatheEditor (LatheProcessor&);
    ~LatheEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void select (int module);
    void computeSpectrum();

    ttui::LookAndFeel lnf;
    LatheProcessor& proc;
    juce::ComboBox presetBox, targetBox;
    juce::TextButton assistButton { "ASSISTANT" };
    ltui::ChainView chain;
    ltui::Meters meters;
    std::unique_ptr<ltui::EqPanel> eqPanel;
    std::unique_ptr<ltui::GenericPanel> panels[lt::NumModules];   // EQ slot unused
    juce::Rectangle<int> statusArea;

    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> ring, fftData, smooth, scope;
    int ringPos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LatheEditor)
};
