// =============================================================================
// JACK backend — Linux low-latency audio server
// Minimal implementation: registers JACK client, creates in/out ports, and
// provides create_audio_backend()/destroy_audio_backend() factory functions.
// Compiled only when -DWITH_JACK=ON and JACK headers/libs are available.
// =============================================================================

#include "audio/backend/audio_backend.h"
#include "audio/engine/audio_engine.h"
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
        bool ports_registered = false;
        AudioEngine *engine = nullptr;
    };

    static bool has_active_client(const AudioBackendState *state)
    {
        return state && state->client;
    }

    static int jack_process(jack_nframes_t nframes, void *arg)
    {
        auto *state = static_cast<AudioBackendState *>(arg);
        if (!state || !state->engine)
            return 0;

        if (!state->in_port || !state->out_port)
            return 0;

        float *in = static_cast<float *>(jack_port_get_buffer(state->in_port, nframes));
        float *out = static_cast<float *>(jack_port_get_buffer(state->out_port, nframes));
        if (in && out)
        {
            state->engine->process_audio(in, out, static_cast<int>(nframes));
        }

        return 0;
    }

    static bool ensure_ports_registered(AudioBackendState *state, AudioEngine *engine)
    {
        if (!state || !state->client || state->ports_registered)
            return state && state->ports_registered;

        state->in_port = jack_port_register(state->client, "in_1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
        state->out_port = jack_port_register(state->client, "out_1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
        if (!state->in_port || !state->out_port)
        {
            std::cerr << "[Amplitron] JACK: failed to register in/out ports." << std::endl;
            return false;
        }

        state->engine = engine;
        jack_set_process_callback(state->client, jack_process, state);
        state->ports_registered = true;
        return true;
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

        return s;
    }

#ifdef AMPLITRON_TESTS
    AudioBackendState *create_disconnected_audio_backend_for_test()
    {
        return new AudioBackendState();
    }

    bool jack_backend_has_active_client_for_test(const AudioBackendState *state)
    {
        return has_active_client(state);
    }
#endif

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

    bool AudioEngine::initialize()
    {
        if (initialized_)
            return true;

        if (!backend_)
        {
            backend_ = create_audio_backend();
        }

        initialized_ = true;
        last_error_.clear();
        return true;
    }

    void AudioEngine::shutdown()
    {
        stop();

        auto *state = static_cast<AudioBackendState *>(backend_);
        if (state && state->client)
        {
            jack_client_close(state->client);
            state->client = nullptr;
            state->in_port = nullptr;
            state->out_port = nullptr;
            state->ports_registered = false;
        }

        initialized_ = false;
    }

    bool AudioEngine::start()
    {
        if (!initialized_ || running_)
            return false;

        auto *state = static_cast<AudioBackendState *>(backend_);
        if (!has_active_client(state))
        {
            last_error_ = "JACK backend is not connected.";
            return false;
        }

        if (!ensure_ports_registered(state, this))
        {
            last_error_ = "Failed to initialise JACK ports.";
            return false;
        }
        if (jack_activate(state->client))
        {
            last_error_ = "Failed to activate JACK client.";
            return false;
        }

        running_ = true;
        last_error_.clear();
        return true;
    }

    void AudioEngine::stop()
    {
        auto *state = static_cast<AudioBackendState *>(backend_);
        if (state && state->client && running_)
        {
            jack_deactivate(state->client);
        }
        running_ = false;
    }

    bool AudioEngine::restart()
    {
        stop();
        bool ok = start();
        if (!ok)
        {
            last_error_ = "Failed to restart JACK audio.";
            std::cerr << "[Amplitron] " << last_error_ << std::endl;
        }
        return ok;
    }

    std::string AudioEngine::get_input_device_name() const { return "JACK in_1"; }
    std::string AudioEngine::get_output_device_name() const { return "JACK out_1"; }

    std::vector<AudioDeviceInfo> AudioEngine::get_input_devices() const
    {
        return {{0, "JACK in_1", 1, 0, static_cast<double>(sample_rate_), false}};
    }

    std::vector<AudioDeviceInfo> AudioEngine::get_output_devices() const
    {
        return {{0, "JACK out_1", 0, 1, static_cast<double>(sample_rate_), false}};
    }

    bool AudioEngine::set_input_device(int device_index)
    {
        if (device_index != 0)
        {
            last_error_ = "Invalid JACK input device.";
            return false;
        }
        input_device_ = 0;
        return true;
    }

    bool AudioEngine::set_output_device(int device_index)
    {
        if (device_index != 0)
        {
            last_error_ = "Invalid JACK output device.";
            return false;
        }
        output_device_ = 0;
        return true;
    }
#else
    // Fallback stub when built without JACK; should never be compiled in this TU
    // unless WITH_JACK is defined in CMake.
    AudioBackendState *create_audio_backend() { return nullptr; }
    void destroy_audio_backend(AudioBackendState *state) { (void)state; }
#endif

} // namespace Amplitron
