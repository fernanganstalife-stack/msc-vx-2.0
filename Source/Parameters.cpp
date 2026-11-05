#include "Parameters.h"
#include "DSP/AutoTune.h"

namespace msc
{
using APF  = juce::AudioParameterFloat;
using APB  = juce::AudioParameterBool;
using APC  = juce::AudioParameterChoice;
using Attr = juce::AudioParameterFloatAttributes;

juce::StringArray getScaleNames()
{
    int n = 0; const ScaleDef* d = getScaleDefs (n);
    juce::StringArray s; for (int i = 0; i < n; ++i) s.add (d[i].name);
    return s;
}
juce::StringArray getNoteNames()            { return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; }
juce::StringArray getAutoTuneModeNames()    { return { "Robotic", "Natural", "Smooth", "Hard" }; }
juce::StringArray getSaturationTypeNames()  { return { "Tube", "Tape", "Transistor" }; }
juce::StringArray getReverbTypeNames()      { return { "Hall", "Room", "Plate", "Chamber" }; }
juce::StringArray getDelayDivisionNames()   { return { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" }; }

static std::unique_ptr<APF> makeFloat (const char* id, const char* name, float lo, float hi, float def,
                                       const juce::String& suffix, int decimals, float skewMid = -1.0f, float step = 0.0f)
{
    juce::NormalisableRange<float> range (lo, hi, step);
    if (skewMid > 0.0f) range.setSkewForCentre (skewMid);
    auto attr = Attr().withStringFromValueFunction ([suffix, decimals] (float v, int)
                                                    { return juce::String (v, decimals) + suffix; })
                      .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue(); });
    return std::make_unique<APF> (juce::ParameterID { id, 1 }, name, range, def, attr);
}
static std::unique_ptr<APB> makeBool (const char* id, const char* name, bool def)
{
    return std::make_unique<APB> (juce::ParameterID { id, 1 }, name, def);
}
static std::unique_ptr<APC> makeChoice (const char* id, const char* name, const juce::StringArray& c, int def)
{
    return std::make_unique<APC> (juce::ParameterID { id, 1 }, name, c, def);
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace ids;
    juce::AudioProcessorValueTreeState::ParameterLayout l;

    l.add (makeFloat (inGain,   "Input Gain",  -24.0f, 24.0f, 0.0f, " dB", 1));
    l.add (makeFloat (outGain,  "Output Gain", -24.0f, 24.0f, 0.0f, " dB", 1));
    l.add (makeFloat (masterMix, "Master Mix",  0.0f, 100.0f, 100.0f, "%", 0));

    l.add (makeBool   (atOn, "Auto-Tune On", true));
    l.add (makeChoice (atScale, "Auto-Tune Scale", getScaleNames(), 2));
    l.add (makeChoice (atNote, "Auto-Tune Note", getNoteNames(), 0));
    l.add (makeFloat  (atAmount, "Auto-Tune Amount", 0.0f, 100.0f, 100.0f, "%", 0));
    l.add (makeFloat  (atSpeed, "Retune Speed", 0.0f, 400.0f, 20.0f, " ms", 0, 60.0f));
    l.add (makeChoice (atMode, "Auto-Tune Mode", getAutoTuneModeNames(), 0));

    l.add (makeBool  (cpOn, "Compressor On", true));
    l.add (makeFloat (cpThresh, "Comp Threshold", -60.0f, 0.0f, -12.0f, " dB", 1));
    l.add (makeFloat (cpRatio, "Comp Ratio", 1.0f, 20.0f, 4.0f, ":1", 1, 5.0f));
    l.add (makeFloat (cpRelease, "Comp Release", 10.0f, 1000.0f, 120.0f, " ms", 0, 150.0f));
    l.add (makeFloat (cpMakeup, "Comp Makeup", 0.0f, 24.0f, 4.0f, " dB", 1));

    l.add (makeBool  (dsOn, "De-Esser On", true));
    l.add (makeFloat (dsThresh, "De-Esser Threshold", -60.0f, 0.0f, -30.0f, " dB", 1));
    l.add (makeFloat (dsFreq, "De-Esser Freq", 2000.0f, 12000.0f, 6000.0f, " Hz", 0, 6000.0f));
    l.add (makeFloat (dsReduction, "De-Esser Reduction", -24.0f, 0.0f, -12.0f, " dB", 1));
    l.add (makeFloat (dsAmount, "De-Esser Amount", 0.0f, 100.0f, 70.0f, "%", 0));

    l.add (makeBool  (eqOn, "EQ On", true));
    l.add (makeFloat (eqLow, "EQ Low", -12.0f, 12.0f, -3.0f, " dB", 1));
    l.add (makeFloat (eqMid, "EQ Mid", -12.0f, 12.0f, 1.5f, " dB", 1));
    l.add (makeFloat (eqHigh, "EQ High", -12.0f, 12.0f, 2.0f, " dB", 1));

    l.add (makeBool  (airOn, "Air On", true));
    l.add (makeFloat (airAmount, "Air Amount", 0.0f, 100.0f, 80.0f, "%", 0));

    l.add (makeBool  (diOn, "Distortion On", false));
    l.add (makeFloat (diDrive, "Distortion Drive", 0.0f, 100.0f, 30.0f, "%", 0));
    l.add (makeFloat (diTone, "Distortion Tone", 0.0f, 100.0f, 50.0f, "%", 0));
    l.add (makeFloat (diMix, "Distortion Mix", 0.0f, 100.0f, 50.0f, "%", 0));

    l.add (makeBool   (saOn, "Saturation On", true));
    l.add (makeFloat  (saDrive, "Saturation Drive", 0.0f, 100.0f, 35.0f, "%", 0));
    l.add (makeFloat  (saMix, "Saturation Mix", 0.0f, 100.0f, 40.0f, "%", 0));
    l.add (makeChoice (saType, "Saturation Type", getSaturationTypeNames(), 0));

    l.add (makeBool   (rvOn, "Reverb On", true));
    l.add (makeFloat  (rvSize, "Reverb Size", 0.0f, 100.0f, 50.0f, "%", 0));
    l.add (makeFloat  (rvDecay, "Reverb Decay", 0.2f, 12.0f, 2.8f, " s", 1, 2.5f));
    l.add (makeFloat  (rvDamping, "Reverb Damping", 0.0f, 100.0f, 50.0f, "%", 0));
    l.add (makeFloat  (rvPredelay, "Reverb Pre-Delay", 0.0f, 200.0f, 20.0f, " ms", 0));
    l.add (makeFloat  (rvMix, "Reverb Mix", 0.0f, 100.0f, 25.0f, "%", 0));
    l.add (makeChoice (rvType, "Reverb Type", getReverbTypeNames(), 0));

    l.add (makeBool   (dlOn, "Delay On", true));
    l.add (makeFloat  (dlTime, "Delay Time", 20.0f, 1500.0f, 350.0f, " ms", 0, 300.0f));
    l.add (makeFloat  (dlFeedback, "Delay Feedback", 0.0f, 95.0f, 35.0f, "%", 0));
    l.add (makeFloat  (dlMix, "Delay Mix", 0.0f, 100.0f, 40.0f, "%", 0));
    l.add (makeFloat  (dlTone, "Delay Tone", 0.0f, 100.0f, 60.0f, "%", 0));
    l.add (makeBool   (dlSync, "Delay Sync", true));
    l.add (makeChoice (dlDiv, "Delay Division", getDelayDivisionNames(), 2));
    l.add (makeBool   (dlDotted, "Delay Dotted", false));
    l.add (makeBool   (dlTriplet, "Delay Triplet", false));
    l.add (makeBool   (dlStereo, "Delay Stereo (Ping-Pong)", true));

    l.add (makeBool  (chOn, "Chorus On", true));
    l.add (makeFloat (chRate, "Chorus Rate", 0.1f, 5.0f, 1.2f, " Hz", 1, 1.0f));
    l.add (makeFloat (chDepth, "Chorus Depth", 0.0f, 100.0f, 70.0f, "%", 0));
    l.add (makeFloat (chWidth, "Chorus Width", 0.0f, 100.0f, 100.0f, "%", 0));
    l.add (makeFloat (chMix, "Chorus Mix", 0.0f, 100.0f, 60.0f, "%", 0));

    l.add (makeBool  (swOn, "Widener On", true));
    l.add (makeFloat (swWidth, "Widener Width", 0.0f, 200.0f, 120.0f, "%", 0));
    l.add (makeFloat (swMix, "Widener Mix", 0.0f, 100.0f, 100.0f, "%", 0));
    return l;
}
} // namespace msc
