/**
 * @file test_json_serialization.cpp
 * @brief Unit tests for the nlohmann/json-based preset serialization layer.
 *
 * Acceptance criteria from issue #96
 * -----------------------------------
 * [AC1] A chosen C++ JSON library is successfully added to the project's
 * build system.                                  → verified by compilation
 * [AC2] Core data structures (effect parameters) have basic serialization
 * methods.                                       → test_effect_data_roundtrip
 * [AC3] A unit test or console output confirms that the application state can
 * be reliably converted to JSON and parsed back into C++ objects
 * without data loss.                             → test_preset_full_roundtrip,
 * test_preset_signal_chain_dump
 */

#include "test_framework.h"
#include "preset_json.h"
#include "preset_manager.h"
#include "audio/engine/audio_engine.h"
#include <stdexcept>

#include "audio/effects/noise_gate.h"
#include "audio/effects/overdrive.h"
#include "audio/effects/equalizer.h"
#include "audio/effects/reverb.h"
#include "audio/effects/compressor.h"
#include "audio/effects/delay.h"
#include "audio/effects/distortion.h" // Added for autosave test
#include "midi/midi_manager.h"

#include <nlohmann/json.hpp>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <filesystem>

using namespace Amplitron;

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------

static PresetData make_test_preset() {
    PresetData p;
    p.name         = "Test Preset";
    p.description  = "Created by test_json_serialization";
    p.input_gain   = 0.75f;
    p.output_gain  = 0.85f;

    {
        PresetData::EffectData fx;
        fx.type    = "Noise Gate";
        fx.enabled = true;
        fx.mix     = 1.0f;
        fx.params  = {{"Threshold", -40.0f}, {"Attack", 0.5f}, {"Release", 50.0f}};
        p.effects.push_back(fx);
    }
    {
        PresetData::EffectData fx;
        fx.type    = "Overdrive";
        fx.enabled = false;
        fx.mix     = 0.8f;
        fx.params  = {{"Drive", 2.5f}, {"Tone", 0.6f}, {"Level", 0.7f}};
        p.effects.push_back(fx);
    }
    {
        PresetData::EffectData fx;
        fx.type     = "Cabinet";
        fx.enabled  = true;
        fx.mix      = 1.0f;
        fx.metadata = {{"ir_path", "/some/path/cabinet.wav"}};
        p.effects.push_back(fx);
    }

    MidiMapping m;
    m.cc_number    = 74;
    m.midi_channel = 0;
    m.target_type  = MidiTargetType::EffectParam;
    m.mode         = MidiMappingMode::Continuous;
    m.effect_name  = "Overdrive";
    m.param_name   = "Drive";
    p.midi_mappings.push_back(m);

    return p;
}

// -----------------------------------------------------------------------
// [AC2] Effect parameter serialization
// -----------------------------------------------------------------------

TEST(json_effect_data_to_json_contains_expected_keys) {
    PresetData::EffectData fx;
    fx.type    = "Reverb";
    fx.enabled = true;
    fx.mix     = 0.5f;
    fx.params  = {{"Decay", 0.8f}, {"Damp", 0.3f}, {"Level", 0.4f}};

    nlohmann::json j;
    Amplitron::to_json(j, fx);

    ASSERT_TRUE(j.contains("type"));
    ASSERT_TRUE(j.contains("enabled"));
    ASSERT_TRUE(j.contains("mix"));
    ASSERT_TRUE(j.contains("params"));
    ASSERT_EQ(j["type"].get<std::string>(), std::string("Reverb"));
    ASSERT_EQ(j["enabled"].get<bool>(), true);
    ASSERT_TRUE(j["params"].contains("Decay"));
    ASSERT_NEAR(j["params"]["Decay"].get<float>(), 0.8f, 0.001f);
}

