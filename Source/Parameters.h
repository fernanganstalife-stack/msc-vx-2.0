#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace msc::ids
{
// global
inline constexpr const char* inGain   = "in_gain";
inline constexpr const char* outGain  = "out_gain";
inline constexpr const char* masterMix = "master_mix";
// auto-tune
inline constexpr const char* atOn     = "at_on";
inline constexpr const char* atScale  = "at_scale";
inline constexpr const char* atNote   = "at_note";
inline constexpr const char* atAmount = "at_amount";
inline constexpr const char* atSpeed  = "at_speed";
inline constexpr const char* atMode   = "at_mode";
// compressor
inline constexpr const char* cpOn = "cp_on";   inline constexpr const char* cpThresh = "cp_thresh";
inline constexpr const char* cpRatio = "cp_ratio"; inline constexpr const char* cpRelease = "cp_release";
inline constexpr const char* cpMakeup = "cp_makeup";
// de-esser
inline constexpr const char* dsOn = "ds_on";   inline constexpr const char* dsThresh = "ds_thresh";
inline constexpr const char* dsFreq = "ds_freq"; inline constexpr const char* dsReduction = "ds_reduction";
inline constexpr const char* dsAmount = "ds_amount";
// eq
inline constexpr const char* eqOn = "eq_on";   inline constexpr const char* eqLow = "eq_low";
inline constexpr const char* eqMid = "eq_mid"; inline constexpr const char* eqHigh = "eq_high";
// air
inline constexpr const char* airOn = "air_on"; inline constexpr const char* airAmount = "air_amount";
// distortion
inline constexpr const char* diOn = "di_on";   inline constexpr const char* diDrive = "di_drive";
inline constexpr const char* diTone = "di_tone"; inline constexpr const char* diMix = "di_mix";
// saturation
inline constexpr const char* saOn = "sa_on";   inline constexpr const char* saDrive = "sa_drive";
inline constexpr const char* saMix = "sa_mix"; inline constexpr const char* saType = "sa_type";
// reverb
inline constexpr const char* rvOn = "rv_on";   inline constexpr const char* rvSize = "rv_size";
inline constexpr const char* rvDecay = "rv_decay"; inline constexpr const char* rvDamping = "rv_damping";
inline constexpr const char* rvPredelay = "rv_predelay"; inline constexpr const char* rvMix = "rv_mix";
inline constexpr const char* rvType = "rv_type";
// delay
inline constexpr const char* dlOn = "dl_on";   inline constexpr const char* dlTime = "dl_time";
inline constexpr const char* dlFeedback = "dl_feedback"; inline constexpr const char* dlMix = "dl_mix";
inline constexpr const char* dlTone = "dl_tone"; inline constexpr const char* dlSync = "dl_sync";
inline constexpr const char* dlDiv = "dl_div"; inline constexpr const char* dlDotted = "dl_dotted";
inline constexpr const char* dlTriplet = "dl_triplet"; inline constexpr const char* dlStereo = "dl_stereo";
// chorus
inline constexpr const char* chOn = "ch_on";   inline constexpr const char* chRate = "ch_rate";
inline constexpr const char* chDepth = "ch_depth"; inline constexpr const char* chWidth = "ch_width";
inline constexpr const char* chMix = "ch_mix";
// stereo widener
inline constexpr const char* swOn = "sw_on";   inline constexpr const char* swWidth = "sw_width";
inline constexpr const char* swMix = "sw_mix";
} // namespace msc::ids

namespace msc
{
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
juce::StringArray getScaleNames();
juce::StringArray getNoteNames();
juce::StringArray getAutoTuneModeNames();
juce::StringArray getSaturationTypeNames();
juce::StringArray getReverbTypeNames();
juce::StringArray getDelayDivisionNames();
} // namespace msc
