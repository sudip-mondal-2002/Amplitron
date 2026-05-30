// =============================================================================
// PortAudio backend — native desktop (Linux, macOS, Windows)
//
// Provides the AudioBackendState factory/destructor, helper functions,
// auto-device detection, and the audio callback.
// =============================================================================

#include "audio/engine/audio_engine.h"
#include "audio/backend/audio_backend.h"
#include "audio/backend/audio_backend_portaudio_helpers.h"
#include "audio/backend/audio_backend_portaudio_internal.h"
#include <portaudio.h>
#ifdef _WIN32
#include <pa_win_wasapi.h>
#endif
#include <cstring>
#include <iostream>
#include <cctype>
#include <algorithm>
#include <vector>

namespace Amplitron {

// AudioBackendState is defined in audio_backend_portaudio_internal.h

AudioBackendState* create_audio_backend() {
    return new AudioBackendState();
}

void destroy_audio_backend(AudioBackendState* state) {
    delete state;
}

// -----------------------------------------------------------------------------
// Helper functions — promoted to non-static for use across TUs
// -----------------------------------------------------------------------------

bool is_usb_device_name(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    static const char* usb_keywords[] = {
        "usb", "guitar", "guitar link", "irig", "scarlett",
        "behringer", "focusrite", "presonus", "steinberg",
        "audio interface", "line 6", "rocksmith", "umc",
        "um2", "uphoria", "podcast", "xenyx"
    };

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
        case paJACK:          return 100;
        case paALSA:          return 70;
        default:              return 10;
    }
#elif defined(_WIN32)
    switch (type) {
        case paASIO:          return 100;
        case paWASAPI:        return 90;
        case paDirectSound:   return 40;
        case paMME:           return 10;
        default:              return 20;
    }
#elif defined(__APPLE__)
    switch (type) {
        case paCoreAudio:     return 100;
        default:              return 30;
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
    return lower.find("epson") != std::string::npos
        || lower.find("projector") != std::string::npos
        || lower.find("hdmi") != std::string::npos
        || lower.find("displayport") != std::string::npos;
}

bool devices_share_host_api(int input_dev, int output_dev) {
    const PaDeviceInfo* in_info = Pa_GetDeviceInfo(input_dev);
    const PaDeviceInfo* out_info = Pa_GetDeviceInfo(output_dev);
    if (!in_info || !out_info) return false;
    return in_info->hostApi == out_info->hostApi;
}

/** @brief Auto-detect the best input/output device pair. */

} // namespace Amplitron
