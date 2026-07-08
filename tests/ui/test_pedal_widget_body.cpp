// Tests for pedal_widget_body.cpp:
//   - render_amp_cabinet()   (all drawing paths)
//   - render_nam_loader_display()  (no model / model loaded / long name / slash
//   in path)
//
// All tests run under ScopedImGuiContext which creates a headless ImGui frame.

#include <imgui_internal.h>

#include <cstring>
#include <memory>
#include <string>

#define private public
#include "gui/pedalboard/pedal_widget.h"
#undef private

#include "audio/effects/amp_cab/amp_simulator.h"
#include "audio/effects/nam_loader.h"
#include "audio/engine/audio_engine.h"
#include "fixtures/file_dialog_mock.h"
#include "gui/commands/command_history.h"
#include "test_fixtures.h"
#include "test_framework.h"

using namespace Amplitron;
using namespace TestFramework;

// ---------------------------------------------------------------------------
// render_amp_cabinet
// ---------------------------------------------------------------------------

TEST_F(PresetTest, pedal_widget_body_render_amp_cabinet_basic) {
    ScopedImGuiContext imgui;
    AudioEngine engine;
    engine.initialize();

    auto amp = std::make_shared<AmpSimulator>();
    PedalWidget widget(engine, amp, 0);

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec2 p0(10.0f, 10.0f);
    ImVec2 p1(200.0f, 370.0f);
    float pedal_width = 190.0f;
    float pedal_height = 360.0f;

    // Basic render — should not crash and must exercise every draw call
    TestAccessor::render_amp_cabinet(widget, dl, p0, p1, pedal_width, pedal_height, 1.0f);

    ImGui::End();
    engine.shutdown();
}

TEST_F(PresetTest, pedal_widget_body_render_amp_cabinet_all_model_indices) {
    // Exercise the model_idx boundary checks: valid index, negative index,
    // out-of-bounds index
    ScopedImGuiContext imgui;
    AudioEngine engine;
    engine.initialize();

    auto amp = std::make_shared<AmpSimulator>();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec2 p0(10.0f, 10.0f);
    ImVec2 p1(200.0f, 370.0f);

    // Valid index = 0 (first model)
    amp->params()[0].value = 0.0f;
    PedalWidget w0(engine, amp, 0);
    TestAccessor::render_amp_cabinet(w0, dl, p0, p1, 190.0f, 360.0f, 1.0f);
    advance_frame();

    // Out-of-bounds index (large positive)
    amp->params()[0].value = 9999.0f;
    PedalWidget w1(engine, amp, 1);
    TestAccessor::render_amp_cabinet(w1, dl, p0, p1, 190.0f, 360.0f, 1.0f);
    advance_frame();

    // Negative index (model_idx < 0)
    amp->params()[0].value = -1.0f;
    PedalWidget w2(engine, amp, 2);
    TestAccessor::render_amp_cabinet(w2, dl, p0, p1, 190.0f, 360.0f, 1.0f);

    ImGui::End();
    engine.shutdown();
}

TEST_F(PresetTest, pedal_widget_body_render_amp_cabinet_zoom_variations) {
    // Ensure the zoom-scaled draw calls don't crash at different zoom levels
    ScopedImGuiContext imgui;
    AudioEngine engine;
    engine.initialize();

    auto amp = std::make_shared<AmpSimulator>();
    amp->params()[0].value = 0.0f;
    PedalWidget widget(engine, amp, 0);

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec2 p0(5.0f, 5.0f);
    ImVec2 p1(205.0f, 375.0f);

    for (float zoom : {0.5f, 1.0f, 1.5f, 2.0f}) {
        TestAccessor::render_amp_cabinet(widget, dl, p0, p1, 200.0f, 370.0f, zoom);
    }

    ImGui::End();
    engine.shutdown();
}

// ---------------------------------------------------------------------------
// render_nam_loader_display — no model loaded
// ---------------------------------------------------------------------------

TEST_F(PresetTest, pedal_widget_body_nam_display_no_model) {
    // When no model is loaded, the "No model loaded" branch should render.
    ScopedImGuiContext imgui;
    AudioEngine engine;
    engine.initialize();

    auto nam = std::make_shared<NamLoader>();
    // model_path() is empty by default
    PedalWidget widget(engine, nam, 0);

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");

    ImVec2 p0(10.0f, 10.0f);
    TestAccessor::render_nam_loader_display(widget, p0, 190.0f, 1.0f);

    ImGui::End();
    engine.shutdown();
}

TEST_F(PresetTest, pedal_widget_body_nam_display_model_loaded_short_name) {
    // When model_path is set and its basename is <= 20 chars, render the short
    // name branch.
    ScopedImGuiContext imgui;
    AudioEngine engine;
    engine.initialize();

    auto nam = std::make_shared<NamLoader>();
    // Load a real model to set model_path_.
    nam->load_model("../tests/assets/rtneural_test_model.json");

    PedalWidget widget(engine, nam, 0);

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");

    ImVec2 p0(10.0f, 10.0f);
    TestAccessor::render_nam_loader_display(widget, p0, 190.0f, 1.0f);

    ImGui::End();
    engine.shutdown();
}

TEST_F(PresetTest, pedal_widget_body_nam_display_model_with_slash_in_path) {
    // Path contains a slash — the substr(slash + 1) branch is taken.
    ScopedImGuiContext imgui;
    AudioEngine engine;
    engine.initialize();

    auto nam = std::make_shared<NamLoader>();
    // load_model uses the full path, which already contains slashes.
    nam->load_model("../tests/assets/rtneural_test_model.json");

    PedalWidget widget(engine, nam, 0);

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");

    ImVec2 p0(10.0f, 10.0f);
    // render_nam_loader_display reads model_path() which contains '/' characters
    TestAccessor::render_nam_loader_display(widget, p0, 190.0f, 1.0f);

    ImGui::End();
    engine.shutdown();
}

