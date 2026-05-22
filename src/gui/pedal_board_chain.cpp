#include "gui/pedal_board.h"
#include "gui/pedal_widget.h"
#include "gui/theme.h"
#include "gui/gui_graph_state.h"
#include "gui/command.h"
#include <imgui.h>
#include <unordered_map>
#include <cmath>

namespace Amplitron {

void PedalBoard::render_signal_chain() {
    auto& ui_state = GuiGraphState::get_instance();
    auto& audio_graph = engine_.graph(); 
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    ImVec2 canvas_end = ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y);

    ImGui::SetCursorScreenPos(canvas_pos);
    ImGuiButtonFlags btn_flags = ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle;
    if (ui_state.hand_tool_active) {
        btn_flags |= ImGuiButtonFlags_MouseButtonLeft;
    }
    
    ImGui::InvisibleButton("canvas_panning_hotspot", canvas_size, btn_flags);
    ImGui::SetItemAllowOverlap();
    
    if (ui_state.hand_tool_active && ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    
    if (ImGui::IsItemActive() && (ImGui::IsMouseDragging(ImGuiMouseButton_Right) || 
                                  ImGui::IsMouseDragging(ImGuiMouseButton_Middle) || 
                                  (ui_state.hand_tool_active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)))) {
        ui_state.scrolling.x += ImGui::GetIO().MouseDelta.x;
        ui_state.scrolling.y += ImGui::GetIO().MouseDelta.y;
    }

    // Zooming is now allowed in both fullscreen and normal modes
    if (ImGui::IsItemHovered()) {
        float scroll_y = ImGui::GetIO().MouseWheel;
        if (scroll_y != 0.0f) {
            float zoom_factor = (scroll_y > 0) ? 1.1f : (1.0f / 1.1f);
            ImVec2 mouse_pos = ImGui::GetMousePos();
            ImVec2 mouse_in_canvas = ImVec2(mouse_pos.x - canvas_pos.x, mouse_pos.y - canvas_pos.y);
            ui_state.scrolling.x = mouse_in_canvas.x - (mouse_in_canvas.x - ui_state.scrolling.x) * zoom_factor;
            ui_state.scrolling.y = mouse_in_canvas.y - (mouse_in_canvas.y - ui_state.scrolling.y) * zoom_factor;
            ui_state.zoom *= zoom_factor;
            if (ui_state.zoom < 0.2f) ui_state.zoom = 0.2f;
            if (ui_state.zoom > 5.0f) ui_state.zoom = 5.0f;
        }
    }

    // Draw fullscreen button at top right
    ImGui::SetCursorScreenPos(ImVec2(canvas_pos.x + canvas_size.x - 70, canvas_pos.y + 10));
    if (ImGui::Button(ui_state.is_fullscreen ? "Exit FS" : "Full Screen")) {
        ui_state.is_fullscreen = !ui_state.is_fullscreen;
        if (!ui_state.is_fullscreen) ui_state.zoom = 1.0f;
    }
    ImGui::SetItemAllowOverlap();

    if (ui_state.show_grid) {
        float GRID_SZ = 32.0f * ui_state.zoom;
        ImU32 GRID_COLOR = IM_COL32(36, 34, 30, 255);
        for (float x = std::fmod(ui_state.scrolling.x, GRID_SZ); x < canvas_size.x; x += GRID_SZ) {
            draw_list->AddLine(ImVec2(canvas_pos.x + x, canvas_pos.y), ImVec2(canvas_pos.x + x, canvas_end.y), GRID_COLOR);
        }
        for (float y = std::fmod(ui_state.scrolling.y, GRID_SZ); y < canvas_size.y; y += GRID_SZ) {
            draw_list->AddLine(ImVec2(canvas_pos.x, canvas_pos.y + y), ImVec2(canvas_end.x, canvas_pos.y + y), GRID_COLOR);
        }
    }

    draw_list->PushClipRect(canvas_pos, canvas_end, true);

    ImVec2 offset = ImVec2(canvas_pos.x + ui_state.scrolling.x, canvas_pos.y + ui_state.scrolling.y);
    std::unordered_map<int, ImVec2> pin_positions_cache;

    int node_to_delete = -1; // Safely track deletions outside the render loop

    // Prune stale nodes from the UI state if the backend graph was reset or rebuilt
    std::vector<int> stale_ids;
    for (auto& pair : ui_state.node_positions) {
        if (!audio_graph.find_node(pair.first)) {
            stale_ids.push_back(pair.first);
        }
    }
    for (int id : stale_ids) {
        ui_state.node_positions.erase(id);
    }

