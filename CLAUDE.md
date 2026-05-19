# Amplitron Processing Agents & System Architecture

This document outlines the **Agent Architecture** within the Amplitron Guitar Amp Simulator. In the context of this high-performance DSP (Digital Signal Processing) pipeline, an "Agent" is defined as an autonomous, encapsulated component responsible for specific processing, coordination, or state management tasks. 

These agents operate concurrently across high-priority audio threads and UI threads, ensuring ultra-low latency (~1.3ms) and glitch-free real-time performance.

---

## 1. System-Level Coordination Agents

These are the core manager agents that oversee the lifecycle, data flow, and hardware interaction of the Amplitron ecosystem.

### 1.1 The Audio Engine Agent (`audio_engine.cpp`)
The master coordinator of the system. Operating at the highest system priority via PortAudio, this agent is responsible for the critical real-time audio callback loop.
* **Role:** Fetches raw mono float32 samples from the hardware input (USB interface/guitar cable), routes them sequentially through the active DSP agents, and pushes the processed frames to the output hardware.
* **Responsibilities:** * Auto-detecting input/output hardware.
  * Buffer size negotiation (32 to 512 samples) and sample rate enforcement (44.1kHz - 96kHz).
  * Enforcing safety clamps (hard limiting to ±1.0) to prevent hardware or auditory damage.
  * Handling lock-free or `try_lock` mutex polling to ensure the GUI thread never blocks the audio thread.

### 1.2 The GUI Manager Agent (`gui_manager.cpp`)
The user-facing orchestration agent built on SDL2 and Dear ImGui.
* **Role:** Translates human interaction into system state changes without interrupting the DSP pipeline.
* **Responsibilities:** Rendering the application window, parsing hardware input (mouse/keyboard), painting the pedal board visually, and updating the global state tree that the Audio Engine Agent consumes.

### 1.3 The Pedal Board Agent (`pedal_board.cpp`)
The state-manager agent for the signal chain.
* **Role:** Acts as a dynamic registry that maintains the ordered list of active DSP Pedal Agents.
* **Responsibilities:** Handling the insertion, deletion, reordering, and bypass toggling of effects. It safely mutates the chain state while communicating with the Audio Engine.

### 1.4 The Preset Manager Agent (`preset_manager.cpp`)
The persistence agent for signal chain configurations.
* **Role:** Serializes and deserializes the complete pedal chain state (effect types, parameter values, enabled states, ordering) to and from JSON files.
* **Responsibilities:** Saving presets, loading presets, creating effect instances by name during deserialization, and managing the presets directory. Integrates with the file dialog system for save/load UI.

### 1.5 The Command History Agent (`command_history.cpp`)
The undo/redo state-tracking agent.
* **Role:** Maintains a stack-based history of all user actions (parameter changes, pedal additions/removals, reordering) to enable full undo/redo functionality.
* **Responsibilities:** Recording commands, executing undo/redo, managing the history stack with proper cleanup when new actions branch off from a past state.

### 1.6 The Snapshot Manager Agent (`snapshot_manager.h`, `gui_snapshots.h/.cpp`)
The in-session A/B/C/D board-state switching agent.
* **Role:** Stores up to 4 complete board configurations in memory for instant, glitch-free recall during a live performance session — without any file I/O.
* **Responsibilities:** Capturing the full effect chain state (effect instances, enabled/mix flags, all parameter values, input/output gains) into numbered slots; restoring a slot via `RecallSnapshotCommand` (undoable via Ctrl+Z); and rendering the [A][B][C][D] toolbar row with visual indication of the active slot. Left-click recalls a filled slot; right-click opens a context menu to save or clear any slot; Ctrl/Cmd+1–4 recalls.

### 1.7 The Spectrum Analyzer Agent (`spectrum_analyzer.cpp`)
The frequency-domain visualization agent.
* **Role:** Performs real-time frequency analysis of the audio signal and renders a visual spectrum display in the GUI.
* **Responsibilities:** Accepting audio data from the lock-free SPSC queue, computing frequency bins, and rendering the spectrum graph.

### 1.8 The Recorder Agent (`recorder.cpp`)
The WAV recording agent.
* **Role:** Captures processed audio output and writes it to WAV files on disk.
* **Responsibilities:** Managing recording state (start/stop), writing WAV headers, buffering audio data, and flushing to disk.

