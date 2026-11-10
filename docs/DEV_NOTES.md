# Analog ’83 for KICK: developer notes

**Optimization and cleanup pass, October 29, 2026**

This note records what changed in the latest build of the Analog ’83 engine and preview app, why it changed, and how each change was checked. Numbers are from an x86 machine, Release build (`-O2`), 48 kHz stereo, unless stated otherwise.

## Results at a glance

### Whole kit (ns per sample; 20,833 ns = 100% of one core at 48 kHz)

| Scenario | Before | After | Change |
|---|---|---|---|
| Demo beat playing, defaults | 293.3 (1.41%) | 115.2 (0.55%) | 2.5× cheaper |
| Idle, nothing playing | 136.0 (0.65%) | 1.1 (0.01%) | effectively zero |
| Beat playing, ROOM 100% | 427.7 (2.05%) | 143.9 (0.69%) | 3.0× cheaper |
| Silence after a hit, ROOM 100%, RECORD 0 (denormal worst case) | 1,821.3 | 0.2 | spike eliminated |

### Per stage (ns per sample)

| Stage | Before | After | What changed |
|---|---|---|---|
| Kick voice (sounding) | 273.5 | 44.1 | Denormal fix (envelopes snap to zero) |
| Hat voice (sounding) | 42.0 | 42.2 | Same cost with polyBLEP added |
| Bus compressor gain | 27.7 | 7.2 | dB math at control rate |
| Saturator | 20.8 | 6.0 | Fast tanh |
| Room (stereo) | 21.9 | 21.6 | Unchanged; now bypassed entirely at 0% |
| Record chain (stereo) | 56.6 | 44.4 | Rotating LFOs, precomputed depths |
| Soft clip | 10.8 | 7.1 | Fast tanh |

### Memory

- Total engine footprint is **89 KB**: a 4.1 KB `Kit` object plus 84.7 KB of buffers, all allocated in `prepare()`.
- There are **zero allocations on the audio thread**. Verified over 2,400 callbacks with mixed block sizes (1, 256 and 4096 samples), ROOM toggling on and off, and sleep/wake cycles.

## Engine changes

### 1. Denormal protection (the biggest win)

**Problem.** Decaying values (filter states, reverb tails, fast envelopes) drift into the denormal float range, where x86 CPUs run 10–100× slower. Two places were affected:

- **The kick's envelopes.** The pitch, droop and beater envelopes decay much faster than the body and went denormal while the kick was still sounding. That's why the kick cost 6× more than the hat.
- **The room's comb filters** after a hit. This was the 1,821 ns/sample spike, a classic cause of random audio dropouts.

**Fix** (`Analog83Common.h`, `Analog83Kick.cpp`, `Analog83Kit.cpp`):

- `ScopedFlushDenormals` sets flush-to-zero for the duration of each `Kit::process()` call, then restores the previous mode. On x86 that's FTZ + DAZ in MXCSR; on AArch64 it's the FZ bit in FPCR. The ARM path is excluded under MSVC.
- `snapToZero()` snaps fast envelopes to exactly 0 below 1e-9, so the voices stay safe even when used outside the kit.

### 2. Idle sleep

**Problem.** The bus chain (compressor, tape, record, limiter) ran at full cost with nothing playing.

**Fix** (`Analog83Kit.h/.cpp`): once every voice has finished and the bus signal, measured *before* the tape hiss, has stayed below about −100 dBFS for 0.25 s, `renderVoices()` returns immediately. `fire()` wakes the bus on the next trigger, with no added latency.

**Side effect (intended):** the tape hiss stops during silence, like a live instrument, and still runs under everything you play.

### 3. Compressor at control rate

**Problem.** `BusCompressor::gainFor()` ran `log10` and `pow` every sample.

**Fix:** the detector still runs every sample. The dB gain computer (`computeGain()`) runs every 8 samples (0.17 ms), and the gain glides linearly in between. That's far finer than the 4 ms attack, so it behaves the same while doing roughly 8× less transcendental math.

### 4. Wow and flutter without `std::sin`

**Problem.** `WowFlutter::process()` called `std::sin` twice per sample and recomputed its depth math every sample.