TEST_F(PresetTest, pedal_widget_body_nam_display_long_name_truncated) {
    // When the extracted basename is > 20 chars it should be truncated to 17 +
    // "..."
    ScopedImGuiContext imgui;
    AudioEngine engine;
    engine.initialize();

    // Build a temp file with a long basename (> 20 chars).
    const std::string long_name = "../tests/assets/very_long_model_name_over_twenty_chars.json";
    // Write a valid RTNeural JSON to it so load_model succeeds.
    {
        std::ofstream f(long_name);
        f << R"({"in_shape":[null,1],"layers":[{"type":"dense","shape":[null,1],"weights":[[[1.0]],[0.0]]}]})";
    }

    auto nam = std::make_shared<NamLoader>();
    bool ok = nam->load_model(long_name);
    // Clean up the temp file regardless of load result
    std::remove(long_name.c_str());

    PedalWidget widget(engine, nam, 0);

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");

    ImVec2 p0(10.0f, 10.0f);
    TestAccessor::render_nam_loader_display(widget, p0, 190.0f, 1.0f);

    ImGui::End();
    engine.shutdown();
}

TEST_F(PresetTest, pedal_widget_body_nam_display_zoom_variations) {
    ScopedImGuiContext imgui;
    AudioEngine engine;
    engine.initialize();

    auto nam = std::make_shared<NamLoader>();
    PedalWidget widget(engine, nam, 0);

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");

    ImVec2 p0(10.0f, 10.0f);
    for (float zoom : {0.5f, 1.0f, 2.0f}) {
        TestAccessor::render_nam_loader_display(widget, p0, 190.0f, zoom);
        advance_frame();
    }

    ImGui::End();
    engine.shutdown();
}

// ---------------------------------------------------------------------------
// render_nam_loader_display — wrong effect type (not NamLoader)
// ---------------------------------------------------------------------------

TEST_F(PresetTest, pedal_widget_body_nam_display_wrong_effect_type) {
    // If effect_ is not a NamLoader, the dynamic_cast returns nullptr
    // and render_nam_loader_display returns immediately.
    ScopedImGuiContext imgui;
    AudioEngine engine;
    engine.initialize();

    auto amp = std::make_shared<AmpSimulator>();
    PedalWidget widget(engine, amp, 0);

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");

    // Calling this with a non-NamLoader effect exercises the early-return path.
    TestAccessor::render_nam_loader_display(widget, ImVec2(10, 10), 190.0f, 1.0f);

    ImGui::End();
    engine.shutdown();
}

TEST_F(PresetTest, pedal_widget_body_nam_display_click_load_success) {
    ScopedImGuiContext imgui;
    AudioEngine engine;
    engine.initialize();

    auto nam = std::make_shared<NamLoader>();
    PedalWidget widget(engine, nam, 0);

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");

    // Set up mock result for show_open_dialog
    Amplitron::TestMocks::reset();
    Amplitron::TestMocks::set_mock_result("../tests/assets/rtneural_test_model.json");

    // Render once to submit the button so its ID exists
    TestAccessor::render_nam_loader_display(widget, ImVec2(10.0f, 10.0f), 190.0f, 1.0f);

    // Get the ID of the button
    ImGuiID id = ImGui::GetID("Load .nam##nam_load_0");
    ASSERT_NE(id, 0U);

    // Activate the button
    ImGuiContext& g = *GImGui;
    g.NavActivateId = id;
    g.NavActivateDownId = id;
    g.NavActivatePressedId = id;

    // Render again — this time the button will be considered clicked!
    TestAccessor::render_nam_loader_display(widget, ImVec2(10.0f, 10.0f), 190.0f, 1.0f);

    ImGui::End();
    engine.shutdown();

    // Verify the model got loaded!
    ASSERT_EQ(nam->model_path(), std::string("../tests/assets/rtneural_test_model.json"));
}

TEST_F(PresetTest, pedal_widget_body_nam_display_click_load_cancel) {
    ScopedImGuiContext imgui;
    AudioEngine engine;
    engine.initialize();

    auto nam = std::make_shared<NamLoader>();
    PedalWidget widget(engine, nam, 0);

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");

    // Set up mock result to empty string
    Amplitron::TestMocks::reset();
    Amplitron::TestMocks::set_mock_result("");

    // Render once to submit the button
    TestAccessor::render_nam_loader_display(widget, ImVec2(10.0f, 10.0f), 190.0f, 1.0f);

    ImGuiID id = ImGui::GetID("Load .nam##nam_load_0");
    ASSERT_NE(id, 0U);

    // Activate the button
    ImGuiContext& g = *GImGui;
    g.NavActivateId = id;
    g.NavActivateDownId = id;
    g.NavActivatePressedId = id;

    // Render again to trigger the click
    TestAccessor::render_nam_loader_display(widget, ImVec2(10.0f, 10.0f), 190.0f, 1.0f);

    ImGui::End();
    engine.shutdown();

    // Verify model path remains empty
    ASSERT_TRUE(nam->model_path().empty());
}

TEST_F(PresetTest, pedal_widget_render_with_nam_loader) {
    ScopedImGuiContext imgui;
    AudioEngine engine;
    engine.initialize();

    auto nam = std::make_shared<NamLoader>();
    PedalWidget widget(engine, nam, 0);

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");

    // Call render() directly to cover the NamLoader branches in pedal_widget.cpp
    bool result = widget.render(1.0f);
    ASSERT_FALSE(result);

    ImGui::End();
    engine.shutdown();
}
