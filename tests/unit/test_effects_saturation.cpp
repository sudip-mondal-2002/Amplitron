#include "test_framework.h"
#include "test_fixtures.h"
#include "audio/effects/overdrive.h"
#include "audio/effects/distortion.h"
#include "audio/effects/amp_simulator.h"
#include <cstring>
#include <cmath>
#include <vector>

using namespace Amplitron;

TEST_F(EffectsTest, overdrive_adds_harmonics) {
    Overdrive od;
    od.set_sample_rate(SR);
    od.reset();

    fill_sine(440.0f);
    od.process(input_buffer, BUFFER_SIZE);

    ASSERT_TRUE(is_finite(input_buffer, BUFFER_SIZE));
    ASSERT_GT(rms(input_buffer, BUFFER_SIZE), 0.01f);
}

TEST_F(EffectsTest, distortion_clips_signal) {
    Distortion dist;
    dist.set_sample_rate(SR);
    dist.reset();

    fill_sine(440.0f);
    dist.process(input_buffer, BUFFER_SIZE);

    ASSERT_TRUE(is_finite(input_buffer, BUFFER_SIZE));

    for (int i = 0; i < BUFFER_SIZE; ++i) {
        ASSERT_GE(input_buffer[i], -1.5f);
        ASSERT_TRUE(input_buffer[i] <= 1.5f);
    }
}

TEST_F(EffectsTest, amp_simulator_processes_without_nan) {
    AmpSimulator amp;
    amp.set_sample_rate(SR);
    amp.reset();

    fill_sine(440.0f);
    amp.process(input_buffer, BUFFER_SIZE);

    ASSERT_TRUE(is_finite(input_buffer, BUFFER_SIZE));
    ASSERT_GT(rms(input_buffer, BUFFER_SIZE), 0.001f);
}

TEST_F(EffectsTest, amp_simulator_models_sound_different) {
    const auto& models = Amplitron::get_amp_models();
    ASSERT_GE((int)models.size(), 3);

    std::vector<float> model_rms;
    for (int m = 0; m < static_cast<int>(models.size()); ++m) {
        AmpSimulator amp;
        amp.set_sample_rate(SR);
        amp.reset();
        amp.params()[0].value = static_cast<float>(m);

        float buf[1024];
        for (int i = 0; i < 1024; ++i) {
            buf[i] = std::sin(2.0f * 3.14159265f * 440.0f * i / SR);
        }
        amp.process(buf, 1024);
        ASSERT_TRUE(is_finite(buf, 1024));
        model_rms.push_back(rms(buf, 1024));
    }

    bool found_diff = false;
    for (size_t i = 0; i < model_rms.size() && !found_diff; ++i) {
        for (size_t j = i + 1; j < model_rms.size(); ++j) {
            if (std::fabs(model_rms[i] - model_rms[j]) > 0.01f) {
                found_diff = true;
                break;
            }
        }
    }
    ASSERT_TRUE(found_diff);
}

TEST_F(EffectsTest, amp_simulator_output_clamped) {
    AmpSimulator amp;
    amp.set_sample_rate(SR);
    amp.reset();
    amp.params()[0].value = 2.0f; // High Gain Modern
    amp.params()[1].value = 1.0f; // Max gain knob

    fill_sine(440.0f);
    amp.process(input_buffer, BUFFER_SIZE);

    for (int i = 0; i < BUFFER_SIZE; ++i) {
        ASSERT_GE(input_buffer[i], -1.0f);
        ASSERT_TRUE(input_buffer[i] <= 1.0f);
    }
}

TEST_F(EffectsTest, amp_simulator_get_models_returns_at_least_three) {
    const auto& models = Amplitron::get_amp_models();
    ASSERT_GE((int)models.size(), 3);
    for (const auto& m : models) {
        ASSERT_TRUE(m.name != nullptr);
        ASSERT_TRUE(m.inspiration != nullptr);
        ASSERT_TRUE(m.description != nullptr);
    }
}
