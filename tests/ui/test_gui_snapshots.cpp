/**
 * @file test_gui_snapshots.cpp
 * @brief Headless-safe tests for GuiSnapshots slot management and rendering.
 *
 * Tests save_to_slot(), recall_slot(), undo integration, accessor safety,
 * and ImGui rendering widgets using a software ImGui context.
 */
#include "test_framework.h"
#include "test_fixtures.h"
#include "gui/command_history.h"
#include "audio/effects/overdrive.h"
#include <memory>

#define private public
#include "gui/gui_snapshots.h"
#undef private

using namespace Amplitron;

TEST_F(PresetTest, gui_snapshots_all_slots_empty_on_construction) {
    CommandHistory history;
    GuiSnapshots gs(engine, history);

    for (int i = 0; i < 4; ++i) {
        ASSERT_FALSE(gs.manager().has_slot(i));
    }
}

TEST_F(PresetTest, gui_snapshots_save_to_slot_fills_slot) {
    CommandHistory history;
    GuiSnapshots gs(engine, history);

    gs.save_to_slot(0);
    ASSERT_TRUE(gs.manager().has_slot(0));
}

TEST_F(PresetTest, gui_snapshots_save_updates_active_slot) {
    CommandHistory history;
    GuiSnapshots gs(engine, history);

    gs.save_to_slot(2);
    ASSERT_EQ(gs.manager().active_slot(), 2);
}

TEST_F(PresetTest, gui_snapshots_save_to_all_four_slots) {
    CommandHistory history;
    GuiSnapshots gs(engine, history);

    for (int slot = 0; slot < 4; ++slot) {
        gs.save_to_slot(slot);
        ASSERT_TRUE(gs.manager().has_slot(slot));
    }
}

TEST_F(PresetTest, gui_snapshots_recall_empty_slot_is_noop) {
    CommandHistory history;
    GuiSnapshots gs(engine, history);

    ASSERT_FALSE(gs.manager().has_slot(3));
    gs.recall_slot(3);  // Must not crash or alter engine
}

TEST_F(PresetTest, gui_snapshots_recall_restores_state) {
    CommandHistory history;
    GuiSnapshots gs(engine, history);

    auto od = std::make_shared<Overdrive>();
    engine.add_effect(od);
    gs.save_to_slot(0);

    engine.remove_effect(0);
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 0);

    gs.recall_slot(0);
    ASSERT_GT(static_cast<int>(engine.effects().size()), 0);
}

TEST_F(PresetTest, gui_snapshots_recall_is_undoable) {
    CommandHistory history;
    GuiSnapshots gs(engine, history);

    auto od = std::make_shared<Overdrive>();
    engine.add_effect(od);
    gs.save_to_slot(1);

    engine.remove_effect(0);
    gs.recall_slot(1);
    ASSERT_GT(static_cast<int>(engine.effects().size()), 0);

    history.undo();
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 0);
}

TEST_F(PresetTest, gui_snapshots_set_pedal_board_nullptr_is_safe) {
    CommandHistory history;
    GuiSnapshots gs(engine, history);

    gs.set_pedal_board(nullptr);
    gs.save_to_slot(0);
    gs.recall_slot(0);  // With nullptr pedal_board, must not crash
}

TEST_F(PresetTest, gui_snapshots_manager_mutable_const_accessors) {
    CommandHistory history;
    GuiSnapshots gs(engine, history);

    SnapshotManager& mut_mgr         = gs.manager();
    const SnapshotManager& const_mgr = const_cast<const GuiSnapshots&>(gs).manager();
    ASSERT_EQ(&mut_mgr, &const_mgr);
}

TEST_F(PresetTest, gui_snapshots_overwrite_existing_slot) {
    CommandHistory history;
    GuiSnapshots gs(engine, history);

    gs.save_to_slot(0);
    engine.add_effect(std::make_shared<Overdrive>());
    gs.save_to_slot(0);  // Overwrite — must not crash

    ASSERT_TRUE(gs.manager().has_slot(0));
}

TEST_F(PresetTest, gui_snapshots_render) {
    ScopedImGuiContext imgui;
    CommandHistory history;
    GuiSnapshots gs(engine, history);

    // 1. Save to slot 0 (renders as filled)
    gs.save_to_slot(0);

    // 2. Clear active slot so we can test non-active filled slot
    gs.manager().set_active_slot(-1);

    // 3. Render once to exercise non-active filled (slot 0) and empty (slots 1-3) buttons
    gs.render();

    // 4. Set active slot to 0 and render again (active filled slot)
    gs.manager().set_active_slot(0);
    gs.render();

    // 5. Force status timer and message to test status text rendering branch
    gs.status_timer_ = 1.5f;
    std::snprintf(gs.status_msg_, sizeof(gs.status_msg_), "Mock Snapshot Notification");
    gs.render();

    // 6. Test programmatic context menus for slots 0 (filled) and 1 (empty)
    ImGui::OpenPopup("SnapCtx_0");
    gs.render();

    ImGui::OpenPopup("SnapCtx_1");
    gs.render();
}
