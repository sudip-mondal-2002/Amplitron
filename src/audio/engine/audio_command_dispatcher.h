#pragma once

#include <atomic>
#include <memory>
#include <vector>

#include "audio/utils/spsc_queue.h"

namespace Amplitron {

class Effect;
class AudioGraph;
class AudioGraphExecutor;

/**
 * @brief Dispatcher for dynamic parameters and UI commands sent to the audio thread.
 * Satisfies the Single Responsibility Principle (SRP).
 */
class AudioCommandDispatcher {
   public:
    AudioCommandDispatcher() = default;
    ~AudioCommandDispatcher() = default;

    /**
     * @brief Pushes a parameter change command for a specific effect.
     * @param effect_index The index of the effect in the signal chain.
     * @param param_index The index of the parameter to change.
     * @param value The new parameter value.
     */
    void push_param_change(int effect_index, int param_index, float value);

    /**
     * @brief Pushes a gain change command for a specific mixer node pin.
     * @param node_id The ID of the mixer node.
     * @param pin_index The index of the input pin on the mixer.
     * @param gain The new gain multiplier.
     */
    void push_mixer_gain_change(int node_id, int pin_index, float gain);

    /**
     * @brief Pushes an enable/disable command for a specific effect.
     * @param effect_index The index of the effect in the signal chain.
     * @param enabled 1.0f to enable, 0.0f to disable.
     */
    void push_effect_enabled(int effect_index, float enabled);

    /**
     * @brief Pushes a mix (dry/wet) change command for a specific effect.
     * @param effect_index The index of the effect in the signal chain.
     * @param mix The new mix ratio (0.0f to 1.0f).
     */
    void push_effect_mix(int effect_index, float mix);

    /**
     * @brief Pushes a global input gain change command.
     * @param gain The new input gain multiplier.
     */
    void push_input_gain(float gain);

    /**
     * @brief Pushes a global output gain change command.
     * @param gain The new output gain multiplier.
     */
    void push_output_gain(float gain);

    /**
     * @brief Drains only the global gain commands from the queue, applying them directly.
     * @param input_gain Reference to the atomic input gain variable to update.
     * @param output_gain Reference to the atomic output gain variable to update.
     * @param executor Shared pointer to the audio graph executor to handle mixer node gains.
     */
    void drain_gain_commands(std::atomic<float>& input_gain, std::atomic<float>& output_gain,
                             std::shared_ptr<AudioGraphExecutor>& executor);

    /**
     * @brief Drains all commands from the queue and applies them to the audio graph and effects.
     * @param input_gain Reference to the atomic input gain variable to update.
     * @param output_gain Reference to the atomic output gain variable to update.
     * @param executor Shared pointer to the audio graph executor.
     * @param main_graph Reference to the main audio graph.
     * @param dummy_effects Fallback list of effects used for legacy sequential routing.
     */
    void drain_commands(std::atomic<float>& input_gain, std::atomic<float>& output_gain,
                        std::shared_ptr<AudioGraphExecutor>& executor, AudioGraph& main_graph,
                        std::vector<std::shared_ptr<Effect>>& dummy_effects);

    /**
     * @brief Polls and atomically resets the error flag.
     * @return true if an error was logged since the last check, false otherwise.
     */
    bool check_and_clear_error() {
        return error_flag_.exchange(false, std::memory_order_acq_rel);
    }

   private:
    SPSCQueue<AudioCommand, 256> command_queue_;
    std::atomic<bool> error_flag_{false};
};

}  // namespace Amplitron
