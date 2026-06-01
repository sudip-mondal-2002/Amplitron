#pragma once

#include <memory>

namespace Amplitron {

/**
 * @brief Opaque, platform-specific audio backend state.
 *
 * Defined in exactly one of the backend compilation units
 * (audio_backend_portaudio.cpp or audio_backend_sdl.cpp).
 * The AudioEngine header only forward-declares it so that
 * no platform headers leak into the public API.
 */
struct AudioBackendState;

/** @brief Allocate and default-initialise the platform backend state. */
AudioBackendState* create_audio_backend();

/** @brief Destroy platform backend state created by create_audio_backend(). */
void destroy_audio_backend(AudioBackendState* state);

} // namespace Amplitron
