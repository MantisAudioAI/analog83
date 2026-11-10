// Analog '83 demo pattern: a one-bar beat used by the test tool, the
// benchmark and the preview app. NOT part of the engine. On KICK, tempo and
// patterns come from KICK's own sequencer; this file stands in for it,
// driving the kit through Kit::noteOn() exactly the way a host sequencer
// would.
//
// Call schedule() on the audio thread right before each Kit::process()
// call with the same block size.
#pragma once

#include "Analog83Kit.h"

#include <cmath>

namespace a83 {

class DemoPattern
{
public:
    void prepare(double newSampleRate) { sampleRate = newSampleRate; reset(); }
    void setBpm(double newBpm) { bpm = newBpm < 20.0 ? 20.0 : newBpm; }
    double getBpm() const { return bpm; }

    // Back to the start of the bar.
    void reset()
    {
        tickIndex = 0;
        nextTick = 0.0;
    }

    // Schedules every hit that falls inside the next numSamples samples.
    // Works with a Kit, or anything else with the same noteOn(voice, velocity,
    // offset) method (the test suite uses that to check exact hit positions).
    template <typename Target>
    void schedule(Target& kit, int numSamples)
    {
        const double samplesPerTick = sampleRate * 60.0 / bpm / 4.0 / kTicksPerStep;
        for (;;)
        {
            // The epsilon keeps a tick that lands exactly on a sample from
            // moving by one depending on float rounding (see Analog83Renders.h)
            const int offset = (int) std::ceil(nextTick - 1e-6);
            if (offset >= numSamples)
                break;
            runTick(kit, offset);
            nextTick += samplesPerTick;
        }
        nextTick -= numSamples;
    }

private:
    // 24 ticks per 16th note, so rolls of 1, 2, 3, 4, 6 and 8 land exactly on a tick
    static constexpr int kTicksPerStep = 24;
    static constexpr int kSteps = 16;

    template <typename Target>
    void runTick(Target& kit, int offset)
    {
        // One bar of 16 steps
        static constexpr float kKick[kSteps]  = { 1.0f, 0, 0, 0,  0, 0, 0, 0.8f,  0, 0, 0.9f, 0,  0, 0, 0, 0 };
        static constexpr int kHatRoll[kSteps] = { 1, 0, 1, 0,  1, 0, 0, 0,  1, 0, 1, 0,  1, 0, 3, 4 }; // ratchet count
        static constexpr float kOpen[kSteps]  = { 0, 0, 0, 0,  0, 0, 0.8f, 0,  0, 0, 0, 0,  0, 0, 0, 0 };

        const int step = tickIndex / kTicksPerStep;
        const int sub = tickIndex % kTicksPerStep;

        if (sub == 0)
        {
            if (kKick[step] > 0.0f) kit.noteOn(VoiceId::Kick, kKick[step], offset);
            if (kOpen[step] > 0.0f) kit.noteOn(VoiceId::OpenHat, kOpen[step], offset);
        }

        const int rolls = kHatRoll[step];
        if (rolls > 0)
        {
            const int spacing = kTicksPerStep / rolls;
            if (sub % spacing == 0)
            {
                // Single hats accent the downbeats; rolls ramp up in velocity
                const int hit = sub / spacing;
                const float velocity = rolls == 1 ? (step % 4 == 0 ? 0.9f : 0.6f)
                                                  : 0.35f + 0.65f * (float) hit / (float) (rolls - 1);
                kit.noteOn(VoiceId::ClosedHat, velocity, offset);
            }
        }

        tickIndex = (tickIndex + 1) % (kSteps * kTicksPerStep);
    }

    double sampleRate = 48000.0;
    double bpm = 92.0;
    int tickIndex = 0;
    double nextTick = 0.0;
};

} // namespace a83
