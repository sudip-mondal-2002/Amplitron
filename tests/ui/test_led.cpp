#include "test_framework.h"
#include "test_fixtures.h"
#include <string>
#include <memory>
#include <cmath>
#include <functional>

#include "gui/components/led.h"

using namespace Amplitron;
using namespace TestFramework;

TEST_F(PresetTest, test_led_component_comprehensive) {
    ScopedImGuiContext imgui;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");

    LedProps props;
    props.enabled = false;
    props.led_color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    props.tooltip = "Status Indicator";
    props.blink = false;

    // Disabled led
    LedComponent::render("TestLed1", props, 1.0f, ImVec2(30, 30));

    // Enabled led
    props.enabled = true;
    LedComponent::render("TestLed2", props, 1.0f, ImVec2(30, 30));

    // Blinking led (on cycle)
    props.blink = true;
    props.blink_time = 0.2f;
    LedComponent::render("TestLed3", props, 1.0f, ImVec2(30, 30));

    // Blinking led (off cycle)
    props.blink_time = 0.8f;
    LedComponent::render("TestLed4", props, 1.0f, ImVec2(30, 30));

    // Blinking led but disabled (to cover the branch in led.cpp)
    props.enabled = false;
    LedComponent::render("TestLed5", props, 1.0f, ImVec2(30, 30));

    // Hover to trigger tooltip branch
    ImGuiIO& io = ImGui::GetIO();
    io.MousePos = ImVec2(30, 30);
    LedComponent::render("TestLed5", props, 1.0f, ImVec2(30, 30));

    ImGui::End();
}
