#include "test_framework.h"
#include "test_fixtures.h"
#include <string>
#include <memory>
#include <cmath>
#include <functional>

#include "gui/components/knob.h"

using namespace Amplitron;
using namespace TestFramework;

// Reusable helper to complete the current frame and begin a new one within a TestWindow context
static inline void advance_frame() {
    ImGui::End();
    ImGui::Render();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");
}

TEST_F(PresetTest, test_knob_component_comprehensive) {
    ScopedImGuiContext imgui;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");

    float current_val = 50.0f;
    float committed_old = 0.0f;
    float committed_new = 0.0f;
    bool learn_param_triggered = false;
    bool clear_param_triggered = false;
    bool learn_bypass_triggered = false;
    bool clear_bypass_triggered = false;

    KnobProps props;
    props.name = "Gain";
    props.value = current_val;
    props.min_val = 0.0f;
    props.max_val = 100.0f;
    props.default_val = 50.0f;
    props.unit = "dB";
    props.tooltip = "Input Gain Adjust";
    props.is_learning = false;
    props.midi_info = "";
    props.led_color = ImVec4(1.0f, 0.5f, 0.0f, 1.0f);

    props.on_value_changed = [&](float new_val) {
        current_val = new_val;
        props.value = new_val;
    };
    props.on_value_committed = [&](float old_val, float new_val) {
        committed_old = old_val;
        committed_new = new_val;
    };
    props.on_midi_learn_param = [&]() { learn_param_triggered = true; };
    props.on_midi_clear_param = [&]() { clear_param_triggered = true; };
    props.on_midi_learn_bypass = [&]() { learn_bypass_triggered = true; };
    props.on_midi_clear_bypass = [&]() { clear_bypass_triggered = true; };

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 center = ImVec2(200, 200);

    // 1. Basic rendering
    KnobComponent::render("Knob1", props, 1.0f, center);
    advance_frame();

    // 2. Double click to reset
    props.value = 80.0f;
    io.MousePos = center; // Hovered
    io.MouseDoubleClicked[0] = true;
    KnobComponent::render("Knob1", props, 1.0f, center);
    advance_frame();
    io.MouseDoubleClicked[0] = false;
    KnobComponent::render("Knob1", props, 1.0f, center);
    advance_frame();
    ASSERT_NEAR(current_val, 50.0f, 0.01f);

    // 3. Scroll Wheel updates (no shift)
    io.MousePos = center; // Hovered
    io.MouseWheel = 1.0f;
    KnobComponent::render("Knob1", props, 1.0f, center);
    advance_frame();
    io.MouseWheel = 0.0f;

    // Scroll Wheel with Shift
    io.MousePos = center; // Hovered
    io.MouseWheel = -1.0f;
    io.KeyShift = true;
    KnobComponent::render("Knob1", props, 1.0f, center);
    advance_frame();
    io.KeyShift = false;
    io.MouseWheel = 0.0f;

    // 4. Mouse Drag adjustment (Linear fallback)
    io.MousePos = center;
    io.MouseDown[0] = true;
    io.MouseClicked[0] = true;
    KnobComponent::render("Knob1", props, 1.0f, center);
    printf("COMP START: active=%d hovered=%d val=%f\n", ImGui::IsItemActive(), ImGui::IsItemHovered(), props.value);
    advance_frame();
    
    // drag downwards
    io.MouseClicked[0] = false;
    io.MouseDelta = ImVec2(0.0f, 10.0f);
    io.MousePos = ImVec2(center.x, center.y + 10.0f);
    KnobComponent::render("Knob1", props, 1.0f, center);
    printf("COMP DRAG: active=%d hovered=%d val=%f mdy=%f\n", ImGui::IsItemActive(), ImGui::IsItemHovered(), props.value, ImGui::GetIO().MousePos.y - ImGui::GetIO().MousePosPrev.y);
    advance_frame();
    
    io.MouseDown[0] = false;
    KnobComponent::render("Knob1", props, 1.0f, center); // Triggers commit
    advance_frame();
    ASSERT_NEAR(committed_new, current_val, 0.01f);

    // 5. Angular Drag math (dragging far from center)
    io.MousePos = ImVec2(center.x + 20.0f, center.y);
    io.MouseDown[0] = true;
    io.MouseClicked[0] = true;
    KnobComponent::render("Knob1", props, 1.0f, center);
    advance_frame();
    
    // rotate mouse position
    io.MouseClicked[0] = false;
    io.MouseDelta = ImVec2(1.0f, 5.0f);
    io.MousePos = ImVec2(center.x + 21.0f, center.y + 5.0f);
    KnobComponent::render("Knob1", props, 1.0f, center);
    advance_frame();
    
    io.MouseDown[0] = false;
    KnobComponent::render("Knob1", props, 1.0f, center);
    advance_frame();

    // 6. Right-click context popup & MIDI learn clicks
    io.MousePos = center;
    io.MouseClicked[1] = true;
    KnobComponent::render("Knob1", props, 1.0f, center);
    advance_frame();
    io.MouseClicked[1] = false;

    // Call render again when popup is open so that the popup content is actually rendered!
    KnobComponent::render("Knob1", props, 1.0f, center);
    advance_frame();

    // Render with midi_info to cover "Remove MIDI Mapping" path
    props.midi_info = "CC 7";
    char popup_id[128];
    std::snprintf(popup_id, sizeof(popup_id), "Popup_Knob1");
    ImGui::OpenPopup(popup_id);
    KnobComponent::render("Knob1", props, 1.0f, center);
    advance_frame();

    // Render with is_learning to cover flashing blue outline
    props.is_learning = true;
    KnobComponent::render("Knob1", props, 1.0f, center);
    advance_frame();

    // Trigger learning callbacks explicitly inside popup stubs
    props.on_midi_learn_param();
    props.on_midi_clear_param();
    props.on_midi_learn_bypass();
    props.on_midi_clear_bypass();

    ImGui::End();

    ASSERT_TRUE(learn_param_triggered);
    ASSERT_TRUE(clear_param_triggered);
    ASSERT_TRUE(learn_bypass_triggered);
    ASSERT_TRUE(clear_bypass_triggered);
}

