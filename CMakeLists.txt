cmake_minimum_required(VERSION 3.22)

# Static MSVC runtime: the plugin runs without installing the Visual C++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)

project(MSC_VOXITY VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(MSC_BUILD_TESTS "Build the stand-alone DSP self test" OFF)

# ----------------------------------------------------------------------------------------------
# DSP self test (plain C++, no JUCE needed)
# ----------------------------------------------------------------------------------------------
if(MSC_BUILD_TESTS)
    add_executable(msc_dsp_test tests/dsp_test.cpp)
    target_include_directories(msc_dsp_test PRIVATE Source/DSP)
    if(MSVC)
        target_compile_options(msc_dsp_test PRIVATE /O2 /EHsc)
    else()
        target_compile_options(msc_dsp_test PRIVATE -O2)
    endif()
    enable_testing()
    add_test(NAME dsp_self_test COMMAND msc_dsp_test)
endif()

# ----------------------------------------------------------------------------------------------
# JUCE (downloaded automatically at configure time)
# ----------------------------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# ----------------------------------------------------------------------------------------------
# Embedded resources: artwork, presets, themes
# ----------------------------------------------------------------------------------------------
file(GLOB MSC_IMAGE_FILES  CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Images/*.jpg"
                                              "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Images/*.png")
file(GLOB MSC_PRESET_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Presets/*.json")
file(GLOB MSC_THEME_FILES  CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Themes/*.json")

juce_add_binary_data(MSCVoxityAssets SOURCES ${MSC_IMAGE_FILES} ${MSC_PRESET_FILES} ${MSC_THEME_FILES})
set_target_properties(MSCVoxityAssets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

# ----------------------------------------------------------------------------------------------
# Plugin
# ----------------------------------------------------------------------------------------------
juce_add_plugin(MSCVoxity
    COMPANY_NAME                 "MSC"
    PLUGIN_MANUFACTURER_CODE     Mscv
    PLUGIN_CODE                  Mvx2
    FORMATS                      VST3
    PRODUCT_NAME                 "MSC VOXITY 2.0"
    IS_SYNTH                     FALSE
    NEEDS_MIDI_INPUT             FALSE
    NEEDS_MIDI_OUTPUT            FALSE
    IS_MIDI_EFFECT               FALSE
    VST3_CATEGORIES              Fx Pitch\ Shift
    COPY_PLUGIN_AFTER_BUILD      FALSE)

target_sources(MSCVoxity PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/UIComponents.h
    Source/LookAndFeel.h
    Source/Parameters.cpp
    Source/Parameters.h
    Source/PresetManager.cpp
    Source/PresetManager.h
    Source/ThemeManager.h
    Source/Assets.h
    Source/DSP/MscDsp.h
    Source/DSP/DspUtils.h
    Source/DSP/Biquad.h
    Source/DSP/Oversampler.h
    Source/DSP/PitchDetector.h
    Source/DSP/PsolaShifter.h
    Source/DSP/AutoTune.h
    Source/DSP/Compressor.h
    Source/DSP/DeEsser.h
    Source/DSP/ParametricEQ.h
    Source/DSP/Air.h
    Source/DSP/Distortion.h
    Source/DSP/Saturation.h
    Source/DSP/Reverb.h
    Source/DSP/Delay.h
    Source/DSP/Chorus.h
    Source/DSP/StereoWidener.h)

target_include_directories(MSCVoxity PRIVATE Source)

target_compile_definitions(MSCVoxity PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(MSCVoxity
    PRIVATE
        MSCVoxityAssets
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

if(MSVC)
    target_compile_options(MSCVoxity PRIVATE /W3 /utf-8)
endif()
