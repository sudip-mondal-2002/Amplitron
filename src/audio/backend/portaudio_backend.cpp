#include "audio/backend/portaudio_backend.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iostream>
#include <vector>

#include "audio/backend/audio_backend_portaudio_helpers.h"
#include "audio/engine/i_audio_engine.h"
#ifdef _WIN32
#include <pa_win_wasapi.h>
#endif

namespace Amplitron {

// -----------------------------------------------------------------------------
// Helper functions — promoted to non-static for use across TUs
// -----------------------------------------------------------------------------

bool is_usb_device_name(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    static const char* usb_keywords[] = {
        "usb",       "guitar",    "guitar link", "irig",      "scarlett",
        "behringer", "focusrite", "presonus",    "steinberg", "audio interface",
        "line 6",    "rocksmith", "umc",         "um2",       "uphoria",
        "podcast",   "xenyx"};

    for (const auto* keyword : usb_keywords) {
        if (lower.find(keyword) != std::string::npos) {
            return true;
        }
    }
    return false;
}

int get_host_api_priority(int host_api_type) {
    auto type = static_cast<PaHostApiTypeId>(host_api_type);
#if defined(__linux__)
    switch (type) {
        case paJACK:
            return 100;
        case paALSA:
            return 70;
        default:
            return 10;
    }
#elif defined(_WIN32)
    switch (type) {
        case paASIO:
            return 100;
        case paWASAPI:
            return 90;
        case paDirectSound:
            return 40;
        case paMME:
            return 10;
        default:
            return 20;
    }
#elif defined(__APPLE__)
    switch (type) {
        case paCoreAudio:
            return 100;
        default:
            return 30;
    }
#else
    (void)type;
    return 30;
#endif
}

bool is_projector_or_hdmi(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return lower.find("epson") != std::string::npos ||
           lower.find("projector") != std::string::npos ||
           lower.find("hdmi") != std::string::npos ||
           lower.find("displayport") != std::string::npos;
}

bool devices_share_host_api(int input_dev, int output_dev) {
    const PaDeviceInfo* in_info = Pa_GetDeviceInfo(input_dev);
    const PaDeviceInfo* out_info = Pa_GetDeviceInfo(output_dev);
    if (!in_info || !out_info) return false;
    return in_info->hostApi == out_info->hostApi;
}

// -----------------------------------------------------------------------------

// Real callback implementation
int pa_audio_callback(const void* input, void* output, unsigned long frame_count,
                      const PaStreamCallbackTimeInfo* /*time_info*/,
                      PaStreamCallbackFlags /*status_flags*/, void* user_data) {
    auto* engine = static_cast<IAudioEngine*>(user_data);
    const auto* in = static_cast<const float*>(input);
    auto* out = static_cast<float*>(output);

    if (!in || !out) {
        if (out) std::memset(out, 0, frame_count * 2 * sizeof(float));
        return paContinue;
    }

    engine->process_audio(in, out, static_cast<int>(frame_count));
    return paContinue;
}

PortAudioBackend::PortAudioBackend() = default;

PortAudioBackend::~PortAudioBackend() { shutdown(); }

bool PortAudioBackend::initialize(IAudioEngine* engine) {
    if (initialized_) return true;
    engine_ = engine;

    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "PortAudio init failed: " << Pa_GetErrorText(err) << std::endl;
        return false;
    }
    initialized_ = true;

    auto_detect_devices();
    return true;
}

void PortAudioBackend::shutdown() {
    stop();
    if (initialized_) {
        Pa_Terminate();
        initialized_ = false;
    }
}

