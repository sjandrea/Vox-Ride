#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace vpcol
{
    const juce::Colour bgTop     { 0xff2b3b45 }; // brushed slate metal
    const juce::Colour bgBottom  { 0xff18232a };
    const juce::Colour panelTop  { 0xff2e3f49 };
    const juce::Colour panelBot  { 0xff1c2930 };
    const juce::Colour well      { 0xff0a1115 }; // recessed areas
    const juce::Colour plotTop   { 0xff10252d };
    const juce::Colour plotBot   { 0xff09151a };
    const juce::Colour text      { 0xffe6eaee };
    const juce::Colour textDim   { 0xff8d9aa3 };
    const juce::Colour accent    { 0xff2fe0d8 }; // cyan  – output / target
    const juce::Colour gain      { 0xffffa126 }; // amber – rider gain
    const juce::Colour vocal     { 0xff7d8fae }; // slate – raw vocal level
    const juce::Colour detect    { 0xffff4b5f }; // red   – detection threshold
    const juce::Colour music     { 0xffa77bff }; // violet – side-chain
    const juce::Colour silver    { 0xffd9dde1 };
    const juce::Colour writeCol  { 0xffe0453f };
    const juce::Colour readCol   { 0xff43d26f };
    const juce::Colour darkText  { 0xff0b1114 };
}

/** Shared drawing helpers for the hardware-style look. */
namespace vpdraw
{
    void glowStroke (juce::Graphics&, const juce::Path&, juce::Colour, float width);
    void recess (juce::Graphics&, juce::Rectangle<float>, float radius, juce::Colour fill = vpcol::well);
    void bevelPanel (juce::Graphics&, juce::Rectangle<float>, float radius);
    void screw (juce::Graphics&, juce::Point<float> centre, float radius, float slotAngle);
    void led (juce::Graphics&, juce::Point<float> centre, float radius, juce::Colour, bool on);
    void faderCap (juce::Graphics&, juce::Rectangle<float>, juce::Colour stripe);
    void faderSlot (juce::Graphics&, float cx, float top, float bottom);
    void glowBar (juce::Graphics&, juce::Rectangle<float>, juce::Colour);
    void dbScale (juce::Graphics&, const std::function<float (float)>& valueToY, float cx, bool labels);
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
    int getSliderThumbRadius (juce::Slider&) override { return 12; }
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int h) override;
    juce::Font getLabelFont (juce::Label&) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

    /** Marks a label so it is drawn as a recessed value display. */
    static void makeValueDisplay (juce::Label&);
};

//==============================================================================
/** Scrolling graph: vocal level, output level, target, threshold and rider gain. */
class HistoryView : public juce::Component
{
public:
    explicit HistoryView (VoxPilotProcessor&);
    void paint (juce::Graphics&) override;
    void pull();

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> levelBox() const;
    juce::Rectangle<float> gainBox() const;
    juce::Rectangle<float> levelPlot() const;
    juce::Rectangle<float> gainPlot() const;
    float levelToY (float db) const;
    float yToLevel (float y) const;
    float gainToY (float db) const;

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
    void setupKnob (juce::Slider&, juce::Label&, const juce::String& name);
    void setChoice (const char* id, float normalised);
    void drawPanel (juce::Graphics&, juce::Rectangle<int>, const juce::String& title);
    void buildBackground();

    VoxPilotProcessor& proc;
    VPLookAndFeel lnf;
    juce::Image background;

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
