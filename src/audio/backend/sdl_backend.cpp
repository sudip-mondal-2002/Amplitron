#include "audio/backend/sdl_backend.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstring>
#include <iostream>
#include <vector>

#include "audio/engine/i_audio_engine.h"

namespace Amplitron {

// Intentionally non-static so it can be accessed in unit tests for validation
void sdl_audio_callback(void* userdata, Uint8* stream, int len) {
    auto* be = static_cast<SdlBackend*>(userdata);
    auto* engine = be->get_engine();
    if (!engine) return;

    auto* out = reinterpret_cast<float*>(stream);
    int frame_count = len / static_cast<int>(2 * sizeof(float));

    auto& cap = be->get_capture_buffer();
    if (frame_count > static_cast<int>(cap.size())) {
        cap.resize(frame_count * 2, 0.0f);
    }

    SDL_AudioDeviceID cap_dev = be->get_capture_device();
    if (cap_dev != 0) {
        Uint32 queued = SDL_GetQueuedAudioSize(cap_dev);
        Uint32 need = static_cast<Uint32>(frame_count * sizeof(float));

        Uint32 max_queued = need * 2;
        while (queued > max_queued) {
            Uint8 junk[4096];
            Uint32 chunk = (queued - need) > 4096 ? 4096 : (queued - need);
            SDL_DequeueAudio(cap_dev, junk, chunk);
            queued -= chunk;
        }

        Uint32 got = SDL_DequeueAudio(cap_dev, cap.data(), need);
        int captured = static_cast<int>(got / sizeof(float));
        if (captured < frame_count)
            std::memset(cap.data() + captured, 0,
                        static_cast<size_t>(frame_count - captured) * sizeof(float));
    } else {
        std::memset(cap.data(), 0, static_cast<size_t>(frame_count) * sizeof(float));
    }

    engine->process_audio(cap.data(), out, frame_count);
}

SdlBackend::SdlBackend() = default;
SdlBackend::~SdlBackend() { shutdown(); }

bool SdlBackend::initialize(IAudioEngine* engine) {
    if (initialized_) return true;
    engine_ = engine;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        std::cerr << "SDL audio init failed: " << SDL_GetError() << std::endl;
        return false;
    }
    initialized_ = true;
    return true;
}

void SdlBackend::shutdown() {
    stop();
    if (initialized_) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        initialized_ = false;
    }
}

bool SdlBackend::start() {
    if (!initialized_ || running_) return false;

    int target_buffer = engine_->get_buffer_size();
#ifdef __EMSCRIPTEN__
    if (target_buffer < 256) {
        target_buffer = 256;
    } else {
        int p = 256;
        while (p < target_buffer && p < 16384) {
            p *= 2;
        }
        target_buffer = p;
    }
#endif
    int target_rate = engine_->get_sample_rate();

    SDL_AudioSpec want_out, have_out;
    SDL_memset(&want_out, 0, sizeof(want_out));
    want_out.freq = target_rate;
    want_out.format = AUDIO_F32;
    want_out.channels = 2;
    want_out.samples = static_cast<Uint16>(target_buffer);
    want_out.callback = sdl_audio_callback;
    want_out.userdata = this;

    audio_device_ =
        SDL_OpenAudioDevice(nullptr, 0, &want_out, &have_out, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (audio_device_ == 0) {
        std::cerr << "[SDL] Failed to open output audio: " << SDL_GetError() << std::endl;
        return false;
    }

    sample_rate_ = have_out.freq;
    buffer_size_ = have_out.samples;

    SDL_AudioSpec want_cap, have_cap;
    SDL_memset(&want_cap, 0, sizeof(want_cap));
    want_cap.freq = sample_rate_;
    want_cap.format = AUDIO_F32;
    want_cap.channels = 1;
    want_cap.samples = static_cast<Uint16>(buffer_size_);
    want_cap.callback = nullptr;

    static const char* usb_keywords[] = {
        "usb",       "guitar",   "irig",      "scarlett",        "behringer",
        "focusrite", "presonus", "steinberg", "audio interface", "line 6",
        "rocksmith", "umc",      "um2",       "uphoria",         "podcast",
        "xenyx",     "external"};

    const char* preferred_device = nullptr;
    int num_capture = SDL_GetNumAudioDevices(1);
    for (int i = 0; i < num_capture; ++i) {
        const char* name = SDL_GetAudioDeviceName(i, 1);
        if (!name) continue;
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        for (const auto* kw : usb_keywords) {
            if (lower.find(kw) != std::string::npos) {
                preferred_device = name;
                break;
            }
        }
        if (preferred_device) break;
    }

    capture_device_ = SDL_OpenAudioDevice(preferred_device, 1, &want_cap, &have_cap,
                                          SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);

    capture_buffer_.resize(static_cast<size_t>(buffer_size_ * 2), 0.0f);

    SDL_PauseAudioDevice(audio_device_, 0);
    if (capture_device_ != 0) SDL_PauseAudioDevice(capture_device_, 0);

    running_ = true;
    return true;
}

void SdlBackend::stop() {
    if (running_) {
        if (audio_device_ != 0) SDL_PauseAudioDevice(audio_device_, 1);
        if (capture_device_ != 0) SDL_PauseAudioDevice(capture_device_, 1);
        running_ = false;
    }
    if (capture_device_ != 0) {
        SDL_CloseAudioDevice(capture_device_);
        capture_device_ = 0;
    }
    if (audio_device_ != 0) {
        SDL_CloseAudioDevice(audio_device_);
        audio_device_ = 0;
    }
}

std::vector<AudioDeviceInfo> SdlBackend::get_input_devices() const {
    return {{0, "Browser Microphone", 1, 0, 48000.0, false}};
}

std::vector<AudioDeviceInfo> SdlBackend::get_output_devices() const {
    return {{0, "Browser Audio Output", 0, 2, 48000.0, false}};
}

bool SdlBackend::set_input_device(int device_index) {
    selected_input_device_ = device_index;
    return true;
}

bool SdlBackend::set_output_device(int device_index) {
    selected_output_device_ = device_index;
    return true;
}

std::string SdlBackend::get_input_device_name() const { return "Browser Microphone"; }

std::string SdlBackend::get_output_device_name() const { return "Browser Audio Output"; }

int SdlBackend::get_sample_rate() const { return sample_rate_; }

int SdlBackend::get_buffer_size() const { return buffer_size_; }

}  // namespace Amplitron