TEST(json_effect_data_roundtrip) {
    PresetData::EffectData original;
    original.type    = "Equalizer";
    original.enabled = true;
    original.mix     = 0.9f;
    original.params  = {{"Bass", 3.0f}, {"Mid", -2.0f}, {"Treble", 1.5f}};
    original.metadata = {{"custom_key", "custom_value"}};

    // Serialise
    nlohmann::json j;
    Amplitron::to_json(j, original);

    // Deserialise
    PresetData::EffectData restored;
    Amplitron::from_json(j, restored);

    ASSERT_EQ(restored.type,    original.type);
    ASSERT_EQ(restored.enabled, original.enabled);
    ASSERT_NEAR(restored.mix,   original.mix, 0.001f);
    ASSERT_EQ(restored.params.size(), original.params.size());

    // Verify each parameter
    for (size_t i = 0; i < original.params.size(); ++i) {
        ASSERT_EQ(restored.params[i].first, original.params[i].first);
        ASSERT_NEAR(restored.params[i].second, original.params[i].second, 0.001f);
    }

    // Verify metadata
    ASSERT_EQ(restored.metadata.count("custom_key"), 1u);
    ASSERT_EQ(restored.metadata.at("custom_key"), std::string("custom_value"));
}

// -----------------------------------------------------------------------
// [AC3] Full preset round-trip (string  →  parse  →  re-serialise)
// -----------------------------------------------------------------------

TEST(json_preset_roundtrip_via_string) {
    PresetData original = make_test_preset();

    // Serialise to JSON string
    std::string json_str = to_json_ext(original);

    // Validate it is parseable as valid JSON
    ASSERT_TRUE(!json_str.empty());
    ASSERT_TRUE(nlohmann::json::accept(json_str));

    // Deserialise back
    PresetData restored;
    bool ok = from_json_ext(json_str, restored);
    ASSERT_TRUE(ok);

    // Top-level fields
    ASSERT_EQ(restored.name,        original.name);
    ASSERT_EQ(restored.description, original.description);
    ASSERT_NEAR(restored.input_gain,  original.input_gain,  0.001f);
    ASSERT_NEAR(restored.output_gain, original.output_gain, 0.001f);

    // Effect chain length
    ASSERT_EQ(restored.effects.size(), original.effects.size());

    // First effect: Noise Gate
    ASSERT_EQ(restored.effects[0].type,    std::string("Noise Gate"));
    ASSERT_EQ(restored.effects[0].enabled, true);
    ASSERT_NEAR(restored.effects[0].mix,   1.0f, 0.001f);
    ASSERT_EQ(restored.effects[0].params.size(), 3u);
    ASSERT_NEAR(restored.effects[0].params[0].second, -40.0f, 0.001f);

    // Second effect: Overdrive (disabled)
    ASSERT_EQ(restored.effects[1].type,    std::string("Overdrive"));
    ASSERT_EQ(restored.effects[1].enabled, false);
    ASSERT_NEAR(restored.effects[1].mix,   0.8f, 0.001f);

    // Third effect: Cabinet with IR metadata
    ASSERT_EQ(restored.effects[2].type,    std::string("Cabinet"));
    ASSERT_EQ(restored.effects[2].metadata.count("ir_path"), 1u);
    ASSERT_EQ(restored.effects[2].metadata.at("ir_path"),
              std::string("/some/path/cabinet.wav"));

    // MIDI mappings
    ASSERT_EQ(restored.midi_mappings.size(), 1u);
    ASSERT_EQ(restored.midi_mappings[0].cc_number,  74);
    ASSERT_EQ(restored.midi_mappings[0].effect_name, std::string("Overdrive"));
    ASSERT_EQ(restored.midi_mappings[0].param_name,  std::string("Drive"));
}

