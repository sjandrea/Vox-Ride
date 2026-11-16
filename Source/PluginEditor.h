#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace vpcol
{
    const juce::Colour panelTop  { 0xff4a5c6d }; // painted steel-blue front plate
    const juce::Colour panelBot  { 0xff3a4a59 };
    const juce::Colour text      { 0xffeef0ef };
    const juce::Colour textDim   { 0xffaeb9c3 };
    const juce::Colour cream     { 0xfff1e4c3 }; // pointers
    const juce::Colour vuFace    { 0xfff4dfa6 };
    const juce::Colour vuEdge    { 0xffcfa65a };
    const juce::Colour vuInk     { 0xff1d1a14 };
    const juce::Colour vuRed     { 0xffc9241c };
    const juce::Colour plaque    { 0xff1d2023 };
    const juce::Colour red       { 0xffc8231d };
    const juce::Colour amber     { 0xffff9a1f };
    const juce::Colour green     { 0xff3fe060 };
    const juce::Colour silver    { 0xffdfe2e4 };
    const juce::Colour darkText  { 0xff111315 };
    // scope (phosphor screen)
    const juce::Colour scopeBg   { 0xff0b120e };
    const juce::Colour trace     { 0xff8dffb4 }; // output
    const juce::Colour vocal     { 0xff6f8b7a }; // raw vocal level
    const juce::Colour gainTrace { 0xffffb03a };
    const juce::Colour detect    { 0xffff5a4e };
}

/** Shared drawing helpers for the retro hardware look. */
namespace vpdraw
{
    void glowStroke (juce::Graphics&, const juce::Path&, juce::Colour, float width);
    void screw (juce::Graphics&, juce::Point<float> centre, float radius, float slotAngle);
    void jewelLamp (juce::Graphics&, juce::Point<float> centre, float radius, juce::Colour, bool on);
    void plaque (juce::Graphics&, juce::Rectangle<float>, juce::Colour fill, bool lit);
    void brushedMetal (juce::Graphics&, juce::Rectangle<float>, bool vertical, juce::Colour light, juce::Colour dark, int seed);
}

//==============================================================================
class VPLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VPLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    juce::Font getLabelFont (juce::Label&) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

    /** Turns a label into a black (or coloured) engraved name plaque. */
    static void makePlaque (juce::Label&, juce::Colour fill = vpcol::plaque);
    /** Sets the numbers printed around a knob: "text:value,text:value,…". */
    static void setScale (juce::Slider&, const juce::String& spec);
};

//==============================================================================
/** Scrolling history in a phosphor "scope" screen: vocal, output, target, threshold, rider gain. */
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
    juce::Rectangle<float> screen() const;
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

    static constexpr float minDb = -60.0f, maxDb = 0.0f;
};

//==============================================================================
/** Analogue needle meter showing the applied rider gain; drag to set "Rider Gain" in Read mode. */
class RiderMeter : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit RiderMeter (VoxPilotProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void update (float gainDb, bool interactive, AutoMode mode);

private:
    juce::Rectangle<float> face() const;
    juce::Point<float> pivot() const;
    float gainToAngle (float db) const;

    VoxPilotProcessor& proc;
    juce::ParameterAttachment attachment;
    float needleDb = 0.0f, targetDb = 0.0f, dragStartDb = 0.0f;
    bool canDrag = false, dragging = false;
    AutoMode currentMode = AutoMode::off;
};

//==============================================================================
/** Chrome bat-handle toggle switch (up = first option). */
class ToggleSwitch : public juce::Component, public juce::SettableTooltipClient
{
public:
    ToggleSwitch (juce::String upText, juce::String downText);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void setUp (bool shouldBeUp) { if (up != shouldBeUp) { up = shouldBeUp; repaint(); } }
    std::function<void (bool up)> onFlip;

private:
    juce::String upLabel, downLabel;
    bool up = true;
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
    void setupKnob (juce::Slider&, juce::Label& plaque, const juce::String& name, const char* paramId,
                    const juce::String& scale, juce::Colour plaqueColour = vpcol::plaque);
    void placeKnob (juce::Slider&, juce::Label& plaque, juce::Point<int> centre, int knobSize);
    void setChoice (const char* id, float normalised);
    void buildBackground();

    VoxPilotProcessor& proc;
    VPLookAndFeel lnf;
    juce::Image background;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    juce::Slider targetKnob, outputKnob, detectKnob, bleedKnob, musicKnob, cutKnob, boostKnob;
    juce::Label  targetPlaque, outputPlaque, detectPlaque, bleedPlaque, musicPlaque, cutPlaque, boostPlaque, responsePlaque;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    RiderMeter meter;
    HistoryView history;
    ToggleSwitch speedSwitch { "FAST", "SLOW" };

    juce::TextButton offBtn { "OFF" }, writeBtn { "WRITE" }, readBtn { "READ" };
    std::array<juce::TextButton, 4> lookaheadBtns;

    juce::TooltipWindow tooltips { this, 600 };

    bool rideLed = false, scLed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxPilotEditor)
};
