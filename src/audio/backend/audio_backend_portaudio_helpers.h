#pragma once

#include <string>

namespace Amplitron {

// Forward-declared helpers, defined in audio_backend_portaudio.cpp
bool is_usb_device_name(const std::string& name);
int get_host_api_priority(int host_api_type);
bool is_projector_or_hdmi(const std::string& name);
bool devices_share_host_api(int input_dev, int output_dev);

} // namespace Amplitron
