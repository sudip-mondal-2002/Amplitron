# Amplitron Processing Agents & System Architecture

This document outlines the **Agent Architecture** and C++ coding paradigm within the Amplitron Guitar Amp Simulator. In this high-performance real-time DSP (Digital Signal Processing) environment, an "Agent" is an autonomous, encapsulated component responsible for specific processing, coordination, or state management tasks.

These agents operate concurrently across high-priority audio threads, MIDI callback threads, and UI threads to achieve ultra-low latency (~1.3ms) and glitch-free, real-time performance.

---

## 1. System-Level Coordination Agents

These core manager agents oversee the lifecycle, thread boundaries, and hardware interactions of the Amplitron ecosystem.

### 1.1 The Audio Engine Agent (`src/audio/engine/audio_engine.cpp`)
The master coordinator of the system, running at the highest OS priority via a platform driver backend.
* **Role:** Manages the real-time audio callback loop, fetching inputs from raw mono float32 hardware and routing them through the dynamic signal chain.
* **Responsibilities:**
  * Auto-detecting input/output hardware and highlighted USB guitar cables.
  * Negotiating buffer sizes (32 to 512 samples) and enforcing sample rates (44.1kHz - 96kHz).
  * Executing sequential DSP block pipelines.
  * Enforcing safety limits (clamping output strictly to ±1.0) to prevent auditory and hardware damage.
  * Operating with a lock-free or `try_lock` mutex protocol to ensure the audio thread never blocks.

### 1.2 The GUI Manager Agent (`src/gui/gui_manager.cpp`)
The user-facing presentation agent built on SDL2 and Dear ImGui.
* **Role:** Orchestrates the visual drawing loop and translates human input (mouse/keyboard events) into thread-safe system commands.
* **Responsibilities:**
  * Initializing gl context, window buffers, and the ImGui render pipeline.
  * Dispatching frame paints to child views.
  * Running asynchronous tasks (such as looking up online updates).

### 1.3 The Pedal Board Agent (`src/gui/pedalboard/pedal_board.cpp`)
The signal chain topology coordinator.
* **Role:** Manages the registry, ordering, and structural modifications (add, delete, swap, reorder) of active effects in the signal chain.
* **Responsibilities:**
  * Relaying structural and bypass state mutations safely to the Audio Engine shadow.
  * Serializing/deserializing preset models in coordination with the Preset Manager.

### 1.4 The Preset Manager Agent (`src/presets/preset_manager.cpp`)
The persistence agent for signal chain configurations.
* **Role:** Translates active pedalboard configurations to and from structured JSON presets.
* **Responsibilities:**
  * Generating standardized JSON strings and directory profiles.
  * Parsing loaded files and instantiating individual DSP effects by name via the effect factory.

### 1.5 The Command History Agent (`src/gui/commands/command_history.cpp`)
The undo/redo transaction registry.
* **Role:** Implements the command pattern via double-stack queues to manage complete undo/redo capabilities.
* **Responsibilities:**
  * Storing parameter commits, pedal swaps, and snapshot recalls.
  * Enforcing limits and merging sequential parameter changes (coalescing drag motions within a 500ms threshold).

### 1.6 The Snapshot Manager Agent (`src/gui/state/snapshot_manager.h`, `src/gui/views/gui_snapshots.cpp`)
The session state bank agent.
* **Role:** Maintains in-memory banks of full pedalboard states for instantaneous, glitch-free recalls during live performances.
* **Responsibilities:**
  * Capturing complete chain structures and parameter snapshots into memory slots `[A][B][C][D]`.
  * Triggering instant active slot switches.

### 1.7 The Recorder Agent (`src/audio/recorder/recorder.cpp`)
The WAV recording and disk flusher.
* **Role:** Captures processed audio frames from the output stream and flushes them to disk asynchronously.
* **Responsibilities:**
  * Managing a thread-safe multi-state recorder buffer (recording, paused, idle).
  * Writing standard RIFF WAV headers and flushing audio samples to disk without blocking.

