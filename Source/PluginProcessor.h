#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "RiderEngine.h"

namespace ParamIDs
{
    inline constexpr const char* target      = "target";
    inline constexpr const char* detect      = "detect";
    inline constexpr const char* bleed       = "bleed";
    inline constexpr const char* musicFollow = "musicFollow";
    inline constexpr const char* rangeLow    = "rangeLow";
    inline constexpr const char* rangeHigh   = "rangeHigh";
    inline constexpr const char* fast        = "fast";
    inline constexpr const char* rider       = "rider";
    inline constexpr const char* autoMode    = "autoMode";
    inline constexpr const char* output      = "output";
    inline constexpr const char* lookahead   = "lookahead";
}

enum class AutoMode { off = 0, write = 1, read = 2 };

/** Lookahead choices (index of the "lookahead" parameter -> milliseconds). */
inline constexpr float lookaheadChoicesMs[] = { 0.0f, 20.0f, 50.0f, 100.0f };

/** One frame of history for the scrolling display. */
struct HistoryFrame
{
    float vocalDb, outDb, gainDb, targetDb, thresholdDb;
    bool active;
};

class VoxPilotProcessor : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    VoxPilotProcessor();
    ~VoxPilotProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // ---- read by the editor (lock-free) -------------------------------------
    std::atomic<float> meterVocalDb { -100.0f }, meterMusicDb { -100.0f }, meterOutDb { -100.0f };
    std::atomic<float> meterGainDb { 0.0f }, meterTargetDb { -18.0f }, meterThresholdDb { -45.0f };
    std::atomic<bool>  meterActive { false }, sidechainConnected { false };

    /** Pops history frames produced by the audio thread. Message thread only. */
    int popHistory (HistoryFrame* dest, int maxFrames);

    static constexpr float gainLimitDb = 12.0f;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void handleAsyncUpdate() override;
    int lookaheadSamplesFor (int choiceIndex) const;
    void pushHistory (const HistoryFrame& f);

    vp::RiderEngine engine;

    juce::AudioParameterFloat* pTarget = nullptr;
    juce::AudioParameterFloat* pDetect = nullptr;
    juce::AudioParameterFloat* pBleed = nullptr;
    juce::AudioParameterFloat* pMusic = nullptr;
    juce::AudioParameterFloat* pRangeLow = nullptr;
    juce::AudioParameterFloat* pRangeHigh = nullptr;
    juce::AudioParameterBool*  pFast = nullptr;
    juce::AudioParameterFloat* pRider = nullptr;
    juce::AudioParameterChoice* pAutoMode = nullptr;
    juce::AudioParameterFloat* pOutput = nullptr;
    juce::AudioParameterChoice* pLookahead = nullptr;

    // lookahead delay line (audio path only; the detector sees the undelayed signal)
    juce::AudioBuffer<float> delayBuffer;
    int delayWritePos = 0;
    int lookaheadIndex = 0, lookaheadSamples = 0;
    std::atomic<int> reportedLatency { 0 };

    double currentSampleRate = 44100.0;
    float appliedGainDb = 0.0f;       // the gain actually applied (after read/write logic)
    float readSmoothCoef = 0.99f;
    juce::SmoothedValue<float> outputGain;

    // automation writing
    bool writeGestureOpen = false;
    float lastWrittenDb = 1000.0f;
    int writeInterval = 441, writeCounter = 0;

    // metering / history
    int historyInterval = 441, historyCounter = 0;
    float historyPeakOut = 0.0f;
    float outMs = 0.0f, outCoef = 0.999f;

    static constexpr int historySize = 2048;
    juce::AbstractFifo historyFifo { historySize };
    std::array<HistoryFrame, historySize> historyBuffer {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxPilotProcessor)
};
