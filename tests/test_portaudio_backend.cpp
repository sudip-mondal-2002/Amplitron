#include "test_framework.h"
#include "audio/audio_engine.h"
#include "audio/audio_backend_portaudio_helpers.h"
#include <portaudio.h>

using namespace Amplitron;

struct PaGuard {
    bool ok;
    PaGuard() {
        ok = (Pa_Initialize() == paNoError);
    }
    ~PaGuard() {
        if (ok) Pa_Terminate();
    }
};

// GROUP 1 — is_usb_device_name (pure string, no hardware):
TEST(is_usb_device_name_usb_keyword) {
    ASSERT_TRUE(is_usb_device_name("USB Audio Device"));
    ASSERT_TRUE(is_usb_device_name("usb microphone"));
}

TEST(is_usb_device_name_guitar_keyword) {
    ASSERT_TRUE(is_usb_device_name("Guitar Link"));
    ASSERT_TRUE(is_usb_device_name("guitar cable v2"));
}

TEST(is_usb_device_name_brands) {
    ASSERT_TRUE(is_usb_device_name("Scarlett"));
    ASSERT_TRUE(is_usb_device_name("Focusrite"));
    ASSERT_TRUE(is_usb_device_name("Behringer"));
    ASSERT_TRUE(is_usb_device_name("iRig"));
    ASSERT_TRUE(is_usb_device_name("PreSonus"));
    ASSERT_TRUE(is_usb_device_name("Rocksmith"));
    ASSERT_TRUE(is_usb_device_name("Line 6"));
}

TEST(is_usb_device_name_negatives) {
    ASSERT_FALSE(is_usb_device_name("Built-in Microphone"));
    ASSERT_FALSE(is_usb_device_name("Internal Speakers"));
    ASSERT_FALSE(is_usb_device_name("HDMI Output"));
    ASSERT_FALSE(is_usb_device_name(""));
}

TEST(is_usb_device_name_case_insensitive) {
    ASSERT_TRUE(is_usb_device_name("SCARLETT"));
    ASSERT_TRUE(is_usb_device_name("scarlett 4i4"));
    ASSERT_TRUE(is_usb_device_name("Behringer"));
}

// GROUP 2 — is_projector_or_hdmi (pure string, no hardware):
TEST(is_projector_or_hdmi_positives) {
    ASSERT_TRUE(is_projector_or_hdmi("Epson Projector"));
    ASSERT_TRUE(is_projector_or_hdmi("HDMI Output"));
    ASSERT_TRUE(is_projector_or_hdmi("DisplayPort Audio"));
}

TEST(is_projector_or_hdmi_negatives) {
    ASSERT_FALSE(is_projector_or_hdmi("Built-in Speakers"));
    ASSERT_FALSE(is_projector_or_hdmi("USB Audio Device"));
    ASSERT_FALSE(is_projector_or_hdmi(""));
}

TEST(is_projector_or_hdmi_case_insensitive) {
    ASSERT_TRUE(is_projector_or_hdmi("hdmi output"));
    ASSERT_TRUE(is_projector_or_hdmi("EPSON xp-7100"));
    ASSERT_TRUE(is_projector_or_hdmi("displayport 1.4"));
}

// GROUP 3 — get_host_api_priority (pure logic, no hardware):
TEST(get_host_api_priority_values) {
    ASSERT_GE(get_host_api_priority(1), 0);
    ASSERT_GE(get_host_api_priority(2), 0);
    ASSERT_GE(get_host_api_priority(5), 0);
    ASSERT_GE(get_host_api_priority(8), 0);
    ASSERT_GE(get_host_api_priority(13), 0);
}

TEST(get_host_api_priority_idempotent) {
    int val1 = get_host_api_priority(1);
    int val2 = get_host_api_priority(1);
    ASSERT_EQ(val1, val2);
}

// GROUP 4 — devices_share_host_api (needs Pa init, use PaGuard):
TEST(devices_share_host_api_invalid) {
    PaGuard pa;
    if (!pa.ok) return;
    ASSERT_FALSE(devices_share_host_api(-1, -1));
    ASSERT_FALSE(devices_share_host_api(-1, 0));
    ASSERT_FALSE(devices_share_host_api(0, -1));
}

TEST(devices_share_host_api_same_device) {
    PaGuard pa;
    if (!pa.ok) return;
    int dev = Pa_GetDefaultInputDevice();
    if (dev == paNoDevice) dev = Pa_GetDefaultOutputDevice();
    if (dev == paNoDevice) return;
    ASSERT_TRUE(devices_share_host_api(dev, dev));
}