**Fix** (`Analog83Record.h/.cpp`, `Analog83Common.h`):

- A new `SineLfo` class rotates a unit vector each sample (a few multiplies) and renormalizes every 4,096 samples so its amplitude never drifts.
- The wow and flutter depths and the base delay are now precomputed in `setDepth()`.

### 5. Fast tanh

**Fix:** `fastTanh()` is a Padé 7/6 approximant with clamping. It's used in `Saturator::process()` and `softClip()`.

**Accuracy:** max error is 1.5e-5 (−97 dB) for |x| < 4, and 9.6e-5 (−80 dB) in the worst case, both inaudible. `Saturator::setAmount()` uses the same function for its DC offset, so silence stays exactly 0. The 18% drive still measures 2.91% THD, mostly the 2nd harmonic, exactly as before.

### 6. Band-limited hat oscillators (quality)

**Problem.** The naive square waves alias, which folds harsh, out-of-tune junk back into the audible range.

**Fix** (`Analog83HiHat.cpp`): polyBLEP correction on both edges of each of the six squares.

**Measured:** aliasing is 16 dB lower at 522.7 Hz (−20.6 → −36.6 dB) and 15 dB lower at 1,067.9 Hz (−17.4 → −32.8 dB). The cost per sample is unchanged.

### 7. Smoothed level knobs (quality)

**Problem.** LEVEL changes jumped once per audio block, causing zipper noise while a sound rang.

**Fix:** `levelGain` follows `levelTarget` through a 5 ms one-pole filter, in both the kick and the hat. A fresh hit, when the voice was idle, starts directly at the knob's level.

### 8. Sample-rate independence (bug fix)

**Problem.** The room's comb damping used a fixed per-sample coefficient, so the room got brighter and longer at higher sample rates.

**Fix:** `damp = kRoomDamp^(48000 / sampleRate)`.

**Measured at 44.1 / 48 / 96 kHz:**

- **Kick:** identical pitch (49.95 Hz) and levels.
- **Hat:** audible-band level within 0.4 dB.
- **Room tail:** within about 1.1 dB (−43.0 / −42.4 / −43.5 dB).

## Sound and default changes made in this round

| Change | Before | After | Reason |
|---|---|---|---|
| ROOM default | 25% | **0% (off)** | A room on everything muddied everyday mixes |
| Room routing | kick and hats sent, hats more | **hats, snare, clap and percussion only**; kick and low/mid toms always dry | Your call: keep the low end tight |
| Room send filter | high-pass at 180 Hz | **band 250 Hz to 9 kHz** | No low end in the room, slightly dark top |
| ROOM knob | stepped | **smoothed (20 ms)**, bypassed and cleared at 0% | No zipper noise; saves CPU |
| BUS COMP default | 50% | **15%** | 50% flattened velocity (a half-velocity kick was only 0.9 dB quieter); now about 3 dB quieter, with the same loudness |
| Hat attack | instant (one-sample jump) | **0.3 ms ramp** | Removed the digital click, like an analog VCA opening |
| Hat LEVEL default | −9 dB | **−3 dB** | Restores the kick/hat balance (hat about 20 dB under the kick) after the compressor and polyBLEP changes |

**Room calibration.** Measured on a closed hat, as the room tail 150–400 ms after the hit relative to the hit:

| ROOM | Tail level |
|---|---|
| 10% | −33 dB |
| 25% | −25 dB |
| 50% | −19 dB |
| 100% | −13 dB |

The kick output is bit-for-bit identical at ROOM 0% and 100%.

## Preview app changes (screen code, JUCE)

| Area | Change |
|---|---|
| Pad grid | Repaints only the pads that changed, and `paint()` skips pads outside the clip region |
| Pad timer | Runs only while playing or while a pad is still fading, then stops; `wake()` starts it when the beat starts |
| Voice panel timer | Starts on demand when a knob moves, redraws at most 30 times a second, then stops |
| Waveform drawing | Filled 1-pixel columns (`fillRect`) instead of antialiased lines, which is cheaper in JUCE’s software renderer |
| Kick waveform preview | Rendered at 16 kHz (0.89 → 0.26 ms per redraw; looks identical); hats stay at 48 kHz |
| Keyboard | Auto-repeat ignored, so holding a key plays the pad once, like a real pad |
| Audio latency | Requests a 256-sample buffer (5.3 ms) and keeps the previous setup if the device refuses |
| Audio callback | `juce::ScopedNoDenormals` added (standard practice, on top of the engine’s own guard) |
| Main window | `setOpaque(true)`, so JUCE never paints behind it |

