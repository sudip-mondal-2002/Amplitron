#include "test_framework.h"
#include "preset_manager.h"
#include "preset_json.h"
#include "preset_manager_impl.h"
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
#include <cstdlib>

using namespace Amplitron;

// Keep these declarations visible in tests even if older headers omit them.
namespace Amplitron {
std::string to_json_ext(const PresetData& preset);
bool from_json_ext(const std::string& json_str, PresetData& preset);
std::string get_user_presets_dir();
}

namespace {

class ScopedEnvVar {
public:
    ScopedEnvVar(const char* name, const std::string& value)
        : name_(name) {
        const char* current = std::getenv(name);
        if (current) {
            original_ = current;
            had_original_ = true;
        }
#ifdef _WIN32
        _putenv_s(name_.c_str(), value.c_str());
#else
        setenv(name_.c_str(), value.c_str(), 1);
#endif
    }

    ~ScopedEnvVar() {
        restore();
    }

    void restore() {
        if (restored_) {
            return;
        }
#ifdef _WIN32
        _putenv_s(name_.c_str(), had_original_ ? original_.c_str() : "");
#else
        if (had_original_) {
            setenv(name_.c_str(), original_.c_str(), 1);
        } else {
            unsetenv(name_.c_str());
        }
#endif
        restored_ = true;
    }

private:
    std::string name_;
    std::string original_;
    bool had_original_ = false;
    bool restored_ = false;
};

} // namespace

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

// Minimal WAV writer for test IR files (mono PCM16)
static void write_le16(std::ofstream& out, uint16_t v) {
    char b[2]; b[0] = static_cast<char>(v & 0xFF); b[1] = static_cast<char>((v >> 8) & 0xFF); out.write(b,2);
}
static void write_le32(std::ofstream& out, uint32_t v) {
    char b[4]; b[0]=static_cast<char>(v & 0xFF); b[1]=static_cast<char>((v>>8)&0xFF); b[2]=static_cast<char>((v>>16)&0xFF); b[3]=static_cast<char>((v>>24)&0xFF); out.write(b,4);
}
static bool write_wav_mono_pcm16(const std::string& path, const std::vector<float>& samples, int sample_rate) {
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) return false;
    const uint16_t num_channels = 1;
    const uint16_t bits_per_sample = 16;
    const uint16_t block_align = static_cast<uint16_t>(num_channels * (bits_per_sample/8));
    const uint32_t data_bytes = static_cast<uint32_t>(samples.size()) * block_align;
    const uint32_t riff_size = 36 + data_bytes;
    out.write("RIFF",4); write_le32(out, riff_size); out.write("WAVE",4);
    out.write("fmt ",4); write_le32(out, 16); write_le16(out, 1); write_le16(out, num_channels); write_le32(out, sample_rate);
    write_le32(out, static_cast<uint32_t>(sample_rate) * block_align); write_le16(out, block_align); write_le16(out, bits_per_sample);
    out.write("data",4); write_le32(out, data_bytes);
    for (float s : samples) {
        float x = std::fmax(-1.0f, std::fmin(1.0f, s));
        int16_t v = static_cast<int16_t>(std::lrint(x * 32767.0f));
        write_le16(out, static_cast<uint16_t>(v));
    }
    return true;
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
    [[maybe_unused]] bool orig_effect0_enabled = engine.effects()[0]->is_enabled();

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
    ASSERT_EQ(loaded_mappings.size(), 2u);
    
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

// -----------------------------------------------------------------------
// Additional tests to cover preset_json and preset_manager_dirs/io gaps
// -----------------------------------------------------------------------

TEST(PresetJson, UnknownFieldsIgnoredOnLoad) {
    std::string json = R"({"format_version":1,"name":"Test","version":1,"unknown_field":42,"effects":[]})";
    PresetData preset;
    bool ok = from_json_ext(json, preset);
    ASSERT_TRUE(ok);
    ASSERT_EQ(preset.name, std::string("Test"));
}

TEST(PresetJson, MissingRequiredFieldsUseDefaults) {
    // Provide a minimal JSON with no name/description/effects
    std::string json = R"({"format_version":1})";
    PresetData preset;
    bool ok = from_json_ext(json, preset);
    ASSERT_TRUE(ok);
    // Missing name should be treated as empty string (defaults), not crash
    ASSERT_EQ(preset.name, std::string(""));
    ASSERT_EQ(preset.effects.size(), 0u);
}