TEST(json_roundtrip_via_file) {
    PresetData original = make_test_preset();
    original.name = "FileRoundtripTest";

    // Keep this PresetManager load test deterministic. IR metadata
    // serialization is already covered in json_preset_roundtrip_via_string.
    original.effects.resize(2);

    std::string path = "presets/test_nlohmann_roundtrip.json";
    std::filesystem::create_directories("presets");

    // Save via PresetManager (which calls to_json_ext internally)
    bool saved = PresetManager::save_preset_data(path, original);
    ASSERT_TRUE(saved);

    // Load via PresetManager (which calls from_json_ext internally)
    AudioEngine engine;
    engine.initialize();
    bool loaded = PresetManager::load_preset(path, engine);
    ASSERT_TRUE(loaded);

    ASSERT_EQ(static_cast<int>(engine.effects().size()), 2); // IR effect skipped (no real IR)
    ASSERT_NEAR(engine.get_input_gain(),  0.75f, 0.01f);
    ASSERT_NEAR(engine.get_output_gain(), 0.85f, 0.01f);

    // Verify the legacy linear preset correctly wired the AudioGraph!
    const auto& graph = engine.graph();
    ASSERT_EQ(graph.get_nodes().size(), 3u); // Input -> Noise Gate -> Overdrive
    ASSERT_EQ(graph.get_links().size(), 2u);
    ASSERT_TRUE(graph.get_nodes().front().is_graph_input);
    ASSERT_TRUE(graph.get_nodes().back().is_graph_output);

    std::remove(path.c_str());
    engine.shutdown();
}

// -----------------------------------------------------------------------
// [AC3] Proof-of-concept: dump default signal chain as JSON to stdout
// -----------------------------------------------------------------------

TEST(json_preset_signal_chain_dump) {
    // Build the default signal chain that Amplitron starts with
    AudioEngine engine;
    engine.initialize();

    engine.add_effect(std::make_shared<NoiseGate>());
    engine.add_effect(std::make_shared<Compressor>());
    engine.add_effect(std::make_shared<Overdrive>());
    engine.add_effect(std::make_shared<Equalizer>());
    engine.add_effect(std::make_shared<Delay>());
    engine.add_effect(std::make_shared<Reverb>());

    engine.set_input_gain(0.7f);
    engine.set_output_gain(0.8f);

    // Collect state into a PresetData (same logic as PresetManager::save_preset)
    PresetData state;
    state.name        = "Default Signal Chain";
    state.description = "Proof-of-concept dump for issue #96";
    state.input_gain  = engine.get_input_gain();
    state.output_gain = engine.get_output_gain();
    for (auto& fx : engine.effects()) {
        PresetData::EffectData fd;
        fd.type    = fx->name();
        fd.enabled = fx->is_enabled();
        fd.mix     = fx->get_mix();
        for (auto& p : fx->params()) {
            fd.params.push_back({p.name, p.value});
        }
        state.effects.push_back(fd);
    }

    // Dump to JSON string and print to console (proves library is working)
    std::string json_str = to_json_ext(state);
    std::cout << "\n[test_json_serialization] Default signal chain JSON:\n"
              << json_str << std::endl;

    // Structural assertions
    ASSERT_FALSE(json_str.empty());

    nlohmann::json j = nlohmann::json::parse(json_str);
    ASSERT_TRUE(j.contains("format_version"));
    ASSERT_TRUE(j.contains("name"));
    ASSERT_TRUE(j.contains("effects"));
    ASSERT_EQ(j["effects"].size(), 6u);

    // Every effect block must have type, enabled, mix, params
    for (auto& jfx : j["effects"]) {
        ASSERT_TRUE(jfx.contains("type"));
        ASSERT_TRUE(jfx.contains("enabled"));
        ASSERT_TRUE(jfx.contains("mix"));
        ASSERT_TRUE(jfx.contains("params"));
    }

    // Round-trip: parse back and verify no data loss
    PresetData restored;
    bool ok = from_json_ext(json_str, restored);
    ASSERT_TRUE(ok);
    ASSERT_EQ(restored.effects.size(), state.effects.size());
    for (size_t i = 0; i < state.effects.size(); ++i) {
        ASSERT_EQ(restored.effects[i].type,    state.effects[i].type);
        ASSERT_EQ(restored.effects[i].enabled, state.effects[i].enabled);
        ASSERT_NEAR(restored.effects[i].mix,   state.effects[i].mix, 0.001f);
        ASSERT_EQ(restored.effects[i].params.size(),
                  state.effects[i].params.size());
        for (size_t p = 0; p < state.effects[i].params.size(); ++p) {
            ASSERT_EQ(restored.effects[i].params[p].first,
                      state.effects[i].params[p].first);
            ASSERT_NEAR(restored.effects[i].params[p].second,
                        state.effects[i].params[p].second, 0.001f);
        }
    }

    engine.shutdown();
}

