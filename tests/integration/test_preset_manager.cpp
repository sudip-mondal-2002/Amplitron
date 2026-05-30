#include "audio/engine/audio_engine.h"
#include "audio/effects/cabinet_sim.h"
#include "audio/effects/compressor.h"
#include "audio/effects/equalizer.h"
#include "audio/effects/noise_gate.h"
#include "audio/effects/overdrive.h"
#include "audio/effects/reverb.h"
#include "gui/state/gui_graph_state.h"
#include "preset_manager.h"
#include "test_framework.h"
#include "test_fixtures.h"
#include <sys/stat.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

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
static bool file_exists(const std::string &path) {
  return std::filesystem::exists(path);
}

// Helper: read entire file to string
static std::string read_file(const std::string &path) {
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

TEST_F(PresetTest, get_presets_dir_creates_dir) {
  std::string dir = PresetManager::get_presets_dir();
  ASSERT_FALSE(dir.empty());
}

TEST_F(PresetTest, save_creates_file) {
  register_temp_file("presets/test_save_preset.json");

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
  bool ok = PresetManager::save_preset(path, "Test Preset",
                                       "A test description", engine);
  ASSERT_TRUE(ok);
  ASSERT_TRUE(file_exists(path));

  // Verify JSON contains expected fields
  std::string json = read_file(path);
  ASSERT_TRUE(json.find("\"name\"") != std::string::npos);
  ASSERT_TRUE(json.find("Test Preset") != std::string::npos);
  ASSERT_TRUE(json.find("A test description") != std::string::npos);
  ASSERT_TRUE(json.find("\"nodes\"") != std::string::npos);
  ASSERT_TRUE(json.find("Noise Gate") != std::string::npos);
  ASSERT_TRUE(json.find("overdrive") != std::string::npos);
  ASSERT_TRUE(json.find("input_gain") != std::string::npos);
  ASSERT_TRUE(json.find("output_gain") != std::string::npos);
}

TEST_F(PresetTest, save_and_load_roundtrip) {
  register_temp_file("presets/test_roundtrip.json");

  // Build a signal chain
  auto ng = std::make_shared<NoiseGate>();
  ng->set_enabled(true);
  engine.add_effect(ng);

  auto eq = std::make_shared<Equalizer>();
  eq->set_enabled(true);
  // Modify a parameter
  if (!eq->params().empty()) {
    eq->params()[0].value =
        eq->params()[0].min_val +
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
  bool saved =
      PresetManager::save_preset(path, "Roundtrip", "roundtrip test", engine);
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
  int graph_node_count = 0;
  for (const auto &n : engine2.graph().get_nodes()) {
    if (n.routing_type == NodeRoutingType::StandardEffect &&
        n.pedal != nullptr) {
      graph_node_count++;
    }
  }
  ASSERT_EQ(graph_node_count, orig_effects_count);
  ASSERT_NEAR(engine2.get_input_gain(), orig_input_gain, 0.01f);
  ASSERT_NEAR(engine2.get_output_gain(), orig_output_gain, 0.01f);

  // Check reverb mix was preserved (assuming it's the 3rd standard node)
  bool found_reverb = false;
  for (const auto &n : engine2.graph().get_nodes()) {
    if (n.pedal && n.pedal->name() == std::string("Reverb")) {
      ASSERT_NEAR(n.pedal->get_mix(), 0.3f, 0.05f);
      found_reverb = true;
    }
  }
  ASSERT_TRUE(found_reverb);

  engine2.shutdown();
}

TEST_F(PresetTest, load_nonexistent_fails) {
  bool loaded =
      PresetManager::load_preset("presets/does_not_exist_12345.json", engine);
  ASSERT_FALSE(loaded);
}

TEST_F(PresetTest, list_finds_files) {
  std::string dir = PresetManager::get_presets_dir();
  std::string path = dir + "/test_list_preset.json";
  register_temp_file(path);

  engine.add_effect(std::make_shared<NoiseGate>());
  PresetManager::save_preset(path, "ListTest", "", engine);

  auto presets = PresetManager::list_presets();
  // Should find at least the one we just saved
  bool found = false;
  for (auto &p : presets) {
    if (p.find("test_list_preset.json") != std::string::npos) {
      found = true;
      break;
    }
  }
  ASSERT_TRUE(found);
}

TEST_F(PresetTest, save_empty_name_still_works) {
  register_temp_file("presets/test_empty_name.json");
  engine.add_effect(std::make_shared<Compressor>());

  std::string path = "presets/test_empty_name.json";
  bool ok = PresetManager::save_preset(path, "", "", engine);
  ASSERT_TRUE(ok);
  ASSERT_TRUE(file_exists(path));
}

TEST_F(PresetTest, set_presets_dir_copies_bundled_presets) {
  std::string test_dir = "presets/test_new_presets_dir_detailed";
  register_temp_dir(test_dir);

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
  for (const auto &entry : std::filesystem::directory_iterator(test_dir)) {
    if (entry.path().extension() == ".json") {
      test_dir_files.push_back(entry.path().string());
    }
  }

  // We should have copied presets to the new directory
  ASSERT_TRUE(test_dir_files.size() > 0);

  // Verify at least one copied preset file exists and is readable
  bool found_valid_preset = false;
  for (const auto &preset_path : test_dir_files) {
    if (file_exists(preset_path)) {
      std::string content = read_file(preset_path);
      if (!content.empty() &&
          content.find("\"format_version\"") != std::string::npos) {
        found_valid_preset = true;
        break;
      }
    }
  }
  ASSERT_TRUE(found_valid_preset);
}

TEST_F(PresetTest, midi_mappings_roundtrip) {
  register_temp_file("presets/test_midi_mappings.json");

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
  bool saved = PresetManager::save_preset(
      path, "Midi Test", "Testing midi mappings", engine, mappings);
  ASSERT_TRUE(saved);

  // Read json to verify
  std::string json = read_file(path);
  ASSERT_TRUE(json.find("\"midi_mappings\"") != std::string::npos);
  ASSERT_TRUE(json.find("WahPedal") != std::string::npos);

  // Verify loading
  AudioEngine engine2;
  engine2.initialize();

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
  ASSERT_EQ(static_cast<int>(loaded_mappings[1].target_type),
            static_cast<int>(MidiTargetType::EffectBypass));
  ASSERT_EQ(static_cast<int>(loaded_mappings[1].mode),
            static_cast<int>(MidiMappingMode::Toggle));
  ASSERT_EQ(loaded_mappings[1].effect_name, "Overdrive");
  ASSERT_EQ(loaded_mappings[1].param_name, "");

  engine2.shutdown();
}

TEST_F(PresetTest, graph_to_json_roundtrip) {
  GuiGraphState::get_instance().node_positions.clear();

  AudioGraph graph;
  int n1 = graph.add_node("Overdrive", NodeRoutingType::StandardEffect,
                          std::make_shared<Overdrive>());
  int n2 = graph.add_node("Splitter", NodeRoutingType::Splitter, nullptr);
  int n3 = graph.add_node("Mixer", NodeRoutingType::Mixer, nullptr);

  graph.set_node_position(n1, 100, 100);
  graph.set_node_position(n2, 200, 100);
  graph.set_node_position(n3, 300, 100);

  // Connect Overdrive output 0 to Splitter input 0
  int od_out = graph.find_node(n1)->output_pin_ids[0];
  int spl_in = graph.find_node(n2)->input_pin_ids[0];
  graph.add_link(od_out, spl_in);

  // Connect Splitter output 0 to Mixer input 0
  int spl_out0 = graph.find_node(n2)->output_pin_ids[0];
  int mix_in0 = graph.find_node(n3)->input_pin_ids[0];
  graph.add_link(spl_out0, mix_in0);

  std::string json = PresetManager::graph_to_json(graph);
  ASSERT_TRUE(json.find("\"routing\": \"graph\"") != std::string::npos);
  ASSERT_TRUE(json.find("overdrive") != std::string::npos);
  ASSERT_TRUE(json.find("splitter") != std::string::npos);
  ASSERT_TRUE(json.find("mixer") != std::string::npos);

  AudioGraph loaded_graph;
  bool ok = PresetManager::graph_from_json(json, loaded_graph);
  ASSERT_TRUE(ok);

  // Exclude graph inputs/outputs from size count
  int count = 0;
  for (const auto &n : loaded_graph.get_nodes()) {
    if (!n.is_graph_input && !n.is_graph_output)
      count++;
  }
  ASSERT_EQ(count, 3);
  ASSERT_EQ(loaded_graph.get_links().size(), 2);
}

TEST_F(PresetTest, parallel_amp_rig_integration) {
  register_temp_file("presets/test_parallel_rig.json");
  GuiGraphState::get_instance().node_positions.clear();

  // Clear initial linear nodes
  engine.clear_effects();
  auto &graph = engine.graph();

  // Build parallel rig
  int spl = graph.add_node("Splitter", NodeRoutingType::Splitter, nullptr);
  int amp1 =
      graph.add_node("Amp Sim", NodeRoutingType::StandardEffect,
                     std::make_shared<Overdrive>()); // using Overdrive as proxy
  int amp2 = graph.add_node("Amp Sim", NodeRoutingType::StandardEffect,
                            std::make_shared<Overdrive>());
  int mix = graph.add_node("Mixer", NodeRoutingType::Mixer, nullptr);

  graph.set_node_position(spl, 100, 200);
  graph.set_node_position(amp1, 300, 100);
  graph.set_node_position(amp2, 300, 300);
  graph.set_node_position(mix, 500, 200);

  auto get_pin = [&](int n, bool is_in, int idx) {
    return is_in ? graph.find_node(n)->input_pin_ids[idx]
                 : graph.find_node(n)->output_pin_ids[idx];
  };

  graph.add_link(get_pin(spl, false, 0), get_pin(amp1, true, 0));
  graph.add_link(get_pin(spl, false, 1), get_pin(amp2, true, 0));
  graph.add_link(get_pin(amp1, false, 0), get_pin(mix, true, 0));
  graph.add_link(get_pin(amp2, false, 0), get_pin(mix, true, 1));

  std::string path = "presets/test_parallel_rig.json";
  bool saved =
      PresetManager::save_preset(path, "Parallel Rig", "Parallel Amps", engine);
  ASSERT_TRUE(saved);

  AudioEngine engine2;
  engine2.initialize();
  bool loaded = PresetManager::load_preset(path, engine2);
  ASSERT_TRUE(loaded);

  int count = 0;
  for (const auto &n : engine2.graph().get_nodes()) {
    if (!n.is_graph_input && !n.is_graph_output)
      count++;
  }
  ASSERT_EQ(count, 4);
  ASSERT_EQ(engine2.graph().get_links().size(), 4);

  // Verify positions
  bool found_spl = false;
  for (const auto &n : engine2.graph().get_nodes()) {
    if (n.routing_type == NodeRoutingType::Splitter) {
      ASSERT_NEAR(n.x, 100.0f, 0.01f);
      ASSERT_NEAR(n.y, 200.0f, 0.01f);
      found_spl = true;
    }
  }
  ASSERT_TRUE(found_spl);

  engine2.shutdown();
}

TEST_F(PresetTest, graph_missing_nodes_throws) {
  std::string json = R"({
    "format_version": 2,
    "routing": "graph",
    "name": "Bad Graph Preset",
    "links": []
  })";
  AudioGraph graph;
  bool loaded = PresetManager::graph_from_json(json, graph);
  ASSERT_FALSE(loaded);
}

