#include "test_framework.h"
#include "common.h"

using namespace Amplitron;

// ============================================================
// common.h utility function tests
// ============================================================

TEST(clamp_within_range) {
    ASSERT_NEAR(clamp(0.5f, 0.0f, 1.0f), 0.5f, 1e-6f);
}

TEST(clamp_below_min) {
    ASSERT_NEAR(clamp(-1.0f, 0.0f, 1.0f), 0.0f, 1e-6f);
}

TEST(clamp_above_max) {
    ASSERT_NEAR(clamp(5.0f, 0.0f, 1.0f), 1.0f, 1e-6f);
}

TEST(clamp_at_boundaries) {
    ASSERT_NEAR(clamp(0.0f, 0.0f, 1.0f), 0.0f, 1e-6f);
    ASSERT_NEAR(clamp(1.0f, 0.0f, 1.0f), 1.0f, 1e-6f);
}

TEST(db_to_linear_zero_db) {
    ASSERT_NEAR(db_to_linear(0.0f), 1.0f, 1e-5f);
}

TEST(db_to_linear_positive_6db) {
    // +6 dB ~= 2.0
    ASSERT_NEAR(db_to_linear(6.0f), 1.9953f, 0.01f);
}

TEST(db_to_linear_negative_20db) {
    // -20 dB = 0.1
    ASSERT_NEAR(db_to_linear(-20.0f), 0.1f, 1e-5f);
}

TEST(linear_to_db_unity) {
    ASSERT_NEAR(linear_to_db(1.0f), 0.0f, 1e-5f);
}

TEST(linear_to_db_half) {
    // 0.5 ~= -6.02 dB
    ASSERT_NEAR(linear_to_db(0.5f), -6.0206f, 0.01f);
}

TEST(linear_to_db_very_small) {
    // Should not crash or produce NaN for very small values
    float result = linear_to_db(1e-15f);
    ASSERT_TRUE(std::isfinite(result));
    ASSERT_LT(result, -100.0f);
}

TEST(db_linear_roundtrip) {
    float original = 0.75f;
    float db = linear_to_db(original);
    float back = db_to_linear(db);
    ASSERT_NEAR(back, original, 1e-5f);
}

TEST(soft_clip_passthrough_small) {
    // Small values should pass through nearly unchanged
    ASSERT_NEAR(soft_clip(0.1f), 0.1f - (0.001f / 3.0f), 1e-5f);
}

TEST(soft_clip_saturates_above_1) {
    ASSERT_NEAR(soft_clip(2.0f), 2.0f / 3.0f, 1e-6f);
    ASSERT_NEAR(soft_clip(100.0f), 2.0f / 3.0f, 1e-6f);
}

TEST(soft_clip_symmetric) {
    ASSERT_NEAR(soft_clip(-0.5f), -soft_clip(0.5f), 1e-6f);
    ASSERT_NEAR(soft_clip(-2.0f), -soft_clip(2.0f), 1e-6f);
}

TEST(hard_clip_within_threshold) {
    ASSERT_NEAR(hard_clip(0.5f, 1.0f), 0.5f, 1e-6f);
}

TEST(hard_clip_above_threshold) {
    ASSERT_NEAR(hard_clip(2.0f, 0.8f), 0.8f, 1e-6f);
    ASSERT_NEAR(hard_clip(-2.0f, 0.8f), -0.8f, 1e-6f);
}

TEST(smooth_param_converges) {
    SmoothParam sp;
    sp.current = 0.0f;
    sp.target = 1.0f;
    sp.coeff = 0.9f;

    for (int i = 0; i < 200; ++i) sp.next();
    ASSERT_NEAR(sp.current, 1.0f, 0.01f);
}

TEST(smooth_param_snap) {
    SmoothParam sp;
    sp.current = 0.0f;
    sp.target = 5.0f;
    sp.snap();
    ASSERT_NEAR(sp.current, 5.0f, 1e-6f);
}
