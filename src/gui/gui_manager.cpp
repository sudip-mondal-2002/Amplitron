#include "amplitron_session.h"
#include "gui/gui_manager.h"
#include "gui/pedalboard/pedal_board.h"
#include "gui/theme/theme.h"
#include "gui/dialogs/file_dialog.h"
#include "gui/commands/command.h"
#include "gui/state/gui_graph_state.h"
#include "preset_manager.h"

#include "gui/gl_setup.h"
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <SDL2/SDL.h>
#if defined(__APPLE__)
#  include <TargetConditionals.h>
#endif
#if defined(EMSCRIPTEN) || (defined(__APPLE__) && TARGET_OS_IOS)
#  define AMPLITRON_NO_DESKTOP_SHELL 1
#endif
#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
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
      gui_midi_(midi_manager_)
{
    pedal_board_ = std::make_unique<PedalBoard>(engine_, command_history_, &gui_midi_);
    gui_presets_.set_pedal_board(pedal_board_.get());
    gui_presets_.set_midi_manager(&midi_manager_);
    gui_analyzer_.set_expanded(engine_.is_analyzer_enabled());
}

GuiManager::~GuiManager() {
    shutdown();
}

bool GuiManager::initialize(int width, int height) {
    if (!window_context_.initialize(width, height, Theme::WINDOW_TITLE)) {
        return false;
    }

    #ifndef AMPLITRON_NO_DESKTOP_SHELL
        gl_context_ = SDL_GL_CreateContext(window_);
        if (!gl_context_) {
            std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << std::endl;
            return false;
        }
        SDL_GL_MakeCurrent(window_, gl_context_);
        SDL_GL_SetSwapInterval(1); // vsync
    #endif

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Amplitron design system
    ImGui::StyleColorsDark();
    Theme::ApplyStyle();

    // --- DPI scaling and font loading ---
    float dpi_scale = 1.0f;
    
    #ifndef AMPLITRON_NO_DESKTOP_SHELL
    {
        int draw_w = window_width_, draw_h = window_height_;
        SDL_GL_GetDrawableSize(window_, &draw_w, &draw_h);
        if (window_width_ > 0)
            dpi_scale = static_cast<float>(draw_w) / static_cast<float>(window_width_);
    }
    #endif

    GuiGraphState::get_instance().dpi_scale = dpi_scale;

    {
        const float base_font_size = 14.0f;
        const float scaled_size    = base_font_size;

        ImFont* loaded_font = nullptr;
        auto try_font = [&](const std::string& path) {
            if (!loaded_font)
                loaded_font = io.Fonts->AddFontFromFileTTF(path.c_str(), scaled_size);
        };

        char* base_path = SDL_GetBasePath();
        if (base_path) {
            try_font(std::string(base_path) + "assets/fonts/Roboto-Medium.ttf");
            SDL_free(base_path);
        }
        try_font("assets/fonts/Roboto-Medium.ttf");
    #ifdef __EMSCRIPTEN__
        try_font("/assets/fonts/Roboto-Medium.ttf");
    #endif
        
        try_font("../assets/fonts/Roboto-Medium.ttf");
        try_font("external/imgui/misc/fonts/Roboto-Medium.ttf");
        try_font("../external/imgui/misc/fonts/Roboto-Medium.ttf");

        if (!loaded_font) {
            io.Fonts->AddFontDefault();
        } 
        io.FontGlobalScale = 1.0f ;

    }

    // Load window icon from assets/icon.svg
    {
        std::string icon_path;
        char* base = SDL_GetBasePath();
        if (base) {
            icon_path = std::string(base) + "assets/icon.svg";
            SDL_free(base);
        }
        NSVGimage* svg = nullptr;
        if (!icon_path.empty())
            svg = nsvgParseFromFile(icon_path.c_str(), "px", 96.0f);
        if (!svg)
            svg = nsvgParseFromFile("../assets/icon.svg", "px", 96.0f);
        if (!svg)
            svg = nsvgParseFromFile("assets/icon.svg", "px", 96.0f);
        if (svg) {
            const int icon_size = 64;
            NSVGrasterizer* rast = nsvgCreateRasterizer();
            if (rast) {
                unsigned char* img = new unsigned char[icon_size * icon_size * 4];
                nsvgRasterize(rast, svg, 0, 0,
                             icon_size / svg->width,
                             img, icon_size, icon_size,
                             icon_size * 4);

                SDL_Surface* icon = SDL_CreateRGBSurfaceFrom(
                    img, icon_size, icon_size, 32, icon_size * 4,
                    0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
                if (icon) {
                    SDL_SetWindowIcon(window_, icon);
                    SDL_FreeSurface(icon);
                }
                delete[] img;
                nsvgDeleteRasterizer(rast);
            }
            nsvgDelete(svg);
        } else {
            std::cerr << "Warning: Could not load assets/icon.svg" << std::endl;
        }
    }

    ImGui_ImplSDL2_InitForOpenGL(window_, gl_context_);
    ImGui_ImplOpenGL3_Init(GLSetup::GLSL_VERSION);

    pedal_board_ = std::make_unique<PedalBoard>(engine_, command_history_, &gui_midi_);
    gui_presets_.set_pedal_board(pedal_board_.get());
    gui_presets_.set_midi_manager(&midi_manager_);

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

} // namespace Amplitron
