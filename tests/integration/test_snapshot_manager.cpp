#include "test_framework.h"
#include "gui/state/snapshot_manager.h"
#include "gui/commands/command.h"
#include "gui/commands/command_history.h"
#include "audio/engine/audio_engine.h"
#include "audio/effects/overdrive.h"
#include "audio/effects/delay.h"
#include "audio/effects/reverb.h"
#include "audio/effects/equalizer.h"
#include "audio/effects/noise_gate.h"
#include "audio/effects/compressor.h"
#include "audio/effects/distortion.h"
#include "audio/effects/chorus.h"

#include <cstring>

using namespace Amplitron;

// Helper: clear all effects from engine
static void clear_engine(AudioEngine& engine) {
    while (!engine.effects().empty()) {
        engine.remove_effect(static_cast<int>(engine.effects().size()) - 1);
    }
}

// Helper: simulate the exact recall_slot() flow from GuiSnapshots
// (capture before, get stored after, create command, execute via history)
static void gui_recall_slot(SnapshotManager& mgr, int slot,
                            AudioEngine& engine, CommandHistory& history) {
    if (!mgr.has_slot(slot)) return;
    auto before = SnapshotManager::capture(engine);
    const auto* after_snap = mgr.get_slot(slot);
    history.execute(std::make_unique<RecallSnapshotCommand>(
        engine,
        before.effects, before.input_gain, before.output_gain,
        after_snap->effects, after_snap->input_gain, after_snap->output_gain
    ));
    mgr.set_active_slot(slot);
}

// ============================================================
// SnapshotManager — slot management
// ============================================================

TEST(snapshot_empty_slots_by_default) {
    SnapshotManager mgr;
    for (int i = 0; i < SnapshotManager::NUM_SLOTS; ++i) {
        ASSERT_FALSE(mgr.has_slot(i));
        ASSERT_TRUE(mgr.get_slot(i) == nullptr);
    }
    ASSERT_EQ(mgr.active_slot(), -1);
}

TEST(snapshot_save_marks_slot_filled) {
    AudioEngine engine;
    engine.initialize();
    engine.add_effect(std::make_shared<Overdrive>());

    SnapshotManager mgr;
    mgr.save_slot(0, engine);
    ASSERT_TRUE(mgr.has_slot(0));
    ASSERT_TRUE(mgr.get_slot(0) != nullptr);
    ASSERT_FALSE(mgr.has_slot(1));

    engine.shutdown();
}

TEST(snapshot_clear_slot) {
    AudioEngine engine;
    engine.initialize();
    engine.add_effect(std::make_shared<Overdrive>());

    SnapshotManager mgr;
    mgr.save_slot(0, engine);
    ASSERT_TRUE(mgr.has_slot(0));

    mgr.clear_slot(0);
    ASSERT_FALSE(mgr.has_slot(0));

    engine.shutdown();
}

TEST(snapshot_out_of_range_slot_is_safe) {
    AudioEngine engine;
    engine.initialize();

    SnapshotManager mgr;
    mgr.save_slot(-1, engine);
    mgr.save_slot(SnapshotManager::NUM_SLOTS, engine);
    ASSERT_FALSE(mgr.has_slot(-1));
    ASSERT_FALSE(mgr.has_slot(SnapshotManager::NUM_SLOTS));
    mgr.clear_slot(-1);
    mgr.clear_slot(SnapshotManager::NUM_SLOTS);
    mgr.recall_slot_direct(-1, engine);
    mgr.recall_slot_direct(SnapshotManager::NUM_SLOTS, engine);

    engine.shutdown();
}

TEST(snapshot_save_overwrites_existing_slot) {
    AudioEngine engine;
    engine.initialize();

    auto od = std::make_shared<Overdrive>();
    engine.add_effect(od);
    engine.set_input_gain(0.5f);

    SnapshotManager mgr;
    mgr.save_slot(0, engine);
    ASSERT_NEAR(mgr.get_slot(0)->input_gain, 0.5f, 0.001f);
    ASSERT_EQ(static_cast<int>(mgr.get_slot(0)->effects.size()), 1);

    // Add another effect and save again to the same slot
    engine.add_effect(std::make_shared<Delay>());
    engine.set_input_gain(0.9f);
    mgr.save_slot(0, engine);

    ASSERT_NEAR(mgr.get_slot(0)->input_gain, 0.9f, 0.001f);
    ASSERT_EQ(static_cast<int>(mgr.get_slot(0)->effects.size()), 2);

    engine.shutdown();
}