// GROUP 5 — AudioEngine device enumeration (guard with initialize()):
TEST(audio_engine_get_input_devices) {
    AudioEngine engine;
    if (!engine.initialize()) return;
    auto devices = engine.get_input_devices();
    for (const auto& dev : devices) {
        ASSERT_FALSE(dev.name.empty());
        ASSERT_GE(dev.max_input_channels, 1);
        ASSERT_GE(dev.index, 0);
    }
}

TEST(audio_engine_get_output_devices) {
    AudioEngine engine;
    if (!engine.initialize()) return;
    auto devices = engine.get_output_devices();
    for (const auto& dev : devices) {
        ASSERT_FALSE(dev.name.empty());
        ASSERT_GE(dev.max_output_channels, 1);
        ASSERT_GE(dev.index, 0);
    }
}

TEST(audio_engine_get_device_names_before_init) {
    AudioEngine engine;
    ASSERT_EQ(engine.get_input_device_name(), std::string("None"));
    ASSERT_EQ(engine.get_output_device_name(), std::string("None"));
}

TEST(audio_engine_is_usb_device_flag) {
    AudioEngine engine;
    if (!engine.initialize()) return;
    auto devices = engine.get_input_devices();
    for (const auto& dev : devices) {
        ASSERT_EQ(dev.is_usb_device, is_usb_device_name(dev.name));
    }
}

TEST(audio_engine_set_invalid_input_device) {
    AudioEngine engine;
    if (!engine.initialize()) return;
    engine.clear_error();
    ASSERT_FALSE(engine.set_input_device(999999));
    ASSERT_FALSE(engine.get_last_error().empty());
}

TEST(audio_engine_set_invalid_output_device) {
    AudioEngine engine;
    if (!engine.initialize()) return;
    engine.clear_error();
    ASSERT_FALSE(engine.set_output_device(999999));
    ASSERT_FALSE(engine.get_last_error().empty());
}

// GROUP 6 — lifecycle: initialize/shutdown:
TEST(audio_engine_init_shutdown) {
    AudioEngine engine;
    if (!engine.initialize()) return;
    ASSERT_EQ(engine.get_last_error(), std::string(""));
    engine.shutdown();
    ASSERT_FALSE(engine.start());
}

TEST(audio_engine_double_shutdown) {
    AudioEngine engine;
    if (!engine.initialize()) return;
    engine.shutdown();
    engine.shutdown();
    ASSERT_FALSE(engine.start());
}

TEST(audio_engine_double_init) {
    AudioEngine engine;
    if (!engine.initialize()) return;
    ASSERT_FALSE(engine.initialize());
}

// GROUP 7 — lifecycle: start/stop:
TEST(audio_engine_start_without_init) {
    AudioEngine engine;
    ASSERT_FALSE(engine.start());
}

TEST(audio_engine_start_after_init) {
    AudioEngine engine;
    if (!engine.initialize()) return;
    engine.start();
    engine.stop();
    ASSERT_EQ(engine.get_last_error(), std::string(""));
}

TEST(audio_engine_stop_without_start) {
    AudioEngine engine;
    engine.stop();
    engine.stop();
    ASSERT_EQ(engine.get_last_error(), std::string(""));
}

TEST(audio_engine_start_stop_start) {
    AudioEngine engine;
    if (!engine.initialize()) return;
    engine.start();
    engine.stop();
    engine.start();
}

TEST(audio_engine_double_start) {
    AudioEngine engine;
    if (!engine.initialize()) return;
    if (engine.start()) {
        ASSERT_FALSE(engine.start());
    }
}

TEST(audio_engine_restart_after_stop) {
    AudioEngine engine;
    if (!engine.initialize()) return;
    engine.start();
    engine.stop();
    if (!engine.restart()) {
        ASSERT_NE(engine.get_last_error(), std::string(""));
    }
}

// GROUP 8 — error string management:
TEST(audio_engine_clear_error) {
    AudioEngine engine;
    if (!engine.initialize()) return;
    engine.set_input_device(999999);
    ASSERT_FALSE(engine.get_last_error().empty());
    engine.clear_error();
    ASSERT_EQ(engine.get_last_error(), std::string(""));
}

TEST(audio_engine_clear_error_init) {
    AudioEngine engine;
    if (!engine.initialize()) return;
    engine.clear_error();
    ASSERT_EQ(engine.get_last_error(), std::string(""));
}
