/**
 * @file test_gui_midi.cpp
 * @brief Headless-safe tests for GuiMidi mapping info queries and ImGui rendering.
 *
 * Tests construction, continuous/toggle mapping descriptions, learn popups,
 * and mapping tables using a software ImGui context.
 */
#include "test_framework.h"
#include "test_fixtures.h"
#include "gui/views/gui_midi.h"
#include "midi/midi_manager.h"
#include <string>

using namespace Amplitron;

// ============================================================
// Construction
// ============================================================

TEST(gui_midi_construction_no_crash) {
    MidiManager midi;
    GuiMidi gm(midi);
    (void)gm;
}

TEST(gui_midi_midi_accessor_returns_same_ref) {
    MidiManager midi;
    GuiMidi gm(midi);
    ASSERT_EQ(&gm.midi(), &midi);
}

TEST(gui_midi_manager_accessor_returns_same_ref) {
    MidiManager midi;
    GuiMidi gm(midi);
    ASSERT_EQ(&gm.manager(), &midi);
}

TEST(gui_midi_const_midi_accessor) {
    MidiManager midi;
    const GuiMidi gm(midi);
    ASSERT_EQ(&gm.midi(), &midi);
}

// ============================================================
// get_mapping_info — no mappings
// ============================================================

TEST(gui_midi_mapping_info_empty_when_no_mappings) {
    MidiManager midi;
    GuiMidi gm(midi);
    ASSERT_TRUE(gm.get_mapping_info("Overdrive", "Drive").empty());
}

TEST(gui_midi_mapping_info_empty_for_unknown_effect) {
    MidiManager midi;
    GuiMidi gm(midi);
    ASSERT_TRUE(gm.get_mapping_info("NonExistentFX", "SomeParam").empty());
}

// ============================================================
// get_mapping_info — Continuous mapping
// ============================================================

TEST(gui_midi_mapping_info_found_for_param_mapping) {
    MidiManager midi;
    GuiMidi gm(midi);

    MidiMapping m;
    m.cc_number   = 42;
    m.midi_channel = -1;
    m.target_type  = MidiTargetType::EffectParam;
    m.effect_name  = "Overdrive";
    m.param_name   = "Drive";
    m.mode         = MidiMappingMode::Continuous;
    midi.add_mapping(m);

    std::string info = gm.get_mapping_info("Overdrive", "Drive");
    ASSERT_FALSE(info.empty());
    ASSERT_TRUE(info.find("CC") != std::string::npos ||
                info.find("42") != std::string::npos);
}

TEST(gui_midi_mapping_info_contains_cc_number) {
    MidiManager midi;
    GuiMidi gm(midi);

    MidiMapping m;
    m.cc_number   = 7;
    m.midi_channel = -1;
    m.target_type  = MidiTargetType::EffectParam;
    m.effect_name  = "Reverb";
    m.param_name   = "Decay";
    m.mode         = MidiMappingMode::Continuous;
    midi.add_mapping(m);

    ASSERT_TRUE(gm.get_mapping_info("Reverb", "Decay").find("7") !=
                std::string::npos);
}

// ============================================================
// get_mapping_info — Toggle mapping
// ============================================================

TEST(gui_midi_mapping_info_toggle_contains_toggle_label) {
    MidiManager midi;
    GuiMidi gm(midi);

    MidiMapping m;
    m.cc_number   = 15;
    m.midi_channel = -1;
    m.target_type  = MidiTargetType::EffectParam;
    m.effect_name  = "Compressor";
    m.param_name   = "Ratio";
    m.mode         = MidiMappingMode::Toggle;
    midi.add_mapping(m);

    std::string info = gm.get_mapping_info("Compressor", "Ratio");
    ASSERT_FALSE(info.empty());
    ASSERT_TRUE(info.find("Toggle") != std::string::npos);
}

// ============================================================
// get_mapping_info — bypass mapping must NOT match param query
// ============================================================

TEST(gui_midi_mapping_info_bypass_not_returned_for_param_query) {
    MidiManager midi;
    GuiMidi gm(midi);

    MidiMapping m;
    m.cc_number   = 5;
    m.midi_channel = -1;
    m.target_type  = MidiTargetType::EffectBypass;
    m.effect_name  = "Reverb";
    m.param_name   = "";
    m.mode         = MidiMappingMode::Toggle;
    midi.add_mapping(m);

    // Querying as a param (not bypass) must return empty
    ASSERT_TRUE(gm.get_mapping_info("Reverb", "Decay").empty());
}

// ============================================================
// get_mapping_info — wrong effect name
// ============================================================

TEST(gui_midi_mapping_info_wrong_effect_returns_empty) {
    MidiManager midi;
    GuiMidi gm(midi);

    MidiMapping m;
    m.cc_number   = 20;
    m.midi_channel = -1;
    m.target_type  = MidiTargetType::EffectParam;
    m.effect_name  = "Delay";
    m.param_name   = "Time";
    m.mode         = MidiMappingMode::Continuous;
    midi.add_mapping(m);

    ASSERT_TRUE(gm.get_mapping_info("Chorus", "Time").empty());
}