bool PortAudioBackend::start() {
    if (!initialized_ || running_) return false;

    int buffer_size = engine_->get_buffer_size();
    int sample_rate = engine_->get_sample_rate();
    double desired_latency = static_cast<double>(buffer_size) / sample_rate;

    PaStreamParameters input_params;
    input_params.device = input_device_;
    input_params.channelCount = 1;
    input_params.sampleFormat = paFloat32;
    input_params.suggestedLatency = desired_latency;
    input_params.hostApiSpecificStreamInfo = nullptr;

    PaStreamParameters output_params;
    output_params.device = output_device_;
    output_params.channelCount = 2;
    output_params.sampleFormat = paFloat32;
    output_params.suggestedLatency = desired_latency;
    output_params.hostApiSpecificStreamInfo = nullptr;

#ifdef _WIN32
    PaWasapiStreamInfo wasapi_in_info = {};
    PaWasapiStreamInfo wasapi_out_info = {};
    const PaDeviceInfo* in_dev = Pa_GetDeviceInfo(input_device_);
    if (in_dev) {
        const PaHostApiInfo* api = Pa_GetHostApiInfo(in_dev->hostApi);
        if (api && api->type == paWASAPI) {
            wasapi_in_info.size = sizeof(PaWasapiStreamInfo);
            wasapi_in_info.hostApiType = paWASAPI;
            wasapi_in_info.version = 1;
            wasapi_in_info.flags = paWinWasapiExclusive;
            input_params.hostApiSpecificStreamInfo = &wasapi_in_info;

            wasapi_out_info.size = sizeof(PaWasapiStreamInfo);
            wasapi_out_info.hostApiType = paWASAPI;
            wasapi_out_info.version = 1;
            wasapi_out_info.flags = paWinWasapiExclusive;
            output_params.hostApiSpecificStreamInfo = &wasapi_out_info;

            std::cout << "  Using WASAPI Exclusive Mode" << std::endl;
        }
    }
#endif

    unsigned long frames = static_cast<unsigned long>(buffer_size);

    PaError err = Pa_OpenStream(&stream_, &input_params, &output_params, sample_rate, frames,
                                paClipOff | paDitherOff, pa_audio_callback, engine_);

    if (err != paNoError) {
        std::cerr << "Failed to open PortAudio stream: " << Pa_GetErrorText(err) << std::endl;

        // Adjust parameters before retrying: disable WASAPI exclusive and reset latency suggestions
        input_params.hostApiSpecificStreamInfo = nullptr;
        output_params.hostApiSpecificStreamInfo = nullptr;

        const PaDeviceInfo* in_info = Pa_GetDeviceInfo(input_device_);
        if (in_info) {
            input_params.suggestedLatency = in_info->defaultLowInputLatency;
        } else {
            input_params.suggestedLatency = 0.0;
        }

        const PaDeviceInfo* out_info = Pa_GetDeviceInfo(output_device_);
        if (out_info) {
            output_params.suggestedLatency = out_info->defaultLowOutputLatency;
        } else {
            output_params.suggestedLatency = 0.0;
        }

        // Retry
        err = Pa_OpenStream(&stream_, &input_params, &output_params, sample_rate, buffer_size,
                            paClipOff | paDitherOff, pa_audio_callback, engine_);
        if (err != paNoError) {
            std::cerr << "PortAudio open stream retry failed: " << Pa_GetErrorText(err)
                      << std::endl;
            return false;
        }
    }

    err = Pa_StartStream(stream_);
    if (err != paNoError) {
        std::cerr << "Failed to start PortAudio stream: " << Pa_GetErrorText(err) << std::endl;
        Pa_CloseStream(stream_);
        stream_ = nullptr;
        return false;
    }

    running_ = true;
    return true;
}

void PortAudioBackend::stop() {
    if (stream_) {
        if (running_) {
            Pa_StopStream(stream_);
            running_ = false;
        }
        Pa_CloseStream(stream_);
        stream_ = nullptr;
    }
}

std::vector<AudioDeviceInfo> PortAudioBackend::get_input_devices() const {
    std::vector<AudioDeviceInfo> devices;
    if (!initialized_) return devices;
    int count = Pa_GetDeviceCount();
    for (int i = 0; i < count; ++i) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        if (info && info->maxInputChannels > 0) {
            devices.push_back({i, info->name, info->maxInputChannels, info->maxOutputChannels,
                               info->defaultSampleRate, is_usb_device_name(info->name)});
        }
    }
    return devices;
}

std::vector<AudioDeviceInfo> PortAudioBackend::get_output_devices() const {
    std::vector<AudioDeviceInfo> devices;
    if (!initialized_) return devices;
    int count = Pa_GetDeviceCount();
    for (int i = 0; i < count; ++i) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        if (info && info->maxOutputChannels > 0) {
            devices.push_back({i, info->name, info->maxInputChannels, info->maxOutputChannels,
                               info->defaultSampleRate, is_usb_device_name(info->name)});
        }
    }
    return devices;
}

