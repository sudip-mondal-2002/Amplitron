#include "test_framework.h"
#define private public
#define protected public
#include "gui/commands/command.h"
#undef private
#undef protected
#include "gui/commands/command_history.h"
#include "audio/effects/overdrive.h"
#include "audio/effects/delay.h"
#include "audio/effects/reverb.h"
#include "audio/effects/distortion.h"
#include "audio/effects/chorus.h"

using namespace Amplitron;
using namespace TestFramework;

// Helper: create a minimal AudioEngine (not started, no PortAudio init)
static AudioEngine& test_engine() {
    static AudioEngine engine;
    return engine;
}

// Helper: clear engine effects
static void clear_engine(AudioEngine& engine) {
    while (!engine.effects().empty()) {
        engine.remove_effect(static_cast<int>(engine.effects().size()) - 1);
    }
}

// ==========================================================================
// AddEffectCommand tests
// ==========================================================================

TEST(AddEffectCommand_Execute) {
    auto& engine = test_engine();
    clear_engine(engine);

    CommandHistory history;
    auto fx = std::make_shared<Overdrive>();
    history.execute(std::make_unique<AddEffectCommand>(engine, fx));

    ASSERT_EQ(static_cast<int>(engine.effects().size()), 1);
    ASSERT_EQ(std::string(engine.effects()[0]->name()), std::string("Overdrive"));
}

TEST(AddEffectCommand_Undo) {
    auto& engine = test_engine();
    clear_engine(engine);

    CommandHistory history;
    auto fx = std::make_shared<Overdrive>();
    history.execute(std::make_unique<AddEffectCommand>(engine, fx));
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 1);

    history.undo();
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 0);
}

TEST(AddEffectCommand_Redo) {
    auto& engine = test_engine();
    clear_engine(engine);

    CommandHistory history;
    auto fx = std::make_shared<Overdrive>();
    history.execute(std::make_unique<AddEffectCommand>(engine, fx));
    history.undo();
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 0);

    history.redo();
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 1);
    ASSERT_EQ(std::string(engine.effects()[0]->name()), std::string("Overdrive"));
}

// ==========================================================================
// RemoveEffectCommand tests
// ==========================================================================

TEST(RemoveEffectCommand_Execute) {
    auto& engine = test_engine();
    clear_engine(engine);

    engine.add_effect(std::make_shared<Overdrive>());
    engine.add_effect(std::make_shared<Delay>());
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 2);

    CommandHistory history;
    history.execute(std::make_unique<RemoveEffectCommand>(engine, 0));
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 1);
    ASSERT_EQ(std::string(engine.effects()[0]->name()), std::string("Delay"));
}

TEST(RemoveEffectCommand_Undo) {
    auto& engine = test_engine();
    clear_engine(engine);

    auto od = std::make_shared<Overdrive>();
    auto dl = std::make_shared<Delay>();
    engine.add_effect(od);
    engine.add_effect(dl);

    CommandHistory history;
    history.execute(std::make_unique<RemoveEffectCommand>(engine, 0));
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 1);

    history.undo();
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 2);
    // Overdrive should be back at index 0
    ASSERT_EQ(std::string(engine.effects()[0]->name()), std::string("Overdrive"));
    ASSERT_EQ(std::string(engine.effects()[1]->name()), std::string("Delay"));
}

TEST(RemoveEffectCommand_Redo) {
    auto& engine = test_engine();
    clear_engine(engine);

    engine.add_effect(std::make_shared<Overdrive>());
    engine.add_effect(std::make_shared<Delay>());

    CommandHistory history;
    history.execute(std::make_unique<RemoveEffectCommand>(engine, 1));
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 1);

    history.undo();
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 2);

    history.redo();
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 1);
    ASSERT_EQ(std::string(engine.effects()[0]->name()), std::string("Overdrive"));
}

// ==========================================================================
// ReorderEffectCommand tests
// ==========================================================================

TEST(ReorderEffectCommand_Execute) {
    auto& engine = test_engine();
    clear_engine(engine);

    engine.add_effect(std::make_shared<Overdrive>());
    engine.add_effect(std::make_shared<Delay>());
    engine.add_effect(std::make_shared<Reverb>());

    CommandHistory history;
    // Move Reverb (index 2) to front (index 0)
    history.execute(std::make_unique<ReorderEffectCommand>(engine, 2, 0));
    ASSERT_EQ(std::string(engine.effects()[0]->name()), std::string("Reverb"));
    ASSERT_EQ(std::string(engine.effects()[1]->name()), std::string("Overdrive"));
    ASSERT_EQ(std::string(engine.effects()[2]->name()), std::string("Delay"));
}

