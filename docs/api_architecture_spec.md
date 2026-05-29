# Amplitron System Architecture & API Specification

This document provides a highly technical, deep-dive specification of the **Amplitron Guitar Amp Simulator** core systems, thread models, real-time concurrency guidelines, and API lifecycles.

---

## 1. Concurrency Model & Lock-Free Thread Interaction

Amplitron operates on a strict multi-threaded model to achieve ultra-low latency (~1.3ms) and glitch-free audio rendering. The architecture divides operations into three primary boundaries:

```
 ┌──────────────────────┐      Lock-Free Queue      ┌──────────────────────┐
 │     GUI Thread       ├──────────────────────────>│     Audio Thread     │
 │ (ImGui / SDL Window) │<──────────────────────────┤ (DSP Block Callback) │
 └──────────────────────┘      Atomic Variable      └──────────────────────┘
```

### 1.1 The Real-Time Audio Thread (DSP)
* **Design Restriction:** Strict real-time safety constraints. The audio thread **MUST NEVER**:
  * Allocate or free memory on the heap (e.g., calling `new`, `malloc`, or resizing standard library vectors).
  * Invoke standard blocking mutexes (`std::mutex::lock`), which trigger OS context switching.
  * Execute synchronous file or console I/O (`std::cout`, `printf`, `std::ofstream`).
* **Bypass & Mutation Protocol:** Uses the **Shadow State Pattern**. A background copy (shadow) of the active signal chain is prepared on the GUI/Main thread. Once configured, a lock-free pointer exchange swaps the active DSP topology on the audio thread.

### 1.2 Lock-Free Queue Communications
Data handoffs (such as incoming MIDI events or real-time level analyzer telemetry) utilize Single-Producer Single-Consumer lock-free ring buffers (`SPSCQueue`):
* **MIDI Learning & Mapping:** Captured by RtMidi threads, mapped to CC values, and written to `midi_queue_`.
* **Telemetry Output:** Tuner cents/frequency detection and spectrum analysis arrays are written using atomic pointers to prevent UI tearing without thread locks.

---

## 2. DSP Node Architecture & The Pedal Board Lifecycle

Every processing node (pedal effect) inherits from the base `Effect` abstract class (`src/audio/effects/effect.h`):

```cpp
namespace Amplitron {

class Effect {
public:
    virtual ~Effect() = default;
    
    // Core DSP Operations
    virtual void process(float* buffer, int num_samples) = 0;
    virtual void process_stereo(float* left, float* right, int num_samples) = 0;
    virtual void reset() = 0;
    
    // Lifecycle Hooks
    virtual void set_sample_rate(int sample_rate) { sample_rate_ = sample_rate; }
    virtual void set_transport_state(float bpm) { (void)bpm; }
    
    // Parameters
    std::vector<Parameter>& params() { return params_; }
    bool is_enabled() const { return enabled_; }
    void set_enabled(bool enabled) { enabled_ = enabled; }
    float get_mix() const { return mix_; }
    void set_mix(float mix) { mix_ = mix; }

protected:
    int sample_rate_ = 44100;
    bool enabled_ = true;
    float mix_ = 1.0f;
    std::vector<Parameter> params_;
};

} // namespace Amplitron
```

### 2.1 The DSP Pipeline Execution
1. **Initialize Phase:** `set_sample_rate()` is called, causing internal structures (like delay buffers, LFO phases, and filter coefficients) to resize or recompute matching the hardware's sampling frequency.
2. **Execution Phase:** In the real-time callback block, the audio engine calls `process` (mono) or `process_stereo` sequentially for each active Node in the `AudioGraph`.
3. **Bypass Phase:** If `enabled_` is false, nodes return immediately, passing inputs directly to outputs without modification.

---

## 3. Preset Schema & Versioning Migrations

Preset configurations are stored as structured JSON payloads. 

### 3.1 Preset File Schema
```json
{
  "format_version": 2,
  "name": "Heavy Metal Rhythm",
  "description": "JCM800 preamp with dynamic noise gate and soft overdrive boost",
  "input_gain": 0.75,
  "output_gain": 0.8,
  "routing": "graph",
  "nodes": [
    {
      "id": "n1",
      "type": "NoiseGate",
      "enabled": true,
      "mix": 1.0,
      "params": [
        ["Threshold", -60.0],
        ["Release", 150.0]
      ]
    }
  ],
  "links": [
    {
      "src_pin": "n1.out0",
      "dst_pin": "n2.in0"
    }
  ]
}
```

### 3.2 Automated Migrations Hook
To ensure backwards compatibility with legacy configurations, `PresetManager::apply_migrations` parses version headers near the root:
* **Version 1 (Legacy):** Linear 1D array of effects. Dynamically migrated to Version 2 by creating standard routing nodes and connecting them sequentially.
* **Version 2 (Current):** Directional Acyclic Graph (DAG) routing schema supporting arbitrary parallel chains, splitters, and mixers.

---

## 4. UI Rendering System (Atomic Component Paradigm)

The graphical interface is built using ImGui rendering layouts decoupled from active audio states:
1. **Model-View Separation:** GUI elements (like rotary `KnobComponent` dials) are stateless renderers. They receive active configuration properties via structural Props.
2. **Event Lambdas:** Changes to knobs, sliders, and buttons do not directly mutate active state. Instead, they fire interactive callbacks (`on_value_changed`, `on_midi_learn`) that request updates safely on the application coordinator.
