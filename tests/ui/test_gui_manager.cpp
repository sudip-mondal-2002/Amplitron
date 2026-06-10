#include <fcntl.h>
#include <sys/types.h>

#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif

// Pre-include standard library headers to protect them from macro poisoning
#include <imgui.h>

#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <codecvt>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iomanip>
#include <ios>
#include <iosfwd>
#include <iostream>
#include <istream>
#include <iterator>
#include <limits>
#include <list>
#include <locale>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <ostream>
#include <queue>
#include <random>
#include <ratio>
#include <set>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <system_error>
#include <thread>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <valarray>
#include <vector>

#define private public
#define protected public
#include "amplitron_session.h"
#include "audio/recorder/recorder.h"
#include "gui/gui_manager.h"
#include "midi/midi_manager.h"
#include "test_fixtures.h"
#include "test_framework.h"

using namespace Amplitron;

TEST(amplitron_session_throws_on_null_arguments) {
    ASSERT_THROW(AmplitronSession(nullptr, std::make_unique<MidiManager>(),
                                  std::make_unique<PresetManagerService>()),
                 std::invalid_argument);
    ASSERT_THROW(AmplitronSession(std::make_unique<AudioEngine>(), nullptr,
                                  std::make_unique<PresetManagerService>()),
                 std::invalid_argument);
    ASSERT_THROW(
        AmplitronSession(std::make_unique<AudioEngine>(), std::make_unique<MidiManager>(), nullptr),
        std::invalid_argument);
}

TEST(gui_manager_basic_lifecycle) {
    AmplitronSession session;
    auto& engine = session.concrete_engine();
    engine.initialize();

    // Construct GuiManager
    GuiManager gui(session);
    gui.initialize();

    // Audio engine reference is correctly stored
    ASSERT_EQ(&gui.audio_engine(), &engine);

    // MIDI manager is reachable through GuiManager
    auto& mm = gui.midi_manager();
    (void)mm;

    // Explicit shutdown before engine teardown
    gui.shutdown();
    engine.shutdown();
}

TEST(gui_manager_double_shutdown_is_safe) {
    AmplitronSession session;
    auto& engine = session.concrete_engine();
    engine.initialize();

    GuiManager gui(session);
    gui.initialize();

    // shutdown() must guard against being called twice (initialized_ flag)
    gui.shutdown();
    gui.shutdown();  // Must not crash

    engine.shutdown();
}

TEST(gui_manager_midi_manager_association) {
    AmplitronSession session;
    auto& engine = session.concrete_engine();
    engine.initialize();

    GuiManager gui(session);

    // midi_manager() must return a stable reference (same address each call)
    ASSERT_EQ(&gui.midi_manager(), &gui.midi_manager());

    gui.shutdown();
    engine.shutdown();
}

TEST(gui_manager_private_rendering_methods) {
    ScopedImGuiContext imgui;
    AmplitronSession session;
    auto& engine = session.concrete_engine();
    engine.initialize();

    GuiManager gui(session);

    // 1. Mute/unmute
    engine.set_running_for_testing(
        true);  // Headless-safe: bypass physical soundcard start requirement
    gui.toggle_audio_mute_state();
    ASSERT_TRUE(gui.audio_muted_);

    engine.set_running_for_testing(
        false);  // Headless-safe: manually update engine state since Pa_Stream is nullptr
    gui.toggle_audio_mute_state();
    ASSERT_FALSE(gui.audio_muted_);

    // 2. Render master controls
    gui.render_master_controls();

    // 3. Render menu bar (without update)
    gui.render_menu_bar();

    // 4. UpdateChecker tests are handled elsewhere, render menu normally
    gui.render_menu_bar();

    gui.shutdown();
    engine.shutdown();
}

