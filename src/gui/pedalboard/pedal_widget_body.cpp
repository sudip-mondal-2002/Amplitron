#include "gui/pedalboard/pedal_widget.h"
#include "audio/effects/amp_simulator.h"
#include "gui/theme/theme.h"

#include <cstdio>

namespace Amplitron {

void PedalWidget::render_amp_cabinet(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float pedal_width, float pedal_height, float zoom) {
    ImU32 cab_body = IM_COL32(30, 22, 16, 255);
    ImU32 cab_border = IM_COL32(90, 70, 40, 255);
    ImU32 cab_grille = IM_COL32(18, 14, 10, 255);
    ImU32 cab_grille_line = IM_COL32(38, 30, 22, 180);

    dl->AddRectFilled(p0, p1, cab_body, Theme::ROUNDING_MD * zoom);
    dl->AddRect(p0, p1, cab_border, Theme::ROUNDING_MD * zoom, 0, 2.5f * zoom);

    dl->AddRectFilled(
        ImVec2(p0.x + 6 * zoom, p0.y + 6 * zoom),
        ImVec2(p1.x - 6 * zoom, p0.y + 10 * zoom),
        Theme::ACCENT_GOLD_DIM, 2.0f * zoom);

    ImVec2 plate_p0 = ImVec2(p0.x + 8 * zoom, p0.y + 14 * zoom);
    ImVec2 plate_p1 = ImVec2(p1.x - 8 * zoom, p0.y + 50 * zoom);
    dl->AddRectFilled(plate_p0, plate_p1,
        IM_COL32(46, 38, 28, 220), Theme::ROUNDING_SM * zoom);
    dl->AddRect(plate_p0, plate_p1,
        IM_COL32(70, 58, 38, 180), Theme::ROUNDING_SM * zoom, 0, 1.0f * zoom);

    ImGui::SetCursorScreenPos(ImVec2(p0.x + 12 * zoom, p0.y + 18 * zoom));
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::Gold());
    ImGui::Text("AMP");
    ImGui::PopStyleColor();

    int model_idx = static_cast<int>(effect_->params()[0].value);
    const auto& models = get_amp_models();
    const char* model_name = "Unknown";
    if (model_idx >= 0 && model_idx < static_cast<int>(models.size())) {
        model_name = models[model_idx].name;
    }
    ImVec2 mn_size = ImGui::CalcTextSize(model_name);
    float mn_x = p0.x + (pedal_width - mn_size.x) * 0.5f;
    ImGui::SetCursorScreenPos(ImVec2(mn_x, p0.y + 33 * zoom));
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextPrimary());
    ImGui::Text("%s", model_name);
    ImGui::PopStyleColor();

    float led_x = p1.x - 22 * zoom;
    float led_y = p0.y + 26 * zoom;
    dl->AddCircleFilled(ImVec2(led_x, led_y), 5 * zoom, Theme::LED_GREEN);
    dl->AddCircleFilled(ImVec2(led_x, led_y), 8 * zoom, Theme::LED_GREEN_GLOW & 0x30FFFFFF);

    float grille_top = p1.y - 100 * zoom;
    float grille_bottom = p1.y - 12 * zoom;
    float grille_left = p0.x + 12 * zoom;
    float grille_right = p1.x - 12 * zoom;

    dl->AddRectFilled(
        ImVec2(grille_left, grille_top),
        ImVec2(grille_right, grille_bottom),
        cab_grille, Theme::ROUNDING_SM * zoom);
    dl->AddRect(
        ImVec2(grille_left, grille_top),
        ImVec2(grille_right, grille_bottom),
        IM_COL32(50, 40, 28, 180), Theme::ROUNDING_SM * zoom, 0, 1.0f * zoom);

    for (float gy = grille_top + 6 * zoom; gy < grille_bottom - 4 * zoom; gy += 5.0f * zoom) {
        dl->AddLine(
            ImVec2(grille_left + 4 * zoom, gy),
            ImVec2(grille_right - 4 * zoom, gy),
            cab_grille_line, 1.0f * zoom);
    }

    dl->AddRectFilled(
        ImVec2(p0.x + 6 * zoom, p1.y - 10 * zoom),
        ImVec2(p1.x - 6 * zoom, p1.y - 6 * zoom),
        Theme::ACCENT_GOLD_DIM, 2.0f * zoom);
}

} // namespace Amplitron
