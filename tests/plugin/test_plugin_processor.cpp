#include "plugin/amplitron_plugin_processor.h"

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

using Amplitron::AmplitronPluginProcessor;

TEST(PluginProcessor, ExposesDefaultParameters) {
    AmplitronPluginProcessor processor;
    processor.prepare(48000.0, 512);

    EXPECT_GT(processor.parameter_count(), 0u);

    const auto info = processor.parameter_info(0);
    EXPECT_FALSE(info.name.empty());
    EXPECT_FALSE(info.module.empty());
    EXPECT_LE(info.min_value, info.default_value);
    EXPECT_GE(info.max_value, info.default_value);
}

TEST(PluginProcessor, NormalizedParameterMappingClampsLowValues) {
    AmplitronPluginProcessor processor;
    processor.prepare(48000.0, 512);

    processor.set_parameter_normalized(0, -10.0f);

    EXPECT_FLOAT_EQ(processor.get_parameter_normalized(0), 0.0f);
}

TEST(PluginProcessor, NormalizedParameterMappingClampsHighValues) {
    AmplitronPluginProcessor processor;
    processor.prepare(48000.0, 512);

    processor.set_parameter_normalized(0, 10.0f);

    EXPECT_FLOAT_EQ(processor.get_parameter_normalized(0), 1.0f);
}

TEST(PluginProcessor, ProcessesStereoAudioWithoutNaN) {
    AmplitronPluginProcessor processor;
    processor.prepare(48000.0, 512);

    std::vector<float> input_left(512, 0.1f);
    std::vector<float> input_right(512, -0.1f);
    std::vector<float> output_left(512, 0.0f);
    std::vector<float> output_right(512, 0.0f);

    const float* inputs[] = {input_left.data(), input_right.data()};
    float* outputs[] = {output_left.data(), output_right.data()};

    processor.process(inputs, outputs, 512, 2, 2);

    for (float sample : output_left) {
        EXPECT_TRUE(std::isfinite(sample));
    }

    for (float sample : output_right) {
        EXPECT_TRUE(std::isfinite(sample));
    }
}

TEST(PluginProcessor, HandlesNullInputAsSilence) {
    AmplitronPluginProcessor processor;
    processor.prepare(48000.0, 128);

    std::vector<float> output_left(128, 1.0f);
    std::vector<float> output_right(128, 1.0f);

    float* outputs[] = {output_left.data(), output_right.data()};

    processor.process(nullptr, outputs, 128, 0, 2);

    for (float sample : output_left) {
        EXPECT_FLOAT_EQ(sample, 0.0f);
    }

    for (float sample : output_right) {
        EXPECT_FLOAT_EQ(sample, 0.0f);
    }
}

TEST(PluginProcessor, StateRoundTripRestoresParameters) {
    AmplitronPluginProcessor processor;
    processor.prepare(48000.0, 512);
    processor.set_parameter_normalized(0, 0.75f);
    processor.set_parameter_normalized(1, 0.25f);

    const std::string state = processor.save_state_json();

    AmplitronPluginProcessor restored;
    restored.prepare(44100.0, 256);

    ASSERT_TRUE(restored.load_state_json(state));
    EXPECT_NEAR(restored.get_parameter_normalized(0), 0.75f, 0.001f);
    EXPECT_NEAR(restored.get_parameter_normalized(1), 0.25f, 0.001f);
}

TEST(PluginProcessor, InvalidStateReturnsFalse) {
    AmplitronPluginProcessor processor;
    processor.prepare(48000.0, 512);

    EXPECT_FALSE(processor.load_state_json("not json"));
}