## New tooling

- **`tools/analog83_bench.cpp`** with a CMake target, `analog83_bench`. It times every stage and the whole kit (playing, idle, ROOM 100%, denormal worst case). Run it in Release.
- **README:** a new Performance section with the before/after table.

## How everything was verified

- **Strict build:** zero warnings with `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion`.
- **Unity-build safe:** KICK compiles its engine objects through `objects.cpp`, which combines several `.cpp` files into one. The kick and hat both defined helper constants named `kNormalise` and `kSilence`, which clashed when combined. They're now `kKickNormalise`/`kKickSilence` and `kHatNormalise`/`kHatSilence`. All four engine files compile as one with zero errors or warnings, and the renders are bit-for-bit identical.
- **Memory checker:** AddressSanitizer and UndefinedBehaviorSanitizer on the full test program, with no errors.
- **Timing:** the sequencer hit counts are still exact (kick 7, closed hat 26, open hat 2).
- **Sound regression against the pre-audit renders:**
  - The kick differs by −53 to −58 dB, which is inaudible.
  - The hats differ in waveform detail because the aliasing was removed; their loudness is within 0.4 dB.
  - Both patterns have identical loudness (−17.0 and −12.3 dB RMS) and peaks (−0.3 dBFS).
- **Core checks:**
  - Kick tuning is within 3 cents from TUNE −12 to +12, and its level holds within about 1 dB across that range.
  - The kick body's 2nd harmonic is about 39 dB down: warm, but still a pure tone.
  - Zero clipped samples.

## Known limits and things not yet verified

- **The preview app hasn’t been compiled.** JUCE couldn’t be downloaded in the build environment, so the first Visual Studio build is its real test.
- **The ARM flush-to-zero path (FPCR inline assembly) hasn’t been compiled yet.** It was only built for x86. The Mac mini (Apple Silicon is AArch64) will be its first real test in December, before the KICK itself.
- **The CPU numbers are x86.** Expect higher absolute numbers on the KICK’s ARM CPU; the relative gains should carry over.
- **Tape hiss stops when the bus sleeps.** This is intended, but worth knowing if you ever want constant hiss.
- **The room tail is cleared when ROOM reaches exactly 0%.** That's intended, so an old tail can never reappear later.
- **The compressor's envelope detector still runs every sample.** It's cheap, but it could move to control rate too if the KICK’s CPU budget needs it.

## Restructure for KICK integration (later on October 29)

This follows a contributor's review on issue #16. They suggested putting the engine next to the sampler, keeping tempo and patterns in KICK's own sequencer, and hooking it up later through `MTProgramType::Plugin`.

| Change | Details |
|---|---|
| **Engine is sound only** | Removed `setPlaying()`, `isPlaying()`, `setBpm()`, `getBpm()`, the tick clock and the demo pattern from `Analog83Kit`. |
| **New `noteOn(voice, velocity, sampleOffset)`** | Audio-thread call for a host sequencer: each hit starts at an exact sample inside the next `process()` call. Up to 128 notes per block, kept in a fixed array (no allocation), sorted with a short insertion sort, with rendering split at each note. `trigger()` stays for UI pad presses. |
| **`tools/Analog83DemoPattern.h`** | The demo beat moved out of the engine. It drives the kit through `noteOn()` exactly as KICK's sequencer will, and is used by the test tool, benchmark and preview app. |
| **New layout** | `drumsynth/` = the engine (maps to `Kick/modules/engine/objects/drumsynth/`, with its own README for maintainers); `tools/` = test, benchmark, demo pattern; `app/` = preview. The old `src/` folder is gone. |
| **Preview app** | The Play button's state and the demo pattern now live in `MainComponent`, so the engine knows nothing about tempo. |

