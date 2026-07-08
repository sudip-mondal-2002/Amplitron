#pragma once

#include <cstring>
#include <memory>
#include <unordered_map>
#include <vector>

#include "audio/effects/core/effect.h"
#include "audio/engine/audio_graph.h"

namespace Amplitron {

class INodeProcessor {
   public:
    virtual ~INodeProcessor() = default;
    virtual void process(const float* input, float* output, int num_samples) = 0;
};

class StandardEffectProcessor : public INodeProcessor {
   public:
    explicit StandardEffectProcessor(std::shared_ptr<Effect> pedal) : pedal_(std::move(pedal)) {}
    void process(const float* input, float* output, int num_samples) override {
        std::memcpy(output, input, static_cast<size_t>(num_samples) * sizeof(float));
        if (pedal_) {
            pedal_->process(output, num_samples);
        }
    }

   private:
    std::shared_ptr<Effect> pedal_;
};

class PassthroughProcessor : public INodeProcessor {
   public:
    void process(const float* input, float* output, int num_samples) override {
        std::memcpy(output, input, static_cast<size_t>(num_samples) * sizeof(float));
    }
};

class AudioGraphExecutor {
   public:
    friend class AudioEngine;

    // ── Public nested types ────────────────────────────────────────────────
    // Kept public so that white-box tests (and AudioEngine friend code) can
    // inspect the execution plan without requiring #define private public.
    struct InputSource {
        int buffer_index;  // The pool index to read from
        float gain = 1.0f;
        int pin_index = 0;
    };

    struct NodeExecutionStep {
        int node_id;
        int buffer_index;  // The pool index this node writes to
        NodeRoutingType type;
        std::shared_ptr<Effect> pedal;
        std::vector<InputSource> input_sources;     // Which buffers to sum together for the input
        std::unique_ptr<INodeProcessor> processor;  // Polymorphic node executor
        bool is_graph_input = false;
        bool is_graph_output = false;
        bool is_sink = false;
    };
    // ── End public nested types ────────────────────────────────────────────

    AudioGraphExecutor();
    ~AudioGraphExecutor() = default;

    // Allocate the memory pool ahead of time (Call this during engine initialization)
    void prepare(int sample_rate, int max_block_size, int max_nodes = 32);

    // Translates the AudioGraph into a flat, allocation-free execution array
    // Call this from the UI thread whenever connections change!
    void compile(const AudioGraph& graph);

    // Broadcast tempo/BPM down to all active nodes in the execution plan
    void update_transport_state(float bpm);

    // Hot-path processing (Strictly allocation-free and lock-free)
    // Adjust the pedal->process signature if your pedals process strictly in-place
    void process(const float* input, float* output, int num_samples);
    void update_mixer_gain(int node_id, int pin_index, float gain);

    std::shared_ptr<Effect> get_effect_by_node_id(int node_id) const {
        for (const auto& step : execution_plan_) {
            if (step.node_id == node_id) {
                return step.pedal;
            }
        }
        return nullptr;
    }

#ifdef AMPLITRON_TESTS
    /** @brief Direct read-only access to the execution plan for white-box testing. */
    const std::vector<NodeExecutionStep>& test_execution_plan() const { return execution_plan_; }
#endif

   private:
    int sample_rate_ = 48000;
    int max_block_size_ = 512;
    int max_nodes_ = 32;

    std::vector<NodeExecutionStep> execution_plan_;

    bool any_explicit_input_ = false;
    int fallback_input_node_id_ = -1;

    // Pre-allocated memory for routing parallel signal streams: [node_capacity][max_block_size]
    std::vector<std::vector<float>> buffer_pool_;

    // A dedicated temporary buffer for summing multiple incoming signals
    std::vector<float> sum_buffer_;
};

}  // namespace Amplitron
