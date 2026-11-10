// 4x4 pad grid for the kit page. Pads 01 KICK, 05 CL HAT and 06 OP HAT are
// playable; the rest are shown as coming in follow-up PRs.
#pragma once

#include "Theme.h"

#include <functional>
#include <optional>

namespace a83ui {

enum class PadIcon { Kick, Drum, Clap, Rim, Cymbal, Cowbell, Conga, Sticks, Shaker };

struct PadDef
{
    const char* name;
    PadIcon icon;
    juce::Colour accent;
    std::optional<a83::VoiceId> voice; // empty = not built yet
};

constexpr int kNumPads = 16;
const PadDef& padDef(int index);

class PadGrid : public juce::Component, private juce::Timer
{
public:
    explicit PadGrid(a83::Kit& kit);

    // Called when a pad is pressed (mouse, touch or keyboard).
    std::function<void(int padIndex)> onPadPressed;

    // Whether the demo beat is playing (keeps the pad timer running).
    std::function<bool()> isPlaying;

    void setSelectedPad(int index);
    int getSelectedPad() const { return selected; }

    // Lights a pad as if it was hit.
    void flashPad(int index);

    // Call when the sequencer starts, so pads flash with the beat. The pad
    // timer runs only while playing or while a pad is still fading, so the
    // grid costs nothing when idle.
    void wake();

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    void timerCallback() override;
    void paintPad(juce::Graphics& g, int index);
    void repaintPad(int index);

    a83::Kit& kit;
    std::array<juce::Rectangle<float>, kNumPads> padRects;
    std::array<float, kNumPads> flash {};
    std::array<uint32_t, a83::kNumVoices> lastHits {};
    int selected = 0;
};

} // namespace a83ui
