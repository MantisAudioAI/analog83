#include "VoicePanel.h"

namespace a83ui {

namespace {

using a83::Param;
using a83::VoiceId;

const char* const kTabNames[6] = { "ENVELOPE", "PITCH", "FILTER", "DIST", "CLICK", "OUTPUT" };
const juce::Colour kKnobColours[4] = { colours::cyan, colours::pink, colours::purple, colours::teal };

constexpr int P(Param p) { return (int) p; }
constexpr int N = -1; // empty knob slot

// Which parameter sits in each of the 4 knob slots, per tab.
// A tab with no parameters is shown greyed out.
using TabLayout = std::array<std::array<int, 4>, 6>;

const TabLayout kKickTabs = { {
    { P(Param::KickTune), P(Param::KickDecay), P(Param::KickPunch), P(Param::KickLevel) }, // ENVELOPE
    { P(Param::KickTune), P(Param::KickPunch), N, N },                                     // PITCH
    { P(Param::KickTone), N, N, N },                                                        // FILTER
    { P(Param::KickDrive), P(Param::KickTone), P(Param::BusDrive), N },                     // DIST
    { N, N, N, N },                                                                         // CLICK
    { P(Param::KickLevel), P(Param::BusComp), P(Param::Record), P(Param::Room) },           // OUTPUT
} };

const TabLayout kClosedHatTabs = { {
    { P(Param::HatTune), P(Param::HatClosedDecay), P(Param::HatTone), P(Param::HatLevel) },
    { P(Param::HatTune), N, N, N },
    { P(Param::HatTone), N, N, N },
    { P(Param::HatDrive), P(Param::BusDrive), N, N },
    { N, N, N, N },
    { P(Param::HatLevel), P(Param::BusComp), P(Param::Record), P(Param::Room) },
} };

const TabLayout kOpenHatTabs = { {
    { P(Param::HatTune), P(Param::HatOpenDecay), P(Param::HatTone), P(Param::HatLevel) },
    { P(Param::HatTune), N, N, N },
    { P(Param::HatTone), N, N, N },
    { P(Param::HatDrive), P(Param::BusDrive), N, N },
    { N, N, N, N },
    { P(Param::HatLevel), P(Param::BusComp), P(Param::Record), P(Param::Room) },
} };

const TabLayout* layoutFor(int padIndex)
{
    const auto& voice = padDef(padIndex).voice;
    if (! voice) return nullptr;
    switch (*voice)
    {
        case VoiceId::Kick:      return &kKickTabs;
        case VoiceId::ClosedHat: return &kClosedHatTabs;
        case VoiceId::OpenHat:   return &kOpenHatTabs;
        case VoiceId::Count:     break;
    }
    return nullptr;
}

double previewSeconds(a83::Kit& kit, VoiceId v)
{
    switch (v)
    {
        case VoiceId::Kick:      return juce::jlimit(0.15, 2.0, kit.getParam(Param::KickDecay) * 1.1);
        case VoiceId::ClosedHat: return juce::jlimit(0.06, 0.4, kit.getParam(Param::HatClosedDecay) * 1.5);
        case VoiceId::OpenHat:   return juce::jlimit(0.2, 2.0, kit.getParam(Param::HatOpenDecay) * 1.2);
        case VoiceId::Count:     break;
    }
    return 0.5;
}

} // namespace

VoicePanel::VoicePanel(a83::Kit& k) : kit(k)
{
    for (int i = 0; i < kNumKnobs; ++i)
    {
        auto& knob = knobs[(size_t) i];
        knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        knob.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                                 juce::MathConstants<float>::pi * 2.75f, true);
        knob.setColour(juce::Slider::rotarySliderFillColourId, kKnobColours[i]);
        knob.setLookAndFeel(&lookAndFeel);
        knob.setWantsKeyboardFocus(false);
        knob.setMouseClickGrabsKeyboardFocus(false);
        knob.onValueChange = [this, i]
        {
            const int param = knobParams[(size_t) i];
            if (param < 0)
                return;
            kit.setParam((Param) param, (float) knobs[(size_t) i].getValue());
            repaint(slotAreas[(size_t) i]);
            requestWaveform();
        };
        addChildComponent(knob);
    }

