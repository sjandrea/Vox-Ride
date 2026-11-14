#include "PluginProcessor.h"
#include "PluginEditor.h"

using APF = juce::AudioParameterFloat;

static juce::String dbString (float v, int)  { return juce::String (v, 1) + " dB"; }
static juce::String pctString (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }

juce::AudioProcessorValueTreeState::ParameterLayout VoxPilotProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto dbAttr  = AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (dbString);
    auto pctAttr = AudioParameterFloatAttributes().withStringFromValueFunction (pctString);

    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::target, 1 }, "Target",
                                       NormalisableRange<float> (-40.0f, 0.0f, 0.1f), -18.0f, dbAttr));
    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::detect, 1 }, "Vocal Detect",
                                       NormalisableRange<float> (-70.0f, -20.0f, 0.1f), -45.0f, dbAttr));
    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::bleed, 1 }, "Bleed Guard",
                                       NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f, pctAttr));
    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::musicFollow, 1 }, "Music Follow",
                                       NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f, pctAttr));
    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::rangeLow, 1 }, "Range Low",
                                       NormalisableRange<float> (-gainLimitDb, 0.0f, 0.1f), -6.0f, dbAttr));
    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::rangeHigh, 1 }, "Range High",
                                       NormalisableRange<float> (0.0f, gainLimitDb, 0.1f), 6.0f, dbAttr));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::fast, 1 }, "Fast", false,
                                       AudioParameterBoolAttributes().withStringFromValueFunction (
                                           [] (bool b, int) { return b ? String ("Fast") : String ("Slow"); })));
    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::rider, 1 }, "Rider Gain",
                                       NormalisableRange<float> (-gainLimitDb, gainLimitDb, 0.01f), 0.0f, dbAttr));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::autoMode, 1 }, "Automation",
                                       StringArray { "Off", "Write", "Read" }, 0));
    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::output, 1 }, "Output",
                                       NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, dbAttr));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::lookahead, 1 }, "Lookahead",
                                       StringArray { "Off", "20 ms", "50 ms", "100 ms" }, 0,
                                       AudioParameterChoiceAttributes().withAutomatable (false)));
    return layout;
}

VoxPilotProcessor::VoxPilotProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "VoxPilotState", createLayout())
{
    auto f = [this] (const char* id) { return dynamic_cast<APF*> (apvts.getParameter (id)); };
    pTarget    = f (ParamIDs::target);
    pDetect    = f (ParamIDs::detect);
    pBleed     = f (ParamIDs::bleed);
    pMusic     = f (ParamIDs::musicFollow);
    pRangeLow  = f (ParamIDs::rangeLow);
    pRangeHigh = f (ParamIDs::rangeHigh);
    pRider     = f (ParamIDs::rider);
    pOutput    = f (ParamIDs::output);
    pFast      = dynamic_cast<juce::AudioParameterBool*>   (apvts.getParameter (ParamIDs::fast));
    pAutoMode  = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (ParamIDs::autoMode));
    pLookahead = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (ParamIDs::lookahead));
    jassert (pTarget && pDetect && pBleed && pMusic && pRangeLow && pRangeHigh && pRider && pOutput && pFast && pAutoMode && pLookahead);
}

bool VoxPilotProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainIn  = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;
    if (mainIn != mainOut)
        return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto& sc = layouts.inputBuses[1];
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void VoxPilotProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    engine.prepare (sampleRate);

    appliedGainDb = 0.0f;
    readSmoothCoef = (float) std::exp (-1.0 / (0.010 * sampleRate));
    outputGain.reset (sampleRate, 0.03);
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->get()));

    writeInterval   = juce::jmax (1, (int) (sampleRate / 100.0)); // 100 automation points / s max
    historyInterval = juce::jmax (1, (int) (sampleRate / 60.0));  // 60 display frames / s
    writeCounter = historyCounter = 0;
    outCoef = (float) std::exp (-1.0 / (0.05 * sampleRate));
    outMs = 0.0f;
    lastWrittenDb = 1000.0f;

    // delay line sized for the longest choice
    const int maxDelay = (int) std::ceil (sampleRate * 0.001 * lookaheadChoicesMs[3]) + 1;
    delayBuffer.setSize (juce::jmax (1, getMainBusNumInputChannels()), maxDelay + 1);
    delayBuffer.clear();
    delayWritePos = 0;
    lookaheadIndex = pLookahead->getIndex();
    lookaheadSamples = lookaheadSamplesFor (lookaheadIndex);
    reportedLatency.store (lookaheadSamples);
    setLatencySamples (lookaheadSamples);
}

