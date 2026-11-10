juce_add_gui_app(Analog83Preview
    PRODUCT_NAME "Analog83 Preview")

target_sources(Analog83Preview PRIVATE
    Main.cpp
    MainComponent.cpp
    Header.cpp
    PadGrid.cpp
    VoicePanel.cpp
)

target_compile_definitions(Analog83Preview PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:Analog83Preview,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:Analog83Preview,JUCE_VERSION>")

target_link_libraries(Analog83Preview
    PRIVATE
        analog83_engine
        analog83_tools
        juce::juce_audio_utils
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
