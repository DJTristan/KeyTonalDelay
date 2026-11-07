cmake_minimum_required(VERSION 3.22)
project(KeyTonalDelay VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_PLUGIN "Build JUCE AU/VST3 plugins" ON)
option(BUILD_TESTING "Build dependency-free DSP tests" ON)

if(BUILD_TESTING)
    enable_testing()
    add_executable(tonal_delay_dsp_tests tests/tonal_delay_tests.cpp)
    target_include_directories(tonal_delay_dsp_tests PRIVATE src)
    add_test(NAME tonal_delay_dsp_tests COMMAND tonal_delay_dsp_tests)
endif()

if(BUILD_PLUGIN)
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)

    juce_add_plugin(KeyTonalDelay
        COMPANY_NAME "Independent Audio"
        BUNDLE_ID "com.independentaudio.keytonaldelay"
        PLUGIN_MANUFACTURER_CODE KTDY
        PLUGIN_CODE KtDl
        FORMATS AU VST3
        PRODUCT_NAME "Key Tonal Delay"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Fx Delay
        AU_MAIN_TYPE kAudioUnitType_Effect
    )
    juce_generate_juce_header(KeyTonalDelay)
    target_sources(KeyTonalDelay PRIVATE
        src/PluginProcessor.cpp
        src/PluginProcessor.h
        src/TonalDelayEngine.h
    )
    target_include_directories(KeyTonalDelay PRIVATE src)
    target_compile_definitions(KeyTonalDelay PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
    )
    target_link_libraries(KeyTonalDelay PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_recommended_config_flags
    )
endif()
