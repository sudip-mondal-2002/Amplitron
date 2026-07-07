#include <algorithm>
#include <string>
#include <vector>

#include "audio/audio_engine.h"
#include "audio/effects/delay.h"
#include "audio/effects/equalizer.h"
#include "audio/effects/noise_gate.h"
#include "audio/effects/overdrive.h"
#include "audio/effects/reverb.h"
#include "gui/command.h"
#include "gui/command_history.h"
#include "gui/gui_manager.h"
#include "test_framework.h"

using namespace Amplitron;

// ============================================================
// Helpers
// ============================================================

static AudioEngine& shared_engine() {
    static AudioEngine engine;
    return engine;
}

static void clear_engine(AudioEngine& engine) {
    while (!engine.effects().empty())
        engine.remove_effect(static_cast<int>(engine.effects().size()) - 1);
}

// ============================================================
// Construction / destruction  (covers GuiManager ctor/dtor)
// ============================================================

TEST(gui_manager_constructs) {
    AudioEngine engine;
    GuiManager manager(engine);
    ASSERT_TRUE(true);
}

TEST(gui_manager_destructor_safe_without_init) {
    AudioEngine engine;
    { GuiManager manager(engine); }
    ASSERT_TRUE(true);
}

TEST(gui_manager_shutdown_without_init) {
    AudioEngine engine;
    GuiManager manager(engine);
    manager.shutdown();
    ASSERT_TRUE(true);
}

TEST(gui_manager_multiple_shutdowns_safe) {
    AudioEngine engine;
    GuiManager manager(engine);
    manager.shutdown();
    manager.shutdown();
    ASSERT_TRUE(true);
}

TEST(gui_manager_multiple_instances_safe) {
    for (int i = 0; i < 5; ++i) {
        AudioEngine engine;
        GuiManager manager(engine);
        manager.shutdown();
    }
    ASSERT_TRUE(true);
}

// ============================================================
// initialize() — headless guard
// SDL_Init will fail on CI / headless machines.  Either outcome
// is valid; what must never happen is a crash or hang.
// ============================================================

TEST(gui_manager_initialize_and_shutdown) {
    AudioEngine engine;
    GuiManager manager(engine);
    bool ok = manager.initialize(800, 600);
    if (ok) manager.shutdown();
    ASSERT_TRUE(true);
}

TEST(gui_manager_initialize_returns_bool) {
    AudioEngine engine;
    GuiManager manager(engine);
    bool ok = manager.initialize(1280, 720);
    (void)ok;
    if (ok) manager.shutdown();
    ASSERT_TRUE(true);
}

TEST(gui_manager_various_window_sizes) {
    const int sizes[][2] = {{320, 240}, {640, 480}, {1280, 720}};
    for (auto& s : sizes) {
        AudioEngine engine;
        GuiManager manager(engine);
        bool ok = manager.initialize(s[0], s[1]);
        if (ok) manager.shutdown();
    }
    ASSERT_TRUE(true);
}

// ============================================================
// midi_manager() accessor  (only public accessor in this version)
// ============================================================

TEST(gui_manager_midi_manager_accessible) {
    AudioEngine engine;
    GuiManager manager(engine);
    MidiManager& mm = manager.midi_manager();
    (void)mm;
    ASSERT_TRUE(true);
}

// ============================================================
// CommandHistory — tested directly (no GuiManager wrapper
// needed; command_history_ is private in this local version)
// ============================================================

TEST(command_history_initially_empty) {
    CommandHistory ch;
    ASSERT_FALSE(ch.can_undo());
    ASSERT_FALSE(ch.can_redo());
}

TEST(command_history_execute_enables_undo) {
    AudioEngine& engine = shared_engine();
    clear_engine(engine);

    CommandHistory ch;
    ch.execute(std::make_unique<AddEffectCommand>(engine, std::make_shared<Overdrive>()));
    ASSERT_TRUE(ch.can_undo());
    ASSERT_FALSE(ch.can_redo());

    clear_engine(engine);
}

TEST(command_history_undo_enables_redo) {
    AudioEngine& engine = shared_engine();
    clear_engine(engine);

    CommandHistory ch;
    ch.execute(std::make_unique<AddEffectCommand>(engine, std::make_shared<Delay>()));
    ch.undo();
    ASSERT_FALSE(ch.can_undo());
    ASSERT_TRUE(ch.can_redo());

    clear_engine(engine);
}

