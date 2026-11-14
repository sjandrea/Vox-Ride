#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace vpcol
{
    const juce::Colour bg        { 0xff0f1115 };
    const juce::Colour panel     { 0xff181b21 };
    const juce::Colour panelEdge { 0xff262a33 };
    const juce::Colour well      { 0xff0b0d10 };
    const juce::Colour text      { 0xffc9ced6 };
    const juce::Colour textDim   { 0xff6d7480 };
    const juce::Colour accent    { 0xff3fd0c9 }; // cyan  – output / target
    const juce::Colour gain      { 0xffffa53d }; // amber – rider gain
    const juce::Colour vocal     { 0xff5a6b8c }; // slate – raw vocal level
    const juce::Colour detect    { 0xffe0566b }; // red   – detection threshold
    const juce::Colour music     { 0xffa98bff }; // violet – side-chain
    const juce::Colour writeCol  { 0xffd94a4a };
    const juce::Colour readCol   { 0xff3fbf6a };
}

//==============================================================================
class VPLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VPLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int h) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

    static void drawFaderCap (juce::Graphics&, juce::Rectangle<float> r, juce::Colour stripe);
};

//==============================================================================
/** Scrolling graph: vocal level, output level, target, threshold and rider gain. */
class HistoryView : public juce::Component
{
public:
    explicit HistoryView (VoxPilotProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void pull();

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    float levelToY (float db) const;
    float yToLevel (float y) const;
    float gainToY (float db) const;
    juce::Rectangle<float> plotArea() const;

    VoxPilotProcessor& proc;
    static constexpr int capacity = 600; // 10 s at 60 fps
    std::vector<HistoryFrame> frames;
    int writePos = 0;
    std::array<HistoryFrame, 256> scratch {};
    bool draggingTarget = false;

    static constexpr float minDb = -66.0f, maxDb = 0.0f;
};

//==============================================================================
/** Displays the applied rider gain; draggable (writes "Rider Gain") only in Read mode. */
class RiderFader : public juce::Component
{
public:
    explicit RiderFader (VoxPilotProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void setDisplayedGain (float db, bool interactive);

private:
    float valueToY (float db) const;
    float yToValue (float y) const;
    juce::Rectangle<float> track() const;

    VoxPilotProcessor& proc;
    juce::ParameterAttachment attachment;
    float shownDb = 0.0f;
    bool canDrag = false, dragging = false;
};

//==============================================================================
class LevelMeter : public juce::Component, public juce::SettableTooltipClient
{
public:
    void setLevel (float db) { if (std::abs (db - level) > 0.05f) { level = db; repaint(); } }
    void paint (juce::Graphics&) override;
    juce::Colour colour = vpcol::accent;
private:
    float level = -100.0f;
};

//==============================================================================
class VoxPilotEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VoxPilotEditor (VoxPilotProcessor&);
    ~VoxPilotEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setupKnob (juce::Slider&, juce::Label&, const juce::String& name, const char* paramId);
    void setChoice (const char* id, float normalised);
    void drawPanel (juce::Graphics&, juce::Rectangle<int>, const juce::String& title);

    VoxPilotProcessor& proc;
    VPLookAndFeel lnf;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    juce::Slider targetKnob, detectKnob, bleedKnob, musicKnob;
    juce::Label  targetLabel, detectLabel, bleedLabel, musicLabel;
    std::unique_ptr<SliderAttachment> targetAtt, detectAtt, bleedAtt, musicAtt;

    juce::Slider rangeSlider { juce::Slider::TwoValueVertical, juce::Slider::NoTextBox };
    juce::ParameterAttachment rangeLowAtt, rangeHighAtt;
    juce::Label rangeLabel, rangeValue;

    RiderFader riderFader;
    juce::Label riderLabel, riderValue;

    juce::Slider outputSlider;
    std::unique_ptr<SliderAttachment> outputAtt;
    juce::Label outputLabel;
    LevelMeter outMeter, inMeter;

    juce::TextButton fastBtn { "FAST" }, slowBtn { "SLOW" };
    juce::TextButton offBtn { "OFF" }, writeBtn { "WRITE" }, readBtn { "READ" };
    std::array<juce::TextButton, 4> lookaheadBtns;

    HistoryView history;
    juce::TooltipWindow tooltips { this, 600 };

    bool rideLed = false, scLed = false;
    float scLevel = -100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxPilotEditor)
};