TEST(gui_manager_logical_builders) {
    ScopedImGuiContext imgui;
    AmplitronSession session;
    auto& engine = session.concrete_engine();
    engine.initialize();

    GuiManager gui(session);

    // 1. build_recording_props under various Recorder states
    {
        auto p1 = gui.build_recording_props();
        ASSERT_FALSE(p1.is_recording);
        ASSERT_FALSE(p1.is_paused);
        ASSERT_FALSE(p1.has_unsaved);

        // Simulate start
        p1.on_start();
        auto p2 = gui.build_recording_props();
        ASSERT_TRUE(p2.is_recording);

        // Pause and stop
        p2.on_pause();
        auto p3 = gui.build_recording_props();
        ASSERT_TRUE(p3.is_paused);

        p3.on_resume();
        p3.on_stop();
        p3.on_discard();
    }

    // 2. build_tuner_props
    {
        auto p = gui.build_tuner_props();
        ASSERT_FALSE(p.has_signal);
        p.on_mute_changed(true);
        p.on_a4_ref_changed(442.0f);

        auto p2 = gui.build_tuner_props();
        ASSERT_TRUE(p2.mute_on);
        ASSERT_NEAR(p2.a4_ref, 442.0f, 0.01f);
    }

    // 3. build_settings_props
    {
        auto p = gui.build_settings_props();
        ASSERT_EQ(p.buffer_size, engine.get_buffer_size());
        p.on_buffer_size_changed(256);
        p.on_sample_rate_changed(48000);
        p.on_auto_buf_changed(true);
        p.on_clear_error();
        p.on_input_device_changed(0);
        p.on_output_device_changed(0);

        auto p2 = gui.build_settings_props();
        ASSERT_EQ(p2.buffer_size, 256);
        ASSERT_EQ(p2.sample_rate, 48000);
        ASSERT_TRUE(p2.auto_buf);
    }

    // 4. build_analyzer_props
    {
        auto p = gui.build_analyzer_props();
        ASSERT_TRUE(p.spectrum.smoothed_input_db ==
                    gui.metrics_service_.spectrum_analyzer().smoothed_input_db());
        p.on_set_analyzer_enabled(true);
    }

    // 5. build_snapshots_props
    {
        auto p = gui.build_snapshots_props();
        ASSERT_FALSE(p.slots[0].is_filled);

        p.on_save_slot(0);
        auto p2 = gui.build_snapshots_props();
        ASSERT_TRUE(p2.slots[0].is_filled);
        ASSERT_TRUE(p2.slots[0].is_active);

        p2.on_recall_slot(0);
        p2.on_clear_slot(0);

        auto p3 = gui.build_snapshots_props();
        ASSERT_FALSE(p3.slots[0].is_filled);
    }

    gui.shutdown();
    engine.shutdown();
}

// =============================================================================
// Headless Mocking Infrastructure for 100% GUI Coverage
// =============================================================================

namespace mock_gui {

struct MockDrawList {
    void AddRectFilled(const ImVec2&, const ImVec2&, unsigned int, float = 0.0f, int = 0) {}
    void AddLine(const ImVec2&, const ImVec2&, unsigned int, float = 1.0f) {}
    void AddRect(const ImVec2&, const ImVec2&, unsigned int, float = 0.0f, int = 0, float = 1.0f) {}
};

struct MockImGui {
    static inline bool begin_main_menu_bar_ret = true;
    static inline bool begin_menu_ret = true;
    static inline bool menu_item_ret = true;
    static inline bool button_ret = true;
    static inline bool small_button_ret = true;
    static inline bool is_item_hovered_ret = true;
    static inline bool is_item_clicked_ret = true;
    static inline bool begin_popup_modal_ret = true;
    static inline bool begin_child_ret = true;

    static inline std::string target_menu_item = "";
    static inline std::string target_popup = "";
    static inline std::string target_button = "";
    static inline std::string target_small_button = "";
    static inline int target_key = 0;

    static void reset() {
        begin_main_menu_bar_ret = true;
        begin_menu_ret = true;
        menu_item_ret = false;
        button_ret = false;
        small_button_ret = false;
        is_item_hovered_ret = false;
        is_item_clicked_ret = false;
        begin_popup_modal_ret = false;
        begin_child_ret = true;
        target_menu_item = "";
        target_popup = "";
        target_button = "";
        target_small_button = "";
        target_key = 0;
    }

