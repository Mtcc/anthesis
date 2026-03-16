cmake_minimum_required(VERSION 3.22)
project(Understory VERSION 1.0.0)

juce_add_plugin(Understory
    COMPANY_NAME "Anthesis"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Anth
    PLUGIN_CODE Und1
    FORMATS VST3 AU
    PRODUCT_NAME "Understory"
    VST3_CATEGORIES Instrument Synth
)

target_sources(Understory PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/BotanicalLookAndFeel.cpp
)

target_compile_definitions(Understory PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Understory PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
