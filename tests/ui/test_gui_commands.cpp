/**
 * @file test_gui_commands.cpp
 * @brief Tests for ResetAllCommand and ClearAllCommand undo/redo behaviour.
 *
 * These command objects are GUI-layer undo-able actions but contain only pure
 * logic — no ImGui rendering required.
 */
#include "test_framework.h"
#include "test_fixtures.h"
#include "gui/commands/command_reset.h"
#include "gui/commands/command_clear.h"
#include "gui/commands/command_history.h"
#include "audio/effects/overdrive.h"
#include "audio/effects/reverb.h"
#include <memory>
#include <string>

using namespace Amplitron;

// ============================================================
// ResetAllCommand
// ============================================================

TEST_F(AudioEngineTest, reset_all_command_resets_params_to_default) {
    auto od = std::make_shared<Overdrive>();
    od->params()[0].value = od->params()[0].min_val;   // move away from default
    engine.add_effect(od);

    CommandHistory history;
    history.execute(std::make_unique<ResetAllCommand>(engine));

    ASSERT_NEAR(od->params()[0].value, od->params()[0].default_val, 0.001f);
}

TEST_F(AudioEngineTest, reset_all_command_is_undoable) {
    auto od = std::make_shared<Overdrive>();
    float original = od->params()[0].min_val;
    od->params()[0].value = original;
    engine.add_effect(od);

    CommandHistory history;
    history.execute(std::make_unique<ResetAllCommand>(engine));
    ASSERT_NEAR(od->params()[0].value, od->params()[0].default_val, 0.001f);

    history.undo();
    ASSERT_NEAR(od->params()[0].value, original, 0.001f);
}

TEST_F(AudioEngineTest, reset_all_command_is_redoable) {
    auto od = std::make_shared<Overdrive>();
    od->params()[0].value = od->params()[0].min_val;
    engine.add_effect(od);

    CommandHistory history;
    history.execute(std::make_unique<ResetAllCommand>(engine));
    history.undo();
    history.redo();

    ASSERT_NEAR(od->params()[0].value, od->params()[0].default_val, 0.001f);
}

TEST_F(AudioEngineTest, reset_all_command_multi_effect) {
    auto od  = std::make_shared<Overdrive>();
    auto rev = std::make_shared<Reverb>();
    od->params()[0].value  = od->params()[0].min_val;
    rev->params()[0].value = rev->params()[0].min_val;
    engine.add_effect(od);
    engine.add_effect(rev);

    CommandHistory history;
    history.execute(std::make_unique<ResetAllCommand>(engine));

    ASSERT_NEAR(od->params()[0].value,  od->params()[0].default_val,  0.001f);
    ASSERT_NEAR(rev->params()[0].value, rev->params()[0].default_val, 0.001f);
}

TEST_F(AudioEngineTest, reset_all_command_empty_engine_safe) {
    // No effects — ResetAllCommand must be a no-op and not crash
    CommandHistory history;
    history.execute(std::make_unique<ResetAllCommand>(engine));
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 0);
}

// ============================================================
// ClearAllCommand
// ============================================================

TEST_F(AudioEngineTest, clear_all_command_removes_all_effects) {
    engine.add_effect(std::make_shared<Overdrive>());
    engine.add_effect(std::make_shared<Reverb>());
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 2);

    CommandHistory history;
    history.execute(std::make_unique<ClearAllCommand>(engine));
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 0);
}

TEST_F(AudioEngineTest, clear_all_command_is_undoable) {
    engine.add_effect(std::make_shared<Overdrive>());
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 1);

    CommandHistory history;
    history.execute(std::make_unique<ClearAllCommand>(engine));
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 0);

    history.undo();
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 1);
    ASSERT_EQ(std::string(engine.effects()[0]->name()), "Overdrive");
}

TEST_F(AudioEngineTest, clear_all_command_is_redoable) {
    engine.add_effect(std::make_shared<Overdrive>());

    CommandHistory history;
    history.execute(std::make_unique<ClearAllCommand>(engine));
    history.undo();
    history.redo();

    ASSERT_EQ(static_cast<int>(engine.effects().size()), 0);
}

TEST_F(AudioEngineTest, clear_all_command_restores_effect_order) {
    auto od  = std::make_shared<Overdrive>();
    auto rev = std::make_shared<Reverb>();
    engine.add_effect(od);
    engine.add_effect(rev);

    CommandHistory history;
    history.execute(std::make_unique<ClearAllCommand>(engine));
    history.undo();

    ASSERT_EQ(static_cast<int>(engine.effects().size()), 2);
    ASSERT_EQ(std::string(engine.effects()[0]->name()), "Overdrive");
    ASSERT_EQ(std::string(engine.effects()[1]->name()), "Reverb");
}

TEST_F(AudioEngineTest, clear_all_command_empty_engine_safe) {
    CommandHistory history;
    history.execute(std::make_unique<ClearAllCommand>(engine));
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 0);
    // Undo of a clear on empty chain must also be safe
    history.undo();
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 0);
}
