#pragma once

#include "gui/commands/command_base.h"
#include "audio/engine/audio_engine.h"
#include "audio/effects/effect.h"
#include <chrono>

namespace Amplitron {

/**
 * @brief Command that records a single parameter value change on an effect.
 *
 * Supports coalescing: rapid changes to the same parameter within 500 ms are
 * merged into one undo step. Uses shared_ptr<Effect> directly so it is robust
 * against effect chain reordering.
 */
class ParameterChangeCommand : public Command {
public:
    /**
     * @brief Construct a ParameterChangeCommand.
     * @param engine      Reference to the AudioEngine.
     * @param effect      Shared pointer to the target effect.
     * @param param_index Index of the parameter within the effect's param list.
     * @param old_value   Value before the change (used by undo).
     * @param new_value   Value after the change (used by execute).
     */
    ParameterChangeCommand(AudioEngine& engine, std::shared_ptr<Effect> effect,
                           int param_index, float old_value, float new_value)
        : engine_(engine), effect_(std::move(effect)),
          param_index_(param_index), old_value_(old_value), new_value_(new_value) {}

    /** @brief Set the parameter to new_value_. */
    bool execute() override {
        auto& params = effect_->params();
        if (param_index_ >= 0 && param_index_ < static_cast<int>(params.size())) {
            params[param_index_].value = new_value_;
            int idx = -1;
            auto& fx = engine_.effects();
            for (int i = 0; i < static_cast<int>(fx.size()); ++i) {
                if (fx[i] == effect_) { idx = i; break; }
            }
            if (idx >= 0) engine_.push_param_change(idx, param_index_, new_value_);
        }
        return true;
    }

    /** @brief Restore the parameter to old_value_. */
    void undo() override {
        auto& params = effect_->params();
        if (param_index_ >= 0 && param_index_ < static_cast<int>(params.size())) {
            params[param_index_].value = old_value_;
            int idx = -1;
            auto& fx = engine_.effects();
            for (int i = 0; i < static_cast<int>(fx.size()); ++i) {
                if (fx[i] == effect_) { idx = i; break; }
            }
            if (idx >= 0) engine_.push_param_change(idx, param_index_, old_value_);
        }
    }

    /** @brief Return "Change Parameter". */
    const char* description() const override { return "Change Parameter"; }

    /**
     * @brief Attempt to coalesce @p other into this command.
     *
     * Merges if @p other is a ParameterChangeCommand targeting the same
     * effect and parameter index within 500 ms. On success, this command's
     * new_value_ and timestamp are updated; old_value_ is preserved.
     *
     * @return true if @p other was absorbed.
     */
    bool merge_with(const Command& other) override {
        auto* pc = dynamic_cast<const ParameterChangeCommand*>(&other);
        if (!pc) return false;
        if (pc->effect_.get() != effect_.get() || pc->param_index_ != param_index_)
            return false;

        auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(
            pc->timestamp_ - timestamp_);
        if (dt.count() > 500) return false;

        new_value_ = pc->new_value_;
        timestamp_ = pc->timestamp_;
        return true;
    }

    /** @brief Accessor for the target effect. */
    std::shared_ptr<Effect> effect() const { return effect_; }

    /** @brief Index of the parameter being changed. */
    int param_index() const { return param_index_; }

    /** @brief Value before the change. */
    float old_value() const { return old_value_; }

    /** @brief Value after the change. */
    float new_value() const { return new_value_; }

private:
    AudioEngine& engine_;
    std::shared_ptr<Effect> effect_;
    int param_index_;
    float old_value_;
    float new_value_;
};

} // namespace Amplitron
