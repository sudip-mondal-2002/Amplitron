#pragma once

#include "gui/commands/command_base.h"
#include "audio/engine/audio_engine.h"
#include "audio/engine/audio_graph.h"
#include "gui/state/gui_graph_state.h"

namespace Amplitron {

using NodeId = int;
using EffectType = NodeRoutingType;

struct AddGraphNodeCommand : public Command {
    AudioEngine& engine_;
    NodeId node_id = -1; // Assigned on first execute
    std::string name;
    EffectType type;
    std::shared_ptr<Effect> pedal;
    ImVec2 position;
    DSPNode cached_node; // To remember exactly what was added for redo

    int num_inputs = 0;

    AddGraphNodeCommand(AudioEngine& engine, const std::string& name, EffectType type, std::shared_ptr<Effect> pedal, ImVec2 pos, int num_inputs = 0)
        : engine_(engine), name(name), type(type), pedal(pedal), position(pos), num_inputs(num_inputs) {}

    bool execute() override {
        if (node_id == -1) {
            node_id = engine_.graph().add_node(name, type, pedal, num_inputs);
            auto* added_node = engine_.graph().find_node(node_id);
            if (added_node) cached_node = *added_node;
        } else {
            // Re-adding the previously deleted/undone node
            engine_.graph().restore_node(cached_node);
        }
        // Only write a fixed position when one was explicitly requested;
        // if position is (0,0) the auto-placement logic in render_signal_chain
        // will assign the correct cascading position on the next frame.
        if (position.x != 0.0f || position.y != 0.0f) {
            GuiGraphState::get_instance().node_positions[node_id] = { position, false, ImVec2(0, 0) };
        }
        engine_.commit_graph_changes();
        return true;
    }

    void undo() override {
        engine_.graph().remove_node(node_id);
        GuiGraphState::get_instance().node_positions.erase(node_id);
        engine_.commit_graph_changes();
    }

    const char* description() const override { return "Add Node"; }
};

struct RemovedPinInfo {
    int node_id;
    int pin_id;
    int index;
    float gain;
};

struct RemoveGraphNodeCommand : public Command {
    AudioEngine& engine_;
    NodeId node_id;
    EffectType type;
    ImVec2 position;
    std::vector<GraphLink> severed_links; // cache for undo
    DSPNode cached_node;                  // full node data for exact restoration
    std::vector<RemovedPinInfo> auto_removed_pins; // cache for dynamically removed pins on other nodes
    std::vector<RemovedPinInfo> auto_removed_out_pins;

    RemoveGraphNodeCommand(AudioEngine& engine, NodeId id, EffectType t, ImVec2 pos)
        : engine_(engine), node_id(id), type(t), position(pos) {}

    bool execute() override {
        auto* node_to_remove = engine_.graph().find_node(node_id);
        if (!node_to_remove) return false;
        
        cached_node = *node_to_remove;

        // Cache severed links before removal
        severed_links.clear();
        for (const auto& link : engine_.graph().get_links()) {
            if (std::find(cached_node.input_pin_ids.begin(), cached_node.input_pin_ids.end(), link.dest_pin_id) != cached_node.input_pin_ids.end() ||
                std::find(cached_node.output_pin_ids.begin(), cached_node.output_pin_ids.end(), link.source_pin_id) != cached_node.output_pin_ids.end()) {
                severed_links.push_back(link);
            }
        }

        engine_.graph().remove_node(node_id);
        GuiGraphState::get_instance().node_positions.erase(node_id);
        
        // Clean up empty pins on mixers that were affected by severed links
        for (const auto& link : severed_links) {
            int dest_node_id = engine_.graph().get_node_from_pin(link.dest_pin_id);
            if (dest_node_id != -1) {
                const DSPNode* n = engine_.graph().find_node(dest_node_id);
                if (n && (n->routing_type == NodeRoutingType::Mixer || n->routing_type == NodeRoutingType::MergeSum)) {
                    if (n->input_pin_ids.size() > 2) {
                        int idx = -1; float gain = 1.0f;
                        for (size_t i = 0; i < n->input_pin_ids.size(); ++i) {
                            if (n->input_pin_ids[i] == link.dest_pin_id) {
                                idx = i; gain = (i < n->input_gains.size()) ? n->input_gains[i] : 1.0f; break;
                            }
                        }
                        if (engine_.graph().remove_input_pin(dest_node_id, link.dest_pin_id)) {
                            auto_removed_pins.push_back({dest_node_id, link.dest_pin_id, idx, gain});
                        }
                    }
                }
            }

            int source_node_id = engine_.graph().get_node_from_pin(link.source_pin_id);
            if (source_node_id != -1) {
                const DSPNode* n = engine_.graph().find_node(source_node_id);
                if (n && n->routing_type == NodeRoutingType::Splitter) {
                    if (n->output_pin_ids.size() > 2) {
                        int idx = -1;
                        for (size_t i = 0; i < n->output_pin_ids.size(); ++i) {
                            if (n->output_pin_ids[i] == link.source_pin_id) {
                                idx = i; break;
                            }
                        }
                        if (engine_.graph().remove_output_pin(source_node_id, link.source_pin_id)) {
                            auto_removed_out_pins.push_back({source_node_id, link.source_pin_id, idx, 1.0f});
                        }
                    }
                }
            }
        }

        engine_.commit_graph_changes();
        return true;
    }

