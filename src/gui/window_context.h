#pragma once
#include <string>

struct SDL_Window;
typedef void* SDL_GLContext;
typedef union SDL_Event SDL_Event;

namespace Amplitron {

class WindowContext {
   public:
    WindowContext();
    ~WindowContext();

    bool initialize(int width, int height, const std::string& title);
    void shutdown();

    // Polls SDL events and passes them to ImGui. Returns false if a quit event is received.
    bool poll_events();

    void begin_frame();
    void end_frame();

    SDL_Window* get_window() const { return window_; }
    int get_width() const { return width_; }
    int get_height() const { return height_; }
    float get_dpi_scale() const { return dpi_scale_; }

   private:
    void load_fonts();
    void load_icon();

    SDL_Window* window_ = nullptr;
#ifndef AMPLITRON_HEADLESS
    SDL_GLContext gl_context_ = nullptr;
#endif
    int width_ = 1280;
    int height_ = 720;
    float dpi_scale_ = 1.0f;
    bool initialized_ = false;
};

}  // namespace Amplitron
