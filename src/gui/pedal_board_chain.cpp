#include "gui/pedal_board.h"
#include "gui/pedal_widget.h"
#include "gui/theme.h"
#include "gui/gui_graph_state.h"
#include "gui/command.h"
#include <algorithm>
#include <imgui.h>
#include <unordered_map>
#include <cmath>
namespace Amplitron {
void PedalBoard::render_signal_chain() {
    auto& ui_state = GuiGraphState::get_instance();
    // canvas_hovered is updated after the InvisibleButton is drawn (see below)
    float dt = ImGui::GetIO().DeltaTime;
    float lerp_factor = 1.0f - std::exp(-30.0f * dt);
    if (lerp_factor < 0.0f) lerp_factor = 0.0f;
    if (lerp_factor > 1.0f) lerp_factor = 1.0f;
    float old_zoom = ui_state.zoom;
    if (std::abs(ui_state.target_zoom - ui_state.zoom) > 0.0001f) {
        ui_state.zoom += (ui_state.target_zoom - ui_state.zoom) * lerp_factor;
    } else {
        ui_state.zoom = ui_state.target_zoom;
    }
    if (old_zoom != ui_state.zoom) {
        // Use canvas-local mouse coords to avoid drift when canvas is not at screen origin
        float actual_factor = ui_state.zoom / old_zoom;
        ImVec2 mouse_pos = ImGui::GetMousePos();
        ImVec2 canvas_pos_now = ui_state.last_canvas_pos;
        float local_x = mouse_pos.x - canvas_pos_now.x;
        float local_y = mouse_pos.y - canvas_pos_now.y;
        ui_state.scrolling.x = local_x - (local_x - ui_state.scrolling.x) * actual_factor;
        ui_state.scrolling.y = local_y - (local_y - ui_state.scrolling.y) * actual_factor;
        ui_state.target_scrolling = ui_state.scrolling;
    }
    if (std::abs(ui_state.target_scrolling.x - ui_state.scrolling.x) > 0.01f ||
        std::abs(ui_state.target_scrolling.y - ui_state.scrolling.y) > 0.01f) {
        ui_state.scrolling.x += (ui_state.target_scrolling.x - ui_state.scrolling.x) * lerp_factor;
        ui_state.scrolling.y += (ui_state.target_scrolling.y - ui_state.scrolling.y) * lerp_factor;
    } else {
        ui_state.scrolling = ui_state.target_scrolling;
    }
    auto& audio_graph = engine_.graph(); 
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    ImVec2 canvas_end = ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y);
    ui_state.last_canvas_pos = canvas_pos;
    ImGui::SetCursorScreenPos(canvas_pos);
    ImGuiButtonFlags btn_flags = ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle;
    if (ui_state.hand_tool_active) {
        btn_flags |= ImGuiButtonFlags_MouseButtonLeft;
    }
    
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton("canvas_panning_hotspot", canvas_size, btn_flags);
    ImGui::SetItemAllowOverlap();
    // Update canvas_hovered here — after InvisibleButton — so it reflects the actual canvas item
    ui_state.canvas_hovered = ImGui::IsItemHovered();
    