**Verified:**

- Zero warnings (strict flags) for the engine, the test tool, the benchmark and a simulated `objects.cpp` unity build.
- No AddressSanitizer or UndefinedBehaviorSanitizer errors.
- Sequencer hit counts exact.
- Renders compared with before the restructure: `kick_default`, `kick_long_boom`, `pattern_trap_140` and `pattern_kit_92` are **bit-for-bit identical**. `hat_choke` differs only in 32 samples at 480 ms, where the bus goes to sleep and the tape hiss stops a few samples earlier (−70 dB, after the drums have ended).
- CPU is unchanged: the old and new builds measure in the same range when run side by side.

## Integration features (later on October 29)

Three additions so the engine fits KICK's sequencer, MIDI and mixer, following the same review:

| Feature | Details |
|---|---|
| **MIDI note numbers** | `noteOn(int midiNote, velocity, offset)` and `trigger(int midiNote, velocity)` use the General MIDI drum map: 35/36 kick, 42/44 closed hat, 46 open hat. Unmapped notes return false. `Kit::voiceForNote()` exposes the map. |
| **Single-voice mode** | `setMode(KitMode::Single, voice, rootNote)` plays one voice chromatically, for single-voice Plugin programs. The root note plays at the voice's TUNE, and other notes shift it by semitones. Each voice got a per-hit pitch offset: kick ±36 semitones, hat ±24 (its oscillators are recomputed only when the pitch changes). The mode is stored in atomics, safe to switch from the UI thread. |
| **Dry outputs** | `processDry(outputs, n)` gives one dry mono channel per voice (`DryOutput::Kick`, `DryOutput::Hat`), with each voice's DRIVE and LEVEL but no pan, room, bus, tape or limiter, for KICK's own mixer and effects. A nullptr output is skipped while its voice still advances. |
| **Shared scheduling** | `process()` and `processDry()` both run through one templated `runBlock()`, so parameters, pad presses and sample-accurate notes behave identically in both. |

**Verified:**

- **15 feature checks pass** under the memory checkers (an earlier note said 16; that was a miscount):
  - The note map works, and unmapped notes are rejected.
  - Chromatic kick pitches 25 / 50 / 74.8 / 99.9 Hz for notes 24 / 36 / 43 / 48, within 3 cents; the small offset is the intentional per-hit drift.
  - Dry outputs start on exactly the scheduled samples and stay fully separated.
  - The dry kick is bit-for-bit identical to the bare kick voice.
- All existing `process()` renders are **bit-for-bit identical**.
- Zero warnings, including the unity build.
- Zero allocations across 3,000 callbacks that mixed `process()` and `processDry()`, note numbers and mode switches.
- CPU unchanged.

## Reviewed against the real KICK source (later on October 29)

With the full repo (`0700-AAM-main`) in hand, I traced how KICK plays a program:

- **`MTAudioProgramManager::createProgram()`** creates a data-side `MTIProgram` per type (`MTDrumProgram` and so on), and there is **no `MTProgramType::Plugin` branch**, so Plugin programs currently get `nullptr`.
- **Each data-side program creates an audio-side `ISynthObject`** (`DrumProgram`, built on `ASliceRenderer` sample voices), registers it with the engine, and wires up output routing, sends, inserts and metering.
- **Notes arrive as `noteOn( Note, Velocity )`** with MIDI note numbers and 0–127 velocity, from two places: the track's sequencer (`noteProcess()`) and live pads (`addLiveMidiNote()`). That's more than one thread.
- **`objects.cpp` is a unity build**, and `objects` depends on `audio_program`, not the other way around.

