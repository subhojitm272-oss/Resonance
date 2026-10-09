cmake_minimum_required(VERSION 3.22)
project(Resonance VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Resonance
    COMPANY_NAME "Resonance Audio"
    BUNDLE_ID "audio.resonance.pitchcoach"
    PLUGIN_MANUFACTURER_CODE Rson
    PLUGIN_CODE Rsn1
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Resonance"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
)

target_sources(Resonance PRIVATE
    main.cpp
    processor.cpp
    processor.h
)
target_compile_definitions(Resonance PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)
target_link_libraries(Resonance PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
