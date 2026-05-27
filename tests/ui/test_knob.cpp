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
    advance_frame();
    
    // drag downwards
    io.MouseClicked[0] = false;
    io.MouseDelta = ImVec2(0.0f, 10.0f);
    io.MousePos = ImVec2(center.x, center.y + 10.0f);
    KnobComponent::render("Knob1", props, 1.0f, center);
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
