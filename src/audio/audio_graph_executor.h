#pragma once

#include "audio/audio_graph.h"
#include "audio/effect.h"
#include <vector>
#include <unordered_map>
#include <memory>

namespace Amplitron {

class AudioGraphExecutor {
public:
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

private:
    int sample_rate_ = 48000;
    int max_block_size_ = 512;
    int max_nodes_ = 32;

    struct InputSource {
        int buffer_index; // The pool index to read from
    };

    struct NodeExecutionStep {
        int node_id;
        int buffer_index; // The pool index this node writes to
        NodeRoutingType type;
        std::shared_ptr<Effect> pedal;
        std::vector<InputSource> input_sources; // Which buffers to sum together for the input
        bool is_graph_input = false;
        bool is_graph_output = false;
        bool is_sink = false;
    };

    std::vector<NodeExecutionStep> execution_plan_;
    
    bool any_explicit_input_ = false;
    int fallback_input_node_id_ = -1;

    // Pre-allocated memory for routing parallel signal streams: [node_capacity][max_block_size]
    std::vector<std::vector<float>> buffer_pool_;
    
    // A dedicated temporary buffer for summing multiple incoming signals
    std::vector<float> sum_buffer_;
};

} // namespace Amplitron