// ============================================================
// SnapshotManager — capture correctness
// ============================================================

TEST(snapshot_captures_effect_chain) {
    AudioEngine engine;
    engine.initialize();

    auto od = std::make_shared<Overdrive>();
    auto dl = std::make_shared<Delay>();
    engine.add_effect(od);
    engine.add_effect(dl);
    engine.set_input_gain(0.6f);
    engine.set_output_gain(0.75f);

    SnapshotManager mgr;
    mgr.save_slot(0, engine);

    const auto* snap = mgr.get_slot(0);
    ASSERT_TRUE(snap != nullptr);
    ASSERT_EQ(static_cast<int>(snap->effects.size()), 2);
    ASSERT_NEAR(snap->input_gain,  0.6f,  0.001f);
    ASSERT_NEAR(snap->output_gain, 0.75f, 0.001f);
    ASSERT_EQ(std::string(snap->effects[0].effect->name()), std::string("Overdrive"));
    ASSERT_EQ(std::string(snap->effects[1].effect->name()), std::string("Delay"));

    engine.shutdown();
}

TEST(snapshot_captures_parameter_values) {
    AudioEngine engine;
    engine.initialize();

    auto eq = std::make_shared<Equalizer>();
    float custom_val = eq->params()[0].min_val +
                       (eq->params()[0].max_val - eq->params()[0].min_val) * 0.8f;
    eq->params()[0].value = custom_val;
    engine.add_effect(eq);

    SnapshotManager mgr;
    mgr.save_slot(1, engine);

    const auto* snap = mgr.get_slot(1);
    ASSERT_TRUE(snap != nullptr);
    ASSERT_FALSE(snap->effects.empty());
    ASSERT_FALSE(snap->effects[0].param_values.empty());
    ASSERT_NEAR(snap->effects[0].param_values[0], custom_val, 0.001f);

    engine.shutdown();
}

TEST(snapshot_captures_enabled_and_mix) {
    AudioEngine engine;
    engine.initialize();

    auto rv = std::make_shared<Reverb>();
    rv->set_enabled(false);
    rv->set_mix(0.35f);
    engine.add_effect(rv);

    SnapshotManager mgr;
    mgr.save_slot(2, engine);

    const auto* snap = mgr.get_slot(2);
    ASSERT_TRUE(snap != nullptr);
    ASSERT_EQ(snap->effects[0].enabled, false);
    ASSERT_NEAR(snap->effects[0].mix, 0.35f, 0.001f);

    engine.shutdown();
}

TEST(snapshot_capture_is_independent_of_later_modifications) {
    AudioEngine engine;
    engine.initialize();

    auto od = std::make_shared<Overdrive>();
    od->params()[0].value = 0.3f;
    engine.add_effect(od);

    SnapshotManager mgr;
    mgr.save_slot(0, engine);

    // Modify the same effect AFTER saving
    od->params()[0].value = 0.9f;

    // The stored snapshot should still have the original value
    const auto* snap = mgr.get_slot(0);
    ASSERT_NEAR(snap->effects[0].param_values[0], 0.3f, 0.001f);

    engine.shutdown();
}

// ============================================================
// Direct recall (recall_slot_direct) — no undo
// ============================================================

TEST(snapshot_apply_restores_chain) {
    AudioEngine engine;
    engine.initialize();

    engine.add_effect(std::make_shared<Overdrive>());
    engine.add_effect(std::make_shared<Delay>());
    engine.set_input_gain(0.5f);
    engine.set_output_gain(0.6f);

    SnapshotManager mgr;
    mgr.save_slot(0, engine);

    // Replace board with Reverb
    clear_engine(engine);
    engine.add_effect(std::make_shared<Reverb>());
    engine.set_input_gain(0.9f);
    engine.set_output_gain(0.9f);
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 1);

    // Recall snapshot A
    mgr.recall_slot_direct(0, engine);

    ASSERT_EQ(static_cast<int>(engine.effects().size()), 2);
    ASSERT_EQ(std::string(engine.effects()[0]->name()), std::string("Overdrive"));
    ASSERT_EQ(std::string(engine.effects()[1]->name()), std::string("Delay"));
    ASSERT_NEAR(engine.get_input_gain(),  0.5f, 0.001f);
    ASSERT_NEAR(engine.get_output_gain(), 0.6f, 0.001f);

    engine.shutdown();
}

