#include "plugin/amplitron_plugin_processor.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <nlohmann/json.hpp>

namespace Amplitron {

AmplitronPluginProcessor::AmplitronPluginProcessor() {
    parameters_.push_back({{0, "Input Gain", "Global", "dB", -24.0f, 24.0f, 0.0f}, 0.0f});
    parameters_.push_back({{1, "Output Gain", "Global", "dB", -24.0f, 24.0f, 0.0f}, 0.0f});
    parameters_.push_back({{2, "Bypass", "Global", "", 0.0f, 1.0f, 0.0f}, 0.0f});
}

void AmplitronPluginProcessor::prepare(double sample_rate, uint32_t max_block_size) {
    sample_rate_ = sample_rate > 0.0 ? sample_rate : 48000.0;
    max_block_size_ = max_block_size > 0 ? max_block_size : 512;
    reset();
}

void AmplitronPluginProcessor::reset() {
    // Reserved for future effect-chain state reset.
}

uint32_t AmplitronPluginProcessor::parameter_count() const {
    return static_cast<uint32_t>(parameters_.size());
}

PluginParameterInfo AmplitronPluginProcessor::parameter_info(uint32_t index) const {
    if (index >= parameters_.size()) {
        return {};
    }

    return parameters_[index].info;
}

float AmplitronPluginProcessor::get_parameter_normalized(uint32_t index) const {
    if (index >= parameters_.size()) {
        return 0.0f;
    }

    const auto& parameter = parameters_[index];
    return normalize(parameter.value, parameter.info.min_value, parameter.info.max_value);
}

void AmplitronPluginProcessor::set_parameter_normalized(uint32_t index, float normalized_value) {
    if (index >= parameters_.size()) {
        return;
    }

    auto& parameter = parameters_[index];
    parameter.value = denormalize(
        clamp01(normalized_value),
        parameter.info.min_value,
        parameter.info.max_value
    );
}

void AmplitronPluginProcessor::process(const float* const* inputs,
                                       float* const* outputs,
                                       uint32_t frames,
                                       uint32_t input_channels,
                                       uint32_t output_channels) {
    if (outputs == nullptr || frames == 0 || output_channels == 0) {
        return;
    }

    const bool bypass = parameters_.size() > 2 && parameters_[2].value >= 0.5f;
    (void)bypass;

    for (uint32_t channel = 0; channel < output_channels; ++channel) {
        float* out = outputs[channel];

        if (out == nullptr) {
            continue;
        }

        const float* in = nullptr;

        if (inputs != nullptr && input_channels > 0) {
            in = inputs[std::min(channel, input_channels - 1)];
        }

        for (uint32_t frame = 0; frame < frames; ++frame) {
            const float sample = in != nullptr ? in[frame] : 0.0f;

            // Phase 1: safe pass-through block processor.
            // Next phase will route this through Amplitron's effect chain.
            out[frame] = std::isfinite(sample) ? sample : 0.0f;
        }
    }
}

std::string AmplitronPluginProcessor::save_state_json() const {
    nlohmann::json state;
    state["version"] = 1;
    state["sample_rate"] = sample_rate_;
    state["max_block_size"] = max_block_size_;
    state["parameters"] = nlohmann::json::array();

    for (const auto& parameter : parameters_) {
        state["parameters"].push_back({
            {"id", parameter.info.id},
            {"name", parameter.info.name},
            {"module", parameter.info.module},
            {"value", parameter.value}
        });
    }

    return state.dump();
}

bool AmplitronPluginProcessor::load_state_json(const std::string& state_text) {
    try {
        const auto state = nlohmann::json::parse(state_text);

        if (!state.contains("parameters") || !state["parameters"].is_array()) {
            return false;
        }

        for (const auto& saved_parameter : state["parameters"]) {
            const uint32_t id = saved_parameter.value(
                "id",
                std::numeric_limits<uint32_t>::max()
            );

            const float value = saved_parameter.value("value", 0.0f);

            for (auto& parameter : parameters_) {
                if (parameter.info.id == id) {
                    parameter.value = std::clamp(
                        value,
                        parameter.info.min_value,
                        parameter.info.max_value
                    );
                    break;
                }
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

float AmplitronPluginProcessor::clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float AmplitronPluginProcessor::denormalize(float normalized,
                                            float min_value,
                                            float max_value) {
    return min_value + normalized * (max_value - min_value);
}

float AmplitronPluginProcessor::normalize(float value,
                                          float min_value,
                                          float max_value) {
    if (std::abs(max_value - min_value) < 1.0e-6f) {
        return 0.0f;
    }

    return clamp01((value - min_value) / (max_value - min_value));
}

} // namespace Amplitron
