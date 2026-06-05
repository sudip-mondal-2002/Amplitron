#include <cmath>
#include <iostream>
#include <vector>

#include "audio/effects/delay_reverb/delay.h"
#include "audio/effects/modulation/chorus.h"
#include "audio/engine/tempo_engine.h"
#include "test_framework.h"

using namespace Amplitron;

// Helper to generate a signal with clicks at a regular BPM
static std::vector<float> generate_bpm_signal(float bpm, int sample_rate,
                                              float duration_seconds) {
  int total_samples = static_cast<int>(sample_rate * duration_seconds);
  std::vector<float> signal(total_samples, 0.0f);

  float beat_interval_seconds = 60.0f / bpm;
  int beat_interval_samples =
      static_cast<int>(sample_rate * beat_interval_seconds);

  // Generate short high-frequency noise bursts at each beat
  for (int sample_idx = 0; sample_idx < total_samples;
       sample_idx += beat_interval_samples) {
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

TEST(TempoEngineTest, circular_buffer_wrapping) {
  TempoEngine engine;
  engine.set_sample_rate(48000);

  // Buffer has space for 4 seconds = 192000 samples.
  // Write 5 seconds of silent data to force wrapping.
  std::vector<float> input(48000, 0.0f);
  for (int i = 0; i < 5; ++i) {
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
