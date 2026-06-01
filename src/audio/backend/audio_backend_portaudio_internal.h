#pragma once

#include <portaudio.h>

namespace Amplitron {

/**
 * @brief Opaque state for PortAudio backend.
 * Defined here so all portaudio split files can access it.
 */
struct AudioBackendState {
    PaStream* stream = nullptr;
};

} // namespace Amplitron