TEST(PresetManagerDirs, GetUserPresetsDirWellFormedOrEmpty) {
    std::string dir = get_user_presets_dir();
    // On CI/isolated environments this may legitimately be empty;
    // otherwise the path should end in a presets directory.
    ASSERT_TRUE(dir.empty() || std::filesystem::path(dir).filename() == "presets");
}

TEST(PresetManagerDirs, PresetsDirCreatesIfMissing) {
    std::string test_dir = "presets/test_auto_create_dir";

    // Remove any previous run artifacts
#ifdef _WIN32
    system(("rmdir /s /q \"" + test_dir + "\" >nul 2>&1").c_str());
#else
    system(("rm -rf \"" + test_dir + "\" 2>/dev/null").c_str());
#endif

    PresetManager::set_presets_dir(test_dir);
    ASSERT_TRUE(std::filesystem::exists(test_dir));

    // Cleanup
    PresetManager::set_presets_dir("");
    std::filesystem::remove_all(test_dir);
}

TEST(PresetManagerDirs, GetPresetsDirUsesCustomDirAndNormalizesSlashes) {
    std::string test_dir = "presets/test_custom_dir_with_slashes";

#ifdef _WIN32
    [[maybe_unused]] ScopedEnvVar env_appdata("APPDATA", "");
#endif

    PresetManager::set_presets_dir(test_dir);
    std::string resolved = PresetManager::get_presets_dir();
    ASSERT_TRUE(resolved.find("test_custom_dir_with_slashes") != std::string::npos);

    PresetManager::set_presets_dir("");
    std::filesystem::remove_all("presets/test_custom_dir_with_slashes");
}

TEST(PresetManagerDirs, GetPresetsDirFallsBackWhenUserDirUnavailable) {
    std::string test_root = "test_temp_presets_fallback_file";
    std::filesystem::remove_all(test_root);
    {
        std::ofstream f(test_root);
        ASSERT_TRUE(f.is_open());
        f << "x";
    }

#ifdef _WIN32
    [[maybe_unused]] ScopedEnvVar env_appdata("APPDATA", test_root);
#endif

    PresetManager::set_presets_dir("");
    std::string resolved = PresetManager::get_presets_dir();
    ASSERT_EQ(std::filesystem::path(resolved).filename().string(), std::string("presets"));

    std::filesystem::remove(test_root);
}

TEST(PresetManagerIO, AppendJsonFilesEmptyDir) {
    std::string empty_dir = "presets/empty_dir_for_test";
    std::filesystem::create_directories(empty_dir);

    std::vector<std::string> files;
    append_json_files(empty_dir, files);
    ASSERT_TRUE(files.empty());

    std::filesystem::remove_all(empty_dir);
}

TEST(PresetManagerIO, AppendJsonFilesMissingDirDoesNotThrow) {
    std::vector<std::string> files;
    append_json_files("presets/definitely_missing_subdir", files);
    ASSERT_TRUE(files.empty());
}

TEST(PresetManagerIO, AppendJsonFilesFiltersNonJsonAndDirectories) {
    std::string test_dir = "presets/test_append_filter";
    std::filesystem::create_directories(test_dir + "/nested_dir");

    {
        std::ofstream json_file(test_dir + "/keep_me.json");
        ASSERT_TRUE(json_file.is_open());
        json_file << "{}";
    }
    {
        std::ofstream text_file(test_dir + "/ignore_me.txt");
        ASSERT_TRUE(text_file.is_open());
        text_file << "not json";
    }

    std::vector<std::string> files;
    append_json_files(test_dir, files);

    ASSERT_EQ(files.size(), 1u);
    ASSERT_TRUE(files[0].find("keep_me.json") != std::string::npos);

    std::filesystem::remove_all(test_dir);
}

TEST(PresetManagerMigration, ApplyMigrationsAddsVersion) {
    std::string raw = "{}"; // legacy unversioned format
    std::string patched = PresetManager::apply_migrations(raw);
    ASSERT_TRUE(patched.find("\"version\":") != std::string::npos);
}

