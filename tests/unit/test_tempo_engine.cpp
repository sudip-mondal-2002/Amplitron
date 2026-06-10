#include <cmath>
#include <iostream>
#include <vector>

#include "audio/effects/delay_reverb/delay.h"
#include "audio/effects/modulation/chorus.h"
#include "audio/engine/tempo_engine.h"
#include "test_framework.h"

using namespace Amplitron;

// Helper to generate a signal with clicks at a regular BPM
static std::vector<float> generate_bpm_signal(float bpm, int sample_rate, float duration_seconds) {
    int total_samples = static_cast<int>(sample_rate * duration_seconds);
    std::vector<float> signal(total_samples, 0.0f);

    float beat_interval_seconds = 60.0f / bpm;
    int beat_interval_samples = static_cast<int>(sample_rate * beat_interval_seconds);

    // Generate short high-frequency noise bursts at each beat
    for (int sample_idx = 0; sample_idx < total_samples; sample_idx += beat_interval_samples) {
        // 15ms burst of sine wave with decay
        int burst_len = static_cast<int>(sample_rate * 0.015f);
        for (int i = 0; i < burst_len && (sample_idx + i) < total_samples; ++i) {
            float t = static_cast<float>(i) / sample_rate;
            float env = std::exp(-200.0f * t);
            signal[sample_idx + i] = env * std::sin(2.0f * 3.14159f * 1000.0f * t);
        }
    }
    return signal;
}

// Helper to generate a kick drum signal at a regular BPM (fast pitch sweep with decay envelope)
static std::vector<float> generate_kick_drum_signal(float bpm, int sample_rate,
                                                    float duration_seconds) {
    int total_samples = static_cast<int>(sample_rate * duration_seconds);
    std::vector<float> signal(total_samples, 0.0f);

    float beat_interval_seconds = 60.0f / bpm;
    int beat_interval_samples = static_cast<int>(sample_rate * beat_interval_seconds);

    for (int sample_idx = 0; sample_idx < total_samples; sample_idx += beat_interval_samples) {
        int kick_len = static_cast<int>(sample_rate * 0.15f);  // 150ms kick sweep
        for (int i = 0; i < kick_len && (sample_idx + i) < total_samples; ++i) {
            float t = static_cast<float>(i) / sample_rate;
            // Sweep from 150Hz down to 45Hz
            float freq = 150.0f * std::exp(-30.0f * t) + 45.0f;
            float env = std::exp(-15.0f * t);
            signal[sample_idx + i] = env * std::sin(2.0f * 3.14159265f * freq * t);
        }
    }
    return signal;
}

// Helper to generate an amplitude-modulated sine wave (pulsed tremolo)
static std::vector<float> generate_modulated_sine_signal(float bpm, int sample_rate,
                                                         float duration_seconds) {
    int total_samples = static_cast<int>(sample_rate * duration_seconds);
    std::vector<float> signal(total_samples, 0.0f);

    float mod_freq = bpm / 60.0f;

    for (int i = 0; i < total_samples; ++i) {
        float t = static_cast<float>(i) / sample_rate;
        // 440 Hz carrier
        float carrier = std::sin(2.0f * 3.14159265f * 440.0f * t);
        // Rhythmic pulsing envelope: full depth amplitude modulation
        float env =
            0.5f * (1.0f + std::sin(2.0f * 3.14159265f * mod_freq * t - 3.14159265f / 2.0f));
        // Make it sharper (more pulse-like) by squaring it
        signal[i] = (env * env) * carrier;
    }
    return signal;
}

// Helper to generate a pure continuous sine wave (no BPM/periodicity)
static std::vector<float> generate_pure_sine_signal(float freq, int sample_rate,
                                                    float duration_seconds) {
    int total_samples = static_cast<int>(sample_rate * duration_seconds);
    std::vector<float> signal(total_samples, 0.0f);
    for (int i = 0; i < total_samples; ++i) {
        float t = static_cast<float>(i) / sample_rate;
        signal[i] = std::sin(2.0f * 3.14159265f * freq * t);
    }
    return signal;
}

// Helper to generate white noise
static std::vector<float> generate_white_noise_signal(int sample_rate, float duration_seconds) {
    int total_samples = static_cast<int>(sample_rate * duration_seconds);
    std::vector<float> signal(total_samples, 0.0f);
    // Simple LCG random generator to be fully deterministic and portable
    uint32_t seed = 12345;
    for (int i = 0; i < total_samples; ++i) {
        seed = seed * 1664525 + 1013904223;
        float val = static_cast<float>(seed) / 4294967296.0f;  // [0, 1)
        signal[i] = val * 2.0f - 1.0f;                         // [-1, 1)
    }
    return signal;
}