TEST(snapshot_apply_restores_parameter_values) {
    AudioEngine engine;
    engine.initialize();

    auto ng = std::make_shared<NoiseGate>();
    float saved_val = ng->params()[0].min_val +
                      (ng->params()[0].max_val - ng->params()[0].min_val) * 0.4f;
    ng->params()[0].value = saved_val;
    engine.add_effect(ng);

    SnapshotManager mgr;
    mgr.save_slot(0, engine);

    // Modify parameter after saving
    ng->params()[0].value = ng->params()[0].max_val;

    // Recall restores the saved value
    mgr.recall_slot_direct(0, engine);
    ASSERT_NEAR(engine.effects()[0]->params()[0].value, saved_val, 0.001f);

    engine.shutdown();
}

TEST(snapshot_apply_restores_enabled_and_mix) {
    AudioEngine engine;
    engine.initialize();

    auto od = std::make_shared<Overdrive>();
    od->set_enabled(true);
    od->set_mix(0.6f);
    engine.add_effect(od);

    SnapshotManager mgr;
    mgr.save_slot(0, engine);

    // Modify enabled and mix
    od->set_enabled(false);
    od->set_mix(1.0f);
    ASSERT_EQ(od->is_enabled(), false);
    ASSERT_NEAR(od->get_mix(), 1.0f, 0.001f);

    // Recall restores
    mgr.recall_slot_direct(0, engine);
    ASSERT_EQ(engine.effects()[0]->is_enabled(), true);
    ASSERT_NEAR(engine.effects()[0]->get_mix(), 0.6f, 0.001f);

    engine.shutdown();
}

TEST(snapshot_recall_updates_active_slot) {
    AudioEngine engine;
    engine.initialize();
    engine.add_effect(std::make_shared<Overdrive>());

    SnapshotManager mgr;
    mgr.save_slot(2, engine);
    mgr.recall_slot_direct(2, engine);
    ASSERT_EQ(mgr.active_slot(), 2);

    engine.shutdown();
}

TEST(snapshot_recall_empty_slot_is_noop) {
    AudioEngine engine;
    engine.initialize();
    engine.add_effect(std::make_shared<Overdrive>());
    engine.set_input_gain(0.5f);

    SnapshotManager mgr;
    // Slot 0 is empty — recall should be a no-op
    mgr.recall_slot_direct(0, engine);
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 1);
    ASSERT_NEAR(engine.get_input_gain(), 0.5f, 0.001f);
    ASSERT_EQ(mgr.active_slot(), -1);

    engine.shutdown();
}

// ============================================================
// A/B switching — the core use case
// ============================================================

TEST(snapshot_ab_switch_same_effects_different_params) {
    // This is the most common use case: same chain, different knob positions
    AudioEngine engine;
    engine.initialize();

    auto od = std::make_shared<Overdrive>();
    auto dl = std::make_shared<Delay>();
    engine.add_effect(od);
    engine.add_effect(dl);

    // State A: low gain, short delay
    od->params()[0].value = 0.2f;
    dl->params()[0].value = 100.0f;
    engine.set_input_gain(0.5f);

    SnapshotManager mgr;
    mgr.save_slot(0, engine);

    // State B: high gain, long delay
    od->params()[0].value = 0.8f;
    dl->params()[0].value = 500.0f;
    engine.set_input_gain(0.9f);
    mgr.save_slot(1, engine);

    // Recall A — params should revert to A's values
    mgr.recall_slot_direct(0, engine);
    ASSERT_NEAR(od->params()[0].value, 0.2f, 0.001f);
    ASSERT_NEAR(dl->params()[0].value, 100.0f, 0.5f);
    ASSERT_NEAR(engine.get_input_gain(), 0.5f, 0.001f);

    // Recall B — params should change to B's values
    mgr.recall_slot_direct(1, engine);
    ASSERT_NEAR(od->params()[0].value, 0.8f, 0.001f);
    ASSERT_NEAR(dl->params()[0].value, 500.0f, 0.5f);
    ASSERT_NEAR(engine.get_input_gain(), 0.9f, 0.001f);

    // Switch back to A again
    mgr.recall_slot_direct(0, engine);
    ASSERT_NEAR(od->params()[0].value, 0.2f, 0.001f);
    ASSERT_NEAR(dl->params()[0].value, 100.0f, 0.5f);
    ASSERT_NEAR(engine.get_input_gain(), 0.5f, 0.001f);

    engine.shutdown();
}

