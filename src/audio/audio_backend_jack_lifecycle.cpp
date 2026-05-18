// JACK backend — lifecycle and process callback
#include "audio/audio_backend.h"
#include "audio/audio_engine.h"
#include "audio/audio_backend_jack_internal.h"
#include <jack/jack.h>
#include <iostream>
#include <cstring>

namespace Amplitron
{

    // JACK process callback
    int jack_process(jack_nframes_t nframes, void *arg)
    {
        AudioEngine *engine = static_cast<AudioEngine *>(arg);
        AudioBackendState *be = engine->backend_;
        if (!be || !be->input_port || !be->output_port)
            return 0;

        float *in = static_cast<float *>(jack_port_get_buffer(be->input_port, nframes));
        float *out = static_cast<float *>(jack_port_get_buffer(be->output_port, nframes));

        if (!in || !out)
        {
            if (out)
                std::memset(out, 0, nframes * sizeof(float));
            return 0;
        }

        engine->process_audio(in, out, static_cast<int>(nframes));
        return 0;
    }

    bool AudioEngine::initialize()
    {
        // Try to open JACK client without auto-starting server
        const char *client_name = "Amplitron";
        jack_options_t options = JackNoStartServer;
        jack_status_t status;

        // Create backend state if not present
        if (!backend_)
            backend_ = create_audio_backend();

        backend_->client = jack_client_open(client_name, options, &status, nullptr);
        if (!backend_->client)
        {
            std::cerr << "JACK: Could not connect to JACK server (is it running?)" << std::endl;
            destroy_audio_backend(backend_);
            backend_ = nullptr;
            return false;
        }

        // Register process callback
        if (jack_set_process_callback(backend_->client, jack_process, this) != 0)
        {
            std::cerr << "JACK: Failed to set process callback" << std::endl;
            jack_client_close(backend_->client);
            backend_->client = nullptr;
            destroy_audio_backend(backend_);
            backend_ = nullptr;
            return false;
        }

        // Register ports
        backend_->input_port = jack_port_register(backend_->client, "in_1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
        backend_->output_port = jack_port_register(backend_->client, "out_1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
        if (!backend_->input_port || !backend_->output_port)
        {
            std::cerr << "JACK: Failed to register ports" << std::endl;
            jack_client_close(backend_->client);
            backend_->client = nullptr;
            destroy_audio_backend(backend_);
            backend_ = nullptr;
            return false;
        }

        // Retrieve sample rate and buffer size from JACK server
        sample_rate_ = jack_get_sample_rate(backend_->client);
        buffer_size_ = static_cast<int>(jack_get_buffer_size(backend_->client));

        initialized_ = true;
        std::cout << "[JACK] Audio subsystem initialized. Rate=" << sample_rate_ << " Buffer=" << buffer_size_ << std::endl;
        return true;
    }

    void AudioEngine::shutdown()
    {
        stop();
        if (backend_)
        {
            if (backend_->client)
            {
                jack_client_close(backend_->client);
                backend_->client = nullptr;
            }
            destroy_audio_backend(backend_);
            backend_ = nullptr;
        }
        initialized_ = false;
    }

    bool AudioEngine::start()
    {
        if (!initialized_ || running_)
            return false;
        if (!backend_ || !backend_->client)
            return false;

        if (jack_activate(backend_->client) != 0)
        {
            std::cerr << "JACK: Failed to activate client" << std::endl;
            return false;
        }

        running_ = true;
        std::cout << "[JACK] Audio stream started" << std::endl;
        return true;
    }

    void AudioEngine::stop()
    {
        if (initialized_ && running_)
        {
            if (backend_ && backend_->client)
            {
                jack_deactivate(backend_->client);
            }
            running_ = false;
        }
    }

    bool AudioEngine::restart()
    {
        stop();
        bool ok = start();
        if (!ok)
        {
            last_error_ = "Failed to restart audio stream. Check JACK server and device settings.";
            std::cerr << "[Amplitron] " << last_error_ << std::endl;
        }
        else
        {
            last_error_.clear();
        }
        return ok;
    }

    // Device name stubs
    std::string AudioEngine::get_input_device_name() const { return "JACK Input"; }
    std::string AudioEngine::get_output_device_name() const { return "JACK Output"; }

    std::vector<AudioDeviceInfo> AudioEngine::get_input_devices() const { return {{0, "JACK Input", 1, 0, (double)sample_rate_, false}}; }

    std::vector<AudioDeviceInfo> AudioEngine::get_output_devices() const { return {{0, "JACK Output", 0, 1, (double)sample_rate_, false}}; }

    bool AudioEngine::set_input_device(int) { return true; }
    bool AudioEngine::set_output_device(int) { return true; }

} // namespace Amplitron