### 1.9 The MIDI Manager Agent (`midi_manager.h/.cpp`, `gui_midi.h/.cpp`)
The MIDI CC mapping and learn agent.
* **Role:** Receives MIDI Control Change messages from hardware controllers via RtMidi (callback thread) and routes them to effect parameters, bypass toggles, or master gains via the GUI/main thread.
* **Responsibilities:** Opening/closing MIDI input ports; maintaining a table of CC-to-parameter mappings (identified by effect name + param name for reorder stability); receiving CC events from RtMidi callbacks and pushing them into a lock-free SPSC queue (`midi_queue_`); providing a MIDI Learn mode where the next incoming CC is automatically bound to a user-selected knob; polling and draining the MIDI queue each GUI frame (via `poll()`) to process events and apply parameter changes; persisting mappings to `midi_config.json`; and rendering a settings window (port selector, mapping table) plus right-click "MIDI Learn" menu items on every pedal knob.

---

## 2. DSP Node Agents (The Pedal Board)

Each effect pedal in Amplitron acts as an independent DSP processing agent. They receive an input buffer, apply mathematical transformations based on their internal state (knob values), and yield an output buffer.

### 2.1 Dynamic Range Agents
* **`NoiseGate` (The Gatekeeper Agent):** Uses envelope following to monitor signal amplitude. If the signal falls below a user-defined threshold, it silences the output to eliminate background hum, strictly respecting configured attack and release times to prevent unnatural audio chopping.
* **`Compressor` (The Dynamics Agent):** Monitors amplitude and applies gain reduction when the signal exceeds a threshold. It relies on internal calculation of ratios, attack/release ballistics, and makeup gain to squash dynamic peaks and sustain notes artificially.

### 2.2 Saturation & Harmonics Agents
* **`Overdrive` (The Tube-Sim Agent):** Simulates the soft, asymmetric clipping of analog vacuum tubes. It utilizes mathematical waveshaping (such as `tanh()` or polynomial functions) to introduce warm, even-order harmonic distortion.
* **`Distortion` (The Hard-Clip Agent):** Applies aggressive, hard-clipping algorithms to the waveform, shearing off the peaks of the audio signal to create dense, heavy harmonic saturation suitable for high-gain modern genres.

### 2.3 Frequency Shaping Agents
* **`Equalizer` (The Tone-Shaping Agent):** A 3-band parametric EQ utilizing active Biquad filters. This agent splits the signal into Low Shelf, Peaking (Mid), and High Shelf bands, allowing precise amplification or attenuation of specific frequency domains.
* **`AmpSimulator` (The Preamp Agent):** A full preamp model simulator with 4 selectable amp models (Clean American / Fender Twin, British Crunch / Marshall JCM800, High Gain Modern / Mesa Rectifier, Jazz Warm / Roland JC-120). Each model packages a characteristic tone-stack EQ curve (3 biquad filters), saturation transfer function (soft/hard clipping blend with asymmetry), and dynamic response (envelope follower with power sag simulation). Exposed parameters include Model, Gain, Bass/Mid/Treble trim, and Level.
* **`CabinetSim` (The Speaker Cabinet Agent):** Replicates the acoustic properties of a guitar speaker cabinet. Supports two modes: (1) parametric EQ approach (3 biquad filters: LP rolloff, HP cut, resonance peak) as fallback, and (2) IR-based convolution using loaded `.wav` impulse response files via an optimized partitioned overlap-add FFT engine (kiss_fft). Parameters include Type (cabinet size), Bright (mic placement), and optional IR file loading with thread-safe atomic kernel swap.

### 2.4 Time & Spatial Agents
* **`Chorus` (The Modulation Agent):** Duplicates the incoming signal and applies a low-frequency oscillator (LFO) to modulate the delay time of the duplicate. By using linear interpolation for fractional delay reads, it creates a thick, multi-instrument illusion.
* **`Delay` (The Echo Agent):** A digital ring-buffer agent that captures the signal and repeats it at specific time intervals. It manages an internal feedback loop, feeding a percentage of the output back into its input to create decaying echoes.
* **`Reverb` (The Spatial Agent):** Utilizes Schroeder reverb architecture. This complex agent runs 4 parallel comb filters feeding into 2 series allpass filters to simulate the thousands of overlapping acoustic reflections found in physical spaces (rooms, halls, caves).

