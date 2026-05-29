#include "test_framework.h"
#include "test_fixtures.h"
#include "audio/effects/octaver.h"
#include "audio/effects/pitch_shifter.h"
#include <cstring>
#include <cmath>
#include <vector>

using namespace Amplitron;

TEST_F(EffectsTest, octaver_sub_octave_produces_half_frequency) {
    static constexpr float FUND = 880.0f;
    static constexpr int   CHUNK = 256;
    static constexpr int   WARM_UP = 12288;
    static constexpr int   N    = 4800;

    Octaver oct;
    oct.set_sample_rate(SR);
    oct.reset();
    oct.params()[0].value = 1.0f;  // P_OCT_DOWN
    oct.params()[1].value = 0.0f;  // P_OCT_UP
    oct.params()[2].value = 0.0f;  // P_DRY

    float chunk_buf[CHUNK];
    for (int off = 0; off < WARM_UP; off += CHUNK) {
        for (int i = 0; i < CHUNK; ++i) {
            chunk_buf[i] = 0.8f * std::sin(2.0f * 3.14159265f * FUND * (off + i) / SR);
        }
        oct.process(chunk_buf, CHUNK);
    }

    float main_buf[N];
    for (int i = 0; i < N; ++i) {
        main_buf[i] = 0.8f * std::sin(2.0f * 3.14159265f * FUND * (WARM_UP + i) / SR);
    }
    oct.process(main_buf, N);

    ASSERT_TRUE(is_finite(main_buf, N));
    ASSERT_GT(rms(main_buf, N), 0.1f);

    int crossings = 0;
    for (int i = 1; i < N; ++i) {
        if (main_buf[i-1] < 0.0f && main_buf[i] >= 0.0f)
            ++crossings;
    }
    ASSERT_GE(crossings, 33);
    ASSERT_LT(crossings, 56);
}

TEST_F(EffectsTest, octaver_upper_octave_produces_double_frequency) {
    static constexpr float FUND = 220.0f;
    static constexpr int   CHUNK = 256;
    static constexpr int   WARM_UP = 12288;
    static constexpr int   N    = 8192;

    Octaver oct;
    oct.set_sample_rate(SR);
    oct.reset();
    oct.params()[0].value = 0.0f;  // P_OCT_DOWN
    oct.params()[1].value = 1.0f;  // P_OCT_UP
    oct.params()[2].value = 0.0f;  // P_DRY

    float chunk_buf[CHUNK];
    for (int off = 0; off < WARM_UP; off += CHUNK) {
        for (int i = 0; i < CHUNK; ++i) {
            chunk_buf[i] = 0.8f * std::sin(2.0f * 3.14159265f * FUND * (off + i) / SR);
        }
        oct.process(chunk_buf, CHUNK);
    }

    float main_buf[N];
    for (int i = 0; i < N; ++i) {
        main_buf[i] = 0.8f * std::sin(2.0f * 3.14159265f * FUND * (WARM_UP + i) / SR);
    }
    oct.process(main_buf, N);

    ASSERT_TRUE(is_finite(main_buf, N));

    float mag_double = dft_magnitude_at(main_buf, N, FUND * 2.0f);
    float mag_fund   = dft_magnitude_at(main_buf, N, FUND);

    ASSERT_GT(mag_double, 0.05f);
    ASSERT_LT(mag_fund, mag_double * 0.5f);
}

TEST_F(EffectsTest, octaver_disabled_no_sub_or_upper_octave) {
    static constexpr float FUND = 220.0f;
    static constexpr int   N    = 8192;

    Octaver oct;
    oct.set_sample_rate(SR);
    oct.reset();
    oct.set_enabled(false);
    oct.params()[0].value = 1.0f;
    oct.params()[1].value = 1.0f;
    oct.params()[2].value = 0.0f;

    float buf[N];
    for (int i = 0; i < N; ++i) {
        buf[i] = 0.8f * std::sin(2.0f * 3.14159265f * FUND * i / SR);
    }
    oct.process(buf, N);

    float mag_fund   = dft_magnitude_at(buf, N, FUND);
    float mag_half   = dft_magnitude_at(buf, N, FUND / 2.0f);
    float mag_double = dft_magnitude_at(buf, N, FUND * 2.0f);

    ASSERT_GT(mag_fund, 0.3f);
    ASSERT_LT(mag_half,   0.01f);
    ASSERT_LT(mag_double, 0.01f);
}

TEST_F(EffectsTest, octaver_params_have_valid_ranges) {
    Octaver oct;
    for (auto& p : oct.params()) {
        ASSERT_TRUE(p.min_val <= p.max_val);
        ASSERT_TRUE(p.value >= p.min_val && p.value <= p.max_val);
        ASSERT_TRUE(p.default_val >= p.min_val && p.default_val <= p.max_val);
        ASSERT_FALSE(p.name.empty());
    }
}

TEST_F(EffectsTest, octaver_silence_passthrough) {
    Octaver oct;
    oct.set_sample_rate(SR);
    oct.reset();

    std::memset(input_buffer, 0, sizeof(input_buffer));
    oct.process(input_buffer, BUFFER_SIZE);

    ASSERT_TRUE(is_finite(input_buffer, BUFFER_SIZE));
    ASSERT_LT(rms(input_buffer, BUFFER_SIZE), 1e-8f);
}