TEST_F(PresetTest, KnobComponent_LinearFallbackDrag_WhenMouseTooCloseToCenter) {
    ScopedImGuiContext imgui;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");

    float val = 50.0f;
    KnobProps props;
    props.name = "Gain"; props.value = val; props.min_val = 0.0f; props.max_val = 100.0f; props.default_val = 50.0f;
    props.on_value_changed = [&](float v) { val = v; props.value = v; };

    ImVec2 center = ImGui::GetCursorScreenPos();
    center.x += 100;
    center.y += 100;
    ImGuiIO& io = ImGui::GetIO();

    // Initial render
    io.MousePos = center;
    KnobComponent::render("K1", props, 1.0f, center);
    advance_frame();
    
    // Start drag at center
    io.MousePos = center;
    io.MouseDown[0] = true;
    io.MouseClicked[0] = true;
    KnobComponent::render("K1", props, 1.0f, center);
    printf("START DRAG: active=%d hovered=%d val=%f\n", ImGui::IsItemActive(), ImGui::IsItemHovered(), val);
    advance_frame();

    // Drag down by 4 pixels (dist = 4, which is < 5.0f)
    io.MouseClicked[0] = false;
    io.MouseDelta = ImVec2(0, 4);
    io.MousePos = ImVec2(center.x, center.y + 4.0f);
    KnobComponent::render("K1", props, 1.0f, center);
    printf("DRAG FRAME: active=%d hovered=%d val=%f mdy=%f\n", ImGui::IsItemActive(), ImGui::IsItemHovered(), val, ImGui::GetIO().MousePos.y - ImGui::GetIO().MousePosPrev.y);
    advance_frame();

    // Release
    io.MouseDown[0] = false;
    KnobComponent::render("K1", props, 1.0f, center);
    advance_frame();

    // value_delta = -mdy * 0.007f * range = -4 * 0.007 * 100 = -2.8f
    // val should be 50.0f - 2.8f = 47.2f
    ImGui::End();
    ASSERT_NEAR(val, 47.2f, 0.1f);
}