TEST(ReorderEffectCommand_Undo) {
    auto& engine = test_engine();
    clear_engine(engine);

    engine.add_effect(std::make_shared<Overdrive>());
    engine.add_effect(std::make_shared<Delay>());
    engine.add_effect(std::make_shared<Reverb>());

    CommandHistory history;
    history.execute(std::make_unique<ReorderEffectCommand>(engine, 2, 0));
    history.undo();

    ASSERT_EQ(std::string(engine.effects()[0]->name()), std::string("Overdrive"));
    ASSERT_EQ(std::string(engine.effects()[1]->name()), std::string("Delay"));
    ASSERT_EQ(std::string(engine.effects()[2]->name()), std::string("Reverb"));
}

// ==========================================================================
// ParameterChangeCommand tests
// ==========================================================================

TEST(ParameterChangeCommand_Execute) {
    auto fx = std::make_shared<Overdrive>();
    auto& params = fx->params();
    ASSERT_TRUE(params.size() > 0);

    float original = params[0].value;
    float new_val = original + 1.0f;

    CommandHistory history;
    history.execute(std::make_unique<ParameterChangeCommand>(test_engine(), fx, 0, original, new_val));
    ASSERT_NEAR(params[0].value, new_val, 0.001f);
}

TEST(ParameterChangeCommand_Undo) {
    auto fx = std::make_shared<Overdrive>();
    auto& params = fx->params();
    float original = params[0].value;
    float new_val = original + 1.0f;

    CommandHistory history;
    history.execute(std::make_unique<ParameterChangeCommand>(test_engine(), fx, 0, original, new_val));
    ASSERT_NEAR(params[0].value, new_val, 0.001f);

    history.undo();
    ASSERT_NEAR(params[0].value, original, 0.001f);
}

TEST(ParameterChangeCommand_Coalescing) {
    auto fx = std::make_shared<Overdrive>();
    auto& params = fx->params();
    float original = params[0].value;

    CommandHistory history;
    // Simulate rapid knob turns — these should coalesce
    params[0].value = original + 0.1f;
    history.push_executed(std::make_unique<ParameterChangeCommand>(test_engine(), fx, 0, original, original + 0.1f));

    params[0].value = original + 0.2f;
    history.push_executed(std::make_unique<ParameterChangeCommand>(test_engine(), fx, 0, original + 0.1f, original + 0.2f));

    params[0].value = original + 0.3f;
    history.push_executed(std::make_unique<ParameterChangeCommand>(test_engine(), fx, 0, original + 0.2f, original + 0.3f));

    // All three should have been coalesced into one entry
    ASSERT_EQ(history.undo_size(), 1);

    // Undo should jump back to original value
    history.undo();
    ASSERT_NEAR(params[0].value, original, 0.001f);
}

TEST(ParameterChangeCommand_NoCoalescing_DifferentParams) {
    auto fx = std::make_shared<Overdrive>();
    auto& params = fx->params();
    ASSERT_TRUE(static_cast<int>(params.size()) >= 2);

    float orig0 = params[0].value;
    float orig1 = params[1].value;

    CommandHistory history;
    params[0].value = orig0 + 0.5f;
    history.push_executed(std::make_unique<ParameterChangeCommand>(test_engine(), fx, 0, orig0, orig0 + 0.5f));

    params[1].value = orig1 + 0.5f;
    history.push_executed(std::make_unique<ParameterChangeCommand>(test_engine(), fx, 1, orig1, orig1 + 0.5f));

    // Different params should NOT coalesce
    ASSERT_EQ(history.undo_size(), 2);
}

// ==========================================================================
// LoadPresetCommand tests
// ==========================================================================