TEST(json_invalid_json_returns_false) {
    PresetData preset;
    bool ok = from_json_ext("{ this is not valid json !!!", preset);
    ASSERT_FALSE(ok);
}

TEST(json_empty_effects_list_roundtrip) {
    PresetData original;
    original.name        = "Empty Chain";
    original.input_gain  = 0.5f;
    original.output_gain = 0.5f;
    // No effects, no midi mappings

    std::string json_str = to_json_ext(original);
    PresetData restored;
    bool ok = from_json_ext(json_str, restored);
    ASSERT_TRUE(ok);
    ASSERT_EQ(restored.name, std::string("Empty Chain"));
    ASSERT_EQ(restored.effects.size(), 0u);
    ASSERT_EQ(restored.midi_mappings.size(), 0u);
}

TEST(json_can_load_existing_factory_presets) {
    // Verify the new parser is backward-compatible with the existing preset files
    const std::vector<std::string> factory_presets = {
        "presets/05_Phase_Shift_Lead.json",
        "presets/06_Jet_Flanger.json",
    };

    int loaded_count = 0;

    for (const auto& path : factory_presets) {
        std::ifstream f(path);
        if (!f.is_open()) continue; // Skip if not found in test environment

        std::string content((std::istreambuf_iterator<char>(f)),
                             std::istreambuf_iterator<char>());
        f.close();

        PresetData preset;
        bool ok = from_json_ext(content, preset);
        ASSERT_TRUE(ok);
        ASSERT_FALSE(preset.name.empty());
        ASSERT_FALSE(preset.effects.empty());
        ++loaded_count;
    }

    // FIX: prevent vacuous pass — if all files are missing the loop skips
    // everything and the test proves nothing (CodeRabbit issue #4).
    ASSERT_GT(loaded_count, 0);
}

// -----------------------------------------------------------------------
// [NEW] Autosave and Crash Recovery Engine State Roundtrip
// -----------------------------------------------------------------------

TEST(json_audio_engine_autosave_roundtrip) {
    AudioEngine engine;
    engine.initialize();
    
    // 1. Setup a specific chain state
    auto distortion = std::make_shared<Distortion>();
    distortion->set_enabled(true);
    distortion->set_mix(0.85f);
    engine.add_effect(distortion);

    auto reverb = std::make_shared<Reverb>();
    reverb->set_enabled(false);
    reverb->set_mix(0.2f);
    engine.add_effect(reverb);

    // 2. Serialize the state to JSON using the new Autosave hooks
    nlohmann::json saved_state = engine.serialize();

    // 3. Deliberately ruin the current state (simulate user messing it up before crash)
    distortion->set_enabled(false);
    distortion->set_mix(0.0f);
    reverb->set_enabled(true);
    reverb->set_mix(1.0f);

    // 4. Trigger Crash Recovery (Deserialize)
    engine.deserialize(saved_state);

    // 5. Assert that everything was restored perfectly
    ASSERT_TRUE(distortion->is_enabled());
    ASSERT_NEAR(distortion->get_mix(), 0.85f, 0.001f); 

    ASSERT_FALSE(reverb->is_enabled());
    ASSERT_NEAR(reverb->get_mix(), 0.2f, 0.001f);
    
    engine.shutdown();
}

