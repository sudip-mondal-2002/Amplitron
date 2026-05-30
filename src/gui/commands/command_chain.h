#pragma once

#include "gui/commands/command_base.h"
#include "audio/engine/audio_engine.h"
#include "audio/effects/effect.h"
#include <cstring>
#include <algorithm>

namespace Amplitron {

/**
 * @brief Command that appends an effect to the engine's signal chain.
 *
 * execute() adds the effect; undo() finds and removes it.
 */
class AddEffectCommand : public Command {
public:
    /**
     * @brief Construct an AddEffectCommand.
     * @param engine  Reference to the audio engine that owns the effect chain.
     * @param effect  Shared pointer to the effect to add.
     */
    AddEffectCommand(AudioEngine& engine, std::shared_ptr<Effect> effect)
        : engine_(engine), effect_(std::move(effect)) {}

    /** @brief Append the effect to the engine's chain (before the amp if present). */
    bool execute() override {
        int amp_idx = -1;
        auto& fx = engine_.effects();
        for (int i = 0; i < static_cast<int>(fx.size()); ++i) {
            if (std::strcmp(fx[i]->name(), "Amp Sim") == 0) {
                amp_idx = i;
                break;
            }
        }
        if (amp_idx >= 0) {
            engine_.insert_effect(amp_idx, effect_);
        } else {
            engine_.add_effect(effect_);
        }
        return true;
    }

    /** @brief Remove the previously added effect from the chain. */
    void undo() override {
        auto& fx = engine_.effects();
        for (int i = static_cast<int>(fx.size()) - 1; i >= 0; --i) {
            if (fx[i] == effect_) {
                engine_.remove_effect(i);
                return;
            }
        }
    }

    /** @brief Return "Add Effect". */
    const char* description() const override { return "Add Effect"; }

    /** @brief Accessor for the wrapped effect. */
    std::shared_ptr<Effect> effect() const { return effect_; }

private:
    AudioEngine& engine_;
    std::shared_ptr<Effect> effect_;
};

/**
 * @brief Command that removes an effect from the engine's signal chain.
 *
 * The constructor captures the effect pointer before removal so undo() can
 * re-insert it at its original position.
 */
class RemoveEffectCommand : public Command {
public:
    /**
     * @brief Construct a RemoveEffectCommand.
     * @param engine  Reference to the audio engine.
     * @param index   Chain index of the effect to remove (captured at construction).
     */
    RemoveEffectCommand(AudioEngine& engine, int index)
        : engine_(engine), index_(index) {
        auto& fx = engine_.effects();
        if (index >= 0 && index < static_cast<int>(fx.size())) {
            effect_ = fx[index];
        }
    }

    /** @brief Remove the effect at the stored index. */
    bool execute() override {
        engine_.remove_effect(index_);
        return true;
    }

    /** @brief Re-insert the captured effect at its original chain position. */
    void undo() override {
        if (effect_) {
            auto& fx = engine_.effects();
            int pos = std::min(index_, static_cast<int>(fx.size()));
            engine_.add_effect(effect_);
            int last = static_cast<int>(engine_.effects().size()) - 1;
            if (last != pos) {
                engine_.move_effect(last, pos);
            }
        }
    }

    /** @brief Return "Remove Effect". */
    const char* description() const override { return "Remove Effect"; }

    /** @brief Original chain index of the removed effect. */
    int index() const { return index_; }

    /** @brief Accessor for the captured effect pointer. */
    std::shared_ptr<Effect> effect() const { return effect_; }

private:
    AudioEngine& engine_;
    int index_;
    std::shared_ptr<Effect> effect_;
};

} // namespace Amplitron
