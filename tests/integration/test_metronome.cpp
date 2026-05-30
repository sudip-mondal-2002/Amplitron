#include "test_framework.h"
#include "test_fixtures.h"
#include "audio/engine/audio_engine.h"

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