/**
 * @file preset_json.cpp
 * @brief Preset serialization / deserialization using nlohmann/json.
 *
 * This replaces the previous hand-rolled string-manipulation parser with a
 * proper, well-tested JSON library (nlohmann/json v3.11+).
 *
 * Design goals
 * ------------
 * 1. **Drop-in replacement** – the on-disk JSON format is unchanged; existing
 *    preset files load without modification.
 * 2. **Standard C++17 interface** – nlohmann ADL hooks (to_json / from_json)
 *    make PresetData and EffectData first-class nlohmann types, so callers
 *    can write `nlohmann::json j = preset;` directly.
 * 3. **Robust error handling** – every parse operation is wrapped in
 *    try/catch; on failure from_json_ext logs to std::cerr and returns false
 *    without mutating the output parameter.
 * 4. **Preserves midi_mappings and metadata** – nothing that the old parser
 *    supported is dropped.
 */

#include "preset_json.h"
#include "midi/midi_manager.h"

#include <ctime>
#include <stdexcept>
#include <iostream>
#include <sstream>

namespace Amplitron {

namespace {

using OrderedJson = nlohmann::ordered_json;

// These ordered helpers are used by to_json_ext/from_json_ext so that the
// preset string/file round-trip preserves effect parameter insertion order.
// nlohmann::json stores object keys in sorted order by default, which breaks
// tests and callers that rely on the original fx.params sequence.
void to_ordered_json(OrderedJson &j, const PresetData::EffectData &fx) {
  OrderedJson params_obj = OrderedJson::object();
  for (const auto &[name, value] : fx.params) {
    params_obj[name] = value;
  }

  j = OrderedJson::object();
  j["type"] = fx.type;
  j["enabled"] = fx.enabled;
  j["mix"] = fx.mix;
  j["params"] = std::move(params_obj);

  if (!fx.metadata.empty()) {
    OrderedJson metadata_obj = OrderedJson::object();
    for (const auto &[key, value] : fx.metadata) {
      metadata_obj[key] = value;
    }
    j["metadata"] = std::move(metadata_obj);
  }
}

void to_ordered_json(OrderedJson &j, const PresetData::NodeData &node) {
  OrderedJson params_obj = OrderedJson::object();
  for (const auto &[name, value] : node.params) {
    params_obj[name] = value;
  }

  j = OrderedJson::object();
  j["id"] = node.id;
  j["type"] = node.type;
  j["position"] = {{"x", node.x}, {"y", node.y}};
  j["enabled"] = node.enabled;
  j["mix"] = node.mix;
  if (node.num_inputs > 0) {
    j["num_inputs"] = node.num_inputs;
  }
  if (!node.params.empty()) {
    j["params"] = std::move(params_obj);
  }
  if (!node.metadata.empty()) {
    OrderedJson metadata_obj = OrderedJson::object();
    for (const auto &[key, value] : node.metadata) {
      metadata_obj[key] = value;
    }
    j["metadata"] = std::move(metadata_obj);
  }
}

void from_ordered_json(const OrderedJson &j, PresetData::NodeData &node) {
  node.id = j.value("id", std::string{});
  node.type = j.value("type", std::string{});
  node.enabled = j.value("enabled", true);
  node.mix = j.value("mix", 1.0f);
  node.num_inputs = j.value("num_inputs", 0);

  if (j.contains("position") && j["position"].is_object()) {
    node.x = j["position"].value("x", 0.0f);
    node.y = j["position"].value("y", 0.0f);
  }

  node.params.clear();
  node.metadata.clear();

  if (j.contains("params") && j["params"].is_object()) {
    for (auto it = j["params"].begin(); it != j["params"].end(); ++it) {
      if (it.value().is_number()) {
        node.params.emplace_back(it.key(), it.value().get<float>());
      }
    }
  }

  if (j.contains("metadata") && j["metadata"].is_object()) {
    for (auto it = j["metadata"].begin(); it != j["metadata"].end(); ++it) {
      if (it.value().is_string()) {
        node.metadata[it.key()] = it.value().get<std::string>();
      }
    }
  }
}

void to_ordered_json(OrderedJson &j, const PresetData::LinkData &link) {
  j = OrderedJson::object();
  j["src_pin"] = link.src_pin;
  j["dst_pin"] = link.dst_pin;
}

void from_ordered_json(const OrderedJson &j, PresetData::LinkData &link) {
  link.src_pin = j.value("src_pin", std::string{});
  link.dst_pin = j.value("dst_pin", std::string{});
}

void from_ordered_json(const OrderedJson &j, PresetData::EffectData &fx) {
  fx.type = j.value("type", std::string{});
  fx.enabled = j.value("enabled", false);
  fx.mix = j.value("mix", 1.0f);

  fx.params.clear();
  fx.metadata.clear();

  if (j.contains("params") && j["params"].is_object()) {
    for (auto it = j["params"].begin(); it != j["params"].end(); ++it) {
      if (it.value().is_number()) {
        fx.params.emplace_back(it.key(), it.value().get<float>());
      }
    }
  }

  if (j.contains("metadata") && j["metadata"].is_object()) {
    for (auto it = j["metadata"].begin(); it != j["metadata"].end(); ++it) {
      if (it.value().is_string()) {
        fx.metadata[it.key()] = it.value().get<std::string>();
      }
    }
  }
}

void to_ordered_json_midi(OrderedJson &j, const MidiMapping &m) {
  j = OrderedJson::object();
  j["cc"] = m.cc_number;
  j["channel"] = m.midi_channel;
  j["target"] = static_cast<int>(m.target_type);
  j["mode"] = static_cast<int>(m.mode);
  j["effect"] = m.effect_name;
  j["param"] = m.param_name;
}

void from_ordered_json_midi(const OrderedJson &j, MidiMapping &m) {
  m.cc_number = j.value("cc", 0);
  m.midi_channel = j.value("channel", -1);
  m.target_type = static_cast<MidiTargetType>(j.value("target", 0));
  m.mode = static_cast<MidiMappingMode>(j.value("mode", 0));
  m.effect_name = j.value("effect", std::string{});
  m.param_name = j.value("param", std::string{});
}

void to_ordered_json(OrderedJson &j, const PresetData &preset) {
  // Generate an ISO-8601 timestamp
  std::time_t now = std::time(nullptr);
  char timebuf[64] = {};
  std::tm tm_info{};
#ifdef _WIN32
  localtime_s(&tm_info, &now);
#else
  localtime_r(&now, &tm_info);
#endif
  std::strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%S", &tm_info);

  OrderedJson effects_arr = OrderedJson::array();
  for (const auto &fx : preset.effects) {
    OrderedJson jfx;
    to_ordered_json(jfx, fx);
    effects_arr.push_back(std::move(jfx));
  }

  OrderedJson midi_arr = OrderedJson::array();
  for (const auto &m : preset.midi_mappings) {
    OrderedJson jm;
    to_ordered_json_midi(jm, m);
    midi_arr.push_back(std::move(jm));
  }

  OrderedJson nodes_arr = OrderedJson::array();
  for (const auto &node : preset.nodes) {
    OrderedJson jn;
    to_ordered_json(jn, node);
    nodes_arr.push_back(std::move(jn));
  }

  OrderedJson links_arr = OrderedJson::array();
  for (const auto &link : preset.links) {
    OrderedJson jl;
    to_ordered_json(jl, link);
    links_arr.push_back(std::move(jl));
  }

  j = OrderedJson::object();
  j["format_version"] = 2; // Increased format version for graph presets
  j["routing"] = preset.routing;
  j["name"] = preset.name;
  j["description"] = preset.description;
  j["saved_at"] = timebuf;
  j["input_gain"] = preset.input_gain;
  j["output_gain"] = preset.output_gain;

  if (preset.routing == "linear") {
    j["effects"] = std::move(effects_arr);
  } else {
    j["nodes"] = std::move(nodes_arr);
    j["links"] = std::move(links_arr);
  }

  j["midi_mappings"] = std::move(midi_arr);
}

void from_ordered_json(const OrderedJson &j, PresetData &preset) {
  preset.name = j.value("name", std::string{});
  preset.description = j.value("description", std::string{});
  preset.routing = j.value("routing", std::string{"linear"});
  preset.input_gain = j.value("input_gain", 0.7f);
  preset.output_gain = j.value("output_gain", 0.8f);

  if (preset.routing == "graph") {
    if (!j.contains("nodes") || !j["nodes"].is_array()) {
      throw std::invalid_argument("Malformed graph preset: missing or invalid 'nodes' array");
    }
    if (!j.contains("links") || !j["links"].is_array()) {
      throw std::invalid_argument("Malformed graph preset: missing or invalid 'links' array");
    }
  }

  preset.effects.clear();
  preset.nodes.clear();
  preset.links.clear();
  preset.midi_mappings.clear();

  if (j.contains("effects") && j["effects"].is_array()) {
    for (const auto &jfx : j["effects"]) {
      PresetData::EffectData fx;
      from_ordered_json(jfx, fx);
      if (!fx.type.empty()) {
        preset.effects.push_back(std::move(fx));
      }
    }
  }

  if (j.contains("nodes") && j["nodes"].is_array()) {
    for (const auto &jn : j["nodes"]) {
      PresetData::NodeData node;
      from_ordered_json(jn, node);
      if (!node.id.empty() && !node.type.empty()) {
        preset.nodes.push_back(std::move(node));
      }
    }
  }

  if (j.contains("links") && j["links"].is_array()) {
    for (const auto &jl : j["links"]) {
      PresetData::LinkData link;
      from_ordered_json(jl, link);
      if (!link.src_pin.empty() && !link.dst_pin.empty()) {
        preset.links.push_back(std::move(link));
      }
    }
  }

  if (j.contains("midi_mappings") && j["midi_mappings"].is_array()) {
    for (const auto &jm : j["midi_mappings"]) {
      MidiMapping m;
      from_ordered_json_midi(jm, m);
      preset.midi_mappings.push_back(m);
    }
  }
}

} // namespace

// ============================================================
// ADL hook: EffectData  ←→  nlohmann::json
// ============================================================

void to_json(nlohmann::json &j, const PresetData::EffectData &fx) {
  // Build the flat params object: { "Drive": 2.0, "Tone": 0.6, ... }
  nlohmann::json params_obj = nlohmann::json::object();
  for (const auto &[name, value] : fx.params) {
    params_obj[name] = value;
  }

  j = {
      {"type", fx.type},
      {"enabled", fx.enabled},
      {"mix", fx.mix},
      {"params", params_obj},
  };

  // Optional metadata sub-object (e.g. IR cabinet file path)
  if (!fx.metadata.empty()) {
    j["metadata"] = fx.metadata;
  }
}

void from_json(const nlohmann::json &j, PresetData::EffectData &fx) {
  fx.type = j.value("type", std::string{});
  fx.enabled = j.value("enabled", false);
  fx.mix = j.value("mix", 1.0f);

  // Clear before repopulating so reusing an object never carries stale data.
  fx.params.clear();
  fx.metadata.clear();

  if (j.contains("params") && j["params"].is_object()) {
    for (const auto &[key, val] : j["params"].items()) {
      if (val.is_number()) {
        fx.params.push_back({key, val.get<float>()});
      }
    }
  }

  if (j.contains("metadata") && j["metadata"].is_object()) {
    for (const auto &[key, val] : j["metadata"].items()) {
      if (val.is_string()) {
        fx.metadata[key] = val.get<std::string>();
      }
    }
  }
}

// ============================================================
// ADL hook: MidiMapping  ←→  nlohmann::json
// ============================================================

static void to_json_midi(nlohmann::json &j, const MidiMapping &m) {
  j = {
      {"cc", m.cc_number},
      {"channel", m.midi_channel},
      {"target", static_cast<int>(m.target_type)},
      {"mode", static_cast<int>(m.mode)},
      {"effect", m.effect_name},
      {"param", m.param_name},
  };
}

static void from_json_midi(const nlohmann::json &j, MidiMapping &m) {
  m.cc_number = j.value("cc", 0);
  m.midi_channel = j.value("channel", -1);
  m.target_type = static_cast<MidiTargetType>(j.value("target", 0));
  m.mode = static_cast<MidiMappingMode>(j.value("mode", 0));
  m.effect_name = j.value("effect", std::string{});
  m.param_name = j.value("param", std::string{});
}

// ============================================================
// ADL hook: PresetData  ←→  nlohmann::json
// ============================================================

void to_json(nlohmann::json &j, const PresetData &preset) {
  // Generate an ISO-8601 timestamp
  std::time_t now = std::time(nullptr);
  char timebuf[64] = {};
  std::tm tm_info{};
#ifdef _WIN32
  localtime_s(&tm_info, &now);
#else
  localtime_r(&now, &tm_info);
#endif
  std::strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%S", &tm_info);

  // Build effects array using the EffectData ADL hook
  nlohmann::json effects_arr = nlohmann::json::array();
  for (const auto &fx : preset.effects) {
    nlohmann::json jfx;
    to_json(jfx, fx);
    effects_arr.push_back(std::move(jfx));
  }

  // Build midi_mappings array
  nlohmann::json midi_arr = nlohmann::json::array();
  for (const auto &m : preset.midi_mappings) {
    nlohmann::json jm;
    to_json_midi(jm, m);
    midi_arr.push_back(std::move(jm));
  }

  // Build nodes and links arrays
  nlohmann::json nodes_arr = nlohmann::json::array();
  for (const auto &node : preset.nodes) {
    nlohmann::json jn = {{"id", node.id},
                         {"type", node.type},
                         {"position", {{"x", node.x}, {"y", node.y}}},
                         {"enabled", node.enabled},
                         {"mix", node.mix}};
    if (node.num_inputs > 0)
      jn["num_inputs"] = node.num_inputs;
    nlohmann::json params_obj = nlohmann::json::object();
    for (const auto &[name, value] : node.params) {
      params_obj[name] = value;
    }
    if (!node.params.empty())
      jn["params"] = params_obj;
    if (!node.metadata.empty())
      jn["metadata"] = node.metadata;
    nodes_arr.push_back(std::move(jn));
  }

  nlohmann::json links_arr = nlohmann::json::array();
  for (const auto &link : preset.links) {
    links_arr.push_back({{"src_pin", link.src_pin}, {"dst_pin", link.dst_pin}});
  }

  j = {
      {"format_version", 2},
      {"routing", preset.routing},
      {"name", preset.name},
      {"description", preset.description},
      {"saved_at", timebuf},
      {"input_gain", preset.input_gain},
      {"output_gain", preset.output_gain},
      {"midi_mappings", std::move(midi_arr)},
  };

  if (preset.routing == "linear") {
    j["effects"] = std::move(effects_arr);
  } else {
    j["nodes"] = std::move(nodes_arr);
    j["links"] = std::move(links_arr);
  }
}

void from_json(const nlohmann::json &j, PresetData &preset) {
  preset.name = j.value("name", std::string{});
  preset.description = j.value("description", std::string{});
  preset.routing = j.value("routing", std::string{"linear"});
  preset.input_gain = j.value("input_gain", 0.7f);
  preset.output_gain = j.value("output_gain", 0.8f);

  if (preset.routing == "graph") {
    if (!j.contains("nodes") || !j["nodes"].is_array()) {
      throw std::invalid_argument("Malformed graph preset: missing or invalid 'nodes' array");
    }
    if (!j.contains("links") || !j["links"].is_array()) {
      throw std::invalid_argument("Malformed graph preset: missing or invalid 'links' array");
    }
  }

  // Clear before repopulating so parsing into a non-empty PresetData never
  // duplicates/retains old entries.
  preset.effects.clear();
  preset.nodes.clear();
  preset.links.clear();
  preset.midi_mappings.clear();

  if (j.contains("effects") && j["effects"].is_array()) {
    for (const auto &jfx : j["effects"]) {
      PresetData::EffectData fx;
      from_json(jfx, fx);
      if (!fx.type.empty()) {
        preset.effects.push_back(std::move(fx));
      }
    }
  }

  if (j.contains("nodes") && j["nodes"].is_array()) {
    for (const auto &jn : j["nodes"]) {
      PresetData::NodeData node;
      node.id = jn.value("id", std::string{});
      node.type = jn.value("type", std::string{});
      node.enabled = jn.value("enabled", true);
      node.mix = jn.value("mix", 1.0f);
      node.num_inputs = jn.value("num_inputs", 0);
      if (jn.contains("position") && jn["position"].is_object()) {
        node.x = jn["position"].value("x", 0.0f);
        node.y = jn["position"].value("y", 0.0f);
      }
      if (jn.contains("params") && jn["params"].is_object()) {
        for (const auto &[key, val] : jn["params"].items()) {
          if (val.is_number())
            node.params.push_back({key, val.get<float>()});
        }
      }
      if (jn.contains("metadata") && jn["metadata"].is_object()) {
        for (const auto &[key, val] : jn["metadata"].items()) {
          if (val.is_string())
            node.metadata[key] = val.get<std::string>();
        }
      }
      if (!node.id.empty() && !node.type.empty()) {
        preset.nodes.push_back(std::move(node));
      }
    }
  }

  if (j.contains("links") && j["links"].is_array()) {
    for (const auto &jl : j["links"]) {
      PresetData::LinkData link;
      link.src_pin = jl.value("src_pin", std::string{});
      link.dst_pin = jl.value("dst_pin", std::string{});
      if (!link.src_pin.empty() && !link.dst_pin.empty()) {
        preset.links.push_back(std::move(link));
      }
    }
  }

  if (j.contains("midi_mappings") && j["midi_mappings"].is_array()) {
    for (const auto &jm : j["midi_mappings"]) {
      MidiMapping m;
      from_json_midi(jm, m);
      preset.midi_mappings.push_back(m);
    }
  }
}

// ============================================================
// Public helpers used by PresetManager
// ============================================================

std::string to_json_ext(const PresetData &preset) {
  OrderedJson j;
  to_ordered_json(j, preset);
  return j.dump(4) + "\n";
}

bool from_json_ext(const std::string &json_str, PresetData &preset) {
  // Deserialize into a temporary so that a mid-way exception never leaves
  // `preset` in a partially-mutated state.
  try {
    OrderedJson j = OrderedJson::parse(json_str);
    PresetData tmp;
    from_ordered_json(j, tmp);
    preset = std::move(tmp);
    return true;
  } catch (const nlohmann::json::exception &e) {
    std::cerr << "[preset_json] JSON parse error: " << e.what() << std::endl;
    return false;
  } catch (const std::exception &e) {
    std::cerr << "[preset_json] Error: " << e.what() << std::endl;
    return false;
  }
}

} // namespace Amplitron