TEST(json_graph_node_and_link_roundtrip) {
    PresetData preset;
    preset.routing = "graph";
    
    PresetData::NodeData n;
    n.id = "n1";
    n.type = "Overdrive";
    n.enabled = true;
    n.mix = 0.5f;
    n.num_inputs = 2;
    n.x = 10.0f;
    n.y = 20.0f;
    n.params.push_back({"Drive", 1.0f});
    n.metadata["test"] = "meta";
    preset.nodes.push_back(n);

    PresetData::LinkData l;
    l.src_pin = "p1";
    l.dst_pin = "p2";
    preset.links.push_back(l);

    std::string json_str = to_json_ext(preset);
    
    PresetData restored;
    bool ok = from_json_ext(json_str, restored);
    ASSERT_TRUE(ok);
    
    ASSERT_EQ(restored.nodes.size(), 1u);
    ASSERT_EQ(restored.nodes[0].id, "n1");
    ASSERT_EQ(restored.nodes[0].type, "Overdrive");
    ASSERT_TRUE(restored.nodes[0].enabled);
    ASSERT_EQ(restored.nodes[0].mix, 0.5f);
    ASSERT_EQ(restored.nodes[0].num_inputs, 2);
    ASSERT_EQ(restored.nodes[0].x, 10.0f);
    ASSERT_EQ(restored.nodes[0].y, 20.0f);
    ASSERT_EQ(restored.nodes[0].params.size(), 1u);
    ASSERT_EQ(restored.nodes[0].metadata["test"], "meta");

    ASSERT_EQ(restored.links.size(), 1u);
    ASSERT_EQ(restored.links[0].src_pin, "p1");
    ASSERT_EQ(restored.links[0].dst_pin, "p2");
}

TEST(json_from_json_ext_exceptions) {
    PresetData p;
    // Missing nodes
    bool ok1 = from_json_ext(R"({"routing": "graph", "links": []})", p);
    ASSERT_FALSE(ok1);
    
    // Missing links
    bool ok2 = from_json_ext(R"({"routing": "graph", "nodes": []})", p);
    ASSERT_FALSE(ok2);
}

TEST(json_from_json_exceptions) {
    nlohmann::json j1 = {
        {"routing", "graph"},
        {"links", nlohmann::json::array()}
    };
    PresetData p1;
    bool caught1 = false;
    try {
        from_json(j1, p1);
    } catch(const std::invalid_argument&) {
        caught1 = true;
    }
    ASSERT_TRUE(caught1);

    nlohmann::json j2 = {
        {"routing", "graph"},
        {"nodes", nlohmann::json::array()}
    };
    PresetData p2;
    bool caught2 = false;
    try {
        from_json(j2, p2);
    } catch(const std::invalid_argument&) {
        caught2 = true;
    }
    ASSERT_TRUE(caught2);
}

TEST(json_from_ordered_json_missing_fields) {
    // Actually the namespace requires us to use from_json_ext because ordered json hook is internal.
    // So we just parse from string
    PresetData p;
    bool ok = from_json_ext(R"({
        "format_version": 2,
        "routing": "graph",
        "name": "Missing Fields",
        "nodes": [
            { "id": "n1", "type": "test" }
        ],
        "links": []
    })", p);
    ASSERT_TRUE(ok);
    ASSERT_EQ(p.nodes.size(), 1u);
    ASSERT_EQ(p.nodes[0].id, "n1");
    ASSERT_TRUE(p.nodes[0].enabled); // Default true
    ASSERT_EQ(p.nodes[0].mix, 1.0f);
}

TEST(json_from_json_missing_fields) {
    nlohmann::json j = nlohmann::json::parse(R"({
        "format_version": 2,
        "routing": "graph",
        "name": "Missing Fields",
        "nodes": [
            { "id": "n1", "type": "test" }
        ],
        "links": []
    })");
    PresetData p;
    from_json(j, p);
    ASSERT_EQ(p.nodes.size(), 1u);
    ASSERT_EQ(p.nodes[0].id, "n1");
    ASSERT_TRUE(p.nodes[0].enabled); 
    ASSERT_EQ(p.nodes[0].mix, 1.0f);
}

