# Analog '83 drum engine (prototype)

Standalone C++17 drum voices for the Analog '83 proposal
(MusicTribeOpenSource/0700-AAM issue #16). No JUCE or KICK code, so it builds
on Windows, macOS or Linux. A test program renders WAV files so you can hear
the voices without the hardware.

## The sound

The goal is the sound of an analog drum machine **on a record**, not straight
out of the machine. Each voice is modelled on how classic early-'80s analog
drum circuits behave (not copied from any schematic), then goes through the
same kind of chain a record does: the mix bus, tape and mastering, plus an
optional room.

**The voices**

- **Kick:** a resonance that starts sharp and drops into its note, a slight
  pitch droop through the body, a soft beater thump, and tiny per-hit pitch
  drift like an analog oscillator.
- **Hats:** six detuned square oscillators through band-pass and high-pass
  filters, opening over 0.3 ms like an analog VCA (no digital click). Closed
  and open share one voice, so a closed hat chokes an open one.
- **DRIVE (18%):** tube/tape-style saturation on each voice, about 3%
  distortion at full level, mostly the warm 2nd harmonic. No break-up.
- **Rounded release:** tails ease smoothly to silence, and a retriggered kick
  crossfades instead of clicking.

**The record chain (stereo)**

1. **Mix:** kick in the centre, hats a little right.
2. **ROOM (off by default):** a dry-to-room knob for a short, slightly dark
   stereo room. Only the hats, snare, clap and percussion go into it; the
   kick and the low and mid toms always stay dry, so the low end stays tight.
   The room also filters out low end (it hears about 250 Hz to 9 kHz).
   Measured on a closed hat: 10% is a hint (tail about 33 dB under the hit),
   25% about 25 dB under, 100% a wet late-'80s room (about 13 dB under).
   The kick is bit-for-bit identical at ROOM 0 and 100.
3. **BUS COMP (15%):** light glue compression, stereo-linked. Kept light so
   velocity still matters: a half-velocity kick is about 3 dB quieter than a
   full one. Higher settings flatten accents without adding much loudness.
4. **BUS DRIVE (15%):** tape saturation with a softened top.
5. **RECORD (60%):** the tape machine. Low-end head bump (+3 dB around
   72 Hz), a clean-up high-pass, a gentle top roll-off (about -2 dB at
   14 kHz), wow and flutter (about 0.05% rms pitch wobble, like a well-kept
   machine) and a faint tape hiss (around -73 dBFS, lower as you turn RECORD down).
6. **Mastering:** a smooth peak limiter, then a soft clip for stray peaks,
   with a -0.3 dBFS ceiling.

**Measured core checks:** kick tuning within 3 cents across TUNE -12 to +12,
kick level within about 1 dB across that range, and a clean kick body (2nd
harmonic about 39 dB down: warm but still a pure tone). The demo beat peaks
at -0.3 dBFS with no clipped samples.
Turn RECORD to 0 for the dry "straight off the machine" sound.

## Performance

Built to run lean on the KICK's ARM CPU and to feel instant on a desktop.
Measured on the demo beat (x86, Release build, stereo, 48 kHz):

| | Before audit | Now |
|---|---|---|
| Demo beat playing | 293 ns/sample (1.4% of a core) | 115 ns/sample (0.55%) |
| Beat with ROOM 100% | 428 ns/sample | 144 ns/sample |
| Idle, nothing playing | 136 ns/sample | 1 ns/sample (bus sleeps) |
| Worst case: decaying tails (denormals) | 1,821 ns/sample spike | 0.2 ns/sample |

- **Denormal-safe:** every audio callback flushes denormals to zero (x86 and
  ARM), and fast-decaying envelopes snap to zero, so decaying tails can never
  cause CPU spikes or dropouts.
- **Sleeps when idle:** when nothing has sounded for 0.25 s the whole bus stops
  processing; the next hit wakes it instantly. (The tape hiss stops in silence.)
- **No allocations on the audio thread:** everything (89 KB total) is
  allocated in `prepare()`. Checked over 3,000 callbacks with odd block sizes
  (1, 256 and 4096 samples), including mode switches and ROOM changes mid-run.
- **Lock-free:** the UI never blocks the audio thread (atomics plus a
  single-producer/single-consumer trigger queue).
- **Cheap where it counts:** compressor gain math at control rate (every 8
  samples, interpolated), wow and flutter LFOs by rotation instead of
  `std::sin`, and a fast tanh accurate to -97 dB.
- **Quality:** band-limited (polyBLEP) hat oscillators, about 15 dB less
  aliasing; smoothed LEVEL and ROOM knobs (no zipper noise); same sound at
  44.1, 48 and 96 kHz.
- **Preview app:** only changed pads repaint, timers stop when idle, the
  computer keyboard ignores auto-repeat, and it asks for a 256-sample audio
  buffer (5.3 ms). For the lowest latency on Windows, choose "Windows Audio
  (Low Latency Mode)" or an ASIO driver under AUDIO.

**On KICK's own compiler flags** (`-Ofast -ffast-math`, from `Kick/CMakeLists.txt`): the demo beat measures 0.54% of one core (0.66% with ROOM at 100%) on a 2.8 GHz Xeon, and all 27 tests pass. KICK's device build targets 64-bit ARM (`BR2_aarch64`) on the i.MX 8M Mini board (Cortex-A53 cores). A rough estimate for one A53 core is 2.5-4% for the demo beat and about 0% when idle. That's an estimate, not a measurement; run `analog83_bench` on the hardware to get the real figure.

Run `analog83_bench` (Release) to reproduce the numbers.

## Project layout

The engine is sound only: tempo and patterns come from the host's sequencer
(on KICK, its own sequencer), which drives the kit through `noteOn()`.

| Folder | What it is | Goes into the KICK PR? |
|---|---|---|
| `drumsynth/` | The engine (see `drumsynth/README.md`) | **Yes**, as `Kick/modules/engine/objects/drumsynth/` |
| `tools/` | `analog83_tests` (test suite), `analog83_test` (WAV renderer), `analog83_bench` (CPU benchmark), `Analog83DemoPattern.h` (a demo beat standing in for KICK's sequencer), `Analog83Renders.h` (the demo sounds both the renderer and the tests use) | No, stays in this repo |
| `app/` | **Analog83 Preview**: the neon kit page at KICK's 1024 x 600 size (JUCE) | No, stays in this repo |
| `integration/` | Draft for the follow-up PR: Analog '83 as a KICK Plugin program (`MTPluginProgram` + `DrumSynthProgram`), written against the real KICK source | Not yet (second PR) |
| `docs/` | Developer notes | No |
| `renders/` | Demo WAV files | No (attach to GitHub comments instead) |

## Testing

`analog83_tests` runs 27 checks and exits with 0 when everything passes:

1. **MIDI note numbers:** the GM drum map.
2. **Single-voice mode:** chromatic pitch accuracy.
3. **Dry outputs:** timing and separation.
4. **Threads:** four threads sending notes at once, with exact delivery.
5. **Real-time safety:** zero heap allocations in 3,000 audio callbacks.
6. **Demo renders:** compared against `tools/analog83_fingerprints.txt`.

Run it in Visual Studio by picking **analog83_tests.exe** as the startup item, or from **Test > Run CTests**.

| Command | When to use it |
|---|---|
| `analog83_tests` | Any time. Compares each render's level every 20 ms within 0.25 dB, so it catches real sound changes but ignores tiny rounding differences between compilers (GCC vs MSVC) and KICK's `-Ofast -ffast-math` build. |
| `analog83_tests --strict` | Before and after a change that should not affect sound at all, like running the formatter. Also requires every render to be bit-exact. Only meaningful against fingerprints made with the same compiler. |
| `analog83_tests --update` | Only when you intend to change the sound. Rewrites the fingerprint file from the current engine. Listen first, then update. |

The committed fingerprints were made with GCC on x86. If the first run in Visual Studio shows small fingerprint differences while everything else passes, that's the compiler; run `--update` once on Windows so later runs (and `--strict`) compare against your own baseline.

**Formatting workflow:** run `analog83_tests --update`, format the files, then run `analog83_tests --strict`. Every render must stay bit-exact.

## Code style

`.clang-format` at the root is KICK's own (copied from `Kick/.clang-format`). In Visual Studio, **Edit > Advanced > Format Document** (Ctrl+K, Ctrl+D) applies it to the open file. Format the `drumsynth/` files before copying them into KICK.

## Build on Windows (Visual Studio 2022)

1. Install Visual Studio Community with the **Desktop development with C++** workload,
   and install **Git for Windows** (git-scm.com). CMake uses Git to download JUCE.
2. **File > Open > Folder** and pick this folder. Visual Studio reads `CMakeLists.txt`.
   The first configure downloads JUCE 7.0.12 (the version KICK uses), so it takes a few minutes.
3. In the startup item dropdown, pick one:
   - **Analog83Preview.exe** – the kit page app
   - **analog83_test.exe** – renders WAV files to the build folder
4. Press **F5**.

To build only the WAV test (no JUCE download), set the CMake option `A83_BUILD_APP` to `OFF`,
or from a *Developer Command Prompt*:

    cl /std:c++17 /O2 /EHsc /I drumsynth /I tools drumsynth\*.cpp tools\analog83_test.cpp /Fe:analog83_test.exe
    analog83_test.exe

## Using the preview app

- **Click a pad** to play it and open its voice page. KICK, CL HAT and OP HAT play; the
  other pads are marked SOON.
- **Computer keyboard** plays the pads in the same 4 x 4 layout:
  `1 2 3 4` / `Q W E R` / `A S D F` / `Z X C V`.
- **Drag a knob** up or down to change the sound. The waveform redraws from the real voice
  as you turn it. **Double-click** a knob to reset it.
- **Tabs** (ENVELOPE, PITCH, DIST...) switch the knobs. Greyed-out tabs aren't built yet.
  DIST has the voice's DRIVE plus the kit-wide BUS DRIVE; OUTPUT has LEVEL plus the
  kit-wide BUS COMP, RECORD and ROOM.
- **Play** (or the space bar) runs a demo beat at 92 BPM with a hat roll at the end of the bar.
- **AUDIO** picks your output device if you hear nothing.

## Output

All files are stereo and rendered through the full kit and record chain, so they sound like the app.

| File | What it shows |
|---|---|
| `kick_default.wav` | default kick |
| `kick_long_boom.wav` | tuned down, long decay, more punch and tone |
| `hat_closed.wav`, `hat_open.wav` | the two hat types |
| `hat_choke.wav` | open hat cut off by a closed hat |
| `pattern_trap_140.wav` | 2-bar trap pattern with x2, x3, x4 and x6 hat rolls and velocity ramps |
| `pattern_kit_92.wav` | the preview app's 92 BPM demo beat (from `Analog83DemoPattern.h`) |

## Using the engine

    a83::Kit kit;
    kit.prepare(48000.0);

    // UI thread
    kit.setParam(a83::Param::KickTune, -3.0f);
    kit.trigger(36, 1.0f);                              // pad press by MIDI note
    kit.setMode(a83::KitMode::Single, a83::VoiceId::Kick, 36); // kick as a chromatic tuned bass

    // Audio thread, each block
    kit.noteOn(42, 0.8f, 37);                           // closed hat at sample 37 of this block
    kit.process(left, right, numSamples);               // stereo, ADDS into left/right
    // or kit.processDry(outputs, numSamples);          // one dry channel per voice

See `drumsynth/README.md` for the full API, the note map and the threading rules, `drumsynth/ARCHITECTURE.md` for how it all works (including how to add a voice), and `drumsynth/CREDITS.md` for the published techniques and values the engine builds on.

## Next steps

- Snare, clap, toms and cowbell voices
- Wrap the kit in a KICK AudioObject behind `MTProgramType::Plugin` (a full kit
  plus single voices), tested in KT_KickEM
- Move the kit page from this preview app into the KICK app's UI