TEST(command_history_redo_restores_undo) {
    AudioEngine& engine = shared_engine();
    clear_engine(engine);

    CommandHistory ch;
    ch.execute(std::make_unique<AddEffectCommand>(engine, std::make_shared<Reverb>()));
    ch.undo();
    ch.redo();
    ASSERT_TRUE(ch.can_undo());
    ASSERT_FALSE(ch.can_redo());

    clear_engine(engine);
}

TEST(command_history_multiple_commands_undo_stack) {
    AudioEngine& engine = shared_engine();
    clear_engine(engine);

    CommandHistory ch;
    ch.execute(std::make_unique<AddEffectCommand>(engine, std::make_shared<NoiseGate>()));
    ch.execute(std::make_unique<AddEffectCommand>(engine, std::make_shared<Equalizer>()));
    ch.execute(std::make_unique<AddEffectCommand>(engine, std::make_shared<Reverb>()));

    ASSERT_EQ(static_cast<int>(engine.effects().size()), 3);

    ch.undo();
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 2);

    ch.undo();
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 1);

    ch.undo();
    ASSERT_EQ(static_cast<int>(engine.effects().size()), 0);
    ASSERT_FALSE(ch.can_undo());

    clear_engine(engine);
}

// ============================================================
// Version parsing logic — mirrors the local lambda and
// comparison loop inside GuiManager::check_for_updates()
// in gui_manager_update.cpp exactly.  Any change to that
// function's parsing logic must be reflected here.
// ============================================================

// Exact copy of the lambda from gui_manager_update.cpp
static std::vector<int> parse_version(const std::string& v) {
    std::vector<int> parts;
    std::string s = v;
    if (!s.empty() && s[0] == 'v') s = s.substr(1);
    size_t pos = 0;
    while (pos < s.size()) {
        size_t dot = s.find('.', pos);
        if (dot == std::string::npos) dot = s.size();
        try {
            parts.push_back(std::stoi(s.substr(pos, dot - pos)));
        } catch (...) {
            parts.push_back(0);
        }
        pos = dot + 1;
    }
    return parts;
}

// Exact copy of the is_newer block from gui_manager_update.cpp
static bool is_version_newer(const std::string& latest, const std::string& current) {
    if (latest.empty()) return false;
    auto latest_parts = parse_version(latest);
    auto current_parts = parse_version(current);
    bool is_newer = false;
    size_t max_len = std::max(latest_parts.size(), current_parts.size());
    for (size_t i = 0; i < max_len; ++i) {
        int lv = (i < latest_parts.size()) ? latest_parts[i] : 0;
        int cv = (i < current_parts.size()) ? current_parts[i] : 0;
        if (lv > cv) {
            is_newer = true;
            break;
        }
        if (lv < cv) {
            break;
        }
    }
    return is_newer;
}

TEST(version_parse_strips_leading_v) {
    auto p = parse_version("v1.2.3");
    ASSERT_EQ(static_cast<int>(p.size()), 3);
    ASSERT_EQ(p[0], 1);
    ASSERT_EQ(p[1], 2);
    ASSERT_EQ(p[2], 3);
}

TEST(version_parse_no_leading_v) {
    auto p = parse_version("0.1.42");
    ASSERT_EQ(static_cast<int>(p.size()), 3);
    ASSERT_EQ(p[0], 0);
    ASSERT_EQ(p[1], 1);
    ASSERT_EQ(p[2], 42);
}

TEST(version_parse_single_component) {
    auto p = parse_version("v5");
    ASSERT_EQ(static_cast<int>(p.size()), 1);
    ASSERT_EQ(p[0], 5);
}

TEST(version_parse_two_components) {
    auto p = parse_version("v2.10");
    ASSERT_EQ(static_cast<int>(p.size()), 2);
    ASSERT_EQ(p[0], 2);
    ASSERT_EQ(p[1], 10);
}

TEST(version_parse_empty_string) {
    auto p = parse_version("");
    ASSERT_EQ(static_cast<int>(p.size()), 0);
}

TEST(version_parse_only_v) {
    auto p = parse_version("v");
    ASSERT_EQ(static_cast<int>(p.size()), 0);
}

TEST(version_parse_non_numeric_component_yields_zero) {
    auto p = parse_version("v1.alpha.3");
    ASSERT_EQ(static_cast<int>(p.size()), 3);
    ASSERT_EQ(p[0], 1);
    ASSERT_EQ(p[1], 0);
    ASSERT_EQ(p[2], 3);
}

TEST(version_is_newer_major_greater) { ASSERT_TRUE(is_version_newer("v2.0.0", "v1.99.99")); }

