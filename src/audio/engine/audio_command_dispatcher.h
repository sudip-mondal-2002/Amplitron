#pragma once

#include "audio/utils/spsc_queue.h"
#include <memory>
#include <atomic>
#include <vector>

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

    void push_param_change(int effect_index, int param_index, float value);
    void push_mixer_gain_change(int node_id, int pin_index, float gain);
    void push_effect_enabled(int effect_index, float enabled);
    void push_effect_mix(int effect_index, float mix);
    void push_input_gain(float gain);
    void push_output_gain(float gain);

    // Audio thread side
    void drain_gain_commands(std::atomic<float>& input_gain,
                             std::atomic<float>& output_gain,
                             std::shared_ptr<AudioGraphExecutor>& executor);

    void drain_commands(std::atomic<float>& input_gain,
                        std::atomic<float>& output_gain,
                        std::shared_ptr<AudioGraphExecutor>& executor,
                        AudioGraph& main_graph,
                        std::vector<std::shared_ptr<Effect>>& dummy_effects);

private:
    SPSCQueue<AudioCommand, 256> command_queue_;
};

} // namespace Amplitron