TEST_F(PresetTest, graph_missing_links_throws) {
  std::string json = R"({
    "format_version": 2,
    "routing": "graph",
    "name": "Bad Graph Preset 2",
    "nodes": []
  })";
  AudioGraph graph;
  bool loaded = PresetManager::graph_from_json(json, graph);
  ASSERT_FALSE(loaded);
}

static void write_dummy_wav(const std::string &path) {
  std::ofstream out(path, std::ios::binary);
  if (!out.is_open())
    return;
  const unsigned char header[44] = {
      'R', 'I', 'F', 'F', 40, 0, 0,   0,   'W', 'A', 'V', 'E', 'f', 'm', 't',
      ' ', 16,  0,   0,   0,  1, 0,   1,   0,   68,  172, 0,   0,   136, 88,
      1,   0,   2,   0,   16, 0, 'd', 'a', 't', 'a', 4,   0,   0,   0};
  out.write(reinterpret_cast<const char *>(header), 44);
  const unsigned char data[4] = {0, 0, 0, 0};
  out.write(reinterpret_cast<const char *>(data), 4);
}

TEST_F(PresetTest, legacy_ir_cabinet_migration) {
  register_temp_file("test.wav");
  register_temp_file("presets/test_legacy_ir_cab.json");

  write_dummy_wav("test.wav");
  std::string json = R"({
    "format_version": 1,
    "routing": "linear",
    "name": "Legacy Preset",
    "effects": [
      {
        "type": "IR Cabinet",
        "enabled": true,
        "mix": 1.0,
        "params": [["Volume", 0.8]],
        "metadata": {"ir_path": "test.wav"}
      }
    ]
  })";

  std::string path = "presets/test_legacy_ir_cab.json";
  std::ofstream out(path);
  out << json;
  out.close();

  bool loaded = PresetManager::load_preset(path, engine);
  ASSERT_TRUE(loaded);

  bool found_cab = false;
  for (const auto &n : engine.graph().get_nodes()) {
    if (n.pedal && n.pedal->name() == std::string("Cabinet")) {
      found_cab = true;
      auto *cab = dynamic_cast<CabinetSim *>(n.pedal.get());
      if (cab) {
        ASSERT_EQ(cab->ir_path(), "test.wav");
      }
    }
  }
  ASSERT_TRUE(found_cab);
}