    static bool BeginMainMenuBar() { return begin_main_menu_bar_ret; }
    static void EndMainMenuBar() {}
    static bool BeginMenu(const char*, bool = true) { return begin_menu_ret; }
    static void EndMenu() {}
    static bool MenuItem(const char* label, const char* = nullptr, bool = false, bool = true) {
        if (target_menu_item == "ALL") return true;
        if (target_menu_item == label) return true;
        return menu_item_ret;
    }
    static void OpenPopup(const char*, int = 0) {}
    static bool BeginPopupModal(const char* label, bool* = nullptr, int = 0) {
        if (target_popup == "ALL") return true;
        if (target_popup == label) return true;
        return begin_popup_modal_ret;
    }
    static void EndPopup() {}
    static void Text(const char*, ...) {}
    static void Separator() {}
    static bool Button(const char* label, const ImVec2& = ImVec2(0, 0)) {
        if (target_button == "ALL") return true;
        if (target_button == label) return true;
        return button_ret;
    }
    static void SameLine(float = 0.0f, float = -1.0f) {}
    static void SetClipboardText(const char*) {}
    static float GetWindowWidth() { return 1000.0f; }
    static ImVec2 CalcTextSize(const char*, const char* = nullptr, bool = false, float = -1.0f) {
        return ImVec2(10.0f, 10.0f);
    }
    static void TextColored(const ImVec4&, const char*, ...) {}
    static bool IsItemHovered(int = 0) { return is_item_hovered_ret; }
    static void SetTooltip(const char*, ...) {}
    static void SetMouseCursor(int) {}
    static bool IsItemClicked(int = 0) { return is_item_clicked_ret; }
    static double GetTime() { return 1.0; }
    static void PushStyleColor(int, const ImVec4&) {}
    static void PushStyleColor(int, unsigned int) {}
    static void PopStyleColor(int = 1) {}
    static bool BeginChild(const char*, const ImVec2& = ImVec2(0, 0), bool = false, int = 0) {
        return begin_child_ret;
    }
    static void EndChild() {}
    static bool SmallButton(const char* label) {
        if (target_small_button == "ALL") return true;
        if (target_small_button == label) return true;
        return small_button_ret;
    }
    static void CloseCurrentPopup() {}
    static ImGuiStyle& GetStyle() {
        static ImGuiStyle style;
        return style;
    }

    static ImGuiIO& GetIO() {
        static ImGuiIO io;
        return io;
    }
    static bool IsKeyPressed(int key, bool = true) {
        if (target_key == key) return true;
        return false;
    }
    static bool IsAnyItemActive() { return false; }
    static void Begin(const char*, bool* = nullptr, int = 0) {}
    static void End() {}
    static void SetNextWindowPos(const ImVec2&, int = 0, const ImVec2& = ImVec2(0, 0)) {}
    static void SetNextWindowSize(const ImVec2&, int = 0) {}
    static void SetNextWindowBgAlpha(float) {}
    static void PushStyleVar(int, float) {}
    static void PushStyleVar(int, const ImVec2&) {}
    static void PopStyleVar(int = 1) {}
    static MockDrawList* GetWindowDrawList() {
        static MockDrawList dl;
        return &dl;
    }
    static ImVec2 GetCursorScreenPos() { return ImVec2(0, 0); }
    static float GetColumnWidth(int = -1) { return 100.0f; }
    static void NextColumn() {}
    static void Columns(int = 1, const char* = nullptr, bool = false) {}
    static void Dummy(const ImVec2&) {}
    static void AlignTextToFramePadding() {}
    static inline bool slider_float_ret = false;
    static bool SliderFloat(const char*, float*, float, float, const char* = nullptr,
                            float = 1.0f) {
        return slider_float_ret;
    }
    static inline bool slider_int_ret = false;
    static bool SliderInt(const char*, int*, int, int, const char* = nullptr, int = 0) {
        return slider_int_ret;
    }
};

}  // namespace mock_gui

namespace Amplitron {

class MockGuiManager : public GuiManager {
   public:
    MockGuiManager(AmplitronSession& s) : GuiManager(s) {}

    void render_menu_bar();
    void render_master_controls();
    RecordingProps build_recording_props();
    TunerProps build_tuner_props();
    SettingsProps build_settings_props();
    AnalyzerProps build_analyzer_props();
    SnapshotsProps build_snapshots_props();
    void toggle_audio_mute_state();
    void set_show_tuner(bool show);
    void recallSnapshotFromSlot(int slot);
    bool run_frame();
};

}  // namespace Amplitron

