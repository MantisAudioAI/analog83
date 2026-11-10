#include "MainComponent.h"

namespace a83ui {

namespace {
// Computer keyboard to pads, laid out like the 4x4 grid:
//   1 2 3 4  ->  pads 01-04 (top row)
//   Q W E R  ->  pads 05-08
//   A S D F  ->  pads 09-12
//   Z X C V  ->  pads 13-16
const char* const kPadKeys = "1234qwerasdfzxcv";
} // namespace

MainComponent::MainComponent()
{
    header.isPlaying = [this] { return playing.load(); };
    header.getBpm = [] { return 92.0f; }; // the demo pattern's tempo
    header.onPlayToggle = [this] { togglePlay(); };
    header.onAudioSettings = [this] { showAudioSettings(); };
    pads.onPadPressed = [this](int i) { pressPad(i); };
    pads.isPlaying = [this] { return playing.load(); };

    addAndMakeVisible(header);
    addAndMakeVisible(pads);
    addAndMakeVisible(voicePanel);

    setOpaque(true); // we fill every pixel, so JUCE never paints behind us
    setWantsKeyboardFocus(true);
    setSize(1024, 600); // KICK's screen size

    setAudioChannels(0, 2); // no inputs, stereo out
    requestLowLatency();
}

// Asks the audio device for a small buffer (256 samples = 5.3 ms at 48 kHz).
// If the device can't do it, the previous working setup is kept. For the
// lowest latency on Windows, pick "Windows Audio (Low Latency Mode)" or an
// ASIO driver in the AUDIO settings.
void MainComponent::requestLowLatency()
{
    constexpr int kTargetBuffer = 256;
    const auto previous = deviceManager.getAudioDeviceSetup();
    if (previous.bufferSize > 0 && previous.bufferSize <= kTargetBuffer)
        return;

    auto setup = previous;
    setup.bufferSize = kTargetBuffer;
    if (deviceManager.setAudioDeviceSetup(setup, true).isNotEmpty())
        deviceManager.setAudioDeviceSetup(previous, true); // device refused: keep what worked
}

MainComponent::~MainComponent()
{
    shutdownAudio();
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    kit.prepare(sampleRate);
    demo.prepare(sampleRate);
    demo.setBpm(92.0);
    left.assign((size_t) juce::jmax(512, samplesPerBlockExpected), 0.0f);
    right.assign(left.size(), 0.0f);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    const juce::ScopedNoDenormals noDenormals;
    info.clearActiveBufferRegion();
    auto* buffer = info.buffer;

    const int channels = buffer->getNumChannels();
    if (channels == 0)
        return;

    int done = 0;
    while (done < info.numSamples)
    {
        const int n = juce::jmin(info.numSamples - done, (int) left.size());
        std::fill(left.begin(), left.begin() + n, 0.0f);
        std::fill(right.begin(), right.begin() + n, 0.0f);

        const bool play = playing.load(std::memory_order_relaxed);
        if (play && ! wasPlaying)
            demo.reset(); // start from the top of the bar
        wasPlaying = play;
        if (play)
            demo.schedule(kit, n); // sample-accurate notes for this block

        kit.process(left.data(), right.data(), n);

        if (channels == 1)
        {
            // Mono output device: mix both sides down
            buffer->copyFrom(0, info.startSample + done, left.data(), n, 0.5f);
            buffer->addFrom(0, info.startSample + done, right.data(), n, 0.5f);
        }
        else
        {
            buffer->copyFrom(0, info.startSample + done, left.data(), n);
            buffer->copyFrom(1, info.startSample + done, right.data(), n);
        }
        done += n;
    }
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(colours::background);
}

void MainComponent::resized()
{
    auto r = getLocalBounds();
    header.setBounds(r.removeFromTop(78));
    r = r.reduced(12, 10);
    pads.setBounds(r.removeFromLeft(480));
    r.removeFromLeft(12);
    voicePanel.setBounds(r);
}

void MainComponent::pressPad(int padIndex)
{
    pads.setSelectedPad(padIndex);
    voicePanel.showPad(padIndex);
    pads.flashPad(padIndex);

    if (const auto voice = padDef(padIndex).voice)
        kit.trigger(*voice, 1.0f);
}

void MainComponent::togglePlay()
{
    playing.store(! playing.load());
    if (playing.load())
        pads.wake(); // pads flash with the beat
    header.repaint();
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey)
    {
        togglePlay();
        return true;
    }

    const auto c = juce::CharacterFunctions::toLowerCase(key.getTextCharacter());
    for (int i = 0; i < 16; ++i)
    {
        if (c == (juce::juce_wchar) kPadKeys[i])
        {
            // Holding a key must not machine-gun the pad (auto-repeat):
            // play once on the press, like a real pad.
            if (! keyHeld[(size_t) i])
            {
                keyHeld[(size_t) i] = true;
                pressPad(i);
            }
            return true;
        }
    }
    return false;
}

bool MainComponent::keyStateChanged(bool)
{
    for (int i = 0; i < 16; ++i)
    {
        const int code = (int) juce::CharacterFunctions::toUpperCase((juce::juce_wchar) kPadKeys[i]);
        if (keyHeld[(size_t) i] && ! juce::KeyPress::isKeyCurrentlyDown(code))
            keyHeld[(size_t) i] = false; // released: ready to play again
    }
    return false;
}

void MainComponent::showAudioSettings()
{
    auto* selector = new juce::AudioDeviceSelectorComponent(deviceManager, 0, 0, 2, 2,
                                                            false, false, true, false);
    selector->setSize(520, 380);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(selector);
    options.dialogTitle = "Audio Settings";
    options.dialogBackgroundColour = colours::panel;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}

} // namespace a83ui
