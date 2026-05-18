#pragma once

#include "gui/command_base.h"
#include "audio/effect.h"
#include <vector>

namespace Amplitron {

/**
 * @brief Command that resets all parameters of all effects to their default values.
 *
 * Captures the current parameter values before resetting so that undo() can restore them.
 */
class ResetAllCommand : public Command {
public:
    explicit ResetAllCommand(AudioEngine& engine)
        : engine_(engine) {
        auto& effects = engine_.effects();
        for (auto& fx : effects) {
            std::vector<float> values;
            for (const auto& param : fx->params()) {
                values.push_back(param.value);
            }
            saved_values_.push_back(values);
            saved_enabled_.push_back(fx->is_enabled());
        }
    }

    void execute() override {
        auto& effects = engine_.effects();
        for (auto& fx : effects) {
            fx->reset(); // Resets internal DSP state
            auto& p = fx->params();
            for (auto& param : p) {
                param.value = param.default_val;
            }
        }
    }

    void undo() override {
        auto& effects = engine_.effects();
        for (size_t i = 0; i < effects.size() && i < saved_values_.size(); ++i) {
            auto& fx = effects[i];
            auto& p = fx->params();
            auto& values = saved_values_[i];
            for (size_t j = 0; j < p.size() && j < values.size(); ++j) {
                p[j].value = values[j];
            }
            fx->set_enabled(saved_enabled_[i]);
        }
    }

    const char* description() const override { return "Reset All"; }

private:
    AudioEngine& engine_;
    std::vector<std::vector<float>> saved_values_;
    std::vector<bool> saved_enabled_;
};

} // namespace Amplitron