    if (ui_state.hand_tool_active && ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    
    if (ImGui::IsItemActive() && (ImGui::IsMouseDragging(ImGuiMouseButton_Right) || 
                                  ImGui::IsMouseDragging(ImGuiMouseButton_Middle) || 
                                  (ui_state.hand_tool_active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)))) {
        ui_state.scrolling.x += ImGui::GetIO().MouseDelta.x;
        ui_state.scrolling.y += ImGui::GetIO().MouseDelta.y;
        ui_state.target_scrolling = ui_state.scrolling;
    }
    // Zooming is now allowed in both fullscreen and normal modes
    if (ImGui::IsItemHovered()) {
        float scroll_x = ImGui::GetIO().MouseWheelH;
        float scroll_y = ImGui::GetIO().MouseWheel;
        if (scroll_x != 0.0f || scroll_y != 0.0f) {
            if (ImGui::GetIO().KeyCtrl) {
                float zoom_factor = std::pow(1.1f, scroll_y);
                ImVec2 mouse_pos = ImGui::GetMousePos();
                ImVec2 mouse_in_canvas = ImVec2(mouse_pos.x - canvas_pos.x, mouse_pos.y - canvas_pos.y);
                float old_zoom = ui_state.target_zoom;
                float new_zoom = old_zoom * zoom_factor;
                if (new_zoom < 0.2f) new_zoom = 0.2f;
                if (new_zoom > 5.0f) new_zoom = 5.0f;
                float actual_factor = new_zoom / old_zoom;
                ui_state.target_scrolling.x = mouse_in_canvas.x - (mouse_in_canvas.x - ui_state.target_scrolling.x) * actual_factor;
                ui_state.target_scrolling.y = mouse_in_canvas.y - (mouse_in_canvas.y - ui_state.target_scrolling.y) * actual_factor;
                ui_state.target_zoom = new_zoom;
            } else {
                ui_state.target_scrolling.x += scroll_x * 20.0f;
                ui_state.target_scrolling.y += scroll_y * 20.0f;
            }
        }
    }
    // Draw fullscreen button at top right
    ImGui::SetCursorScreenPos(ImVec2(canvas_pos.x + canvas_size.x - 70, canvas_pos.y + 10));
    ImGui::SetNextItemAllowOverlap();
    if (ImGui::Button(ui_state.is_fullscreen ? "Exit FS" : "Full Screen")) {
        ui_state.is_fullscreen = !ui_state.is_fullscreen;
        if (!ui_state.is_fullscreen) {
            ui_state.zoom = 1.0f;
            ui_state.target_zoom = 1.0f;
        }
    }
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
            }
            float insert_x = ui_state.node_positions.empty() ? 40.0f : max_right + 80.0f;
            ui_state.node_positions[node.id] = { ImVec2(insert_x, 60.0f), false };
        }
    }
    // Animation pulse based on time and audio level
    float level = engine_.get_output_level();
    float time = (float)ImGui::GetTime();
    bool is_running = engine_.is_running();
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
#ifdef __EMSCRIPTEN__
            ImGui::SetWindowFontScale(ui_state.zoom / ui_state.dpi_scale);
#else
            ImGui::SetWindowFontScale(ui_state.zoom);
#endif
            target_widget->render(ui_state.zoom); 
            ImGui::SetWindowFontScale(1.0f);
            ImGui::EndGroup();
            ImGui::SetCursorScreenPos(node_screen_pos);
            ImGui::SetNextItemAllowOverlap(); 
            ImGui::InvisibleButton("native_drag_handle", ImVec2(node_width - 25.0f * ui_state.zoom, 30.0f * ui_state.zoom));
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
            ImGui::SetNextItemAllowOverlap();
            ImGui::InvisibleButton("util_drag_handle", ImVec2(node_width - 25.0f * ui_state.zoom, node_height));
            if (!ui_state.hand_tool_active && ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                node_layout.position.x += ImGui::GetIO().MouseDelta.x / ui_state.zoom;
                node_layout.position.y += ImGui::GetIO().MouseDelta.y / ui_state.zoom;
            }
            ImVec2 text_pos = ImVec2(node_screen_pos.x + 12.0f * ui_state.zoom, node_screen_pos.y + 25.0f * ui_state.zoom);
#ifdef __EMSCRIPTEN__
            ImGui::SetWindowFontScale(ui_state.zoom / ui_state.dpi_scale);
#else
            ImGui::SetWindowFontScale(ui_state.zoom);
