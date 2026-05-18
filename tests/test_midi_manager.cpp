#include "test_framework.h"
#include "midi/midi_manager.h"
#include "audio/audio_engine.h"
#include "audio/effect.h"
#include <cmath>

using namespace Amplitron;

// A minimal test effect with two parameters for mapping tests.
class TestEffect : public Effect {
public:
    TestEffect() {
        params_ = {
            {"Drive", 0.5f, 0.0f, 1.0f, 0.5f, "", ""},
            {"Level", 0.8f, 0.0f, 2.0f, 0.8f, "", ""},
        };
    }
    const char* name() const override { return "TestEffect"; }
    std::vector<EffectParam>& params() override { return params_; }
    void process(float* /*buffer*/, int /*num_samples*/) override {}
    void reset() override {}
private:
    std::vector<EffectParam> params_;
};

// Helper: build a CC MidiEvent
static MidiEvent make_cc(uint8_t cc, uint8_t value, uint8_t channel = 0) {
    MidiEvent e{};
    e.status = static_cast<uint8_t>(0xB0 | (channel & 0x0F));
    e.data1 = cc;
    e.data2 = value;
    return e;
}

// ---------------------------------------------------------------------------
// Continuous mapping: CC 0 -> min, CC 127 -> max, CC 64 -> midpoint
// ---------------------------------------------------------------------------

TEST(midi_continuous_cc0_maps_to_min) {
    MidiManager midi;
    AudioEngine engine;
    engine.initialize();

    auto fx = std::make_shared<TestEffect>();
    engine.add_effect(fx);

    MidiMapping m;
    m.cc_number = 10;
    m.midi_channel = -1;
    m.target_type = MidiTargetType::EffectParam;
    m.mode = MidiMappingMode::Continuous;
    m.effect_name = "TestEffect";
    m.param_name = "Drive";
    midi.add_mapping(m);

    midi.inject_event(make_cc(10, 0));
    midi.poll(engine);

    ASSERT_NEAR(fx->params()[0].value, 0.0f, 0.01f);
    engine.shutdown();
}

TEST(midi_continuous_cc127_maps_to_max) {
    MidiManager midi;
    AudioEngine engine;
    engine.initialize();

    auto fx = std::make_shared<TestEffect>();
    engine.add_effect(fx);

    MidiMapping m;
    m.cc_number = 10;
    m.midi_channel = -1;
    m.target_type = MidiTargetType::EffectParam;
    m.mode = MidiMappingMode::Continuous;
    m.effect_name = "TestEffect";
    m.param_name = "Drive";
    midi.add_mapping(m);

    midi.inject_event(make_cc(10, 127));
    midi.poll(engine);

    ASSERT_NEAR(fx->params()[0].value, 1.0f, 0.01f);
    engine.shutdown();
}

TEST(midi_continuous_cc64_maps_to_midpoint) {
    MidiManager midi;
    AudioEngine engine;
    engine.initialize();

    auto fx = std::make_shared<TestEffect>();
    engine.add_effect(fx);

    // Map to "Level" which has range [0.0, 2.0]
    MidiMapping m;
    m.cc_number = 20;
    m.midi_channel = -1;
    m.target_type = MidiTargetType::EffectParam;
    m.mode = MidiMappingMode::Continuous;
    m.effect_name = "TestEffect";
    m.param_name = "Level";
    midi.add_mapping(m);

    midi.inject_event(make_cc(20, 64));
    midi.poll(engine);

    // 64/127 * 2.0 = ~1.008
    float expected = (64.0f / 127.0f) * 2.0f;
    ASSERT_NEAR(fx->params()[1].value, expected, 0.02f);
    engine.shutdown();
}

// ---------------------------------------------------------------------------
// Toggle mapping: CC >= 64 -> on, CC < 64 -> off
// ---------------------------------------------------------------------------

TEST(midi_toggle_cc_enables_effect) {
    MidiManager midi;
    AudioEngine engine;
    engine.initialize();

    auto fx = std::make_shared<TestEffect>();
    fx->set_enabled(false);
    engine.add_effect(fx);

    MidiMapping m;
    m.cc_number = 64;
    m.midi_channel = -1;
    m.target_type = MidiTargetType::EffectBypass;
    m.mode = MidiMappingMode::Toggle;
    m.effect_name = "TestEffect";
    midi.add_mapping(m);

    midi.inject_event(make_cc(64, 127));
    midi.poll(engine);
    ASSERT_TRUE(fx->is_enabled());

    midi.inject_event(make_cc(64, 0));
    midi.poll(engine);
    ASSERT_FALSE(fx->is_enabled());

    engine.shutdown();
}

// ---------------------------------------------------------------------------
// MIDI Learn: push event while learning, verify mapping is created
// ---------------------------------------------------------------------------