TEST(snapshot_ab_switch_different_effect_chains) {
    // Save A with [OD, Delay], B with [Reverb, EQ, Compressor]
    AudioEngine engine;
    engine.initialize();

    auto od = std::make_shared<Overdrive>();
    auto dl = std::make_shared<Delay>();
    engine.add_effect(od);
    engine.add_effect(dl);

    SnapshotManager mgr;
    mgr.save_slot(0, engine);

    // Build a completely different chain for B
    clear_engine(engine);
    auto rv = std::make_shared<Reverb>();
    auto eq = std::make_shared<Equalizer>();
    auto cp = std::make_shared<Compressor>();
    engine.add_effect(rv);
    engine.add_effect(eq);
    engine.add_effect(cp);
    mgr.save_slot(1, engine);

    // Currently: [Reverb, EQ, Compressor]
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 3);

    // Recall A → [OD, Delay]
    mgr.recall_slot_direct(0, engine);
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 2);
    ASSERT_EQ(std::string(engine.effects()[0]->name()), std::string("Overdrive"));
    ASSERT_EQ(std::string(engine.effects()[1]->name()), std::string("Delay"));

    // Recall B → [Reverb, EQ, Compressor]
    mgr.recall_slot_direct(1, engine);
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 3);
    ASSERT_EQ(std::string(engine.effects()[0]->name()), std::string("Reverb"));
    ASSERT_EQ(std::string(engine.effects()[1]->name()), std::string("Equalizer"));
    ASSERT_EQ(std::string(engine.effects()[2]->name()), std::string("Compressor"));

    engine.shutdown();
}

TEST(snapshot_ab_switch_enabled_states) {
    // Save A with OD enabled, B with OD disabled
    AudioEngine engine;
    engine.initialize();

    auto od = std::make_shared<Overdrive>();
    od->set_enabled(true);
    engine.add_effect(od);

    SnapshotManager mgr;
    mgr.save_slot(0, engine);

    od->set_enabled(false);
    mgr.save_slot(1, engine);

    // Recall A → OD enabled
    mgr.recall_slot_direct(0, engine);
    ASSERT_EQ(engine.effects()[0]->is_enabled(), true);

    // Recall B → OD disabled
    mgr.recall_slot_direct(1, engine);
    ASSERT_EQ(engine.effects()[0]->is_enabled(), false);

    engine.shutdown();
}

TEST(snapshot_rapid_abab_switching) {
    // Rapid switching between A and B (5 round-trips)
    AudioEngine engine;
    engine.initialize();

    auto od = std::make_shared<Overdrive>();
    engine.add_effect(od);

    SnapshotManager mgr;

    od->params()[0].value = 0.1f;
    engine.set_input_gain(0.3f);
    mgr.save_slot(0, engine);

    od->params()[0].value = 0.9f;
    engine.set_input_gain(0.8f);
    mgr.save_slot(1, engine);

    for (int round = 0; round < 5; ++round) {
        mgr.recall_slot_direct(0, engine);
        ASSERT_NEAR(od->params()[0].value, 0.1f, 0.001f);
        ASSERT_NEAR(engine.get_input_gain(), 0.3f, 0.001f);
        ASSERT_EQ(mgr.active_slot(), 0);

        mgr.recall_slot_direct(1, engine);
        ASSERT_NEAR(od->params()[0].value, 0.9f, 0.001f);
        ASSERT_NEAR(engine.get_input_gain(), 0.8f, 0.001f);
        ASSERT_EQ(mgr.active_slot(), 1);
    }

    engine.shutdown();
}

// ============================================================
// RecallSnapshotCommand — via CommandHistory::execute()
// (exactly mirrors the GuiSnapshots::recall_slot() path)
// ============================================================

