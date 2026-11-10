// Analog '83 test suite. Dev tool: not part of the engine.
//
//   analog83_tests            run every check (exit code 0 = all passed)
//   analog83_tests --strict   also require bit-exact renders (only meaningful
//                             against fingerprints made with the same compiler)
//   analog83_tests --update   rewrite the fingerprint file from the current
//                             engine (do this only when a sound change is intended)
//   analog83_tests --fingerprints <file>   use a different fingerprint file
//
// Render fingerprints: each demo render (Analog83Renders.h) is summarised as
// its length, peak, an exact sample hash, and its level every 20 ms per
// channel. Normal runs compare levels within 0.25 dB (1 dB below -70 dBFS),
// which catches any real sound change but ignores the tiny floating-point
// differences between compilers (GCC vs MSVC), CPUs and KICK's fast-math flags.

#include "Analog83Renders.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <new>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifndef A83_FINGERPRINT_FILE
 #define A83_FINGERPRINT_FILE "analog83_fingerprints.txt"
#endif

// ---------------------------------------------------------------- allocation counter
// Counts heap allocations while `g_trackAllocations` is set, to prove the
// audio thread never allocates.
// GCC flags malloc/free inside replaced operator new/delete as "mismatched";
// that pairing is exactly right for a replacement allocator, so silence it.
#if defined(__GNUC__) && ! defined(__clang__)
 #pragma GCC diagnostic ignored "-Wmismatched-new-delete"
#endif
static std::atomic<bool> g_trackAllocations { false };
static std::atomic<long> g_allocations { 0 };