    void undo() override {
        engine_.graph().restore_node(cached_node);
        GuiGraphState::get_instance().node_positions[node_id] = { position, false, ImVec2(0, 0) };
        
        for (auto it = auto_removed_pins.rbegin(); it != auto_removed_pins.rend(); ++it) {
            engine_.graph().restore_input_pin(it->node_id, it->pin_id, it->index, it->gain);
        }
        for (auto it = auto_removed_out_pins.rbegin(); it != auto_removed_out_pins.rend(); ++it) {
            engine_.graph().restore_output_pin(it->node_id, it->pin_id, it->index);
        }
        for (const auto& link : severed_links) {
            engine_.graph().restore_link(link);
        }
        engine_.commit_graph_changes();
    }

    const char* description() const override { return "Remove Node"; }
};

struct AddGraphLinkCommand : public Command {
    AudioEngine& engine_;
    GraphLink link;
    bool was_successful = false;
    
    int auto_added_pin_node_id = -1;
    int auto_added_pin_id = -1;
    int auto_added_pin_index = -1;
    float auto_added_pin_gain = 1.0f;
    
    int auto_added_out_pin_node_id = -1;
    int auto_added_out_pin_id = -1;
    int auto_added_out_pin_index = -1;

    AddGraphLinkCommand(AudioEngine& engine, int src_pin, int dst_pin)
        : engine_(engine) {
            link.source_pin_id = src_pin;
            link.dest_pin_id = dst_pin;
            link.id = -1; // Unknown until execute
        }

    bool execute() override {
        if (link.id == -1) {
            bool already_exists = false;
            for (const auto& l : engine_.graph().get_links()) {
                if (l.source_pin_id == link.source_pin_id && l.dest_pin_id == link.dest_pin_id) {
                    already_exists = true;
                    break;
                }
            }
            if (already_exists) return false;

            link.id = engine_.graph().add_link(link.source_pin_id, link.dest_pin_id);
            was_successful = (link.id != -1);
            
            if (was_successful) {
                int dest_node_id = engine_.graph().get_node_from_pin(link.dest_pin_id);
                if (dest_node_id != -1) {
                    const DSPNode* node = engine_.graph().find_node(dest_node_id);
                    if (node && (node->routing_type == NodeRoutingType::Mixer || node->routing_type == NodeRoutingType::MergeSum)) {
                        size_t occupied_count = 0;
                        for (int p : node->input_pin_ids) {
                            for (const auto& l : engine_.graph().get_links()) {
                                if (l.dest_pin_id == p) {
                                    occupied_count++; break;
                                }
                            }
                        }
                        if (occupied_count == node->input_pin_ids.size()) {
                            if (engine_.graph().add_input_pin(dest_node_id)) {
                                const DSPNode* updated_node = engine_.graph().find_node(dest_node_id);
                                int new_pin = updated_node->input_pin_ids.back();
                                
                                auto_added_pin_node_id = dest_node_id;
                                auto_added_pin_id = new_pin;
                                auto_added_pin_index = updated_node->input_pin_ids.size() - 1;
                                auto_added_pin_gain = 1.0f;
                            }
                        }
                    }
                }

                int source_node_id = engine_.graph().get_node_from_pin(link.source_pin_id);
                if (source_node_id != -1) {
                    const DSPNode* node = engine_.graph().find_node(source_node_id);
                    if (node && node->routing_type == NodeRoutingType::Splitter) {
                        size_t occupied_count = 0;
                        for (int p : node->output_pin_ids) {
                            for (const auto& l : engine_.graph().get_links()) {
                                if (l.source_pin_id == p) {
                                    occupied_count++; break;
                                }
                            }
                        }
                        if (occupied_count == node->output_pin_ids.size()) {
                            if (engine_.graph().add_output_pin(source_node_id)) {
                                const DSPNode* updated_node = engine_.graph().find_node(source_node_id);
                                int new_pin = updated_node->output_pin_ids.back();
                                
                                auto_added_out_pin_node_id = source_node_id;
                                auto_added_out_pin_id = new_pin;
                                auto_added_out_pin_index = updated_node->output_pin_ids.size() - 1;
                            }
                        }
                    }
                }
            }
        } else if (was_successful) {
            if (auto_added_pin_node_id != -1) {
                engine_.graph().restore_input_pin(auto_added_pin_node_id, auto_added_pin_id, auto_added_pin_index, auto_added_pin_gain);
            }
            if (auto_added_out_pin_node_id != -1) {
                engine_.graph().restore_output_pin(auto_added_out_pin_node_id, auto_added_out_pin_id, auto_added_out_pin_index);
            }
            engine_.graph().restore_link(link);
        }
        if (was_successful) {
            engine_.commit_graph_changes();
        }
        return was_successful;
    }

