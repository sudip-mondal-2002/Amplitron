// =============================================================================
// SDL audio backend — Emscripten / WebAssembly
//
// Implements AudioEngine member functions: initialize, shutdown, start, stop,
// restart, and device management stubs. Also provides the SDL audio callback
// and the AudioBackendState factory / destructor.
// =============================================================================

#include "audio/engine/audio_engine.h"
#include "audio/backend/audio_backend.h"
#include <SDL.h>
#include <cstring>
#include <iostream>
#include <cctype>
#include <algorithm>

namespace Amplitron {

// -----------------------------------------------------------------------------
// Backend state
// -----------------------------------------------------------------------------

struct AudioBackendState {
    AudioEngine* engine = nullptr;          // back-pointer set by start()
    SDL_AudioDeviceID audio_device = 0;
    SDL_AudioDeviceID capture_device = 0;
    std::vector<float> capture_buffer;
};

AudioBackendState* create_audio_backend() {
    return new AudioBackendState();
}

void destroy_audio_backend(AudioBackendState* state) {
    delete state;
}

// -----------------------------------------------------------------------------
// SDL audio callback (file-local)
// -----------------------------------------------------------------------------

static void sdl_audio_callback(void* userdata, Uint8* stream, int len) {
    auto* be = static_cast<AudioBackendState*>(userdata);
    auto* engine = be->engine;
    auto* out = reinterpret_cast<float*>(stream);
    // SDL delivers len bytes for interleaved stereo; divide by 2 channels
    int frame_count = len / static_cast<int>(2 * sizeof(float));

    auto& cap = be->capture_buffer;
    if (static_cast<int>(cap.size()) < frame_count)
        cap.resize(static_cast<size_t>(frame_count), 0.0f);

    if (be->capture_device != 0) {
        Uint32 queued = SDL_GetQueuedAudioSize(be->capture_device);
        Uint32 need = static_cast<Uint32>(frame_count * sizeof(float));

        // Drain excess to prevent latency buildup (keep at most 2 buffers ahead)
        Uint32 max_queued = need * 2;
        while (queued > max_queued) {
            Uint8 junk[4096];
            Uint32 chunk = (queued - need) > 4096 ? 4096 : (queued - need);
            SDL_DequeueAudio(be->capture_device, junk, chunk);
            queued -= chunk;
        }

        Uint32 got = SDL_DequeueAudio(be->capture_device, cap.data(), need);
        int captured = static_cast<int>(got / sizeof(float));
        if (captured < frame_count)
            std::memset(cap.data() + captured, 0,
                        static_cast<size_t>(frame_count - captured) * sizeof(float));
    } else {
        std::memset(cap.data(), 0, static_cast<size_t>(frame_count) * sizeof(float));
    }

    engine->process_audio(cap.data(), out, frame_count);
}

// =============================================================================
// AudioEngine member functions — SDL / Emscripten implementations
// =============================================================================

bool AudioEngine::initialize() {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        std::cerr << "SDL audio init failed: " << SDL_GetError() << std::endl;
        return false;
    }
    initialized_ = true;
    std::cout << "[Web] Audio subsystem initialized." << std::endl;
    return true;
}

void AudioEngine::shutdown() {
    stop();
    if (initialized_) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        initialized_ = false;
    }
}

