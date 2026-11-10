// The kit page: header, 4x4 pads and the voice edit panel, plus the audio
// output that plays the Analog '83 kit.
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "Analog83DemoPattern.h"
#include "Header.h"
#include "PadGrid.h"
#include "VoicePanel.h"

namespace a83ui {

class MainComponent : public juce::AudioAppComponent
{
public:
    MainComponent();
    ~MainComponent() override;

    // Audio
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override;
    void releaseResources() override {}

    // UI
    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    bool keyStateChanged(bool isKeyDown) override;

private:
    void pressPad(int padIndex);
    void togglePlay();
    void showAudioSettings();
    void requestLowLatency();

    std::array<bool, 16> keyHeld {}; // pads played from the computer keyboard

    a83::Kit kit;

    // Play button: the app owns the demo beat, not the engine (on KICK,
    // tempo and patterns come from KICK's own sequencer).
    a83::DemoPattern demo;           // used on the audio thread only
    std::atomic<bool> playing { false };
    bool wasPlaying = false;         // audio thread only
    std::vector<float> left, right;

    Header header;
    PadGrid pads { kit };
    VoicePanel voicePanel { kit };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace a83ui
