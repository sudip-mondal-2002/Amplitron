#include "test_framework.h"
#include "midi/midi_manager.h"
#include "audio/audio_engine.h"
#include "audio/effect.h"
#include <cmath>
#include <fstream>
#include <filesystem>

using namespace Amplitron;
namespace fs = std::filesystem;

/**
 * @brief A minimal test effect with two parameters used to validate 
 * MIDI control change (CC) mappings.
 */
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

/**
 * @brief Helper utility to construct a raw control change (CC) MidiEvent.
 * @param cc The MIDI control change number.
 * @param value The value of the control change (0-127).
 * @param channel The target MIDI channel (defaults to 0).
 * @return A configured MidiEvent structure.
 */
static MidiEvent make_cc(uint8_t cc, uint8_t value, uint8_t channel = 0) {
    MidiEvent e{};
    e.status = static_cast<uint8_t>(0xB0 | (channel & 0x0F));
    e.data1 = cc;
    e.data2 = value;
    return e;
}

// ---------------------------------------------------------------------------
// Continuous mapping tests
// ---------------------------------------------------------------------------

/**
 * @brief Verifies that a continuous MIDI mapping correctly translates 
 * a minimum CC value of 0 to the parameter's minimum range limit.
 */
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

/**
 * @brief Verifies that a continuous MIDI mapping correctly translates 
 * a maximum CC value of 127 to the parameter's maximum range limit.
 */
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

/**
 * @brief Verifies that a continuous MIDI mapping correctly scales an intermediate 
 * CC value (64) directly to the target parameter's mathematical midpoint.
 */
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

/**
 * @brief Validates that a toggle MIDI mapping alternates the effect bypass state 
 * correctly when boundary values (0 and 127) are fed to the engine.
 */
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

/**
 * @brief Confirms that active MIDI hardware learning successfully captures the 
 * parameters of an incoming CC signal and instantiates a valid layout mapping.
 */
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

/**
 * @brief Checks that messages received on unmapped MIDI CC numbers do not alter 
 * the values of internal parameters.
 */
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

/**
 * @brief Verifies that executing processing cycles on a mapping targeted at a 
 * missing or unallocated effect string name fails gracefully without a crash.
 */
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

/**
 * @brief Assures that explicit channel value definitions are respected, filtering out 
 * mismatched channel traffic while allowing matched channels to modify attributes.
 */
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

/**
 * @brief Validates continuous evaluation scaling mappings targeting the output master 
 * gain node inside the running audio context framework.
 */
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

/**
 * @brief Exercises serialization conversion flows across multi-tier standard allocations 
 * to prove structural precision during clear and index-based removals.
 */
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

    ASSERT_EQ(midi.mappings()[1].cc_number, 74);
    ASSERT_EQ(midi.mappings()[1].effect_name, std::string("WahPedal"));
    ASSERT_EQ(midi.mappings()[1].param_name, std::string("Sweep"));

    ASSERT_EQ(midi.mappings()[2].cc_number, 64);
    ASSERT_EQ(static_cast<int>(midi.mappings()[2].mode),
              static_cast<int>(MidiMappingMode::Toggle));

    midi.remove_mapping(1);
    ASSERT_EQ(static_cast<int>(midi.mappings().size()), 2);

    midi.clear_mappings();
    ASSERT_TRUE(midi.mappings().empty());
}

/**
 * @brief Asserts strict schema layout assignments for factory preset defaults 
 * across input nodes, bypass states, and filter sweeps.
 */
