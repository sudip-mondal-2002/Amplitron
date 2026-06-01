#pragma once

#include <imgui.h>
#include <string>
#include <functional>

namespace Amplitron {

struct KnobProps {
    std::string name;
    float value = 0.0f;
    float min_val = 0.0f;
    float max_val = 1.0f;
    float default_val = 0.0f;
    std::string unit;
    std::string tooltip;

    // MIDI status
    bool is_learning = false;
    std::string midi_info;

    // Color theme
    ImVec4 led_color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

    // Callback events
    std::function<void(float)> on_value_changed;
    std::function<void(float, float)> on_value_committed; // old_val, new_val
    std::function<void()> on_midi_learn_param;
    std::function<void()> on_midi_clear_param;
    std::function<void()> on_midi_learn_bypass;
    std::function<void()> on_midi_clear_bypass;
};

class KnobComponent {
public:
    /**
     * @brief Render a reusable parameter rotary knob.
     * @param imgui_id Unique ID string for ImGui tracking.
     * @param props    Configuration, current state, and callbacks.
     * @param zoom     DPI / GUI zoom multiplier.
     * @param center   Center coordinates where the knob should be drawn.
     */
    static void render(const char* imgui_id, const KnobProps& props, float zoom, ImVec2 center);
};

} // namespace Amplitron
