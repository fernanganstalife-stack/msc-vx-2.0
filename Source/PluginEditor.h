#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "LookAndFeel.h"
#include "ThemeManager.h"

// The UI is laid out in the 1536x1024 coordinate system of the reference artwork and scaled to the window.
class MSCVoxEditor : public juce::AudioProcessorEditor
{
public:
    explicit MSCVoxEditor (MSCVoxProcessor&);
    ~MSCVoxEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

private:
    class Content;
    MSCVoxProcessor& proc;
    msc::ThemeManager themes;
    msc::MscLookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    std::unique_ptr<Content> content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MSCVoxEditor)
};
