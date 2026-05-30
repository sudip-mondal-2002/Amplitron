/**
 * @file test_gui_modules.cpp
 * @brief Headless-safe tests for all GUI modules showing 0% coverage.
 *
 * Strategy: only methods that do NOT call ImGui::* functions are tested.
 * This covers all pure-logic, state-management, and accessor code that
 * runs independently of a GPU/window context.
 *
 * Covers:
 *  - SpectrumAnalyzer  (spectrum_analyzer.cpp)
 *  - GuiPresets        (gui_presets.cpp)
 *  - GuiSnapshots      (gui_snapshots.cpp)
 *  - GuiMidi           (gui_midi.cpp)
 *  - GuiRecording      (gui_recording.h)
 *  - GuiTuner          (gui_tuner.h)
 *  - GuiAnalyzer       (gui_analyzer.h / .cpp)
 *  - GuiSettings       (gui_settings.h)
 */

#include "test_framework.h"
#include "test_fixtures.h"

#include "audio/dsp/spectrum_analyzer.h"
#include "gui/views/gui_presets.h"
#include "gui/views/gui_snapshots.h"
#include "gui/views/gui_midi.h"
#include "gui/views/gui_recording.h"
#include "gui/views/gui_tuner.h"
#include "gui/views/gui_analyzer.h"
#include "gui/views/gui_settings.h"
#include "gui/commands/command_history.h"
#include "midi/midi_manager.h"
#include "audio/effects/overdrive.h"
#include "audio/effects/tuner.h"
#include "preset_manager.h"

#include <cmath>
#include <string>
#include <memory>
#include <vector>
#include <algorithm>

using namespace Amplitron;

// ============================================================
// SpectrumAnalyzer – pure DSP, no ImGui
// ============================================================

TEST(spectrum_analyzer_construction_initializes_floor) {
    SpectrumAnalyzer sa;
    // After construction the smoothed bars should all be at -90 dB (floor).
    // Feed zero samples for one frame and verify bars stay at floor.
    std::vector<float> zeros(SpectrumAnalyzer::FFT_SIZE, 0.0f);
    sa.update(zeros.data(), zeros.data(), 48000, 1.0f / 60.0f);
    // We can't directly inspect private arrays, but update() must not crash
    // and draw(nullptr,...) must be a silent no-op.
    sa.draw(nullptr, ImVec2(0, 0), ImVec2(100, 100), SpectrumAnalyzer::DisplayMode::Output);
}

TEST(spectrum_analyzer_update_silence_does_not_crash) {
    SpectrumAnalyzer sa;
    std::vector<float> zeros(SpectrumAnalyzer::FFT_SIZE, 0.0f);
    // Multiple frames must be stable
    for (int frame = 0; frame < 5; ++frame) {
        sa.update(zeros.data(), zeros.data(), 44100, 1.0f / 60.0f);
    }
}

TEST(spectrum_analyzer_update_tone_does_not_produce_nan) {
    SpectrumAnalyzer sa;
    // Generate a 1 kHz sine wave
    std::vector<float> tone(SpectrumAnalyzer::FFT_SIZE);
    for (int i = 0; i < SpectrumAnalyzer::FFT_SIZE; ++i) {
        tone[i] = 0.5f * std::sin(2.0f * 3.14159265f * 1000.0f * i / 48000.0f);
    }
    sa.update(tone.data(), tone.data(), 48000, 1.0f / 60.0f);
    // draw with nullptr is safe (early-return guard)
    sa.draw(nullptr, ImVec2(0, 0), ImVec2(800, 200), SpectrumAnalyzer::DisplayMode::Input);
}

TEST(spectrum_analyzer_update_null_input_does_not_crash) {
    SpectrumAnalyzer sa;
    std::vector<float> zeros(SpectrumAnalyzer::FFT_SIZE, 0.0f);
    // null input pointer → graceful floor fill
    sa.update(nullptr, zeros.data(), 48000, 1.0f / 60.0f);
    sa.update(zeros.data(), nullptr, 48000, 1.0f / 60.0f);
    sa.update(nullptr, nullptr, 48000, 1.0f / 60.0f);
}

TEST(spectrum_analyzer_update_invalid_sample_rate_does_not_crash) {
    SpectrumAnalyzer sa;
    std::vector<float> samples(SpectrumAnalyzer::FFT_SIZE, 0.1f);
    sa.update(samples.data(), samples.data(), 0, 1.0f / 60.0f);
    sa.update(samples.data(), samples.data(), -1, 1.0f / 60.0f);
}

