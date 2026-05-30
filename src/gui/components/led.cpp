#include "gui/components/led.h"
#include "gui/theme/theme.h"

namespace Amplitron {

void LedComponent::render(const char* imgui_id, const LedProps& props, float zoom, ImVec2 center) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float radius = 6.0f * zoom;
    float glow_radius = 10.0f * zoom;

    ImU32 base_color = Theme::LED_OFF;
    bool is_on = props.enabled;

    if (props.blink) {
        // Blinking alpha oscillation
        ImVec4 osc = Theme::RecBlink(props.blink_time);
        if (is_on) {
            base_color = ImGui::ColorConvertFloat4ToU32(
                ImVec4(props.led_color.x, props.led_color.y, props.led_color.z, osc.w)
            );
        }
    } else if (is_on) {
        base_color = ImGui::ColorConvertFloat4ToU32(props.led_color);
    }

    dl->AddCircleFilled(center, radius, base_color);

    if (is_on) {
        int glow_alpha = props.blink ? static_cast<int>(Theme::RecBlink(props.blink_time).w * 40) : 40;
        dl->AddCircleFilled(center, glow_radius,
            IM_COL32(
                static_cast<int>(props.led_color.x * 255),
                static_cast<int>(props.led_color.y * 255),
                static_cast<int>(props.led_color.z * 255),
                glow_alpha
            )
        );
    }

    if (props.tooltip) {
        ImGui::SetCursorScreenPos(ImVec2(center.x - 10.0f * zoom, center.y - 10.0f * zoom));
        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton(imgui_id, ImVec2(20.0f * zoom, 20.0f * zoom));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", props.tooltip);
        }
    }
}

} // namespace Amplitron
