#include "Header.h"

namespace a83ui {

namespace {
const char* const kTabNames[4] = { "PATTERN", "SONG", "EDIT", "MIX" };

void drawTriangle(juce::Graphics& g, juce::Rectangle<float> r, bool pointsRight)
{
    juce::Path p;
    if (pointsRight)
        p.addTriangle(r.getX(), r.getY(), r.getX(), r.getBottom(), r.getRight(), r.getCentreY());
    else
        p.addTriangle(r.getRight(), r.getY(), r.getRight(), r.getBottom(), r.getX(), r.getCentreY());
    g.fillPath(p);
}
} // namespace

void Header::resized()
{
    const int h = getHeight();
    const int boxY = (h - 44) / 2;
    logoArea  = { 12, 0, 290, h };
    kitArea   = { 310, boxY, 250, 44 };
    bpmArea   = { 570, boxY, 78, 44 };
    playArea  = { 654, boxY, 50, 44 };
    for (int i = 0; i < 4; ++i)
        tabAreas[(size_t) i] = { 712 + i * 58, boxY + 4, 54, 36 };
    audioArea = { 950, boxY + 4, 62, 36 };
}

void Header::paint(juce::Graphics& g)
{
    // ---- Logo: three stripes, "ANALOG" in cream, "'83" in pink
    const auto logo = logoArea.toFloat();
    const juce::Colour stripes[3] = { colours::cyan, colours::pink, colours::purple };
    for (int i = 0; i < 3; ++i)
    {
        g.setColour(stripes[i]);
        g.fillRect(juce::Rectangle<float>(logo.getX(), logo.getCentreY() - 13.0f + (float) i * 10.0f, 40.0f, 6.0f));
    }

    const auto logoFont = font(40.0f, true, true);
    g.setFont(logoFont);
    const float textX = logo.getX() + 50.0f;
    const float analogWidth = logoFont.getStringWidthFloat("ANALOG");
    g.setColour(colours::cream);
    g.drawText("ANALOG", juce::Rectangle<float>(textX, logo.getY(), analogWidth + 4.0f, logo.getHeight()),
               juce::Justification::centredLeft, false);
    g.setColour(colours::pink);
    g.drawText("'83", juce::Rectangle<float>(textX + analogWidth + 6.0f, logo.getY(), 90.0f, logo.getHeight()),
               juce::Justification::centredLeft, false);

    // ---- Kit selector
    const auto kit = kitArea.toFloat();
    g.setColour(colours::panel);
    g.fillRoundedRectangle(kit, 4.0f);
    g.setColour(colours::cyan);
    g.drawRoundedRectangle(kit.reduced(0.5f), 4.0f, 1.5f);
    drawTriangle(g, { kit.getX() + 12.0f, kit.getCentreY() - 7.0f, 10.0f, 14.0f }, false);
    drawTriangle(g, { kit.getRight() - 22.0f, kit.getCentreY() - 7.0f, 10.0f, 14.0f }, true);
    g.setFont(font(19.0f));
    g.drawText(juce::String::fromUTF8("KIT 01 \xe2\x80\x93 CLASSIC"), kit.reduced(28.0f, 0.0f),
               juce::Justification::centred, false);

    // ---- BPM
    const auto bpm = bpmArea.toFloat();
    g.setColour(colours::cyan);
    g.setFont(font(12.0f));
    g.drawText("BPM", bpm.withHeight(14.0f), juce::Justification::centred, false);
    g.setColour(colours::pink);
    g.setFont(font(26.0f));
    g.drawText(juce::String(getBpm ? getBpm() : 92.0f, 1), bpm.withTrimmedTop(12.0f),
               juce::Justification::centred, false);

    // ---- Play / stop
    const bool playing = isPlaying && isPlaying();
    const auto play = playArea.toFloat();
    g.setColour(playing ? colours::cyan.withAlpha(0.25f) : colours::panel);
    g.fillRoundedRectangle(play, 4.0f);
    g.setColour(colours::cyan);
    g.drawRoundedRectangle(play.reduced(0.5f), 4.0f, 1.5f);
    if (playing)
        g.fillRect(play.withSizeKeepingCentre(16.0f, 16.0f));
    else
        drawTriangle(g, play.withSizeKeepingCentre(18.0f, 20.0f), true);

    // ---- Page tabs (PATTERN is the only page in this preview)
    for (int i = 0; i < 4; ++i)
    {
        const auto r = tabAreas[(size_t) i].toFloat();
        const bool active = i == 0;
        g.setColour(active ? colours::pink : colours::panel);
        g.fillRoundedRectangle(r, 3.0f);
        g.setColour(active ? colours::pink : colours::outline);
        g.drawRoundedRectangle(r.reduced(0.5f), 3.0f, 1.0f);
        g.setColour(active ? colours::background : colours::cyan);
        g.setFont(font(12.0f));
        g.drawText(kTabNames[i], r, juce::Justification::centred, false);
    }

    // ---- Audio settings button
    const auto audio = audioArea.toFloat();
    g.setColour(colours::panel);
    g.fillRoundedRectangle(audio, 3.0f);
    g.setColour(colours::outline);
    g.drawRoundedRectangle(audio.reduced(0.5f), 3.0f, 1.0f);
    g.setColour(colours::textDim);
    g.setFont(font(12.0f));
    g.drawText("AUDIO", audio, juce::Justification::centred, false);

    // ---- Neon divider lines under the header
    const float w = (float) getWidth();
    const float bottom = (float) getHeight();
    g.setColour(colours::pink);
    g.fillRect(0.0f, bottom - 9.0f, w, 3.0f);
    g.setColour(colours::cyan);
    g.fillRect(0.0f, bottom - 5.0f, w, 2.0f);
    g.setColour(colours::purple.withAlpha(0.7f));
    g.fillRect(0.0f, bottom - 2.0f, w, 2.0f);
}

void Header::mouseDown(const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    if (playArea.contains(p) && onPlayToggle)
        onPlayToggle();
    else if (audioArea.contains(p) && onAudioSettings)
        onAudioSettings();
    repaint();
}

} // namespace a83ui
