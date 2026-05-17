#include "preset_json.h"
#include <sstream>
#include <ctime>

namespace Amplitron {
std::string escape_json_string_ext(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out += c;
        }
    }
    return out;
}

std::string unescape_json_string_ext(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i + 1]) {
                case '"':  out += '"'; ++i; break;
                case '\\': out += '\\'; ++i; break;
                case 'n':  out += '\n'; ++i; break;
                case 'r':  out += '\r'; ++i; break;
                case 't':  out += '\t'; ++i; break;
                default:   out += s[i]; break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

std::string to_json_ext(const PresetData& preset) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"format_version\": 1,\n";
    ss << "  \"name\": \"" << escape_json_string_ext(preset.name) << "\",\n";
    ss << "  \"description\": \"" << escape_json_string_ext(preset.description) << "\",\n";

    // Timestamp
    std::time_t now = std::time(nullptr);
    char timebuf[64];
    std::tm time_info;
#ifdef _WIN32
    localtime_s(&time_info, &now);
#else
    localtime_r(&now, &time_info);
#endif
    std::strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%S", &time_info);
    ss << "  \"saved_at\": \"" << timebuf << "\",\n";

    ss << "  \"input_gain\": " << preset.input_gain << ",\n";
    ss << "  \"output_gain\": " << preset.output_gain << ",\n";
    ss << "  \"effects\": [\n";

    for (size_t e = 0; e < preset.effects.size(); ++e) {
        const auto& fx = preset.effects[e];
        ss << "    {\n";
        ss << "      \"type\": \"" << escape_json_string_ext(fx.type) << "\",\n";
        ss << "      \"enabled\": " << (fx.enabled ? "true" : "false") << ",\n";
        ss << "      \"mix\": " << fx.mix << ",\n";
        ss << "      \"params\": {\n";

        for (size_t p = 0; p < fx.params.size(); ++p) {
            ss << "        \"" << escape_json_string_ext(fx.params[p].first) << "\": "
               << fx.params[p].second;
            if (p + 1 < fx.params.size()) ss << ",";
            ss << "\n";
        }

        ss << "      }";

        // Serialize metadata if present
        if (!fx.metadata.empty()) {
            ss << ",\n      \"metadata\": {\n";
            size_t m = 0;
            for (const auto& kv : fx.metadata) {
                ss << "        \"" << escape_json_string_ext(kv.first) << "\": \""
                   << escape_json_string_ext(kv.second) << "\"";
                if (m + 1 < fx.metadata.size()) ss << ",";
                ss << "\n";
                ++m;
            }
            ss << "      }";
        }

        ss << "\n";
        ss << "    }";
        if (e + 1 < preset.effects.size()) ss << ",";
        ss << "\n";
    }

    ss << "  ],\n";

    ss << "  \"midi_mappings\": [\n";
    for (size_t m = 0; m < preset.midi_mappings.size(); ++m) {
        const auto& mm = preset.midi_mappings[m];
        ss << "    {\n";
        ss << "      \"cc\": " << mm.cc_number << ",\n";
        ss << "      \"channel\": " << mm.midi_channel << ",\n";
        ss << "      \"target\": " << static_cast<int>(mm.target_type) << ",\n";
        ss << "      \"mode\": " << static_cast<int>(mm.mode) << ",\n";
        ss << "      \"effect\": \"" << escape_json_string_ext(mm.effect_name) << "\",\n";
        ss << "      \"param\": \"" << escape_json_string_ext(mm.param_name) << "\"\n";
        ss << "    }";
        if (m + 1 < preset.midi_mappings.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";

    ss << "}\n";
    return ss.str();
}

// Minimal JSON parser helpers
static std::string extract_string_value(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos);
    if (pos == std::string::npos) return "";
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return "";
    size_t end = pos + 1;
    while (end < json.size() && !(json[end] == '"' && json[end - 1] != '\\')) ++end;
    return json.substr(pos + 1, end - pos - 1);
}

static float extract_float_value(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return 0.0f;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return 0.0f;
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
    try {
        return std::stof(json.substr(pos));
    } catch (...) {
        return 0.0f;
    }
}

static int extract_int_value(const std::string& json, const std::string& key, int def = 0) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return def;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return def;
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
    try {
        return std::stoi(json.substr(pos));
    } catch (...) {
        return def;
    }
}

static bool extract_bool_value(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return false;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return false;
    return json.find("true", pos) < json.find("false", pos);
}

