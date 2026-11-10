// Right side of the kit page: voice name, live waveform, 4 knobs and the
// ENVELOPE / PITCH / FILTER / DIST / CLICK / OUTPUT tabs.
#pragma once

#include "PadGrid.h"

namespace a83ui {

class VoicePanel : public juce::Component, private juce::Timer
{
public:
    explicit VoicePanel(a83::Kit& kit);
    ~VoicePanel() override;

    void showPad(int padIndex);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    static constexpr int kNumKnobs = 4;
    static constexpr int kNumTabs = 6;

    void timerCallback() override;
    void setTab(int tabIndex);
    void updateKnobs();
    void rebuildWaveform();
    void requestWaveform(); // redraws at most 30 times a second, then the timer stops
    bool tabHasKnobs(int tabIndex) const;
    int paramForSlot(int tabIndex, int slot) const;

    a83::Kit& kit;
    NeonLookAndFeel lookAndFeel;

    int pad = 0;
    int tab = 0;
    std::array<juce::Slider, kNumKnobs> knobs;
    std::array<int, kNumKnobs> knobParams { -1, -1, -1, -1 };

    std::vector<std::pair<float, float>> waveColumns; // min, max per pixel column
    bool waveDirty = true;

    juce::Rectangle<int> titleArea, bodyArea, waveArea;
    std::array<juce::Rectangle<int>, kNumKnobs> slotAreas;
    std::array<juce::Rectangle<int>, kNumTabs> tabAreas;
};

} // namespace a83ui
