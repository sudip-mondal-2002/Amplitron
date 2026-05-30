#include "test_framework.h"
#include "audio/dsp/level_analyzer.h"
#include <cmath>
#include <cstdint>
#include <cstring>

using namespace Amplitron;

TEST(level_analyzer_initial_state) {
    LevelAnalyzer la;
    ASSERT_NEAR(la.smoothed_input_rms(), 0.0f, 1e-6f);
    ASSERT_NEAR(la.smoothed_output_rms(), 0.0f, 1e-6f);
    ASSERT_NEAR(la.input_peak_hold(), 0.0f, 1e-6f);
    ASSERT_NEAR(la.output_peak_hold(), 0.0f, 1e-6f);
    ASSERT_NEAR(la.input_clip_flash(), 0.0f, 1e-6f);
    ASSERT_NEAR(la.output_clip_flash(), 0.0f, 1e-6f);
}

TEST(level_analyzer_normal_update_smoothing) {
    LevelAnalyzer la;

    // First update: inputs are 0.5 RMS, dt = 0.1s
    la.update(0.5f, 0.5f, false, false, 0.1f);

    // RMS smoothing: 0 + (0.5 - 0) * 0.22 = 0.11
    ASSERT_NEAR(la.smoothed_input_rms(), 0.11f, 1e-4f);
    ASSERT_NEAR(la.smoothed_output_rms(), 0.11f, 1e-4f);

    // Peak hold should match smoothed RMS initially as peak_hold_ - decay*dt will be <= smoothed RMS
    ASSERT_NEAR(la.input_peak_hold(), 0.11f, 1e-4f);
    ASSERT_NEAR(la.output_peak_hold(), 0.11f, 1e-4f);
}

TEST(level_analyzer_peak_decay_and_clipping) {
    LevelAnalyzer la;

    // Trigger clipping flash and set high initial RMS
    la.update(1.0f, 1.0f, true, true, 0.1f);
    ASSERT_NEAR(la.input_clip_flash(), 0.8f, 1e-4f); // 1.0 - 0.1 * 2.0 = 0.8
    ASSERT_NEAR(la.output_clip_flash(), 0.8f, 1e-4f);

    float last_peak_in = la.input_peak_hold();
    float last_peak_out = la.output_peak_hold();

    // Now update with 0 RMS, so peak hold starts decaying
    la.update(0.0f, 0.0f, false, false, 0.1f);

    // Peak decay = 0.45 * 0.1 = 0.045
    // smoothed_rms is 0.22 - 0.22 * 0.22 = 0.1716
    // decayed_peak is 0.22 - 0.045 = 0.175. std::max(0.1716, 0.175) = 0.175
    ASSERT_NEAR(la.input_peak_hold(), last_peak_in - 0.045f, 1e-4f);
    ASSERT_NEAR(la.output_peak_hold(), last_peak_out - 0.045f, 1e-4f);

    // Clip flash decay = 0.8 - 0.1 * 2.0 = 0.6
    ASSERT_NEAR(la.input_clip_flash(), 0.6f, 1e-4f);
    ASSERT_NEAR(la.output_clip_flash(), 0.6f, 1e-4f);
}

TEST(level_analyzer_invalid_inputs_nan_negative) {
    LevelAnalyzer la;

    // Set a valid initial state
    la.update(0.5f, 0.5f, false, false, 0.1f);
    float valid_in_rms = la.smoothed_input_rms();
    float valid_out_rms = la.smoothed_output_rms();
    float valid_in_peak = la.input_peak_hold();
    float valid_out_peak = la.output_peak_hold();

    // Construct runtime bitwise NaN and Infinity to prevent compile-time optimizations under -ffast-math
    float nan_val;
    uint32_t nan_bits = 0x7FC00000;
    std::memcpy(&nan_val, &nan_bits, sizeof(float));

    float inf_val;
    uint32_t inf_bits = 0x7F800000;
    std::memcpy(&inf_val, &inf_bits, sizeof(float));

    // 1. Invalid dt (<= 0)
    la.update(0.8f, 0.8f, false, false, -0.01f);
    ASSERT_NEAR(la.smoothed_input_rms(), valid_in_rms, 1e-6f);
    ASSERT_NEAR(la.input_peak_hold(), valid_in_peak, 1e-6f);

    // 2. Invalid dt (infinite/NaN)
    la.update(0.8f, 0.8f, false, false, nan_val);
    ASSERT_NEAR(la.smoothed_input_rms(), valid_in_rms, 1e-6f);

    // 3. Invalid input RMS (< 0)
    la.update(-0.5f, 0.5f, false, false, 0.1f);
    ASSERT_NEAR(la.smoothed_input_rms(), valid_in_rms, 1e-6f);

    // 4. Invalid input RMS (NaN)
    la.update(nan_val, 0.5f, false, false, 0.1f);
    ASSERT_NEAR(la.smoothed_input_rms(), valid_in_rms, 1e-6f);

    // 5. Invalid output RMS (< 0)
    la.update(0.5f, -0.1f, false, false, 0.1f);
    ASSERT_NEAR(la.smoothed_output_rms(), valid_out_rms, 1e-6f);

    // 6. Invalid output RMS (inf)
    la.update(0.5f, inf_val, false, false, 0.1f);
    ASSERT_NEAR(la.smoothed_output_rms(), valid_out_rms, 1e-6f);
}