TEST_F(PresetTest, graph_cabinet_ir_loading) {
  register_temp_file("my_ir.wav");

  write_dummy_wav("my_ir.wav");
  std::string json = R"({
    "format_version": 2,
    "routing": "graph",
    "name": "Graph Cab Preset",
    "nodes": [
      {
        "id": "n100",
        "type": "cabinet",
        "enabled": true,
        "mix": 1.0,
        "params": [],
        "metadata": {"ir_path": "my_ir.wav"}
      }
    ],
    "links": []
  })";

  AudioGraph graph;
  bool loaded = PresetManager::graph_from_json(json, graph);
  ASSERT_TRUE(loaded);

  bool found_cab = false;
  for (const auto &n : graph.get_nodes()) {
    if (n.pedal && n.pedal->name() == std::string("Cabinet")) {
      found_cab = true;
      auto *cab = dynamic_cast<CabinetSim *>(n.pedal.get());
      if (cab) {
        ASSERT_EQ(cab->ir_path(), "my_ir.wav");
      }
    }
  }
  ASSERT_TRUE(found_cab);
}

TEST_F(PresetTest, graph_widened_mixer) {
  std::string json = R"({
    "format_version": 2,
    "routing": "graph",
    "name": "Wide Mixer Preset",
    "nodes": [
      {
        "id": "n200",
        "type": "mixer",
        "num_inputs": 4,
        "enabled": true,
        "mix": 1.0,
        "params": []
      }
    ],
    "links": []
  })";

  AudioGraph graph;
  bool loaded = PresetManager::graph_from_json(json, graph);
  ASSERT_TRUE(loaded);

  bool found_mixer = false;
  for (const auto &n : graph.get_nodes()) {
    if (n.routing_type == NodeRoutingType::Mixer) {
      found_mixer = true;
      ASSERT_EQ(n.input_pin_ids.size(), 4);
    }
  }
  ASSERT_TRUE(found_mixer);
}

