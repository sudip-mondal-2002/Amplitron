#include "audio/engine/audio_command_dispatcher.h"
#include "audio/engine/audio_graph.h"
#include "audio/engine/audio_graph_executor.h"
#include "audio/effects/core/effect.h"
#include <iostream>

namespace Amplitron {

void AudioCommandDispatcher::push_param_change(int effect_index, int param_index, float value) {
    AudioCommand cmd{};
    cmd.type = AudioCommand::SetEffectParam;
    cmd.effect_index = effect_index;
    cmd.param_index = param_index;
    cmd.value = value;
    command_queue_.try_push(cmd);
}

void AudioCommandDispatcher::push_mixer_gain_change(int node_id, int pin_index, float gain) {
    AudioCommand cmd{};
    cmd.type = AudioCommand::SetMixerGain;
    cmd.effect_index = node_id; // Overload effect_index to mean node_id
    cmd.param_index = pin_index; // Overload param_index to mean pin_index
    cmd.value = gain;
    command_queue_.try_push(cmd);
}

void AudioCommandDispatcher::push_effect_enabled(int effect_index, float enabled) {
    AudioCommand cmd{};
    cmd.type = AudioCommand::SetEffectEnabled;
    cmd.effect_index = effect_index;
    cmd.value = enabled;
    command_queue_.try_push(cmd);
}

void AudioCommandDispatcher::push_effect_mix(int effect_index, float mix) {
    AudioCommand cmd{};
    cmd.type = AudioCommand::SetEffectMix;
    cmd.effect_index = effect_index;
    cmd.value = mix;
    command_queue_.try_push(cmd);
}

void AudioCommandDispatcher::push_input_gain(float gain) {
    AudioCommand cmd{};
    cmd.type = AudioCommand::SetInputGain;
    cmd.value = gain;
    command_queue_.try_push(cmd);
}

void AudioCommandDispatcher::push_output_gain(float gain) {
    AudioCommand cmd{};
    cmd.type = AudioCommand::SetOutputGain;
    cmd.value = gain;
    command_queue_.try_push(cmd);
}

void AudioCommandDispatcher::drain_gain_commands(std::atomic<float>& input_gain,
                                                 std::atomic<float>& output_gain,
                                                 std::shared_ptr<AudioGraphExecutor>& executor) {
    AudioCommand cmd;
    while (command_queue_.try_peek(cmd)) {
        if (cmd.type == AudioCommand::SetInputGain) {
            command_queue_.try_pop(cmd);
            input_gain.store(cmd.value, std::memory_order_relaxed);
        } else if (cmd.type == AudioCommand::SetOutputGain) {
            command_queue_.try_pop(cmd);
            output_gain.store(cmd.value, std::memory_order_relaxed);
        } else if (cmd.type == AudioCommand::SetMixerGain) {
            command_queue_.try_pop(cmd);
            if (executor) {
                executor->update_mixer_gain(cmd.effect_index, cmd.param_index, cmd.value);
            }
        } else {
            break;
        }
    }
}

void AudioCommandDispatcher::drain_commands(std::atomic<float>& input_gain,
                                            std::atomic<float>& output_gain,
                                            std::shared_ptr<AudioGraphExecutor>& executor,
                                            AudioGraph& main_graph,
                                            std::vector<std::shared_ptr<Effect>>& dummy_effects) {
    AudioCommand cmd;
    while (command_queue_.try_pop(cmd)) {
        // Helper to find the effect pointer safely in the compiled executor by node_id, with fallback
        auto get_effect_by_id = [&](int node_id) -> std::shared_ptr<Effect> {
            if (executor) {
                if (auto fx = executor->get_effect_by_node_id(node_id)) {
                    return fx;
                }
            }
            for (const auto& node : main_graph.get_nodes()) {
                if (node.id == node_id) {
                    return node.pedal;
                }
            }
            if (node_id >= 0 && node_id < static_cast<int>(dummy_effects.size())) {
                // Comments on node_id semantics: node_id is used as a 0-based linear index fallback for the GUI and tests
                std::cerr << "[AudioCommandDispatcher] Node ID " << node_id 
                          << " not found in executor or graph; falling back to dummy_effects index." << std::endl;
                return dummy_effects[node_id];
            }
            std::cerr << "[AudioCommandDispatcher] Node ID " << node_id << " lookup failed completely." << std::endl;
            return nullptr;
        };

        switch (cmd.type) {
            case AudioCommand::SetEffectParam: {
                if (auto fx = get_effect_by_id(cmd.effect_index)) {
                    auto& params = fx->params();
                    if (cmd.param_index >= 0 &&
                        cmd.param_index < static_cast<int>(params.size())) {
                        params[cmd.param_index].value = cmd.value;
                    }
                }
                break;
            }
            case AudioCommand::SetEffectEnabled: {
                if (auto fx = get_effect_by_id(cmd.effect_index)) {
                    fx->set_enabled(cmd.value > 0.5f);
                }
                break;
            }
            case AudioCommand::SetEffectMix: {
                if (auto fx = get_effect_by_id(cmd.effect_index)) {
                    fx->set_mix(cmd.value);
                }
                break;
            }
            case AudioCommand::SetMixerGain:
                // Skip SetMixerGain in the mutex-gated path, it is handled lock-free
                break;
            case AudioCommand::SetInputGain:
                input_gain.store(cmd.value, std::memory_order_relaxed);
                break;
            case AudioCommand::SetOutputGain:
                output_gain.store(cmd.value, std::memory_order_relaxed);
                break;
            default:
                break;
        }
    }
}

} // namespace Amplitron