// ============================================================
// get_mapping_info — wrong param name
// ============================================================

TEST(gui_midi_mapping_info_wrong_param_returns_empty) {
    MidiManager midi;
    GuiMidi gm(midi);

    MidiMapping m;
    m.cc_number   = 11;
    m.midi_channel = -1;
    m.target_type  = MidiTargetType::EffectParam;
    m.effect_name  = "Overdrive";
    m.param_name   = "Drive";
    m.mode         = MidiMappingMode::Continuous;
    midi.add_mapping(m);

    ASSERT_TRUE(gm.get_mapping_info("Overdrive", "Tone").empty());
}

// ============================================================
// get_mapping_info — multiple mappings, first matching wins
// ============================================================

TEST(gui_midi_mapping_info_first_match_returned) {
    MidiManager midi;
    GuiMidi gm(midi);

    MidiMapping m1;
    m1.cc_number   = 1;
    m1.midi_channel = -1;
    m1.target_type  = MidiTargetType::EffectParam;
    m1.effect_name  = "Overdrive";
    m1.param_name   = "Drive";
    m1.mode         = MidiMappingMode::Continuous;
    midi.add_mapping(m1);

    MidiMapping m2;
    m2.cc_number   = 2;
    m2.midi_channel = -1;
    m2.target_type  = MidiTargetType::EffectParam;
    m2.effect_name  = "Overdrive";
    m2.param_name   = "Drive";
    m2.mode         = MidiMappingMode::Continuous;
    midi.add_mapping(m2);

    std::string info = gm.get_mapping_info("Overdrive", "Drive");
    ASSERT_FALSE(info.empty());
    // Should mention CC 1 (first match)
    ASSERT_TRUE(info.find("1") != std::string::npos);
}

// ============================================================
// ImGui Dialog and MenuItem Rendering
// ============================================================

TEST(gui_midi_render_window) {
    ScopedImGuiContext imgui;
    MidiManager midi;
    GuiMidi gm(midi);

    // 1. Continuous parameter mapping
    MidiMapping m1;
    m1.cc_number = 10;
    m1.midi_channel = 1;
    m1.target_type = MidiTargetType::EffectParam;
    m1.effect_name = "Overdrive";
    m1.param_name = "Drive";
    m1.mode = MidiMappingMode::Continuous;
    midi.add_mapping(m1);

    // 2. Toggle bypass mapping
    MidiMapping m2;
    m2.cc_number = 11;
    m2.midi_channel = -1; // "All"
    m2.target_type = MidiTargetType::EffectBypass;
    m2.effect_name = "Reverb";
    m2.mode = MidiMappingMode::Toggle;
    midi.add_mapping(m2);

    // 3. Input gain mapping
    MidiMapping m3;
    m3.cc_number = 12;
    m3.midi_channel = 2;
    m3.target_type = MidiTargetType::InputGain;
    m3.mode = MidiMappingMode::Continuous;
    midi.add_mapping(m3);

    // 4. Output gain mapping
    MidiMapping m4;
    m4.cc_number = 13;
    m4.midi_channel = 3;
    m4.target_type = MidiTargetType::OutputGain;
    m4.mode = MidiMappingMode::Continuous;
    midi.add_mapping(m4);

    // Renders ports and mapping table with all target types and columns
    bool show = true;
    gm.render(show);

    // Test clear all mappings in UI
    midi.clear_mappings();
    gm.render(show);
}

TEST(gui_midi_render_learn_menu_items) {
    ScopedImGuiContext imgui;
    MidiManager midi;
    GuiMidi gm(midi);

    // Call render_learn_menu_item and render_learn_bypass_item
    gm.render_learn_menu_item("Overdrive", "Drive");
    gm.render_learn_bypass_item("Overdrive");

    // Add a mapping and call them again to exercise the "Remove Mapping" path!
    MidiMapping m;
    m.cc_number   = 12;
    m.midi_channel = -1;
    m.target_type  = MidiTargetType::EffectParam;
    m.effect_name  = "Overdrive";
    m.param_name   = "Drive";
    m.mode         = MidiMappingMode::Continuous;
    midi.add_mapping(m);

    gm.render_learn_menu_item("Overdrive", "Drive");
    gm.render_remove_mapping_item("Overdrive", "Drive");

    // Bypass mapping
    MidiMapping mb;
    mb.cc_number   = 13;
    mb.midi_channel = -1;
    mb.target_type  = MidiTargetType::EffectBypass;
    mb.effect_name  = "Overdrive";
    mb.param_name   = "";
    mb.mode         = MidiMappingMode::Toggle;
    midi.add_mapping(mb);

    gm.render_remove_bypass_item("Overdrive");
}
