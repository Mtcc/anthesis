juce_add_plugin(Resin
    COMPANY_NAME "Anthesis"
    PLUGIN_MANUFACTURER_CODE Anth
    PLUGIN_CODE Rsn1
    FORMATS VST3 AU
    PRODUCT_NAME "Resin"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Distortion
)

target_sources(Resin PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/BotanicalLookAndFeel.cpp
    Source/ParticleSystem.cpp
)

target_compile_definitions(Resin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Resin PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