TEST_F(PresetTest, config_roundtrip) {
  std::string test_dir = "presets_custom_test_dir";
  register_temp_dir(test_dir);

  // Test load_config when it might not exist (shouldn't crash)
  PresetManager::load_config();

  // Set custom dir and save
  PresetManager::set_presets_dir(test_dir);
  PresetManager::save_config();

  // Clear it and reload from config
  PresetManager::set_presets_dir("");
  PresetManager::load_config();

  // It should have loaded our custom dir
  std::string loaded_dir = PresetManager::get_presets_dir();
  ASSERT_TRUE(loaded_dir.find(test_dir) != std::string::npos);
}

TEST_F(PresetTest, save_preset_data_invalid_path) {
  PresetData p;
  bool saved = PresetManager::save_preset_data(
      "/invalid_path_that_does_not_exist/preset.json", p);
  ASSERT_FALSE(saved);
}

TEST_F(PresetTest, load_preset_invalid_json) {
  register_temp_file("presets/invalid_preset_test.json");

  std::string path = "presets/invalid_preset_test.json";
  std::ofstream f(path);
  f << "{ this is not valid json";
  f.close();

  bool loaded = PresetManager::load_preset(path, engine);
  ASSERT_FALSE(loaded);
}

TEST_F(PresetTest, load_preset_graph_failure) {
  register_temp_file("presets/invalid_graph_preset.json");

  std::string path = "presets/invalid_graph_preset.json";
  std::ofstream f(path);
  f << R"({
        "format_version": 2,
        "routing": "graph",
        "name": "Invalid Graph",
        "nodes": [],
        "links": [{"src_pin": "n1.out0", "dst_pin": "n2.in0"}]
    })";
  f.close();

  bool loaded = PresetManager::load_preset(path, engine);
  ASSERT_FALSE(loaded);
}

