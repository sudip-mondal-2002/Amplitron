#include "gui/gui_manager.h"

#include <SDL2/SDL.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>

#include "amplitron_session.h"
#include "gui/commands/command.h"
#include "gui/dialogs/file_dialog.h"
#include "gui/gl_setup.h"
#include "gui/pedalboard/pedal_board.h"
#include "gui/state/gui_graph_state.h"
#include "gui/theme/theme.h"
#include "preset_manager.h"
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
#if defined(EMSCRIPTEN) || (defined(__APPLE__) && TARGET_OS_IOS)
#define AMPLITRON_NO_DESKTOP_SHELL 1
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#pragma GCC diagnostic push
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#pragma GCC diagnostic pop

namespace Amplitron {

GuiManager::GuiManager(AmplitronSession& session)
    : session_(session),
      engine_(session.engine()),
      command_history_(session.command_history()),
      midi_manager_(session.midi()),
      snapshot_manager_(session.snapshot_manager()),
      tuner_pedal_(std::make_shared<TunerPedal>()),
      gui_presets_(engine_, command_history_, session.presets()),
      gui_midi_(midi_manager_) {
    pedal_board_ = std::make_unique<PedalBoard>(engine_, command_history_, &gui_midi_);
    gui_presets_.set_pedal_board(pedal_board_.get());
    gui_presets_.set_midi_manager(&midi_manager_);
    gui_analyzer_.set_expanded(engine_.is_analyzer_enabled());
}

GuiManager::~GuiManager() { shutdown(); }

bool GuiManager::initialize(int width, int height) {
    if (!window_context_.initialize(width, height, Theme::WINDOW_TITLE)) {
        return false;
    }
    PresetManager::load_config();

    // MIDI: load config first; if no saved mappings, install defaults
    midi_manager_.load_config();
    if (midi_manager_.mappings().empty()) {
        midi_manager_.install_default_mappings();
    }
    midi_manager_.initialize();

#ifndef AMPLITRON_NO_DESKTOP_SHELL
    update_checker_.start_check();
#endif

    if (pedal_board_) {
        pedal_board_->rebuild_widgets();
    }

    initialized_ = true;
    return true;
}

void GuiManager::shutdown() {
    if (!initialized_) return;
    initialized_ = false;

    midi_manager_.save_config();
    midi_manager_.shutdown();

    engine_.clear_tuner_tap();
    pedal_board_.reset();

    window_context_.shutdown();
}

void GuiManager::force_rebuild_pedal_widgets() {
    if (pedal_board_) {
        pedal_board_->rebuild_widgets();
    }
}

}  // namespace Amplitron