#endif
            draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 255), node.name.c_str());
            ImGui::SetWindowFontScale(1.0f);
        }
        if (!node.is_reachable) {
            ImVec2 node_end = ImVec2(node_screen_pos.x + node_width, node_screen_pos.y + node_height);
            draw_list->AddRectFilled(node_screen_pos, node_end, IM_COL32(0, 0, 0, 180), Theme::ROUNDING_MD * ui_state.zoom);
            
            ImVec2 text_pos = ImVec2(node_screen_pos.x + 10.0f * ui_state.zoom, node_screen_pos.y + node_height - 25.0f * ui_state.zoom);
#ifdef __EMSCRIPTEN__
            ImGui::SetWindowFontScale(ui_state.zoom * 0.9f / ui_state.dpi_scale);
#else
            ImGui::SetWindowFontScale(ui_state.zoom * 0.9f);
#endif
            draw_list->AddText(text_pos, IM_COL32(255, 60, 60, 255), "DISCONNECTED");
            ImGui::SetWindowFontScale(1.0f);
        }
        // --- INTERNAL SIGNAL FLOW / ELECTRICITY EFFECT ---
        if (node.is_reachable && (!node.input_pin_ids.empty() || !node.output_pin_ids.empty())) {
            bool enabled = true;
            if (target_widget) {
                enabled = target_widget->get_effect()->is_enabled();
            }
            // Align with pins
            float in_y = node_screen_pos.y + node_height * 0.5f;
            if (!node.input_pin_ids.empty()) {
                in_y = node_screen_pos.y + (node_height * (0 + 1.0f) / (node.input_pin_ids.size() + 1.0f));
            }
            float out_y = node_screen_pos.y + node_height * 0.5f;
            if (!node.output_pin_ids.empty()) {
                out_y = node_screen_pos.y + (node_height * (0 + 1.0f) / (node.output_pin_ids.size() + 1.0f));
            }
            
            ImVec2 flow_p1(node_screen_pos.x, in_y);
            ImVec2 flow_p2(node_screen_pos.x + node_width, out_y);
            // --- COLOR & PULSE CALCULATIONS (SHARED) ---
            ImU32 flow_col = IM_COL32(200, 230, 255, 255);
            if (target_widget) {
                const auto* colors = get_effect_color(target_widget->get_effect()->name());
                flow_col = ImGui::ColorConvertFloat4ToU32(colors->led_color);
            }
            
            ImU32 r = (flow_col >> 0) & 0xFF;
            ImU32 g = (flow_col >> 8) & 0xFF;
            ImU32 b = (flow_col >> 16) & 0xFF;
            float pulse = 0.6f + 0.4f * std::sin(time * 8.0f) * (0.5f + level * 2.0f);
            float jitter = std::sin(time * 40.0f) * 1.5f * ui_state.zoom;
            if (enabled) {
                // --- ELECTRICITY PASSING THROUGH (ACTIVE) ---
                // No internal line, only glowing edges
                ImVec2 p_min(node_screen_pos.x - (2.0f + jitter), node_screen_pos.y - (2.0f + jitter));
                ImVec2 p_max(node_screen_pos.x + node_width + (2.0f + jitter), node_screen_pos.y + node_height + (2.0f + jitter));
                
                draw_list->AddRect(p_min, p_max, IM_COL32(r, g, b, (int)(180 * pulse)), Theme::ROUNDING_MD * ui_state.zoom, 0, 3.0f * ui_state.zoom);
                draw_list->AddRect(p_min, p_max, IM_COL32(255, 255, 255, (int)(220 * pulse)), Theme::ROUNDING_MD * ui_state.zoom, 0, 1.0f * ui_state.zoom);
            } else {
                // --- ELECTRIC HIGH-RAIL BYPASS PATH ---
                // Rectangular "bridge" with CONSISTENT glow style
                float rail_height = 65.0f * ui_state.zoom;
                float rail_y = node_screen_pos.y - rail_height;
                
                ImVec2 p_rail_in(flow_p1.x, rail_y);
                ImVec2 p_rail_out(flow_p2.x, rail_y);
                auto draw_consistent_glow_line = [&](ImVec2 p1, ImVec2 p2) {
                    ImVec2 p1j(p1.x, p1.y + jitter);
                    ImVec2 p2j(p2.x, p2.y + jitter);
                    draw_list->AddLine(p1j, p2j, IM_COL32(r, g, b, (int)(180 * pulse)), 4.0f * ui_state.zoom);
                    draw_list->AddLine(p1j, p2j, IM_COL32(255, 255, 255, (int)(220 * pulse)), 1.5f * ui_state.zoom);
                };
                // Bridge segments
                draw_consistent_glow_line(flow_p1, p_rail_in);
                draw_consistent_glow_line(p_rail_in, p_rail_out);
                draw_consistent_glow_line(p_rail_out, flow_p2);
                // --- MOVING ELECTRONS (PULSES) ---
                // Electrons travel along the bridge path: Up -> Across -> Down
                float electron_time = std::fmod(time * 2.5f, 1.0f);
                for (int e = 0; e < 2; ++e) {
                    float t = std::fmod(electron_time + e * 0.5f, 1.0f);
                    ImVec2 electron_pos;
                    
                    if (t < 0.2f) { // Segment 1: Up
                        float st = t / 0.2f;
                        electron_pos = ImVec2(flow_p1.x, flow_p1.y + (rail_y - flow_p1.y) * st + jitter);
                    } else if (t < 0.8f) { // Segment 2: Across
                        float st = (t - 0.2f) / 0.6f;
                        electron_pos = ImVec2(p_rail_in.x + (p_rail_out.x - p_rail_in.x) * st, rail_y + jitter);
                    } else { // Segment 3: Down
                        float st = (t - 0.8f) / 0.2f;
                        electron_pos = ImVec2(p_rail_out.x, rail_y + (flow_p2.y - rail_y) * st + jitter);
                    }
                    
                    draw_list->AddCircleFilled(electron_pos, 3.5f * ui_state.zoom, IM_COL32(255, 255, 255, (int)(220 * pulse)));
                    draw_list->AddCircle(electron_pos, 7.0f * ui_state.zoom, IM_COL32(r, g, b, (int)(180 * pulse)), 0, 2.0f * ui_state.zoom);
                }
                if (ImGui::IsMouseHoveringRect(ImVec2(node_screen_pos.x, rail_y - 10), flow_p2)) {
                    ImGui::SetTooltip("%s (Bypassed - High Rail Signal Path)", target_widget->get_effect()->name());
                }
            }
            if (ImGui::IsMouseHoveringRect(flow_p1, flow_p2) && (target_widget || !node.name.empty())) {
                const char* name = target_widget ? target_widget->get_effect()->name() : node.name.c_str();
                ImGui::SetTooltip("%s (%s)", name, enabled ? "Active" : "Bypassed");
            }
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
            ImGui::SetNextItemAllowOverlap();
            if (ImGui::SmallButton(remove_label.c_str())) {
                node_to_delete = node.id;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Remove %s from chain", node.name.c_str());
            }
            
            ImGui::PopStyleColor(2);
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
                ImGui::SetNextItemAllowOverlap();
                ImGui::InvisibleButton("in_pin", ImVec2(20.0f * ui_state.zoom, 20.0f * ui_state.zoom));
                
                // Check if hovered while releasing a dragged wire
                ImVec2 mouse_pos = ImGui::GetMousePos();
                float dist_sq = pow(mouse_pos.x - pin_pos.x, 2) + pow(mouse_pos.y - pin_pos.y, 2);
                if (dist_sq < pow(15.0f * ui_state.zoom, 2) && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                    printf("MANUAL DROP HOVER DETECTED! src: %d, dest: %d\n", ui_state.active_src_pin_id, pin_id);
                    if (ui_state.active_src_pin_id != -1) {
                        int res = audio_graph.add_link(ui_state.active_src_pin_id, pin_id);
                        printf("add_link returned: %d\n", res);
                        engine_.commit_graph_changes();
                        ui_state.active_src_pin_id = -1;
                    }
                }
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
                ImGui::SetNextItemAllowOverlap();
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
            // --- Signal Pulse Animation on Cables ---
            if (is_running) {
                float t_off = std::fmod(time * 2.0f, 1.0f); // Slightly faster
                for (int step = 0; step < 4; ++step) { // More pulses
                    float t = std::fmod(t_off + step * 0.25f, 1.0f);
                    float u = 1.0f - t;
                    float hx = (u*u*u) * p1.x + (3*u*u*t) * cp1.x + (3*u*t*t) * cp2.x + (t*t*t) * p2.x;
                    float hy = (u*u*u) * p1.y + (3*u*u*t) * cp1.y + (3*u*t*t) * cp2.y + (t*t*t) * p2.y;
                    
                    // Larger, more vibrant pulse
                    float pulse_size = (3.0f + 2.0f * level) * ui_state.zoom;
                    draw_list->AddCircleFilled(ImVec2(hx, hy), pulse_size, Theme::ACCENT_GOLD_HOT);
                    draw_list->AddCircle(ImVec2(hx, hy), pulse_size + 1.0f * ui_state.zoom, IM_COL32(255, 255, 255, 150));
                }
            }
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
    // Fix ImGui cursor bounds warnings after free panning
    ImGui::SetCursorPos(ImVec2(0, 0));
    ImGui::Dummy(canvas_size);
    draw_list->PopClipRect();
}
} // namespace Amplitron