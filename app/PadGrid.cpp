#include "PadGrid.h"

namespace a83ui {

namespace {

using a83::VoiceId;

const PadDef kPads[kNumPads] = {
    { "KICK",     PadIcon::Kick,    colours::pink,   VoiceId::Kick },
    { "SNARE",    PadIcon::Drum,    colours::pink,   std::nullopt },
    { "CLAP",     PadIcon::Clap,    colours::purple, std::nullopt },
    { "RIM",      PadIcon::Rim,     colours::cyan,   std::nullopt },
    { "CL HAT",   PadIcon::Cymbal,  colours::cyan,   VoiceId::ClosedHat },
    { "OP HAT",   PadIcon::Cymbal,  colours::pink,   VoiceId::OpenHat },
    { "CYMBAL",   PadIcon::Cymbal,  colours::purple, std::nullopt },
    { "COWBELL",  PadIcon::Cowbell, colours::teal,   std::nullopt },
    { "LO TOM",   PadIcon::Drum,    colours::purple, std::nullopt },
    { "MID TOM",  PadIcon::Drum,    colours::cyan,   std::nullopt },
    { "HI TOM",   PadIcon::Drum,    colours::pink,   std::nullopt },
    { "CONGA",    PadIcon::Conga,   colours::purple, std::nullopt },
    { "LO CONGA", PadIcon::Conga,   colours::cyan,   std::nullopt },
    { "HI CONGA", PadIcon::Conga,   colours::pink,   std::nullopt },
    { "CLAVE",    PadIcon::Sticks,  colours::teal,   std::nullopt },
    { "SHAKER",   PadIcon::Shaker,  colours::purple, std::nullopt },
};

void drawIcon(juce::Graphics& g, PadIcon icon, juce::Rectangle<float> r, juce::Colour c)
{
    const float stroke = 2.2f;
    g.setColour(c);
    const auto centre = r.getCentre();

    switch (icon)
    {
        case PadIcon::Kick:
        {
            const auto outer = r.withSizeKeepingCentre(r.getHeight(), r.getHeight());
            g.drawEllipse(outer, stroke);
            g.drawEllipse(outer.reduced(outer.getWidth() * 0.18f), stroke);
            g.fillEllipse(juce::Rectangle<float>(5.0f, 5.0f).withCentre(centre));
            break;
        }
        case PadIcon::Drum:
        case PadIcon::Rim:
        {
            const float h = icon == PadIcon::Rim ? r.getHeight() * 0.45f : r.getHeight() * 0.8f;
            const auto body = r.withSizeKeepingCentre(r.getHeight() * 1.3f, h);
            const float lipH = body.getHeight() * (icon == PadIcon::Rim ? 0.5f : 0.3f);
            g.drawEllipse(body.withHeight(lipH), stroke);
            g.drawLine(body.getX(), body.getY() + lipH * 0.5f, body.getX(), body.getBottom() - lipH * 0.5f, stroke);
            g.drawLine(body.getRight(), body.getY() + lipH * 0.5f, body.getRight(), body.getBottom() - lipH * 0.5f, stroke);
            juce::Path bottom;
            bottom.addCentredArc(body.getCentreX(), body.getBottom() - lipH * 0.5f, body.getWidth() * 0.5f,
                                 lipH * 0.5f, 0.0f, juce::MathConstants<float>::halfPi,
                                 juce::MathConstants<float>::halfPi * 3.0f, true);
            g.strokePath(bottom, juce::PathStrokeType(stroke));
            break;
        }
        case PadIcon::Clap:
        {
            for (int i = 0; i < 2; ++i)
            {
                juce::Path palm;
                palm.addRoundedRectangle(juce::Rectangle<float>(r.getHeight() * 0.42f, r.getHeight() * 0.9f)
                                             .withCentre(centre.translated(i == 0 ? -5.0f : 5.0f, 0.0f)),
                                         8.0f);
                palm.applyTransform(juce::AffineTransform::rotation(i == 0 ? -0.35f : 0.35f, centre.x, centre.y));
                g.strokePath(palm, juce::PathStrokeType(stroke));
            }
            break;
        }
        case PadIcon::Cymbal:
        {
            const auto plate = r.withSizeKeepingCentre(r.getHeight() * 1.5f, r.getHeight() * 0.28f)
                                   .translated(0.0f, -r.getHeight() * 0.12f);
            g.drawEllipse(plate, stroke);
            g.fillEllipse(juce::Rectangle<float>(8.0f, 6.0f).withCentre(plate.getCentre().translated(0.0f, -3.0f)));
            g.drawLine(centre.x, plate.getY() - 6.0f, centre.x, r.getBottom(), stroke);
            g.drawLine(centre.x - 10.0f, r.getBottom(), centre.x + 10.0f, r.getBottom(), stroke);
            break;
        }
        case PadIcon::Cowbell:
        {
            const float w = r.getHeight();
            juce::Path bell;
            bell.startNewSubPath(centre.x - w * 0.25f, r.getY() + 4.0f);
            bell.lineTo(centre.x + w * 0.25f, r.getY() + 4.0f);
            bell.lineTo(centre.x + w * 0.45f, r.getBottom() - 4.0f);
            bell.lineTo(centre.x - w * 0.45f, r.getBottom() - 4.0f);
            bell.closeSubPath();
            g.strokePath(bell, juce::PathStrokeType(stroke, juce::PathStrokeType::curved));
            break;
        }
        case PadIcon::Conga:
        {
            const auto body = r.withSizeKeepingCentre(r.getHeight() * 0.55f, r.getHeight());
            g.drawRoundedRectangle(body, body.getWidth() * 0.35f, stroke);
            g.drawEllipse(body.withHeight(body.getHeight() * 0.2f), stroke);
            break;
        }
        case PadIcon::Sticks:
        {
            const float s = r.getHeight() * 0.45f;
            g.drawLine(centre.x - s, centre.y + s * 0.6f, centre.x + s * 0.4f, centre.y - s, stroke * 2.2f);
            g.drawLine(centre.x - s * 0.4f, centre.y + s, centre.x + s, centre.y - s * 0.6f, stroke * 2.2f);
            break;
        }
        case PadIcon::Shaker:
        {
            const auto body = r.withSizeKeepingCentre(r.getHeight() * 0.5f, r.getHeight() * 0.9f);
            juce::Path p;
            p.addRoundedRectangle(body, body.getWidth() * 0.45f);
            p.applyTransform(juce::AffineTransform::rotation(0.6f, centre.x, centre.y));
            g.strokePath(p, juce::PathStrokeType(stroke));
            for (int i = -1; i <= 1; ++i)
                for (int j = -1; j <= 1; ++j)
                    g.fillEllipse(juce::Rectangle<float>(3.0f, 3.0f)
                                      .withCentre(centre.translated((float) i * 5.0f + (float) j * 3.0f,
                                                                    (float) j * 6.0f - (float) i * 3.0f)));
            break;
        }
    }
}

} // namespace

const PadDef& padDef(int index) { return kPads[juce::jlimit(0, kNumPads - 1, index)]; }

PadGrid::PadGrid(a83::Kit& k) : kit(k)
{
    for (int v = 0; v < a83::kNumVoices; ++v)
        lastHits[(size_t) v] = kit.getHitCount((a83::VoiceId) v);
}

void PadGrid::setSelectedPad(int index)
{
    const int previous = selected;
    selected = juce::jlimit(0, kNumPads - 1, index);
    if (previous != selected)
    {
        repaintPad(previous); // only the two pads that changed
        repaintPad(selected);
    }
}

void PadGrid::flashPad(int index)
{
    const int i = juce::jlimit(0, kNumPads - 1, index);
    flash[(size_t) i] = 1.0f;
    repaintPad(i);
    wake();
}

void PadGrid::wake()
{
    if (! isTimerRunning())
        startTimerHz(60);
}

void PadGrid::repaintPad(int index)
{
    // The selection glow extends 2 px outside the pad
    repaint(padRects[(size_t) index].expanded(3.0f).getSmallestIntegerContainer());
}

void PadGrid::resized()
{
    const float gap = 10.0f;
    const float w = ((float) getWidth() - gap * 3.0f) / 4.0f;
    const float h = ((float) getHeight() - gap * 3.0f) / 4.0f;
    for (int i = 0; i < kNumPads; ++i)
    {
        const int row = i / 4, col = i % 4; // pad 01 is top-left, like the mockup
        padRects[(size_t) i] = { (float) col * (w + gap), (float) row * (h + gap), w, h };
    }
}

void PadGrid::paint(juce::Graphics& g)
{
    // Only draw pads inside the area being repainted (usually one or two)
    const auto clip = g.getClipBounds().toFloat();
    for (int i = 0; i < kNumPads; ++i)
        if (padRects[(size_t) i].expanded(3.0f).intersects(clip))
            paintPad(g, i);
}

void PadGrid::paintPad(juce::Graphics& g, int index)
{
    const auto& def = padDef(index);
    const auto r = padRects[(size_t) index];
    const bool isSelected = index == selected;
    const bool built = def.voice.has_value();
    const float f = flash[(size_t) index];
    const float corner = 8.0f;

    // Body
    g.setColour(isSelected ? colours::cyan : colours::panel);
    g.fillRoundedRectangle(r, corner);
    if (f > 0.0f)
    {
        g.setColour(def.accent.withAlpha(0.55f * f));
        g.fillRoundedRectangle(r, corner);
    }

    // Accent strip along the top
    {
        juce::Graphics::ScopedSaveState save(g);
        juce::Path clip;
        clip.addRoundedRectangle(r, corner);
        g.reduceClipRegion(clip);
        g.setColour(def.accent.withAlpha(built ? 1.0f : 0.5f));
        g.fillRect(r.withHeight(10.0f));
    }

    // Border: pink glow when selected
    if (isSelected)
    {
        g.setColour(colours::pink.withAlpha(0.35f));
        g.drawRoundedRectangle(r.expanded(2.0f), corner + 2.0f, 4.0f);
        g.setColour(colours::pink);
        g.drawRoundedRectangle(r.reduced(1.0f), corner, 3.0f);
    }
    else
    {
        g.setColour(colours::outline.withAlpha(0.8f));
        g.drawRoundedRectangle(r.reduced(0.5f), corner, 1.2f);
    }

    const auto ink = isSelected ? colours::background : colours::text;
    const float alpha = built ? 1.0f : 0.45f;

    // Pad number
    g.setColour((isSelected ? colours::background : def.accent).withAlpha(alpha));
    g.setFont(font(13.0f));
    g.drawText(juce::String(index + 1).paddedLeft('0', 2), r.withTrimmedTop(12.0f).withTrimmedLeft(8.0f).withHeight(16.0f),
               juce::Justification::centredLeft, false);

    // "SOON" badge for voices that aren't built yet
    if (! built)
    {
        g.setColour(colours::textDim);
        g.setFont(font(10.0f));
        g.drawText("SOON", r.withTrimmedTop(12.0f).withTrimmedRight(8.0f).withHeight(16.0f),
                   juce::Justification::centredRight, false);
    }

    // Icon and name
    const auto iconArea = r.withTrimmedTop(28.0f).withTrimmedBottom(34.0f).reduced(10.0f, 4.0f);
    drawIcon(g, def.icon, iconArea, (isSelected ? colours::background : def.accent).withAlpha(alpha));

    g.setColour(ink.withAlpha(alpha));
    g.setFont(font(17.0f));
    g.drawText(def.name, r.withTrimmedTop(r.getHeight() - 32.0f).reduced(4.0f, 0.0f),
               juce::Justification::centred, true);
}

void PadGrid::mouseDown(const juce::MouseEvent& e)
{
    const auto p = e.position;
    for (int i = 0; i < kNumPads; ++i)
    {
        if (padRects[(size_t) i].contains(p))
        {
            if (onPadPressed)
                onPadPressed(i);
            return;
        }
    }
}

void PadGrid::timerCallback()
{
    // Flash every pad whose voice was triggered since the last check
    // (covers hits from the demo sequencer, not just clicks).
    for (int v = 0; v < a83::kNumVoices; ++v)
    {
        const auto hits = kit.getHitCount((a83::VoiceId) v);
        if (hits != lastHits[(size_t) v])
        {
            lastHits[(size_t) v] = hits;
            for (int i = 0; i < kNumPads; ++i)
                if (padDef(i).voice == (a83::VoiceId) v)
                    flash[(size_t) i] = 1.0f;
        }
    }

    bool anyLit = false;
    for (int i = 0; i < kNumPads; ++i)
    {
        auto& f = flash[(size_t) i];
        if (f > 0.0f)
        {
            f = f < 0.02f ? 0.0f : f * 0.82f;
            repaintPad(i); // only pads that are actually fading
            anyLit = true;
        }
    }

    if (! anyLit && ! (isPlaying && isPlaying()))
        stopTimer(); // fully idle: no timer, no repaints
}

} // namespace a83ui
