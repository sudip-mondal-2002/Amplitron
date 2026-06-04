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
        for (size_t pin_idx = 0; pin_idx < it->input_pin_ids.size(); ++pin_idx) {
            int in_pin = it->input_pin_ids[pin_idx];
            float pin_gain = 1.0f;
            if (pin_idx < it->input_gains.size()) {
                pin_gain = it->input_gains[pin_idx];
            }
            for (const auto& link : links) {
                if (link.dest_pin_id == in_pin) {
                    int src_node_id = graph.get_node_from_pin(link.source_pin_id);
                    if (src_node_id != -1 && node_to_buffer.count(src_node_id)) {
                        step.input_sources.push_back({ node_to_buffer[src_node_id], pin_gain, static_cast<int>(pin_idx) });
                    }
                }
            }
        }

        if (it->routing_type == NodeRoutingType::StandardEffect && it->pedal) {
            step.processor = std::make_unique<StandardEffectProcessor>(it->pedal);
        } else {
            step.processor = std::make_unique<PassthroughProcessor>();
        }

        execution_plan_.push_back(std::move(step));
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
        std::memset(output, 0, static_cast<size_t>(num_samples) * sizeof(float));
        return;
    }
    if (execution_plan_.empty()) {
        std::memset(output, 0, num_samples * sizeof(float));
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
                if (src.gain == 1.0f) {
                    for (int i = 0; i < num_samples; ++i) {
                        node_input[i] += src_buf[i];
                    }
                } else {
                    for (int i = 0; i < num_samples; ++i) {
                        node_input[i] += src_buf[i] * src.gain;
                    }
                }
            }
        }

        float* node_output = buffer_pool_[step.buffer_index].data();

        if (step.processor) {
            step.processor->process(node_input, node_output, num_samples);
        } else {
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

void AudioGraphExecutor::update_mixer_gain(int node_id, int pin_index, float gain) {
    for (auto& step : execution_plan_) {
        if (step.node_id == node_id && (step.type == NodeRoutingType::Mixer || step.type == NodeRoutingType::MergeSum)) {
            for (auto& src : step.input_sources) {
                if (src.pin_index == pin_index) {
                    src.gain = std::clamp(gain, 0.0f, 2.0f);
                    break;
                }
            }
            break;
        }
    }
}

} // namespace Amplitron