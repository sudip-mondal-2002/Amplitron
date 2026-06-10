#pragma once

#include <string>
#include <vector>

#include "audio/backend/audio_device_info.h"

namespace Amplitron {

class IAudioEngine;

class IAudioBackend {
   public:
    virtual ~IAudioBackend() = default;

    virtual bool initialize(IAudioEngine* engine) = 0;
    virtual void shutdown() = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;

    virtual std::vector<AudioDeviceInfo> get_input_devices() const = 0;
    virtual std::vector<AudioDeviceInfo> get_output_devices() const = 0;

    virtual bool set_input_device(int device_index) = 0;
    virtual bool set_output_device(int device_index) = 0;

    virtual std::string get_input_device_name() const = 0;
    virtual std::string get_output_device_name() const = 0;

    virtual int get_sample_rate() const = 0;
    virtual int get_buffer_size() const = 0;

    virtual int get_input_device() const = 0;
    virtual int get_output_device() const = 0;
};

}  // namespace Amplitron
