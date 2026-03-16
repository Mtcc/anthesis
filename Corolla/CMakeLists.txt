juce_add_plugin(Corolla
    COMPANY_NAME "Anthesis"
    PLUGIN_MANUFACTURER_CODE Anth
    PLUGIN_CODE Cor1
    FORMATS VST3 AU
    PRODUCT_NAME "Corolla"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES EQ Spatial
)

target_sources(Corolla PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/BotanicalLookAndFeel.cpp
    Source/ParticleSystem.cpp
    Source/NatureLayer.cpp
)

target_compile_definitions(Corolla PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Corolla PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