    // Give all new nodes a default position at the end of the chain without shifting existing nodes
    for (const auto& node : audio_graph.get_nodes()) {
        if (ui_state.node_positions.find(node.id) == ui_state.node_positions.end()) {
            float max_right = 40.0f;
            for (const auto& existing_node : audio_graph.get_nodes()) {
                auto pos_it = ui_state.node_positions.find(existing_node.id);
                if (pos_it != ui_state.node_positions.end()) {
                    float width = 110.0f;
                    if (existing_node.routing_type == NodeRoutingType::StandardEffect) {
                        if (existing_node.pedal && std::strcmp(existing_node.pedal->name(), "MultiBand Compressor") == 0) {
                            width = 190.0f * 2.2f;
                        } else {
                            width = 190.0f;
                        }
                    }
                    float right_edge = pos_it->second.position.x + width;
                    if (right_edge > max_right) {
                        max_right = right_edge;
                    }
                }
                float insert_x = ui_state.node_positions.empty() ? 40.0f : max_right + 80.0f;
                ui_state.node_positions[node.id] = { ImVec2(insert_x, 60.0f), false };
            }
        }
    }

    for (const auto& node : audio_graph.get_nodes()) {

        auto& node_layout = ui_state.node_positions[node.id];
        ImVec2 node_screen_pos = ImVec2(offset.x + node_layout.position.x * ui_state.zoom, offset.y + node_layout.position.y * ui_state.zoom);

        PedalWidget* target_widget = nullptr;
        if (node.routing_type == NodeRoutingType::StandardEffect) {
            for (auto& w : widgets_) {
                if (w->get_effect() == node.pedal) { target_widget = w.get(); break; }
            }
        }

        bool is_mb_comp = false;
        if (target_widget && std::strcmp(target_widget->get_effect()->name(), "MultiBand Compressor") == 0) {
            is_mb_comp = true;
        }
        float node_width = (target_widget ? (is_mb_comp ? 190.0f * 2.2f : 190.0f) : 110.0f) * ui_state.zoom;
        float node_height = (target_widget ? 360.0f : 70.0f) * ui_state.zoom;

        ImGui::PushID(node.id);


        if (target_widget) {
            ImGui::SetCursorScreenPos(node_screen_pos);
            ImGui::BeginGroup();
            ImGui::SetWindowFontScale(ui_state.zoom);
            target_widget->render(ui_state.zoom); 
            ImGui::SetWindowFontScale(1.0f);
            ImGui::EndGroup();

            ImGui::SetCursorScreenPos(node_screen_pos);
            ImGui::InvisibleButton("native_drag_handle", ImVec2(node_width - 25.0f * ui_state.zoom, 30.0f * ui_state.zoom));
            ImGui::SetItemAllowOverlap(); 
            if (!ui_state.hand_tool_active && ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                node_layout.position.x += ImGui::GetIO().MouseDelta.x / ui_state.zoom;
                node_layout.position.y += ImGui::GetIO().MouseDelta.y / ui_state.zoom;
            }
        } else {
            ImVec2 node_end = ImVec2(node_screen_pos.x + node_width, node_screen_pos.y + node_height);
            ImU32 bg_color = IM_COL32(50, 35, 60, 255);
            draw_list->AddRectFilled(node_screen_pos, node_end, bg_color, Theme::ROUNDING_MD * ui_state.zoom);
            draw_list->AddRect(node_screen_pos, node_end, IM_COL32(180, 140, 80, 180), Theme::ROUNDING_MD * ui_state.zoom, 0, 1.5f * ui_state.zoom);

            ImGui::SetCursorScreenPos(node_screen_pos);
            ImGui::InvisibleButton("util_drag_handle", ImVec2(node_width - 25.0f * ui_state.zoom, node_height));
            ImGui::SetItemAllowOverlap();
            if (!ui_state.hand_tool_active && ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                node_layout.position.x += ImGui::GetIO().MouseDelta.x / ui_state.zoom;
                node_layout.position.y += ImGui::GetIO().MouseDelta.y / ui_state.zoom;
            }
            ImVec2 text_pos = ImVec2(node_screen_pos.x + 12.0f * ui_state.zoom, node_screen_pos.y + 25.0f * ui_state.zoom);
            ImGui::SetWindowFontScale(ui_state.zoom);
            draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 255), node.name.c_str());
            ImGui::SetWindowFontScale(1.0f);
        }

        if (!node.is_reachable) {
            ImVec2 node_end = ImVec2(node_screen_pos.x + node_width, node_screen_pos.y + node_height);
            draw_list->AddRectFilled(node_screen_pos, node_end, IM_COL32(0, 0, 0, 180), Theme::ROUNDING_MD * ui_state.zoom);
            
            ImVec2 text_pos = ImVec2(node_screen_pos.x + 10.0f * ui_state.zoom, node_screen_pos.y + node_height - 25.0f * ui_state.zoom);
            ImGui::SetWindowFontScale(ui_state.zoom * 0.9f);
            draw_list->AddText(text_pos, IM_COL32(255, 60, 60, 255), "DISCONNECTED");
            ImGui::SetWindowFontScale(1.0f);
        }

        // ====================================================================
        // THE DELETION [X] BUTTON
        // ====================================================================
        bool is_amp = (node.name == "Amp Sim"); 
        bool is_input_node = (node.name == "Input");

        if (!is_amp && !is_input_node) {
            ImVec2 cross_pos = ImVec2(node_screen_pos.x + node_width - 24.0f * ui_state.zoom, node_screen_pos.y + 4.0f * ui_state.zoom);
            ImGui::SetCursorScreenPos(cross_pos);
            
            // Your exact color styling
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.1f, 0.1f, 0.8f));
            
            // Use SmallButton and exact string formatting
            std::string remove_label = "X##rm" + std::to_string(node.id);
            if (ImGui::SmallButton(remove_label.c_str())) {
                node_to_delete = node.id;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Remove %s from chain", node.name.c_str());
            }
            
            ImGui::PopStyleColor(2);
            ImGui::SetItemAllowOverlap();
        }

        // ====================================================================
        // FIX: THE WIRE DROP ZONE (Input Pins)
        // ====================================================================
        if (!is_input_node) {
            for (size_t idx = 0; idx < node.input_pin_ids.size(); ++idx) {
                int pin_id = node.input_pin_ids[idx];
                float pin_y = node_screen_pos.y + (node_height * (idx + 1.0f) / (node.input_pin_ids.size() + 1.0f));
                ImVec2 pin_pos(node_screen_pos.x - 2.0f * ui_state.zoom, pin_y); 
                pin_positions_cache[pin_id] = pin_pos;

                draw_list->AddCircleFilled(pin_pos, 5.0f * ui_state.zoom, IM_COL32(46, 204, 113, 255)); 
                draw_list->AddCircle(pin_pos, 6.5f * ui_state.zoom, IM_COL32(255, 255, 255, 200));

                ImGui::SetCursorScreenPos(ImVec2(pin_pos.x - 10.0f * ui_state.zoom, pin_pos.y - 10.0f * ui_state.zoom));
                ImGui::PushID(pin_id);
                ImGui::InvisibleButton("in_pin", ImVec2(20.0f * ui_state.zoom, 20.0f * ui_state.zoom));
                
                // Check if hovered while releasing a dragged wire
                if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                    if (ui_state.active_src_pin_id != -1) {
                        audio_graph.add_link(ui_state.active_src_pin_id, pin_id);
                        engine_.commit_graph_changes();
                        ui_state.active_src_pin_id = -1;
                    }
                }
                ImGui::SetItemAllowOverlap();
                ImGui::PopID();
            }
        }

        // ====================================================================
        // FIX: THE WIRE DRAG START (Output Pins)
        // ====================================================================
        if (!is_amp) {
            for (size_t idx = 0; idx < node.output_pin_ids.size(); ++idx) {
                int pin_id = node.output_pin_ids[idx];
                float pin_y = node_screen_pos.y + (node_height * (idx + 1.0f) / (node.output_pin_ids.size() + 1.0f));
                ImVec2 pin_pos(node_screen_pos.x + node_width + 2.0f * ui_state.zoom, pin_y);
                pin_positions_cache[pin_id] = pin_pos;

                // Track active wire position to snap to the pin perfectly
                if (ui_state.active_src_pin_id == pin_id) ui_state.active_src_pin_pos = pin_pos;

                draw_list->AddCircleFilled(pin_pos, 5.0f * ui_state.zoom, IM_COL32(231, 76, 60, 255)); 
                draw_list->AddCircle(pin_pos, 6.5f * ui_state.zoom, IM_COL32(255, 255, 255, 200));

                ImGui::SetCursorScreenPos(ImVec2(pin_pos.x - 10.0f * ui_state.zoom, pin_pos.y - 10.0f * ui_state.zoom));
                ImGui::PushID(pin_id);
                ImGui::InvisibleButton("out_pin", ImVec2(20.0f * ui_state.zoom, 20.0f * ui_state.zoom));
                
                // Start drafting wire instantly on Mouse DOWN or delete on right click
                if (ImGui::IsItemHovered()) {
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        ui_state.active_src_pin_id = pin_id;
                        ui_state.active_src_pin_pos = pin_pos;
                    } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                        bool deleted_any = false;
                        auto links = audio_graph.get_links(); // copy
                        for (const auto& l : links) {
                            if (l.source_pin_id == pin_id) {
                                audio_graph.remove_link(l.id);
                                deleted_any = true;
                            }
                        }
                        if (deleted_any) engine_.commit_graph_changes();
                    }
                }
                ImGui::SetItemAllowOverlap();
                ImGui::PopID();
            }
        }

        ImGui::PopID();
    }

    // Process Deletions safely after iterating
    if (node_to_delete != -1) {
        auto* node_ptr = audio_graph.find_node(node_to_delete);
        if (node_ptr && node_ptr->routing_type == NodeRoutingType::StandardEffect) {
            auto& effects = engine_.effects();
            for (size_t i = 0; i < effects.size(); ++i) {
                if (effects[i] == node_ptr->pedal) {
                    history_.execute(std::make_unique<RemoveEffectCommand>(engine_, static_cast<int>(i)));
                    rebuild_widgets();
                    break;
                }
            }
        } else if (node_ptr) {
            audio_graph.remove_node(node_to_delete);
            engine_.commit_graph_changes();
        }
        ui_state.node_positions.erase(node_to_delete);
        ui_state.active_src_pin_id = -1; // avoid stale pin state after topology change
    }

    // Draw Patch Cables
    int link_to_delete = -1;
    for (const auto& link : audio_graph.get_links()) {
        if (pin_positions_cache.count(link.source_pin_id) && pin_positions_cache.count(link.dest_pin_id)) {
            ImVec2 p1 = pin_positions_cache[link.source_pin_id];
            ImVec2 p2 = pin_positions_cache[link.dest_pin_id];
            
            ImVec2 cp1 = ImVec2(p1.x + 45.0f * ui_state.zoom, p1.y);
            ImVec2 cp2 = ImVec2(p2.x - 45.0f * ui_state.zoom, p2.y);

            // Distance detection for hovering/clicking
            bool hovered = false;
            ImVec2 mouse_pos = ImGui::GetMousePos();
            for (float t = 0.0f; t <= 1.0f; t += 0.1f) {
                float u = 1.0f - t;
                float px = (u*u*u) * p1.x + (3*u*u*t) * cp1.x + (3*u*t*t) * cp2.x + (t*t*t) * p2.x;
                float py = (u*u*u) * p1.y + (3*u*u*t) * cp1.y + (3*u*t*t) * cp2.y + (t*t*t) * p2.y;
                float dx = px - mouse_pos.x;
                float dy = py - mouse_pos.y;
                if (dx * dx + dy * dy < 100.0f * ui_state.zoom * ui_state.zoom) {
                    hovered = true;
                    break;
                }
            }

            ImU32 color = hovered ? IM_COL32(255, 100, 100, 255) : IM_COL32(212, 175, 55, 255);
            draw_list->AddBezierCubic(p1, cp1, cp2, p2, color, hovered ? 5.0f * ui_state.zoom : 3.0f * ui_state.zoom);

            if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                link_to_delete = link.id;
            }
        }
    }
    
    if (link_to_delete != -1) {
        audio_graph.remove_link(link_to_delete);
        engine_.commit_graph_changes();
    }

    // Draw Wire Spline Drafting
    if (ui_state.active_src_pin_id != -1) {
        ImVec2 mouse_pos = ImGui::GetMousePos();
        ImVec2 p1 = ui_state.active_src_pin_pos;
        ImVec2 cp1 = ImVec2(p1.x + 45.0f, p1.y);
        ImVec2 cp2 = ImVec2(mouse_pos.x - 45.0f, mouse_pos.y);
        draw_list->AddBezierCubic(p1, cp1, cp2, mouse_pos, IM_COL32(255, 255, 255, 160), 2.0f, 0);

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            ui_state.active_src_pin_id = -1; // Snap cable back if dropped in empty space
        }
    }

    draw_list->PopClipRect();
}

} // namespace Amplitron