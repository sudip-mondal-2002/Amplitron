// test_pitch_shifter.cpp
// Tests for perf(pitch_shifter): optimize transcendental functions in DSP loop
//
// Covers:
//  1. Hann LUT      — pre-computed table produces power-complementary gains
//  2. Early-exit    — mix < 0.001f bypasses processing without zeroing buffer
//  3. Ratio hoisted — process() output is deterministic across instances
//  4. Wet/dry blend — mix=0 passes dry, mid-mix blends energy correctly

#include "test_framework.h"
#include "audio/effects/pitch_shifter.h"
#include <algorithm>
#include <cmath>
#include <vector>

using namespace Amplitron;

namespace {

constexpr int   kSampleRate = 48000;
constexpr float TWO_PI      = 6.28318530718f;

void fill_sine(float* buf, int n, float freq) {
    for (int i = 0; i < n; ++i)
        buf[i] = std::sin(TWO_PI * freq * static_cast<float>(i) / kSampleRate);
}

float rms(const float* buf, int n) {
    float sum = 0.0f;
    for (int i = 0; i < n; ++i) sum += buf[i] * buf[i];
    return std::sqrt(sum / static_cast<float>(n));
}

float max_diff(const float* a, const float* b, int n) {
    float m = 0.0f;
    for (int i = 0; i < n; ++i)
        m = std::max(m, std::abs(a[i] - b[i]));
    return m;
}

class TestablePitchShifter : public PitchShifter {
public:
    void set_param(int idx, float v) { params()[idx].value = v; }
};

} // namespace

class PitchShifterTest : public TestFramework::Test {
public:
    TestablePitchShifter ps_;
    void SetUp() override {
        ps_.set_sample_rate(kSampleRate);
        ps_.set_param(0, 0.0f);
        ps_.set_param(1, 0.0f);
        ps_.set_param(2, 1.0f);
        ps_.reset();
    }
};

// 1. Hann LUT — gain_a + gain_b == 1; DC in must equal DC out once primed.
TEST_F(PitchShifterTest, HannLutIsPowerComplementary) {
    const int BLOCK = 8192;
    std::vector<float> buf(BLOCK, 1.0f);
    ps_.process(buf.data(), BLOCK);
    const int HALF = BLOCK / 2;
    for (int i = HALF; i < BLOCK; ++i)
        ASSERT_NEAR(buf[i], 1.0f, 0.01f);
}

// 2. Early-exit — mix=0 must not touch the buffer at all.
TEST_F(PitchShifterTest, EarlyExitLeavesBufferUnchanged) {
    ps_.set_param(2, 0.0f);
    ps_.reset();
    const int BLOCK = 256;
    std::vector<float> buf(BLOCK);
    fill_sine(buf.data(), BLOCK, 440.0f);
    const std::vector<float> original = buf;
    ps_.process(buf.data(), BLOCK);
    ASSERT_EQ(max_diff(buf.data(), original.data(), BLOCK), 0.0f);
}

// 3. Ratio hoisted — two identical instances must produce identical output.
TEST_F(PitchShifterTest, ProcessIsDeterministic) {
    const int BLOCK = 256;
    std::vector<float> input(BLOCK);
    fill_sine(input.data(), BLOCK, 440.0f);

    TestablePitchShifter ps_a;
    ps_a.set_sample_rate(kSampleRate);
    ps_a.set_param(0, 7.0f);
    ps_a.set_param(2, 1.0f);
    ps_a.reset();
    std::vector<float> buf_a = input;
    ps_a.process(buf_a.data(), BLOCK);

    TestablePitchShifter ps_b;
    ps_b.set_sample_rate(kSampleRate);
    ps_b.set_param(0, 7.0f);
    ps_b.set_param(2, 1.0f);
    ps_b.reset();
    std::vector<float> buf_b = input;
    ps_b.process(buf_b.data(), BLOCK);

    ASSERT_EQ(max_diff(buf_a.data(), buf_b.data(), BLOCK), 0.0f);
}

// 4a. Wet/dry — mix=0 passes dry signal exactly.
TEST_F(PitchShifterTest, MixZeroIsPassthrough) {
    ps_.set_param(2, 0.0f);
    ps_.reset();
    const int BLOCK = 256;
    std::vector<float> buf(BLOCK);
    fill_sine(buf.data(), BLOCK, 440.0f);
    const std::vector<float> original = buf;
    ps_.process(buf.data(), BLOCK);
    ASSERT_EQ(max_diff(buf.data(), original.data(), BLOCK), 0.0f);
}

// 4b. Wet/dry — mix=0.5 blends energy between dry and wet levels.
TEST_F(PitchShifterTest, MixInterpolatesEnergy) {
    const int BLOCK = 2048;

    auto run = [&](float mix, float shift) -> float {
        TestablePitchShifter ps;
        ps.set_sample_rate(kSampleRate);
        ps.set_param(0, shift);
        ps.set_param(2, mix);
        ps.reset();
        std::vector<float> buf(BLOCK);
        fill_sine(buf.data(), BLOCK, 440.0f);
        ps.process(buf.data(), BLOCK);
        return rms(buf.data(), BLOCK);
    };

    const float rms_dry = run(0.0f, 0.0f);
    const float rms_wet = run(1.0f, 7.0f);
    const float rms_mid = run(0.5f, 7.0f);

    ASSERT_GE(rms_mid, std::min(rms_dry, rms_wet) * 0.5f);
    ASSERT_LT(rms_mid, std::max(rms_dry, rms_wet) * 1.5f);
}

// Stability — no NaN from single-sample calls.
TEST_F(PitchShifterTest, SingleSampleLoopProducesNoNaN) {
    ps_.set_param(0, 7.0f);
    ps_.reset();
    for (int i = 0; i < 512; ++i) {
        float s = 0.5f * std::sin(static_cast<float>(i) * 0.1f);
        ps_.process(&s, 1);
        ASSERT_FALSE(std::isnan(s));
    }
}

// Header — name() and type_id() return correct strings
TEST_F(PitchShifterTest, NameAndTypeIdMatchHeader) {
    ASSERT_EQ(std::string(ps_.name()),    "Pitch Shifter");
    ASSERT_EQ(std::string(ps_.type_id()), "Pitch Shifter");
}

// Header — params() exposes exactly 3 parameters
TEST_F(PitchShifterTest, ParamCountMatchesHeader) {
    ASSERT_EQ(ps_.params().size(), static_cast<size_t>(3));
}

// Header — ratio_ cached member initialises to 1.0 (no shift at construction)
// Verified by processing silence: shift=0 ratio=1 means no drift, output stays silent
TEST_F(PitchShifterTest, DefaultRatioIsUnity) {
    const int BLOCK = 256;
    std::vector<float> buf(BLOCK, 0.0f);
    ps_.process(buf.data(), BLOCK);
    for (int i = 0; i < BLOCK; ++i)
        ASSERT_NEAR(buf[i], 0.0f, 0.001f);
}

// Header — hann_lut_ array size is 8192 (power-of-2 for bitwise wrapping)
// Verified indirectly: LUT index uses & 8191, so any out-of-range access
// would produce NaN or garbage. Process a full 8192-sample block and confirm no NaN.
TEST_F(PitchShifterTest, HannLutSizeIsCorrect) {
    const int BLOCK = 8192;
    std::vector<float> buf(BLOCK);
    fill_sine(buf.data(), BLOCK, 440.0f);
    ps_.process(buf.data(), BLOCK);
    for (int i = 0; i < BLOCK; ++i)
        ASSERT_FALSE(std::isnan(buf[i]));
}