cmake_minimum_required(VERSION 3.22)
project(SpectralStrip VERSION 0.7.4 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS")
set(JUCE_PATH "/Applications/JUCE" CACHE PATH "Local JUCE source directory")
if(NOT EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(FATAL_ERROR "Set JUCE_PATH to your JUCE installation")
endif()
add_subdirectory("${JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")
juce_add_plugin(SpectralStrip
    COMPANY_NAME "Giorgio Audio"
    BUNDLE_ID "audio.giorgio.spectralstrip"
    PLUGIN_MANUFACTURER_CODE GiAu
    PLUGIN_CODE SpSt
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Spectral Strip"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Spectral Strip needs audio input to process sound."
)
target_sources(SpectralStrip PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp
    Source/PluginProcessor.h Source/PluginEditor.h Source/SpectralEngine.h)
target_compile_definitions(SpectralStrip PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(SpectralStrip PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
option(SPECTRAL_BUILD_TESTS "Build DSP regression tests" ON)
if(SPECTRAL_BUILD_TESTS)
    juce_add_console_app(SpectralStripTests PRODUCT_NAME "SpectralStripTests")
    target_sources(SpectralStripTests PRIVATE Tests/EngineTests.cpp Tests/ProcessorTests.cpp Tests/AuditTests.cpp Tests/ReverbTests.cpp
        Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(SpectralStripTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(SpectralStripTests PRIVATE juce::juce_dsp juce::juce_audio_utils)
    enable_testing()
    add_test(NAME SpectralEngine COMMAND SpectralStripTests)
    juce_add_gui_app(SpectralStripPreview PRODUCT_NAME "Spectral Strip Preview" BUNDLE_ID "audio.giorgio.spectralstrip.preview")
    target_sources(SpectralStripPreview PRIVATE Tests/EditorPreview.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(SpectralStripPreview PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(SpectralStripPreview PRIVATE juce::juce_audio_utils juce::juce_dsp)
endif()