TEST_F(PresetTest, graph_from_json_parse_errors) {
  // 1. Link parsing errors: invalid node ID format
  std::string json1 = R"({
        "format_version": 2,
        "routing": "graph",
        "name": "Bad Link",
        "nodes": [
            {"id": "n1", "type": "Input", "enabled": true, "mix": 1.0, "params": []},
            {"id": "n2", "type": "Output", "enabled": true, "mix": 1.0, "params": []}
        ],
        "links": [{"src_pin": "n1.out0", "dst_pin": "invalid_node.in0"}]
    })";

  AudioGraph graph1;
  bool loaded1 = PresetManager::graph_from_json(json1, graph1);
  ASSERT_FALSE(loaded1);

  // 2. Link parsing errors: pin out of bounds
  std::string json2 = R"({
        "format_version": 2,
        "routing": "graph",
        "name": "Bad Pin Index",
        "nodes": [
            {"id": "n1", "type": "Input", "enabled": true, "mix": 1.0, "params": []},
            {"id": "n2", "type": "Output", "enabled": true, "mix": 1.0, "params": []}
        ],
        "links": [{"src_pin": "n1.out999", "dst_pin": "n2.in0"}]
    })";

  AudioGraph graph2;
  bool loaded2 = PresetManager::graph_from_json(json2, graph2);
  ASSERT_FALSE(loaded2);

  // 3. Link parsing errors: missing pin
  std::string json3 = R"({
        "format_version": 2,
        "routing": "graph",
        "name": "Missing Pin Str",
        "nodes": [
            {"id": "n1", "type": "Input", "enabled": true, "mix": 1.0, "params": []},
            {"id": "n2", "type": "Output", "enabled": true, "mix": 1.0, "params": []}
        ],
        "links": [{"src_pin": "n1", "dst_pin": "n2.in0"}]
    })";

  AudioGraph graph3;
  bool loaded3 = PresetManager::graph_from_json(json3, graph3);
  ASSERT_FALSE(loaded3);
}

