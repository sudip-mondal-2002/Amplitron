#include "audio/engine/audio_graph_executor.h"
#include <algorithm>
#include <cstring>

namespace Amplitron {

AudioGraphExecutor::AudioGraphExecutor() {}

void AudioGraphExecutor::prepare(int sample_rate, int max_block_size, int max_nodes) {
    sample_rate_ = sample_rate;
    max_block_size_ = max_block_size;
    max_nodes_ = max_nodes;

    // Pre-allocate the memory pool to guarantee no allocations on the audio thread
    buffer_pool_.resize(max_nodes_, std::vector<float>(max_block_size_, 0.0f));
    sum_buffer_.resize(max_block_size_, 0.0f);
    execution_plan_.reserve(max_nodes_);
}

void AudioGraphExecutor::compile(const AudioGraph& graph) {
    execution_plan_.clear();
    const auto& sorted_ids = graph.get_sorted_nodes();
    const auto& nodes = graph.get_nodes();
    const auto& links = graph.get_links();

    if (sorted_ids.empty() || sorted_ids.size() > (size_t)max_nodes_) return;

    // Map each Node ID to a dedicated row in the buffer pool
    std::unordered_map<int, int> node_to_buffer;
    for (size_t i = 0; i < sorted_ids.size(); ++i) {
        node_to_buffer[sorted_ids[i]] = static_cast<int>(i);
    }

    // Determine if there are any explicit inputs or outputs
    any_explicit_input_ = false;
    bool any_explicit_output = false;
    for (int node_id : sorted_ids) {
        auto it = std::find_if(nodes.begin(), nodes.end(), [&](const DSPNode& n){ return n.id == node_id; });
        if (it != nodes.end()) {
            if (it->is_graph_input) any_explicit_input_ = true;
            if (it->is_graph_output) any_explicit_output = true;
        }
    }

    fallback_input_node_id_ = -1;
    if (!any_explicit_input_ && !sorted_ids.empty()) {
        fallback_input_node_id_ = sorted_ids[0];
    }

    // Build the flat execution array
    for (int node_id : sorted_ids) {
        auto it = std::find_if(nodes.begin(), nodes.end(), [&](const DSPNode& n){ return n.id == node_id; });
        if (it == nodes.end() || !it->is_reachable) continue;

        NodeExecutionStep step;
        step.node_id = node_id;
        step.buffer_index = node_to_buffer[node_id];
        step.type = it->routing_type;
        step.pedal = it->pedal;
        step.is_graph_input = it->is_graph_input;
        step.is_graph_output = it->is_graph_output;

        if (any_explicit_output) {
            step.is_sink = it->is_graph_output;
        } else {
            // Check if this node has outgoing links
            bool has_outgoing = false;
            for (int out_pin : it->output_pin_ids) {
                for (const auto& link : links) {
                    if (link.source_pin_id == out_pin) {
                        has_outgoing = true;
                        break;
                    }
                }
                if (has_outgoing) break;
            }
            step.is_sink = !has_outgoing;
        }

        // Trace upstream connections to find which buffers to read from
        for (int in_pin : it->input_pin_ids) {
            for (const auto& link : links) {
                if (link.dest_pin_id == in_pin) {
                    int src_node_id = graph.get_node_from_pin(link.source_pin_id);
                    if (src_node_id != -1 && node_to_buffer.count(src_node_id)) {
                        step.input_sources.push_back({ node_to_buffer[src_node_id] });
                    }
                }
            }
        }
        execution_plan_.push_back(step);
    }
}

void AudioGraphExecutor::update_transport_state(float bpm) {
    for (auto& step : execution_plan_) {
        if (step.pedal) {
            step.pedal->set_transport_state(bpm);
        }
    }
}

void AudioGraphExecutor::process(const float* input, float* output, int num_samples) {
    if (num_samples > max_block_size_) {
        std::memcpy(output, input, static_cast<size_t>(max_block_size_) * sizeof(float));
        return;
    }
    if (execution_plan_.empty()) {
        std::memcpy(output, input, num_samples * sizeof(float));
        return;
    }

    for (const auto& step : execution_plan_) {
        float* node_input = sum_buffer_.data();

        bool is_input_source = false;
        if (any_explicit_input_) {
            is_input_source = step.is_graph_input;
        } else {
            is_input_source = (step.node_id == fallback_input_node_id_);
        }

        if (is_input_source) {
            // Feed the master guitar input
            std::memcpy(node_input, input, num_samples * sizeof(float));
        } else {
            // Summation: Additively mix all incoming paths
            std::memset(node_input, 0, num_samples * sizeof(float));
            for (const auto& src : step.input_sources) {
                const float* src_buf = buffer_pool_[src.buffer_index].data();
                for (int i = 0; i < num_samples; ++i) {
                    node_input[i] += src_buf[i];
                }
            }
        }

        float* node_output = buffer_pool_[step.buffer_index].data();

        if (step.type == NodeRoutingType::StandardEffect && step.pedal) {
            // FIX: In-place processing! 
            // 1. Copy the summed input data into our dedicated output buffer
            std::memcpy(node_output, node_input, num_samples * sizeof(float));
            
            // 2. Tell the pedal to process and overwrite that buffer directly
            step.pedal->process(node_output, num_samples); 
        } else {
            // Merge nodes or empty wrappers just pass the summed input directly downstream
            std::memcpy(node_output, node_input, num_samples * sizeof(float));
        }
    }

    // Accumulate/mix outputs from all explicit sink nodes into the final output buffer
    std::memset(output, 0, num_samples * sizeof(float));
    for (const auto& step : execution_plan_) {
        if (step.is_sink) {
            const float* sink_buf = buffer_pool_[step.buffer_index].data();
            for (int i = 0; i < num_samples; ++i) {
                output[i] += sink_buf[i];
            }
        }
    }
}

} // namespace Amplitron