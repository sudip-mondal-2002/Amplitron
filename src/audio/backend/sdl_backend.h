#pragma once

#include <SDL.h>

#include "audio/backend/i_audio_backend.h"

namespace Amplitron {

class SdlBackend : public IAudioBackend {
   public:
    SdlBackend();
    ~SdlBackend() override;

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

    /**
     * @brief Return the selected input device index.
     * Note: Unlike PortAudioBackend, SDL does not support setting custom input hardware
     * devices dynamically by index under all platforms, but the index is tracked here.
     */
    int get_input_device() const override { return selected_input_device_; }

    /**
     * @brief Return the selected output device index.
     * Note: Unlike PortAudioBackend, SDL does not support setting custom output hardware
     * devices dynamically by index under all platforms, but the index is tracked here.
     */
    int get_output_device() const override { return selected_output_device_; }

    // Callback helpers
    IAudioEngine* get_engine() const { return engine_; }
    SDL_AudioDeviceID get_capture_device() const { return capture_device_; }
    std::vector<float>& get_capture_buffer() { return capture_buffer_; }

   private:
    IAudioEngine* engine_ = nullptr;
    SDL_AudioDeviceID audio_device_ = 0;
    SDL_AudioDeviceID capture_device_ = 0;
    std::vector<float> capture_buffer_;
    bool initialized_ = false;
    bool running_ = false;
    int sample_rate_ = 48000;
    int buffer_size_ = 512;
    int selected_input_device_ = 0;
    int selected_output_device_ = 0;
};

}  // namespace Amplitron
