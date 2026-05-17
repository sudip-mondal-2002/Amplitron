// Minimal JACK backend factory and helpers
#include "audio/audio_backend.h"
#include "audio/audio_backend_jack_internal.h"
#include <iostream>

namespace Amplitron
{

    AudioBackendState *create_audio_backend()
    {
        return new AudioBackendState();
    }

    void destroy_audio_backend(AudioBackendState *state)
    {
        delete state;
    }

} // namespace Amplitron