TEST(json_from_json_invalid_types_skipped) {
    nlohmann::json j = nlohmann::json::parse(R"({
        "type": "Delay",
        "params": {
            "Valid": 1.0,
            "Invalid": "string_instead_of_number"
        },
        "metadata": {
            "ValidMeta": "string",
            "InvalidMeta": 123
        }
    })");
    PresetData::EffectData fx;
    from_json(j, fx);
    
    ASSERT_EQ(fx.params.size(), 1u);
    ASSERT_EQ(fx.params[0].first, "Valid");
    ASSERT_EQ(fx.metadata.size(), 1u);
    ASSERT_EQ(fx.metadata["ValidMeta"], "string");
}

TEST(json_from_json_node_invalid_types_skipped) {
    nlohmann::json j = nlohmann::json::parse(R"({
        "routing": "graph",
        "name": "Test",
        "nodes": [
            {
                "id": "n1",
                "type": "Delay",
                "params": {
                    "Valid": 1.0,
                    "Invalid": "string"
                },
                "metadata": {
                    "ValidMeta": "string",
                    "InvalidMeta": 123
                }
            }
        ],
        "links": []
    })");
    PresetData p;
    from_json(j, p);
    
    ASSERT_EQ(p.nodes.size(), 1u);
    ASSERT_EQ(p.nodes[0].params.size(), 1u);
    ASSERT_EQ(p.nodes[0].params[0].first, "Valid");
    ASSERT_EQ(p.nodes[0].metadata.size(), 1u);
    ASSERT_EQ(p.nodes[0].metadata["ValidMeta"], "string");
}

TEST(json_from_ordered_json_invalid_types_skipped) {
    PresetData p;
    bool ok = from_json_ext(R"({
        "format_version": 2,
        "routing": "graph",
        "name": "Test",
        "nodes": [
            {
                "id": "n1",
                "type": "Delay",
                "params": {
                    "Valid": 1.0,
                    "Invalid": "string"
                },
                "metadata": {
                    "ValidMeta": "string",
                    "InvalidMeta": 123
                }
            }
        ],
        "links": []
    })", p);
    
    ASSERT_TRUE(ok);
    ASSERT_EQ(p.nodes.size(), 1u);
    ASSERT_EQ(p.nodes[0].params.size(), 1u);
    ASSERT_EQ(p.nodes[0].params[0].first, "Valid");
    ASSERT_EQ(p.nodes[0].metadata.size(), 1u);
    ASSERT_EQ(p.nodes[0].metadata["ValidMeta"], "string");
}

TEST(json_from_ordered_json_effect_invalid_types_skipped) {
    PresetData p;
    bool ok = from_json_ext(R"({
        "format_version": 2,
        "routing": "linear",
        "name": "Test",
        "effects": [
            {
                "type": "Delay",
                "params": {
                    "Valid": 1.0,
                    "Invalid": "string"
                },
                "metadata": {
                    "ValidMeta": "string",
                    "InvalidMeta": 123
                }
            }
        ]
    })", p);
    
    ASSERT_TRUE(ok);
    ASSERT_EQ(p.effects.size(), 1u);
    ASSERT_EQ(p.effects[0].params.size(), 1u);
    ASSERT_EQ(p.effects[0].metadata.size(), 1u);
}

TEST(json_to_json_linear_routing) {
    PresetData p;
    p.routing = "linear";
    p.effects.push_back({"Delay", true, 0.5f, {{"Time", 0.5f}}, {}});
    
    nlohmann::json j = p; // Uses to_json ADL hook
    ASSERT_TRUE(j.contains("effects"));
    ASSERT_EQ(j["effects"].size(), 1u);
    ASSERT_EQ(j["effects"][0]["type"], "Delay");
}