TEST_F(PresetTest, graph_from_json_node_types) {
  std::string json = R"({
        "format_version": 2,
        "routing": "graph",
        "name": "Node Types",
        "nodes": [
            {"id": "n1", "type": "splitter", "enabled": true, "mix": 1.0, "params": []},
            {"id": "n2", "type": "mixer", "enabled": true, "mix": 1.0, "params": [], "num_inputs": 2},
            {"id": "n3", "type": "amp_simulator", "enabled": true, "mix": 1.0, "params": []},
            {"id": "n4", "type": "overdrive", "enabled": true, "mix": 1.0, "params": []},
            {"id": "n5", "type": "distortion", "enabled": true, "mix": 1.0, "params": []},
            {"id": "n6", "type": "cabinet", "enabled": true, "mix": 1.0, "params": []},
            {"id": "n7", "type": "UnknownEffect", "enabled": true, "mix": 1.0, "params": []}
        ],
        "links": []
    })";

  AudioGraph graph;
  bool loaded = PresetManager::graph_from_json(json, graph);
  ASSERT_TRUE(loaded);
}

// Internal function test via namespace alias
namespace Amplitron {
extern void append_json_files(const std::string &dir,
                              std::vector<std::string> &result);
}

TEST_F(PresetTest, manager_append_json_files_invalid_dir) {
  std::vector<std::string> results;
  Amplitron::append_json_files("/path/that/does/not/exist", results);
  ASSERT_EQ(results.size(), 0u);
}

TEST_F(PresetTest, manager_apply_migrations) {
  // 1. Valid migration
  std::string json = "{\n  \"name\": \"Test\"\n}";
  std::string migrated = PresetManager::apply_migrations(json);
  ASSERT_TRUE(migrated.find("\"version\"") != std::string::npos);

  // 2. Already versioned
  std::string json2 = "{\n  \"version\": 1,\n  \"name\": \"Test\"\n}";
  std::string migrated2 = PresetManager::apply_migrations(json2);
  ASSERT_EQ(json2, migrated2);

  // 3. Invalid JSON string
  std::string json3 = "this is not json";
  std::string migrated3 = PresetManager::apply_migrations(json3);
  ASSERT_EQ(json3, migrated3);

  // 4. Empty JSON object
  std::string json4 = "{}";
  std::string migrated4 = PresetManager::apply_migrations(json4);
  ASSERT_TRUE(migrated4.find("\"version\"") != std::string::npos);
}

TEST_F(PresetTest, manager_config_no_home) {
  register_temp_file("amplitron_config.json");

  // Unset HOME
  const char *old_home = std::getenv("HOME");
  std::string home_str = old_home ? old_home : "";
#ifdef _WIN32
  _putenv("APPDATA=");
#else
  unsetenv("HOME");
#endif

  PresetManager::save_config();
  PresetManager::load_config();

#ifdef _WIN32
  if (!home_str.empty()) {
    std::string env = "APPDATA=" + home_str;
    _putenv(env.c_str());
  }
#else
  if (!home_str.empty()) {
    setenv("HOME", home_str.c_str(), 1);
  }
#endif
}

TEST_F(PresetTest, manager_get_presets_dir_no_home) {
  PresetManager::set_presets_dir(""); // Clear custom
  const char *old_home = std::getenv("HOME");
  std::string home_str = old_home ? old_home : "";
#ifdef _WIN32
  _putenv("APPDATA=");
#else
  unsetenv("HOME");
#endif

  std::string dir = PresetManager::get_presets_dir();
  ASSERT_EQ(dir, "presets");

#ifdef _WIN32
  if (!home_str.empty()) {
    std::string env = "APPDATA=" + home_str;
    _putenv(env.c_str());
  }
#else
  if (!home_str.empty()) {
    setenv("HOME", home_str.c_str(), 1);
  }
#endif
}