    showPad(0);
}

VoicePanel::~VoicePanel()
{
    for (auto& knob : knobs)
        knob.setLookAndFeel(nullptr);
}

void VoicePanel::requestWaveform()
{
    waveDirty = true;
    if (! isTimerRunning())
        startTimerHz(30);
}

void VoicePanel::showPad(int padIndex)
{
    pad = padIndex;
    tab = 0;
    updateKnobs();
    requestWaveform();
    repaint();
}

int VoicePanel::paramForSlot(int tabIndex, int slot) const
{
    const auto* layout = layoutFor(pad);
    return layout ? (*layout)[(size_t) tabIndex][(size_t) slot] : -1;
}

bool VoicePanel::tabHasKnobs(int tabIndex) const
{
    for (int s = 0; s < kNumKnobs; ++s)
        if (paramForSlot(tabIndex, s) >= 0)
            return true;
    return false;
}

void VoicePanel::setTab(int tabIndex)
{
    if (! tabHasKnobs(tabIndex))
        return;
    tab = tabIndex;
    updateKnobs();
    repaint();
}

void VoicePanel::updateKnobs()
{
    for (int s = 0; s < kNumKnobs; ++s)
    {
        auto& knob = knobs[(size_t) s];
        const int param = paramForSlot(tab, s);
        knobParams[(size_t) s] = -1; // stop onValueChange while reconfiguring

        if (param < 0)
        {
            knob.setVisible(false);
            continue;
        }

        const auto p = (Param) param;
        const auto& info = a83::paramInfo(p);
        const bool stepped = info.unit == a83::Unit::Semitones || info.unit == a83::Unit::Percent;

        knob.setRange(info.min, info.max, stepped ? 1.0 : 0.0);
        if (p == Param::KickDecay)      knob.setSkewFactorFromMidPoint(0.3);
        else if (p == Param::HatClosedDecay) knob.setSkewFactorFromMidPoint(0.07);
        else if (p == Param::HatOpenDecay)   knob.setSkewFactorFromMidPoint(0.5);
        else if (info.unit == a83::Unit::Decibels) knob.setSkewFactorFromMidPoint(-12.0);
        else knob.setSkewFactor(1.0);

        knob.setDoubleClickReturnValue(true, info.defaultValue); // double-click resets
        knob.setValue(kit.getParam(p), juce::dontSendNotification);
        knob.setVisible(true);
        knobParams[(size_t) s] = param;
    }
}

void VoicePanel::resized()
{
    auto r = getLocalBounds();
    titleArea = r.removeFromTop(56);
    r.removeFromTop(8);
    bodyArea = r;

    auto inner = bodyArea.reduced(10);
    waveArea = inner.removeFromTop(170);
    inner.removeFromTop(10);

    auto tabsRow = inner.removeFromBottom(40);
    const int tabW = tabsRow.getWidth() / kNumTabs;
    for (int i = 0; i < kNumTabs; ++i)
        tabAreas[(size_t) i] = tabsRow.removeFromLeft(tabW).reduced(3, 0);

    inner.removeFromBottom(8);
    const int slotW = inner.getWidth() / kNumKnobs;
    for (int i = 0; i < kNumKnobs; ++i)
    {
        slotAreas[(size_t) i] = inner.removeFromLeft(slotW);
        // label 20 px, knob, captions 16 px, value box 30 px
        auto slot = slotAreas[(size_t) i];
        slot.removeFromTop(20);
        slot.removeFromBottom(46);
        knobs[(size_t) i].setBounds(slot.withSizeKeepingCentre(juce::jmin(slot.getWidth(), slot.getHeight()),
                                                               juce::jmin(slot.getWidth(), slot.getHeight())));
    }

    requestWaveform();
}