    void undo() override {
        if (was_successful) {
            engine_.graph().remove_link(link.id);
            if (auto_added_pin_node_id != -1) {
                engine_.graph().remove_input_pin(auto_added_pin_node_id, auto_added_pin_id);
            }
            if (auto_added_out_pin_node_id != -1) {
                engine_.graph().remove_output_pin(auto_added_out_pin_node_id, auto_added_out_pin_id);
            }
            engine_.commit_graph_changes();
        }
    }

    const char* description() const override { return "Add Link"; }
};

struct RemoveGraphLinkCommand : public Command {
    AudioEngine& engine_;
    GraphLink link;
    
    int auto_removed_pin_node_id = -1;
    int auto_removed_pin_id = -1;
    int auto_removed_pin_index = -1;
    float auto_removed_pin_gain = 1.0f;
    
    int auto_removed_out_pin_node_id = -1;
    int auto_removed_out_pin_id = -1;
    int auto_removed_out_pin_index = -1;

    RemoveGraphLinkCommand(AudioEngine& engine, const GraphLink& l)
        : engine_(engine), link(l) {}

    bool execute() override {
        bool success = engine_.graph().remove_link(link.id);
        if (success) {
            int dest_node_id = engine_.graph().get_node_from_pin(link.dest_pin_id);
            if (dest_node_id != -1) {
                const DSPNode* node = engine_.graph().find_node(dest_node_id);
                if (node && (node->routing_type == NodeRoutingType::Mixer || node->routing_type == NodeRoutingType::MergeSum)) {
                    if (node->input_pin_ids.size() > 2) {
                        for (size_t i = 0; i < node->input_pin_ids.size(); ++i) {
                            if (node->input_pin_ids[i] == link.dest_pin_id) {
                                auto_removed_pin_index = i;
                                auto_removed_pin_gain = (i < node->input_gains.size()) ? node->input_gains[i] : 1.0f;
                                break;
                            }
                        }
                        if (engine_.graph().remove_input_pin(dest_node_id, link.dest_pin_id)) {
                            auto_removed_pin_node_id = dest_node_id;
                            auto_removed_pin_id = link.dest_pin_id;
                        }
                    }
                }
            }

            int source_node_id = engine_.graph().get_node_from_pin(link.source_pin_id);
            if (source_node_id != -1) {
                const DSPNode* node = engine_.graph().find_node(source_node_id);
                if (node && node->routing_type == NodeRoutingType::Splitter) {
                    if (node->output_pin_ids.size() > 2) {
                        for (size_t i = 0; i < node->output_pin_ids.size(); ++i) {
                            if (node->output_pin_ids[i] == link.source_pin_id) {
                                auto_removed_out_pin_index = i;
                                break;
                            }
                        }
                        if (engine_.graph().remove_output_pin(source_node_id, link.source_pin_id)) {
                            auto_removed_out_pin_node_id = source_node_id;
                            auto_removed_out_pin_id = link.source_pin_id;
                        }
                    }
                }
            }
            engine_.commit_graph_changes();
        }
        return success;
    }

    void undo() override {
        if (auto_removed_pin_node_id != -1) {
            engine_.graph().restore_input_pin(auto_removed_pin_node_id, auto_removed_pin_id, auto_removed_pin_index, auto_removed_pin_gain);
        }
        if (auto_removed_out_pin_node_id != -1) {
            engine_.graph().restore_output_pin(auto_removed_out_pin_node_id, auto_removed_out_pin_id, auto_removed_out_pin_index);
        }
        engine_.graph().restore_link(link);
        engine_.commit_graph_changes();
    }

    const char* description() const override { return "Remove Link"; }
};

struct MoveGraphNodeCommand : public Command {
    NodeId node_id;
    ImVec2 old_pos;
    ImVec2 new_pos;

    MoveGraphNodeCommand(NodeId id, ImVec2 old_pos, ImVec2 new_pos)
        : node_id(id), old_pos(old_pos), new_pos(new_pos) {}

    bool execute() override {
        auto& positions = GuiGraphState::get_instance().node_positions;
        auto it = positions.find(node_id);
        if (it == positions.end()) return false;
        
        if (it->second.position.x == new_pos.x && it->second.position.y == new_pos.y) {
            return false;
        }
        
        it->second.position = new_pos;
        return true;
    }

    void undo() override {
        auto& positions = GuiGraphState::get_instance().node_positions;
        if (positions.count(node_id)) {
            positions[node_id].position = old_pos;
        }
    }

    const char* description() const override { return "Move Node"; }
};

} // namespace Amplitron