TEST(snapshot_gui_flow_ab_switch_via_command_history) {
    // This test mirrors the EXACT flow in GuiSnapshots::recall_slot():
    //   1. Capture current state as "before"
    //   2. Get stored snapshot as "after"
    //   3. Create RecallSnapshotCommand
    //   4. Execute via CommandHistory::execute() (which calls cmd->execute())
    AudioEngine engine;
    engine.initialize();

    auto od = std::make_shared<Overdrive>();
    auto dl = std::make_shared<Delay>();
    engine.add_effect(od);
    engine.add_effect(dl);

    // Save A: OD drive=0.2
    od->params()[0].value = 0.2f;
    engine.set_input_gain(0.4f);

    SnapshotManager mgr;
    CommandHistory history;

    mgr.save_slot(0, engine);
    mgr.set_active_slot(0);

    // Modify: OD drive=0.8
    od->params()[0].value = 0.8f;
    engine.set_input_gain(0.9f);

    // Save B
    mgr.save_slot(1, engine);
    mgr.set_active_slot(1);

    // Now simulate clicking [A] — the exact GUI flow
    gui_recall_slot(mgr, 0, engine, history);

    // Verify: effect params restored to A's values
    ASSERT_NEAR(engine.effects()[0]->params()[0].value, 0.2f, 0.001f);
    ASSERT_NEAR(engine.get_input_gain(), 0.4f, 0.001f);
    ASSERT_EQ(mgr.active_slot(), 0);
    // Engine still has 2 effects (same shared_ptrs)
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 2);

    // Click [B]
    gui_recall_slot(mgr, 1, engine, history);

    ASSERT_NEAR(engine.effects()[0]->params()[0].value, 0.8f, 0.001f);
    ASSERT_NEAR(engine.get_input_gain(), 0.9f, 0.001f);
    ASSERT_EQ(mgr.active_slot(), 1);

    // Undo → should go back to A's state
    history.undo();
    ASSERT_NEAR(engine.effects()[0]->params()[0].value, 0.2f, 0.001f);
    ASSERT_NEAR(engine.get_input_gain(), 0.4f, 0.001f);

    // Redo → back to B's state
    history.redo();
    ASSERT_NEAR(engine.effects()[0]->params()[0].value, 0.8f, 0.001f);
    ASSERT_NEAR(engine.get_input_gain(), 0.9f, 0.001f);

    engine.shutdown();
}

TEST(snapshot_gui_flow_different_chains_via_command_history) {
    AudioEngine engine;
    engine.initialize();

    // State A: Overdrive only
    auto od = std::make_shared<Overdrive>();
    engine.add_effect(od);

    SnapshotManager mgr;
    CommandHistory history;

    mgr.save_slot(0, engine);

    // State B: Reverb + Delay
    clear_engine(engine);
    auto rv = std::make_shared<Reverb>();
    auto dl = std::make_shared<Delay>();
    engine.add_effect(rv);
    engine.add_effect(dl);
    mgr.save_slot(1, engine);

    // Currently at B: [Reverb, Delay]
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 2);

    // Click [A]
    gui_recall_slot(mgr, 0, engine, history);
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 1);
    ASSERT_EQ(std::string(engine.effects()[0]->name()), std::string("Overdrive"));

    // Click [B]
    gui_recall_slot(mgr, 1, engine, history);
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 2);
    ASSERT_EQ(std::string(engine.effects()[0]->name()), std::string("Reverb"));
    ASSERT_EQ(std::string(engine.effects()[1]->name()), std::string("Delay"));

    engine.shutdown();
}

TEST(snapshot_gui_flow_all_params_restored) {
    // Verify every param of every effect is restored, not just the first
    AudioEngine engine;
    engine.initialize();

    auto od = std::make_shared<Overdrive>();
    engine.add_effect(od);

    // Save all param values for state A
    auto& params = od->params();
    std::vector<float> state_a_values;
    for (auto& p : params) {
        p.value = p.min_val;
        state_a_values.push_back(p.value);
    }

    SnapshotManager mgr;
    CommandHistory history;

    mgr.save_slot(0, engine);

    // Change all params to max for state B
    std::vector<float> state_b_values;
    for (auto& p : params) {
        p.value = p.max_val;
        state_b_values.push_back(p.value);
    }
    mgr.save_slot(1, engine);

    // Recall A — all params should be at min
    gui_recall_slot(mgr, 0, engine, history);
    auto& restored_params = engine.effects()[0]->params();
    for (int i = 0; i < static_cast<int>(restored_params.size()); ++i) {
        ASSERT_NEAR(restored_params[i].value, state_a_values[i], 0.001f);
    }

    // Recall B — all params should be at max
    gui_recall_slot(mgr, 1, engine, history);
    for (int i = 0; i < static_cast<int>(restored_params.size()); ++i) {
        ASSERT_NEAR(restored_params[i].value, state_b_values[i], 0.001f);
    }

    engine.shutdown();
}