### 2.5 Modulation & Filter Agents
* **`WahPedal` (The Wah Agent):** A state-variable filter (Chamberlin SVF topology) wah effect with two operating modes. In **Manual** mode, the `Sweep` parameter controls the filter's centre frequency directly (heel-down = low, toe-down = high). In **Auto-wah** mode, an internal envelope follower tracks input amplitude and drives the sweep automatically according to `Sensitivity`, `Attack`, and `Release` parameters. One-pole smoothing is applied to both the sweep position and Q value to eliminate zipper noise on rapid knob moves. Uses `try_lock` for non-blocking parameter snapshots in the audio thread.
* **`Phaser` (The Sweep Agent):** Cascaded 1st-order all-pass filters (4, 6, 8, or 12 stages) modulated by an LFO. Blends the all-pass output with the dry signal to create classic phaser sweep effects. Supports stereo operation with 180° out-of-phase LFO modulation on the right channel for a wide, spatial sweep.
* **`Flanger` (The Comb Filter Agent):** Short modulated delay line (0.1–15ms) mixed with the dry signal. An LFO sweeps the delay time, and feedback through the delay line creates the characteristic comb filter sweep sound. Stereo operation uses 180° out-of-phase LFO for wide stereo flanging.

### 2.6 Pitch & Octave Shift Agents
* **`Octaver` (The Frequency Divider Agent):** Monophonic octave generator producing sub-octave (Oct-1) and upper-octave (Oct+1) signals blended with the dry input. Oct-1 uses a zero-crossing flip-flop divider producing a square wave at half the input frequency, shaped by the input envelope for warm, organ-like tones. Oct+1 uses full-wave rectification (|x|) to double the fundamental frequency, followed by DC removal and envelope shaping. References: Boss OC-2, EHX Octave Multiplexer.
* **`PitchShifter` (The Granular Agent):** Pitch shifting by ±12 semitones using a dual-tap granular overlap-add algorithm. Two read pointers scan a circular buffer at rates determined by the pitch ratio, with a raised-cosine (Hann) window crossfade between the two taps to hide grain boundary discontinuities. Controls include Shift (semitones), Fine (cents), and Mix.

### 2.7 Utility Agents
* **`TunerPedal` (The Pitch Detection Agent):** A chromatic tuner using the YIN pitch detection algorithm. Operates on a 4096-sample circular buffer (~85ms window at 48kHz), providing accurate fundamental frequency detection down to E2 (82.41Hz). Reports detected note name, octave, cent offset, and signal presence via atomic variables for thread-safe GUI display. Updates at ~15Hz to balance responsiveness and CPU usage.

---

## 3. Agent Communication & Concurrency Protocol

Because the UI Agent and the DSP Agents operate on entirely different threads (with vastly different priority levels), they must communicate carefully to avoid "Dropouts" (audio clicking/stuttering).

* **The `try_lock` + Shadow-Chain Paradigm:** The Audio Engine maintains an audio-thread-private shadow copy of the effect chain (`audio_shadow_effects_` / `audio_shadow_tuner_`). Each callback it attempts a non-blocking `try_lock` on `effect_mutex_`. If acquired it drains the SPSC command queue (applying pending parameter updates) and refreshes the shadow from `effects_`. If contended (GUI is mid-structural-mutation), it falls through and processes with the previous shadow — at most one callback behind, which is imperceptible. This eliminates the dry-pass glitch that previously occurred when skipping effect processing entirely on a failed `try_lock`.
* **MIDI Callback → GUI Handoff:** The MIDI Manager maintains a lock-free SPSC queue (`midi_queue_`) between the RtMidi callback thread (producer) and the GUI thread (consumer). RtMidi callbacks push `MidiEvent` objects into the queue without blocking. The GUI thread drains the queue each frame via `MidiManager::poll(AudioEngine&)`, which routes CC values to effect parameters, bypass toggles, or master gains through the existing `engine.push_param_change()` path. Mappings are persisted to `midi_config.json` so they survive session restarts.
* **Parameter Smoothing:** DSP Agents utilize one-pole filters internally on their parameter inputs. If the UI Agent jumps a parameter from `0.1` to `0.9` instantly, the DSP Agent interpolates the value over several samples to prevent audible "zipper" noise or clicking.

---

## 4. Testing Protocol

