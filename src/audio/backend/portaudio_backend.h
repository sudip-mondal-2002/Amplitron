#pragma once

#include <portaudio.h>

#include "audio/backend/i_audio_backend.h"

namespace Amplitron {

class PortAudioBackend : public IAudioBackend {
   public:
    PortAudioBackend();
    ~PortAudioBackend() override;

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

    int get_input_device() const override { return input_device_; }
    int get_output_device() const override { return output_device_; }

   private:
    void auto_detect_devices();

    IAudioEngine* engine_ = nullptr;
    PaStream* stream_ = nullptr;
    bool initialized_ = false;
    bool running_ = false;
    int input_device_ = -1;
    int output_device_ = -1;
};

}  // namespace Amplitron