namespace Amplitron {
extern bool g_mock_window_context_initialize_fail;
extern bool g_mock_window_context_poll_events_fail;
extern std::string g_mock_save_dialog_result;
extern std::string g_mock_open_dialog_result;
extern std::string g_mock_folder_dialog_result;
}  // namespace Amplitron

#ifndef _WIN32
static int mock_fork_val = -1;
inline pid_t mock_fork() { return mock_fork_val; }
inline int mock_pipe(int*) { return 0; }
inline int mock_dup2(int, int) { return 0; }
inline int mock_open(const char*, int, ...) { return 1; }
inline int mock_close(int) { return 0; }
inline int mock_execl(const char*, const char*, ...) { return 0; }
inline void mock_exit(int) {}
inline pid_t mock_waitpid(pid_t, int*, int) { return 0; }
#endif

static bool mock_serialise_current_preset_to_json_empty = false;

struct MockPresetsWrapper {
    Amplitron::GuiPresets& real_presets;
    std::string serialise_current_preset_to_json() const {
        if (mock_serialise_current_preset_to_json_empty) return "";
        return real_presets.serialise_current_preset_to_json();
    }
    std::string current_preset_name() const { return real_presets.current_preset_name(); }
    bool is_dirty() const { return real_presets.is_dirty(); }
    void refresh_presets(bool force) { real_presets.refresh_presets(force); }
    void begin_new_preset() { real_presets.begin_new_preset(); }
    void begin_save_preset() { real_presets.begin_save_preset(); }
    void ensure_factory_presets() { real_presets.ensure_factory_presets(); }
    int selected_preset_index() const { return real_presets.selected_preset_index(); }
    int preset_count() const { return real_presets.preset_count(); }
    bool delete_preset_by_index(int index) { return real_presets.delete_preset_by_index(index); }
    void render_save_popup(bool& show) { real_presets.render_save_popup(show); }
    void render_load_popup(bool& show) { real_presets.render_load_popup(show); }
};

struct MockTunerWrapper {
    Amplitron::GuiTuner& real_tuner;
    void render(bool& show) {
        real_tuner.render(show);
        show = false;
    }
    void set_props(const Amplitron::TunerProps& p) { real_tuner.set_props(p); }
};

#define GuiManager MockGuiManager
#define ImGui mock_gui::MockImGui
#define ImDrawList mock_gui::MockDrawList
#ifndef _WIN32
#define fork mock_fork
#define pipe mock_pipe
#define dup2 mock_dup2
#define open mock_open
#define close mock_close
#define execl mock_execl
#define _exit mock_exit
#define waitpid mock_waitpid
#endif
#define gui_presets_ \
    MockPresetsWrapper { gui_presets_ }
#define gui_tuner_ \
    MockTunerWrapper { gui_tuner_ }
#include "gui/gui_manager_frame.cpp"
#include "gui/gui_manager_menu.cpp"
#undef GuiManager
#undef ImGui
#undef ImDrawList
#ifndef _WIN32
#undef fork
#undef pipe
#undef dup2
#undef open
#undef close
#undef execl
#undef _exit
#undef waitpid
#endif
#undef gui_presets_
#undef gui_tuner_

// UpdateChecker mocks
static std::string mock_popen_result;
static size_t mock_popen_pos = 0;
static bool mock_popen_fail = false;

inline FILE* mock_popen(const char*, const char*) {
    mock_popen_pos = 0;
    if (mock_popen_fail) return nullptr;
    return reinterpret_cast<FILE*>(1);
}

inline int mock_pclose(FILE*) { return 0; }

inline char* mock_fgets(char* str, int num, FILE*) {
    if (mock_popen_pos >= mock_popen_result.size()) {
        return nullptr;
    }
    size_t len = 0;
    while (mock_popen_pos < mock_popen_result.size() && len < static_cast<size_t>(num - 1)) {
        char c = mock_popen_result[mock_popen_pos++];
        str[len++] = c;
        if (c == '\n') break;
    }
    str[len] = '\0';
    return str;
}

