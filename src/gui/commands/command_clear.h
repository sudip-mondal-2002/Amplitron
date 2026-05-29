#pragma once

#include "gui/commands/command_base.h"
#include "audio/engine/audio_engine.h"
#include "audio/effects/effect.h"
#include <vector>

namespace Amplitron {

/**
 * @brief Command that removes every effect from the signal chain at once.
 *
 * Captures the full chain state before clearing so that undo() can restore it.
 */
class ClearAllCommand : public Command {
public:
    explicit ClearAllCommand(AudioEngine& engine)
        : engine_(engine) {
        for (auto& fx : engine_.effects()) {
            saved_.push_back(fx);
        }
    }

    bool execute() override {
        while (!engine_.effects().empty()) {
            engine_.remove_effect(static_cast<int>(engine_.effects().size()) - 1);
        }
        return true;
    }

    void undo() override {
        for (auto& fx : saved_) {
            engine_.add_effect(fx);
        }
    }

    const char* description() const override { return "Clear All"; }

private:
    AudioEngine& engine_;
    std::vector<std::shared_ptr<Effect>> saved_;
};

} // namespace Amplitron
