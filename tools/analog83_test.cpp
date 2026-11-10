// Analog '83 test program.
// Renders the demo sounds (see Analog83Renders.h) to WAV files so the voices
// can be heard and checked without any KICK hardware or app.

#include "Analog83Renders.h"

#include <cstdio>
#include <fstream>

namespace {

using a83::demo::Stereo;
constexpr double kSampleRate = a83::demo::kSampleRate;

// ---------------------------------------------------------------- WAV output

void writeLE(std::ofstream& f, uint32_t value, int bytes)
{
    for (int i = 0; i < bytes; ++i)
        f.put((char) ((value >> (8 * i)) & 0xFF));
}


bool writeWav(const std::string& path, const Stereo& audio)
{
    std::ofstream f(path, std::ios::binary);
    if (! f)
        return false;

    const uint32_t dataBytes = (uint32_t) audio.size() * 4;
    f.write("RIFF", 4); writeLE(f, 36 + dataBytes, 4);
    f.write("WAVE", 4);
    f.write("fmt ", 4); writeLE(f, 16, 4);
    writeLE(f, 1, 2);                              // PCM
    writeLE(f, 2, 2);                              // stereo
    writeLE(f, (uint32_t) kSampleRate, 4);
    writeLE(f, (uint32_t) kSampleRate * 4, 4);     // bytes per second
    writeLE(f, 4, 2);                              // block align
    writeLE(f, 16, 2);                             // bits per sample
    f.write("data", 4); writeLE(f, dataBytes, 4);

    for (size_t i = 0; i < audio.size(); ++i)
        for (float s : { audio.left[i], audio.right[i] })
            writeLE(f, (uint32_t) (uint16_t) (int16_t) (a83::clampf(s, -1.0f, 1.0f) * 32767.0f), 2);
    return (bool) f;
}

void report(const std::string& path, const Stereo& audio)
{
    float peak = 0.0f;
    for (size_t i = 0; i < audio.size(); ++i)
        peak = std::max({ peak, std::abs(audio.left[i]), std::abs(audio.right[i]) });
    const bool ok = writeWav(path, audio);
    std::printf("%-22s %5.2f s  peak %.2f  %s\n", path.c_str(),
                (double) audio.size() / kSampleRate, peak, ok ? "ok" : "FAILED TO WRITE");
}

} // namespace

int main()
{
    std::printf("Analog '83 test render, %.0f Hz (full kit: voices + bus)\n\n", kSampleRate);

    const auto set = a83::demo::renderAll();
    for (const auto& r : set.renders)
        report(r.name, r.audio);

    std::printf("  hits: kick %u (expect 7), closed hat %u (expect 26), open hat %u (expect 2)\n",
                set.patternKicks, set.patternClosedHats, set.patternOpenHats);
    std::printf("\nDone. Open the WAV files in any audio player.\n");
    return 0;
}