### 1.8 The MIDI Manager Agent (`src/midi/midi_manager.cpp`, `src/gui/views/gui_midi.cpp`)
The MIDI mapping and learning coordinator.
* **Role:** Captures MIDI Control Change (CC) messages from hardware controllers via RtMidi callback threads and routes them to parameters, bypass states, and gain sliders.
* **Responsibilities:**
  * Polling the non-blocking SPSC queue to map CC signals to active pedal parameters.
  * Handling the interactive MIDI Learn mode and mapping table persistence to `midi_config.json`.

---

## 2. DSP Node Agents (The Pedal Board)

Each pedal effect under `src/audio/effects/` operates as a monophonic DSP node. They inherit from the base `Effect` class, receiving float input buffers, performing sample-by-sample math equations based on user parameters, and returning output buffers.

### 2.1 Dynamic Range Agents
* **`NoiseGate`:** Envelope follower-driven noise reduction that silences feedback hum below thresholds using smoothed attack and release curves.
* **`Compressor`:** Dynamic range processor executing threshold-based gain reduction, compression ratios, soft-knees, and makeup gain.

### 2.2 Saturation & Harmonics Agents
* **`Overdrive`:** Asymmetric soft-clipper simulating warm tube distortion via waveshaping equations (e.g. `tanh()`).
* **`Distortion`:** High-gain hard-clipper executing aggressive waveshaping saturation for dense harmonic content.

### 2.3 Frequency Shaping Agents
* **`Equalizer`:** 3-band parametric EQ using 2nd-order Biquad filters (Low Shelf, Peaking Mid, High Shelf).
* **`AmpSimulator`:** Full preamp simulation providing Fender Twin, Marshall JCM800, Mesa Rectifier, and Roland JC-120 characteristics, including tone stacks, asymmetric gain stages, and power sag.
* **`CabinetSim`:** Cabinet impulse responder utilizing kiss_fft for partitioned overlap-add (OLA) convolution, with a parametric biquad filter fallback.

### 2.4 Time & Spatial Agents
* **`Chorus`:** Modulation effect utilizing a low-frequency oscillator (LFO) to sweep delay times, employing linear interpolation to read fractional ring-buffers.
* **`Delay`:** Feedback digital delay line featuring low/high damping filters.
* **`Reverb`:** Schroeder physical space simulator featuring 4 parallel comb filters feeding 2 serial allpass networks.

### 2.5 Modulation & Filter Agents
* **`WahPedal`:** State-variable filter (Chamberlin SVF) offering manual sweep and envelope-follower-driven auto-wah.
* **`Phaser`:** Cascaded all-pass filter networks modulated by an LFO, featuring stereo phase offsets.
* **`Flanger`:** Comb filter modulation sweep using ultra-short modulated delay lines (0.1ms to 15ms).

### 2.6 Pitch & Octave Shift Agents
* **`Octaver`:** Monophonic octave generator combining a zero-crossing flip-flop sub-octave divider (Boss OC-2 style) and a rectified double-frequency upper octave.
* **`PitchShifter`:** Granular dual-tap pitch converter using overlap-add and Hann window crossfading.

### 2.7 Utility Agents
* **`TunerPedal`:** Pitch detection engine using the YIN autocorrelation algorithm to identify fundamental frequencies (E2 to E6) and reporting chromatic offsets via atomic variables.

---

## 3. Atomic Visual Component Paradigm

To maintain visual consistency and decouple rendering concerns from functional logic, the GUI implements an **Atomic Visual Component Paradigm** in `src/gui/components/`.

```
                  ┌───────────────────────────────┐
                  │          PedalWidget          │ (Domain Orchestrator)
                  └──────┬─────────────────┬──────┘
                         │                 │
     ┌───────────────────▼───┐     ┌───────▼────────────────┐
     │     LedComponent      │     │     KnobComponent      │ (Atomic Renderers)
     │ (Stateless Draw/Glow) │     │  (Stateless Rotary)    │
     └───────────────────────┘     └────────────────────────┘
```