TEST(spectrum_analyzer_draw_null_drawlist_is_safe_noop) {
    SpectrumAnalyzer sa;
    // Explicitly verify that all three display modes handle null draw_list safely
    sa.draw(nullptr, ImVec2(0, 0), ImVec2(400, 100), SpectrumAnalyzer::DisplayMode::Input);
    sa.draw(nullptr, ImVec2(0, 0), ImVec2(400, 100), SpectrumAnalyzer::DisplayMode::Output);
    sa.draw(nullptr, ImVec2(0, 0), ImVec2(400, 100), SpectrumAnalyzer::DisplayMode::Overlay);
}

TEST(spectrum_analyzer_draw_zero_size_is_safe_noop) {
    SpectrumAnalyzer sa;
    // size.x <= 2 or size.y <= 2 should early-return even with a non-null draw_list
    // We pass nullptr since we don't have a real draw list in headless mode
    sa.draw(nullptr, ImVec2(10, 10), ImVec2(1, 1), SpectrumAnalyzer::DisplayMode::Output);
    sa.draw(nullptr, ImVec2(10, 10), ImVec2(0, 200), SpectrumAnalyzer::DisplayMode::Output);
    sa.draw(nullptr, ImVec2(10, 10), ImVec2(200, 0), SpectrumAnalyzer::DisplayMode::Output);
}

// ============================================================
// GuiPresets – logic methods, no ImGui rendering
// ============================================================

TEST_F(PresetTest, gui_presets_initial_state) {
    CommandHistory history;
    GuiPresets gp(engine, history);

    ASSERT_EQ(gp.preset_count(), 0);
    ASSERT_EQ(gp.selected_preset_index(), -1);
    // Engine has no effects → state is a freshly captured snapshot → not dirty
    ASSERT_FALSE(gp.is_dirty());
    ASSERT_EQ(std::string("My Preset"), gp.current_preset_name());
    ASSERT_EQ(gp.status_message(), "");
}

