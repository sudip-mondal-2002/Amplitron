#pragma once

#include <imgui.h>
#include <functional>

namespace Amplitron {

struct LedProps {
    bool enabled = false;
    ImVec4 led_color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    const char* tooltip = nullptr;
    
    // Pulse animation options
    bool blink = false;
    float blink_time = 0.0f; // time used to calculate blink rate
};

class LedComponent {
public:
    /**
     * @brief Render a status LED indicator with glowing outlines.
     * @param imgui_id Unique ID for ImGui interactions (tooltip hover area).
     * @param props    LED state, color, and tooltip.
     * @param zoom     DPI zoom factor.
     * @param center   Center coordinate of the LED.
     */
    static void render(const char* imgui_id, const LedProps& props, float zoom, ImVec2 center);
};

} // namespace Amplitron
