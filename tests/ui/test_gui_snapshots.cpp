/**
 * @file test_gui_snapshots.cpp
 * @brief Headless-safe tests for GuiSnapshots component rendering and reactive properties.
 *
 * Tests the reactive GuiSnapshots component using build_snapshots_props,
 * set_props, and software ImGui context rendering.
 */
#include "test_framework.h"
#include "test_fixtures.h"
#include "gui/commands/command_history.h"
#include "audio/effects/overdrive.h"
#include <memory>

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