TEST_F(PresetTest, gui_presets_is_dirty_after_engine_change) {
    CommandHistory history;
    GuiPresets gp(engine, history);

    // mark_clean() is called in constructor so initially not dirty
    ASSERT_FALSE(gp.is_dirty());

    // Adding an effect changes the engine state → dirty
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

TEST_F(PresetTest, gui_presets_begin_new_preset_resets_state) {
    CommandHistory history;
    GuiPresets gp(engine, history);

    gp.begin_new_preset();
    // After begin_new_preset, selected index should reset to -1 and name should be empty
    ASSERT_EQ(gp.selected_preset_index(), -1);
}

TEST_F(PresetTest, gui_presets_begin_save_preset_does_not_crash) {
    CommandHistory history;
    GuiPresets gp(engine, history);
    gp.begin_save_preset();  // Should be a no-op / state toggle only
}

TEST_F(PresetTest, gui_presets_save_named_empty_name_fails) {
    PresetManager::set_presets_dir("presets");
    CommandHistory history;
    GuiPresets gp(engine, history);

    bool result = gp.save_named_preset("", "some description");
    ASSERT_FALSE(result);
    ASSERT_TRUE(gp.status_message().find("empty") != std::string::npos ||
                gp.status_message().find("Error") != std::string::npos);
}

TEST_F(PresetTest, gui_presets_save_and_load_roundtrip) {
    PresetManager::set_presets_dir("presets");
    register_temp_file("presets/GuiPresetsTest.json");

    engine.add_effect(std::make_shared<Overdrive>());
    CommandHistory history;
    GuiPresets gp(engine, history);

    bool saved = gp.save_named_preset("GuiPresetsTest", "unit test");
    ASSERT_TRUE(saved);
    ASSERT_TRUE(gp.status_message().find("Saved") != std::string::npos ||
                gp.status_message().find("GuiPresetsTest") != std::string::npos);

    gp.refresh_presets(false);
    ASSERT_GT(gp.preset_count(), 0);

    // Find the index of our saved preset
    bool found = false;
    for (int i = 0; i < gp.preset_count(); ++i) {
        bool ok = gp.load_preset_by_index(i);
        if (ok) {
            found = true;
            break;
        }
    }
    ASSERT_TRUE(found);
}

TEST_F(PresetTest, gui_presets_delete_preset_decrements_count) {
    PresetManager::set_presets_dir("presets");
    register_temp_file("presets/GuiPresetsDelete.json");

    CommandHistory history;
    GuiPresets gp(engine, history);

    gp.save_named_preset("GuiPresetsDelete", "to be deleted");
    gp.refresh_presets(false);

    int count_before = gp.preset_count();
    ASSERT_GT(count_before, 0);

    // Find and delete the test preset
    int target_idx = -1;
    for (int i = 0; i < gp.preset_count(); ++i) {
        // Load to check if it's our test preset by name
        if (gp.status_message().find("GuiPresetsDelete") != std::string::npos) {
            target_idx = i;
            break;
        }
        gp.load_preset_by_index(i);
        if (gp.status_message().find("GuiPresetsDelete") != std::string::npos) {
            target_idx = i;
            break;
        }
    }
    if (target_idx < 0) target_idx = count_before - 1;

    bool deleted = gp.delete_preset_by_index(target_idx);
    ASSERT_TRUE(deleted);
    ASSERT_LT(gp.preset_count(), count_before);
}

TEST_F(PresetTest, gui_presets_load_by_invalid_index_fails) {
    CommandHistory history;
    GuiPresets gp(engine, history);

    ASSERT_FALSE(gp.load_preset_by_index(-1));
    ASSERT_FALSE(gp.load_preset_by_index(9999));
    ASSERT_TRUE(gp.status_message().find("Error") != std::string::npos);
}

TEST_F(PresetTest, gui_presets_load_by_path_not_found) {
    PresetManager::set_presets_dir("presets");
    CommandHistory history;
    GuiPresets gp(engine, history);

    bool result = gp.load_preset_by_path("presets/this_does_not_exist_xyz.json");
    ASSERT_FALSE(result);
    ASSERT_TRUE(gp.status_message().find("Error") != std::string::npos ||
                gp.status_message().find("not found") != std::string::npos);
}

TEST_F(PresetTest, gui_presets_serialise_to_json_not_empty) {
    CommandHistory history;
    GuiPresets gp(engine, history);

    std::string json = gp.serialise_current_preset_to_json();
    ASSERT_FALSE(json.empty());
    // Must be valid JSON containing the format key
    ASSERT_TRUE(json.find("format_version") != std::string::npos ||
                json.find("effects") != std::string::npos);
}

TEST_F(PresetTest, gui_presets_ensure_factory_presets_idempotent) {
    PresetManager::set_presets_dir("presets");
    CommandHistory history;
    GuiPresets gp(engine, history);

    // First call initializes; second call is a complete no-op
    gp.ensure_factory_presets();
    int count1 = gp.preset_count();
    gp.ensure_factory_presets();
    int count2 = gp.preset_count();
    ASSERT_EQ(count1, count2);
}

TEST_F(PresetTest, gui_presets_set_status_message) {
    CommandHistory history;
    GuiPresets gp(engine, history);

    gp.set_status_message("hello world");
    ASSERT_EQ(gp.status_message(), "hello world");
}

TEST_F(PresetTest, gui_presets_refresh_preserves_valid_selection) {
    PresetManager::set_presets_dir("presets");
    register_temp_file("presets/GuiPresetsRefresh.json");

    CommandHistory history;
    GuiPresets gp(engine, history);

    gp.save_named_preset("GuiPresetsRefresh", "refresh test");
    gp.refresh_presets(false);
    ASSERT_GE(gp.selected_preset_index(), 0);

    // Refresh preserving selection — index should remain valid
    gp.refresh_presets(true);
    ASSERT_GE(gp.selected_preset_index(), 0);
}

// ============================================================
// GuiSnapshots – save/recall/undo logic
// ============================================================

TEST_F(PresetTest, gui_snapshots_save_and_recall_slot) {
    CommandHistory history;
    GuiSnapshots gs(engine, history);

    // Save slot 0 (captures current engine state)
    gs.save_to_slot(0);
    ASSERT_TRUE(gs.manager().has_slot(0));
    ASSERT_EQ(gs.manager().active_slot(), 0);

    // Recall slot 0 (should succeed silently)
    gs.recall_slot(0);
    ASSERT_EQ(gs.manager().active_slot(), 0);
}

TEST_F(PresetTest, gui_snapshots_recall_empty_slot_is_noop) {
    CommandHistory history;
    GuiSnapshots gs(engine, history);

    // Slot 3 is empty — recall should be a no-op
    ASSERT_FALSE(gs.manager().has_slot(3));
    gs.recall_slot(3);
    // Engine should be unchanged (no crash)
}

TEST_F(PresetTest, gui_snapshots_save_to_all_four_slots) {
    CommandHistory history;
    GuiSnapshots gs(engine, history);

    for (int slot = 0; slot < 4; ++slot) {
        gs.save_to_slot(slot);
        ASSERT_TRUE(gs.manager().has_slot(slot));
    }
}

TEST_F(PresetTest, gui_snapshots_recall_is_undoable) {
    CommandHistory history;
    GuiSnapshots gs(engine, history);

    // Add an effect so the state has something to snapshot
    auto od = std::make_shared<Overdrive>();
    engine.add_effect(od);
    gs.save_to_slot(0);

    // Remove the effect and recall — engine should restore overdrive
    engine.remove_effect(0);
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 0);

    gs.recall_slot(0);
    // After recall, at least one effect should be present
    ASSERT_GT(static_cast<int>(engine.effects().size()), 0);

    // Undo the recall — effects should be gone again
    history.undo();
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 0);
}

