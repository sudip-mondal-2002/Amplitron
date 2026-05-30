#pragma once

#include <imgui.h>
#include <functional>

namespace Amplitron {

struct FootswitchProps {
    bool enabled = false;
    const char* tooltip_prefix = "";

    // Callbacks
    std::function<void()> on_clicked;
};

class FootswitchComponent {
public:
    /**
     * @brief Render a reusable metallic footswitch toggle component.
     * @param imgui_id Unique ID for ImGui button tracking.
     * @param props    Footswitch state and callbacks.
     * @param zoom     DPI zoom scale.
     * @param center   Center coordinate of the footswitch.
     */
    static void render(const char* imgui_id, const FootswitchProps& props, float zoom, ImVec2 center);
};

} // namespace Amplitron