namespace Amplitron {

class MockUpdateChecker : public UpdateChecker {
   public:
    MockUpdateChecker();
    ~MockUpdateChecker();
    void start_check();
    void shutdown();
    bool has_new_release() const;
    std::string new_release_version() const;
    std::string new_release_url() const;
    void check_for_updates();
};

}  // namespace Amplitron

#define UpdateChecker MockUpdateChecker
#ifdef popen
#undef popen
#endif
#define popen mock_popen
#ifdef pclose
#undef pclose
#endif
#define pclose mock_pclose
#ifdef fgets
#undef fgets
#endif
#define fgets mock_fgets
#include "gui/update_checker.cpp"
#undef UpdateChecker
#undef popen
#undef pclose
#undef fgets

// =============================================================================
// Tests
// =============================================================================

TEST(gui_manager_menu_bar_comprehensive_coverage) {
    Amplitron::AmplitronSession session;
    Amplitron::MockGuiManager gui(session);

    mock_gui::MockImGui::reset();

    // 1. Render menu bar with no clicks
    gui.render_menu_bar();

    // 2. Trigger "New Preset..."
    mock_gui::MockImGui::target_menu_item = "New Preset...";
    gui.render_menu_bar();

    // 3. Trigger "Save Preset..."
    mock_gui::MockImGui::target_menu_item = "Save Preset...";
    gui.render_menu_bar();

    // 4. Trigger "Load Preset..."
    mock_gui::MockImGui::target_menu_item = "Load Preset...";
    gui.render_menu_bar();

    // 5. Trigger "Delete Selected Preset" -> triggers OpenPopup
    mock_gui::MockImGui::target_menu_item = "Delete Selected Preset";
    gui.render_menu_bar();

    // Render "Confirm Delete Preset" popup
    mock_gui::MockImGui::target_popup = "Confirm Delete Preset";
    mock_gui::MockImGui::target_button = "Delete";
    gui.render_menu_bar();

    // Cancel deletion
    mock_gui::MockImGui::target_button = "Cancel";
    gui.render_menu_bar();

    // 6. Trigger "Copy Preset to Clipboard"
    mock_gui::MockImGui::target_menu_item = "Copy Preset to Clipboard";
    gui.render_menu_bar();

    // 7. Trigger "Change Presets Directory..."
    mock_gui::MockImGui::target_menu_item = "Change Presets Directory...";
    gui.render_menu_bar();

    // 8. Trigger "Reset to Default Presets Directory"
    mock_gui::MockImGui::target_menu_item = "Reset to Default Presets Directory";
    gui.render_menu_bar();

    // Render "Confirm Reset Presets Dir" popup
    mock_gui::MockImGui::target_popup = "Confirm Reset Presets Dir";
    mock_gui::MockImGui::target_button = "Reset";
    gui.render_menu_bar();

    // Cancel reset
    mock_gui::MockImGui::target_button = "Cancel";
    gui.render_menu_bar();

    // 9. Trigger "Settings"
    mock_gui::MockImGui::target_menu_item = "Settings";
    gui.render_menu_bar();

    // 10. Trigger "Quit"
    mock_gui::MockImGui::target_menu_item = "Quit";
    gui.render_menu_bar();

    // 11. Trigger "Stop Audio" / "Start Audio" / "Restart Audio"
    session.concrete_engine().set_running_for_testing(true);
    mock_gui::MockImGui::target_menu_item = "Stop Audio";
    gui.render_menu_bar();

    session.concrete_engine().set_running_for_testing(false);
    mock_gui::MockImGui::target_menu_item = "Start Audio";
    gui.render_menu_bar();

    mock_gui::MockImGui::target_menu_item = "Restart Audio";
    gui.render_menu_bar();

    // 12. Trigger "Open Tuner" / "MIDI Settings"
    mock_gui::MockImGui::target_menu_item = "Open Tuner";
    gui.render_menu_bar();
    mock_gui::MockImGui::target_menu_item = "MIDI Settings";
    gui.render_menu_bar();

    // 13. Trigger MIDI status interactions
    mock_gui::MockImGui::target_menu_item = "MIDI";
    mock_gui::MockImGui::is_item_hovered_ret = true;
    mock_gui::MockImGui::is_item_clicked_ret = true;
    gui.render_menu_bar();

    // 14. Trigger Audio Error Banner actions
    session.concrete_engine().set_running_for_testing(false);
    mock_gui::MockImGui::target_small_button = "Restart Audio";
    gui.render_menu_bar();

    mock_gui::MockImGui::target_small_button = "Settings";
    gui.render_menu_bar();
}

