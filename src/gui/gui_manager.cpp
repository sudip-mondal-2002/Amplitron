#include "gui/gui_manager.h"
#include "gui/pedal_board.h"
#include "gui/theme.h"
#include "gui/file_dialog.h"
#include "gui/command.h"
#include "gui/gui_graph_state.h"
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
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"
#pragma GCC diagnostic pop

namespace Amplitron {

GuiManager::GuiManager(AudioEngine& engine)
    : engine_(engine),
      command_history_(),
      gui_settings_(engine),
      gui_presets_(engine, command_history_),
      gui_recording_(engine),
      gui_tuner_(engine, std::make_shared<TunerPedal>()),
      gui_analyzer_(engine),
      gui_snapshots_(engine, command_history_), // <-- UNCOMMENT THIS FIELD HERE!
      gui_midi_(midi_manager_) 
{
    pedal_board_ = std::make_unique<PedalBoard>(engine_, command_history_, &gui_midi_);
    gui_presets_.set_pedal_board(pedal_board_.get());
    gui_presets_.set_midi_manager(&midi_manager_);
    
    gui_snapshots_.set_pedal_board(pedal_board_.get()); // <-- UNCOMMENT THIS ACTION TOO!
}

GuiManager::~GuiManager() {
    shutdown();
}

bool GuiManager::initialize(int width, int height) {
    window_width_ = width;
    window_height_ = height;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, GLSetup::GL_CONTEXT_PROFILE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, GLSetup::GL_MAJOR);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, GLSetup::GL_MINOR);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    window_ = SDL_CreateWindow(
        Theme::WINDOW_TITLE,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        window_width_, window_height_,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );

    if (!window_) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        return false;
    }

    gl_context_ = SDL_GL_CreateContext(window_);
    if (!gl_context_) {
        std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << std::endl;
        return false;
    }
    SDL_GL_MakeCurrent(window_, gl_context_);
    SDL_GL_SetSwapInterval(1); // vsync

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
    {
        int draw_w = window_width_, draw_h = window_height_;
        SDL_GL_GetDrawableSize(window_, &draw_w, &draw_h);
        if (window_width_ > 0)
            dpi_scale = static_cast<float>(draw_w) / static_cast<float>(window_width_);
    }

#ifdef __EMSCRIPTEN__
    // If SDL didn't pick up a high DPI scaling factor inside the browser, fallback safely
    if (dpi_scale <= 1.0f) {
        dpi_scale = emscripten_get_device_pixel_ratio();
        if (dpi_scale <= 0.0f) dpi_scale = 1.0f;
    }
#endif

    GuiGraphState::get_instance().dpi_scale = dpi_scale;

    {
        const float base_font_size = 14.0f;
        const float scaled_size    = base_font_size * dpi_scale;

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
        try_font("../assets/fonts/Roboto-Medium.ttf");
        try_font("external/imgui/misc/fonts/Roboto-Medium.ttf");
        try_font("../external/imgui/misc/fonts/Roboto-Medium.ttf");

        if (!loaded_font)
            io.Fonts->AddFontDefault();

        // On desktop platforms (like macOS), SDL uses logical coordinates for ImGui, 
        // so we shouldn't scale the style sizes (padding, margins, etc.) by dpi_scale.
#ifdef __EMSCRIPTEN__
        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(dpi_scale);
        // On web viewports, setting FontGlobalScale smaller shrinks text rendering. 
        // We set it to 1.0f here so that font rendering uses the high-res texture space fully.
        io.FontGlobalScale = 1.0f;
#else
        io.FontGlobalScale = 1.0f / dpi_scale;
#endif
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
    gui_snapshots_.set_pedal_board(pedal_board_.get());

    PresetManager::load_config();

    // MIDI: load config first; if no saved mappings, install defaults
    midi_manager_.load_config();
    if (midi_manager_.mappings().empty()) {
        midi_manager_.install_default_mappings();
    }
    midi_manager_.initialize();

#ifndef AMPLITRON_NO_DESKTOP_SHELL
    update_check_thread_ = std::thread([this]() { this->check_for_updates(); });
#endif

    initialized_ = true;
    return true;
}


void GuiManager::shutdown() {
    if (!initialized_) return;
    initialized_ = false;

    if (update_check_thread_.joinable()) {
        update_check_thread_.join();
    }

    midi_manager_.save_config();
    midi_manager_.shutdown();

    engine_.clear_tuner_tap();
    pedal_board_.reset();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    if (gl_context_) {
        SDL_GL_DeleteContext(gl_context_);
        gl_context_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

} // namespace Amplitron