TEST(TempoEngineTest, circular_buffer_wrapping) {
    TempoEngine engine;
    engine.set_sample_rate(48000);

    // Buffer has space for 10 seconds = 480000 samples.
    // Write 11 seconds of silent data to force wrapping.
    std::vector<float> input(48000, 0.0f);
    for (int i = 0; i < 11; ++i) {
        engine.write_input(input.data(), 48000);
    }

    // Attempt detection; it should handle the wrapped buffer gracefully (even if
    // silent)
    float bpm = engine.detect_bpm();
    // Silent signal should return -1.0f (no periodic signal detected)
    ASSERT_NEAR(bpm, -1.0f, 1e-6f);
}

TEST(TempoEngineTest, detect_bpm_120_at_48khz) {
    TempoEngine engine;
    int sr = 48000;
    engine.set_sample_rate(sr);

    std::vector<float> signal = generate_bpm_signal(120.0f, sr, 4.2f);

    // Write in chunks to simulate block processing
    const int chunk_size = 256;
    for (size_t i = 0; i < signal.size(); i += chunk_size) {
        int to_write = std::min(static_cast<int>(signal.size() - i), chunk_size);
        engine.write_input(signal.data() + i, to_write);
    }

    float detected = engine.detect_bpm();
    ASSERT_GT(detected, 0.0f);
    std::cout << "Detected BPM (Expected 120.0): " << detected << std::endl;
    ASSERT_NEAR(detected, 120.0f, 2.0f);  // Should be within 2 BPM
}

TEST(TempoEngineTest, detect_bpm_90_at_44_1khz) {
    TempoEngine engine;
    int sr = 44100;
    engine.set_sample_rate(sr);

    std::vector<float> signal = generate_bpm_signal(90.0f, sr, 4.5f);

    const int chunk_size = 256;
    for (size_t i = 0; i < signal.size(); i += chunk_size) {
        int to_write = std::min(static_cast<int>(signal.size() - i), chunk_size);
        engine.write_input(signal.data() + i, to_write);
    }

    float detected = engine.detect_bpm();
    ASSERT_GT(detected, 0.0f);
    std::cout << "Detected BPM (Expected 90.0): " << detected << std::endl;
    ASSERT_NEAR(detected, 90.0f, 2.0f);
}

TEST(TempoEngineTest, detect_bpm_150_at_96khz) {
    TempoEngine engine;
    int sr = 96000;
    engine.set_sample_rate(sr);

    std::vector<float> signal = generate_bpm_signal(150.0f, sr, 4.1f);

    const int chunk_size = 512;
    for (size_t i = 0; i < signal.size(); i += chunk_size) {
        int to_write = std::min(static_cast<int>(signal.size() - i), chunk_size);
        engine.write_input(signal.data() + i, to_write);
    }

    float detected = engine.detect_bpm();
    ASSERT_GT(detected, 0.0f);
    std::cout << "Detected BPM (Expected 150.0): " << detected << std::endl;
    ASSERT_NEAR(detected, 150.0f, 2.0f);
}

TEST(EffectSyncTest, delay_bpm_sync) {
    Delay delay;
    delay.set_sample_rate(48000);
    delay.reset();

    // Verify default parameters
    ASSERT_EQ(delay.params()[4].name, "Sync");
    ASSERT_EQ(delay.params()[5].name, "Subdivision");

    // Enable Sync (1.0f)
    delay.params()[4].value = 1.0f;

    // Set Subdivision to 1/4 (0.0f)
    delay.params()[5].value = 0.0f;
    delay.set_transport_state(120.0f);
    // At 120 BPM, 1/4 note is 500ms
    ASSERT_NEAR(delay.params()[0].value, 500.0f, 1e-4f);

    // Set Subdivision to 1/8 (1.0f)
    delay.params()[5].value = 1.0f;
    delay.set_transport_state(120.0f);
    // At 120 BPM, 1/8 note is 250ms
    ASSERT_NEAR(delay.params()[0].value, 250.0f, 1e-4f);

    // Set Subdivision to 1/16 (2.0f)
    delay.params()[5].value = 2.0f;
    delay.set_transport_state(120.0f);
    // At 120 BPM, 1/16 note is 125ms
    ASSERT_NEAR(delay.params()[0].value, 125.0f, 1e-4f);

    // Set Subdivision to Dotted 1/8 (3.0f)
    delay.params()[5].value = 3.0f;
    delay.set_transport_state(120.0f);
    // At 120 BPM, Dotted 1/8 note is 375ms
    ASSERT_NEAR(delay.params()[0].value, 375.0f, 1e-4f);
}

