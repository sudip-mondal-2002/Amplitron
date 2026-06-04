#include "test_framework.h"
#include "audio/engine/tempo_math.h"
#include "audio/engine/tempo_engine.h"
#include "audio/effects/delay_reverb/delay.h"
#include "audio/effects/modulation/chorus.h"
#include <chrono>
#include <thread>
#include <cmath>

using namespace Amplitron;

// 1. Test TapTempo logic
TEST(tap_tempo_basic) {
    TapTempo tap_tempo;
    auto now = std::chrono::steady_clock::now();

    // Not enough taps yet
    ASSERT_FALSE(tap_tempo.has_enough_taps());
    ASSERT_LT(tap_tempo.get_bpm(now), 0.0f);

    // Tap at 500ms intervals (120 BPM)
    for (int i = 0; i < 5; ++i) {
        tap_tempo.tap(now + std::chrono::milliseconds(i * 500));
    }

    ASSERT_TRUE(tap_tempo.has_enough_taps());
    float bpm = tap_tempo.get_bpm(now + std::chrono::milliseconds(2000));
    // Expect BPM to be close to 120.0f
    ASSERT_TRUE(std::fabs(bpm - 120.0f) < 2.0f);
}

TEST(tap_tempo_outliers) {
    TapTempo tap_tempo;
    auto now = std::chrono::steady_clock::now();

    // 5 taps: 500ms, 500ms, 900ms (outlier), 500ms
    tap_tempo.tap(now);
    tap_tempo.tap(now + std::chrono::milliseconds(500));
    tap_tempo.tap(now + std::chrono::milliseconds(1000));
    tap_tempo.tap(now + std::chrono::milliseconds(1900)); // Outlier
    tap_tempo.tap(now + std::chrono::milliseconds(2400));

    // Trimmed mean should reject the 900ms interval
    float bpm = tap_tempo.get_bpm(now + std::chrono::milliseconds(2400));
    ASSERT_TRUE(std::fabs(bpm - 120.0f) < 5.0f);
}

TEST(tap_tempo_inactivity_reset) {
    TapTempo tap_tempo;
    auto now = std::chrono::steady_clock::now();

    tap_tempo.tap(now);
    tap_tempo.tap(now + std::chrono::milliseconds(500));
    ASSERT_TRUE(tap_tempo.has_enough_taps());

    // Inactivity of 5 seconds
    tap_tempo.tap(now + std::chrono::milliseconds(5500));
    // History should have reset, so only 1 tap remains in history -> not enough taps!
    ASSERT_FALSE(tap_tempo.has_enough_taps());
}

// 2. Test TempoEngine autocorrelation beat detection
TEST(tempo_engine_detection) {
    TempoEngine tempo_engine;
    tempo_engine.set_sample_rate(44100);

    // Create a 4-second input buffer with pulses at 120 BPM (every 500ms / 22050 samples)
    std::vector<float> audio_data(4 * 44100, 0.0f);
    int pulse_interval = 44100 * 60 / 120; // 22050
    for (int i = 0; i < 4 * 44100; i += pulse_interval) {
        // Pulse burst
        for (int p = 0; p < 200 && (i + p) < 4 * 44100; ++p) {
            audio_data[i + p] = 0.5f * std::sin(2.0f * 3.14159f * 440.0f * p / 44100.0f);
        }
    }

    tempo_engine.write_input(audio_data.data(), static_cast<int>(audio_data.size()));
    float bpm = tempo_engine.detect_bpm();

    // Autocorrelation should detect around 120 BPM
    ASSERT_TRUE(bpm > 0.0f);
    ASSERT_TRUE(std::fabs(bpm - 120.0f) < 5.0f);
}

// 3. Test Delay Sync parameter override
TEST(delay_sync_parameter) {
    Delay delay;
    // Set Sync to ON
    delay.params()[4].value = 1.0f; // Sync parameter
    delay.params()[5].value = 0.0f; // Subdivision 1/4 (multiplier 1.0)

    delay.set_transport_state(100.0f); // 100 BPM
    // Quarter note at 100 BPM = 600ms = 60000 / 100
    ASSERT_NEAR(delay.params()[0].value, 600.0f, 0.1f);

    // Change subdivision to 1/8 (multiplier 0.5)
    delay.params()[5].value = 1.0f;
    // Run process to update parameters
    std::vector<float> buf(256, 0.0f);
    delay.process(buf.data(), 256);
    ASSERT_NEAR(delay.params()[0].value, 300.0f, 0.1f);
}

// 4. Test Chorus Sync parameter override
TEST(chorus_sync_parameter) {
    Chorus chorus;
    // Set Sync to ON
    chorus.params()[3].value = 1.0f; // Sync parameter
    chorus.params()[4].value = 0.0f; // Subdivision 1/4 (multiplier 1.0)

    chorus.set_transport_state(120.0f); // 120 BPM
    // 120 BPM = 2.0 Hz rate (120 / 60)
    ASSERT_NEAR(chorus.params()[0].value, 2.0f, 0.05f);

    // Change subdivision to 1/16 (multiplier 4.0)
    chorus.params()[4].value = 2.0f;
    std::vector<float> buf(256, 0.0f);
    chorus.process(buf.data(), 256);
    ASSERT_NEAR(chorus.params()[0].value, 8.0f, 0.05f);
}