void* operator new(std::size_t n)
{
    if (g_trackAllocations.load(std::memory_order_relaxed))
        g_allocations.fetch_add(1, std::memory_order_relaxed);
    if (void* p = std::malloc(n == 0 ? 1 : n))
        return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

namespace {

using namespace a83;
constexpr double kPi = 3.14159265358979323846;
constexpr double kSr = demo::kSampleRate;

int g_passed = 0, g_failed = 0;

void check(bool ok, const std::string& what)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
    (ok ? g_passed : g_failed)++;
}

std::string fmt(const char* f, double a = 0, double b = 0, double c = 0)
{
    char buf[256];
    std::snprintf(buf, sizeof buf, f, a, b, c);
    return buf;
}

// Strongest frequency between lo and hi Hz in x[a..b) (Hann-windowed scan).
double peakHz(const std::vector<float>& x, int a, int b, double lo, double hi)
{
    double best = 0, bestHz = 0;
    for (double f = lo; f < hi; f += 0.1)
    {
        double re = 0, im = 0;
        for (int i = a; i < b; ++i)
        {
            const double w = 0.5 - 0.5 * std::cos(2 * kPi * (i - a) / (b - a));
            re += w * x[(size_t) i] * std::cos(2 * kPi * f * i / kSr);
            im -= w * x[(size_t) i] * std::sin(2 * kPi * f * i / kSr);
        }
        const double mag = re * re + im * im;
        if (mag > best) { best = mag; bestHz = f; }
    }
    return bestHz;
}

std::unique_ptr<Kit> freshKit()
{
    auto kit = std::make_unique<Kit>();
    kit->prepare(kSr);
    return kit;
}

// ---------------------------------------------------------------- 1. note numbers
void testNoteNumbers()
{
    std::printf("\n1. MIDI note numbers (General MIDI drum map)\n");
    auto kit = freshKit();
    std::vector<float> L(256), R(256);
    const bool a = kit->noteOn(36, 1, 0), b = kit->noteOn(35, 1, 10), c = kit->noteOn(42, 1, 20),
               d = kit->noteOn(44, 1, 30), e = kit->noteOn(46, 1, 40), f = kit->noteOn(60, 1, 50);
    kit->process(L.data(), R.data(), 256);
    check(a && b && c && d && e, "notes 36, 35, 42, 44, 46 accepted");
    check(! f, "unmapped note 60 rejected in Multi mode");
    check(kit->getHitCount(VoiceId::Kick) == 2 && kit->getHitCount(VoiceId::ClosedHat) == 2
              && kit->getHitCount(VoiceId::OpenHat) == 1,
          "hits: kick 2, closed hat 2, open hat 1");
    const bool t = kit->trigger(42, 0.8f), u = ! kit->trigger(61, 0.8f);
    kit->process(L.data(), R.data(), 256);
    check(t && u && kit->getHitCount(VoiceId::ClosedHat) == 3, "trigger(note): 42 plays, 61 rejected");
}

// ---------------------------------------------------------------- 2. single-voice mode
std::vector<float> dryKick(Kit& kit, int note, int n = 48000)
{
    std::vector<float> kickOut((size_t) n, 0.0f), hatOut((size_t) n, 0.0f);
    kit.noteOn(note, 1.0f, 0);
    for (int p = 0; p < n; p += 256)
    {
        float* outs[kNumDryOutputs] = { kickOut.data() + p, hatOut.data() + p };
        kit.processDry(outs, std::min(256, n - p));
    }
    return kickOut;
}

void testSingleMode()
{
    std::printf("\n2. Single-voice mode (kick played chromatically, root note 36)\n");
    for (int note : { 24, 36, 43, 48 })
    {
        auto kit = freshKit();
        kit->setParam(Param::KickDecay, 1.5f);
        kit->setMode(KitMode::Single, VoiceId::Kick, 36);
        const auto x = dryKick(*kit, note);
        const double hz = peakHz(x, 12000, 21600, 15, 200), want = 50.0 * std::pow(2.0, (note - 36) / 12.0);
        const double cents = 1200.0 * std::log2(hz / want);
        // +/-10 cents: the kick has a deliberate +/-0.4% (about 7 cent) analog drift per hit
        check(std::abs(cents) < 10.0, fmt("note %.0f plays %.2f Hz (expected %.2f)", note, hz, want));
    }
    auto kit = freshKit();
    kit->setMode(KitMode::Single, VoiceId::ClosedHat, 42);
    std::vector<float> L(4096), R(4096);
    const bool ok = kit->noteOn(70, 1, 0);
    kit->process(L.data(), R.data(), 4096);
    check(ok && kit->getHitCount(VoiceId::ClosedHat) == 1, "hat in Single mode: any note (70) plays the hat");
    kit->setMode(KitMode::Multi);
    check(! kit->noteOn(70, 1, 0), "back to Multi mode: note 70 rejected again");
}

// ---------------------------------------------------------------- 3. dry outputs
void testDryOutputs()
{
    std::printf("\n3. Dry outputs (processDry)\n");
    auto kit = freshKit();
    const int n = 24000;
    std::vector<float> ko((size_t) n, 0.0f), ho((size_t) n, 0.0f);
    for (int p = 0; p < n; p += 256)
    {
        if (p == 0) kit->noteOn(36, 1, 100);
        if (p == 9984) kit->noteOn(42, 1, 16);
        float* outs[kNumDryOutputs] = { ko.data() + p, ho.data() + p };
        kit->processDry(outs, std::min(256, n - p));
    }
    int kickStart = -1, hatStart = -1;
    double hatBefore = 0;
    for (int i = 0; i < n; ++i)
    {
        if (kickStart < 0 && ko[(size_t) i] != 0.0f) kickStart = i;
        if (hatStart < 0 && ho[(size_t) i] != 0.0f) hatStart = i;
        if (i < 10000) hatBefore = std::max(hatBefore, (double) std::abs(ho[(size_t) i]));
    }
    check(kickStart == 100, fmt("kick starts on sample %.0f (scheduled 100)", kickStart));
    check(hatStart == 10000, fmt("hat starts on sample %.0f (scheduled 10000)", hatStart));
    check(hatBefore == 0.0, "hat output silent until the hat plays (voices fully separated)");

    // The dry kick must equal the bare kick voice with the kit's default settings
    Kick bare;
    bare.prepare(kSr);
    bare.setTune(paramInfo(Param::KickTune).defaultValue);
    bare.setDecay(paramInfo(Param::KickDecay).defaultValue);
    bare.setPunch(paramInfo(Param::KickPunch).defaultValue / 100.0f);
    bare.setTone(paramInfo(Param::KickTone).defaultValue / 100.0f);
    bare.setDrive(paramInfo(Param::KickDrive).defaultValue / 100.0f);
    bare.setLevel(paramInfo(Param::KickLevel).defaultValue);
    std::vector<float> b((size_t) n, 0.0f);
    bare.trigger(1.0f);
    bare.process(b.data() + 100, n - 100);
    double maxDiff = 0;
    for (int i = 0; i < n; ++i)
        maxDiff = std::max(maxDiff, (double) std::abs(b[(size_t) i] - ko[(size_t) i]));
    check(maxDiff < 1e-6, fmt("dry kick matches the bare kick voice (largest difference %.2g)", maxDiff));

    auto kit2 = freshKit();
    std::vector<float> only(256, 0.0f);
    kit2->noteOn(42, 1, 0);
    float* outs[kNumDryOutputs] = { only.data(), nullptr };
    kit2->processDry(outs, 256);
    check(kit2->getHitCount(VoiceId::ClosedHat) == 1, "a nullptr output is skipped safely");
}

// ---------------------------------------------------------------- 4. threads
void testThreads()
{
    std::printf("\n4. Several threads sending notes at once\n");
    {
        MpscQueue<int, 64> q;
        std::atomic<long> pushed { 0 }, popped { 0 };
        std::atomic<bool> stop { false };
        std::vector<std::thread> producers;
        for (int t = 0; t < 4; ++t)
            producers.emplace_back([&, t] {
                for (int i = 0; i < 20000; ++i)
                {
                    while (! q.push(t * 100000 + i))
                        std::this_thread::yield();
                    pushed++;
                }
            });
        std::thread consumer([&] {
            int v;
            for (;;)
            {
                if (q.pop(v)) popped++;
                else if (stop.load())
                {
                    while (q.pop(v)) popped++;
                    break;
                }
            }
        });
        for (auto& p : producers) p.join();
        stop = true;
        consumer.join();
        check(pushed == 80000 && popped == 80000, fmt("queue: 4 threads x 20,000 events, %.0f of %.0f delivered", (double) popped, (double) pushed));
    }
    {
        auto kit = freshKit();
        std::atomic<long> kicksOk { 0 }, hatsOk { 0 };
        std::atomic<bool> done { false };
        std::thread audio([&] {
            std::vector<float> L(128), R(128);
            while (! done.load()) kit->process(L.data(), R.data(), 128);
            for (int i = 0; i < 8; ++i) kit->process(L.data(), R.data(), 128); // drain
        });
        std::vector<std::thread> senders;
        for (int t = 0; t < 4; ++t)
            senders.emplace_back([&, t] {
                for (int i = 0; i < 2000; ++i)
                {
                    const bool ok = (t % 2) ? kit->trigger(42, 0.7f) : kit->trigger(VoiceId::Kick, 0.9f);
                    if (ok) ((t % 2) ? hatsOk : kicksOk)++;
                    std::this_thread::yield();
                }
            });
        for (auto& s : senders) s.join();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        done = true;
        audio.join();
        check((long) kit->getHitCount(VoiceId::Kick) == kicksOk.load(),
              fmt("kit: every accepted kick fired exactly once (%.0f)", (double) kicksOk.load()));
        check((long) kit->getHitCount(VoiceId::ClosedHat) == hatsOk.load(),
              fmt("kit: every accepted hat fired exactly once (%.0f)", (double) hatsOk.load()));
    }
}

// ---------------------------------------------------------------- 5. allocations
void testNoAllocations()
{
    std::printf("\n5. Real-time safety\n");
    auto kit = freshKit();
    std::vector<float> L(4096), R(4096), K(4096), H(4096);
    g_allocations = 0;
    g_trackAllocations = true;
    for (int b = 0; b < 3000; ++b)
    {
        const int n = (b % 7 == 0) ? 4096 : (b % 3 == 0 ? 1 : 256);
        if (b == 1000) kit->setMode(KitMode::Single, VoiceId::Kick, 36);
        if (b == 2000) kit->setMode(KitMode::Multi);
        if (b == 500) kit->setParam(Param::Room, 100.0f);
        if (b == 1500) kit->setParam(Param::Room, 0.0f);
        kit->noteOn(36 + (b % 13), 1.0f, b % n);
        kit->trigger(46, 0.9f);
        if (b % 2)
            kit->process(L.data(), R.data(), n);
        else
        {
            float* outs[kNumDryOutputs] = { K.data(), H.data() };
            kit->processDry(outs, n);
        }
    }
    g_trackAllocations = false;
    check(g_allocations == 0, fmt("no heap allocations in 3,000 audio callbacks (counted %.0f)", (double) g_allocations.load()));
}

// ---------------------------------------------------------------- 6. render fingerprints
struct Fingerprint
{
    size_t frames = 0;
    double peakDb = 0;
    std::string hash;
    std::vector<double> envL, envR;
};

constexpr int kWindow = 960; // 20 ms at 48 kHz

double toDb(double v) { return v > 1e-6 ? 20.0 * std::log10(v) : -120.0; }

Fingerprint fingerprintOf(const demo::Stereo& a)
{
    Fingerprint fp;
    fp.frames = a.size();
    double peak = 0;
    uint64_t h = 1469598103934665603ull; // FNV-1a over the 16-bit samples, as written to WAV
    for (size_t i = 0; i < a.size(); ++i)
        for (float s : { a.left[i], a.right[i] })
        {
            peak = std::max(peak, (double) std::abs(s));
            const float c = s < -1.0f ? -1.0f : (s > 1.0f ? 1.0f : s);
            const auto q = (uint16_t) (int16_t) (c * 32767.0f);
            h = (h ^ (q & 0xFFu)) * 1099511628211ull;
            h = (h ^ (q >> 8)) * 1099511628211ull;
        }
    fp.peakDb = toDb(peak);
    char hex[17];
    std::snprintf(hex, sizeof hex, "%016llx", (unsigned long long) h);
    fp.hash = hex;
    for (size_t w = 0; w < a.size(); w += kWindow)
    {
        double sl = 0, sr = 0;
        const size_t end = std::min(a.size(), w + kWindow);
        for (size_t i = w; i < end; ++i)
        {
            sl += (double) a.left[i] * a.left[i];
            sr += (double) a.right[i] * a.right[i];
        }
        fp.envL.push_back(toDb(std::sqrt(sl / (double) (end - w))));
        fp.envR.push_back(toDb(std::sqrt(sr / (double) (end - w))));
    }
    return fp;
}

bool writeFingerprints(const std::string& path, const std::vector<demo::NamedRender>& renders)
{
    std::ofstream f(path);
    if (! f) return false;
    f << "# Analog '83 render fingerprints, written by: analog83_tests --update\n"
      << "# Per render: length, peak, exact 16-bit sample hash, then the level\n"
      << "# every 20 ms (dBFS) for the left and right channel.\n";
    char num[32];
    for (const auto& r : renders)
    {
        const auto fp = fingerprintOf(r.audio);
        std::snprintf(num, sizeof num, "%.3f", fp.peakDb);
        f << "render " << r.name << " frames " << fp.frames << " peak " << num << " hash " << fp.hash << "\n";
        for (int ch = 0; ch < 2; ++ch)
        {
            f << (ch == 0 ? "L" : "R");
            for (double v : (ch == 0 ? fp.envL : fp.envR))
            {
                std::snprintf(num, sizeof num, " %.2f", v);
                f << num;
            }
            f << "\n";
        }
    }
    return (bool) f;
}

std::map<std::string, Fingerprint> readFingerprints(const std::string& path)
{
    std::map<std::string, Fingerprint> out;
    std::ifstream f(path);
    std::string line, current;
    while (std::getline(f, line))
    {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream in(line);
        std::string tag;
        in >> tag;
        if (tag == "render")
        {
            std::string k1, k2, k3;
            Fingerprint fp;
            in >> current >> k1 >> fp.frames >> k2 >> fp.peakDb >> k3 >> fp.hash;
            out[current] = fp;
        }
        else if ((tag == "L" || tag == "R") && out.count(current))
        {
            auto& env = tag == "L" ? out[current].envL : out[current].envR;
            double v;
            while (in >> v) env.push_back(v);
        }
    }
    return out;
}

void testFingerprints(const std::string& path, bool strict, bool update)
{
    std::printf("\n6. Demo renders vs fingerprints (%s)\n", path.c_str());
    const auto set = demo::renderAll();
    check(set.patternKicks == 7 && set.patternClosedHats == 26 && set.patternOpenHats == 2,
          fmt("92 BPM pattern hit counts: kick %.0f, closed hat %.0f, open hat %.0f (expect 7, 26, 2)",
              set.patternKicks, set.patternClosedHats, set.patternOpenHats));

    if (update)
    {
        check(writeFingerprints(path, set.renders), "fingerprint file rewritten");
        return;
    }

    const auto stored = readFingerprints(path);
    if (stored.empty())
    {
        check(false, "fingerprint file found (run with --update once to create it)");
        return;
    }
    for (const auto& r : set.renders)
    {
        const auto it = stored.find(r.name);
        if (it == stored.end()) { check(false, r.name + ": no stored fingerprint"); continue; }
        const auto now = fingerprintOf(r.audio);
        const auto& was = it->second;

        bool ok = now.frames == was.frames && std::abs(now.peakDb - was.peakDb) <= 0.1
                  && now.envL.size() == was.envL.size() && now.envR.size() == was.envR.size();
        double worst = 0;
        size_t worstWindow = 0;
        for (int ch = 0; ok && ch < 2; ++ch)
        {
            const auto& a = ch == 0 ? now.envL : now.envR;
            const auto& b = ch == 0 ? was.envL : was.envR;
            for (size_t w = 0; w < a.size(); ++w)
            {
                if (a[w] < -100 && b[w] < -100) continue;          // both silent
                // 0.25 dB: covers rounding differences between compilers and
                // KICK's -Ofast -ffast-math build (measured up to 0.11 dB on a
                // limited transient), while real changes move levels by dB.
                const double tolerance = std::max(a[w], b[w]) > -70 ? 0.25 : 1.0;
                const double diff = std::abs(a[w] - b[w]);
                if (diff > worst) { worst = diff; worstWindow = w; }
                if (diff > tolerance) ok = false;
            }
        }
        std::string what = r.name + (ok ? ": matches" : ": CHANGED");
        if (! ok)
            what += fmt(" (largest level change %.2f dB at %.2f s)", worst, (double) worstWindow * kWindow / kSr);
        if (strict)
        {
            const bool exact = now.hash == was.hash;
            what += exact ? ", bit-exact" : ", NOT bit-exact";
            ok = ok && exact;
        }
        check(ok, what);
    }
}

} // namespace

int main(int argc, char** argv)
{
    bool strict = false, update = false;
    std::string path = A83_FINGERPRINT_FILE;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--strict") strict = true;
        else if (arg == "--update") update = true;
        else if (arg == "--fingerprints" && i + 1 < argc) path = argv[++i];
    }

    std::printf("Analog '83 test suite\n");
    testNoteNumbers();
    testSingleMode();
    testDryOutputs();
    testThreads();
    testNoAllocations();
    testFingerprints(path, strict, update);

    std::printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