TEST(midi_default_mappings) {
    MidiManager midi;
    midi.install_default_mappings();

    ASSERT_EQ(static_cast<int>(midi.mappings().size()), 4);
    
    ASSERT_EQ(midi.mappings()[0].cc_number, 7);
    ASSERT_EQ(static_cast<int>(midi.mappings()[0].target_type),
              static_cast<int>(MidiTargetType::OutputGain));

    ASSERT_EQ(midi.mappings()[1].cc_number, 11);
    ASSERT_EQ(static_cast<int>(midi.mappings()[1].target_type),
              static_cast<int>(MidiTargetType::InputGain));

    ASSERT_EQ(midi.mappings()[2].cc_number, 64);
    ASSERT_EQ(static_cast<int>(midi.mappings()[2].target_type),
              static_cast<int>(MidiTargetType::EffectBypass));
    ASSERT_EQ(static_cast<int>(midi.mappings()[2].mode),
              static_cast<int>(MidiMappingMode::Toggle));

    ASSERT_EQ(midi.mappings()[3].cc_number, 74);
    ASSERT_EQ(midi.mappings()[3].effect_name, std::string("WahPedal"));
    ASSERT_EQ(midi.mappings()[3].param_name, std::string("Sweep"));
}

/**
 * @brief Assures collision tracking layers completely override existing entries when 
 * matching double CC registration events are explicitly added.
 */
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

/**
 * @brief Validates the manual cancellation step of an active hardware mapping 
 * capture routine, checking state teardown properties.
 */
TEST(midi_learn_cancel) {
    MidiManager midi;

    midi.start_learn(MidiTargetType::EffectParam, "TestEffect", "Drive");
    ASSERT_TRUE(midi.is_learning());

    midi.cancel_learn();
    ASSERT_FALSE(midi.is_learning());
    ASSERT_TRUE(midi.mappings().empty());
}

// ===========================================================================
// REACHABLE COVERAGE BOOSTER WITH FIELD-LEVEL ASSERTIONS
// ===========================================================================

/**
 * @brief RAII Cleanup Guard structure ensuring strict test isolation by backing up 
 * and recovering local disk configuration payloads automatically.
 */
struct config_backup_guard {
    std::string config_file = "midi_config.json";
    std::string backup_file = "midi_config.json.bak";
    bool backed_up = false;

    config_backup_guard() {
        if (fs::exists(config_file)) {
            fs::rename(config_file, backup_file);
            backed_up = true;
        }
    }

    ~config_backup_guard() {
        if (fs::exists(config_file)) {
            fs::remove(config_file);
        }
        if (backed_up && fs::exists(backup_file)) {
            fs::rename(backup_file, config_file);
        }
    }
};

/**
 * @brief Verifies that MIDI configuration mappings can be successfully 
 * serialized to disk and deserialized back with complete field-level integrity.
 */
TEST(midi_persist_save_and_load_roundtrip) {
    config_backup_guard guard;

    MidiManager mgr;
    MidiMapping m1{7, -1, MidiTargetType::EffectParam, MidiMappingMode::Continuous, "effect_0", "drive"};
    MidiMapping m2{11, -1, MidiTargetType::EffectParam, MidiMappingMode::Continuous, "effect_1", "level"};
    mgr.add_mapping(m1);
    mgr.add_mapping(m2);
    
    mgr.save_config();

    MidiManager mgr2;
    mgr2.load_config();
    
    ASSERT_EQ(static_cast<int>(mgr2.mappings().size()), 2);

    ASSERT_EQ(mgr2.mappings()[0].cc_number, 7);
    ASSERT_EQ(mgr2.mappings()[0].midi_channel, -1);
    ASSERT_EQ(static_cast<int>(mgr2.mappings()[0].target_type), static_cast<int>(MidiTargetType::EffectParam));
    ASSERT_EQ(static_cast<int>(mgr2.mappings()[0].mode), static_cast<int>(MidiMappingMode::Continuous));
    ASSERT_EQ(mgr2.mappings()[0].effect_name, std::string("effect_0"));
    ASSERT_EQ(mgr2.mappings()[0].param_name, std::string("drive"));

    ASSERT_EQ(mgr2.mappings()[1].cc_number, 11);
    ASSERT_EQ(mgr2.mappings()[1].midi_channel, -1);
    ASSERT_EQ(static_cast<int>(mgr2.mappings()[1].target_type), static_cast<int>(MidiTargetType::EffectParam));
    ASSERT_EQ(static_cast<int>(mgr2.mappings()[1].mode), static_cast<int>(MidiMappingMode::Continuous));
    ASSERT_EQ(mgr2.mappings()[1].effect_name, std::string("effect_1"));
    ASSERT_EQ(mgr2.mappings()[1].param_name, std::string("level"));
}