void VoicePanel::rebuildWaveform()
{
    waveDirty = false;
    waveColumns.clear();

    const auto& voice = padDef(pad).voice;
    const int columns = waveArea.getWidth() - 16;
    if (! voice || columns <= 0)
        return;

    // The kick's waveform looks identical at 16 kHz and renders ~3x faster;
    // hats keep the full rate so their fast detail is drawn correctly.
    const double previewRate = *voice == VoiceId::Kick ? 16000.0 : 48000.0;
    const auto samples = kit.renderPreview(*voice, previewSeconds(kit, *voice), previewRate);
    float peak = 0.0f;
    for (float s : samples)
        peak = juce::jmax(peak, std::abs(s));
    const float norm = peak > 0.0f ? 1.0f / peak : 1.0f;

    waveColumns.resize((size_t) columns);
    const double perColumn = (double) samples.size() / columns;
    for (int c = 0; c < columns; ++c)
    {
        const size_t from = (size_t) (c * perColumn);
        const size_t to = juce::jmax(from + 1, (size_t) ((c + 1) * perColumn));
        float lo = 0.0f, hi = 0.0f;
        for (size_t i = from; i < to && i < samples.size(); ++i)
        {
            lo = juce::jmin(lo, samples[i]);
            hi = juce::jmax(hi, samples[i]);
        }
        waveColumns[(size_t) c] = { lo * norm, hi * norm };
    }
}

void VoicePanel::timerCallback()
{
    if (! waveDirty)
    {
        stopTimer(); // nothing changed since the last frame: go idle
        return;
    }
    rebuildWaveform();
    repaint(waveArea);
}