TEST_F(PresetTest, manager_save_config_failure) {
  register_temp_dir("amplitron_config.json");

  const char *old_home = std::getenv("HOME");
  std::string home_str = old_home ? old_home : "";
#ifdef _WIN32
  _putenv("APPDATA=");
#else
  unsetenv("HOME");
#endif

  // Create a directory named amplitron_config.json to force ofstream to fail
  std::filesystem::create_directories("amplitron_config.json");

  PresetManager::save_config(); // This should fail gracefully

#ifdef _WIN32
  if (!home_str.empty()) {
    std::string env = "APPDATA=" + home_str;
    _putenv(env.c_str());
  }
#else
  if (!home_str.empty()) {
    setenv("HOME", home_str.c_str(), 1);
  }
#endif
}

TEST_F(PresetTest, manager_append_json_files_exception) {
  register_temp_file("test_fake_dir.txt");

  std::ofstream f("test_fake_dir.txt");
  f << "test";
  f.close();

  std::vector<std::string> results;
  Amplitron::append_json_files("test_fake_dir.txt", results);
  ASSERT_EQ(results.size(), 0u);
}

TEST_F(PresetTest, manager_save_factory_presets_write_failure) {
  std::string test_dir = "presets_readonly_test";
  register_temp_dir(test_dir);
  register_temp_file("presets/dummy_factory.json");

  std::filesystem::create_directories(test_dir);

  // Create a dummy file in presets to act as bundled
  std::filesystem::create_directories("presets");
  std::ofstream f("presets/dummy_factory.json");
  f << "{}";
  f.close();

  // Make directory read-only
#ifndef _WIN32
  chmod(test_dir.c_str(), 0555);
#endif

  PresetManager::set_presets_dir(test_dir);

#ifndef _WIN32
  chmod(test_dir.c_str(), 0777);
#endif
}

TEST_F(PresetTest, load_linear_legacy_conversion) {
  register_temp_file("presets/legacy_conversion_test.json");

  std::string json = R"({
        "format_version": 1,
        "routing": "linear",
        "name": "Legacy",
        "effects": [
            {"type": "IR Cabinet", "enabled": true, "mix": 1.0, "params": {}},
            {"type": "UnknownEffect", "enabled": true, "mix": 1.0, "params": {}}
        ]
    })";
  std::string path = "presets/legacy_conversion_test.json";
  std::ofstream f(path);
  f << json;
  f.close();

  bool loaded = PresetManager::load_preset(path, engine);
  ASSERT_TRUE(loaded);
}

TEST_F(PresetTest, save_all_effect_types) {
  register_temp_file("presets/all_effects.json");

  engine.clear_effects();

  auto comp = std::make_shared<Compressor>();
  auto eq = std::make_shared<Equalizer>();
  auto gate = std::make_shared<NoiseGate>();
  auto drive = std::make_shared<Overdrive>();
  auto reverb = std::make_shared<Reverb>();

  engine.add_initial_effects({comp, eq, gate, drive, reverb});

  std::string path = "presets/all_effects.json";
  bool saved = PresetManager::save_preset(path, "All FX", "Test", engine);
  ASSERT_TRUE(saved);
}

TEST_F(PresetTest, graph_from_json_add_link_failure) {
  std::string json1 = R"({
        "format_version": 2,
        "routing": "graph",
        "name": "Cycle",
        "nodes": [
            {"id": "n1", "type": "Input", "enabled": true, "mix": 1.0, "params": []},
            {"id": "n2", "type": "Output", "enabled": true, "mix": 1.0, "params": []},
            {"id": "n3", "type": "Delay", "enabled": true, "mix": 1.0, "params": []}
        ],
        "links": [
            {"src_pin": "n1.out0", "dst_pin": "n2.in0"},
            {"src_pin": "n1.out0", "dst_pin": "n3.in0"}
        ]
    })";

  AudioGraph graph1;
  bool loaded1 = PresetManager::graph_from_json(json1, graph1);
  ASSERT_FALSE(loaded1);
}

TEST_F(PresetTest, save_preset_invalid_path) {
  bool saved = PresetManager::save_preset(
      "/invalid_path_that_does_not_exist/preset.json", "Name", "Desc", engine);
  ASSERT_FALSE(saved);
}

