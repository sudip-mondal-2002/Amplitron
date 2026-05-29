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

    AddGraphNodeCommand(AudioEngine& engine, const std::string& name, EffectType type, std::shared_ptr<Effect> pedal, ImVec2 pos)
        : engine_(engine), name(name), type(type), pedal(pedal), position(pos) {}

    bool execute() override {
        if (node_id == -1) {
            node_id = engine_.graph().add_node(name, type, pedal);
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

struct RemoveGraphNodeCommand : public Command {
    AudioEngine& engine_;
    NodeId node_id;
    EffectType type;
    ImVec2 position;
    std::vector<GraphLink> severed_links; // cache for undo
    DSPNode cached_node;                  // full node data for exact restoration

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
        engine_.commit_graph_changes();
        return true;
    }

    void undo() override {
        engine_.graph().restore_node(cached_node);
        GuiGraphState::get_instance().node_positions[node_id] = { position, false, ImVec2(0, 0) };
        
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
        } else if (was_successful) {
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
            engine_.commit_graph_changes();
        }
    }

    const char* description() const override { return "Add Link"; }
};

struct RemoveGraphLinkCommand : public Command {
    AudioEngine& engine_;
    GraphLink link;

    RemoveGraphLinkCommand(AudioEngine& engine, const GraphLink& l)
        : engine_(engine), link(l) {}

    bool execute() override {
        bool success = engine_.graph().remove_link(link.id);
        if (success) {
            engine_.commit_graph_changes();
        }
        return success;
    }

    void undo() override {
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
