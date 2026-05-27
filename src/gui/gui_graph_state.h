#pragma once

#include <imgui.h>
#include <unordered_map>
#include <vector>

namespace Amplitron {

struct NodeLayoutState {
    ImVec2 position;
    bool is_dragging = false;
};

class GuiGraphState {
public:
    static GuiGraphState& get_instance() {
        static GuiGraphState instance;
        return instance;
    }

    // Canvas panning and zoom/grid configurations
    ImVec2 scrolling = ImVec2(0.0f, 0.0f);
    ImVec2 target_scrolling = ImVec2(0.0f, 0.0f);
    bool show_grid = true;
    bool is_fullscreen = false;
    bool hand_tool_active = false;
    float zoom = 1.0f;
    float target_zoom = 1.0f;
    float dpi_scale = 1.0f;
    ImVec2 last_canvas_pos = ImVec2(0.0f, 0.0f);
    bool canvas_hovered = false;

    // Node positioning registry mapped by Node ID
    std::unordered_map<int, NodeLayoutState> node_positions;

    // State tracking for wire connections currently being dragged
    int active_src_pin_id = -1;
    ImVec2 active_src_pin_pos = ImVec2(0.0f, 0.0f);

    void set_default_position_if_missing(int node_id, float default_x, float default_y) {
        if (node_positions.find(node_id) == node_positions.end()) {
            node_positions[node_id] = { ImVec2(default_x, default_y), false };
        }
    }
};

} // namespace Amplitron