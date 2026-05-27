/**
 * @file test_gui_presets.cpp
 * @brief Headless-safe tests for GuiPresets logic and rendering.
 *
 * Tests properties, dirty flags, save/load/delete, path sanitation
 * for filename-unsafe special characters, and ImGui rendering dialogs
 * using a software ImGui context.
 */
#include "test_framework.h"
#include "test_fixtures.h"
#include "gui/views/gui_presets.h"
#include "gui/commands/command_history.h"
#include "audio/effects/overdrive.h"
#include "audio/effects/reverb.h"
#include "preset_manager.h"
#include <filesystem>
#include <string>
#include <memory>

using namespace Amplitron;

// ============================================================
// Initial state
// ============================================================

TEST_F(PresetTest, gui_presets_initial_preset_count_is_zero) {
    CommandHistory history;
    GuiPresets gp(engine, history);
    ASSERT_EQ(gp.preset_count(), 0);
}

TEST_F(PresetTest, gui_presets_initial_selection_is_minus_one) {
    CommandHistory history;
    GuiPresets gp(engine, history);
    ASSERT_EQ(gp.selected_preset_index(), -1);
}

TEST_F(PresetTest, gui_presets_initial_status_message_is_empty) {
    CommandHistory history;
    GuiPresets gp(engine, history);
    ASSERT_TRUE(gp.status_message().empty());
}

TEST_F(PresetTest, gui_presets_initial_not_dirty) {
    CommandHistory history;
    GuiPresets gp(engine, history);
    ASSERT_FALSE(gp.is_dirty());
}

TEST_F(PresetTest, gui_presets_default_name_is_my_preset) {
    CommandHistory history;
    GuiPresets gp(engine, history);
    ASSERT_EQ(gp.current_preset_name(), "My Preset");
}

// ============================================================
// Dirty / clean tracking
// ============================================================

TEST_F(PresetTest, gui_presets_dirty_after_adding_effect) {
    CommandHistory history;
    GuiPresets gp(engine, history);

    engine.add_effect(std::make_shared<Overdrive>());
    ASSERT_TRUE(gp.is_dirty());
}

TEST_F(PresetTest, gui_presets_mark_clean_clears_dirty_flag) {
    CommandHistory history;
    GuiPresets gp(engine, history);

    engine.add_effect(std::make_shared<Overdrive>());
    ASSERT_TRUE(gp.is_dirty());
    gp.mark_clean();
    ASSERT_FALSE(gp.is_dirty());
}

TEST_F(PresetTest, gui_presets_dirty_again_after_further_change) {
    CommandHistory history;
    GuiPresets gp(engine, history);

    engine.add_effect(std::make_shared<Overdrive>());
    gp.mark_clean();
    ASSERT_FALSE(gp.is_dirty());

    engine.add_effect(std::make_shared<Reverb>());
    ASSERT_TRUE(gp.is_dirty());
}

// ============================================================
// begin_new_preset / begin_save_preset
// ============================================================

TEST_F(PresetTest, gui_presets_begin_new_preset_resets_index) {
    CommandHistory history;
    GuiPresets gp(engine, history);

    gp.begin_new_preset();
    ASSERT_EQ(gp.selected_preset_index(), -1);
}

TEST_F(PresetTest, gui_presets_begin_save_preset_does_not_crash) {
    CommandHistory history;
    GuiPresets gp(engine, history);
    gp.begin_save_preset();
}

// ============================================================
// Save validation
// ============================================================

TEST_F(PresetTest, gui_presets_save_empty_name_fails) {
    PresetManager::set_presets_dir("presets");
    CommandHistory history;
    GuiPresets gp(engine, history);

    bool ok = gp.save_named_preset("", "desc");
    ASSERT_FALSE(ok);
    ASSERT_FALSE(gp.status_message().empty());
}

TEST_F(PresetTest, gui_presets_status_message_set_get) {
    CommandHistory history;
    GuiPresets gp(engine, history);

    gp.set_status_message("test message");
    ASSERT_EQ(gp.status_message(), "test message");
}

// ============================================================
// Save → refresh → load round-trip
// ============================================================

TEST_F(PresetTest, gui_presets_save_and_load_roundtrip) {
    PresetManager::set_presets_dir("presets");
    register_temp_file("presets/GuiPresetsRT.json");

    engine.add_effect(std::make_shared<Overdrive>());
    CommandHistory history;
    GuiPresets gp(engine, history);

    ASSERT_TRUE(gp.save_named_preset("GuiPresetsRT", "roundtrip"));
    gp.refresh_presets(false);
    ASSERT_GT(gp.preset_count(), 0);

    // Load should succeed
    bool loaded = false;
    for (int i = 0; i < gp.preset_count(); ++i) {
        if (gp.load_preset_by_index(i)) { loaded = true; break; }
    }
    ASSERT_TRUE(loaded);
}