TEST(LoadPresetCommand_UndoRedo) {
    auto& engine = test_engine();
    clear_engine(engine);

    // Set up initial state: one Overdrive
    auto od = std::make_shared<Overdrive>();
    engine.add_effect(od);
    engine.set_input_gain(0.5f);
    engine.set_output_gain(0.6f);

    // Capture before
    std::vector<LoadPresetCommand::EffectSnapshot> before;
    for (auto& fx : engine.effects()) {
        LoadPresetCommand::EffectSnapshot snap;
        snap.effect = fx;
        snap.enabled = fx->is_enabled();
        snap.mix = fx->get_mix();
        for (auto& p : fx->params()) snap.param_values.push_back(p.value);
        before.push_back(std::move(snap));
    }
    float before_in = engine.get_input_gain();
    float before_out = engine.get_output_gain();

    // Simulate loading a preset: clear and add different effects
    clear_engine(engine);
    auto dl = std::make_shared<Delay>();
    auto rv = std::make_shared<Reverb>();
    engine.add_effect(dl);
    engine.add_effect(rv);
    engine.set_input_gain(0.9f);
    engine.set_output_gain(0.7f);

    // Capture after
    std::vector<LoadPresetCommand::EffectSnapshot> after;
    for (auto& fx : engine.effects()) {
        LoadPresetCommand::EffectSnapshot snap;
        snap.effect = fx;
        snap.enabled = fx->is_enabled();
        snap.mix = fx->get_mix();
        for (auto& p : fx->params()) snap.param_values.push_back(p.value);
        after.push_back(std::move(snap));
    }
    float after_in = engine.get_input_gain();
    float after_out = engine.get_output_gain();

    CommandHistory history;
    history.push_executed(std::make_unique<LoadPresetCommand>(
        engine, std::move(before), before_in, before_out,
        std::move(after), after_in, after_out));

    // Currently: Delay + Reverb
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 2);
    ASSERT_EQ(std::string(engine.effects()[0]->name()), std::string("Delay"));
    ASSERT_EQ(std::string(engine.effects()[1]->name()), std::string("Reverb"));

    // Undo: back to Overdrive
    history.undo();
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 1);
    ASSERT_EQ(std::string(engine.effects()[0]->name()), std::string("Overdrive"));
    ASSERT_NEAR(engine.get_input_gain(), before_in, 0.001f);
    ASSERT_NEAR(engine.get_output_gain(), before_out, 0.001f);
    // Verify per-effect state was restored
    ASSERT_TRUE(engine.effects()[0]->is_enabled());
    ASSERT_NEAR(engine.effects()[0]->get_mix(), 1.0f, 0.01f);

    // Redo: back to Delay + Reverb
    history.redo();
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 2);
    ASSERT_EQ(std::string(engine.effects()[0]->name()), std::string("Delay"));
    ASSERT_EQ(std::string(engine.effects()[1]->name()), std::string("Reverb"));
    ASSERT_NEAR(engine.get_input_gain(), after_in, 0.001f);
    ASSERT_NEAR(engine.get_output_gain(), after_out, 0.001f);
    // Verify per-effect state after redo
    ASSERT_TRUE(engine.effects()[0]->is_enabled());
    ASSERT_TRUE(engine.effects()[1]->is_enabled());
}

// ==========================================================================
// CommandHistory stack behavior tests
// ==========================================================================

TEST(CommandHistory_MaxDepth) {
    auto& engine = test_engine();
    clear_engine(engine);

    CommandHistory history(5);  // max 5

    for (int i = 0; i < 10; ++i) {
        auto fx = std::make_shared<Overdrive>();
        history.execute(std::make_unique<AddEffectCommand>(engine, fx));
    }

    ASSERT_EQ(history.undo_size(), 5);

    // Clean up
    clear_engine(engine);
}

TEST(CommandHistory_RedoClearedOnNewAction) {
    auto& engine = test_engine();
    clear_engine(engine);

    CommandHistory history;
    history.execute(std::make_unique<AddEffectCommand>(engine, std::make_shared<Overdrive>()));
    history.execute(std::make_unique<AddEffectCommand>(engine, std::make_shared<Delay>()));

    history.undo();
    ASSERT_TRUE(history.can_redo());

    // New action should clear redo
    history.execute(std::make_unique<AddEffectCommand>(engine, std::make_shared<Reverb>()));
    ASSERT_FALSE(history.can_redo());

    clear_engine(engine);
}

TEST(CommandHistory_Clear) {
    auto& engine = test_engine();
    clear_engine(engine);

    CommandHistory history;
    history.execute(std::make_unique<AddEffectCommand>(engine, std::make_shared<Overdrive>()));
    history.execute(std::make_unique<AddEffectCommand>(engine, std::make_shared<Delay>()));

    history.clear();
    ASSERT_FALSE(history.can_undo());
    ASSERT_FALSE(history.can_redo());
    ASSERT_EQ(history.undo_size(), 0);
    ASSERT_EQ(history.redo_size(), 0);

    clear_engine(engine);
}

TEST(CommandHistory_UndoEmpty) {
    CommandHistory history;
    ASSERT_FALSE(history.undo());
}

TEST(CommandHistory_RedoEmpty) {
    CommandHistory history;
    ASSERT_FALSE(history.redo());
}

TEST(CommandHistory_MultipleUndoRedo) {
    auto& engine = test_engine();
    clear_engine(engine);

    CommandHistory history;
    history.execute(std::make_unique<AddEffectCommand>(engine, std::make_shared<Overdrive>()));
    history.execute(std::make_unique<AddEffectCommand>(engine, std::make_shared<Delay>()));
    history.execute(std::make_unique<AddEffectCommand>(engine, std::make_shared<Reverb>()));
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 3);

    // Undo all three
    history.undo();
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 2);
    history.undo();
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 1);
    history.undo();
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 0);

    // Redo all three
    history.redo();
    history.redo();
    history.redo();
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 3);
    ASSERT_EQ(std::string(engine.effects()[0]->name()), std::string("Overdrive"));
    ASSERT_EQ(std::string(engine.effects()[1]->name()), std::string("Delay"));
    ASSERT_EQ(std::string(engine.effects()[2]->name()), std::string("Reverb"));

    clear_engine(engine);
}

