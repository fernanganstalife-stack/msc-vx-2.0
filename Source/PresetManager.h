#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace msc
{
struct Preset
{
    juce::String name, description;
    juce::var params;   // object: parameterID -> real value (choices as display name)
};

// Factory presets are JSON files in /Resources/Presets embedded in the plugin binary.
class PresetManager
{
public:
    PresetManager();
    int size() const { return (int) presets.size(); }
    const Preset& get (int i) const { return presets[(size_t) juce::jlimit (0, size() - 1, i)]; }
    juce::StringArray names() const;

    // Sets every parameter listed in the preset (notifying the host).
    static void apply (juce::AudioProcessorValueTreeState& apvts, const Preset& preset);
private:
    std::vector<Preset> presets;
};
} // namespace msc