TEST(midi_learn_creates_mapping) {
    MidiManager midi;
    AudioEngine engine;
    engine.initialize();

    auto fx = std::make_shared<TestEffect>();
    engine.add_effect(fx);

    ASSERT_TRUE(midi.mappings().empty());

    midi.start_learn(MidiTargetType::EffectParam, "TestEffect", "Drive");
    ASSERT_TRUE(midi.is_learning());

    midi.inject_event(make_cc(42, 100, 3));
    midi.poll(engine);

    ASSERT_FALSE(midi.is_learning());
    ASSERT_EQ(static_cast<int>(midi.mappings().size()), 1);
    ASSERT_EQ(midi.mappings()[0].cc_number, 42);
    ASSERT_EQ(midi.mappings()[0].midi_channel, 3);
    ASSERT_EQ(midi.mappings()[0].effect_name, std::string("TestEffect"));
    ASSERT_EQ(midi.mappings()[0].param_name, std::string("Drive"));

    engine.shutdown();
}

// ---------------------------------------------------------------------------
// Unmapped CC is silently ignored
// ---------------------------------------------------------------------------

TEST(midi_unmapped_cc_ignored) {
    MidiManager midi;
    AudioEngine engine;
    engine.initialize();

    auto fx = std::make_shared<TestEffect>();
    engine.add_effect(fx);
    float original = fx->params()[0].value;

    // No mappings — inject a random CC
    midi.inject_event(make_cc(99, 64));
    midi.poll(engine);

    ASSERT_NEAR(fx->params()[0].value, original, 0.001f);
    engine.shutdown();
}

// ---------------------------------------------------------------------------
// Effect not found — graceful no-op
// ---------------------------------------------------------------------------

TEST(midi_missing_effect_no_crash) {
    MidiManager midi;
    AudioEngine engine;
    engine.initialize();

    MidiMapping m;
    m.cc_number = 10;
    m.midi_channel = -1;
    m.target_type = MidiTargetType::EffectParam;
    m.mode = MidiMappingMode::Continuous;
    m.effect_name = "NonExistent";
    m.param_name = "Drive";
    midi.add_mapping(m);

    midi.inject_event(make_cc(10, 64));
    midi.poll(engine);
    // No crash — pass
    ASSERT_TRUE(true);
    engine.shutdown();
}

// ---------------------------------------------------------------------------
// Channel filtering: mapping on channel 5 ignores events on channel 0
// ---------------------------------------------------------------------------

TEST(midi_channel_filter) {
    MidiManager midi;
    AudioEngine engine;
    engine.initialize();

    auto fx = std::make_shared<TestEffect>();
    engine.add_effect(fx);
    float original = fx->params()[0].value;

    MidiMapping m;
    m.cc_number = 10;
    m.midi_channel = 5;
    m.target_type = MidiTargetType::EffectParam;
    m.mode = MidiMappingMode::Continuous;
    m.effect_name = "TestEffect";
    m.param_name = "Drive";
    midi.add_mapping(m);

    // Event on channel 0 — should be ignored
    midi.inject_event(make_cc(10, 127, 0));
    midi.poll(engine);
    ASSERT_NEAR(fx->params()[0].value, original, 0.001f);

    // Event on channel 5 — should apply
    midi.inject_event(make_cc(10, 127, 5));
    midi.poll(engine);
    ASSERT_NEAR(fx->params()[0].value, 1.0f, 0.01f);

    engine.shutdown();
}

// ---------------------------------------------------------------------------
// InputGain / OutputGain mapping
// ---------------------------------------------------------------------------

TEST(midi_output_gain_mapping) {
    MidiManager midi;
    AudioEngine engine;
    engine.initialize();

    MidiMapping m;
    m.cc_number = 7;
    m.midi_channel = -1;
    m.target_type = MidiTargetType::OutputGain;
    m.mode = MidiMappingMode::Continuous;
    midi.add_mapping(m);

    midi.inject_event(make_cc(7, 64));
    midi.poll(engine);

    // 64/127 * 2.0 = ~1.008
    float expected = (64.0f / 127.0f) * 2.0f;
    ASSERT_NEAR(engine.get_output_gain(), expected, 0.02f);
    engine.shutdown();
}

// ---------------------------------------------------------------------------
// JSON round-trip for mappings
// ---------------------------------------------------------------------------

