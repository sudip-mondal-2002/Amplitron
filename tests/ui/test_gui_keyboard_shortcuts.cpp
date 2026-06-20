#include <imgui_internal.h>

#include <memory>

#include "gui/views/gui_keyboard_shortcuts.h"
#include "test_fixtures.h"
#include "test_framework.h"

using namespace Amplitron;

static ImGuiID get_item_id(const char* window_substr, const char* item_id_str) {
    ImGuiContext& g = *GImGui;
    ImGuiID popup_id = ImGui::GetID(window_substr);
    char popup_window_name[64];
    snprintf(popup_window_name, sizeof(popup_window_name), "##Popup_%08x", popup_id);

    for (int i = 0; i < g.Windows.Size; i++) {
        if (strstr(g.Windows[i]->Name, window_substr) ||
            strstr(g.Windows[i]->Name, popup_window_name)) {
            return g.Windows[i]->GetID(item_id_str);
        }
    }
    return 0;
}

static void click_item(const char* window_substr, const char* item_id_str) {
    ImGuiID id = get_item_id(window_substr, item_id_str);
    if (id != 0) {
        ImGuiContext& g = *GImGui;
        g.NavActivateId = id;
        g.NavActivateDownId = id;
        g.NavActivatePressedId = id;
    }
}

TEST_F(PresetTest, gui_keyboard_shortcuts_construction_no_crash) {
    GuiKeyboardShortcuts gks;
    (void)gks;
}

TEST_F(PresetTest, gui_keyboard_shortcuts_render_hidden) {
    ScopedImGuiContext imgui;
    GuiKeyboardShortcuts gks;

    KeyboardShortcutsProps props;
    gks.set_props(props);

    bool show = false;
    gks.render(show);
}

TEST_F(PresetTest, gui_keyboard_shortcuts_render_visible) {
    ScopedImGuiContext imgui;
    GuiKeyboardShortcuts gks;

    KeyboardShortcutsProps props;
    gks.set_props(props);

    bool show = true;
    gks.render(show);

    // End current frame and start a new one to allow popup modal to begin
    ImGui::Render();
    ImGui::NewFrame();

    gks.render(show);
}

TEST_F(PresetTest, gui_keyboard_shortcuts_click_close_button) {
    ScopedImGuiContext imgui;
    GuiKeyboardShortcuts gks;

    KeyboardShortcutsProps props;
    gks.set_props(props);

    bool show = true;
    // Frame 1: open the popup modal
    gks.render(show);

    // End Frame 1 and start Frame 2
    ImGui::Render();
    ImGui::NewFrame();

    // Render Frame 2: popup modal is now active and drawn
    gks.render(show);

    // End Frame 2 and start Frame 3
    ImGui::Render();
    ImGui::NewFrame();

    // Click the Close button inside the "Keyboard Shortcuts" popup modal
    click_item("Keyboard Shortcuts", "Close");

    // Render Frame 3: process the click event and update the show flag
    gks.render(show);

    // The show flag should now be set to false after clicking Close
    ASSERT_FALSE(show);
}