TEST(EffectSyncTest, chorus_bpm_sync) {
    Chorus chorus;
    chorus.set_sample_rate(48000);
    chorus.reset();

    ASSERT_EQ(chorus.params()[3].name, "Sync");
    ASSERT_EQ(chorus.params()[4].name, "Subdivision");

    chorus.params()[3].value = 1.0f;

    // Subdivision 1/1 (0.0f)
    chorus.params()[4].value = 0.0f;
    chorus.set_transport_state(120.0f);
    // At 120 BPM, LFO Rate = (120/60) * 0.25 = 0.5 Hz
    ASSERT_NEAR(chorus.params()[0].value, 0.5f, 1e-4f);

    // Subdivision 1/4 (2.0f)
    chorus.params()[4].value = 2.0f;
    chorus.set_transport_state(120.0f);
    // At 120 BPM, LFO Rate = (120/60) * 1.0 = 2.0 Hz
    ASSERT_NEAR(chorus.params()[0].value, 2.0f, 1e-4f);
}

TEST(TempoEngineTest, reset_clears_buffer) {
    TempoEngine engine;
    int sr = 48000;
    engine.set_sample_rate(sr);

    // Write some non-zero data
    std::vector<float> input(1000, 1.0f);
    engine.write_input(input.data(), 1000);

    // Call reset
    engine.reset();

    // The next write_input should process the reset request and start fresh.
    std::vector<float> silent_input(1000, 0.0f);
    engine.write_input(silent_input.data(), 1000);

    // At this point, the buffer has been cleared and only has 1000 silent samples.
    // Let's verify that detecting BPM returns -1.0f.
    float detected = engine.detect_bpm();
    ASSERT_NEAR(detected, -1.0f, 1e-6f);
}

TEST(TempoEngineTest, detect_bpm_kick_drum_100_at_48khz) {
    TempoEngine engine;
    int sr = 48000;
    engine.set_sample_rate(sr);

    std::vector<float> signal = generate_kick_drum_signal(100.0f, sr, 4.5f);

    const int chunk_size = 256;
    for (size_t i = 0; i < signal.size(); i += chunk_size) {
        int to_write = std::min(static_cast<int>(signal.size() - i), chunk_size);
        engine.write_input(signal.data() + i, to_write);
    }

    float detected = engine.detect_bpm();
    ASSERT_GT(detected, 0.0f);
    std::cout << "Detected Kick BPM (Expected 100.0): " << detected << std::endl;
    ASSERT_NEAR(detected, 100.0f, 2.0f);
}

TEST(TempoEngineTest, detect_bpm_modulated_sine_120_at_48khz) {
    TempoEngine engine;
    int sr = 48000;
    engine.set_sample_rate(sr);

    std::vector<float> signal = generate_modulated_sine_signal(120.0f, sr, 4.5f);

    const int chunk_size = 256;
    for (size_t i = 0; i < signal.size(); i += chunk_size) {
        int to_write = std::min(static_cast<int>(signal.size() - i), chunk_size);
        engine.write_input(signal.data() + i, to_write);
    }

    float detected = engine.detect_bpm();
    ASSERT_GT(detected, 0.0f);
    std::cout << "Detected Modulated Sine BPM (Expected 120.0): " << detected << std::endl;
    ASSERT_NEAR(detected, 120.0f, 2.0f);
}

TEST(TempoEngineTest, detect_bpm_unmodulated_sine_returns_negative) {
    TempoEngine engine;
    int sr = 48000;
    engine.set_sample_rate(sr);

    std::vector<float> signal = generate_pure_sine_signal(440.0f, sr, 4.5f);

    const int chunk_size = 256;
    for (size_t i = 0; i < signal.size(); i += chunk_size) {
        int to_write = std::min(static_cast<int>(signal.size() - i), chunk_size);
        engine.write_input(signal.data() + i, to_write);
    }

    float detected = engine.detect_bpm();
    // No beat periodicity in pure sine, should return -1.0f
    ASSERT_NEAR(detected, -1.0f, 1e-6f);
}

TEST(TempoEngineTest, detect_bpm_white_noise_returns_negative) {
    TempoEngine engine;
    int sr = 48000;
    engine.set_sample_rate(sr);

    std::vector<float> signal = generate_white_noise_signal(sr, 4.5f);

    const int chunk_size = 256;
    for (size_t i = 0; i < signal.size(); i += chunk_size) {
        int to_write = std::min(static_cast<int>(signal.size() - i), chunk_size);
        engine.write_input(signal.data() + i, to_write);
    }

    float detected = engine.detect_bpm();
    // No periodicity, should return -1.0f
    ASSERT_NEAR(detected, -1.0f, 1e-6f);
}

