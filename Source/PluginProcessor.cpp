#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace msc;

MSCVoxProcessor::MSCVoxProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MSCVOXITY", createParameterLayout())
{
    for (auto& s : scope) s.store (0.0f);
}

bool MSCVoxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return out == layouts.getMainInputChannelSet();
}

void MSCVoxProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    sampleRate = sr;
    autoTune.prepare (sr);   compressor.prepare (sr);  deEsser.prepare (sr);   eq.prepare (sr);
    air.prepare (sr);        distortion.prepare (sr);  saturation.prepare (sr); chorus.prepare (sr);
    delay.prepare (sr);      reverb.prepare (sr);

    latency = autoTune.latencySamples();
    setLatencySamples (latency);

    const int ringSize = msc::nextPow2 (latency + 8);
    for (auto& r : dryRing) r.assign ((size_t) ringSize, 0.0f);
    dryMask = ringSize - 1; dryPos = 0;

    bufL.assign ((size_t) std::max (samplesPerBlock, 1), 0.0f);
    bufR.assign ((size_t) std::max (samplesPerBlock, 1), 0.0f);
    dryL.assign ((size_t) std::max (samplesPerBlock, 1), 0.0f);
    dryR.assign ((size_t) std::max (samplesPerBlock, 1), 0.0f);
    needDelaySnap = true;

    inGainSm.reset (sr, 0.02);  inGainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (param (ids::inGain)));
    outGainSm.reset (sr, 0.02); outGainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (param (ids::outGain)));
    mixSm.reset (sr, 0.02);     mixSm.setCurrentAndTargetValue (param (ids::masterMix) * 0.01f);
    scopeCounter = 0;
}

double MSCVoxProcessor::currentTempo()
{
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                return *bpm;
    return 120.0;
}

void MSCVoxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    if (n <= 0 || numCh <= 0) return;

    if ((int) bufL.size() < n) { bufL.resize ((size_t) n); bufR.resize ((size_t) n); dryL.resize ((size_t) n); dryR.resize ((size_t) n); }
    float* l = bufL.data();
    float* r = bufR.data();
    std::copy (buffer.getReadPointer (0), buffer.getReadPointer (0) + n, l);
    std::copy (buffer.getReadPointer (numCh > 1 ? 1 : 0), buffer.getReadPointer (numCh > 1 ? 1 : 0) + n, r);

    // ---------------------------------------------------------------- input gain + meter
    inGainSm.setTargetValue (juce::Decibels::decibelsToGain (param (ids::inGain)));
    float peakIn = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float g = inGainSm.getNextValue();
        l[i] *= g; r[i] *= g;
        peakIn = std::max (peakIn, std::max (std::fabs (l[i]), std::fabs (r[i])));
    }
    inPeak.store (peakIn);

    // ---------------------------------------------------------------- latency-compensated dry copy
    float* dry[2] = { dryL.data(), dryR.data() };
    for (int c = 0; c < 2; ++c)
    {
        const float* src = c == 0 ? l : r;
        for (int i = 0; i < n; ++i)
        {
            dryRing[c][(size_t) ((dryPos + i) & dryMask)] = src[i];
            dry[c][i] = dryRing[c][(size_t) ((dryPos + i - latency) & dryMask)];
        }
    }
    dryPos = (dryPos + n) & dryMask;

    // ---------------------------------------------------------------- DSP chain
    autoTune.setParams (flag (ids::atOn), (int) param (ids::atScale), (int) param (ids::atNote),
                        param (ids::atAmount) * 0.01f, param (ids::atSpeed), (AutoTuneMode) (int) param (ids::atMode));
    autoTune.process (l, r, n);
    detectedMidi.store (autoTune.getDetectedMidi());
    targetMidi.store (autoTune.getTargetMidi());

    if (flag (ids::cpOn))
    {
        compressor.setParams (param (ids::cpThresh), param (ids::cpRatio), 10.0f, param (ids::cpRelease), param (ids::cpMakeup));
        compressor.process (l, r, n);
        compGainReductionDb.store (compressor.getGainReductionDb());
    }
    else { compGainReductionDb.store (0.0f); compressor.reset(); }

    if (flag (ids::dsOn))
    {
        deEsser.setParams (param (ids::dsThresh), param (ids::dsFreq), param (ids::dsReduction), param (ids::dsAmount) * 0.01f);
        deEsser.process (l, r, n);
    }
    if (flag (ids::eqOn))
    {
        eq.setGains (param (ids::eqLow), param (ids::eqMid), param (ids::eqHigh));
        eq.process (l, r, n);
    }
    if (flag (ids::airOn))
    {
        air.setAmount (param (ids::airAmount) * 0.01f);
        air.process (l, r, n);
    }
    if (flag (ids::diOn))
    {
        distortion.setParams (param (ids::diDrive) * 0.01f, param (ids::diTone) * 0.01f, param (ids::diMix) * 0.01f);
        distortion.process (l, r, n);
    }
    if (flag (ids::saOn))
    {
        saturation.setParams (param (ids::saDrive) * 0.01f, param (ids::saMix) * 0.01f, (SaturationType) (int) param (ids::saType));
        saturation.process (l, r, n);
    }
    if (flag (ids::chOn))
    {
        chorus.setParams (param (ids::chRate), param (ids::chDepth) * 0.01f, param (ids::chWidth) * 0.01f, param (ids::chMix) * 0.01f);
        chorus.process (l, r, n);
    }
    if (flag (ids::dlOn))
    {
        float ms = param (ids::dlTime);
        if (flag (ids::dlSync))
            ms = Delay::syncedMs (currentTempo(), (int) param (ids::dlDiv), flag (ids::dlDotted), flag (ids::dlTriplet));
        delay.setParams (ms, param (ids::dlFeedback) * 0.01f, param (ids::dlMix) * 0.01f, param (ids::dlTone) * 0.01f, flag (ids::dlStereo));
        if (needDelaySnap) { delay.snap(); needDelaySnap = false; }
        delay.process (l, r, n);
    }
    if (flag (ids::rvOn))
    {
        reverb.setParams (param (ids::rvSize) * 0.01f, param (ids::rvDecay), param (ids::rvDamping) * 0.01f,
                          param (ids::rvPredelay), param (ids::rvMix) * 0.01f, (ReverbType) (int) param (ids::rvType));
        reverb.process (l, r, n);
    }
    if (flag (ids::swOn))
    {
        widener.setParams (param (ids::swWidth) * 0.01f, param (ids::swMix) * 0.01f);
        widener.process (l, r, n);
    }

    // ---------------------------------------------------------------- master mix, output gain, meters
    mixSm.setTargetValue (param (ids::masterMix) * 0.01f);
    outGainSm.setTargetValue (juce::Decibels::decibelsToGain (param (ids::outGain)));
    float peakOut = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float m = mixSm.getNextValue();
        const float g = outGainSm.getNextValue();
        l[i] = (dry[0][i] + m * (l[i] - dry[0][i])) * g;
        r[i] = (dry[1][i] + m * (r[i] - dry[1][i])) * g;
        peakOut = std::max (peakOut, std::max (std::fabs (l[i]), std::fabs (r[i])));
        if (++scopeCounter >= 64)
        {
            scopeCounter = 0;
            const int p = (scopePos.load() + 1) % kScopeSize;
            scope[(size_t) p].store (0.5f * (l[i] + r[i]));
            scopePos.store (p);
        }
    }
    outPeak.store (peakOut);

    // ---------------------------------------------------------------- write back
    if (numCh == 1)
    {
        float* o = buffer.getWritePointer (0);
        for (int i = 0; i < n; ++i) o[i] = 0.5f * (l[i] + r[i]);
    }
    else
    {
        std::copy (l, l + n, buffer.getWritePointer (0));
        std::copy (r, r + n, buffer.getWritePointer (1));
        for (int c = 2; c < numCh; ++c) buffer.clear (c, 0, n);
    }
}

juce::AudioProcessorEditor* MSCVoxProcessor::createEditor() { return new MSCVoxEditor (*this); }

void MSCVoxProcessor::setCurrentProgram (int index)
{
    index = juce::jlimit (0, presets.size() - 1, index);
    currentPreset = index;
    apvts.state.setProperty ("preset", index, nullptr);
    PresetManager::apply (apvts, presets.get (index));
}

void MSCVoxProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    apvts.state.setProperty ("preset", currentPreset, nullptr);
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MSCVoxProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            currentPreset = juce::jlimit (0, presets.size() - 1, (int) apvts.state.getProperty ("preset", 7));
        }
}

juce::String MSCVoxProcessor::stateToXmlString()
{
    apvts.state.setProperty ("preset", currentPreset, nullptr);
    if (auto xml = apvts.copyState().createXml()) return xml->toString();
    return {};
}

bool MSCVoxProcessor::loadStateFromXmlString (const juce::String& text)
{
    if (auto xml = juce::XmlDocument::parse (text))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            currentPreset = juce::jlimit (0, presets.size() - 1, (int) apvts.state.getProperty ("preset", 7));
            return true;
        }
    return false;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MSCVoxProcessor(); }
