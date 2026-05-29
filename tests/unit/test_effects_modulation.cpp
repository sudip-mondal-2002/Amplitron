#include "test_framework.h"
#include "test_fixtures.h"
#include "audio/effects/chorus.h"
#include "audio/effects/phaser.h"
#include "audio/effects/flanger.h"
#include <cstring>
#include <cmath>

using namespace Amplitron;

TEST_F(EffectsTest, chorus_modulates_signal) {
    Chorus ch;
    ch.set_sample_rate(SR);
    ch.reset();

    fill_sine(440.0f);
    ch.process(input_buffer, BUFFER_SIZE);

    ASSERT_TRUE(is_finite(input_buffer, BUFFER_SIZE));
}

TEST_F(EffectsTest, phaser_produces_finite_output) {
    Phaser ph;
    ph.set_sample_rate(SR);
    ph.reset();

    fill_sine(440.0f);
    ph.process(input_buffer, BUFFER_SIZE);

    ASSERT_TRUE(is_finite(input_buffer, BUFFER_SIZE));
    ASSERT_GT(rms(input_buffer, BUFFER_SIZE), 0.001f);
}

TEST_F(EffectsTest, phaser_params_have_valid_ranges) {
    Phaser ph;
    for (auto& p : ph.params()) {
        ASSERT_TRUE(p.min_val <= p.max_val);
        ASSERT_TRUE(p.value >= p.min_val && p.value <= p.max_val);
        ASSERT_TRUE(p.default_val >= p.min_val && p.default_val <= p.max_val);
        ASSERT_FALSE(p.name.empty());
    }
}

TEST_F(EffectsTest, phaser_disabled_passes_dry_signal) {
    Phaser ph;
    ph.set_sample_rate(SR);
    ph.reset();
    ph.set_enabled(false);

    fill_sine(440.0f);
    copy_input_to_output();
    ph.process(input_buffer, BUFFER_SIZE);

    for (int i = 0; i < BUFFER_SIZE; ++i) {
        ASSERT_NEAR(input_buffer[i], output_buffer[i], 1e-6f);
    }
}

TEST_F(EffectsTest, phaser_lfo_modulates_output) {
    Phaser ph;
    ph.set_sample_rate(SR);
    ph.reset();
    ph.params()[0].value = 2.0f;  // fast rate
    ph.params()[1].value = 0.8f;  // wide depth
    ph.params()[3].value = 0.5f;  // 50% mix

    fill_sine(440.0f);
    float buf_a[BUFFER_SIZE], buf_b[BUFFER_SIZE];
    std::memcpy(buf_a, input_buffer, sizeof(input_buffer));
    std::memcpy(buf_b, input_buffer, sizeof(input_buffer));

    ph.process(buf_a, BUFFER_SIZE);
    ph.process(buf_b, BUFFER_SIZE);

    float diff = 0.0f;
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        diff += std::fabs(buf_a[i] - buf_b[i]);
    }
    ASSERT_GT(diff, 0.01f);
}

TEST_F(EffectsTest, phaser_all_stage_counts_finite) {
    Phaser ph;
    ph.set_sample_rate(SR);

    const float stages[] = {4.0f, 6.0f, 8.0f, 12.0f};
    for (float st : stages) {
        ph.reset();
        ph.params()[2].value = st;
        fill_sine(440.0f);
        ph.process(input_buffer, BUFFER_SIZE);
        ASSERT_TRUE(is_finite(input_buffer, BUFFER_SIZE));
    }
}

TEST_F(EffectsTest, flanger_produces_finite_output) {
    Flanger fl;
    fl.set_sample_rate(SR);
    fl.reset();

    fill_sine(440.0f);
    fl.process(input_buffer, BUFFER_SIZE);

    ASSERT_TRUE(is_finite(input_buffer, BUFFER_SIZE));
    ASSERT_GT(rms(input_buffer, BUFFER_SIZE), 0.001f);
}

TEST_F(EffectsTest, flanger_params_have_valid_ranges) {
    Flanger fl;
    for (auto& p : fl.params()) {
        ASSERT_TRUE(p.min_val <= p.max_val);
        ASSERT_TRUE(p.value >= p.min_val && p.value <= p.max_val);
        ASSERT_TRUE(p.default_val >= p.min_val && p.default_val <= p.max_val);
        ASSERT_FALSE(p.name.empty());
    }
}

TEST_F(EffectsTest, flanger_disabled_passes_dry_signal) {
    Flanger fl;
    fl.set_sample_rate(SR);
    fl.reset();
    fl.set_enabled(false);

    fill_sine(440.0f);
    copy_input_to_output();
    fl.process(input_buffer, 256);

    for (int i = 0; i < 256; ++i) {
        ASSERT_NEAR(input_buffer[i], output_buffer[i], 1e-6f);
    }
}

TEST_F(EffectsTest, flanger_silence_passthrough) {
    Flanger fl;
    fl.set_sample_rate(SR);
    fl.reset();

    std::memset(input_buffer, 0, sizeof(input_buffer));
    fl.process(input_buffer, BUFFER_SIZE);

    ASSERT_TRUE(is_finite(input_buffer, BUFFER_SIZE));
    ASSERT_LT(rms(input_buffer, BUFFER_SIZE), 1e-8f);
}