At the end of any coding task (new feature, bug fix, refactor), run the full test suite before considering the task complete.

```bash
cmake --build build --target amplitron-tests && ./build/amplitron-tests
```

* All tests must pass (0 failed) before the task is done.
* If tests fail, fix the root cause — do not skip or comment out tests.
* If you add a new DSP effect or system agent, add corresponding tests in `tests/`.

---

## Adding New External Libraries

When a new system library dependency is added (e.g. via `brew install` / `apt-get` / MSYS2 pacboy), **all three CI platform jobs must be updated in `.github/workflows/ci.yml`**. Failing to do so causes linker errors only on the CI runner, even if the local build works.

### Checklist for every new external library

1. **Linux** (`test-linux` job): add the package to the `apt-get install` line (e.g. `librtmidi-dev`).
2. **macOS** (`test-macos` job):
   - Add the package to the `brew install` line.
   - Add explicit `-DFOO_INCLUDE_DIRS` and `-DFOO_LIBRARIES` flags to the `cmake` invocation in the Build step, using `$HOMEBREW_PREFIX` paths. This is required because `pkg_check_modules` on macOS sets the library name only (`LIBRARIES=foo`) without adding the Homebrew lib directory to the linker search path. PortAudio and SDL2 use this same pattern as reference.
3. **Windows** (`test-windows` job): add `foo:p` to the `pacboy` package list. If the library ships a DLL, also copy `libfoo*.dll` in the `Collect Binaries` step.

### CMakeLists.txt pattern for new external libraries

```cmake
# Foo library
if(NOT FOO_LIBRARIES)          # allow CI to override via -D flags
    if(PkgConfig_FOUND)
        pkg_check_modules(FOO foo)
    endif()
    if(NOT FOO_FOUND)
        find_path(FOO_INCLUDE_DIRS foo/Foo.h PATHS ...)
        find_library(FOO_LIBRARIES NAMES foo PATHS ...)
    endif()
endif()
if(FOO_LIBRARY_DIRS)           # add search dir when pkg-config resolved the name
    link_directories(${FOO_LIBRARY_DIRS})
endif()
```

Then add `${FOO_INCLUDE_DIRS}` to `target_include_directories` and `${FOO_LIBRARIES}` to `target_link_libraries` for both `Amplitron` and `amplitron-tests`.

---

## Updating `CLAUDE.md`

At the end of any task that meaningfully changes the system architecture, update this file to keep it accurate. Follow these rules:

### When to update
- A new DSP effect agent is added or removed — update Section 2 and the agent count in the footer.
- A system-level agent (`audio_engine`, `gui_manager`, `pedal_board`, etc.) has its responsibilities materially changed — update its entry in Section 1.
- A new inter-thread communication pattern or concurrency mechanism is introduced — update Section 3.
- The sample rate range, buffer size range, or safety clamping behaviour changes — update Section 1.1.
- Do **not** update for purely internal refactors (renames, code style changes) that leave the observable architecture unchanged.

### What to update
- **Agent descriptions:** Keep the one-line role and bullet responsibilities accurate. Do not pad with implementation details that belong in code comments.
- **Footer agent counts:** The line showing DSP effects and system agents must reflect the actual current counts after every structural addition or removal.
- **Section headings:** If a new category of DSP agent is introduced (e.g. a new subsection under Section 2), add it with the same format as existing subsections.

### How to update
1. Read the current `CLAUDE.md` fully before editing.
2. Make the minimal accurate change — do not rewrite sections that are still correct.
3. Keep the agent description style consistent: bold name, parenthetical role label, then brief prose + bullet responsibilities.
4. Do not add implementation details (algorithm internals, variable names) unless they are architecturally significant (e.g. the YIN algorithm in TunerPedal is worth naming; an internal loop counter is not).


---
**Maintained by:** [@sudip-mondal-2002](https://github.com/sudip-mondal-2002)
**Architecture Reference** — 16 DSP effects, 9 system agents

## graphify

This project has a graphify knowledge graph at graphify-out/.

Rules:
- Before answering architecture or codebase questions, read graphify-out/GRAPH_REPORT.md for god nodes and community structure
- If graphify-out/wiki/index.md exists, navigate it instead of reading raw files
- After modifying code files in this session, run `python3 -c "from graphify.watch import _rebuild_code; from pathlib import Path; _rebuild_code(Path('.'))"` to keep the graph current
