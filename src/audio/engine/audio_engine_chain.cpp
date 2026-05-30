#include "audio/engine/audio_engine.h"
#include <algorithm>

namespace Amplitron {

void AudioEngine::sync_graph_with_dummy_effects(bool reset_graph) {
    {
        std::lock_guard<std::mutex> lock(effect_mutex_);
        if (reset_graph) {
            main_graph_ = AudioGraph();
        }
        
        if (main_graph_.get_nodes().empty()) {
            // 1. INITIAL SETUP: Reset the main graph model completely and auto-wire
            main_graph_ = AudioGraph();
            
            float cursor_x = 40.0f;
            float cursor_y = 60.0f;
            
            int input_node_id = main_graph_.add_node("Input", NodeRoutingType::StandardEffect, nullptr);
            main_graph_.set_node_as_input(input_node_id, true);
            main_graph_.set_node_position(input_node_id, cursor_x, cursor_y);
            cursor_x += 160.0f; // Input node is narrower
            
            int prev_output_pin = main_graph_.get_nodes().back().output_pin_ids.empty() ? -1 : main_graph_.get_nodes().back().output_pin_ids[0];
            
            // Loop through the linear pedals and wire them back-to-back in the DAG
            for (auto& fx : dummy_effects_) {
                fx->set_sample_rate(sample_rate_);
                fx->reset();
                int node_id = main_graph_.add_node(fx->name(), NodeRoutingType::StandardEffect, fx);
                main_graph_.set_node_position(node_id, cursor_x, cursor_y);
                cursor_x += 230.0f; // Standard pedals width + comfortable gap
                
                // Output routing will be handled dynamically at the end
                
                const auto& nodes = main_graph_.get_nodes();
                if (nodes.empty()) continue;
                const auto& current_node = nodes.back();
                // Connect the previous pedal's output pin to this pedal's input pin
                if (prev_output_pin != -1 && !current_node.input_pin_ids.empty()) {
                    main_graph_.add_link(prev_output_pin, current_node.input_pin_ids[0]);
                }
                
                if (!current_node.output_pin_ids.empty()) {
                    prev_output_pin = current_node.output_pin_ids[0];
                }
            }
            
            // Mark the last node in the linear chain as the Output so sound reaches the speakers
            if (!main_graph_.get_nodes().empty()) {
                main_graph_.set_node_as_output(main_graph_.get_nodes().back().id, true);
            }
        } else {
            // 2. MODULAR MODE: Remove standard nodes that are no longer in dummy_effects_
            std::vector<int> nodes_to_remove;
            for (const auto& node : main_graph_.get_nodes()) {
                if (node.routing_type == NodeRoutingType::StandardEffect && node.pedal != nullptr) {
                    auto it = std::find(dummy_effects_.begin(), dummy_effects_.end(), node.pedal);
                    if (it == dummy_effects_.end()) {
                        nodes_to_remove.push_back(node.id);
                    }
                }
            }
            for (int nid : nodes_to_remove) {
                main_graph_.remove_node(nid);
            }
            
            // Add standard nodes for effects in dummy_effects_ that are not yet in the graph
            for (int i = 0; i < static_cast<int>(dummy_effects_.size()); ++i) {
                auto& fx = dummy_effects_[i];
                // Find if a node already exists for this effect
                bool exists = false;
                for (const auto& node : main_graph_.get_nodes()) {
                    if (node.pedal == fx) {
                        exists = true;
                        break;
                    }
                }
                
                if (!exists) {
                    fx->set_sample_rate(sample_rate_);
                    fx->reset();
                    int node_id = main_graph_.add_node(fx->name(), NodeRoutingType::StandardEffect, fx);
                    if (std::string(fx->name()) == "Amp Sim") {
                        main_graph_.set_node_as_output(node_id, true);
                    }
                }
            }
        }
    }
    
    // 3. Compile the topology plan and push it to the hot audio thread safely
    commit_graph_changes();
}

void AudioEngine::add_effect(std::shared_ptr<Effect> fx) {
    dummy_effects_.push_back(fx);
    sync_graph_with_dummy_effects();
}

void AudioEngine::insert_effect(int index, std::shared_ptr<Effect> fx) {
    if (index >= 0 && index <= static_cast<int>(dummy_effects_.size())) {
        dummy_effects_.insert(dummy_effects_.begin() + index, fx);
        sync_graph_with_dummy_effects();
    }
}

void AudioEngine::remove_effect(int index) {
    if (index >= 0 && index < static_cast<int>(dummy_effects_.size())) {
        dummy_effects_.erase(dummy_effects_.begin() + index);
        sync_graph_with_dummy_effects();
    }
}

void AudioEngine::clear_effects() {
    dummy_effects_.clear();
    sync_graph_with_dummy_effects(true);
}

void AudioEngine::move_effect(int from, int to) {
    int size = static_cast<int>(dummy_effects_.size());
    if (from < 0 || from >= size || to < 0 || to >= size) {
        return;
    }
    
    if (from == to) return;

    auto fx = dummy_effects_[from];
    dummy_effects_.erase(dummy_effects_.begin() + from);
    dummy_effects_.insert(dummy_effects_.begin() + to, fx);
    
    sync_graph_with_dummy_effects();
}

void AudioEngine::restore_effects_state(std::vector<std::shared_ptr<Effect>> state) {
    dummy_effects_ = state;
    sync_graph_with_dummy_effects();
}

void AudioEngine::set_tuner_tap(std::shared_ptr<Effect> tap) {
    std::lock_guard<std::mutex> lock(effect_mutex_);
    tuner_tap_ = std::move(tap);
    if (tuner_tap_) {
        tuner_tap_->set_sample_rate(sample_rate_);
        tuner_tap_->reset();
    }
    topology_dirty_.store(true, std::memory_order_release);
}

void AudioEngine::clear_tuner_tap() {
    std::lock_guard<std::mutex> lock(effect_mutex_);
    tuner_tap_.reset();
    topology_dirty_.store(true, std::memory_order_release);
}

bool AudioEngine::has_tuner_tap() const {
    return tuner_tap_ != nullptr;
}

} // namespace Amplitron