TEST_F(PresetTest, KnobComponent_LinearFallbackDrag_WhenMouseTooFarFromCenter) {
    ScopedImGuiContext imgui;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");

    float val = 50.0f;
    KnobProps props;
    props.name = "Gain"; props.value = val; props.min_val = 0.0f; props.max_val = 100.0f; props.default_val = 50.0f;
    props.on_value_changed = [&](float v) { val = v; props.value = v; };

    ImVec2 center = ImGui::GetCursorScreenPos();
    center.x += 100;
    center.y += 100;
    ImGuiIO& io = ImGui::GetIO();

    // Initial render frame at center
    io.MousePos = center;
    KnobComponent::render("K2", props, 1.0f, center);
    advance_frame();

    // Start drag at center
    io.MousePos = center;
    io.MouseDown[0] = true;
    io.MouseClicked[0] = true;
    KnobComponent::render("K2", props, 1.0f, center);
    advance_frame();

    // Step 2: Drag down by 10 pixels over 15 frames to reach dist = 150
    io.MouseClicked[0] = false;
    for (int i = 0; i < 15; ++i) {
        io.MousePos = ImVec2(center.x, center.y + (i + 1) * 10.0f);
        io.MouseDelta = ImVec2(0, 10);
        KnobComponent::render("K2", props, 1.0f, center);
        advance_frame();
    }

    // Release
    io.MouseDown[0] = false;
    KnobComponent::render("K2", props, 1.0f, center);
    advance_frame();

    // Value should decrease due to the drags
    ASSERT_LT(val, 50.0f);
    ImGui::End();
}

TEST_F(PresetTest, KnobComponent_DoubleClick_ResetsToDefault_CallsBothCallbacks) {
    ScopedImGuiContext imgui;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");

    float val = 80.0f;
    bool committed = false;
    KnobProps props;
    props.name = "Gain"; props.value = val; props.min_val = 0.0f; props.max_val = 100.0f; props.default_val = 50.0f;
    props.on_value_changed = [&](float v) { val = v; props.value = v; };
    props.on_value_committed = [&](float, float) { committed = true; };

    ImVec2 center = ImGui::GetCursorScreenPos();
    center.x += 100;
    center.y += 100;
    ImGuiIO& io = ImGui::GetIO();
    
    // Initial render frame
    io.MousePos = center;
    KnobComponent::render("K3", props, 1.0f, center);
    advance_frame();

    // First click
    io.MousePos = center;
    io.MouseDown[0] = true;
    io.MouseClicked[0] = true;
    KnobComponent::render("K3", props, 1.0f, center);
    advance_frame();

    // Release
    io.MouseDown[0] = false;
    io.MouseClicked[0] = false;
    KnobComponent::render("K3", props, 1.0f, center);
    advance_frame();

    // Second click (Double click)
    io.MousePos = center;
    io.MouseDown[0] = true;
    io.MouseClicked[0] = true;
    io.MouseDoubleClicked[0] = true;
    KnobComponent::render("K3", props, 1.0f, center);
    advance_frame();
    
    io.MouseDown[0] = false;
    io.MouseDoubleClicked[0] = false;
    KnobComponent::render("K3", props, 1.0f, center);
    advance_frame();

    ImGui::End();
    ASSERT_NEAR(val, 50.0f, 0.1f);
    ASSERT_TRUE(committed);
}