bool AudioEngine::start() {
    if (!initialized_ || running_) return false;

    // Web Audio needs buffer 256–16384; use 1024 for glitch-free playback
    int web_buffer = 1024;

    // --- Open output (playback) device with callback ---
    SDL_AudioSpec want_out, have_out;
    SDL_memset(&want_out, 0, sizeof(want_out));
    want_out.freq = sample_rate_;
    want_out.format = AUDIO_F32;
    want_out.channels = 2;
    want_out.samples = static_cast<Uint16>(web_buffer);
    want_out.callback = sdl_audio_callback;
    backend_->engine = this;
    want_out.userdata = backend_;

    backend_->audio_device = SDL_OpenAudioDevice(nullptr, 0, &want_out, &have_out,
                                                  SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (backend_->audio_device == 0) {
        std::cerr << "[Web] Failed to open output audio: " << SDL_GetError() << std::endl;
        return false;
    }

    sample_rate_ = have_out.freq;
    buffer_size_ = have_out.samples;

    // Update all effects with the actual sample rate the browser gave us
    {
        std::lock_guard<std::mutex> lock(effect_mutex_);
        for (auto& node : main_graph_.get_nodes()) {
            if (node.pedal) {
                node.pedal->set_sample_rate(sample_rate_);
            }
        }
    }

    // --- Open capture (microphone) device ---
    SDL_AudioSpec want_cap, have_cap;
    SDL_memset(&want_cap, 0, sizeof(want_cap));
    want_cap.freq = sample_rate_;
    want_cap.format = AUDIO_F32;
    want_cap.channels = 1;
    want_cap.samples = static_cast<Uint16>(web_buffer);
    want_cap.callback = nullptr;  // use SDL_DequeueAudio for capture

    // Keywords matching USB guitar cables / audio interfaces
    static const char* usb_keywords[] = {
        "usb", "guitar", "irig", "scarlett", "behringer", "focusrite",
        "presonus", "steinberg", "audio interface", "line 6", "rocksmith",
        "umc", "um2", "uphoria", "podcast", "xenyx", "external"
    };

    const char* preferred_device = nullptr;
    int num_capture = SDL_GetNumAudioDevices(1);
    std::cout << "[Web] Found " << num_capture << " capture device(s):" << std::endl;
    for (int i = 0; i < num_capture; ++i) {
        const char* name = SDL_GetAudioDeviceName(i, 1);
        if (!name) continue;
        std::cout << "  [" << i << "] " << name << std::endl;

        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        for (const auto* kw : usb_keywords) {
            if (lower.find(kw) != std::string::npos) {
                preferred_device = name;
                std::cout << "  >> Preferred (matches '" << kw << "')" << std::endl;
                break;
            }
        }
        if (preferred_device) break;
    }

    backend_->capture_device = SDL_OpenAudioDevice(preferred_device, 1, &want_cap, &have_cap,
                                                    SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (backend_->capture_device == 0) {
        std::cerr << "[Web] Warning: No microphone available: " << SDL_GetError() << std::endl;
    }

    // Pre-allocate capture buffer
    backend_->capture_buffer.resize(static_cast<size_t>(buffer_size_), 0.0f);

    // Unpause both devices
    SDL_PauseAudioDevice(backend_->audio_device, 0);
    if (backend_->capture_device != 0)
        SDL_PauseAudioDevice(backend_->capture_device, 0);

    running_ = true;

    std::cout << "[Web] Audio stream started:" << std::endl;
    std::cout << "  Rate:   " << sample_rate_ << " Hz" << std::endl;
    std::cout << "  Buffer: " << buffer_size_ << " samples" << std::endl;
    std::cout << "  Mic:    " << (backend_->capture_device ? "yes" : "unavailable") << std::endl;
    return true;
}

void AudioEngine::stop() {
    if (running_) {
        if (backend_->audio_device != 0)
            SDL_PauseAudioDevice(backend_->audio_device, 1);
        if (backend_->capture_device != 0)
            SDL_PauseAudioDevice(backend_->capture_device, 1);
        running_ = false;
    }
    if (backend_->capture_device != 0) {
        SDL_CloseAudioDevice(backend_->capture_device);
        backend_->capture_device = 0;
    }
    if (backend_->audio_device != 0) {
        SDL_CloseAudioDevice(backend_->audio_device);
        backend_->audio_device = 0;
    }
}

bool AudioEngine::restart() {
    stop();
    bool ok = start();
    if (!ok) {
        last_error_ = "Failed to restart audio.";
    } else {
        last_error_.clear();
    }
    return ok;
}

// =============================================================================
// Device management stubs — browser handles devices
// =============================================================================

std::string AudioEngine::get_input_device_name() const { return "Browser Microphone"; }
std::string AudioEngine::get_output_device_name() const { return "Browser Audio Output"; }

std::vector<AudioDeviceInfo> AudioEngine::get_input_devices() const {
    return {{0, "Browser Microphone", 1, 0, 48000.0, false}};
}

std::vector<AudioDeviceInfo> AudioEngine::get_output_devices() const {
    return {{0, "Browser Audio Output", 0, 2, 48000.0, false}};
}

bool AudioEngine::set_input_device(int) { return true; }
bool AudioEngine::set_output_device(int) { return true; }

} // namespace Amplitron