int VoxPilotProcessor::lookaheadSamplesFor (int choiceIndex) const
{
    return (int) std::round (currentSampleRate * 0.001 * lookaheadChoicesMs[juce::jlimit (0, 3, choiceIndex)]);
}

void VoxPilotProcessor::handleAsyncUpdate()
{
    // tell the host (Logic) about the new plug-in delay so it can compensate
    setLatencySamples (reportedLatency.load());
}

void VoxPilotProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto main = getBusBuffer (buffer, true, 0);
    const int numSamples = buffer.getNumSamples();
    const int numCh = main.getNumChannels();

    // clear any output channels without input
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    // ---- side-chain -------------------------------------------------------------
    juce::AudioBuffer<float> sc;
    if (auto* scBus = getBus (true, 1); scBus != nullptr && scBus->isEnabled() && scBus->getNumberOfChannels() > 0)
        sc = getBusBuffer (buffer, true, 1);
    const int scCh = sc.getNumChannels();
    const bool hasMusic = scCh > 0;
    sidechainConnected.store (hasMusic, std::memory_order_relaxed);

    // ---- settings ---------------------------------------------------------------
    vp::RiderEngine::Settings s;
    s.targetDb    = pTarget->get();
    s.detectDb    = pDetect->get();
    s.bleedGuard  = pBleed->get();
    s.musicFollow = pMusic->get();
    s.rangeLowDb  = pRangeLow->get();
    s.rangeHighDb = pRangeHigh->get();
    s.fast        = pFast->get();

    // lookahead change: new delay, clear the line (avoids a click from stale audio), report latency
    if (const int idx = pLookahead->getIndex(); idx != lookaheadIndex)
    {
        lookaheadIndex = idx;
        lookaheadSamples = lookaheadSamplesFor (idx);
        delayBuffer.clear();
        reportedLatency.store (lookaheadSamples);
        triggerAsyncUpdate();
    }
    s.lookaheadMs = lookaheadChoicesMs[lookaheadIndex];
    engine.setSettings (s);

    outputGain.setTargetValue (juce::Decibels::decibelsToGain (pOutput->get()));

    const auto mode = (AutoMode) pAutoMode->getIndex();

    // open / close the host gesture that lets Logic record our "Rider Gain" moves
    if (mode == AutoMode::write && ! writeGestureOpen)
    {
        pRider->beginChangeGesture();
        writeGestureOpen = true;
        lastWrittenDb = 1000.0f;
    }
    else if (mode != AutoMode::write && writeGestureOpen)
    {
        pRider->endChangeGesture();
        writeGestureOpen = false;
    }

    const float readTarget = pRider->get();
    const float invCh = numCh > 0 ? 1.0f / (float) numCh : 0.0f;
    const float invSc = scCh > 0 ? 1.0f / (float) scCh : 0.0f;

    float* const* chans = main.getArrayOfWritePointers();
    float* const* delayChans = delayBuffer.getArrayOfWritePointers();
    const int delayLen = delayBuffer.getNumSamples();
    const int delayCh = juce::jmin (numCh, delayBuffer.getNumChannels());
    const bool useDelay = lookaheadSamples > 0 && delayCh == numCh;
    const float* const* scChans = hasMusic ? sc.getArrayOfReadPointers() : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        float vocal = 0.0f;
        for (int ch = 0; ch < numCh; ++ch) vocal += chans[ch][i];
        vocal *= invCh;

        float music = 0.0f;
        if (hasMusic)
        {
            for (int ch = 0; ch < scCh; ++ch) music += scChans[ch][i];
            music *= invSc;
        }

        const float computed = engine.processSample (vocal, music, hasMusic);

        if (mode == AutoMode::read)
            appliedGainDb = readTarget + (appliedGainDb - readTarget) * readSmoothCoef;
        else
            appliedGainDb = computed;

        const float g = std::exp (appliedGainDb * 0.11512925464970229f) * outputGain.getNextValue();

        if (useDelay)
        {
            // the detector has already seen this sample; play the audio from `lookaheadSamples` ago
            int readPos = delayWritePos - lookaheadSamples;
            if (readPos < 0) readPos += delayLen;
            for (int ch = 0; ch < numCh; ++ch)
            {
                delayChans[ch][delayWritePos] = chans[ch][i];
                chans[ch][i] = delayChans[ch][readPos];
            }
            if (++delayWritePos >= delayLen) delayWritePos = 0;
        }

        float outSum = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            chans[ch][i] *= g;
            outSum += chans[ch][i];
        }
        outSum *= invCh;
        outMs = outCoef * outMs + (1.0f - outCoef) * outSum * outSum;

        // ---- automation write -------------------------------------------------
        if (mode == AutoMode::write && ++writeCounter >= writeInterval)
        {
            writeCounter = 0;
            if (std::abs (computed - lastWrittenDb) > 0.05f)
            {
                pRider->setValueNotifyingHost (pRider->convertTo0to1 (computed));
                lastWrittenDb = computed;
            }
        }

        // ---- display history --------------------------------------------------
        if (++historyCounter >= historyInterval)
        {
            historyCounter = 0;
            const float outDb = vp::RiderEngine::toDb (outMs);
            pushHistory ({ engine.getVocalDb(), outDb, appliedGainDb,
                           engine.getEffectiveTargetDb(), engine.getEffectiveThresholdDb(), engine.isActive() });
        }
    }

    meterVocalDb.store (engine.getVocalDb(), std::memory_order_relaxed);
    meterMusicDb.store (engine.getMusicDb(), std::memory_order_relaxed);
    meterOutDb.store (vp::RiderEngine::toDb (outMs), std::memory_order_relaxed);
    meterGainDb.store (appliedGainDb, std::memory_order_relaxed);
    meterTargetDb.store (engine.getEffectiveTargetDb(), std::memory_order_relaxed);
    meterThresholdDb.store (engine.getEffectiveThresholdDb(), std::memory_order_relaxed);
    meterActive.store (engine.isActive() && mode != AutoMode::read, std::memory_order_relaxed);
}

void VoxPilotProcessor::pushHistory (const HistoryFrame& f)
{
    const auto scope = historyFifo.write (1);
    if (scope.blockSize1 > 0) historyBuffer[(size_t) scope.startIndex1] = f;
    else if (scope.blockSize2 > 0) historyBuffer[(size_t) scope.startIndex2] = f;
    // if the FIFO is full (editor closed) the frame is simply dropped
}

int VoxPilotProcessor::popHistory (HistoryFrame* dest, int maxFrames)
{
    const auto scope = historyFifo.read (juce::jmin (maxFrames, historyFifo.getNumReady()));
    int n = 0;
    for (int i = 0; i < scope.blockSize1; ++i) dest[n++] = historyBuffer[(size_t) (scope.startIndex1 + i)];
    for (int i = 0; i < scope.blockSize2; ++i) dest[n++] = historyBuffer[(size_t) (scope.startIndex2 + i)];
    return n;
}

void VoxPilotProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void VoxPilotProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* VoxPilotProcessor::createEditor()
{
    return new VoxPilotEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VoxPilotProcessor();
}