TEST_F(PresetTest, manager_config_parsing_edge_cases) {
  std::string test_home = "test_home_config";
  register_temp_dir(test_home);

  const char *old_home = std::getenv("HOME");
  std::string home_str = old_home ? old_home : "";
  std::filesystem::create_directories(test_home + "/.config/amplitron");
#ifdef _WIN32
  _putenv(("APPDATA=" + test_home).c_str());
#else
  setenv("HOME", test_home.c_str(), 1);
#endif

  auto write_and_load = [&](const std::string &content) {
    std::ofstream f(test_home + "/.config/amplitron/config.json");
    f << content;
    f.close();
    PresetManager::load_config();
  };

  // 1. Missing colon
  write_and_load("{\"presets_dir\" \"value\"}");
  // 2. Missing quote
  write_and_load("{\"presets_dir\": value}");
  // 3. Escaped values
  write_and_load("{\"presets_dir\": \"test_dir_escapes\\n\\\"\\\\\"}");
  // 4. Unknown escape sequence
  write_and_load("{\"presets_dir\": \"test_dir_escapes\\x\"}");

#ifdef _WIN32
  if (!home_str.empty())
    _putenv(("APPDATA=" + home_str).c_str());
  else
    _putenv("APPDATA=");
#else
  if (!home_str.empty())
    setenv("HOME", home_str.c_str(), 1);
  else
    unsetenv("HOME");
#endif
}

TEST_F(PresetTest, load_preset_data_permission_denied) {
#ifdef _WIN32
  std::string path = "presets_unreadable_dir_test";
  register_temp_dir(path);
  std::filesystem::create_directories(path);
#else
  std::string path = "presets_unreadable_file_test.json";
  register_temp_file(path);
  std::ofstream f(path);
  f << "{}";
  f.close();
  chmod(path.c_str(), 0000);
#endif

  bool loaded = PresetManager::load_preset(path, engine);
  ASSERT_FALSE(loaded);

#ifndef _WIN32
  chmod(path.c_str(), 0644);
#endif
}

TEST_F(PresetTest, save_graph_all_fields) {
  register_temp_file("presets/graph_all_fields.json");
  register_temp_file("test_ir.wav");

  auto comp = std::make_shared<Compressor>();
  engine.graph().add_node("Comp", NodeRoutingType::StandardEffect, comp, 2);

  auto cab = std::make_shared<CabinetSim>();
  
  const char wav_header[46] = {
      'R','I','F','F', 38, 0, 0, 0, 'W','A','V','E',
      'f','m','t',' ', 16, 0, 0, 0, 1, 0, 1, 0,
      (char)0x80, (char)0xbb, 0, 0, 0, (char)0x77, 1, 0, 2, 0, 16, 0,
      'd','a','t','a', 2, 0, 0, 0,
      0, 0
  };
  std::ofstream out("test_ir.wav", std::ios::binary);
  out.write(wav_header, 46);
  out.close();

  bool ir_loaded = cab->load_ir("test_ir.wav");
  ASSERT_TRUE(ir_loaded);
  engine.graph().add_node("Cab", NodeRoutingType::StandardEffect, cab, 1);

  std::string path = "presets/graph_all_fields.json";
  bool saved = PresetManager::save_preset(path, "Graph", "Desc", engine);
  ASSERT_TRUE(saved);

  // Roundtrip to test from_ordered_json's metadata processing
  AudioEngine engine2;
  engine2.initialize();
  bool loaded = PresetManager::load_preset(path, engine2);
  ASSERT_TRUE(loaded);
  
  // Verify metadata loaded
  const auto& graph = engine2.graph();
  bool found_metadata = false;
  for (const auto& node : graph.get_nodes()) {
      if (auto cab_sim = std::dynamic_pointer_cast<CabinetSim>(node.pedal)) {
          if (cab_sim->ir_path() == "test_ir.wav") found_metadata = true;
      }
  }
  ASSERT_TRUE(found_metadata);

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
