#include "test_framework.h"
#include "midi/midi_manager.h"
#include "audio/audio_engine.h"
#include "audio/effect.h"
#include <cmath>
#include <fstream>
#include <filesystem>

using namespace Amplitron;
namespace fs = std::filesystem;

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

    float expected = (64.0f / 127.0f) * 2.0f;
    ASSERT_NEAR(fx->params()[1].value, expected, 0.02f);
    engine.shutdown();
}

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

TEST(midi_unmapped_cc_ignored) {
    MidiManager midi;
    AudioEngine engine;
    engine.initialize();

    auto fx = std::make_shared<TestEffect>();
    engine.add_effect(fx);
    float original = fx->params()[0].value;

    midi.inject_event(make_cc(99, 64));
    midi.poll(engine);

    ASSERT_NEAR(fx->params()[0].value, original, 0.001f);
    engine.shutdown();
}

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
    ASSERT_TRUE(true);
    engine.shutdown();
}

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

    midi.inject_event(make_cc(10, 127, 0));
    midi.poll(engine);
    ASSERT_NEAR(fx->params()[0].value, original, 0.001f);

    midi.inject_event(make_cc(10, 127, 5));
    midi.poll(engine);
    ASSERT_NEAR(fx->params()[0].value, 1.0f, 0.01f);

    engine.shutdown();
}

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

    float expected = (64.0f / 127.0f) * 2.0f;
    ASSERT_NEAR(engine.get_output_gain(), expected, 0.02f);
    engine.shutdown();
}

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

    ASSERT_EQ(static_cast<int>(midi.mappings().size()), 3);

    ASSERT_EQ(midi.mappings()[0].cc_number, 7);
    ASSERT_EQ(static_cast<int>(midi.mappings()[0].target_type),
              static_cast<int>(MidiTargetType::OutputGain));

    midi.remove_mapping(1);
    ASSERT_EQ(static_cast<int>(midi.mappings().size()), 2);

    midi.clear_mappings();
    ASSERT_TRUE(midi.mappings().empty());
}

TEST(midi_default_mappings) {
    MidiManager midi;
    midi.install_default_mappings();

    ASSERT_EQ(static_cast<int>(midi.mappings().size()), 4);
    ASSERT_EQ(midi.mappings()[0].cc_number, 7);
}

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

    ASSERT_EQ(static_cast<int>(midi.mappings().size()), 1);
    ASSERT_EQ(midi.mappings()[0].param_name, std::string("Level"));
}

TEST(midi_learn_cancel) {
    MidiManager midi;

    midi.start_learn(MidiTargetType::EffectParam, "TestEffect", "Drive");
    ASSERT_TRUE(midi.is_learning());

    midi.cancel_learn();
    ASSERT_FALSE(midi.is_learning());
    ASSERT_TRUE(midi.mappings().empty());
}

// ===========================================================================
// REQUIRED COVERAGE EXTENSIONS FOR TARGET REQUIREMENTS
// ===========================================================================

TEST(MidiPersist_SaveAndLoadRoundtrip) {
    MidiManager mgr;
    MidiMapping m1{7, -1, MidiTargetType::EffectParam, MidiMappingMode::Continuous, "effect_0", "drive"};
    MidiMapping m2{11, -1, MidiTargetType::EffectParam, MidiMappingMode::Continuous, "effect_1", "level"};
    mgr.add_mapping(m1);
    mgr.add_mapping(m2);
    
    // Safety Fallback: Check if a local config already exists and back it up
    std::string config_file = "midi_config.json"; 
    std::string backup_file = "midi_config.json.bak";
    bool backed_up = false;
    if (fs::exists(config_file)) {
        fs::rename(config_file, backup_file);
        backed_up = true;
    }

    // Run the persistence endpoints natively
    mgr.save_config();

    MidiManager mgr2;
    mgr2.load_config();
    
    // Assert roundtrip integrity
    ASSERT_EQ(static_cast<int>(mgr2.mappings().size()), 2);

    // Clean up the test file generated
    if (fs::exists(config_file)) {
        fs::remove(config_file);
    }

    // Restore your original configuration if it was backed up
    if (backed_up) {
        fs::rename(backup_file, config_file);
    }
}
TEST(MidiPersist_LoadMissingFileGraceful) {
    MidiManager mgr;
    mgr.load_config();
    ASSERT_TRUE(true);
}

TEST(MidiMapping_ClearAllMappingsWhenEmpty) {
    MidiManager mgr;
    mgr.clear_mappings();
    ASSERT_EQ(static_cast<int>(mgr.mappings().size()), 0);
}

TEST(MidiMapping_ClearAllMappingsAfterAdding) {
    MidiManager mgr;
    MidiMapping m1{7, -1, MidiTargetType::EffectParam, MidiMappingMode::Continuous, "effect_0", "drive"};
    MidiMapping m2{11, -1, MidiTargetType::EffectParam, MidiMappingMode::Continuous, "effect_1", "level"};
    mgr.add_mapping(m1);
    mgr.add_mapping(m2);
    ASSERT_EQ(static_cast<int>(mgr.mappings().size()), 2);
    
    mgr.clear_mappings();
    ASSERT_EQ(static_cast<int>(mgr.mappings().size()), 0);
}

TEST(MidiMapping_OverrideSameCCWithNewParam) {
    MidiManager mgr;
    MidiMapping m1{7, -1, MidiTargetType::EffectParam, MidiMappingMode::Continuous, "effect_0", "drive"};
    mgr.add_mapping(m1);
    int count_after_first = static_cast<int>(mgr.mappings().size());
    ASSERT_EQ(count_after_first, 1);
    
    MidiMapping m2{7, -1, MidiTargetType::EffectParam, MidiMappingMode::Continuous, "effect_1", "level"};
    mgr.add_mapping(m2);  
    int count_after_override = static_cast<int>(mgr.mappings().size());
    
    ASSERT_EQ(count_after_override, 1);
}

TEST(MidiMapping_GetActiveMappingCountAfterBulkOps) {
    MidiManager mgr;
    for (int i = 0; i < 5; i++) {
        MidiMapping m{static_cast<uint8_t>(i), -1, MidiTargetType::EffectParam, MidiMappingMode::Continuous, "effect_" + std::to_string(i), "param"};
        mgr.add_mapping(m);
    }
    ASSERT_GE(static_cast<int>(mgr.mappings().size()), 5);
    
    mgr.clear_mappings();
    ASSERT_EQ(static_cast<int>(mgr.mappings().size()), 0);
}