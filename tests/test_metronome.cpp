#include "test_framework.h"
#include "audio/audio_engine.h"

using namespace Amplitron;

// 1. Test basic initialization and defaults
TEST(metronome_initial_state) {
    AudioEngine engine;
    engine.initialize();

    ASSERT_FALSE(engine.get_metronome_enabled());
    ASSERT_EQ(engine.get_metronome_bpm(), 120);
    ASSERT_EQ(engine.get_metronome_volume(), 0.5f);

    engine.shutdown();
}

// 2. Test toggle functionality
TEST(metronome_toggle_state) {
    AudioEngine engine;
    engine.initialize();

    engine.toggle_metronome();
    ASSERT_TRUE(engine.get_metronome_enabled());

    engine.toggle_metronome();
    ASSERT_FALSE(engine.get_metronome_enabled());

    engine.shutdown();
}

// 3. Test BPM boundaries and targets (40 to 240)
TEST(metronome_bpm_boundaries) {
    AudioEngine engine;
    engine.initialize();

    // Test lower boundary
    engine.set_metronome_bpm(40);
    ASSERT_EQ(engine.get_metronome_bpm(), 40);

    // Test standard target
    engine.set_metronome_bpm(120);
    ASSERT_EQ(engine.get_metronome_bpm(), 120);

    // Test upper boundary
    engine.set_metronome_bpm(240);
    ASSERT_EQ(engine.get_metronome_bpm(), 240);

    engine.shutdown();
}

// 4. Test sample rate sync (core timing fix)
TEST(metronome_sample_rate_sync) {
    AudioEngine engine;
    engine.initialize();

    engine.set_sample_rate(48000);
    engine.start();
    ASSERT_EQ(engine.get_sample_rate(), 48000);

    engine.shutdown();
}

// 5. Test volume parameter targeting
TEST(metronome_volume_targeting) {
    AudioEngine engine;
    engine.initialize();

    engine.set_metronome_volume(0.9f);
    ASSERT_EQ(engine.get_metronome_volume(), 0.9f);

    engine.set_metronome_volume(0.0f);
    ASSERT_EQ(engine.get_metronome_volume(), 0.0f);

    engine.shutdown();
}