bool PortAudioBackend::set_input_device(int device_index) {
    const PaDeviceInfo* info = Pa_GetDeviceInfo(device_index);
    if (!info || info->maxInputChannels < 1) {
        return false;
    }
    input_device_ = device_index;
    return true;
}

bool PortAudioBackend::set_output_device(int device_index) {
    const PaDeviceInfo* info = Pa_GetDeviceInfo(device_index);
    if (!info || info->maxOutputChannels < 1) {
        return false;
    }
    output_device_ = device_index;
    return true;
}

std::string PortAudioBackend::get_input_device_name() const {
    if (input_device_ >= 0) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(input_device_);
        if (info) return info->name;
    }
    return "None";
}

std::string PortAudioBackend::get_output_device_name() const {
    if (output_device_ >= 0) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(output_device_);
        if (info) return info->name;
    }
    return "None";
}

int PortAudioBackend::get_sample_rate() const {
    if (stream_) {
        const PaStreamInfo* si = Pa_GetStreamInfo(stream_);
        if (si && si->sampleRate > 0.0) {
            return static_cast<int>(si->sampleRate + 0.5);
        }
    }
    return engine_ ? engine_->get_sample_rate() : 48000;
}

int PortAudioBackend::get_buffer_size() const { return engine_ ? engine_->get_buffer_size() : 512; }

void PortAudioBackend::auto_detect_devices() {
    int device_count = Pa_GetDeviceCount();
    int num_apis = Pa_GetHostApiCount();

    struct ApiCandidate {
        int api_index;
        int priority;
        int usb_input;
        int best_output;
        std::string api_name;
    };

    std::vector<ApiCandidate> candidates;
    for (int a = 0; a < num_apis; ++a) {
        const PaHostApiInfo* api = Pa_GetHostApiInfo(a);
        if (!api) continue;

        ApiCandidate c;
        c.api_index = a;
        c.priority = get_host_api_priority(api->type);
        c.usb_input = -1;
        c.best_output = -1;
        c.api_name = api->name;

        for (int d = 0; d < api->deviceCount; ++d) {
            int dev_idx = Pa_HostApiDeviceIndexToDeviceIndex(a, d);
            const PaDeviceInfo* info = Pa_GetDeviceInfo(dev_idx);
            if (!info) continue;

            bool is_usb = is_usb_device_name(info->name);

            if (is_usb && info->maxInputChannels > 0 && c.usb_input < 0) {
                c.usb_input = dev_idx;
            }
            if (!is_usb && !is_projector_or_hdmi(info->name) && info->maxOutputChannels > 0 &&
                c.best_output < 0) {
                c.best_output = dev_idx;
            }
        }

        if (c.best_output < 0) {
            for (int d = 0; d < api->deviceCount; ++d) {
                int dev_idx = Pa_HostApiDeviceIndexToDeviceIndex(a, d);
                const PaDeviceInfo* info = Pa_GetDeviceInfo(dev_idx);
                if (!info) continue;
                if (!is_usb_device_name(info->name) && info->maxOutputChannels > 0) {
                    c.best_output = dev_idx;
                    break;
                }
            }
        }

        candidates.push_back(c);
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const ApiCandidate& a, const ApiCandidate& b) { return a.priority > b.priority; });

    bool found_pair = false;
    for (auto& c : candidates) {
        if (c.usb_input >= 0 && c.best_output >= 0) {
            input_device_ = c.usb_input;
            output_device_ = c.best_output;
            found_pair = true;
            break;
        }
    }

    if (!found_pair) {
        for (auto& c : candidates) {
            if (c.best_output >= 0) {
                const PaHostApiInfo* api = Pa_GetHostApiInfo(c.api_index);
                for (int d = 0; d < api->deviceCount; ++d) {
                    int dev_idx = Pa_HostApiDeviceIndexToDeviceIndex(c.api_index, d);
                    const PaDeviceInfo* info = Pa_GetDeviceInfo(dev_idx);
                    if (info && info->maxInputChannels > 0) {
                        input_device_ = dev_idx;
                        output_device_ = c.best_output;
                        found_pair = true;
                        break;
                    }
                }
                if (found_pair) break;
            }
        }
    }

    if (!found_pair) {
        input_device_ = Pa_GetDefaultInputDevice();
        output_device_ = Pa_GetDefaultOutputDevice();
    }
}

}  // namespace Amplitron