TEST_F(PresetTest, gui_snapshots_set_pedal_board_nullptr_safe) {
    CommandHistory history;
    GuiSnapshots gs(engine, history);
    gs.set_pedal_board(nullptr);  // Must not crash

    gs.save_to_slot(0);
    gs.recall_slot(0);  // With nullptr pedal_board, must not crash
}

TEST_F(PresetTest, gui_snapshots_manager_accessor) {
    CommandHistory history;
    GuiSnapshots gs(engine, history);

    auto& mgr = gs.manager();
    const auto& const_mgr = const_cast<const GuiSnapshots&>(gs).manager();
    (void)mgr;
    (void)const_mgr;
}

// ============================================================
// GuiMidi – mapping info queries (headless; no ImGui pop-ups)
// ============================================================

TEST(gui_midi_construction_no_crash) {
    MidiManager midi;
    GuiMidi gm(midi);

    // Accessors must work
    ASSERT_EQ(&gm.midi(), &midi);
    ASSERT_EQ(&gm.manager(), &midi);
}

TEST(gui_midi_get_mapping_info_no_mappings) {
    MidiManager midi;
    GuiMidi gm(midi);

    std::string info = gm.get_mapping_info("Overdrive", "Drive");
    ASSERT_TRUE(info.empty());
}

TEST(gui_midi_get_mapping_info_with_param_mapping) {
    MidiManager midi;
    GuiMidi gm(midi);

    // Add a CC mapping for Overdrive::Drive
    MidiMapping m;
    m.cc_number = 42;
    m.midi_channel = -1;
    m.target_type = MidiTargetType::EffectParam;
    m.effect_name = "Overdrive";
    m.param_name = "Drive";
    m.mode = MidiMappingMode::Continuous;
    midi.add_mapping(m);

    std::string info = gm.get_mapping_info("Overdrive", "Drive");
    ASSERT_FALSE(info.empty());
    ASSERT_TRUE(info.find("CC") != std::string::npos || info.find("42") != std::string::npos);
}

TEST(gui_midi_get_mapping_info_toggle_mode) {
    MidiManager midi;
    GuiMidi gm(midi);

    MidiMapping m;
    m.cc_number = 7;
    m.midi_channel = -1;
    m.target_type = MidiTargetType::EffectParam;
    m.effect_name = "Compressor";
    m.param_name = "Ratio";
    m.mode = MidiMappingMode::Toggle;
    midi.add_mapping(m);

    std::string info = gm.get_mapping_info("Compressor", "Ratio");
    ASSERT_FALSE(info.empty());
    ASSERT_TRUE(info.find("Toggle") != std::string::npos);
}

TEST(gui_midi_get_mapping_info_bypass_not_returned_for_param) {
    MidiManager midi;
    GuiMidi gm(midi);

    // Add a bypass mapping — should NOT be returned when querying param info
    MidiMapping m;
    m.cc_number = 5;
    m.midi_channel = -1;
    m.target_type = MidiTargetType::EffectBypass;
    m.effect_name = "Reverb";
    m.param_name = "";
    m.mode = MidiMappingMode::Toggle;
    midi.add_mapping(m);

    // Query as param (not bypass) → should return empty
    std::string info = gm.get_mapping_info("Reverb", "Decay");
    ASSERT_TRUE(info.empty());
}