TEST(gui_manager_frame_comprehensive_coverage) {
    ScopedImGuiContext imgui;
    Amplitron::AmplitronSession session;
    Amplitron::MockGuiManager gui(session);

    mock_gui::MockImGui::reset();

    // 1. Run basic frame
    gui.run_frame();

    // 2. Keyboard shortcuts - Undo
    mock_gui::MockImGui::target_key = ImGuiKey_Z;
    mock_gui::MockImGui::GetIO().KeyCtrl = true;
    mock_gui::MockImGui::GetIO().KeyShift = false;
    gui.run_frame();

    // Keyboard shortcuts - Redo (Ctrl+Shift+Z)
    mock_gui::MockImGui::target_key = ImGuiKey_Z;
    mock_gui::MockImGui::GetIO().KeyCtrl = true;
    mock_gui::MockImGui::GetIO().KeyShift = true;
    gui.run_frame();

    // Keyboard shortcuts - Redo (Ctrl+Y)
    mock_gui::MockImGui::target_key = ImGuiKey_Y;
    mock_gui::MockImGui::GetIO().KeyCtrl = true;
    mock_gui::MockImGui::GetIO().KeyShift = false;
    gui.run_frame();

    // Keyboard shortcuts - Mute (M)
    mock_gui::MockImGui::target_key = ImGuiKey_M;
    mock_gui::MockImGui::GetIO().KeyCtrl = false;
    mock_gui::MockImGui::GetIO().KeySuper = false;
    mock_gui::MockImGui::GetIO().WantTextInput = false;
    gui.run_frame();

    // Keyboard shortcuts - Snapshot Slot (Ctrl+1)
    mock_gui::MockImGui::target_key = ImGuiKey_1;
    mock_gui::MockImGui::GetIO().KeyCtrl = true;
    gui.run_frame();

    // 3. Trigger showing popups
    gui.show_settings_ = true;
    gui.show_save_preset_ = true;
    gui.show_load_preset_ = true;
    gui.show_tuner_ = true;
    gui.show_midi_ = true;
    gui.toast_timer_ = 1.0f;
    gui.run_frame();

    // 4. Trigger Save dialog actions
    gui.gui_recording_.set_state([](RecordingState& s) { s.needs_save = true; });
    gui.run_frame();
}

TEST(update_checker_comprehensive_coverage) {
    Amplitron::MockUpdateChecker checker;

    // Test popen fail
    mock_popen_fail = true;
    checker.check_for_updates();
    ASSERT_FALSE(checker.has_new_release());
    mock_popen_fail = false;

    // Test POP fails
    mock_popen_result = "";
    checker.check_for_updates();
    ASSERT_FALSE(checker.has_new_release());

    // Test version matches
    mock_popen_result = std::string(AMPLITRON_VERSION) + "\n";
    checker.check_for_updates();
    ASSERT_FALSE(checker.has_new_release());

    // Test new release version
    mock_popen_result = "v9.9.9\n";
    checker.check_for_updates();
    ASSERT_TRUE(checker.has_new_release());
    ASSERT_EQ(checker.new_release_version(), "v9.9.9");
    ASSERT_EQ(checker.new_release_url(),
              "https://github.com/amplitron-dsp/Amplitron/releases/tag/v9.9.9");

    // Test shutdown while running
    checker.start_check();
    checker.shutdown();
}

// =============================================================================
// UIComponent 100% Function Coverage
// =============================================================================
struct DummyProps {};
struct DummyState {
    int val = 0;
};
class TestUIComponent : public Amplitron::UIComponent<DummyProps, DummyState> {
   public:
    void render() override {}
    void trigger_update() {
        set_state([](DummyState& s) { s.val = 42; });
    }
};

