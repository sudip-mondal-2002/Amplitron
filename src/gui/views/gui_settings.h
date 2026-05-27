#pragma once

#include "audio/engine/audio_engine.h"
#include "gui/ui_component.h"
#include <functional>
#include <string>
#include <vector>

namespace Amplitron {

struct AudioDeviceEntry {
    int index = -1;
    std::string name;
    bool is_usb = false;
};

struct SettingsProps {
    // Routing
    std::string input_device_name;
    std::string output_device_name;
    std::string device_error;

    // Latency
    int  buffer_size     = 128;
    int  sample_rate     = 44100;
    int  suggested_buf   = 128;
    float latency_ms     = 0.0f;
    float cpu_load       = 0.0f;
    bool  auto_buf       = false;

    // Devices
    std::vector<AudioDeviceEntry> input_devices;
    std::vector<AudioDeviceEntry> output_devices;
    int  current_input  = -1;
    int  current_output = -1;

#ifdef AMPLITRON_ANDROID_OBOE
    const char* oboe_mode_label = "";
#endif

    std::function<void(int)>   on_buffer_size_changed;
    std::function<void(int)>   on_sample_rate_changed;
    std::function<void(bool)>  on_auto_buf_changed;
    std::function<void()>      on_clear_error;
    std::function<void(int)>   on_input_device_changed;
    std::function<void(int)>   on_output_device_changed;
};

/**
 * @brief Reactive Audio Settings modal component.
 */
class GuiSettings : public UIComponent<SettingsProps> {
public:
    GuiSettings() = default;

    /** @brief Render the settings window using internal show_ flag. */
    void render() override { render(show_); }

    /** @brief Render with an external show flag (GuiManager is authoritative). */
    void render(bool& show);

private:
    bool show_ = true;
};

} // namespace Amplitron
