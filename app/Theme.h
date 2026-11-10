// Analog '83 neon theme: colours, fonts, knob drawing and value formatting.
#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "Analog83Kit.h"

namespace a83ui {

// ---- Colours (neon 1983 palette from the mockups)
namespace colours {
const juce::Colour background { 0xff0a0f24 };
const juce::Colour panel      { 0xff0f1833 };
const juce::Colour panelLight { 0xff16224a };
const juce::Colour outline    { 0xff2a3d7a };
const juce::Colour pink       { 0xffff2e9a };
const juce::Colour cyan       { 0xff22d3ee };
const juce::Colour purple     { 0xffa855f7 };
const juce::Colour teal       { 0xff2de2c4 };
const juce::Colour cream      { 0xfff4ecdc };
const juce::Colour text       { 0xffdce6ff };
const juce::Colour textDim    { 0xff6b7aa8 };
} // namespace colours

// JUCE 7 font constructor (the KICK app uses JUCE 7.0.12).
inline juce::Font font(float height, bool bold = true, bool italic = false)
{
    int style = juce::Font::plain;
    if (bold)   style |= juce::Font::bold;
    if (italic) style |= juce::Font::italic;
    return juce::Font(height, style);
}

// ---- Value text for knobs, matching the mockup ("380 ms", "0.0 dB", "+3")
inline juce::String formatValue(a83::Param p, float v)
{
    switch (a83::paramInfo(p).unit)
    {
        case a83::Unit::Semitones:
        {
            const int st = juce::roundToInt(v);
            return st > 0 ? "+" + juce::String(st) : juce::String(st);
        }
        case a83::Unit::Seconds:
            return v < 1.0f ? juce::String(juce::roundToInt(v * 1000.0f)) + " ms"
                            : juce::String(v, 2) + " s";
        case a83::Unit::Percent:
            return juce::String(juce::roundToInt(v));
        case a83::Unit::Decibels:
            return v <= -59.5f ? juce::String::fromUTF8("-\xe2\x88\x9e dB")
                               : juce::String(v, 1) + " dB";
    }
    return juce::String(v);
}

// Min / max captions under each knob ("-24  +24", "10 ms  2.0 s").
inline juce::String formatLimit(a83::Param p, float v)
{
    const auto unit = a83::paramInfo(p).unit;
    if (unit == a83::Unit::Decibels && v <= -59.5f)
        return juce::String::fromUTF8("-\xe2\x88\x9e");
    if (unit == a83::Unit::Seconds)
        return v < 1.0f ? juce::String(juce::roundToInt(v * 1000.0f)) + " ms"
                        : juce::String(v, 1) + " s";
    if (unit == a83::Unit::Decibels)
        return v > 0.0f ? "+" + juce::String(juce::roundToInt(v)) : juce::String(juce::roundToInt(v));
    return formatValue(p, v);
}

// ---- Knob drawing: tick ring, dim track, glowing value arc, dark body
class NeonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float pos, float startAngle, float endAngle, juce::Slider& slider) override
    {
        const auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId);
        const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
        const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);

        // Tick marks
        g.setColour(colours::textDim.withAlpha(0.6f));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
            const auto inner = centre.getPointOnCircumference(radius - 2.0f, a);
            const auto outer = centre.getPointOnCircumference(radius + 2.0f, a);
            g.drawLine({ inner, outer }, 1.2f);
        }

        // Track and value arc
        const float arcRadius = radius - 8.0f;
        juce::Path track, value;
        track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
        value.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);

        g.setColour(colours::outline);
        g.strokePath(track, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(accent.withAlpha(0.25f));
        g.strokePath(value, juce::PathStrokeType(10.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(accent);
        g.strokePath(value, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Knob body
        const float bodyRadius = arcRadius - 7.0f;
        const auto body = juce::Rectangle<float>(bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre(centre);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff2b3350), centre.x, body.getY(),
                                               juce::Colour(0xff0b0f1c), centre.x, body.getBottom(), false));
        g.fillEllipse(body);
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.drawEllipse(body, 1.5f);

        // Pointer
        const auto tip = centre.getPointOnCircumference(bodyRadius - 4.0f, angle);
        const auto base = centre.getPointOnCircumference(bodyRadius * 0.35f, angle);
        g.setColour(juce::Colours::white);
        g.drawLine({ base, tip }, 3.0f);
    }
};

} // namespace a83ui
