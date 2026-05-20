#include "test_framework.h"
#include "preset_json.h"
#include "preset_manager.h"
#include <string>
#include <cmath>

using namespace Amplitron;

// Test 1: Serialised JSON is non-empty for a valid preset
TEST(clipboard_preset_serialise_returns_nonempty_string) {
    PresetData pd;
    pd.name = "Test Preset";
    pd.description = "Test description";
    pd.input_gain = 0.5f;
    pd.output_gain = 0.5f;
    std::string json = to_json_ext(pd);
    ASSERT_FALSE(json.empty());
}

// Test 2: Serialised JSON is valid JSON (contains { and })
TEST(clipboard_preset_serialise_returns_valid_json) {
    PresetData pd;
    pd.name = "Test Preset";
    std::string json = to_json_ext(pd);
    ASSERT_NE(json.find('{'), std::string::npos);
    ASSERT_NE(json.find('}'), std::string::npos);
}

// Test 3: Round-trip — serialise then deserialise gives same preset
TEST(clipboard_preset_roundtrip_restores_same_preset) {
    PresetData pd;
    pd.name = "Test Preset";
    pd.description = "Test description";
    pd.input_gain = 0.5f;
    pd.output_gain = 0.5f;
    
    std::string json = to_json_ext(pd);

    PresetData pd2;
    bool loaded = from_json_ext(json, pd2);
    ASSERT_TRUE(loaded);
    
    ASSERT_EQ(pd.name, pd2.name);
    ASSERT_EQ(pd.description, pd2.description);
    ASSERT_NEAR(pd.input_gain, pd2.input_gain, 0.001f);
    ASSERT_NEAR(pd.output_gain, pd2.output_gain, 0.001f);
}

// Test 4: Empty/invalid JSON does not crash
TEST(clipboard_preset_load_invalid_json_returns_false) {
    PresetData pd;
    bool loaded = from_json_ext("not valid json {{{{", pd);
    ASSERT_FALSE(loaded);
}
