// Top bar of the kit page: logo, kit name, BPM, play, page tabs.
#pragma once

#include "Theme.h"

#include <functional>

namespace a83ui {

class Header : public juce::Component
{
public:
    std::function<void()> onPlayToggle;
    std::function<void()> onAudioSettings;
    std::function<bool()> isPlaying;
    std::function<float()> getBpm;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    juce::Rectangle<int> logoArea, kitArea, bpmArea, playArea, audioArea;
    std::array<juce::Rectangle<int>, 4> tabAreas;
};

} // namespace a83ui
