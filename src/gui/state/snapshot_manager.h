#pragma once

#include "common.h"
#include "audio/engine/audio_engine.h"
#include "audio/effects/effect.h"
#include "gui/commands/command.h"
#include <array>
#include <optional>
#include <vector>

namespace Amplitron {

/**
 * @brief In-memory snapshot manager for A/B/C/D board state switching.
 *
 * Stores up to NUM_SLOTS complete board configurations (effect chain + gains)
 * in memory for instant recall during performance, without file I/O. Each slot
 * captures the same state as a preset — effect instances, enabled/mix flags,
 * and all parameter values — using the LoadPresetCommand::EffectSnapshot pattern
 * so that recall integrates cleanly with the undo/redo system.
 */
class SnapshotManager {
public:
    static constexpr int NUM_SLOTS = 4;

    /** @brief Display labels for each slot. */
    static constexpr const char* SLOT_LABELS[NUM_SLOTS] = {"A", "B", "C", "D"};

    /**
     * @brief Complete board state captured at a point in time.
     *
     * Mirrors the data captured by LoadPresetCommand so that RecallSnapshotCommand
     * can restore it using the same atomic engine swap.
     */
    struct BoardSnapshot {
        std::vector<LoadPresetCommand::EffectSnapshot> effects;
        float input_gain  = 0.7f;
        float output_gain = 0.8f;
    };

    // ------------------------------------------------------------------
    // Slot management
    // ------------------------------------------------------------------

    /** @brief Capture the current engine state into the given slot (0–3). */
    void save_slot(int slot, AudioEngine& engine) {
        if (slot < 0 || slot >= NUM_SLOTS) return;
        slots_[slot] = capture(engine);
    }

    /** @brief True if the slot contains a saved snapshot. */
    bool has_slot(int slot) const {
        if (slot < 0 || slot >= NUM_SLOTS) return false;
        return slots_[slot].has_value();
    }

    /**
     * @brief Apply the stored snapshot directly to the engine (no undo/redo).
     *
     * For use in tests and headless scenarios. GUI code should call
     * GuiSnapshots::recall_slot() instead to get undo/redo support.
     */
    void recall_slot_direct(int slot, AudioEngine& engine) {
        if (slot < 0 || slot >= NUM_SLOTS) return;
        if (!slots_[slot].has_value()) return;
        apply(*slots_[slot], engine);
        active_slot_ = slot;
    }

    /** @brief Return a pointer to the stored snapshot, or nullptr if the slot is empty. */
    const BoardSnapshot* get_slot(int slot) const {
        if (slot < 0 || slot >= NUM_SLOTS) return nullptr;
        return slots_[slot].has_value() ? &(*slots_[slot]) : nullptr;
    }

    /** @brief Clear the stored snapshot from a slot. */
    void clear_slot(int slot) {
        if (slot < 0 || slot >= NUM_SLOTS) return;
        slots_[slot].reset();
        if (active_slot_ == slot) active_slot_ = -1;
    }

    // ------------------------------------------------------------------
    // Active slot tracking
    // ------------------------------------------------------------------

    /** @brief Index of the most recently recalled slot (-1 = none active). */
    int active_slot() const { return active_slot_; }

    /** @brief Mark a slot as active (called after save or recall). */
    void set_active_slot(int slot) { active_slot_ = slot; }

    // ------------------------------------------------------------------
    // Static helpers (shared with RecallSnapshotCommand)
    // ------------------------------------------------------------------

    /** @brief Capture the current engine state as a BoardSnapshot. */
    static BoardSnapshot capture(AudioEngine& engine) {
        BoardSnapshot snap;
        snap.input_gain  = engine.get_input_gain();
        snap.output_gain = engine.get_output_gain();

        for (auto& fx : engine.effects()) {
            LoadPresetCommand::EffectSnapshot es;
            es.effect  = fx;
            es.enabled = fx->is_enabled();
            es.mix     = fx->get_mix();
            for (auto& p : fx->params()) {
                es.param_values.push_back(p.value);
            }
            snap.effects.push_back(std::move(es));
        }
        return snap;
    }

    /**
     * @brief Apply a BoardSnapshot to the engine.
     *
     * Restores all effect enabled/mix/param states and performs an atomic
     * effect chain swap via AudioEngine::restore_effects_state so the audio
     * thread never sees a partial state.
     */
    static void apply(const BoardSnapshot& snap, AudioEngine& engine) {
        std::vector<std::shared_ptr<Effect>> new_effects;
        new_effects.reserve(snap.effects.size());

        for (const auto& es : snap.effects) {
            es.effect->set_enabled(es.enabled);
            es.effect->set_mix(es.mix);
            auto& params = es.effect->params();
            for (int i = 0; i < static_cast<int>(params.size()) &&
                            i < static_cast<int>(es.param_values.size()); ++i) {
                params[i].value = es.param_values[i];
            }
            new_effects.push_back(es.effect);
        }

        engine.restore_effects_state(std::move(new_effects));
        engine.set_input_gain(snap.input_gain);
        engine.set_output_gain(snap.output_gain);
    }

private:
    std::array<std::optional<BoardSnapshot>, NUM_SLOTS> slots_;
    int active_slot_ = -1;
};

} // namespace Amplitron