bool from_json_ext(const std::string& json, PresetData& preset) {
    preset.name = unescape_json_string_ext(extract_string_value(json, "name"));
    preset.description = unescape_json_string_ext(extract_string_value(json, "description"));
    preset.input_gain = extract_float_value(json, "input_gain");
    preset.output_gain = extract_float_value(json, "output_gain");

    // Parse effects array
    size_t effects_pos = json.find("\"effects\"");
    if (effects_pos == std::string::npos) return true;

    size_t arr_start = json.find('[', effects_pos);
    if (arr_start == std::string::npos) return true;

    // Find each effect object {...}
    size_t search_pos = arr_start;
    while (true) {
        size_t obj_start = json.find('{', search_pos + 1);
        if (obj_start == std::string::npos) break;

        // Find matching closing brace (handle nested braces for params)
        int depth = 0;
        size_t obj_end = obj_start;
        for (size_t i = obj_start; i < json.size(); ++i) {
            if (json[i] == '{') ++depth;
            if (json[i] == '}') {
                --depth;
                if (depth == 0) { obj_end = i; break; }
            }
        }
        if (obj_end <= obj_start) break;

        std::string obj = json.substr(obj_start, obj_end - obj_start + 1);

        PresetData::EffectData fx;
        fx.type = unescape_json_string_ext(extract_string_value(obj, "type"));
        fx.enabled = extract_bool_value(obj, "enabled");
        fx.mix = extract_float_value(obj, "mix");

        // Parse params sub-object
        size_t params_pos = obj.find("\"params\"");
        if (params_pos != std::string::npos) {
            size_t p_start = obj.find('{', params_pos);
            size_t p_end = obj.find('}', p_start + 1);
            if (p_start != std::string::npos && p_end != std::string::npos) {
                std::string params_str = obj.substr(p_start + 1, p_end - p_start - 1);

                // Parse key: value pairs
                size_t pos = 0;
                while (pos < params_str.size()) {
                    size_t key_start = params_str.find('"', pos);
                    if (key_start == std::string::npos) break;
                    size_t key_end = params_str.find('"', key_start + 1);
                    if (key_end == std::string::npos) break;

                    std::string pkey = params_str.substr(key_start + 1, key_end - key_start - 1);

                    size_t colon = params_str.find(':', key_end);
                    if (colon == std::string::npos) break;

                    size_t val_start = colon + 1;
                    while (val_start < params_str.size() &&
                           (params_str[val_start] == ' ' || params_str[val_start] == '\t'))
                        ++val_start;

                    try {
                        float val = std::stof(params_str.substr(val_start));
                        fx.params.push_back({pkey, val});
                    } catch (...) {}

                    pos = params_str.find(',', val_start);
                    if (pos == std::string::npos) break;
                    ++pos;
                }
            }
        }

        // Parse optional metadata sub-object
        size_t meta_pos = obj.find("\"metadata\"");
        if (meta_pos != std::string::npos) {
            size_t m_start = obj.find('{', meta_pos);
            size_t m_end = obj.find('}', m_start + 1);
            if (m_start != std::string::npos && m_end != std::string::npos) {
                std::string meta_str = obj.substr(m_start + 1, m_end - m_start - 1);

                size_t mpos = 0;
                while (mpos < meta_str.size()) {
                    size_t mk_start = meta_str.find('"', mpos);
                    if (mk_start == std::string::npos) break;
                    size_t mk_end = meta_str.find('"', mk_start + 1);
                    if (mk_end == std::string::npos) break;

                    std::string mkey = meta_str.substr(mk_start + 1, mk_end - mk_start - 1);

                    size_t mcolon = meta_str.find(':', mk_end);
                    if (mcolon == std::string::npos) break;

                    size_t mv_start = meta_str.find('"', mcolon + 1);
                    if (mv_start == std::string::npos) break;
                    size_t mv_end = mv_start + 1;
                    while (mv_end < meta_str.size() &&
                           !(meta_str[mv_end] == '"' && meta_str[mv_end - 1] != '\\'))
                        ++mv_end;

                    std::string mval = meta_str.substr(mv_start + 1, mv_end - mv_start - 1);
                    fx.metadata[unescape_json_string_ext(mkey)] = unescape_json_string_ext(mval);

                    mpos = meta_str.find(',', mv_end);
                    if (mpos == std::string::npos) break;
                    ++mpos;
                }
            }
        }

        if (!fx.type.empty()) {
            preset.effects.push_back(fx);
        }

        search_pos = obj_end;
    }

    // Parse midi_mappings array
    size_t midi_pos = json.find("\"midi_mappings\"");
    if (midi_pos != std::string::npos) {
        size_t m_arr_start = json.find('[', midi_pos);
        if (m_arr_start != std::string::npos) {
            size_t m_search = m_arr_start;
            while (true) {
                size_t m_obj_start = json.find('{', m_search + 1);
                if (m_obj_start == std::string::npos) break;

                int depth = 0;
                size_t m_obj_end = m_obj_start;
                for (size_t i = m_obj_start; i < json.size(); ++i) {
                    if (json[i] == '{') ++depth;
                    if (json[i] == '}') {
                        --depth;
                        if (depth == 0) { m_obj_end = i; break; }
                    }
                }
                if (m_obj_end <= m_obj_start) break;

                std::string m_obj = json.substr(m_obj_start, m_obj_end - m_obj_start + 1);

                MidiMapping m;
                m.cc_number = extract_int_value(m_obj, "cc", 0);
                m.midi_channel = extract_int_value(m_obj, "channel", -1);
                m.target_type = static_cast<MidiTargetType>(extract_int_value(m_obj, "target", 0));
                m.mode = static_cast<MidiMappingMode>(extract_int_value(m_obj, "mode", 0));
                m.effect_name = unescape_json_string_ext(extract_string_value(m_obj, "effect"));
                m.param_name = unescape_json_string_ext(extract_string_value(m_obj, "param"));
                preset.midi_mappings.push_back(m);

                m_search = m_obj_end;
            }
        }
    }

    return true;
}


}
