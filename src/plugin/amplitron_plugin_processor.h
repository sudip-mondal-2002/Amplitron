#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Amplitron {

struct PluginParameterInfo {
    uint32_t id{};
    std::string name;
    std::string module;
    std::string unit;
    float min_value{};
    float max_value{};
    float default_value{};
};

class AmplitronPluginProcessor {
public:
    AmplitronPluginProcessor();

    void prepare(double sample_rate, uint32_t max_block_size);
    void reset();

    uint32_t parameter_count() const;
    PluginParameterInfo parameter_info(uint32_t index) const;

    float get_parameter_normalized(uint32_t index) const;
    void set_parameter_normalized(uint32_t index, float normalized_value);

    void process(const float* const* inputs,
                 float* const* outputs,
                 uint32_t frames,
                 uint32_t input_channels,
                 uint32_t output_channels);

    std::string save_state_json() const;
    bool load_state_json(const std::string& state);

private:
    struct RuntimeParameter {
        PluginParameterInfo info;
        float value{};
    };

    double sample_rate_{48000.0};
    uint32_t max_block_size_{512};
    std::vector<RuntimeParameter> parameters_;

    static float clamp01(float value);
    static float denormalize(float normalized, float min_value, float max_value);
    static float normalize(float value, float min_value, float max_value);
};

} // namespace Amplitron