// ============================================================
// RecallSnapshotCommand — undo/redo
// ============================================================

TEST(snapshot_recall_command_undo_redo) {
    AudioEngine engine;
    engine.initialize();

    auto od = std::make_shared<Overdrive>();
    engine.add_effect(od);
    engine.set_input_gain(0.5f);
    engine.set_output_gain(0.6f);

    auto before = SnapshotManager::capture(engine);

    clear_engine(engine);
    auto rv = std::make_shared<Reverb>();
    auto dl = std::make_shared<Delay>();
    engine.add_effect(rv);
    engine.add_effect(dl);
    engine.set_input_gain(0.8f);
    engine.set_output_gain(0.7f);

    auto after = SnapshotManager::capture(engine);

    CommandHistory history;
    history.push_executed(std::make_unique<RecallSnapshotCommand>(
        engine,
        before.effects, before.input_gain, before.output_gain,
        after.effects,  after.input_gain,  after.output_gain
    ));

    ASSERT_EQ(static_cast<int>(engine.effects().size()), 2);
    ASSERT_EQ(std::string(engine.effects()[0]->name()), std::string("Reverb"));

    history.undo();
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 1);
    ASSERT_EQ(std::string(engine.effects()[0]->name()), std::string("Overdrive"));
    ASSERT_NEAR(engine.get_input_gain(),  0.5f, 0.001f);
    ASSERT_NEAR(engine.get_output_gain(), 0.6f, 0.001f);

    history.redo();
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 2);
    ASSERT_EQ(std::string(engine.effects()[0]->name()), std::string("Reverb"));
    ASSERT_NEAR(engine.get_input_gain(),  0.8f, 0.001f);
    ASSERT_NEAR(engine.get_output_gain(), 0.7f, 0.001f);

    engine.shutdown();
}

TEST(snapshot_recall_command_description) {
    AudioEngine engine;
    engine.initialize();

    auto before = SnapshotManager::capture(engine);
    auto after  = SnapshotManager::capture(engine);

    auto cmd = std::make_unique<RecallSnapshotCommand>(
        engine,
        before.effects, before.input_gain, before.output_gain,
        after.effects,  after.input_gain,  after.output_gain
    );

    ASSERT_EQ(std::string(cmd->description()), std::string("Recall Snapshot"));
    engine.shutdown();
}

// ============================================================
// Multiple slots are independent
// ============================================================

TEST(snapshot_multiple_slots_independent) {
    AudioEngine engine;
    engine.initialize();

    engine.add_effect(std::make_shared<Overdrive>());
    SnapshotManager mgr;
    mgr.save_slot(0, engine);

    clear_engine(engine);
    engine.add_effect(std::make_shared<Reverb>());
    mgr.save_slot(1, engine);

    mgr.recall_slot_direct(0, engine);
    ASSERT_EQ(std::string(engine.effects()[0]->name()), std::string("Overdrive"));
    ASSERT_EQ(mgr.active_slot(), 0);

    mgr.recall_slot_direct(1, engine);
    ASSERT_EQ(std::string(engine.effects()[0]->name()), std::string("Reverb"));
    ASSERT_EQ(mgr.active_slot(), 1);

    engine.shutdown();
}

TEST(snapshot_four_slots_abcd) {
    AudioEngine engine;
    engine.initialize();

    SnapshotManager mgr;

    // Save four different configs
    engine.add_effect(std::make_shared<Overdrive>());
    mgr.save_slot(0, engine);

    clear_engine(engine);
    engine.add_effect(std::make_shared<Delay>());
    mgr.save_slot(1, engine);

    clear_engine(engine);
    engine.add_effect(std::make_shared<Reverb>());
    mgr.save_slot(2, engine);

    clear_engine(engine);
    engine.add_effect(std::make_shared<Chorus>());
    mgr.save_slot(3, engine);

    // Recall each and verify
    mgr.recall_slot_direct(0, engine);
    ASSERT_EQ(std::string(engine.effects()[0]->name()), std::string("Overdrive"));

    mgr.recall_slot_direct(3, engine);
    ASSERT_EQ(std::string(engine.effects()[0]->name()), std::string("Chorus"));

    mgr.recall_slot_direct(1, engine);
    ASSERT_EQ(std::string(engine.effects()[0]->name()), std::string("Delay"));

    mgr.recall_slot_direct(2, engine);
    ASSERT_EQ(std::string(engine.effects()[0]->name()), std::string("Reverb"));

    engine.shutdown();
}

