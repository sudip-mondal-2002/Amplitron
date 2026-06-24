#include "audio/engine/audio_engine.h"
#include "audio/engine/metronome.h"
#include "test_fixtures.h"
#include "test_framework.h"

using namespace Amplitron;

// 1. Test basic initialization and defaults
TEST_F(AudioEngineTest, metronome_initial_state) {
    ASSERT_FALSE(engine.get_metronome_enabled());
    ASSERT_EQ(engine.get_metronome_bpm(), 120);
    ASSERT_EQ(engine.get_metronome_volume(), 0.5f);
}

// 2. Test toggle functionality
TEST_F(AudioEngineTest, metronome_toggle_state) {
    engine.toggle_metronome();
    ASSERT_TRUE(engine.get_metronome_enabled());

    engine.toggle_metronome();
    ASSERT_FALSE(engine.get_metronome_enabled());
}

// 3. Test BPM boundaries and targets (40 to 240)
TEST_F(AudioEngineTest, metronome_bpm_boundaries) {
    // Test lower boundary
    engine.set_metronome_bpm(40);
    ASSERT_EQ(engine.get_metronome_bpm(), 40);

    // Test standard target
    engine.set_metronome_bpm(120);
    ASSERT_EQ(engine.get_metronome_bpm(), 120);

    // Test upper boundary
    engine.set_metronome_bpm(240);
    ASSERT_EQ(engine.get_metronome_bpm(), 240);
}

// 4. Test sample rate sync (core timing fix)
TEST_F(AudioEngineTest, metronome_sample_rate_sync) {
    engine.set_sample_rate(48000);
    engine.start();
    ASSERT_EQ(engine.get_sample_rate(), 48000);
}

// 5. Test volume parameter targeting
TEST_F(AudioEngineTest, metronome_volume_targeting) {
    engine.set_metronome_volume(0.9f);
    ASSERT_EQ(engine.get_metronome_volume(), 0.9f);

    engine.set_metronome_volume(0.0f);
    ASSERT_EQ(engine.get_metronome_volume(), 0.0f);
}

TEST(Metronome_DirectUnitTests) {
    Metronome met;

    // 1. Initial State
    ASSERT_FALSE(met.is_enabled());
    ASSERT_EQ(met.get_bpm(), 120);
    ASSERT_NEAR(met.get_volume(), 0.5f, 1e-6f);

    // 2. Toggle and Setters
    met.toggle();
    ASSERT_TRUE(met.is_enabled());
    met.set_bpm(10);  // clamps to 40
    ASSERT_EQ(met.get_bpm(), 40);
    met.set_bpm(300);  // clamps to 240
    ASSERT_EQ(met.get_bpm(), 240);
    met.set_volume(-0.5f);  // clamps to 0.0
    ASSERT_NEAR(met.get_volume(), 0.0f, 1e-6f);
    met.set_volume(1.5f);  // clamps to 1.0
    ASSERT_NEAR(met.get_volume(), 1.0f, 1e-6f);

    // 3. Sample Rate edge case <= 0
    met.set_sample_rate(0);
    met.test_samples_per_beat() = 0.0;
    ASSERT_NEAR(met.test_samples_per_beat(), 0.0, 1e-6f);

    // Restore sample rate
    met.set_sample_rate(48000);

    // 4. Reset
    met.reset();
    ASSERT_NEAR(met.test_sample_counter(), 0.0, 1e-6f);
    ASSERT_EQ(met.test_click_samples_remaining(), 0);

    // 5. samples_per_beat < 1.0 (Low sample rate + High BPM)
    met.set_sample_rate(1);
    met.set_bpm(240);
    ASSERT_NEAR(met.test_samples_per_beat(), 1.0, 1e-6f);

    // 6. next_sample edge cases
    met.set_sample_rate(48000);
    met.set_bpm(120);
    met.set_volume(1.0f);
    met.set_enabled(true);

    // Call next_sample() once to process state changes
    met.next_sample();

    // Phase wrap-around
    bool generated_click = false;
    for (int i = 0; i < 100; ++i) {
        float sample = met.next_sample();
        if (std::abs(sample) > 0.0f) {
            generated_click = true;
        }
    }
    ASSERT_TRUE(generated_click);

    // 7. timing_dirty with counter out of bounds
    met.set_bpm(60);
    met.test_sample_counter() = -5.0;  // out of bounds (<= 0)
    met.next_sample();                 // timing_dirty reset counter to samples_per_beat
    ASSERT_GT(met.test_sample_counter(), 0.0);

    // 8. timing_dirty with counter > samples_per_beat
    met.set_bpm(40);
    met.next_sample();
    met.test_sample_counter() = 50000.0;
    met.set_bpm(240);
    met.next_sample();  // timing_dirty should trigger: sample_counter > samples_per_beat (12000.0)
    ASSERT_NEAR(met.test_sample_counter(), 11999.0, 1e-6f);
}
