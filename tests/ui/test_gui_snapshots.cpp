/**
 * @file test_gui_snapshots.cpp
 * @brief Headless UI tests for GuiSnapshots view component.
 */
#include <imgui_internal.h>

#include <memory>

#include "audio/effects/distortion/overdrive.h"
#include "gui/commands/command_history.h"
#include "test_fixtures.h"
#include "test_framework.h"

#define private public
#include "gui/views/gui_snapshots.h"
#undef private

using namespace Amplitron;

TEST_F(PresetTest, gui_snapshots_render_lifecycle) {
    ScopedImGuiContext imgui;
    GuiSnapshots gs;

    SnapshotsProps props;
    props.slots[0] = {true, true, "A"};
    props.slots[1] = {true, false, "B"};
    props.slots[2] = {false, false, "C"};
    props.slots[3] = {false, false, "D"};

    int recalled_slot = -1;
    int saved_slot = -1;
    int cleared_slot = -1;

    props.on_recall_slot = [&](int i) { recalled_slot = i; };
    props.on_save_slot = [&](int i) { saved_slot = i; };
    props.on_clear_slot = [&](int i) { cleared_slot = i; };

    gs.set_props(props);

    // Render non-active, active, and empty buttons
    gs.render();

    // Trigger status notification
    gs.status_timer_ = 1.5f;
    std::snprintf(gs.status_msg_, sizeof(gs.status_msg_), "Mock Snapshot Notification");
    gs.render();

    // Context menu triggering
    ImGui::OpenPopup("SnapCtx_0");
    gs.render();

    ImGui::OpenPopup("SnapCtx_2");
    gs.render();
}

TEST_F(PresetTest, gui_snapshots_interactions) {
    ScopedImGuiContext imgui;
    ImGuiIO& io = ImGui::GetIO();
    GuiSnapshots gs;

    SnapshotsProps props;
    props.slots[0] = {true, true, "A"};
    props.slots[1] = {true, false, "B"};
    props.slots[2] = {false, false, "C"};
    props.slots[3] = {false, false, "D"};

    int recalled_slot = -1;
    props.on_recall_slot = [&](int i) { recalled_slot = i; };
    gs.set_props(props);

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");

    // Render once to layout buttons before click interactions
    gs.render();

    ImVec2 btn_min = gs.button_mins_[0];
    ImVec2 btn_max = gs.button_maxs_[0];
    ImVec2 click_pos = ImVec2((btn_min.x + btn_max.x) * 0.5f, (btn_min.y + btn_max.y) * 0.5f);

    // Frame 1: Hover at Slot 0 button "A"
    io.MousePos = click_pos;
    io.MouseDown[0] = false;
    io.MouseClicked[0] = false;
    advance_frame();
    gs.render();

    // Frame 2: Mouse Down (Click starts)
    io.MouseDown[0] = true;
    io.MouseClicked[0] = true;
    advance_frame();
    gs.render();

    // Frame 3: Mouse Up (Click releases and triggers recall)
    io.MouseDown[0] = false;
    io.MouseClicked[0] = false;
    advance_frame();
    gs.render();

    ImGui::End();

    ASSERT_EQ(recalled_slot, 0);
}

TEST_F(PresetTest, gui_snapshots_context_menu_interactions) {
    ScopedImGuiContext imgui;
    ImGuiIO& io = ImGui::GetIO();
    GuiSnapshots gs;

    SnapshotsProps props;
    props.slots[0] = {true, true, "A"};
    props.slots[1] = {true, false, "B"};
    props.slots[2] = {false, false, "C"};
    props.slots[3] = {false, false, "D"};

    int saved_slot = -1;
    int cleared_slot = -1;
    props.on_save_slot = [&](int i) { saved_slot = i; };
    props.on_clear_slot = [&](int i) { cleared_slot = i; };
    gs.set_props(props);

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");

    // Render once to layout buttons before context menu interactions
    gs.render();

    ImVec2 btn_min = gs.button_mins_[0];
    ImVec2 btn_max = gs.button_maxs_[0];
    ImVec2 click_pos = ImVec2((btn_min.x + btn_max.x) * 0.5f, (btn_min.y + btn_max.y) * 0.5f);

    // 1. Right click on Slot 0 to open context menu
    io.MousePos = click_pos;
    io.MouseDown[1] = false;
    io.MouseClicked[1] = false;
    advance_frame();
    gs.render();

    io.MouseDown[1] = true;
    io.MouseClicked[1] = true;
    advance_frame();
    gs.render();

    io.MouseDown[1] = false;
    io.MouseClicked[1] = false;
    advance_frame();
    gs.render();  // Popup opens

    // Allow popup to render layouts
    advance_frame();
    gs.render();

    ImVec2 menu_min = gs.menu_item_mins_[0];
    ImVec2 menu_max = gs.menu_item_maxs_[0];
    ImVec2 menu_click_pos =
        ImVec2((menu_min.x + menu_max.x) * 0.5f, (menu_min.y + menu_max.y) * 0.5f);

    // 2. Click "Save current board to A"
    io.MousePos = menu_click_pos;
    io.MouseDown[0] = false;
    io.MouseClicked[0] = false;
    advance_frame();
    gs.render();

    io.MouseDown[0] = true;
    io.MouseClicked[0] = true;
    advance_frame();
    gs.render();

    io.MouseDown[0] = false;
    io.MouseClicked[0] = false;
    advance_frame();
    gs.render();

    // 3. Right click on Slot 0 again to open context menu to test "Clear A"
    io.MousePos = click_pos;
    io.MouseDown[1] = false;
    io.MouseClicked[1] = false;
    advance_frame();
    gs.render();

    io.MouseDown[1] = true;
    io.MouseClicked[1] = true;
    advance_frame();
    gs.render();

    io.MouseDown[1] = false;
    io.MouseClicked[1] = false;
    advance_frame();
    gs.render();  // Popup opens

    advance_frame();
    gs.render();

    ImVec2 clear_min = gs.menu_item_mins_[1];
    ImVec2 clear_max = gs.menu_item_maxs_[1];
    ImVec2 clear_click_pos =
        ImVec2((clear_min.x + clear_max.x) * 0.5f, (clear_min.y + clear_max.y) * 0.5f);

    // 4. Click "Clear A"
    io.MousePos = clear_click_pos;
    io.MouseDown[0] = false;
    io.MouseClicked[0] = false;
    advance_frame();
    gs.render();

    io.MouseDown[0] = true;
    io.MouseClicked[0] = true;
    advance_frame();
    gs.render();

    io.MouseDown[0] = false;
    io.MouseClicked[0] = false;
    advance_frame();
    gs.render();

    ImGui::End();

    ASSERT_EQ(saved_slot, 0);
    ASSERT_EQ(cleared_slot, 0);
}
