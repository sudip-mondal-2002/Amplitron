#include "gui/components/footswitch.h"
#include "gui/theme/theme.h"
#include <cstdio>

namespace Amplitron {

void FootswitchComponent::render(const char* imgui_id, const FootswitchProps& props, float zoom, ImVec2 center) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float radius = 18.0f * zoom;
    float inner_radius = 12.0f * zoom;
    float hit_w = 50.0f * zoom;
    float hit_h = 30.0f * zoom;

    dl->AddCircleFilled(center, radius, Theme::SWITCH_BODY);
    dl->AddCircle(center, radius, Theme::SWITCH_RING, 0, 2.0f * zoom);
    dl->AddCircleFilled(center, inner_radius, props.enabled ? Theme::SWITCH_ACTIVE : Theme::SWITCH_IDLE);

    ImGui::SetCursorScreenPos(ImVec2(center.x - hit_w * 0.5f, center.y - hit_h * 0.5f));
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton(imgui_id, ImVec2(hit_w, hit_h));

    if (ImGui::IsItemClicked() && props.on_clicked) {
        props.on_clicked();
    }

    if (ImGui::IsItemHovered()) {
        char tip[128];
        std::snprintf(tip, sizeof(tip), "%s%s", props.tooltip_prefix, props.enabled ? "Click to bypass" : "Click to enable");
        ImGui::SetTooltip("%s", tip);
    }
}

} // namespace Amplitron