TEST_F(EffectsTest, octaver_extreme_mix_values) {
    Octaver oct;
    oct.set_sample_rate(SR);

    float ref[BUFFER_SIZE];
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        ref[i] = std::sin(2.0f * 3.14159265f * 440.0f * i / SR);
    }
    oct.reset();

    float dry_buf[BUFFER_SIZE];
    std::memcpy(dry_buf, ref, sizeof(ref));

    oct.params()[0].value = 1.0f; // sub octave
    oct.params()[1].value = 1.0f; // upper octave
    oct.params()[2].value = 1.0f; // fully dry

    oct.process(dry_buf, BUFFER_SIZE);

    ASSERT_TRUE(is_finite(dry_buf, BUFFER_SIZE));
    ASSERT_GT(rms(dry_buf, BUFFER_SIZE), 0.1f);
    oct.reset();

    float wet_buf[BUFFER_SIZE];
    std::memcpy(wet_buf, ref, sizeof(ref));

    oct.params()[0].value = 1.0f;
    oct.params()[1].value = 1.0f;
    oct.params()[2].value = 0.0f; // no dry signal

    oct.process(wet_buf, BUFFER_SIZE);

    ASSERT_TRUE(is_finite(wet_buf, BUFFER_SIZE));
    ASSERT_GT(rms(wet_buf, BUFFER_SIZE), 0.01f);
}

TEST_F(EffectsTest, octaver_parameter_combinations_stay_finite) {
    Octaver oct;
    oct.set_sample_rate(SR);

    const float values[] = {0.0f, 0.5f, 1.0f};

    for (float down : values) {
        for (float up : values) {
            for (float dry : values) {
                oct.reset();
                oct.params()[0].value = down;
                oct.params()[1].value = up;
                oct.params()[2].value = dry;

                float buf[1024];
                for (int i = 0; i < 1024; ++i) {
                    buf[i] = std::sin(2.0f * 3.14159265f * 440.0f * i / SR);
                }

                oct.process(buf, 1024);
                ASSERT_TRUE(is_finite(buf, 1024));
            }
        }
    }
}

TEST_F(EffectsTest, pitch_shifter_params_have_valid_ranges) {
    PitchShifter ps;
    for (auto& p : ps.params()) {
        ASSERT_TRUE(p.min_val <= p.max_val);
        ASSERT_TRUE(p.value >= p.min_val && p.value <= p.max_val);
        ASSERT_TRUE(p.default_val >= p.min_val && p.default_val <= p.max_val);
        ASSERT_FALSE(p.name.empty());
    }
}

TEST_F(EffectsTest, pitch_shifter_default_is_transparent) {
    PitchShifter ps;
    ps.set_sample_rate(SR);
    ps.reset();
    ASSERT_NEAR(ps.params()[0].value, 0.0f, 1e-6f);  // Shift
    ASSERT_NEAR(ps.params()[1].value, 0.0f, 1e-6f);  // Fine
    ASSERT_NEAR(ps.params()[2].value, 0.0f, 1e-6f);  // Mix

    fill_sine(440.0f);
    copy_input_to_output();

    ps.process(input_buffer, 256);

    for (int i = 0; i < 256; ++i) {
        ASSERT_NEAR(input_buffer[i], output_buffer[i], 1e-5f);
    }
}

TEST_F(EffectsTest, pitch_shifter_with_mix_and_shift_differs_from_dry) {
    PitchShifter ps;
    ps.set_sample_rate(SR);
    ps.reset();
    ps.params()[0].value = 7.0f;   // Shift = +7 semitones
    ps.params()[2].value = 1.0f;   // Mix = fully wet

    float warm[512];
    for (int rep = 0; rep < 30; ++rep) {
        for (int i = 0; i < 512; ++i) {
            warm[i] = std::sin(2.0f * 3.14159265f * 440.0f * i / SR);
        }
        ps.process(warm, 512);
    }

    fill_sine(440.0f);
    ps.process(input_buffer, BUFFER_SIZE);

    ASSERT_TRUE(is_finite(input_buffer, BUFFER_SIZE));
    ASSERT_GT(rms(input_buffer, BUFFER_SIZE), 0.01f);

    const float shifted_freq = 440.0f * std::pow(2.0f, 7.0f / 12.0f);

    const float mag_440 = dft_magnitude_at(input_buffer, BUFFER_SIZE, 440.0f);
    const float mag_shifted = dft_magnitude_at(input_buffer, BUFFER_SIZE, shifted_freq);

    ASSERT_GT(mag_shifted, mag_440 * 0.5f);
}

TEST_F(EffectsTest, pitch_shifter_extreme_shift_wraps_phase_without_instability) {
    PitchShifter ps;
    ps.set_sample_rate(SR);
    ps.params()[0].value = 120.0f;
    ps.params()[1].value = 500.0f;
    ps.params()[2].value = 1.0f;
    ps.reset();

    std::vector<float> buf(4096);
    for (size_t i = 0; i < buf.size(); ++i) {
        buf[i] = std::sin(2.0f * 3.14159265f * 440.0f * i / SR);
    }

    ps.process(buf.data(), static_cast<int>(buf.size()));

    ASSERT_TRUE(is_finite(buf.data(), static_cast<int>(buf.size())));
    ASSERT_GT(rms(buf.data(), static_cast<int>(buf.size())), 0.001f);
}
