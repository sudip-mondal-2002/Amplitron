#include "test_framework.h"
#include "audio/engine/audio_engine.h"
#include "audio/effects/overdrive.h"
#include "preset_manager.h"
#include <cstring>
#include <memory>
#include <vector>

using namespace Amplitron;

// =============================================================================
// Headless Mirror of GuiPresets State Capture (Copied from gui_presets.cpp)
// =============================================================================

static PresetData capture_current_state(AudioEngine& engine) {
    PresetData preset;
    preset.input_gain = engine.get_input_gain();
    preset.output_gain = engine.get_output_gain();

    for (auto& fx : engine.effects()) {
        PresetData::EffectData fd;
        fd.type = fx->name();
        fd.enabled = fx->is_enabled();
        fd.mix = fx->get_mix();
        for (auto& p : fx->params()) {
            fd.params.push_back({p.name, p.value});
        }
        preset.effects.push_back(std::move(fd));
    }
    return preset;
}

static bool equal_effect_data(const PresetData::EffectData& a, const PresetData::EffectData& b) {
    if (a.type != b.type || a.enabled != b.enabled || a.mix != b.mix) return false;
    if (a.params.size() != b.params.size()) return false;
    for (size_t i = 0; i < a.params.size(); ++i) {
        if (a.params[i] != b.params[i]) return false;
    }
    return true;
}

static bool equal_preset_data(const PresetData& a, const PresetData& b) {
    if (a.input_gain != b.input_gain || a.output_gain != b.output_gain) return false;
    if (a.effects.size() != b.effects.size()) return false;
    for (size_t i = 0; i < a.effects.size(); ++i) {
        if (!equal_effect_data(a.effects[i], b.effects[i])) return false;
    }
    return true;
}

// =============================================================================
// Isolated State Tracking Assertions
// =============================================================================

TEST(presets_dirty_flag_input_gain) {
    AudioEngine engine;
    engine.initialize();

    PresetData saved_state = capture_current_state(engine);
    ASSERT_TRUE(equal_preset_data(saved_state, capture_current_state(engine)));

    engine.set_input_gain(engine.get_input_gain() + 0.123f);
    
    bool is_dirty = !equal_preset_data(saved_state, capture_current_state(engine));
    ASSERT_TRUE(is_dirty);

    saved_state = capture_current_state(engine);
    is_dirty = !equal_preset_data(saved_state, capture_current_state(engine));
    ASSERT_FALSE(is_dirty);

    engine.shutdown();
}

TEST(presets_dirty_on_add_effect) {
    AudioEngine engine;
    engine.initialize();

    PresetData saved_state = capture_current_state(engine);
    ASSERT_TRUE(equal_preset_data(saved_state, capture_current_state(engine)));

    auto od = std::make_shared<Overdrive>();
    engine.add_effect(od);
    
    bool is_dirty = !equal_preset_data(saved_state, capture_current_state(engine));
    ASSERT_TRUE(is_dirty);

    saved_state = capture_current_state(engine);
    is_dirty = !equal_preset_data(saved_state, capture_current_state(engine));
    ASSERT_FALSE(is_dirty);

    engine.shutdown();
}

TEST(preset_migration_load_v1) {
    std::string legacy_v1_payload = "{\n  \"name\": \"Old School Drive\"\n}";

    // Trigger the migration pipeline manually using standard strings
    std::string upgraded_payload = PresetManager::apply_migrations(legacy_v1_payload);

    // Verify the version string and fallback properties were safely injected
    ASSERT_TRUE(upgraded_payload.find("\"version\": 2") != std::string::npos);
    ASSERT_TRUE(upgraded_payload.find("\"input_gain\": 0.7") != std::string::npos);
}

// SATISFY: "Unit tests cover: load v2 preset"
TEST(preset_migration_load_v2) {
    std::string modern_v2_payload = "{\n  \"version\": 2,\n  \"name\": \"Modern Tone\"\n}";

    std::string processed_payload = PresetManager::apply_migrations(modern_v2_payload);

    // It shouldn't touch or double-modify a file that is already at version 2
    ASSERT_TRUE(processed_payload == modern_v2_payload);
}