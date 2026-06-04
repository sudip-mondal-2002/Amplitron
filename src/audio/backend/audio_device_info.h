#pragma once

#include <string>

namespace Amplitron {

struct AudioDeviceInfo {
    int index = -1;
    std::string name;
    int max_input_channels = 0;
    int max_output_channels = 0;
    double default_sample_rate = 0.0;
    bool is_usb_device = false;
};

} // namespace Amplitron
