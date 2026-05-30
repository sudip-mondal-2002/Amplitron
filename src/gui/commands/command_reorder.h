#pragma once

#include "audio/engine/audio_engine.h"
#include "gui/commands/command_base.h"

namespace Amplitron {

/**
 * @brief Command that moves an effect from one chain position to another.
 *
 * execute() calls move_effect(from, to); undo() reverses the move.
 */
class ReorderEffectCommand : public Command {
public:
  /**
   * @brief Construct a ReorderEffectCommand.
   * @param engine  Reference to the audio engine.
   * @param from    Source index in the effect chain.
   * @param to      Destination index in the effect chain.
   */
  ReorderEffectCommand(AudioEngine &engine, int from, int to)
      : engine_(engine), from_(from), to_(to) {}

  /** @brief Move the effect from source to destination index. */
  bool execute() override {
    engine_.move_effect(from_, to_);
    return true;
  }

  /** @brief Move the effect back from destination to source index. */
  void undo() override { engine_.move_effect(to_, from_); }

  /** @brief Return "Reorder Effect". */
  const char *description() const override { return "Reorder Effect"; }

  /** @brief Original source index. */
  int from() const { return from_; }

  /** @brief Destination index. */
  int to() const { return to_; }

private:
  AudioEngine &engine_;
  int from_;
  int to_;
};

} // namespace Amplitron
