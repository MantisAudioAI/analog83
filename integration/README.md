# Integration draft: Analog ’83 as a KICK Plugin program

**Status: draft for the follow-up PR, not compiled yet.** It's written against the real KICK source (`0700-AAM-main`, October 2026), mirroring how `MTDrumProgram` and `DrumProgram` work. The first PR only adds the engine; this second PR makes it playable as a Plugin program.

## How KICK plays a program (what this mirrors)

| Layer | Drum program (existing) | Plugin program (this draft) |
|---|---|---|
| **Data side**: settings in the project ValueTree, routing, sends, inserts | `MTDrumProgram` (`audio_program/source/`) | `MTPluginProgram` (`audio_program/source/mt_audio_plugin_program.*`) |
| **Audio side**: an `ISynthObject` the engine runs | `DrumProgram` (`audio_program/synth/`) | `DrumSynthProgram` (`objects/drumsynth/drumSynthProgram.*`) |
| **Created by** | `MTAudioProgramManager::createProgram()` | One new line in the same function |

Notes reach a program two ways, both ending in `noteOn( Note, Velocity )`:

- **The track's sequencer**, through `noteProcess()`. `MTIProgram`'s default turns MIDI note-on messages into `noteOn()`.
- **Live pads**, through `MTAudioInterface::addLiveMidiNote()`.

Because these can come from different threads, the engine's `Kit::trigger()` uses a lock-free multi-producer queue. It was tested under ThreadSanitizer with four threads sending at once.

## Edits, step by step

### 1. `Kick/modules/engine/objects/` (already in the first PR)

`objects.h`, after the sampler includes:

```cpp
    #include "drumsynth/Analog83Kit.h"
    #include "drumsynth/drumSynthProgram.h"   // follow-up PR
```

`objects.cpp`, after the sampler sources:

```cpp
#include "drumsynth/Analog83Kick.cpp"
#include "drumsynth/Analog83HiHat.cpp"
#include "drumsynth/Analog83Record.cpp"
#include "drumsynth/Analog83Kit.cpp"
#include "drumsynth/drumSynthProgram.cpp"    // follow-up PR
```

### 2. `Kick/modules/engine/audio_program/`

Copy `source/mt_audio_plugin_program.h/.cpp` in, then:

- **`audio_program.h`:** after `#include "source/mt_audio_drum_program.h"`, add
  `#include "source/mt_audio_plugin_program.h"`
- **`audio_program.cpp`:** after `#include "source/mt_audio_drum_program.cpp"`, add
  `#include "source/mt_audio_plugin_program.cpp"`

### 3. `Kick/modules/engine/audio_engine/engine/mt_audio_manager_program.cpp`

In `MTAudioProgramManager::createProgram( juce::ValueTree& VT, bool addToTree )`, next to the other types:

```cpp
        if( type == MTProgramType::Plugin ) prog = new MTPluginProgram( engine, VT, undoManager );
```

Today `MTProgramType::Plugin` exists in the UI, but `createProgram()` returns `nullptr` for it. This line is what gives it an engine.

## The design question for the maintainers

The reviewer suggested `objects/drumsynth/` for the engine, which fits. But `objects` depends on `audio_program` (see the dependencies in `objects.h`), so `MTPluginProgram` in `audio_program` can't `#include` the engine directly. The draft solves this with a creator function: `drumSynthProgram.cpp` registers itself with `MTPluginProgram::setSynthCreator()` at startup, like `REGISTER_OBJECT`. KICK's object factory can't be used as-is because it only builds objects with no constructor arguments, and `ISynthObject` needs the engine's `AudIO::Design&`.

Alternatives to offer:

- **A.** Keep the creator hook (this draft). The engine stays in `objects/drumsynth/`, as suggested, and later synth engines can register the same way.
- **B.** Move `DrumSynthProgram` and the engine into `audio_program/synth/` next to `DrumProgram`. That's simpler with no hook, but the engine leaves `objects/`.
- **C.** Extend `AudIO::ObjectFactory` to pass a `Design&`. That's the most general option, but it touches the AudIO library.

## Checked against the KICK source

Every KICK name the draft uses was looked up in `0700-AAM-main` (October 2026) and matches:

| Used in the draft | Where it's defined | Status |
|---|---|---|
| `ISynthObject( AudIO::Design&, int Polyphony, BusType = Object )` | `audio_program/synth/iSynthObject.h` | matches |
| `noteOn( int, int )`, `noteOff( int )`, `noteOff( std::vector<int>& )`, `noteOffAll()` (overrides) | `IMidiMsgProcessor` and `ISynthObject` | matches |
| `initialize( uint32_t, uint32_t )`, `doProcess( juce::AudioBuffer<float>& )` (overrides) | `ISynthObject` (called from its `prepareToPlay` / `process`) | matches |
| `setProgram( MTIProgram* )`, `setBypass( bool )`, `setName( std::string )` | `ISynthObject`, `AudIO::Object` | matches |
| `CommonUtils::isValidMidiNote( int )` | `common/source/kick_common_utils.h` | matches |
| `MTIProgram( juce::ValueTree&, juce::UndoManager* )`, `getName()` returning `juce::String` | `audio_program/source/mt_audio_iprogram.h` | matches |
| `enableOutputMetering( bool )`, `readOutMeter() const` returning `const MTMeterData&` | `MTIProgram` | matches |
| `Engine.createObject( std::unique_ptr<Object> )`, `engine.removeObject( id )`; `DawEngine` derives from `Design` | `AudIO/Includes/Design.h`, `AudIO/Daw/DawEngine.h` | matches |
| `initAudioRouting()`, `connectOutputTo( id )`, `setGainsForSend( vector )`, `setGainForSend( int, float )` (public) | `AudIO/Daw/DawRouting.h` | matches |
| `MTIInsertsInterface::setup( ValueTree&, UndoManager*, DawInserts* )` | `interface/source/mt_audio_inserts_types.h` | matches (`ISynthObject` is a `DawInserts`) |
| `onNameChanged`, `onOutChanged`, `onSendGainChanged`, `getOutput()`, `getSendGains()`, `IDs::INSERTS` | `MTIProgram`, used the same way by `MTDrumProgram` | matches |
| Abstract methods a program must implement | only `noteProcess()`, which `MTIProgram` already implements | none left |

This confirms the names and signatures, not that it compiles. That still needs JUCE and a real build.

## Before it's ready

- [ ] Compile in KICK (Mac mini / KT_KickEM) and fix what the draft got wrong.
- [ ] Decide which outputs KICK wants: `process()` (stereo with the character bus), or `processDry()` per voice into KICK's mixer.
- [ ] A Plugin program edit page (the Analog ’83 kit page) and saving parameters in the ValueTree.
- [ ] Test in KT_KickEM with the reviewer.