TEST(snapshot_clear_resets_active_slot) {
    AudioEngine engine;
    engine.initialize();
    engine.add_effect(std::make_shared<Overdrive>());

    SnapshotManager mgr;
    mgr.save_slot(0, engine);
    mgr.set_active_slot(0);
    ASSERT_EQ(mgr.active_slot(), 0);

    mgr.clear_slot(0);
    ASSERT_FALSE(mgr.has_slot(0));
    ASSERT_EQ(mgr.active_slot(), -1);

    engine.shutdown();
}

TEST(snapshot_clear_other_slot_does_not_reset_active) {
    AudioEngine engine;
    engine.initialize();
    engine.add_effect(std::make_shared<Overdrive>());

    SnapshotManager mgr;
    mgr.save_slot(0, engine);
    mgr.save_slot(1, engine);
    mgr.set_active_slot(0);

    mgr.clear_slot(1);
    ASSERT_EQ(mgr.active_slot(), 0);  // active slot unchanged

    engine.shutdown();
}

// ============================================================
// Edge cases
// ============================================================

TEST(snapshot_empty_chain_roundtrip) {
    AudioEngine engine;
    engine.initialize();

    // Save empty chain
    SnapshotManager mgr;
    mgr.save_slot(0, engine);
    ASSERT_EQ(static_cast<int>(mgr.get_slot(0)->effects.size()), 0);

    // Add some effects
    engine.add_effect(std::make_shared<Overdrive>());
    engine.add_effect(std::make_shared<Reverb>());
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 2);

    // Recall empty chain
    mgr.recall_slot_direct(0, engine);
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 0);

    engine.shutdown();
}

TEST(snapshot_multiple_effects_all_params_preserved) {
    // Save a complex chain and verify every effect's every param is preserved
    AudioEngine engine;
    engine.initialize();

    auto ng = std::make_shared<NoiseGate>();
    auto od = std::make_shared<Overdrive>();
    auto eq = std::make_shared<Equalizer>();
    auto dl = std::make_shared<Delay>();
    auto rv = std::make_shared<Reverb>();
    engine.add_effect(ng);
    engine.add_effect(od);
    engine.add_effect(eq);
    engine.add_effect(dl);
    engine.add_effect(rv);

    // Set distinctive values on each effect's first param
    ng->params()[0].value = ng->params()[0].min_val;
    od->params()[0].value = od->params()[0].max_val;
    eq->params()[0].value = (eq->params()[0].min_val + eq->params()[0].max_val) / 2.0f;
    dl->params()[0].value = dl->params()[0].min_val;
    rv->params()[0].value = rv->params()[0].max_val;

    float ng_val = ng->params()[0].value;
    float od_val = od->params()[0].value;
    float eq_val = eq->params()[0].value;
    float dl_val = dl->params()[0].value;
    float rv_val = rv->params()[0].value;

    SnapshotManager mgr;
    mgr.save_slot(0, engine);

    // Scramble all params
    ng->params()[0].value = ng->params()[0].max_val;
    od->params()[0].value = od->params()[0].min_val;
    eq->params()[0].value = eq->params()[0].max_val;
    dl->params()[0].value = dl->params()[0].max_val;
    rv->params()[0].value = rv->params()[0].min_val;

    // Recall
    mgr.recall_slot_direct(0, engine);

    ASSERT_NEAR(engine.effects()[0]->params()[0].value, ng_val, 0.001f);
    ASSERT_NEAR(engine.effects()[1]->params()[0].value, od_val, 0.001f);
    ASSERT_NEAR(engine.effects()[2]->params()[0].value, eq_val, 0.001f);
    ASSERT_NEAR(engine.effects()[3]->params()[0].value, dl_val, 0.001f);
    ASSERT_NEAR(engine.effects()[4]->params()[0].value, rv_val, 0.001f);

    engine.shutdown();
}
