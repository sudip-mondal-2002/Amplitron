#pragma once

#include "audio/engine/audio_engine.h"
#include "audio/effects/effect.h"
#include "gui/commands/command_base.h"
#include <vector>
#include <utility>

namespace Amplitron {

/**
 * @brief Command that captures the full effect-chain state before and after a
 *        preset load, enabling undo/redo of the entire preset switch.
 *
 * Uses AudioEngine::restore_effects_state() so the effect chain is replaced
 * atomically under the engine's mutex.
 */
class LoadPresetCommand : public Command {
public:
  /**
   * @brief Snapshot of a single effect's configuration at a point in time.
   */
  struct EffectSnapshot {
    std::shared_ptr<Effect> effect;  ///< The effect instance.
    bool enabled;                    ///< Whether the effect was enabled.
    float mix;                       ///< Dry/wet mix level.
    std::vector<float> param_values; ///< Ordered parameter values.
  };

  /**
   * @brief Construct a LoadPresetCommand.
   * @param engine             Reference to the audio engine.
   * @param before_state       Effect chain snapshot before the load.
   * @param before_input_gain  Input gain before the load.
   * @param before_output_gain Output gain before the load.
   * @param after_state        Effect chain snapshot after the load.
   * @param after_input_gain   Input gain after the load.
   * @param after_output_gain  Output gain after the load.
   */
  LoadPresetCommand(AudioEngine &engine,
                    std::vector<EffectSnapshot> before_state,
                    float before_input_gain, float before_output_gain,
                    std::vector<EffectSnapshot> after_state,
                    float after_input_gain, float after_output_gain)
      : engine_(engine), before_state_(std::move(before_state)),
        before_input_gain_(before_input_gain),
        before_output_gain_(before_output_gain),
        after_state_(std::move(after_state)),
        after_input_gain_(after_input_gain),
        after_output_gain_(after_output_gain) {}

  /** @brief Restore the after-load state (redo). */
  bool execute() override {
    apply_state(after_state_, after_input_gain_, after_output_gain_);
    return true;
  }

  /** @brief Restore the before-load state (undo). */
  void undo() override {
    apply_state(before_state_, before_input_gain_, before_output_gain_);
  }

  /** @brief Return "Load Preset". */
  const char *description() const override { return "Load Preset"; }

private:
  /**
   * @brief Replace the engine's effect chain and gains with the given snapshot.
   * @param state        Vector of EffectSnapshot to restore.
   * @param input_gain   Input gain to set.
   * @param output_gain  Output gain to set.
   */
  void apply_state(const std::vector<EffectSnapshot> &state, float input_gain,
                   float output_gain) {
    // Prepare effect list with restored params before swapping
    std::vector<std::shared_ptr<Effect>> new_effects;
    new_effects.reserve(state.size());
    for (auto &snap : state) {
      snap.effect->set_enabled(snap.enabled);
      snap.effect->set_mix(snap.mix);
      auto &params = snap.effect->params();
      for (int i = 0; i < static_cast<int>(params.size()) &&
                      i < static_cast<int>(snap.param_values.size());
           ++i) {
        params[i].value = snap.param_values[i];
      }
      new_effects.push_back(snap.effect);
    }

    // Atomic swap under engine lock — audio thread never sees half-applied
    // state
    engine_.restore_effects_state(std::move(new_effects));

    engine_.set_input_gain(input_gain);
    engine_.set_output_gain(output_gain);
  }

  AudioEngine &engine_;
  std::vector<EffectSnapshot> before_state_;
  float before_input_gain_;
  float before_output_gain_;
  std::vector<EffectSnapshot> after_state_;
  float after_input_gain_;
  float after_output_gain_;
};

/**
 * @brief Command that applies a stored in-session snapshot (A/B/C/D) to the
 * engine.
 *
 * Semantically identical to LoadPresetCommand but describes itself as
 * "Recall Snapshot" in the undo/redo menu. Constructed from a pair of
 * SnapshotManager::BoardSnapshot values (before/after) by GuiSnapshots.
 */
class RecallSnapshotCommand : public Command {
public:
  using EffectSnapshot = LoadPresetCommand::EffectSnapshot;

  RecallSnapshotCommand(AudioEngine &engine,
                        std::vector<EffectSnapshot> before_effects,
                        float before_input_gain, float before_output_gain,
                        std::vector<EffectSnapshot> after_effects,
                        float after_input_gain, float after_output_gain)
      : engine_(engine), before_effects_(std::move(before_effects)),
        before_input_gain_(before_input_gain),
        before_output_gain_(before_output_gain),
        after_effects_(std::move(after_effects)),
        after_input_gain_(after_input_gain),
        after_output_gain_(after_output_gain) {}

  /** @brief Restore the after-recall state (redo). */
  bool execute() override {
    apply_state(after_effects_, after_input_gain_, after_output_gain_);
    return true;
  }

  /** @brief Restore the before-recall state (undo). */
  void undo() override {
    apply_state(before_effects_, before_input_gain_, before_output_gain_);
  }

  /** @brief Return "Recall Snapshot". */
  const char *description() const override { return "Recall Snapshot"; }

private:
  void apply_state(const std::vector<EffectSnapshot> &state, float input_gain,
                   float output_gain) {
    std::vector<std::shared_ptr<Effect>> new_effects;
    new_effects.reserve(state.size());
    for (const auto &snap : state) {
      snap.effect->set_enabled(snap.enabled);
      snap.effect->set_mix(snap.mix);
      auto &params = snap.effect->params();
      for (int i = 0; i < static_cast<int>(params.size()) &&
                      i < static_cast<int>(snap.param_values.size());
           ++i) {
        params[i].value = snap.param_values[i];
      }
      new_effects.push_back(snap.effect);
    }
    engine_.restore_effects_state(std::move(new_effects));
    engine_.set_input_gain(input_gain);
    engine_.set_output_gain(output_gain);
  }

  AudioEngine &engine_;
  std::vector<EffectSnapshot> before_effects_;
  float before_input_gain_;
  float before_output_gain_;
  std::vector<EffectSnapshot> after_effects_;
  float after_input_gain_;
  float after_output_gain_;
};

} // namespace Amplitron