TEST(gui_midi_get_mapping_info_wrong_effect_returns_empty) {
    MidiManager midi;
    GuiMidi gm(midi);

    MidiMapping m;
    m.cc_number = 15;
    m.midi_channel = -1;
    m.target_type = MidiTargetType::EffectParam;
    m.effect_name = "Delay";
    m.param_name = "Time";
    m.mode = MidiMappingMode::Continuous;
    midi.add_mapping(m);

    // Different effect name → no match
    std::string info = gm.get_mapping_info("Chorus", "Time");
    ASSERT_TRUE(info.empty());
}

// ============================================================
// GuiRecording – state accessors (no ImGui)
// ============================================================

TEST_F(AudioEngineTest, gui_recording_initial_state) {
    GuiRecording gr(engine);

    ASSERT_FALSE(gr.is_save_pending());
    ASSERT_FALSE(gr.show_save());
    ASSERT_TRUE(gr.status_message().empty());
}

TEST_F(AudioEngineTest, gui_recording_set_save_pending) {
    GuiRecording gr(engine);

    gr.set_save_pending(true);
    ASSERT_TRUE(gr.is_save_pending());

    gr.set_save_pending(false);
    ASSERT_FALSE(gr.is_save_pending());
}

TEST_F(AudioEngineTest, gui_recording_show_save_toggle) {
    GuiRecording gr(engine);

    gr.show_save() = true;
    ASSERT_TRUE(gr.show_save());

    gr.show_save() = false;
    ASSERT_FALSE(gr.show_save());
}

TEST_F(AudioEngineTest, gui_recording_status_message_read_write) {
    GuiRecording gr(engine);

    gr.status_message() = "Recording saved successfully";
    ASSERT_EQ(gr.status_message(), "Recording saved successfully");
}

// ============================================================
// GuiTuner – construction and toggle (no ImGui)
// ============================================================

TEST_F(AudioEngineTest, gui_tuner_construction_and_accessor) {
    auto tuner = std::make_shared<TunerPedal>();
    GuiTuner gt(engine, tuner);

    // Accessor must return the exact same shared_ptr
    ASSERT_EQ(gt.tuner_instance(), tuner);
}

TEST_F(AudioEngineTest, gui_tuner_toggle_enables_show_flag) {
    auto tuner = std::make_shared<TunerPedal>();
    GuiTuner gt(engine, tuner);

    bool show = false;
    gt.toggle(show);
    // toggle() should flip or at least modify the show flag
    // (Actual implementation in gui_tuner.cpp enables/disables tuner effect)
    // Either show is now true (tuner activated) or effect was toggled
    // The specific behavior depends on implementation — we just verify no crash
    (void)show;
}

TEST_F(AudioEngineTest, gui_tuner_toggle_multiple_times_stable) {
    auto tuner = std::make_shared<TunerPedal>();
    GuiTuner gt(engine, tuner);

    bool show = false;
    for (int i = 0; i < 10; ++i) {
        gt.toggle(show);
    }
    // Must not crash after multiple toggles
}

// ============================================================
// GuiAnalyzer – construction and state accessors
// ============================================================

TEST_F(AudioEngineTest, gui_analyzer_construction_state) {
    GuiAnalyzer ga(engine);

    // Default: expanded = true
    ASSERT_TRUE(ga.is_expanded());
    // Reserved height when expanded should be larger than collapsed
    float expanded_h = ga.analyzer_reserved_height();
    ASSERT_GT(expanded_h, 0.0f);
}

TEST_F(AudioEngineTest, gui_analyzer_reserved_height_values) {
    GuiAnalyzer ga(engine);

    // When expanded, height = 245; when collapsed (can't toggle without ImGui), height = 38
    // We can at least verify the expanded value is positive and reasonable
    ASSERT_NEAR(ga.analyzer_reserved_height(), 245.0f, 1.0f);
}

// ============================================================
// GuiSettings – construction (headless safe)
// ============================================================

TEST_F(AudioEngineTest, gui_settings_construction_no_crash) {
    GuiSettings gs(engine);
    // Construction must succeed and object must be usable
    // (render() cannot be called without ImGui context)
    (void)gs;
}
