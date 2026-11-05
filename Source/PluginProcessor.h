#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "Parameters.h"
#include "PresetManager.h"
#include "DSP/MscDsp.h"

class MSCVoxProcessor : public juce::AudioProcessor
{
public:
    MSCVoxProcessor();
    ~MSCVoxProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MSC VOXITY 2.0"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return presets.size(); }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override { return presets.get (index).name; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // theme index is stored in the plugin state
    int getThemeIndex() const { return (int) apvts.state.getProperty ("theme", 0); }
    void setThemeIndex (int i) { apvts.state.setProperty ("theme", i, nullptr); }

    // Save/Load helpers used by the editor buttons.
    juce::String stateToXmlString();
    bool loadStateFromXmlString (const juce::String& xml);

    juce::AudioProcessorValueTreeState apvts;
    msc::PresetManager presets;

    // ---- data shared with the editor (lock-free, written on the audio thread)
    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f };
    std::atomic<float> detectedMidi { 0.0f }, targetMidi { 0.0f };
    std::atomic<float> compGainReductionDb { 0.0f };
    static constexpr int kScopeSize = 256;
    std::array<std::atomic<float>, kScopeSize> scope;
    std::atomic<int> scopePos { 0 };

private:
    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    bool  flag  (const char* id) const { return param (id) > 0.5f; }
    double currentTempo();

    // DSP chain
    msc::AutoTune autoTune;
    msc::Compressor compressor;
    msc::DeEsser deEsser;
    msc::ParametricEQ eq;
    msc::Air air;
    msc::Distortion distortion;
    msc::Saturation saturation;
    msc::Chorus chorus;
    msc::Delay delay;
    msc::Reverb reverb;
    msc::StereoWidener widener;

    std::vector<float> bufL, bufR, dryL, dryR;
    bool needDelaySnap = true;
    std::vector<float> dryRing[2];
    int dryMask = 0, dryPos = 0, latency = 0, scopeCounter = 0;
    double sampleRate = 44100.0;
    int currentPreset = 7;

    juce::SmoothedValue<float> inGainSm, outGainSm, mixSm;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MSCVoxProcessor)
};