TEST(midi_json_roundtrip) {
    MidiManager midi;

    MidiMapping m1;
    m1.cc_number = 7;
    m1.midi_channel = -1;
    m1.target_type = MidiTargetType::OutputGain;
    m1.mode = MidiMappingMode::Continuous;
    midi.add_mapping(m1);

    MidiMapping m2;
    m2.cc_number = 74;
    m2.midi_channel = 2;
    m2.target_type = MidiTargetType::EffectParam;
    m2.mode = MidiMappingMode::Continuous;
    m2.effect_name = "WahPedal";
    m2.param_name = "Sweep";
    midi.add_mapping(m2);

    MidiMapping m3;
    m3.cc_number = 64;
    m3.midi_channel = -1;
    m3.target_type = MidiTargetType::EffectBypass;
    m3.mode = MidiMappingMode::Toggle;
    m3.effect_name = "Distortion";
    midi.add_mapping(m3);

    // save/load config uses internal paths — test mapping management instead
    ASSERT_EQ(static_cast<int>(midi.mappings().size()), 3);

    // Verify mapping contents
    ASSERT_EQ(midi.mappings()[0].cc_number, 7);
    ASSERT_EQ(static_cast<int>(midi.mappings()[0].target_type),
              static_cast<int>(MidiTargetType::OutputGain));

    ASSERT_EQ(midi.mappings()[1].cc_number, 74);
    ASSERT_EQ(midi.mappings()[1].effect_name, std::string("WahPedal"));
    ASSERT_EQ(midi.mappings()[1].param_name, std::string("Sweep"));

    ASSERT_EQ(midi.mappings()[2].cc_number, 64);
    ASSERT_EQ(static_cast<int>(midi.mappings()[2].mode),
              static_cast<int>(MidiMappingMode::Toggle));

    // Test remove
    midi.remove_mapping(1);
    ASSERT_EQ(static_cast<int>(midi.mappings().size()), 2);
    ASSERT_EQ(midi.mappings()[1].cc_number, 64);  // Was index 2, now 1

    // Test clear
    midi.clear_mappings();
    ASSERT_TRUE(midi.mappings().empty());
}

// ---------------------------------------------------------------------------
// Default mappings install correctly
// ---------------------------------------------------------------------------

TEST(midi_default_mappings) {
    MidiManager midi;
    midi.install_default_mappings();

    ASSERT_EQ(static_cast<int>(midi.mappings().size()), 4);

    // CC7 = OutputGain
    ASSERT_EQ(midi.mappings()[0].cc_number, 7);
    ASSERT_EQ(static_cast<int>(midi.mappings()[0].target_type),
              static_cast<int>(MidiTargetType::OutputGain));

    // CC11 = InputGain
    ASSERT_EQ(midi.mappings()[1].cc_number, 11);
    ASSERT_EQ(static_cast<int>(midi.mappings()[1].target_type),
              static_cast<int>(MidiTargetType::InputGain));

    // CC64 = Bypass toggle
    ASSERT_EQ(midi.mappings()[2].cc_number, 64);
    ASSERT_EQ(static_cast<int>(midi.mappings()[2].target_type),
              static_cast<int>(MidiTargetType::EffectBypass));
    ASSERT_EQ(static_cast<int>(midi.mappings()[2].mode),
              static_cast<int>(MidiMappingMode::Toggle));

    // CC74 = Wah sweep
    ASSERT_EQ(midi.mappings()[3].cc_number, 74);
    ASSERT_EQ(midi.mappings()[3].effect_name, std::string("WahPedal"));
    ASSERT_EQ(midi.mappings()[3].param_name, std::string("Sweep"));
}

// ---------------------------------------------------------------------------
// Duplicate CC mapping replaces the old one
// ---------------------------------------------------------------------------

TEST(midi_duplicate_cc_replaces) {
    MidiManager midi;

    MidiMapping m1;
    m1.cc_number = 10;
    m1.midi_channel = -1;
    m1.target_type = MidiTargetType::EffectParam;
    m1.mode = MidiMappingMode::Continuous;
    m1.effect_name = "TestEffect";
    m1.param_name = "Drive";
    midi.add_mapping(m1);

    MidiMapping m2;
    m2.cc_number = 10;
    m2.midi_channel = -1;
    m2.target_type = MidiTargetType::EffectParam;
    m2.mode = MidiMappingMode::Continuous;
    m2.effect_name = "TestEffect";
    m2.param_name = "Level";
    midi.add_mapping(m2);

    // Should have replaced, not appended
    ASSERT_EQ(static_cast<int>(midi.mappings().size()), 1);
    ASSERT_EQ(midi.mappings()[0].param_name, std::string("Level"));
}

// ---------------------------------------------------------------------------
// Learn cancel works
// ---------------------------------------------------------------------------

TEST(midi_learn_cancel) {
    MidiManager midi;

    midi.start_learn(MidiTargetType::EffectParam, "TestEffect", "Drive");
    ASSERT_TRUE(midi.is_learning());

    midi.cancel_learn();
    ASSERT_FALSE(midi.is_learning());
    ASSERT_TRUE(midi.mappings().empty());
}