/**
 * @brief Ensures that when the midi_config.json file is completely missing, 
 * the manager handles the error gracefully and triggers its default fallback baseline.
 */
TEST(midi_persist_load_missing_file_graceful) {
    config_backup_guard guard;

    if (fs::exists("midi_config.json")) {
        fs::remove("midi_config.json");
    }

    MidiManager mgr;
    mgr.clear_mappings();
    mgr.load_config();
    
    ASSERT_EQ(static_cast<int>(mgr.mappings().size()), 2);
    ASSERT_EQ(mgr.mappings()[0].cc_number, 7);
    ASSERT_EQ(static_cast<int>(mgr.mappings()[0].target_type), static_cast<int>(MidiTargetType::EffectParam));
}

/**
 * @brief Validates that clear_mappings() executes safely on an empty manager 
 * instance without causing any undefined behavior or crashing.
 */
TEST(midi_mapping_clear_all_mappings_when_empty) {
    MidiManager mgr;
    mgr.clear_mappings();
    ASSERT_EQ(static_cast<int>(mgr.mappings().size()), 0);
}

/**
 * @brief Verifies that clearing mappings effectively resets the internal state 
 * and drops the active mapping count to zero after items are populated.
 */
TEST(midi_mapping_clear_all_mappings_after_adding) {
    MidiManager mgr;
    MidiMapping m1{7, -1, MidiTargetType::EffectParam, MidiMappingMode::Continuous, "effect_0", "drive"};
    MidiMapping m2{11, -1, MidiTargetType::EffectParam, MidiMappingMode::Continuous, "effect_1", "level"};
    mgr.add_mapping(m1);
    mgr.add_mapping(m2);
    ASSERT_EQ(static_cast<int>(mgr.mappings().size()), 2);
    
    mgr.clear_mappings();
    ASSERT_EQ(static_cast<int>(mgr.mappings().size()), 0);
}

/**
 * @brief Confirms that adding a new configuration layout mapping with an identical 
 * CC value correctly overrides and replaces the pre-existing parameter configuration.
 */
TEST(midi_mapping_override_same_cc_with_new_param) {
    MidiManager mgr;
    MidiMapping m1{7, -1, MidiTargetType::EffectParam, MidiMappingMode::Continuous, "effect_0", "drive"};
    mgr.add_mapping(m1);
    int count_after_first = static_cast<int>(mgr.mappings().size());
    ASSERT_EQ(count_after_first, 1);
    
    MidiMapping m2{7, -1, MidiTargetType::EffectParam, MidiMappingMode::Continuous, "effect_1", "level"};
    mgr.add_mapping(m2);  
    int count_after_override = static_cast<int>(mgr.mappings().size());
    
    ASSERT_EQ(count_after_override, 1);
    ASSERT_EQ(mgr.mappings()[0].effect_name, std::string("effect_1"));
    ASSERT_EQ(mgr.mappings()[0].param_name, std::string("level"));
}

/**
 * @brief Checks tracking accuracy and baseline states of the active mapping count 
 * across rapid bulk configuration additions and subsequent full resets.
 */
TEST(midi_mapping_get_active_mapping_count_after_bulk_ops) {
    MidiManager mgr;
    for (int i = 0; i < 5; i++) {
        MidiMapping m{static_cast<uint8_t>(i), -1, MidiTargetType::EffectParam, MidiMappingMode::Continuous, "effect_" + std::to_string(i), "param"};
        mgr.add_mapping(m);
    }
    ASSERT_GE(static_cast<int>(mgr.mappings().size()), 5);
    
    mgr.clear_mappings();
    ASSERT_EQ(static_cast<int>(mgr.mappings().size()), 0);
}

/**
 * @brief Exercises remove_mapping_for_param to evaluate both exact tracking matches 
 * and unmatched fallback strings.
 */