TEST(ui_component_full_function_coverage) {
    TestUIComponent comp;
    DummyProps props;
    comp.set_props(props);
    comp.render();

    DummyState s;
    s.val = 10;
    comp.set_state(s);
    ASSERT_EQ(comp.get_state().val, 10);

    comp.trigger_update();
    ASSERT_EQ(comp.get_state().val, 42);
}

TEST(gui_manager_additional_coverage_run) {
    // 1. gui_manager.cpp Line 59 (initialize fail)
    {
        Amplitron::AmplitronSession session;
        Amplitron::GuiManager gui(session);
        Amplitron::g_mock_window_context_initialize_fail = true;
        ASSERT_FALSE(gui.initialize());
        Amplitron::g_mock_window_context_initialize_fail = false;
    }

    // 2. gui_manager.cpp Lines 66-67 (default MIDI mappings when load config is empty)
    {
#ifdef _WIN32
        const char* env_name = "APPDATA";
#else
        const char* env_name = "HOME";
#endif
        char* old_val = std::getenv(env_name);
        std::cout << "[DEBUG] OLD " << env_name << ": " << (old_val ? old_val : "NULL")
                  << std::endl;
#ifdef _WIN32
        _putenv_s(env_name, "/tmp/non_existent_dir_amplitron_test");
#else
        setenv(env_name, "/tmp/non_existent_dir_amplitron_test", 1);
#endif
        std::cout << "[DEBUG] NEW " << env_name << ": "
                  << (std::getenv(env_name) ? std::getenv(env_name) : "NULL") << std::endl;
        std::cout << "[DEBUG] CONFIG PATH: " << MidiManager::get_config_path() << std::endl;

        Amplitron::AmplitronSession session;
        Amplitron::GuiManager gui(session);
        std::cout << "[DEBUG] MAPPINGS SIZE BEFORE CLEAR: " << gui.midi_manager().mappings().size()
                  << std::endl;
        gui.midi_manager().clear_mappings();
        std::cout << "[DEBUG] MAPPINGS SIZE AFTER CLEAR: " << gui.midi_manager().mappings().size()
                  << std::endl;
        gui.initialize();
        std::cout << "[DEBUG] MAPPINGS SIZE AFTER INIT: " << gui.midi_manager().mappings().size()
                  << std::endl;

        if (old_val) {
#ifdef _WIN32
            _putenv_s(env_name, old_val);
#else
            setenv(env_name, old_val, 1);
#endif
        } else {
#ifdef _WIN32
            _putenv_s(env_name, "");
#else
            unsetenv(env_name);
#endif
        }
    }

    // 3. gui_manager_frame.cpp Line 186 (window_context_.poll_events() fail)
    {
        ScopedImGuiContext imgui;
        Amplitron::AmplitronSession session;
        Amplitron::MockGuiManager gui(session);
        Amplitron::g_mock_window_context_poll_events_fail = true;
        ASSERT_FALSE(gui.run_frame());
        Amplitron::g_mock_window_context_poll_events_fail = false;
    }

    // 4. gui_manager_frame.cpp Lines 165-166 (set_show_tuner false)
    {
        ScopedImGuiContext imgui;
        Amplitron::AmplitronSession session;
        Amplitron::MockGuiManager gui(session);
        gui.set_show_tuner(false);
    }

    // 5. gui_manager_frame.cpp Lines 272-276 (save recorder callback)
    {
        ScopedImGuiContext imgui;
        Amplitron::AmplitronSession session;
        Amplitron::MockGuiManager gui(session);
        Amplitron::g_mock_save_dialog_result = "test_recording_output.wav";
        auto& rec = static_cast<Recorder&>(gui.engine_.recorder());
        rec.recording_.store(false);
        rec.has_unsaved_.store(true);
        rec.filepath_ = "test_recording_output.wav";
        gui.gui_recording_.set_state([](RecordingState& s) { s.needs_save = true; });

        gui.run_frame();

        std::remove("test_recording_output.wav");
        std::remove("test_recording_output.meta.json");
        Amplitron::g_mock_save_dialog_result = "";
    }

    // 6. gui_manager_frame.cpp Lines 283-284 (tuner modal closed)
    {
        ScopedImGuiContext imgui;
        Amplitron::AmplitronSession session;
        Amplitron::MockGuiManager gui(session);
        gui.show_tuner_ = true;
        gui.run_frame();
    }

    // 7. gui_manager_frame.cpp Slider floats active (Lines 323, 368, 389)
    {
        ScopedImGuiContext imgui;
        Amplitron::AmplitronSession session;
        Amplitron::MockGuiManager gui(session);
        gui.audio_muted_ = true;
        mock_gui::MockImGui::reset();
        mock_gui::MockImGui::slider_float_ret = true;
        gui.run_frame();
        mock_gui::MockImGui::slider_float_ret = false;
    }

    // 8. gui_manager_menu.cpp Lines 38-65 (open_url_safe fork branches)
#ifndef _WIN32
    for (int fork_val : {-1, 0, 1}) {
        ScopedImGuiContext imgui;
        Amplitron::AmplitronSession session;
        Amplitron::MockGuiManager gui(session);
        gui.update_checker_.has_new_release_ = true;
        gui.update_checker_.new_release_version_ = "v9.9.9";
        gui.update_checker_.new_release_url_ = "http://mock-url";

        mock_fork_val = fork_val;
        mock_gui::MockImGui::reset();
        mock_gui::MockImGui::is_item_hovered_ret = true;
        mock_gui::MockImGui::target_menu_item = "New Release Available: v9.9.9";
        mock_gui::MockImGui::is_item_clicked_ret = true;
        gui.render_menu_bar();
    }
    mock_fork_val = -1;
#endif

    // 9. gui_manager_menu.cpp Lines 157-158 (copy preset empty failure toast)
    {
        ScopedImGuiContext imgui;
        Amplitron::AmplitronSession session;
        Amplitron::MockGuiManager gui(session);
        mock_serialise_current_preset_to_json_empty = true;
        mock_gui::MockImGui::reset();
        mock_gui::MockImGui::target_menu_item = "Copy Preset to Clipboard";
        gui.render_menu_bar();
        mock_serialise_current_preset_to_json_empty = false;
    }

    // 10. gui_manager_menu.cpp Lines 166-169 (change preset folder Native dialog)
    {
        ScopedImGuiContext imgui;
        Amplitron::AmplitronSession session;
        Amplitron::MockGuiManager gui(session);
        Amplitron::g_mock_folder_dialog_result = "/mock/presets/dir";
        mock_gui::MockImGui::reset();
        mock_gui::MockImGui::target_menu_item = "Change Presets Directory...";
        gui.render_menu_bar();
        Amplitron::g_mock_folder_dialog_result = "";
    }

    // 11. gui_manager_menu.cpp Lines 215-217 & 220-223 (Undo/Redo items clicked)
    {
        class DummyCommand : public Command {
           public:
            void undo() override {}
            const char* description() const override { return "Dummy"; }
        };

        ScopedImGuiContext imgui;
        Amplitron::AmplitronSession session;
        Amplitron::MockGuiManager gui(session);

        gui.command_history_.execute(std::make_unique<DummyCommand>());

        mock_gui::MockImGui::reset();
        mock_gui::MockImGui::target_menu_item = "Undo Dummy";
        gui.render_menu_bar();

        mock_gui::MockImGui::reset();
        mock_gui::MockImGui::target_menu_item = "Redo Dummy";
        gui.render_menu_bar();
    }

    // 12. gui_manager_menu.cpp Lines 279-282 (Recorder active REC indicator)
    {
        ScopedImGuiContext imgui;
        Amplitron::AmplitronSession session;
        Amplitron::MockGuiManager gui(session);
        auto& rec = static_cast<Recorder&>(gui.engine_.recorder());
        rec.recording_.store(true);

        mock_gui::MockImGui::reset();
        gui.render_menu_bar();

        rec.recording_.store(false);
    }

    // 13. gui_manager_menu.cpp Lines 319-320 (MIDI port open indicator)
    {
        ScopedImGuiContext imgui;
        Amplitron::AmplitronSession session;
        Amplitron::MockGuiManager gui(session);
        auto& mm = static_cast<MidiManager&>(gui.midi_manager());
        mm.current_port_ = 0;

        mock_gui::MockImGui::reset();
        gui.render_menu_bar();
    }
}
