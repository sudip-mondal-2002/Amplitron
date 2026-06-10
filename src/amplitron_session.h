#pragma once

#include <memory>
#include <stdexcept>

#include "audio/engine/audio_engine.h"
#include "audio/engine/i_audio_engine.h"
#include "gui/commands/command_history.h"
#include "gui/state/snapshot_manager.h"
#include "midi/i_midi_manager.h"
#include "midi/midi_manager.h"
#include "presets/i_preset_manager.h"
#include "presets/preset_manager.h"

namespace Amplitron {

/**
 * @brief Unified facade session class that holds and coordinates core subsystems.
 * Satisfies the Facade Pattern and the Dependency Inversion Principle (DIP).
 */
class AmplitronSession {
   public:
    AmplitronSession(
        std::unique_ptr<IAudioEngine> engine = std::make_unique<AudioEngine>(),
        std::unique_ptr<IMidiManager> midi = std::make_unique<MidiManager>(),
        std::unique_ptr<IPresetManager> presets = std::make_unique<PresetManagerService>()) {
        if (!engine) {
            throw std::invalid_argument("engine cannot be null");
        }
        if (!midi) {
            throw std::invalid_argument("midi cannot be null");
        }
        if (!presets) {
            throw std::invalid_argument("presets cannot be null");
        }
        engine_ = std::move(engine);
        midi_ = std::move(midi);
        presets_ = std::move(presets);
    }

    IAudioEngine& engine() { return *engine_; }
    IMidiManager& midi() { return *midi_; }
    IPresetManager& presets() { return *presets_; }

    AudioEngine& concrete_engine() {
        auto* p = dynamic_cast<AudioEngine*>(engine_.get());
        if (!p) {
            throw std::runtime_error("Engine is not a concrete AudioEngine");
        }
        return *p;
    }
    MidiManager& concrete_midi() {
        auto* p = dynamic_cast<MidiManager*>(midi_.get());
        if (!p) {
            throw std::runtime_error("MIDI is not a concrete MidiManager");
        }
        return *p;
    }

    CommandHistory& command_history() { return command_history_; }
    SnapshotManager& snapshot_manager() { return snapshot_manager_; }

   private:
    std::unique_ptr<IAudioEngine> engine_;
    std::unique_ptr<IMidiManager> midi_;
    std::unique_ptr<IPresetManager> presets_;
    CommandHistory command_history_;
    SnapshotManager snapshot_manager_;
};

}  // namespace Amplitron