TEST(version_is_newer_minor_greater) { ASSERT_TRUE(is_version_newer("v1.2.0", "v1.1.99")); }

TEST(version_is_newer_patch_greater) { ASSERT_TRUE(is_version_newer("v1.1.2", "v1.1.1")); }

TEST(version_is_not_newer_when_equal) { ASSERT_FALSE(is_version_newer("v1.2.3", "v1.2.3")); }

TEST(version_is_not_newer_when_older) {
    ASSERT_FALSE(is_version_newer("v1.0.0", "v1.0.1"));
    ASSERT_FALSE(is_version_newer("v0.9.9", "v1.0.0"));
}

TEST(version_is_not_newer_when_empty) { ASSERT_FALSE(is_version_newer("", "v1.0.0")); }

TEST(version_newer_different_component_counts) {
    ASSERT_TRUE(is_version_newer("v1.0.1", "v1.0"));
    ASSERT_FALSE(is_version_newer("v1.0", "v1.0.1"));
}

TEST(version_newer_extra_fourth_component) {
    ASSERT_FALSE(is_version_newer("v1.0.0", "v1.0.0.1"));
    ASSERT_TRUE(is_version_newer("v1.0.0.1", "v1.0.0"));
}

// ============================================================
// JSON extraction logic — mirrors the find/substr block in
// GuiManager::check_for_updates() in gui_manager_update.cpp
// ============================================================

static std::string extract_tag_name(const std::string& response) {
    const std::string search_str = "\"tag_name\": \"";
    size_t pos = response.find(search_str);
    if (pos == std::string::npos) return "";
    pos += search_str.length();
    size_t end_pos = response.find("\"", pos);
    if (end_pos == std::string::npos) return "";
    return response.substr(pos, end_pos - pos);
}

static std::string extract_html_url(const std::string& response) {
    const std::string url_search_str = "\"html_url\": \"";
    size_t url_pos = response.find(url_search_str);
    if (url_pos == std::string::npos) return "";
    url_pos += url_search_str.length();
    size_t url_end_pos = response.find("\"", url_pos);
    if (url_end_pos == std::string::npos) return "";
    return response.substr(url_pos, url_end_pos - url_pos);
}

TEST(update_extract_tag_name_found) {
    std::string json =
        R"([{"tag_name": "v1.2.3","html_url": "https://example.com/releases/v1.2.3"}])";
    ASSERT_EQ(extract_tag_name(json), std::string("v1.2.3"));
}

TEST(update_extract_tag_name_not_found) {
    ASSERT_EQ(extract_tag_name(R"([{"name": "release"}])"), std::string(""));
}

TEST(update_extract_html_url_found) {
    std::string json =
        R"([{"tag_name": "v1.0.0","html_url": "https://github.com/releases/v1.0.0"}])";
    ASSERT_EQ(extract_html_url(json), std::string("https://github.com/releases/v1.0.0"));
}

TEST(update_extract_html_url_not_found) {
    ASSERT_EQ(extract_html_url(R"([{"tag_name": "v1.0.0"}])"), std::string(""));
}

TEST(update_extract_tag_name_empty_response) { ASSERT_EQ(extract_tag_name(""), std::string("")); }

TEST(update_extract_html_url_empty_response) { ASSERT_EQ(extract_html_url(""), std::string("")); }

TEST(update_extract_tag_name_unterminated_value) {
    ASSERT_EQ(extract_tag_name(R"({"tag_name": "v1.0.0)"), std::string(""));
}

TEST(update_full_flow_newer_release_detected) {
    std::string api_response =
        R"([{"tag_name": "v0.1.999","html_url": "https://github.com/example/releases/tag/v0.1.999"}])";
    std::string tag = extract_tag_name(api_response);
    std::string url = extract_html_url(api_response);
    ASSERT_EQ(tag, std::string("v0.1.999"));
    ASSERT_FALSE(url.empty());
    ASSERT_TRUE(is_version_newer(tag, "v0.1.1"));
}

TEST(update_full_flow_same_version_not_flagged) {
    std::string api_response =
        R"([{"tag_name": "v0.1.1","html_url": "https://github.com/example/releases/tag/v0.1.1"}])";
    ASSERT_FALSE(is_version_newer(extract_tag_name(api_response), "v0.1.1"));
}

TEST(update_full_flow_older_release_not_flagged) {
    std::string api_response =
        R"([{"tag_name": "v0.1.0","html_url": "https://github.com/example/releases/tag/v0.1.0"}])";
    ASSERT_FALSE(is_version_newer(extract_tag_name(api_response), "v0.1.1"));
}
