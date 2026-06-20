#include "gui/views/gui_keyboard_shortcuts.h"

#include <imgui.h>

#include "gui/theme/theme.h"

namespace Amplitron {

void GuiKeyboardShortcuts::render(bool& show) {
    if (!show) return;

    ImGui::OpenPopup("Keyboard Shortcuts");

    ImGui::SetNextWindowSize(ImVec2(450, 420), ImGuiCond_FirstUseEver);
    bool open = show;
    if (ImGui::BeginPopupModal("Keyboard Shortcuts", &open, ImGuiWindowFlags_NoResize)) {
        ImGui::BeginChild("##ShortcutsScroll", ImVec2(0, -40), true);

        if (ImGui::BeginTable("##ShortcutsTable", 2,
                              ImGuiTableFlags_RowBg | ImGuiTableFlags_PadOuterX)) {
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed, 180);

            auto section_header = [](const char* name) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Spacing();
                ImGui::TextColored(Theme::Gold(), "%s", name);
                ImGui::TableNextColumn();
                ImGui::Spacing();
            };

            auto shortcut_row = [](const char* action, const char* keys) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::Text("%s", action);
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextColored(Theme::TextSecondary(), "%s", keys);
            };

            section_header("General");
            shortcut_row("Undo last action", "Ctrl+Z");
            shortcut_row("Redo last action", "Ctrl+Shift+Z / Ctrl+Y");
            shortcut_row("Toggle audio mute", "M");
            shortcut_row("Show keyboard shortcuts", "F1");

            section_header("Preset Management");
            shortcut_row("Save preset", "Ctrl+S");
            shortcut_row("Load preset", "Ctrl+O");

            section_header("Snapshots");
            shortcut_row("Recall Snapshot A", "Ctrl+1");
            shortcut_row("Recall Snapshot B", "Ctrl+2");
            shortcut_row("Recall Snapshot C", "Ctrl+3");
            shortcut_row("Recall Snapshot D", "Ctrl+4");

            section_header("Application");
            shortcut_row("Quit", "Alt+F4");

            ImGui::EndTable();
        }

        ImGui::EndChild();

        ImGui::Spacing();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 110);
        if (ImGui::Button("Close", ImVec2(100, 25))) {
            open = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
    show = open;
}

}  // namespace Amplitron