TEST(json_from_json_skips_empty_type) {
    nlohmann::json j = nlohmann::json::parse(R"({
        "routing": "linear",
        "effects": [
            { "type": "" },
            { "type": "Valid" }
        ],
        "nodes": [
            { "id": "n1", "type": "" },
            { "id": "", "type": "Valid" },
            { "id": "n3", "type": "Valid" }
        ],
        "links": [
            { "src_pin": "", "dst_pin": "n2.in" },
            { "src_pin": "n1.out", "dst_pin": "" },
            { "src_pin": "n1.out", "dst_pin": "n2.in" }
        ]
    })");
    PresetData p;
    from_json(j, p);
    
    ASSERT_EQ(p.effects.size(), 1u);
    ASSERT_EQ(p.effects[0].type, "Valid");
    ASSERT_EQ(p.nodes.size(), 1u);
    ASSERT_EQ(p.nodes[0].id, "n3");
    ASSERT_EQ(p.links.size(), 1u);
}

TEST(json_from_ordered_json_skips_empty_type) {
    PresetData p;
    bool ok = from_json_ext(R"({
        "format_version": 2,
        "routing": "linear",
        "effects": [
            { "type": "" },
            { "type": "Valid" }
        ],
        "nodes": [
            { "id": "n1", "type": "" },
            { "id": "", "type": "Valid" },
            { "id": "n3", "type": "Valid" }
        ],
        "links": [
            { "src_pin": "", "dst_pin": "n2.in" },
            { "src_pin": "n1.out", "dst_pin": "" },
            { "src_pin": "n1.out", "dst_pin": "n2.in" }
        ]
    })", p);
    
    ASSERT_TRUE(ok);
    ASSERT_EQ(p.effects.size(), 1u);
    ASSERT_EQ(p.nodes.size(), 1u);
    ASSERT_EQ(p.links.size(), 1u);
}

TEST(json_from_json_missing_nodes_throws) {
    nlohmann::json j = nlohmann::json::parse(R"({"routing": "graph"})");
    PresetData p;
    ASSERT_THROW(from_json(j, p), std::invalid_argument);
}

TEST(json_from_json_missing_links_throws) {
    nlohmann::json j = nlohmann::json::parse(R"({"routing": "graph", "nodes": []})");
    PresetData p;
    ASSERT_THROW(from_json(j, p), std::invalid_argument);
}TEST(json_midi_mapping) {
    PresetData p;
    p.routing = "linear";
    MidiMapping m;
    m.cc_number = 7;
    m.midi_channel = 1;
    m.target_type = MidiTargetType::EffectParam;
    m.mode = MidiMappingMode::Continuous;
    m.effect_name = "Delay";
    m.param_name = "Mix";
    p.midi_mappings.push_back(m);
    
    std::string json_str = to_json_ext(p);
    PresetData p2;
    from_json_ext(json_str, p2);
    
    ASSERT_EQ(p2.midi_mappings.size(), 1u);
    ASSERT_EQ(p2.midi_mappings[0].cc_number, 7);
    ASSERT_EQ(p2.midi_mappings[0].midi_channel, 1);
    ASSERT_EQ(p2.midi_mappings[0].effect_name, "Delay");
    ASSERT_EQ(p2.midi_mappings[0].param_name, "Mix");
}

TEST(json_midi_mapping_missing_fields) {
    nlohmann::json j = nlohmann::json::parse(R"({"routing": "linear", "midi_mappings": [{}]})");
    PresetData p;
    from_json(j, p);
    ASSERT_EQ(p.midi_mappings.size(), 1u);
    ASSERT_EQ(p.midi_mappings[0].cc_number, 0);
    ASSERT_EQ(p.midi_mappings[0].midi_channel, -1);
    ASSERT_EQ(p.midi_mappings[0].effect_name, "");
}