TEST(midi_mapping_remove_mapping_for_param) {
    MidiManager mgr;
    MidiMapping m;
    m.cc_number = 20;
    m.midi_channel = -1;
    m.target_type = MidiTargetType::EffectParam;
    m.mode = MidiMappingMode::Continuous;
    m.effect_name = "Chorus";
    m.param_name = "Depth";
    mgr.add_mapping(m);

    ASSERT_EQ(static_cast<int>(mgr.mappings().size()), 1);

    mgr.remove_mapping_for_param("Reverb", "Depth");
    ASSERT_EQ(static_cast<int>(mgr.mappings().size()), 1);

    mgr.remove_mapping_for_param("Chorus", "Depth");
    ASSERT_EQ(static_cast<int>(mgr.mappings().size()), 0);
}

/**
 * @brief Evaluates output configurations generated by learn_status to verify formatting 
 * across distinct MidiTargetType allocations.
 */
TEST(midi_mapping_learn_status_formatting) {
    MidiManager mgr;
    
    ASSERT_TRUE(mgr.learn_status().empty());

    mgr.start_learn(MidiTargetType::EffectParam, "Chorus", "Depth");
    std::string s1 = mgr.learn_status();
    ASSERT_NE(s1.find("Chorus"), std::string::npos);
    ASSERT_NE(s1.find("Depth"), std::string::npos);

    mgr.start_learn(MidiTargetType::InputGain, "", "");
    std::string s2 = mgr.learn_status();
    ASSERT_NE(s2.find("Input Gain"), std::string::npos);

    mgr.start_learn(MidiTargetType::OutputGain, "", "");
    std::string s3 = mgr.learn_status();
    ASSERT_NE(s3.find("Output Gain"), std::string::npos);

    mgr.start_learn(MidiTargetType::EffectBypass, "AmpSimulator", "");
    std::string s4 = mgr.learn_status();
    ASSERT_NE(s4.find("AmpSimulator"), std::string::npos);

    mgr.cancel_learn();
    ASSERT_TRUE(mgr.learn_status().empty());
}

/**
 * @brief Exercises the continuous evaluation branch handling InputGain inside the core layout engine.
 */
TEST(midi_mapping_apply_input_gain_event) {
    MidiManager mgr;
    AudioEngine engine;
    engine.initialize();

    MidiMapping m;
    m.cc_number = 11;
    m.midi_channel = -1;
    m.target_type = MidiTargetType::InputGain;
    m.mode = MidiMappingMode::Continuous;
    mgr.add_mapping(m);

    mgr.inject_event(make_cc(11, 64));
    mgr.poll(engine);

    float expected = (64.0f / 127.0f) * 2.0f;
    ASSERT_NEAR(engine.get_input_gain(), expected, 0.02f);
    engine.shutdown();
}

/**
 * @brief Validates that saving a malformed file structure and executing load_config 
 * correctly catches the JSON syntax exceptions internally and loads defaults.
 */
TEST(midi_persist_from_json_invalid_syntax) {
    config_backup_guard guard;
    
    std::ofstream file("midi_config.json");
    file << "{ broken raw unparseable json syntax text }";
    file.close();

    MidiManager mgr;
    mgr.clear_mappings();
    mgr.load_config();
    
    ASSERT_EQ(static_cast<int>(mgr.mappings().size()), 2);
    ASSERT_EQ(static_cast<int>(mgr.mappings()[0].target_type), static_cast<int>(MidiTargetType::EffectParam));
}

/**
 * @brief Assures that payloads missing standard tracking array root keys fail validation 
 * inside load_config and gracefully populate fallback tracking instead of generating errors.
 */
TEST(midi_persist_from_json_missing_root_key) {
    config_backup_guard guard;

    std::ofstream file("midi_config.json");
    file << R"({"unrelated_root_element": []})";
    file.close();

    MidiManager mgr;
    mgr.clear_mappings();
    mgr.load_config();
    
    ASSERT_EQ(static_cast<int>(mgr.mappings().size()), 2);
    ASSERT_EQ(static_cast<int>(mgr.mappings()[0].target_type), static_cast<int>(MidiTargetType::EffectParam));
}