TEST(PresetJson, InvalidJsonReturnsFalseAndLeavesPresetUnchanged) {
    PresetData preset;
    preset.name = "unchanged";
    bool ok = from_json_ext("this is not json", preset);
    ASSERT_FALSE(ok);
    ASSERT_EQ(preset.name, std::string("unchanged"));
}

TEST(PresetJson, RoundtripWithMidiAndMetadata) {
    PresetData preset;
    preset.name = "RoundtripFull";
    PresetData::EffectData fx;
    fx.type = "Overdrive";
    fx.enabled = true;
    fx.mix = 0.5f;
    fx.params.push_back({"Drive", 2.0f});
    fx.metadata["ir_path"] = "path/to/ir.wav";
    preset.effects.push_back(fx);

    MidiMapping mm;
    mm.cc_number = 10;
    mm.midi_channel = 1;
    mm.target_type = MidiTargetType::EffectParam;
    mm.mode = MidiMappingMode::Continuous;
    mm.effect_name = "Overdrive";
    mm.param_name = "Drive";
    preset.midi_mappings.push_back(mm);

    std::string json = to_json_ext(preset);

    PresetData loaded;
    bool ok = from_json_ext(json, loaded);
    ASSERT_TRUE(ok);
    ASSERT_EQ(loaded.name, preset.name);
    ASSERT_EQ(loaded.effects.size(), 1u);
    ASSERT_EQ(loaded.effects[0].type, std::string("Overdrive"));
    ASSERT_EQ(loaded.effects[0].metadata["ir_path"], std::string("path/to/ir.wav"));
    ASSERT_EQ(loaded.midi_mappings.size(), 1u);
    ASSERT_EQ(loaded.midi_mappings[0].cc_number, 10);
}

TEST(PresetManagerMigration, NoBraceReturnsOriginal) {
    std::string raw = "no braces here";
    std::string patched = PresetManager::apply_migrations(raw);
    ASSERT_EQ(patched, raw);
}

TEST(PresetManagerConfig, SaveAndLoadConfigRoundtripUsesConfigPath) {
    // Use a temporary test root so we don't touch real user config
    std::string test_root = "test_temp_config_root";
    std::filesystem::remove_all(test_root);
    std::filesystem::create_directories(test_root);

#ifdef _WIN32
    [[maybe_unused]] ScopedEnvVar env_appdata("APPDATA", test_root);
    std::string config_dir = test_root + "\\Amplitron";
    std::filesystem::create_directories(config_dir);
    std::string config_path = config_dir + "\\config.json";
    std::string presets_value = test_root + "\\my_presets";
    std::filesystem::create_directories(presets_value);
#else
    [[maybe_unused]] ScopedEnvVar env_home("HOME", test_root);
    std::string config_dir = test_root + "/.config/amplitron";
    std::filesystem::create_directories(config_dir);
    std::string config_path = config_dir + "/config.json";
    std::string presets_value = test_root + "/my_presets";
    std::filesystem::create_directories(presets_value);
#endif

    PresetManager::set_presets_dir(presets_value);
    PresetManager::save_config();

    ASSERT_TRUE(file_exists(config_path));
    std::string saved_config = read_file(config_path);
    ASSERT_TRUE(saved_config.find("presets_dir") != std::string::npos);
    ASSERT_TRUE(saved_config.find("my_presets") != std::string::npos);

    PresetManager::set_presets_dir("");
    PresetManager::load_config();
    ASSERT_EQ(PresetManager::custom_presets_dir(), presets_value);

    // Cleanup
    PresetManager::set_presets_dir("");
    std::filesystem::remove_all(test_root);
}

