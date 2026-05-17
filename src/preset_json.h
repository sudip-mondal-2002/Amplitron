#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include "preset_manager.h"

namespace Amplitron {

// ---------------------------------------------------------------------------
// Low-level helpers (kept for unit-test access and migration code)
// ---------------------------------------------------------------------------

/**
 * @brief Serialise a PresetData to a pretty-printed JSON string.
 *
 * Replaces the old hand-rolled to_json_ext() with a proper nlohmann/json
 * implementation.  The output format is identical to the previous version
 * (same keys, same nesting) so all existing preset files remain compatible.
 */
std::string to_json_ext(const PresetData& preset);

/**
 * @brief Deserialise a JSON string into a PresetData.
 *
 * Returns true on success.  On failure the function logs to std::cerr and
 * returns false without mutating the output parameter.
 */
bool from_json_ext(const std::string& json_str, PresetData& preset);

// ---------------------------------------------------------------------------
// nlohmann ADL hooks — allow nlohmann::json j = preset; and vice-versa
// ---------------------------------------------------------------------------
void to_json(nlohmann::json& j, const PresetData::EffectData& fx);
void from_json(const nlohmann::json& j, PresetData::EffectData& fx);

void to_json(nlohmann::json& j, const PresetData& preset);
void from_json(const nlohmann::json& j, PresetData& preset);

} // namespace Amplitron