TEST_F(PresetTest, gui_presets_load_invalid_index_fails) {
    CommandHistory history;
    GuiPresets gp(engine, history);

    ASSERT_FALSE(gp.load_preset_by_index(-1));
    ASSERT_FALSE(gp.load_preset_by_index(9999));
    ASSERT_FALSE(gp.status_message().empty());
}

TEST_F(PresetTest, gui_presets_load_nonexistent_path_fails) {
    PresetManager::set_presets_dir("presets");
    CommandHistory history;
    GuiPresets gp(engine, history);

    ASSERT_FALSE(gp.load_preset_by_path("presets/__no_such_file__.json"));
    ASSERT_FALSE(gp.status_message().empty());
}

// ============================================================
// Delete
// ============================================================

TEST_F(PresetTest, gui_presets_delete_reduces_count) {
    PresetManager::set_presets_dir("presets");
    register_temp_file("presets/GuiPresetsDel.json");

    CommandHistory history;
    GuiPresets gp(engine, history);

    gp.save_named_preset("GuiPresetsDel", "delete me");
    gp.refresh_presets(false);
    int before = gp.preset_count();
    ASSERT_GT(before, 0);

    // Find the exact index of our test preset to delete
    int idx = -1;
    for (int i = 0; i < gp.preset_count(); ++i) {
        if (gp.load_preset_by_index(i) && gp.current_preset_name() == "GuiPresetsDel") {
            idx = i;
            break;
        }
    }
    ASSERT_NE(idx, -1);
    ASSERT_TRUE(gp.delete_preset_by_index(idx));
    ASSERT_LT(gp.preset_count(), before);
}

TEST_F(PresetTest, gui_presets_delete_invalid_index_fails) {
    CommandHistory history;
    GuiPresets gp(engine, history);

    ASSERT_FALSE(gp.delete_preset_by_index(-1));
    ASSERT_FALSE(gp.delete_preset_by_index(999));
}

// ============================================================
// Serialisation
// ============================================================

TEST_F(PresetTest, gui_presets_serialise_returns_non_empty_json) {
    CommandHistory history;
    GuiPresets gp(engine, history);

    std::string json = gp.serialise_current_preset_to_json();
    ASSERT_FALSE(json.empty());
    ASSERT_TRUE(json.find("effects") != std::string::npos ||
                json.find("format_version") != std::string::npos);
}

// ============================================================
// Factory presets
// ============================================================

TEST_F(PresetTest, gui_presets_ensure_factory_presets_idempotent) {
    PresetManager::set_presets_dir("presets");
    CommandHistory history;
    GuiPresets gp(engine, history);

    gp.ensure_factory_presets();
    int count1 = gp.preset_count();
    gp.ensure_factory_presets(); // second call must be a no-op
    int count2 = gp.preset_count();
    ASSERT_EQ(count1, count2);
}

// ============================================================
// Refresh preserves selection
// ============================================================

TEST_F(PresetTest, gui_presets_refresh_preserves_valid_selection) {
    PresetManager::set_presets_dir("presets");
    register_temp_file("presets/GuiPresetsRefresh.json");

    CommandHistory history;
    GuiPresets gp(engine, history);

    gp.save_named_preset("GuiPresetsRefresh", "refresh test");
    gp.refresh_presets(false);
    ASSERT_GE(gp.selected_preset_index(), 0);

    gp.refresh_presets(true);  // preserve selection
    ASSERT_GE(gp.selected_preset_index(), 0);
}

// ============================================================
// Path Sanitation & Special Characters
// ============================================================

TEST_F(PresetTest, gui_presets_save_special_characters_sanitization) {
    PresetManager::set_presets_dir("presets");
    register_temp_file("presets/My___Cool___Presets___.json");

    CommandHistory history;
    GuiPresets gp(engine, history);

    // Save with spaces, slashes, backslashes, colons, asterisks
    bool ok = gp.save_named_preset("My / Cool : Presets? *", "special chars");
    ASSERT_TRUE(ok);

    gp.refresh_presets(false);
    
    // Verify it was correctly sanitized and exists on disk
    ASSERT_TRUE(std::filesystem::exists("presets/My___Cool___Presets___.json"));
}

// ============================================================
// ImGui Dialog Rendering
// ============================================================

TEST_F(PresetTest, gui_presets_render_save_popup) {
    ScopedImGuiContext imgui;
    CommandHistory history;
    GuiPresets gp(engine, history);

    bool show = true;
    gp.render_save_popup(show);
}

TEST_F(PresetTest, gui_presets_render_load_popup) {
    ScopedImGuiContext imgui;
    CommandHistory history;
    GuiPresets gp(engine, history);

    bool show = true;
    gp.render_load_popup(show);
}
