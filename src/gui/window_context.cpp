#include "gui/window_context.h"

#include <SDL2/SDL.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>

#include <iostream>

#include "gui/gl_setup.h"
#include "gui/state/gui_graph_state.h"
#include "gui/theme/theme.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
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

WindowContext::WindowContext() = default;

WindowContext::~WindowContext() { shutdown(); }

#ifdef AMPLITRON_HEADLESS
bool g_mock_window_context_initialize_fail = false;
bool g_mock_window_context_poll_events_fail = false;
#endif

bool WindowContext::initialize(int width, int height, const std::string& title) {
    (void)title;
#ifdef AMPLITRON_HEADLESS
    if (g_mock_window_context_initialize_fail) {
        return false;
    }
    width_ = width;
    height_ = height;
    initialized_ = true;
    return true;
#else
    width_ = width;
    height_ = height;

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

    window_ = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               width_, height_,
                               SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

    if (!window_) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return false;
    }

    gl_context_ = SDL_GL_CreateContext(window_);
    if (!gl_context_) {
        std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        SDL_Quit();
        return false;
    }
    SDL_GL_MakeCurrent(window_, gl_context_);
    SDL_GL_SetSwapInterval(1);  // vsync

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    Theme::ApplyStyle();

    if (window_) {
        SDL_GetWindowSize(window_, &width_, &height_);
    }

    load_fonts();
    load_icon();

    ImGui_ImplSDL2_InitForOpenGL(window_, gl_context_);
    ImGui_ImplOpenGL3_Init(GLSetup::GLSL_VERSION);

    initialized_ = true;
    return true;
#endif
}

#ifndef AMPLITRON_HEADLESS
void WindowContext::load_fonts() {
    dpi_scale_ = 1.0f;
    int draw_w = width_, draw_h = height_;
    SDL_GL_GetDrawableSize(window_, &draw_w, &draw_h);
    if (width_ > 0) {
        dpi_scale_ = static_cast<float>(draw_w) / static_cast<float>(width_);
    }

#ifdef __EMSCRIPTEN__
    if (dpi_scale_ <= 1.0f) {
        dpi_scale_ = emscripten_get_device_pixel_ratio();
        if (dpi_scale_ <= 0.0f) {
            dpi_scale_ = 1.0f;
        }
    }
#endif

    GuiGraphState::get_instance().dpi_scale = dpi_scale_;
    ImGuiIO& io = ImGui::GetIO();

    const float base_font_size = 14.0f;
    const float scaled_size = base_font_size;

    ImFont* loaded_font = nullptr;
    auto try_font = [&](const std::string& path) {
        if (!loaded_font) {
            loaded_font = io.Fonts->AddFontFromFileTTF(path.c_str(), scaled_size);
        }
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
    } else {
        io.FontGlobalScale = 1.0f;
    }
}

void WindowContext::load_icon() {
    std::string icon_path;
    char* base = SDL_GetBasePath();
    if (base) {
        icon_path = std::string(base) + "assets/icon.svg";
        SDL_free(base);
    }
    NSVGimage* svg = nullptr;
    if (!icon_path.empty()) {
        svg = nsvgParseFromFile(icon_path.c_str(), "px", 96.0f);
    }
    if (!svg) {
        svg = nsvgParseFromFile("../assets/icon.svg", "px", 96.0f);
    }
    if (!svg) {
        svg = nsvgParseFromFile("assets/icon.svg", "px", 96.0f);
    }

    if (svg) {
        const int icon_size = 64;
        NSVGrasterizer* rast = nsvgCreateRasterizer();
        if (rast) {
            unsigned char* img = new unsigned char[icon_size * icon_size * 4];
            nsvgRasterize(rast, svg, 0, 0, icon_size / svg->width, img, icon_size, icon_size,
                          icon_size * 4);

            SDL_Surface* icon =
                SDL_CreateRGBSurfaceFrom(img, icon_size, icon_size, 32, icon_size * 4, 0x000000FF,
                                         0x0000FF00, 0x00FF0000, 0xFF000000);
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
#endif

void WindowContext::shutdown() {
    if (!initialized_) return;
    initialized_ = false;

#ifndef AMPLITRON_HEADLESS
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
#endif
}

bool WindowContext::poll_events() {
#ifdef AMPLITRON_HEADLESS
    if (g_mock_window_context_poll_events_fail) {
        return false;
    }
    return true;
#else
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);
        if (event.type == SDL_QUIT) return false;
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
            event.window.windowID == SDL_GetWindowID(window_))
            return false;
    }
    if (window_) {
        SDL_GetWindowSize(window_, &width_, &height_);
    }
    return true;
#endif
}

void WindowContext::begin_frame() {
#ifndef AMPLITRON_HEADLESS
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
#endif
}

void WindowContext::end_frame() {
#ifndef AMPLITRON_HEADLESS
    ImGui::Render();
    int display_w, display_h;
    SDL_GL_GetDrawableSize(window_, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.078f, 0.071f, 0.063f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window_);
#endif
}

}  // namespace Amplitron
