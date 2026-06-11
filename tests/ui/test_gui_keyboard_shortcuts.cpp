/**
 * @file test_gui_keyboard_shortcuts.cpp
 * @brief Headless-safe tests for GuiKeyboardShortcuts.
 */
#include <memory>

#include "gui/views/gui_keyboard_shortcuts.h"
#include "test_fixtures.h"
#include "test_framework.h"

using namespace Amplitron;

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