void VoicePanel::paint(juce::Graphics& g)
{
    const auto& def = padDef(pad);
    const bool built = def.voice.has_value();

    // ---- Title bar
    g.setColour(colours::cyan);
    g.fillRoundedRectangle(titleArea.toFloat(), 6.0f);
    g.setColour(colours::background);
    g.setFont(font(42.0f, true, true));
    g.drawText(def.name, titleArea.withTrimmedLeft(16), juce::Justification::centredLeft, false);
    g.setFont(font(18.0f));
    g.drawText("|   VOICE EDIT", titleArea.withTrimmedRight(16), juce::Justification::centredRight, false);

    // ---- Body
    g.setColour(colours::panel);
    g.fillRoundedRectangle(bodyArea.toFloat(), 6.0f);
    g.setColour(colours::cyan.withAlpha(0.8f));
    g.drawRoundedRectangle(bodyArea.toFloat().reduced(1.0f), 6.0f, 2.0f);

    // ---- Waveform display
    const auto wave = waveArea.toFloat();
    g.setColour(colours::background);
    g.fillRect(wave);
    g.setColour(colours::cyan.withAlpha(0.12f));
    for (int i = 1; i < 10; ++i)
        g.drawVerticalLine(waveArea.getX() + waveArea.getWidth() * i / 10, wave.getY(), wave.getBottom());
    for (int i = 1; i < 4; ++i)
        g.drawHorizontalLine(waveArea.getY() + waveArea.getHeight() * i / 4, wave.getX(), wave.getRight());
    g.setColour(colours::outline);
    g.drawRect(wave, 1.0f);

    if (built)
    {
        const float mid = wave.getCentreY();
        const float scale = wave.getHeight() * 0.42f;
        const float x0 = wave.getX() + 8.0f;

        // Filled 1-px columns: much cheaper than antialiased lines in JUCE's
        // software renderer, and pixel-identical for vertical strokes.
        g.setColour(colours::pink.withAlpha(0.25f)); // glow
        for (size_t c = 0; c < waveColumns.size(); ++c)
        {
            const float top = mid - waveColumns[c].second * scale - 2.0f;
            const float bottom = mid - waveColumns[c].first * scale + 2.0f;
            g.fillRect(x0 + (float) c - 1.0f, top, 3.0f, bottom - top);
        }
        g.setColour(colours::pink);
        for (size_t c = 0; c < waveColumns.size(); ++c)
        {
            const float top = mid - waveColumns[c].second * scale;
            const float bottom = mid - waveColumns[c].first * scale + 1.0f;
            g.fillRect(x0 + (float) c, top, 1.0f, bottom - top);
        }
    }
    else
    {
        g.setColour(colours::textDim);
        g.setFont(font(20.0f));
        g.drawText("COMING IN A FOLLOW-UP PR", wave, juce::Justification::centred, false);
    }

    // SAMPLE / SYNTH toggle (SYNTH is the only engine for now)
    const auto toggle = juce::Rectangle<float>(wave.getRight() - 150.0f, wave.getY() + 8.0f, 142.0f, 26.0f);
    g.setColour(colours::panel);
    g.fillRect(toggle);
    g.setColour(colours::pink);
    g.fillRect(toggle.withTrimmedLeft(71.0f));
    g.setColour(colours::outline);
    g.drawRect(toggle, 1.0f);
    g.setFont(font(13.0f));
    g.setColour(colours::textDim);
    g.drawText("SAMPLE", toggle.withWidth(71.0f), juce::Justification::centred, false);
    g.setColour(colours::background);
    g.drawText("SYNTH", toggle.withTrimmedLeft(71.0f), juce::Justification::centred, false);

    if (! built)
        return;

    // ---- Knob labels, min/max captions and value boxes
    for (int s = 0; s < kNumKnobs; ++s)
    {
        const int param = knobParams[(size_t) s];
        if (param < 0)
            continue;

        const auto p = (Param) param;
        const auto& info = a83::paramInfo(p);
        auto slot = slotAreas[(size_t) s];

        g.setColour(kKnobColours[s]);
        g.setFont(font(15.0f));
        g.drawText(info.label, slot.removeFromTop(20), juce::Justification::centred, false);

        auto valueBox = slot.removeFromBottom(30).reduced(12, 2);
        auto captions = slot.removeFromBottom(16).reduced(8, 0);

        g.setColour(colours::textDim);
        g.setFont(font(12.0f, false));
        g.drawText(formatLimit(p, info.min), captions, juce::Justification::centredLeft, false);
        g.drawText(formatLimit(p, info.max), captions, juce::Justification::centredRight, false);

        g.setColour(colours::background);
        g.fillRect(valueBox);
        g.setColour(colours::cyan.withAlpha(0.8f));
        g.drawRect(valueBox, 1);
        g.setColour(colours::text);
        g.setFont(font(16.0f));
        g.drawText(formatValue(p, (float) knobs[(size_t) s].getValue()), valueBox,
                   juce::Justification::centred, false);
    }

    // ---- Tabs
    for (int t = 0; t < kNumTabs; ++t)
    {
        const auto r = tabAreas[(size_t) t].toFloat();
        const bool active = t == tab;
        const bool enabled = tabHasKnobs(t);
        g.setColour(active ? colours::pink : colours::panelLight);
        g.fillRoundedRectangle(r, 3.0f);
        g.setColour(active ? colours::pink : colours::outline);
        g.drawRoundedRectangle(r.reduced(0.5f), 3.0f, 1.0f);
        g.setColour(active ? colours::background : (enabled ? colours::cyan : colours::textDim.withAlpha(0.5f)));
        g.setFont(font(12.0f));
        g.drawText(kTabNames[t], r, juce::Justification::centred, false);
    }
}

void VoicePanel::mouseDown(const juce::MouseEvent& e)
{
    for (int t = 0; t < kNumTabs; ++t)
        if (tabAreas[(size_t) t].contains(e.getPosition()))
            setTab(t);
}

} // namespace a83ui