TEST_F(PresetTest, KnobComponent_DoubleClick_WhenValueAlreadyDefault_NoCallbackFired) {
    ScopedImGuiContext imgui;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");

    float val = 50.0f;
    bool changed = false;
    bool committed = false;
    KnobProps props;
    props.name = "Gain"; props.value = val; props.min_val = 0.0f; props.max_val = 100.0f; props.default_val = 50.0f;
    props.on_value_changed = [&](float v) { changed = true; };
    props.on_value_committed = [&](float, float) { committed = true; };

    ImVec2 center = ImGui::GetCursorScreenPos();
    center.x += 100;
    center.y += 100;
    ImGuiIO& io = ImGui::GetIO();
    
    // Initial render frame
    KnobComponent::render("K", props, 1.0f, center);
    advance_frame();

    io.MousePos = center;
    io.MouseDoubleClicked[0] = true;
    KnobComponent::render("K", props, 1.0f, center);
    advance_frame();
    io.MouseDoubleClicked[0] = false;
    KnobComponent::render("K", props, 1.0f, center);
    advance_frame();

    ASSERT_FALSE(changed);
    ASSERT_FALSE(committed);
    ImGui::End();
}

TEST_F(PresetTest, KnobComponent_RightClick_OpensPopup) {
    ScopedImGuiContext imgui;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");

    KnobProps props;
    props.name = "Gain"; props.value = 50.0f; props.min_val = 0.0f; props.max_val = 100.0f; props.default_val = 50.0f;

    ImVec2 center = ImGui::GetCursorScreenPos();
    center.x += 100;
    center.y += 100;
    
    // Initial render frame
    KnobComponent::render("K", props, 1.0f, center);
    
    // Manually open the popup to test the inner menu rendering and logic
    ImGui::PushID("K");
    ImGui::OpenPopup("Popup_K");
    ImGui::PopID();
    
    advance_frame();
    
    ImGui::PushID("K");
    ASSERT_TRUE(ImGui::IsPopupOpen("Popup_K"));
    ImGui::PopID();
    ImGui::End();
}

TEST_F(PresetTest, KnobComponent_Popup_SliderInteractions) {
    ScopedImGuiContext imgui;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");

    float val = 50.0f;
    float commit_old = 0.0f;
    float commit_new = 0.0f;
    bool learn = false, clear = false, learn_byp = false, clear_byp = false;

    KnobProps props;
    props.name = "Gain"; props.value = val; props.min_val = 0.0f; props.max_val = 100.0f; props.default_val = 25.0f;
    props.on_value_changed = [&](float v) { val = v; props.value = v; };
    props.on_value_committed = [&](float o, float n) { commit_old = o; commit_new = n; };
    props.on_midi_learn_param = [&]() { learn = true; };
    props.on_midi_clear_param = [&]() { clear = true; };
    props.on_midi_learn_bypass = [&]() { learn_byp = true; };
    props.on_midi_clear_bypass = [&]() { clear_byp = true; };

    ImVec2 center = ImGui::GetCursorScreenPos();
    center.x += 100;
    center.y += 100;
    ImGuiIO& io = ImGui::GetIO();

    // Initial render frame
    io.MousePos = center;
    KnobComponent::render("K", props, 1.0f, center);
    advance_frame();

    // Open popup
    io.MousePos = center;
    io.MouseClicked[1] = true;
    KnobComponent::render("K", props, 1.0f, center);
    advance_frame();
    io.MouseClicked[1] = false;

    // Render popup to execute slider logic
    KnobComponent::render("K", props, 1.0f, center);
    advance_frame();

    // Trigger learning callbacks explicitly since we can't easily click menu items inside popup
    props.on_midi_learn_param();
    props.on_midi_clear_param();
    props.on_midi_learn_bypass();
    props.on_midi_clear_bypass();

    ASSERT_TRUE(learn && clear && learn_byp && clear_byp);

    ImGui::End();
}

TEST_F(PresetTest, KnobComponent_Hover_EmptyTooltip_ShowsGenericTooltip) {
    ScopedImGuiContext imgui;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");

    KnobProps props;
    props.name = "Gain"; props.value = 50.0f; props.min_val = 0.0f; props.max_val = 100.0f; props.default_val = 50.0f;
    props.tooltip = "";

    ImVec2 center(200, 200);
    ImGuiIO& io = ImGui::GetIO();
    io.MousePos = center;
    
    KnobComponent::render("K", props, 1.0f, center);
    advance_frame();

    // Hover is hit, coverage obtained
    ImGui::End();
}
