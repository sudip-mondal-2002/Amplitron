# DSP Performance Profiler

The DSP Performance Profiler provides real-time visibility into Amplitron's audio processing health. It helps users and developers understand whether the audio callback is staying within its real-time processing budget and which DSP modules are consuming the most processing time.

## What it tracks

- Sample rate and buffer size
- Estimated audio latency in milliseconds
- Audio callback budget in microseconds
- Last callback processing time
- CPU load as a percentage of the real-time callback budget
- Callback deadline misses
- Stream underrun and overrun counters
- Per-DSP-module processing time
- Per-module average, peak, and budget percentage
- Latency and CPU history graphs

## User interface

Open the profiler from:

```text
Utilities -> DSP Performance Profiler
```

The panel shows a real-time overview of audio callback health, processing history, and per-node DSP timing.

## Warning thresholds

The profiler reports:

- `OK` when the callback is comfortably within budget
- `WARNING` when CPU load reaches 80% of the real-time callback budget
- `CRITICAL` when CPU load reaches or exceeds 100%
- `OVERLOADED` for DSP modules consuming 25% or more of the callback budget

Warnings are displayed as explicit text and table status labels, not color alone.

## Real-time safety

The profiler is designed for use from the audio callback path.

The audio-thread instrumentation avoids:

- heap allocation
- file I/O
- logging
- mutex locking
- command queue changes
- graph topology changes

Callback and module metrics are stored using atomics and fixed-size history buffers.

## Backend stream health

The profiler records stream health events from supported backends:

- PortAudio callback status flags for input/output underflow and overflow
- SDL capture queue monitoring for underrun and overrun conditions

These counters help identify glitches caused by audio-device starvation, delayed capture input, or overloaded processing.

## Developer notes

The profiler is implemented in:

```text
src/audio/engine/dsp_performance_profiler.h
src/audio/engine/dsp_performance_profiler.cpp
src/gui/views/gui_dsp_profiler.h
src/gui/views/gui_dsp_profiler.cpp
```

Main integration points:

```text
src/audio/engine/audio_engine_process.cpp
src/audio/engine/audio_graph_executor.cpp
src/audio/backend/portaudio_backend.cpp
src/audio/backend/sdl_backend.cpp
src/gui/gui_manager_menu.cpp
src/gui/gui_manager_frame.cpp
```

## Validation

Recommended local validation:

```bash
cmake --build build --target Amplitron --parallel
```

Then run the application and open:

```text
Utilities -> DSP Performance Profiler
```

Confirm that the panel displays latency, CPU load, history graphs, per-module timings, and stream health counters.
