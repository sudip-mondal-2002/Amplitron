#include "test_framework.h"

#include "audio/effects/looper.h"

#include <cmath>

using namespace Amplitron;

static float soft_clip_ref(float x) {
    const float ax = std::fabs(x);
    return x / (1.0f + ax);
}

TEST(looper_initial_state_empty) {
    Looper looper;
    looper.set_sample_rate(1000);
    looper.reset();

    ASSERT_EQ(static_cast<uint32_t>(looper.state()), static_cast<uint32_t>(Looper::State::Empty));
    ASSERT_FALSE(looper.has_loop());
    ASSERT_EQ(looper.loop_length_samples(), 0);
    ASSERT_EQ(looper.playhead_samples(), 0);
}

TEST(looper_record_then_play_mixes_loop) {
    Looper looper;
    looper.set_sample_rate(1000);
    looper.reset();

    // Record 120 samples (>= 0.10s @ 1kHz => min 100 samples).
    constexpr int kRecord = 120;
    float record_buf[kRecord];
    for (int i = 0; i < kRecord; ++i) record_buf[i] = 0.5f;

    looper.request_record_toggle(); // start recording
    looper.process(record_buf, kRecord);

    // Stop recording and immediately play (process one sample to execute toggle).
    float out_buf[1] = {0.0f};
    looper.request_record_toggle();
    looper.process(out_buf, 1);

    ASSERT_TRUE(looper.has_loop());
    ASSERT_EQ(looper.loop_length_samples(), kRecord);
    ASSERT_EQ(static_cast<uint32_t>(looper.state()), static_cast<uint32_t>(Looper::State::Playing));

    // Expected: output = soft_clip(0 + recorded(0.5) * default_level(0.8))
    const float expected = soft_clip_ref(0.5f * 0.80f);
    ASSERT_NEAR(out_buf[0], expected, 1e-4f);
}

TEST(looper_play_toggle_keeps_loop) {
    Looper looper;
    looper.set_sample_rate(1000);
    looper.reset();

    constexpr int kRecord = 120;
    float record_buf[kRecord];
    for (int i = 0; i < kRecord; ++i) record_buf[i] = 0.25f;

    looper.request_record_toggle();
    looper.process(record_buf, kRecord);

    float tmp[1] = {0.0f};
    looper.request_record_toggle(); // stop -> playing
    looper.process(tmp, 1);

    ASSERT_TRUE(looper.has_loop());
    ASSERT_EQ(static_cast<uint32_t>(looper.state()), static_cast<uint32_t>(Looper::State::Playing));

    // Toggle play -> idle (but loop remains).
    looper.request_play_toggle();
    looper.process(tmp, 1);
    ASSERT_TRUE(looper.has_loop());
    ASSERT_EQ(static_cast<uint32_t>(looper.state()), static_cast<uint32_t>(Looper::State::Idle));

    // Toggle play -> playing again.
    looper.request_play_toggle();
    looper.process(tmp, 1);
    ASSERT_TRUE(looper.has_loop());
    ASSERT_EQ(static_cast<uint32_t>(looper.state()), static_cast<uint32_t>(Looper::State::Playing));
}

TEST(looper_clear_resets_to_empty) {
    Looper looper;
    looper.set_sample_rate(1000);
    looper.reset();

    constexpr int kRecord = 120;
    float record_buf[kRecord];
    for (int i = 0; i < kRecord; ++i) record_buf[i] = 0.1f;

    looper.request_record_toggle();
    looper.process(record_buf, kRecord);

    float tmp[1] = {0.0f};
    looper.request_record_toggle(); // stop -> playing
    looper.process(tmp, 1);

    ASSERT_TRUE(looper.has_loop());

    looper.request_clear();
    looper.process(tmp, 1);

    ASSERT_FALSE(looper.has_loop());
    ASSERT_EQ(looper.loop_length_samples(), 0);
    ASSERT_EQ(static_cast<uint32_t>(looper.state()), static_cast<uint32_t>(Looper::State::Empty));
}