TEST(EffectSyncTest, delay_and_chorus_extra_sync_and_snap) {
    Delay delay;
    delay.set_sample_rate(48000);
    delay.reset();

    // 1. Invalid BPM checks (Delay)
    delay.set_transport_state(-10.0f);
    delay.set_transport_state(0.0f);
    delay.set_transport_state(NAN);

    // 2. Sync off snapping tests (snaps to closest subdivision)
    delay.params()[4].value = 0.0f; // Sync off
    
    // Quarter note is 500ms (at 120 BPM). If time is 480ms, it should snap to 500ms.
    delay.params()[0].value = 480.0f;
    delay.set_transport_state(120.0f);
    ASSERT_NEAR(delay.params()[0].value, 500.0f, 1e-4f);

    // Toggle BPM
    delay.set_transport_state(60.0f);

    // Eighth note is 250ms. If time is 260ms, it should snap to 250ms.
    delay.params()[0].value = 260.0f;
    delay.set_transport_state(120.0f);
    ASSERT_NEAR(delay.params()[0].value, 250.0f, 1e-4f);

    // Toggle BPM
    delay.set_transport_state(60.0f);

    // Sixteenth note is 125ms. If time is 130ms, it should snap to 125ms.
    delay.params()[0].value = 130.0f;
    delay.set_transport_state(120.0f);
    ASSERT_NEAR(delay.params()[0].value, 125.0f, 1e-4f);

    // Toggle BPM
    delay.set_transport_state(60.0f);

    // Dotted eighth note is 375ms. If time is 370ms, it should snap to 375ms.
    delay.params()[0].value = 370.0f;
    delay.set_transport_state(120.0f);
    ASSERT_NEAR(delay.params()[0].value, 375.0f, 1e-4f);

    // Chorus tests
    Chorus chorus;
    chorus.set_sample_rate(48000);
    chorus.reset();

    // 1. Invalid BPM checks (Chorus)
    chorus.set_transport_state(-10.0f);
    chorus.set_transport_state(0.0f);
    chorus.set_transport_state(NAN);

    // 2. Sync ON subdivisions (0, 1, 2, 3)
    chorus.params()[3].value = 1.0f; // Sync on
    
    // Subdivision 1/1 (0.0f)
    chorus.params()[4].value = 0.0f;
    chorus.set_transport_state(120.0f);
    ASSERT_NEAR(chorus.params()[0].value, 0.5f, 1e-4f);

    // Subdivision 1/2 (1.0f)
    chorus.params()[4].value = 1.0f;
    chorus.set_transport_state(120.0f);
    ASSERT_NEAR(chorus.params()[0].value, 1.0f, 1e-4f);

    // Subdivision 1/4 (2.0f)
    chorus.params()[4].value = 2.0f;
    chorus.set_transport_state(120.0f);
    ASSERT_NEAR(chorus.params()[0].value, 2.0f, 1e-4f);

    // Subdivision 1/8 (3.0f)
    chorus.params()[4].value = 3.0f;
    chorus.set_transport_state(120.0f);
    ASSERT_NEAR(chorus.params()[0].value, 4.0f, 1e-4f);

    // 3. Sync OFF (snaps rate to BPM / 60)
    chorus.params()[3].value = 0.0f;
    chorus.set_transport_state(120.0f);
    ASSERT_NEAR(chorus.params()[0].value, 2.0f, 1e-4f);
}

TEST(TempoEngineTest, detect_bpm_edge_cases) {
    TempoEngine engine;
    
    // 1. Invalid write_input params
    engine.write_input(nullptr, 0);
    engine.write_input(nullptr, -5);

    // 2. circular_buffer_wrapping with insufficient samples
    engine.set_sample_rate(48000);
    std::vector<float> small_input(100, 0.0f);
    engine.write_input(small_input.data(), 100);
    float detected1 = engine.detect_bpm();
    ASSERT_NEAR(detected1, -1.0f, 1e-6f);

    // 3. Trigger num_frames <= 2
    // F_sf calculation and frame size check
    engine.set_sample_rate(200);
    std::vector<float> input2(400, 0.5f);
    engine.write_input(input2.data(), 400);
    float detected2 = engine.detect_bpm();
    ASSERT_NEAR(detected2, -1.0f, 1e-6f);

    // 4. Trigger flux_envelope.size() < 4
    engine.reset();
    engine.set_sample_rate(600);
    std::vector<float> input3(1280, 0.5f);
    engine.write_input(input3.data(), 1280);
    float detected3 = engine.detect_bpm();
    ASSERT_NEAR(detected3, -1.0f, 1e-6f);

    // 5. Trigger min_lag >= max_lag
    engine.reset();
    engine.set_sample_rate(10);
    std::vector<float> input4(30, 0.5f);
    engine.write_input(input4.data(), 30);
    float detected4 = engine.detect_bpm();
    ASSERT_NEAR(detected4, -1.0f, 1e-6f);
}

