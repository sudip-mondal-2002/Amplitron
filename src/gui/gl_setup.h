#pragma once

// =============================================================================
// Platform-specific OpenGL / GLSL configuration
//
// Centralises the GL context attributes and shader version string so that
// gui_manager.cpp (and any future rendering code) stays free of #ifdefs.
// =============================================================================

#include <SDL.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

// OpenGL ES 3.0 on Web (Emscripten), Android, and iOS.
// Desktop (Windows / macOS / Linux) uses OpenGL Core 3.3.
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
#include <GLES3/gl3.h>
#elif defined(__APPLE__) && defined(TARGET_OS_IOS) && TARGET_OS_IOS
#include <OpenGLES/ES3/gl.h>
#else
#include <SDL_opengl.h>
#endif

namespace Amplitron {
namespace GLSetup {

#if defined(__EMSCRIPTEN__) || defined(__ANDROID__) || \
    (defined(__APPLE__) && defined(TARGET_OS_IOS) && TARGET_OS_IOS)
inline constexpr int GL_CONTEXT_PROFILE = SDL_GL_CONTEXT_PROFILE_ES;
inline constexpr int GL_MAJOR = 3;
inline constexpr int GL_MINOR = 0;
inline constexpr const char* GLSL_VERSION = "#version 300 es";
#else
inline constexpr int GL_CONTEXT_PROFILE = SDL_GL_CONTEXT_PROFILE_CORE;
inline constexpr int GL_MAJOR = 3;
inline constexpr int GL_MINOR = 3;
inline constexpr const char* GLSL_VERSION = "#version 330";
#endif

}  // namespace GLSetup
}  // namespace Amplitron
