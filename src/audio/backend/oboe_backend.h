#pragma once

#include "audio/backend/i_audio_backend.h"

namespace Amplitron {

class OboeBackend : public IAudioBackend {
   public:
    OboeBackend();
    ~OboeBackend() override;

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

    const char* get_oboe_sharing_mode_label() const;

   private:
    IAudioEngine* engine_ = nullptr;
    bool initialized_ = false;
    bool running_ = false;
    int sample_rate_ = 48000;
    int buffer_size_ = 512;
    void* impl_ = nullptr;
};

}  // namespace Amplitron