These components are **stateless and event-driven**:
1. **Stateless Drawing**: They do not own or mutate long-term domain state (e.g. effect parameter floats). Instead, they receive a read-only `Props` structure containing the current values, labels, and cosmetic flags.
2. **Callback-Driven Action**: To propagate changes (such as knob drags or switch clicks), the components trigger lambda callbacks passed inside the `Props` structure (`on_value_changed`, `on_midi_learn`, etc.).
3. **Encapsulated Math**: All calculations regarding mouse drag mathematics, fine adjustments (e.g., Shift + Drag for precision), right-click popups, and visual coordinate interpolation are completely encapsulated inside the component's source file, removing noise from the high-level orchestrators (`PedalWidget`).

### Core Visual Components
* **`KnobComponent` (`knob.h/.cpp`)**: Renders rotary dials, tooltips, right-click popups, and MIDI learn borders.
* **`FootswitchComponent` (`footswitch.h/.cpp`)**: Draws dynamic metallic switches with click animations.
* **`LedComponent` (`led.h/.cpp`)**: Draws glowing lights supporting pulsating glows, custom color hexes, and blink frequencies.
* **`ScreenComponent` (`screen.h/.cpp`)**: Renders specialized visual plots for the Tuner, Cabinet grid, Looper circle, and MultiBand crossovers.

---

## 4. Code Guidelines & Standards

Contributors must strictly adhere to the following C++17 coding design conventions:

### 4.1 Naming Conventions
* **Files**: Lowercase snake_case (e.g. `preset_manager.cpp`, `audio_engine.h`).
* **Classes / Structs**: PascalCase (e.g. `AudioEngine`, `KnobProps`).
* **Functions / Methods**: lowercase snake_case (e.g. `push_param_change()`, `remove_mapping()`).
* **Variables / Parameters**: lowercase snake_case (e.g. `effect_name`, `pi`).
* **Class Member Variables**: lowercase snake_case with a **trailing underscore** (e.g. `current_port_`, `midi_queue_`).
* **Global Constants / Macros**: Uppercase snake_case (e.g. `AMPLITRON_HEADLESS`, `KNOB_RADIUS`).

### 4.2 Concurrency Protocols
* **Audio Thread Safety**: Never allocate memory, perform disk/console I/O, or block on standard mutexes inside the audio callback loop.
* **Shadow Paradigm**: The audio engine must use `try_lock` to capture non-blocking copies of the pedalboard chain, falling back to a cached shadow copy if the main thread is mutating the active chain.
* **Handoffs**: Use lock-free SPSC queues (`SPSCQueue`) to communicate midi/audio events from hardware threads to the GUI thread.

---

## 5. Extending the Codebase

### 5.1 Adding a New DSP Effect Pedal
1. Inherit from `Effect` in `src/audio/effects/effect.h`.
2. Define its unique parameter indices, default limits, and titles inside your constructor.
3. Implement `process_sample(float in)` or `process_block(...)` to write its DSP equations.
4. Register the new effect type in `src/audio/effects/effect_factory.h` and the visual preset manager lists.
5. Create a corresponding test suite file under `tests/unit/` (e.g., `test_effects_my_pedal.cpp`) and verify using `amplitron-tests`.

### 5.2 Adding a New Visual Widget
1. Create a stateless static helper class under `src/gui/components/`.
2. Expose a single `static void render(const char* id, const MyProps& props, float zoom, ImVec2 center)` endpoint.
3. Keep all drawing routines encapsulated using Dear ImGui coordinates (`ImDrawList`), and trigger lambdas passed in `props` for all interactivity.

---

## 6. Testing & CI Pipeline

Always verify all targets before pushing commits:
```bash
# 1. Configure and Build the Test Target
cmake -B build && cmake --build build --target amplitron-tests

# 2. Run the Headless Test Suite (ensure 100% pass)
./build/amplitron-tests

# 3. Build the Production Executable
cmake --build build --target Amplitron
```

---
**Maintained by:** [@sudip-mondal-2002](https://github.com/sudip-mondal-2002)
**Architecture Reference** — 16 DSP effects, 9 system agents, 4 atomic visual widgets
