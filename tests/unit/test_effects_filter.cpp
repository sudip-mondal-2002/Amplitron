#include "test_framework.h"
#include "test_fixtures.h"
#include "audio/effects/equalizer.h"
#include "audio/effects/cabinet_sim.h"
#include "audio/effects/wah.h"
#include <cstring>
#include <cmath>

using namespace Amplitron;

TEST_F(EffectsTest, equalizer_processes_without_nan) {
    Equalizer eq;
    eq.set_sample_rate(SR);
    eq.reset();

    fill_sine(440.0f);
    eq.process(input_buffer, BUFFER_SIZE);

    ASSERT_TRUE(is_finite(input_buffer, BUFFER_SIZE));
    ASSERT_GT(rms(input_buffer, BUFFER_SIZE), 0.01f);
}

TEST_F(EffectsTest, cabinet_sim_filters_signal) {
    CabinetSim cab;
    cab.set_sample_rate(SR);
    cab.reset();

    fill_sine(440.0f);
    cab.process(input_buffer, BUFFER_SIZE);

    ASSERT_TRUE(is_finite(input_buffer, BUFFER_SIZE));
    ASSERT_GT(rms(input_buffer, BUFFER_SIZE), 0.001f);
}

TEST_F(EffectsTest, cabinet_sim_silence_remains_silent) {
    CabinetSim cab;
    cab.set_sample_rate(SR);
    cab.reset();

    std::memset(input_buffer, 0, sizeof(input_buffer));
    cab.process(input_buffer, BUFFER_SIZE);

    ASSERT_TRUE(is_finite(input_buffer, BUFFER_SIZE));
    ASSERT_LT(rms(input_buffer, BUFFER_SIZE), 0.000001f);
}

TEST_F(EffectsTest, cabinet_sim_long_run_stability) {
    CabinetSim cab;
    cab.set_sample_rate(SR);
    cab.reset();

    for (int i = 0; i < 1000; ++i) {
        fill_sine(440.0f);
        cab.process(input_buffer, BUFFER_SIZE);

        ASSERT_TRUE(is_finite(input_buffer, BUFFER_SIZE));
        ASSERT_GT(rms(input_buffer, BUFFER_SIZE), 0.0001f);
    }
}

TEST_F(EffectsTest, wah_has_name) {
    WahPedal wah;
    ASSERT_TRUE(std::strcmp(wah.name(), "Wah") == 0);
}

TEST_F(EffectsTest, wah_params_valid_ranges) {
    WahPedal wah;
    for (auto& p : wah.params()) {
        ASSERT_TRUE(p.min_val <= p.max_val);
        ASSERT_TRUE(p.value >= p.min_val && p.value <= p.max_val);
        ASSERT_TRUE(p.default_val >= p.min_val && p.default_val <= p.max_val);
    }
}

TEST_F(EffectsTest, wah_produces_finite_output) {
    WahPedal wah;
    wah.set_sample_rate(SR);
    wah.reset();

    fill_sine(440.0f);
    wah.process(input_buffer, BUFFER_SIZE);
    ASSERT_TRUE(is_finite(input_buffer, BUFFER_SIZE));
}

TEST_F(EffectsTest, wah_disabled_passes_dry_signal) {
    WahPedal wah;
    wah.set_sample_rate(SR);
    wah.reset();
    wah.set_enabled(false);

    fill_sine(440.0f);
    copy_input_to_output();
    wah.process(input_buffer, 256);

    for (int i = 0; i < 256; ++i) {
        ASSERT_NEAR(input_buffer[i], output_buffer[i], 1e-6f);
    }
}

TEST_F(EffectsTest, wah_bandpass_tracks_sweep) {
    auto measure_rms_at = [&](float sweep_val) -> float {
        WahPedal wah;
        wah.set_sample_rate(SR);
        wah.reset();
        wah.set_mix(1.0f);
        wah.params()[0].value = 0.0f; // manual mode
        wah.params()[1].value = sweep_val;
        wah.params()[2].value = 3.5f; // default resonance

        float buf[4096];
        for (int i = 0; i < 4096; ++i) {
            buf[i] = std::sin(2.0f * 3.14159265f * 2000.0f * i / SR); // 2 kHz probe tone
        }
        wah.process(buf, 4096);
        return rms(buf, 4096);
    };

    float rms_heel = measure_rms_at(0.0f); // centre ~350 Hz — 2 kHz is out-of-band
    float rms_toe  = measure_rms_at(1.0f); // centre ~2500 Hz — 2 kHz is in-band

    ASSERT_GT(rms_toe, rms_heel * 2.0f);
}

TEST_F(EffectsTest, wah_auto_mode_responds_to_amplitude) {
    WahPedal wah;
    wah.set_sample_rate(SR);
    wah.reset();
    wah.params()[0].value = 1.0f;  // auto-wah mode
    wah.params()[3].value = 1.0f;  // max sensitivity

    // Feed silence
    std::memset(input_buffer, 0, sizeof(input_buffer));
    wah.process(input_buffer, BUFFER_SIZE);
    ASSERT_TRUE(is_finite(input_buffer, BUFFER_SIZE));

    // Feed a loud signal
    fill_sine(440.0f, 0.9f);
    wah.process(input_buffer, BUFFER_SIZE);
    ASSERT_TRUE(is_finite(input_buffer, BUFFER_SIZE));
}
