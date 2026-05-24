// =============================================================================
// JACK backend — Linux low-latency audio server
// Minimal implementation: registers JACK client, creates in/out ports, and
// provides create_audio_backend()/destroy_audio_backend() factory functions.
// Compiled only when -DWITH_JACK=ON and JACK headers/libs are available.
// =============================================================================

#include "audio/audio_backend.h"
#include "audio/audio_engine.h"
#include <iostream>
#ifdef WITH_JACK
#include <jack/jack.h>
#endif

namespace Amplitron
{

#ifdef WITH_JACK
    struct AudioBackendState
    {
        jack_client_t *client = nullptr;
        jack_port_t *in_port = nullptr;
        jack_port_t *out_port = nullptr;
    };

    static int jack_process(jack_nframes_t nframes, void *arg)
    {
        auto *s = static_cast<AudioBackendState *>(arg);
        if (!s || !s->in_port || !s->out_port)
            return 0;

        float *in = static_cast<float *>(jack_port_get_buffer(s->in_port, nframes));
        float *out = static_cast<float *>(jack_port_get_buffer(s->out_port, nframes));

        // For now, pass-through (copy input to output). The AudioEngine expects
        // the platform backend to feed audio into the engine via its own callback
        // wiring; a full integration will forward buffers into AudioEngine's
        // processing path.
        if (in && out)
        {
            for (jack_nframes_t i = 0; i < nframes; ++i)
            {
                out[i] = in[i];
            }
        }

        return 0;
    }

    AudioBackendState *create_audio_backend()
    {
        AudioBackendState *s = new AudioBackendState();
        jack_status_t status = static_cast<jack_status_t>(0);
        s->client = jack_client_open("Amplitron", JackNoStartServer, &status);
        if (!s->client)
        {
            std::cerr << "[Amplitron] JACK: could not open JACK server (is jackd running?)." << std::endl;
            // Return a state object without an active client so destroy is safe.
            return s;
        }

        s->in_port = jack_port_register(s->client, "in_1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
        s->out_port = jack_port_register(s->client, "out_1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

        jack_set_process_callback(s->client, jack_process, s);

        if (jack_activate(s->client))
        {
            std::cerr << "[Amplitron] JACK: failed to activate client." << std::endl;
        }
        else
        {
            std::cerr << "[Amplitron] JACK backend initialised." << std::endl;
        }

        return s;
    }

    void destroy_audio_backend(AudioBackendState *state)
    {
        if (!state)
            return;
        if (state->client)
        {
            jack_client_close(state->client);
        }
        delete state;
    }
#else
    // Fallback stub when built without JACK; should never be compiled in this TU
    // unless WITH_JACK is defined in CMake.
    AudioBackendState *create_audio_backend() { return nullptr; }
    void destroy_audio_backend(AudioBackendState *state) { (void)state; }
#endif

} // namespace Amplitron
