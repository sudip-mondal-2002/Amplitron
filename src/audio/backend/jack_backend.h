#pragma once

#include "audio/backend/i_audio_backend.h"

#ifdef WITH_JACK
#include <jack/jack.h>
#endif

namespace Amplitron {

class JackBackend : public IAudioBackend {
public:
    JackBackend();
    ~JackBackend() override;

    bool initialize(IAudioEngine* engine) override;
    void shutdown() override;
    bool start() override;
    void stop() override;

    std::vector<AudioDeviceInfo> get_input_devices() const override;
    std::vector<AudioDeviceInfo> get_output_devices() const override;

    bool set_input_device(int device_index) override;
    bool set_output_device(int device_index) override;

    std::string get_input_device_name() const override;
    std::string get_output_device_name() const override;

    int get_sample_rate() const override;
    int get_buffer_size() const override;

    int get_input_device() const override { return 0; }
    int get_output_device() const override { return 0; }

#ifdef WITH_JACK
    jack_client_t* get_client() const { return client_; }
    jack_port_t* get_in_port() const { return in_port_; }
    jack_port_t* get_out_port() const { return out_port_; }
    IAudioEngine* get_engine() const { return engine_; }
#endif

private:
    IAudioEngine* engine_ = nullptr;
    bool initialized_ = false;
    bool running_ = false;

#ifdef WITH_JACK
    jack_client_t* client_ = nullptr;
    jack_port_t* in_port_ = nullptr;
    jack_port_t* out_port_ = nullptr;
    bool ports_registered_ = false;
#endif
};

} // namespace Amplitron