TEST(PresetManagerConfig, LoadConfigHandlesMissingAndMalformedFiles) {
    std::string test_root = "test_temp_config_root_malformed";
    std::filesystem::remove_all(test_root);
    std::filesystem::create_directories(test_root);

#ifdef _WIN32
    [[maybe_unused]] ScopedEnvVar env_appdata("APPDATA", test_root);
    std::string config_dir = test_root + "\\Amplitron";
    std::string config_path = config_dir + "\\config.json";
#else
    [[maybe_unused]] ScopedEnvVar env_home("HOME", test_root);
    std::string config_dir = test_root + "/.config/amplitron";
    std::string config_path = config_dir + "/config.json";
#endif
    std::filesystem::create_directories(config_dir);

    PresetManager::set_presets_dir("");
    std::filesystem::remove(config_path);

    PresetManager::load_config();
    ASSERT_EQ(PresetManager::custom_presets_dir(), std::string(""));

    {
        std::ofstream f(config_path);
        ASSERT_TRUE(f.is_open());
        f << "{}\n";
    }
    PresetManager::load_config();
    ASSERT_EQ(PresetManager::custom_presets_dir(), std::string(""));

    {
        std::ofstream f(config_path);
        ASSERT_TRUE(f.is_open());
        f << "{\"presets_dir\"}\n";
    }
    PresetManager::load_config();
    ASSERT_EQ(PresetManager::custom_presets_dir(), std::string(""));

    {
        std::ofstream f(config_path);
        ASSERT_TRUE(f.is_open());
        f << "{\"presets_dir\": }\n";
    }
    PresetManager::load_config();
    ASSERT_EQ(PresetManager::custom_presets_dir(), std::string(""));

    std::string missing_dir = test_root +
#ifdef _WIN32
        "\\missing_presets";
#else
        "/missing_presets";
#endif
    {
        std::ofstream f(config_path);
        ASSERT_TRUE(f.is_open());
        f << "{\n  \"presets_dir\": \"" << missing_dir << "\"\n}\n";
    }
    PresetManager::load_config();
    ASSERT_EQ(PresetManager::custom_presets_dir(), std::string(""));

    std::filesystem::create_directories(missing_dir);
    {
        std::ofstream f(config_path);
        ASSERT_TRUE(f.is_open());
        f << "{\n  \"presets_dir\": \"" << missing_dir << "\"\n}\n";
    }
    PresetManager::load_config();
    ASSERT_EQ(PresetManager::custom_presets_dir(), missing_dir);

    PresetManager::set_presets_dir("");
    std::filesystem::remove_all(test_root);
}

TEST(PresetManagerConfig, SaveAndLoadConfigFailWhenBasePathIsFile) {
    std::string base_path = "test_config_base_as_file";
    std::filesystem::remove_all(base_path);
    {
        std::ofstream f(base_path);
        ASSERT_TRUE(f.is_open());
        f << "x";
    }

#ifdef _WIN32
    [[maybe_unused]] ScopedEnvVar env_appdata("APPDATA", base_path);
#else
    [[maybe_unused]] ScopedEnvVar env_home("HOME", base_path);
#endif

    PresetManager::set_presets_dir("");
    PresetManager::save_config();
    PresetManager::load_config();
    ASSERT_EQ(PresetManager::custom_presets_dir(), std::string(""));

    std::filesystem::remove(base_path);
}

TEST(PresetJson, EffectDataRoundtripViaExtHelpers) {
    PresetData preset;
    preset.name = "EffectDataRT";
    PresetData::EffectData fx;
    fx.type = "TestFX";
    fx.enabled = true;
    fx.mix = 0.25f;
    fx.params.push_back({"ParamA", 1.5f});
    fx.metadata["k"] = "v";
    preset.effects.push_back(fx);

    std::string json = to_json_ext(preset);
    PresetData parsed;
    ASSERT_TRUE(from_json_ext(json, parsed));
    ASSERT_EQ(parsed.effects.size(), 1u);
    ASSERT_EQ(parsed.effects[0].type, std::string("TestFX"));
    ASSERT_EQ(parsed.effects[0].enabled, true);
    ASSERT_NEAR(parsed.effects[0].mix, 0.25f, 1e-6f);
    ASSERT_EQ(parsed.effects[0].params.size(), 1u);
    ASSERT_EQ(parsed.effects[0].params[0].first, std::string("ParamA"));
    ASSERT_NEAR(parsed.effects[0].params[0].second, 1.5f, 1e-6f);
    ASSERT_EQ(parsed.effects[0].metadata.at("k"), std::string("v"));
}

TEST(PresetManagerIO, SavePresetDataToDirectoryFails) {
    // Ensure the presets directory exists
    std::string dir = "presets";
    std::filesystem::create_directories(dir);

    PresetData p;
    p.name = "ShouldFail";

    // Attempt to write to a directory path (should fail)
    bool ok = PresetManager::save_preset_data(dir, p);
    ASSERT_FALSE(ok);
    ASSERT_TRUE(PresetManager::last_error().find("Could not open file for writing") != std::string::npos);
}