TEST(CommandHistory_Description) {
    auto& engine = test_engine();
    clear_engine(engine);

    CommandHistory history;
    ASSERT_TRUE(history.undo_description() == nullptr);
    ASSERT_TRUE(history.redo_description() == nullptr);

    history.execute(std::make_unique<AddEffectCommand>(engine, std::make_shared<Overdrive>()));
    ASSERT_TRUE(history.undo_description() != nullptr);
    ASSERT_EQ(std::string(history.undo_description()), std::string("Add Effect"));

    history.undo();
    ASSERT_TRUE(history.redo_description() != nullptr);
    ASSERT_EQ(std::string(history.redo_description()), std::string("Add Effect"));

    clear_engine(engine);
}

TEST(CommandHistory_EdgeCases_And_Out_Of_Bounds) {
    auto& engine = test_engine();
    clear_engine(engine);

    // 1. RemoveEffectCommand out-of-bounds indices
    {
        CommandHistory history;
        // Construct with out of bounds should not crash
        auto cmd1 = std::make_unique<RemoveEffectCommand>(engine, -1);
        ASSERT_EQ(cmd1->index(), -1);
        ASSERT_EQ(cmd1->effect(), nullptr);

        auto cmd2 = std::make_unique<RemoveEffectCommand>(engine, 100);
        ASSERT_EQ(cmd2->index(), 100);
        ASSERT_EQ(cmd2->effect(), nullptr);

        // Execute or undo on null captured effect must be a safe no-op
        cmd1->execute();
        cmd1->undo();
        cmd2->execute();
        cmd2->undo();
    }

    // 2. ParameterChangeCommand out-of-bounds indices
    {
        auto fx = std::make_shared<Overdrive>();
        CommandHistory history;

        // Construct with out of bounds indices
        auto cmd1 = std::make_unique<ParameterChangeCommand>(engine, fx, -1, 0.0f, 1.0f);
        auto cmd2 = std::make_unique<ParameterChangeCommand>(engine, fx, 999, 0.0f, 1.0f);
        
        ASSERT_EQ(cmd1->param_index(), -1);
        ASSERT_EQ(cmd2->param_index(), 999);
        ASSERT_EQ(cmd1->effect(), fx);

        // Execute/undo should return/handle out of bounds index safely (noop)
        cmd1->execute();
        cmd1->undo();
        cmd2->execute();
        cmd2->undo();
    }

    // 3. ParameterChangeCommand merge_with edge cases
    {
        auto fx1 = std::make_shared<Overdrive>();
        auto fx2 = std::make_shared<Delay>();
        
        ParameterChangeCommand cmd1(engine, fx1, 0, 0.0f, 0.5f);
        ParameterChangeCommand cmd_diff_fx(engine, fx2, 0, 0.0f, 0.5f);
        ParameterChangeCommand cmd_diff_param(engine, fx1, 1, 0.0f, 0.5f);
        
        // Mismatched command type
        AddEffectCommand dummy_cmd(engine, fx1);
        ASSERT_FALSE(cmd1.merge_with(dummy_cmd));

        // Different effect
        ASSERT_FALSE(cmd1.merge_with(cmd_diff_fx));

        // Different param index
        ASSERT_FALSE(cmd1.merge_with(cmd_diff_param));

        // Coalesce interval > 500 ms
        ParameterChangeCommand cmd_time_gap(engine, fx1, 0, 0.5f, 0.8f);
        // Artificially subtract time from timestamp to simulate 600ms gap
        cmd1.timestamp_ -= std::chrono::milliseconds(600);
        ASSERT_FALSE(cmd1.merge_with(cmd_time_gap));
    }

    // 4. ClearAllCommand and ResetAllCommand edge cases
    {
        engine.add_effect(std::make_shared<Overdrive>());
        engine.add_effect(std::make_shared<Delay>());
        
        CommandHistory history;
        // Reset All
        history.execute(std::make_unique<ResetAllCommand>(engine));
        ASSERT_EQ(std::string(engine.effects()[0]->name()), "Overdrive");
        history.undo();

        // Clear All
        history.execute(std::make_unique<ClearAllCommand>(engine));
        ASSERT_EQ(static_cast<int>(engine.effects().size()), 0);
        
        history.undo();
        ASSERT_EQ(static_cast<int>(engine.effects().size()), 2);
    }

    clear_engine(engine);
}