| Change | Details |
|---|---|
| **Bug fix: multi-thread-safe `trigger()`** | The old queue assumed one sending thread. Under ThreadSanitizer, four threads sending at once caused a **data race and lost or garbled 34,169 of 80,000 events**. It was replaced with a bounded lock-free multi-producer queue (Vyukov's design): 80,000 of 80,000 delivered, 0 ThreadSanitizer warnings, and every accepted press fired exactly once in the full kit. `trigger( VoiceId, … )` now returns false when the queue is full, like the note version. |
| **License headers** | Every engine file now starts with KICK's own AGPL-3.0-or-later block, with a FlowSphereAI 2026 copyright line. |
| **`.clang-format`** | KICK's file is copied to the project root, so Visual Studio's Format Document applies KICK's style. clang-format wasn't available in the build environment, so formatting is done on Windows. |
| **Macro check** | None of KICK's 38 `#define` names match any identifier in the engine, so the unity build can't silently rewrite engine code. |
| **Integration draft** | `integration/` holds `MTPluginProgram` (data side, mirroring `MTDrumProgram`), `DrumSynthProgram` (an `ISynthObject` wrapping `a83::Kit`), and the exact edits to `objects.h/.cpp`, `audio_program.h/.cpp` and `createProgram()`. A startup creator hook avoids the `objects`/`audio_program` circular dependency; alternatives are listed for the maintainers. **Not compiled yet**, since JUCE isn't available here. |

**Verified after these changes:**

- Zero warnings on the engine, tools and unity build.
- 15 of 15 feature checks pass.
- All 7 renders are bit-for-bit identical.
- CPU is unchanged.

## Test suite, product-name cleanup and integration check (later on October 29)

| Change | Details |
|---|---|
| **`tools/analog83_tests`** (new) | 27 checks in one program (exit code 0 = all passed), registered with CTest: GM drum note map (4), Single-voice pitch (6), dry outputs (5), threads (3: 80,000 events from 4 threads delivered; every accepted kick and hat fired exactly once), zero allocations across 3,000 callbacks (1), and the 7 demo renders plus pattern hit counts against fingerprints (8). |
| **Render fingerprints** | `tools/analog83_fingerprints.txt` stores, per render, its length, peak, an exact 16-bit sample hash, and the level every 20 ms per channel. Normal runs allow 0.1 dB (1 dB below −70 dBFS), so a real change fails but GCC/MSVC rounding doesn't. `--strict` also requires bit-exact hashes; `--update` rewrites the file. |
| **Proven both ways** | Changing the kick's default decay by 10 ms **failed** exactly the two renders that use the default kick. Doubling every indent and adding KICK-style spaces inside every parenthesis **passed `--strict`**, bit-exact. |
| **`tools/Analog83Renders.h`** (new) | The 7 demo renders moved into one shared header, so the WAV renderer and the tests always use identical sounds. The renderer's output stayed bit-for-bit identical after the move. |
| **No product names in the code** | Removed the remaining mentions of other companies' drum machines and products from code comments, both READMEs and the PR template, with neutral wording (“tuned bass”, “single-voice Plugin programs”). Behringer and KICK stay only where they describe the project itself. |
| **Integration draft checked** | Every KICK name and signature the draft uses (11 groups) was looked up in the real source and matches. The only abstract method a program must implement, `noteProcess()`, is already implemented by `MTIProgram`. The startup-registration variable is marked `[[maybe_unused]]`. |

## Credits added (later on October 29)

- **Product-name check:** zero mentions of Roland, 808, 909 or any “TR-” model in any `.h`, `.cpp`, `.txt` or CMake file. Those names appear in exactly two places in the project, both paper titles in `drumsynth/CREDITS.md`, because a correct citation has to use the real title.
- **New `drumsynth/CREDITS.md`** (goes into the PR with the engine). It cites every published technique and value the engine uses:
  - **Freeverb** (Jezar at Dreampoint, June 2000, public domain): the room's comb and all-pass lengths and stereo spread are Freeverb's tuning values (1116/1188/1277/1356, 556/441, spread 23 at 44.1 kHz) converted to ms. The code comment previously described these as our own choice; it now credits Freeverb.
  - **The six-oscillator hi-hat:** the architecture is analyzed in Werner, Abel & Smith (ICMC/SMC 2014). The six frequencies are widely published values that I couldn't trace to a single primary source, and the file says so.
  - **Algorithms:** the TPT state-variable filter (Simper; Zavalishin), the RBJ Audio EQ Cookbook, polyBLEP (Valimaki & Huovilainen, 2007), Vyukov's bounded queue, xorshift (Marsaglia, 2003), the DC blocker (J. O. Smith), the Padé tanh, and Hermite interpolation.
  - **Background reading,** clearly marked as not used in the code: the 2014 bass drum paper and the other projects mentioned in issue #16.
- **Code comments** now point to `CREDITS.md` next to each borrowed technique, in neutral wording. These were comment-only changes: `analog83_tests --strict` still passes 27 of 27 bit-exact, with zero warnings.
- **Verified by search:** Freeverb's authorship, public-domain status and tuning values; the 2014 cymbal and bass drum papers (authors, titles, venues). The other algorithm citations are standard references.

## MSVC-proofing, preview-app check and KICK build flags (later on October 29)

| Change | Details |
|---|---|
| **MSVC warning C4324** | `alignas(64)` in `MpscQueue` makes MSVC warn that the queue, and the `Kit` that contains it, were padded. The padding is intended, so the warning is now disabled for MSVC only, scoped to those two classes. |
| **MSVC warning C4127** | `while (true)` in the demo pattern is now `for (;;)`. |
| **Source encoding** | All C++ files confirmed pure ASCII, so no code-page problems on Windows. |
| **Preview app compile check** | Built a stand-in for the 45 JUCE 7.0.12 names the app uses, with JUCE's real signatures, and compiled every app file against it. **0 errors and 0 warnings** with strict flags, and the full app **links**, so every declared function exists. A planted mistake (a call to the removed `kit.isPlaying()`) was caught. The stand-in initially lacked JUCE's `String(float)` constructor, which caused one false warning; adding it cleared that. This proves the code is internally consistent; it can't prove it runs. |
| **KICK's build flags** | `Kick/CMakeLists.txt` compiles with `-Ofast -ffast-math`. The engine has no NaN or infinity checks for fast-math to break. |
| **Timing bug found (test tools)** | Under fast-math, two renders differed. The cause: hits that land exactly on a whole sample in exact math (step 7 at 140 BPM = sample 36000, plus four others) computed as 35999.9999… and were truncated a sample early, depending on compiler flags. The engine was fine; the WAV renderer's `secondsToSamples()` and the demo pattern's `ceil()` now use a 1e-6 epsilon. One hat in the trap render moved one sample to its correct position (it had been a sample early in the normal build), so the fingerprints were updated. All 34 demo-pattern hits now land on identical samples in both builds. |
| **Fingerprint tolerance** | 0.1 dB → 0.25 dB. Fast-math rounding in the limiter moved one transient window by 0.11 dB (−49 dB overall difference). A 10 ms decay change still fails 2 renders. |
| **`DemoPattern::schedule()`** | Now a template that drives any target with `noteOn( voice, velocity, offset )`, used to verify exact hit positions. |
| **CPU on KICK's flags** | Demo beat 0.54% of one 2.8 GHz Xeon core (0.66% with ROOM 100%). The device build targets AArch64 (`BR2_aarch64`) on the i.MX 8M Mini board. Rough estimate: **2.5–4% of one Cortex-A53 core** for the demo beat, about 0% idle. This is an estimate; measure on hardware. |

**Verified:**

- Normal build: 27/27 normal and strict.
- KICK flags: 27/27.
- ThreadSanitizer under KICK flags: 0 warnings.

## Follow-ups

- [ ] Build the preview app on Windows and fix any compile errors.
- [ ] Run `analog83_bench` on the Windows PC and keep the numbers alongside these.
- [ ] December: build on the Mac mini (tests the ARM flush-to-zero path), then run in KT_KickEM.
- [ ] Give the snare, clap and percussion voices their room sends when they’re built; keep the low and mid toms dry.
- [ ] Format the `drumsynth/` files with Visual Studio's Format Document (KICK's `.clang-format` is in the project root).
- [ ] Ask maintainers whether the classes should live in `mt::` or keep the `a83` namespace.
- [ ] Follow-up PR: wrap the kit in an `AudioObject` behind `MTProgramType::Plugin` (full kit plus single voices).
- [ ] Profile on the KICK hardware once available; consider control-rate envelope detection if needed.