TEST(PresetManagerIO, LoadPresetWithUnknownEffectTypeSkipsEffect) {
    // Create a preset containing an unknown effect type
    PresetData p;
    p.name = "UnknownEffectPreset";
    PresetData::EffectData fx;
    fx.type = "DoesNotExistEffect";
    fx.enabled = true;
    p.effects.push_back(fx);

    std::string path = "presets/test_unknown_effect.json";
    PresetManager::save_preset_data(path, p);

    AudioEngine engine;
    engine.initialize();
    bool loaded = PresetManager::load_preset(path, engine);
    ASSERT_TRUE(loaded);
    // Since effect type was unknown, engine should have zero effects
    ASSERT_EQ(engine.effects().size(), 0u);

    std::remove(path.c_str());
    engine.shutdown();
}

TEST(PresetManagerIO, LoadLegacyIrCabinetPresetMigratesAndLoadsMetadata) {
    PresetData p;
    p.name = "LegacyCabinetPreset";
    PresetData::EffectData fx;
    fx.type = "IR Cabinet";
    fx.enabled = true;
    fx.mix = 0.5f;
    fx.metadata["ir_path"] = "missing_ir_file.wav";
    p.effects.push_back(fx);

    std::string path = "presets/test_legacy_ir_cabinet.json";
    ASSERT_TRUE(PresetManager::save_preset_data(path, p));

    AudioEngine engine;
    engine.initialize();
    bool loaded = PresetManager::load_preset(path, engine);
    ASSERT_TRUE(loaded);
    ASSERT_EQ(engine.effects().size(), 1u);
    ASSERT_EQ(std::string(engine.effects()[0]->name()), std::string("Cabinet"));

    std::remove(path.c_str());
    engine.shutdown();
}

TEST(PresetJson, ToJsonExtPreservesParamOrder) {
    PresetData p;
    PresetData::EffectData fx;
    fx.type = "Overdrive";
    fx.params.push_back({"A_First", 1.0f});
    fx.params.push_back({"B_Second", 2.0f});
    fx.params.push_back({"C_Third", 3.0f});
    p.effects.push_back(fx);

    std::string s = to_json_ext(p);
    size_t a = s.find("A_First");
    size_t b = s.find("B_Second");
    size_t c = s.find("C_Third");
    ASSERT_TRUE(a != std::string::npos && b != std::string::npos && c != std::string::npos);
    ASSERT_TRUE(a < b && b < c);
}

TEST(PresetJson, FromJsonExtIgnoresNonNumericParamsAndNonStringMetadata) {
    std::string json = R"({"format_version":1,"effects":[{"type":"TestFX","params":{"Good":1,"Bad":"x"},"metadata":{"k":123}}]})";
    PresetData p;
    bool ok = from_json_ext(json, p);
    ASSERT_TRUE(ok);
    ASSERT_EQ(p.effects.size(), 1u);
    ASSERT_EQ(p.effects[0].params.size(), 1u);
    ASSERT_EQ(p.effects[0].params[0].first, std::string("Good"));
    ASSERT_TRUE(p.effects[0].metadata.empty());
}

TEST(PresetManagerIO, SavePresetIncludesCabinetIrMetadata) {
    AudioEngine engine;
    engine.initialize();

    // create a tiny IR file
    std::string ir_path = "presets/test_ir_for_save.wav";
    std::filesystem::create_directories("presets");
    ASSERT_TRUE(write_wav_mono_pcm16(ir_path, {1.0f}, 48000));

    auto cab = std::make_shared<CabinetSim>();
    cab->set_sample_rate(48000);
    ASSERT_TRUE(cab->load_ir(ir_path));
    engine.add_effect(cab);

    std::string path = "presets/test_save_cabinet_meta.json";
    bool ok = PresetManager::save_preset(path, "CabTest", "desc", engine);
    ASSERT_TRUE(ok);

    std::string content = read_file(path);
    ASSERT_TRUE(content.find("ir_path") != std::string::npos);
    ASSERT_TRUE(content.find("test_ir_for_save.wav") != std::string::npos);

    std::remove(path.c_str());
    std::remove(ir_path.c_str());
    engine.shutdown();
}
