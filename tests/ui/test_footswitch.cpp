#include "test_framework.h"
#include "test_fixtures.h"
#include <string>
#include <memory>
#include <cmath>
#include <functional>

#include "gui/components/footswitch.h"

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

TEST_F(PresetTest, test_footswitch_component_comprehensive) {
    ScopedImGuiContext imgui;
    
    ImGuiIO& io = ImGui::GetIO();
    
    // Position the mouse and set not clicked
    io.MousePos = ImVec2(150, 150);
    io.MouseDown[0] = false;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");

    bool clicked = false;
    FootswitchProps props;
    props.enabled = false;
    props.tooltip_prefix = "Bypass";
    props.on_clicked = [&]() { clicked = true; };

    // Render 1: Initial frame setup (setting the mouse pos on the window)
    FootswitchComponent::render("TestFootswitch1", props, 1.0f, ImVec2(150, 150));
    
    // Advance frame so the mouse position (150, 150) is processed and the footswitch becomes hovered!
    advance_frame();
    
    // Render 2: Hover frame (now fully hovered)
    FootswitchComponent::render("TestFootswitch1", props, 1.0f, ImVec2(150, 150));
    
    // Set mouse down BEFORE advancing to trigger the click
    io.MouseDown[0] = true;
    advance_frame();
    
    // Render 3: Click frame (mouse down processed while hovered -> click triggers)
    FootswitchComponent::render("TestFootswitch1", props, 1.0f, ImVec2(150, 150));
    
    // Set mouse up BEFORE advancing to release the click
    io.MouseDown[0] = false;
    advance_frame();
    
    // Render 4: Release frame
    FootswitchComponent::render("TestFootswitch1", props, 1.0f, ImVec2(150, 150));
    
    ImGui::End();
    
    ASSERT_TRUE(clicked);

    // Render in active state (new window frame)
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");
    props.enabled = true;
    FootswitchComponent::render("TestFootswitch2", props, 1.2f, ImVec2(150, 150));
    ImGui::End();
}
