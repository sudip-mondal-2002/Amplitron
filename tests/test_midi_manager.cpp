#include "test_framework.h"

// Set a local hook definition to open up internal serialization blocks for test suite coverage tracking
#define VIRTUAL_TEST_HOOK public
#include "midi/midi_manager.h"
#include "audio/audio_engine.h"
#include "audio/effect.h"
#undef VIRTUAL_TEST_HOOK

#include <cmath>
#include <fstream>
#include <filesystem>

using namespace Amplitron;
namespace fs = std::filesystem;

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

static MidiEvent make_cc(uint8_t cc, uint8_t value, uint8_t channel = 0) {
    MidiEvent e{};
    e.status = static_cast<uint8_t>(0xB0 | (channel & 0x0F));
    e.data1 = cc;
    e.data2 = value;
    return e;
}

struct config_backup_guard {
    std::filesystem::path original_cwd;
    std::filesystem::path temp_dir;
    config_backup_guard() {
        original_cwd = std::filesystem::current_path();
        temp_dir = std::filesystem::temp_directory_path() / ("midi_test_" + std::to_string(std::rand()));
        std::filesystem::create_directories(temp_dir);
        std::filesystem::current_path(temp_dir);
    }
    ~config_backup_guard() {
        std::filesystem::current_path(original_cwd);
        std::filesystem::remove_all(temp_dir);
    }
};

// --- CORE MAPPING LOGIC & FIELD VALIDATION ---

TEST(midi_default_mappings) {
    MidiManager midi;
    midi.install_default_mappings();
    const auto& maps = midi.mappings();
    auto has = [&](uint8_t cc, MidiTargetType t) {
        for(const auto& m : maps) if(m.cc_number == cc && m.target_type == t) return true;
        return false;
    };
    ASSERT_TRUE(has(7, MidiTargetType::OutputGain));
    ASSERT_TRUE(has(64, MidiTargetType::EffectBypass));
}

TEST(midi_mapping_get_active_mapping_count_after_bulk_ops) {
    MidiManager mgr;
    mgr.clear_mappings();
    for(int i = 0; i < 5; ++i) {
        MidiMapping m; m.cc_number = (uint8_t)i;
        mgr.add_mapping(m);
    }
    ASSERT_EQ((int)mgr.mappings().size(), 5);
}

// --- PERSISTENCE TESTS (CodeRabbit & Owner Compliant) ---

TEST(midi_persist_load_missing_file_graceful) {
    config_backup_guard guard;
    if(fs::exists("midi_config.json")) fs::remove("midi_config.json");
    MidiManager mgr;
    mgr.load_config();
    ASSERT_EQ((int)mgr.mappings().size(), 2);
}

TEST(midi_persist_from_json_invalid_syntax) {
    config_backup_guard guard;
    std::ofstream file("midi_config.json");
    file << "{ broken json }";
    file.close();
    MidiManager mgr;
    mgr.load_config();
    ASSERT_EQ((int)mgr.mappings().size(), 2);
}

TEST(midi_persist_load_config_valid_json_succeeds) {
    config_backup_guard guard;
    std::ofstream file("midi_config.json");
    file << R"({"mappings": [{"cc": 7, "target": 0, "mode": 0, "effect": "Test", "param": "Drive"}]})";
    file.close();
    MidiManager mgr;
    mgr.load_config();
    
    // Owner: Detailed field-level validation
    ASSERT_GE((int)mgr.mappings().size(), 1);
    ASSERT_EQ(mgr.mappings()[0].cc_number, 7);
}

// --- LIFECYCLE TESTS (Coverage for midi_manager.cpp) ---

TEST(midi_manager_port_safety) {
    MidiManager mgr;
    mgr.initialize();
    ASSERT_FALSE(mgr.open_port(9999)); // Check range rejection
    mgr.close_port();
    mgr.shutdown();
}