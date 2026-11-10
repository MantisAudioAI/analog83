// Analog '83 demo renders, shared by analog83_test (writes WAV files) and
// analog83_tests (fingerprints them), so both always use exactly the same
// sounds. Dev tool: not part of the engine.
#pragma once

#include "Analog83DemoPattern.h"
#include "Analog83Kit.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace a83 {
namespace demo {

constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 64; // render in small blocks, like a real audio callback

struct Stereo
{
    std::vector<float> left, right;
    explicit Stereo(size_t n = 0) : left(n, 0.0f), right(n, 0.0f) {}
    size_t size() const { return left.size(); }
};

using a83::VoiceId;

struct Event
{
    int samplePos;
    VoiceId voice;
    float velocity;
};

// Rounds down to a whole sample, but a hit that lands exactly on a sample in
// exact math (like step 7 at 140 BPM = sample 36000) can come out as
// 35999.9999... in floating point, depending on compiler flags (KICK builds
// with -ffast-math). The tiny epsilon makes every build pick the same sample.
inline int secondsToSamples(double s) { return (int) std::floor(s * kSampleRate + 1e-6); }

// Renders events through the full Analog '83 kit (voices + bus), in small
// blocks like an audio callback. Each block ends exactly at the next event,
// so every hit lands on the sample it was scheduled for.
inline Stereo render(a83::Kit& kit, std::vector<Event> events, double seconds)
{
    std::sort(events.begin(), events.end(),
              [](const Event& a, const Event& b) { return a.samplePos < b.samplePos; });

    Stereo out((size_t) (seconds * kSampleRate));
    const int total = (int) out.size();
    size_t next = 0;

    // Fixed-size blocks like a real audio callback. Each event is handed to
    // the kit with noteOn() and its exact sample offset inside the block,
    // the same way KICK's sequencer will drive the engine.
    for (int pos = 0; pos < total; pos += kBlockSize)
    {
        const int n = std::min(kBlockSize, total - pos);
        while (next < events.size() && events[next].samplePos < pos + n)
        {
            kit.noteOn(events[next].voice, events[next].velocity, events[next].samplePos - pos);
            ++next;
        }
        kit.process(out.left.data() + pos, out.right.data() + pos, n);
    }
    return out;
}

// A 2-bar, 140 BPM trap pattern. Each hat step has a ratchet count:
// 1 = single hit, 2/3/4/6 = roll that splits the step into that many hits
// with a rising velocity ramp, like the ROLL / RATCHET mockup.
inline std::vector<Event> trapPattern(double bpm)
{
    const double stepSeconds = 60.0 / bpm / 4.0; // one 16th note
    std::vector<Event> events;

    const int kickSteps[] = { 0, 7, 10, 16, 19, 26, 29 };
    for (int s : kickSteps)
        events.push_back({ secondsToSamples(s * stepSeconds), VoiceId::Kick, s % 16 == 0 ? 1.0f : 0.85f });

    // 0 = rest, -1 = open hat, otherwise ratchet count
    const int hats[32] = {
        1, 0, 1, 0,  1, 0, 1, 3,   1, 0, 1, 0,  1, 0, 4, 4,
        1, 0, 1, 0, -1, 0, 1, 2,   1, 0, 1, 0,  3, 3, 4, 6,
    };

    for (int step = 0; step < 32; ++step)
    {
        const int r = hats[step];
        const double t = step * stepSeconds;
        if (r == -1)
        {
            events.push_back({ secondsToSamples(t), VoiceId::OpenHat, 0.9f });
        }
        else if (r > 0)
        {
            for (int i = 0; i < r; ++i)
            {
                const float ramp = r == 1 ? 0.8f : 0.4f + 0.6f * (float) i / (float) (r - 1);
                events.push_back({ secondsToSamples(t + i * stepSeconds / r), VoiceId::ClosedHat, ramp });
            }
        }
    }
    return events;
}

struct NamedRender
{
    std::string name;
    Stereo audio;
};

struct RenderSet
{
    std::vector<NamedRender> renders;
    // Hit counts from the 92 BPM demo pattern (expected 7 / 26 / 2)
    uint32_t patternKicks = 0, patternClosedHats = 0, patternOpenHats = 0;
};

// Renders every demo sound, in a fixed order with fixed settings.
inline RenderSet renderAll()
{
    auto freshKit = []
    {
        auto kit = std::make_unique<a83::Kit>();
        kit->prepare(kSampleRate);
        return kit;
    };

    RenderSet set;
    auto add = [&](const char* name, Stereo audio) { set.renders.push_back({ name, std::move(audio) }); };

    { auto kit = freshKit(); add("kick_default.wav", render(*kit, { { 0, VoiceId::Kick, 1.0f } }, 1.0)); }
    {
        auto kit = freshKit();
        kit->setParam(a83::Param::KickTune, -5.0f);
        kit->setParam(a83::Param::KickDecay, 1.6f);
        kit->setParam(a83::Param::KickPunch, 80.0f);
        kit->setParam(a83::Param::KickTone, 55.0f);
        add("kick_long_boom.wav", render(*kit, { { 0, VoiceId::Kick, 1.0f } }, 2.0));
    }
    { auto kit = freshKit(); add("hat_closed.wav", render(*kit, { { 0, VoiceId::ClosedHat, 1.0f } }, 0.3)); }
    { auto kit = freshKit(); add("hat_open.wav", render(*kit, { { 0, VoiceId::OpenHat, 1.0f } }, 1.0)); }
    {
        // Open hat cut off by a closed hat 150 ms later
        auto kit = freshKit();
        add("hat_choke.wav", render(*kit, { { 0, VoiceId::OpenHat, 1.0f },
                                            { secondsToSamples(0.15), VoiceId::ClosedHat, 0.8f } }, 0.6));
    }
    {
        auto kit = freshKit();
        kit->setParam(a83::Param::KickDecay, 0.9f);
        const double bpm = 140.0;
        add("pattern_trap_140.wav", render(*kit, trapPattern(bpm), 2 * 4 * 60.0 / bpm + 0.8));
    }
    {
        // The 92 BPM demo pattern (driving the kit through noteOn), plus one
        // "pad press" from the UI side, rendered in 512-sample blocks.
        auto kit = freshKit();
        a83::DemoPattern pattern; // stands in for KICK's own sequencer
        pattern.prepare(kSampleRate);
        const double barSeconds = 4 * 60.0 / 92.0;
        Stereo out((size_t) ((2 * barSeconds - 0.01) * kSampleRate));
        const int block = 512;
        for (int pos = 0; pos < (int) out.size(); pos += block)
        {
            if (pos == 512 * 100)
                kit->trigger(VoiceId::Kick, 0.5f); // simulated pad press
            const int n = std::min(block, (int) out.size() - pos);
            pattern.schedule(*kit, n);
            kit->process(out.left.data() + pos, out.right.data() + pos, n);
        }
        add("pattern_kit_92.wav", std::move(out));
        set.patternKicks = kit->getHitCount(VoiceId::Kick);
        set.patternClosedHats = kit->getHitCount(VoiceId::ClosedHat);
        set.patternOpenHats = kit->getHitCount(VoiceId::OpenHat);
    }
    return set;
}

} // namespace demo
} // namespace a83