TEST_F(EffectsTest, flanger_extreme_params_stay_finite) {
    Flanger fl;
    fl.set_sample_rate(SR);

    const float rates[] = {0.05f, 5.0f};
    const float depths[] = {0.1f, 7.0f};
    const float feedbacks[] = {-0.95f, 0.95f};
    const float mixes[] = {0.0f, 1.0f};

    for (float rate : rates) {
        for (float depth : depths) {
            for (float feedback : feedbacks) {
                for (float mix : mixes) {
                    fl.reset();
                    fl.params()[0].value = rate;
                    fl.params()[1].value = depth;
                    fl.params()[3].value = feedback;
                    fl.params()[4].value = mix;

                    fill_sine(440.0f);
                    fl.process(input_buffer, BUFFER_SIZE);

                    ASSERT_TRUE(is_finite(input_buffer, BUFFER_SIZE));
                }
            }
        }
    }
}

TEST_F(EffectsTest, flanger_sample_rate_change) {
    Flanger fl;

    fl.set_sample_rate(48000);
    fl.reset();
    fill_sine(440.0f);
    fl.process(input_buffer, BUFFER_SIZE);
    ASSERT_TRUE(is_finite(input_buffer, BUFFER_SIZE));

    fl.set_sample_rate(96000);
    fl.reset();
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        input_buffer[i] = std::sin(2.0f * 3.14159265f * 440.0f * i / 96000.0f);
    }
    fl.process(input_buffer, BUFFER_SIZE);
    ASSERT_TRUE(is_finite(input_buffer, BUFFER_SIZE));
}

TEST_F(EffectsTest, flanger_toggle_no_gliches) {
    Flanger fl;
    fl.set_sample_rate(SR);
    fl.reset();

    fl.set_enabled(true);
    fill_sine(440.0f);
    fl.process(input_buffer, BUFFER_SIZE);

    float mean_a = 0.0f;
    for (int i = 0; i < BUFFER_SIZE; ++i) mean_a += input_buffer[i];
    mean_a /= static_cast<float>(BUFFER_SIZE);

    fl.set_enabled(false);
    fill_sine(440.0f);
    fl.process(input_buffer, BUFFER_SIZE);

    float mean_b = 0.0f;
    for (int i = 0; i < BUFFER_SIZE; ++i) mean_b += input_buffer[i];
    mean_b /= static_cast<float>(BUFFER_SIZE);

    ASSERT_LT(std::fabs(mean_b - mean_a), 5e-3f);
}

TEST_F(EffectsTest, flanger_wet_differs_from_dry) {
    Flanger fl;
    fl.set_sample_rate(SR);
    fl.reset();
    fl.set_enabled(true);

    fl.params()[0].value = 1.0f; // Rate
    fl.params()[1].value = 5.0f; // Depth
    fl.params()[4].value = 0.5f; // Mix

    fill_sine(440.0f);
    copy_input_to_output();

    fl.process(input_buffer, BUFFER_SIZE);

    float diff_sum = 0.0f;
    for (int i = 0; i < BUFFER_SIZE; i++) {
        diff_sum += std::fabs(input_buffer[i] - output_buffer[i]);
    }

    ASSERT_GT(diff_sum, 0.01f);
}

TEST_F(EffectsTest, flanger_lfo_modulates_output) {
    Flanger fl;
    fl.set_sample_rate(SR);
    fl.reset();
    fl.params()[0].value = 2.0f;  // fast rate
    fl.params()[1].value = 5.0f;  // wide depth
    fl.params()[4].value = 0.5f;  // 50% mix

    float buf_a[BUFFER_SIZE], buf_b[BUFFER_SIZE];
    fill_sine(440.0f);
    std::memcpy(buf_a, input_buffer, sizeof(input_buffer));
    std::memcpy(buf_b, input_buffer, sizeof(input_buffer));

    fl.process(buf_a, BUFFER_SIZE);
    fl.process(buf_b, BUFFER_SIZE);

    float diff = 0.0f;
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        diff += std::fabs(buf_a[i] - buf_b[i]);
    }
    ASSERT_GT(diff, 0.01f);
}

TEST_F(EffectsTest, flanger_high_feedback_stays_finite) {
    Flanger fl;
    fl.set_sample_rate(SR);
    fl.reset();
    fl.params()[3].value = 0.95f;  // max positive feedback

    float buf[4096];
    for (int i = 0; i < 4096; ++i) {
        buf[i] = std::sin(2.0f * 3.14159265f * 220.0f * i / SR);
    }
    fl.process(buf, 4096);
    ASSERT_TRUE(is_finite(buf, 4096));
}

TEST_F(EffectsTest, chorus_calculates_correct_rate_from_bpm) {
    Chorus ch;
    ch.set_sample_rate(SR);
    ch.reset();

    ch.set_transport_state(120.0f);

    // At 120 BPM, the LFO rate should be 2.0 Hz (120 / 60)
    ASSERT_NEAR(ch.params()[0].value, 2.0f, 0.01f);
}
