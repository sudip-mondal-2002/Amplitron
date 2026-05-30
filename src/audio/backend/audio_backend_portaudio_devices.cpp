// =============================================================================
// PortAudio backend — device management
// =============================================================================

#include "audio/engine/audio_engine.h"
#include "audio/backend/audio_backend.h"
#include "audio/backend/audio_backend_portaudio_helpers.h"
#include "audio/backend/audio_backend_portaudio_internal.h"
#include <portaudio.h>
#include <iostream>
#include <vector>

namespace Amplitron {

std::string AudioEngine::get_input_device_name() const {
    if (input_device_ >= 0) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(input_device_);
        if (info) return info->name;
    }
    return "None";
}

std::string AudioEngine::get_output_device_name() const {
    if (output_device_ >= 0) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(output_device_);
        if (info) return info->name;
    }
    return "None";
}

std::vector<AudioDeviceInfo> AudioEngine::get_input_devices() const {
    std::vector<AudioDeviceInfo> devices;
    int count = Pa_GetDeviceCount();
    for (int i = 0; i < count; ++i) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        if (info && info->maxInputChannels > 0) {
            devices.push_back({
                i, info->name,
                info->maxInputChannels, info->maxOutputChannels,
                info->defaultSampleRate,
                is_usb_device_name(info->name)
            });
        }
    }
    return devices;
}

std::vector<AudioDeviceInfo> AudioEngine::get_output_devices() const {
    std::vector<AudioDeviceInfo> devices;
    int count = Pa_GetDeviceCount();
    for (int i = 0; i < count; ++i) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        if (info && info->maxOutputChannels > 0) {
            devices.push_back({
                i, info->name,
                info->maxInputChannels, info->maxOutputChannels,
                info->defaultSampleRate,
                is_usb_device_name(info->name)
            });
        }
    }
    return devices;
}

bool AudioEngine::set_input_device(int device_index) {
    const PaDeviceInfo* info = Pa_GetDeviceInfo(device_index);
    if (!info || info->maxInputChannels < 1) {
        last_error_ = "Invalid input device.";
        return false;
    }

    const PaDeviceInfo* out_info = Pa_GetDeviceInfo(output_device_);
    if (out_info && info->hostApi != out_info->hostApi) {
        const PaHostApiInfo* in_api = Pa_GetHostApiInfo(info->hostApi);
        const PaHostApiInfo* out_api = Pa_GetHostApiInfo(out_info->hostApi);
        std::cerr << "[Amplitron] Warning: Input (" << (in_api ? in_api->name : "?")
                  << ") and output (" << (out_api ? out_api->name : "?")
                  << ") are on different host APIs. Stream may fail." << std::endl;
    }

    int prev_device = input_device_;
    bool was_running = running_;
    if (was_running) stop();
    input_device_ = device_index;
    if (was_running) {
        if (!start()) {
            last_error_ = "Failed to start with new input device. Reverting.";
            std::cerr << "[Amplitron] " << last_error_ << std::endl;
            input_device_ = prev_device;
            if (!start()) {
                last_error_ = "Failed to revert to previous input device. Engine stopped.";
                std::cerr << "[Amplitron] " << last_error_ << std::endl;
            }
            return false;
        }
        last_error_.clear();
    }
    return true;
}

bool AudioEngine::set_output_device(int device_index) {
    const PaDeviceInfo* info = Pa_GetDeviceInfo(device_index);
    if (!info || info->maxOutputChannels < 1) {
        last_error_ = "Invalid output device.";
        return false;
    }

    const PaDeviceInfo* in_info = Pa_GetDeviceInfo(input_device_);
    if (in_info && in_info->hostApi != info->hostApi) {
        const PaHostApiInfo* in_api = Pa_GetHostApiInfo(in_info->hostApi);
        const PaHostApiInfo* out_api = Pa_GetHostApiInfo(info->hostApi);
        std::cerr << "[Amplitron] Warning: Input (" << (in_api ? in_api->name : "?")
                  << ") and output (" << (out_api ? out_api->name : "?")
                  << ") are on different host APIs. Stream may fail." << std::endl;
    }

    int prev_device = output_device_;
    bool was_running = running_;
    if (was_running) stop();
    output_device_ = device_index;
    if (was_running) {
        if (!start()) {
            last_error_ = "Failed to start with new output device. Reverting.";
            std::cerr << "[Amplitron] " << last_error_ << std::endl;
            output_device_ = prev_device;
            if (!start()) {
                last_error_ = "Failed to revert to previous output device. Engine stopped.";
                std::cerr << "[Amplitron] " << last_error_ << std::endl;
            }
            return false;
        }
        last_error_.clear();
    }
    return true;
}

} // namespace Amplitron
