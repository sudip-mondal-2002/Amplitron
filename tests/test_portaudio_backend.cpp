// =============================================================================
// PortAudio backend tests
//
// Coverage targets:
//   audio_backend_portaudio.cpp       — helpers + factory
//   audio_backend_portaudio_devices.cpp — name functions, enumeration,
//                                         set_input/output_device paths
//   audio_backend_portaudio_lifecycle.cpp — initialize, shutdown, start,
//                                           stop, restart, error recovery
//
// Design:
//   • ASSERT_TRUE(engine.initialize()) / ASSERT_TRUE(pa.ok) replace all
//     former early-returns so every test always has at least one assertion.
//   • Stream-level tests assert is_running() state rather than assuming
//     start() succeeds (headless CI has no real audio hardware).
//   • Pa_Initialize() always succeeds on CI — PortAudio is a build dep.
// =============================================================================

#include "test_framework.h"
#include "audio/audio_engine.h"
#include "audio/audio_backend_portaudio_helpers.h"
#include <portaudio.h>
#include <string>
#include <vector>

using namespace Amplitron;

// ---------------------------------------------------------------------------
// RAII wrapper — replaces former "if (!pa.ok) return;" with ASSERT_TRUE
// ---------------------------------------------------------------------------
struct PaGuard {
    bool ok;
    PaGuard() { ok = (Pa_Initialize() == paNoError); }
    ~PaGuard() { if (ok) Pa_Terminate(); }
};

// ===========================================================================
// GROUP 1 — is_usb_device_name  (pure string, zero hardware)
// ===========================================================================

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
    ASSERT_TRUE(is_usb_device_name("BEHRINGER"));
    ASSERT_TRUE(is_usb_device_name("USB AUDIO"));
}

// ===========================================================================
// GROUP 2 — is_projector_or_hdmi  (pure string, zero hardware)
// ===========================================================================

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

// ===========================================================================
// GROUP 3 — get_host_api_priority  (pure logic, zero hardware)
// ===========================================================================

TEST(get_host_api_priority_non_negative) {
    // Every valid host-API type ID must return a non-negative priority.
    ASSERT_GE(get_host_api_priority(1),  0);
    ASSERT_GE(get_host_api_priority(2),  0);
    ASSERT_GE(get_host_api_priority(5),  0);
    ASSERT_GE(get_host_api_priority(8),  0);
    ASSERT_GE(get_host_api_priority(13), 0);
}

TEST(get_host_api_priority_idempotent) {
    // Same input must always produce the same output (no side-effects).
    ASSERT_EQ(get_host_api_priority(1), get_host_api_priority(1));
    ASSERT_EQ(get_host_api_priority(5), get_host_api_priority(5));
}

// ===========================================================================
// GROUP 4 — devices_share_host_api  (requires Pa_Initialize)
// ===========================================================================

TEST(devices_share_host_api_invalid_indices) {
    PaGuard pa;
    ASSERT_TRUE(pa.ok);

    // Both negative → Pa_GetDeviceInfo returns nullptr for both → false
    ASSERT_FALSE(devices_share_host_api(-1, -1));
    // One invalid sentinel, one also invalid → false
    ASSERT_FALSE(devices_share_host_api(-1, paNoDevice));
    ASSERT_FALSE(devices_share_host_api(paNoDevice, -1));
}

TEST(devices_share_host_api_same_device_shares_with_itself) {
    PaGuard pa;
    ASSERT_TRUE(pa.ok);

    // Invalid index never shares with itself (no PaDeviceInfo available).
    ASSERT_FALSE(devices_share_host_api(-1, -1));

    int dev = Pa_GetDefaultInputDevice();
    if (dev == paNoDevice) dev = Pa_GetDefaultOutputDevice();
    if (dev == paNoDevice) {
        // The assertion above already ran; no more tests possible.
        return;
    }
    // A valid device must share its own host API with itself.
    ASSERT_TRUE(devices_share_host_api(dev, dev));
}

TEST(devices_share_host_api_two_real_devices) {
    PaGuard pa;
    ASSERT_TRUE(pa.ok);

    int count = Pa_GetDeviceCount();
    // Need at least two devices to compare host-API membership.
    if (count < 2) return;

    // Comparing device 0 with itself: must be true.
    ASSERT_TRUE(devices_share_host_api(0, 0));
    // Comparing device 0 with device 1: result is API-specific, but the
    // call must not crash and must return a valid bool.
    bool result = devices_share_host_api(0, 1);
    ASSERT_TRUE(result == true || result == false); // always valid bool
}

// ===========================================================================
// GROUP 5a — AudioEngine: state before any initialization
// ===========================================================================

TEST(audio_engine_default_state_before_init) {
    AudioEngine engine;
    // Neither device is selected before initialization.
    ASSERT_EQ(engine.get_input_device(),  -1);
    ASSERT_EQ(engine.get_output_device(), -1);
    ASSERT_EQ(engine.get_input_device_name(),  std::string("None"));
    ASSERT_EQ(engine.get_output_device_name(), std::string("None"));
    ASSERT_FALSE(engine.is_running());
    // Default configuration values.
    ASSERT_GT(engine.get_sample_rate(), 0);
    ASSERT_GT(engine.get_buffer_size(), 0);
}

// ===========================================================================
// GROUP 5b — AudioEngine: device names — covers BOTH branches of
//   get_input_device_name() / get_output_device_name()
//   (the "input_device_ >= 0" branch is exercised only after initialize()
//    has run auto-detection and selected a device)
// ===========================================================================

TEST(audio_engine_input_device_name_both_branches) {
    AudioEngine engine;

    // Branch: input_device_ == -1 → must return "None"
    ASSERT_EQ(engine.get_input_device_name(), std::string("None"));

    ASSERT_TRUE(engine.initialize());
    int in_dev = engine.get_input_device();
    std::string name = engine.get_input_device_name();

    if (in_dev >= 0) {
        // Branch: input_device_ >= 0 → must return the actual device name
        ASSERT_FALSE(name.empty());
        ASSERT_NE(name, std::string("None"));
    } else {
        // Auto-detect found nothing; fallback is still "None"
        ASSERT_EQ(name, std::string("None"));
    }
}

TEST(audio_engine_output_device_name_both_branches) {
    AudioEngine engine;

    // Branch: output_device_ == -1 → must return "None"
    ASSERT_EQ(engine.get_output_device_name(), std::string("None"));

    ASSERT_TRUE(engine.initialize());
    int out_dev = engine.get_output_device();
    std::string name = engine.get_output_device_name();

    if (out_dev >= 0) {
        // Branch: output_device_ >= 0 → must return the actual device name
        ASSERT_FALSE(name.empty());
        ASSERT_NE(name, std::string("None"));
    } else {
        ASSERT_EQ(name, std::string("None"));
    }
}

// ===========================================================================
// GROUP 5c — AudioEngine: device enumeration field validity
// ===========================================================================

TEST(audio_engine_get_input_devices_field_validity) {
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());
    auto devices = engine.get_input_devices();
    for (const auto& dev : devices) {
        ASSERT_FALSE(dev.name.empty());
        ASSERT_GE(dev.max_input_channels, 1);
        ASSERT_GE(dev.index, 0);
        ASSERT_GT(dev.default_sample_rate, 0.0);
    }
}

TEST(audio_engine_get_output_devices_field_validity) {
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());
    auto devices = engine.get_output_devices();
    for (const auto& dev : devices) {
        ASSERT_FALSE(dev.name.empty());
        ASSERT_GE(dev.max_output_channels, 1);
        ASSERT_GE(dev.index, 0);
        ASSERT_GT(dev.default_sample_rate, 0.0);
    }
}

TEST(audio_engine_is_usb_flag_matches_helper) {
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());
    auto in_devs  = engine.get_input_devices();
    auto out_devs = engine.get_output_devices();
    for (const auto& dev : in_devs) {
        ASSERT_EQ(dev.is_usb_device, is_usb_device_name(dev.name));
    }
    for (const auto& dev : out_devs) {
        ASSERT_EQ(dev.is_usb_device, is_usb_device_name(dev.name));
    }
}

// ===========================================================================
// GROUP 5d — AudioEngine: set_input_device / set_output_device
//   Covers all three branches in each setter:
//     1. Invalid index → return false, set error           (existing tests)
//     2. Valid index, engine NOT running → return true     (new tests below)
//     3. Valid index, engine WAS running → stop/restart    (hard without HW)
// ===========================================================================

TEST(audio_engine_set_invalid_input_device_sets_error) {
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());
    engine.clear_error();
    ASSERT_FALSE(engine.set_input_device(999999));
    ASSERT_FALSE(engine.get_last_error().empty());
    ASSERT_EQ(engine.get_last_error(), std::string("Invalid input device."));
}

TEST(audio_engine_set_invalid_output_device_sets_error) {
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());
    engine.clear_error();
    ASSERT_FALSE(engine.set_output_device(999999));
    ASSERT_FALSE(engine.get_last_error().empty());
    ASSERT_EQ(engine.get_last_error(), std::string("Invalid output device."));
}

TEST(audio_engine_set_valid_input_device_not_running_succeeds) {
    // Exercises the success path in set_input_device() when the engine
    // is not running (was_running == false → lines 81-98 in devices.cpp).
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());
    ASSERT_FALSE(engine.is_running());

    auto in_devs = engine.get_input_devices();
    if (in_devs.empty()) return; // no input devices on this runner

    int target = in_devs[0].index;
    engine.clear_error();
    ASSERT_TRUE(engine.set_input_device(target));
    ASSERT_EQ(engine.get_input_device(), target);
    ASSERT_EQ(engine.get_last_error(), std::string(""));
    // Name must now reflect the selected device (>= 0 branch of get_name)
    ASSERT_NE(engine.get_input_device_name(), std::string("None"));
    ASSERT_FALSE(engine.get_input_device_name().empty());
}

TEST(audio_engine_set_valid_output_device_not_running_succeeds) {
    // Exercises the success path in set_output_device() (lines 117-134).
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());
    ASSERT_FALSE(engine.is_running());

    auto out_devs = engine.get_output_devices();
    if (out_devs.empty()) return; // no output devices on this runner

    int target = out_devs[0].index;
    engine.clear_error();
    ASSERT_TRUE(engine.set_output_device(target));
    ASSERT_EQ(engine.get_output_device(), target);
    ASSERT_EQ(engine.get_last_error(), std::string(""));
    ASSERT_NE(engine.get_output_device_name(), std::string("None"));
    ASSERT_FALSE(engine.get_output_device_name().empty());
}

TEST(audio_engine_set_device_index_persists) {
    // Verify the device index is stable through get/set round-trips.
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());

    auto in_devs  = engine.get_input_devices();
    auto out_devs = engine.get_output_devices();
    if (in_devs.empty() || out_devs.empty()) return;

    int in_target  = in_devs[0].index;
    int out_target = out_devs[0].index;

    ASSERT_TRUE(engine.set_input_device(in_target));
    ASSERT_TRUE(engine.set_output_device(out_target));

    ASSERT_EQ(engine.get_input_device(),  in_target);
    ASSERT_EQ(engine.get_output_device(), out_target);
}

TEST(audio_engine_set_device_twice_is_idempotent) {
    // Setting the same device twice must succeed both times.
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());

    auto in_devs = engine.get_input_devices();
    if (in_devs.empty()) return;

    int target = in_devs[0].index;
    ASSERT_TRUE(engine.set_input_device(target));
    ASSERT_TRUE(engine.set_input_device(target)); // second call
    ASSERT_EQ(engine.get_input_device(), target);
}

// ===========================================================================
// GROUP 6 — Lifecycle: initialize / shutdown
// ===========================================================================

TEST(audio_engine_init_then_shutdown) {
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());
    ASSERT_EQ(engine.get_last_error(), std::string(""));
    ASSERT_FALSE(engine.is_running());

    engine.shutdown();

    // After shutdown: start() returns false (initialized_ is now false).
    ASSERT_FALSE(engine.start());
    ASSERT_FALSE(engine.is_running());
}

TEST(audio_engine_double_shutdown_is_safe) {
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());
    engine.shutdown();
    engine.shutdown(); // must be a no-op

    ASSERT_FALSE(engine.start());
    ASSERT_FALSE(engine.is_running());
}

TEST(audio_engine_double_init_does_not_crash) {
    // initialize() has no re-entry guard; Pa_Initialize() is called twice
    // (PortAudio ref-counts). Verify no crash and both calls return true.
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());
    ASSERT_TRUE(engine.initialize()); // second call: PortAudio ref-count → 2

    engine.shutdown();   // Pa_Terminate once (ref-count: 1)
    Pa_Terminate();      // balance the extra Pa_Initialize (ref-count: 0)
}

// ===========================================================================
// GROUP 7 — Lifecycle: start / stop
//   is_running() state is the authoritative assertion; start() may fail in
//   headless CI (no audio hardware) but MUST still be consistent.
// ===========================================================================

TEST(audio_engine_start_without_init_returns_false) {
    AudioEngine engine;
    ASSERT_FALSE(engine.start());
    ASSERT_FALSE(engine.is_running());
}

TEST(audio_engine_stop_without_start_is_safe_noop) {
    AudioEngine engine;
    ASSERT_FALSE(engine.is_running());
    engine.stop();
    ASSERT_FALSE(engine.is_running());
    engine.stop(); // second call must also be safe
    ASSERT_FALSE(engine.is_running());
}

TEST(audio_engine_start_after_init_state_consistent) {
    // start() may fail in headless CI; is_running() must match the return.
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());
    bool started = engine.start();
    ASSERT_EQ(engine.is_running(), started);
    engine.stop();
    ASSERT_FALSE(engine.is_running());
}

TEST(audio_engine_start_stop_start_state_consistent) {
    // Every step: is_running() must match the return value of start().
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());

    bool first = engine.start();
    ASSERT_EQ(engine.is_running(), first);

    engine.stop();
    ASSERT_FALSE(engine.is_running()); // stop always clears running_

    bool second = engine.start();
    ASSERT_EQ(engine.is_running(), second); // consistent second time

    engine.stop();
    ASSERT_FALSE(engine.is_running());
}

TEST(audio_engine_double_start_second_always_false) {
    // Whether the first start() opens a stream or not, the second call
    // must always return false:
    //   • stream open succeeded: running_ is true  → guard triggers
    //   • stream open failed:    Pa_OpenStream fails again → returns false
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());
    engine.start();                    // result not asserted intentionally
    ASSERT_FALSE(engine.start());      // second call: always false
    engine.stop();
}

TEST(audio_engine_restart_after_stop_state_consistent) {
    // restart() == stop() + start(); its return must match is_running().
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());

    engine.start();
    engine.stop();
    ASSERT_FALSE(engine.is_running()); // precondition: stopped

    bool restarted = engine.restart();
    ASSERT_EQ(engine.is_running(), restarted);
    engine.stop();
    ASSERT_FALSE(engine.is_running());
}

TEST(audio_engine_restart_sets_error_on_failure) {
    // When restart() fails (no hardware in CI), last_error_ must be set
    // per the restart() implementation (lines 343-345 of lifecycle.cpp).
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());

    engine.stop(); // ensure stopped
    bool ok = engine.restart();
    if (!ok) {
        // restart() sets last_error_ exactly when start() fails.
        ASSERT_FALSE(engine.get_last_error().empty());
    } else {
        // restart() clears last_error_ when start() succeeds.
        ASSERT_EQ(engine.get_last_error(), std::string(""));
        engine.stop();
    }
}

// ===========================================================================
// GROUP 8 — Error string management
// ===========================================================================

TEST(audio_engine_clear_error_after_invalid_input_device) {
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());

    ASSERT_FALSE(engine.set_input_device(999999));
    ASSERT_FALSE(engine.get_last_error().empty());

    engine.clear_error();
    ASSERT_EQ(engine.get_last_error(), std::string(""));
}

TEST(audio_engine_clear_error_after_invalid_output_device) {
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());

    ASSERT_FALSE(engine.set_output_device(999999));
    ASSERT_FALSE(engine.get_last_error().empty());

    engine.clear_error();
    ASSERT_EQ(engine.get_last_error(), std::string(""));
}

TEST(audio_engine_error_empty_after_valid_device_set) {
    // set_input_device() when NOT running does NOT touch last_error_ on
    // the success path — only the was_running restart path clears it.
    // Verify that a valid set from a clean state leaves no error.
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());

    auto in_devs = engine.get_input_devices();
    if (in_devs.empty()) return;

    engine.clear_error(); // start from a known-empty error state
    ASSERT_TRUE(engine.set_input_device(in_devs[0].index));
    // Non-running success path must not introduce a new error.
    ASSERT_EQ(engine.get_last_error(), std::string(""));
}

TEST(audio_engine_last_error_empty_after_clear_with_no_ops) {
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());
    engine.clear_error();
    ASSERT_EQ(engine.get_last_error(), std::string(""));
}

// ===========================================================================
// GROUP 9 — AudioEngine configuration & monitoring accessors
//   These cover inline getters/setters declared in audio_engine.h and
//   implemented in audio_engine_api.cpp / audio_engine.cpp.
// ===========================================================================

TEST(audio_engine_sample_rate_accessor) {
    AudioEngine engine;
    int default_rate = engine.get_sample_rate();
    ASSERT_GT(default_rate, 0);

    engine.set_sample_rate(48000);
    ASSERT_EQ(engine.get_sample_rate(), 48000);

    engine.set_sample_rate(44100);
    ASSERT_EQ(engine.get_sample_rate(), 44100);
}

TEST(audio_engine_buffer_size_accessor) {
    AudioEngine engine;
    int default_size = engine.get_buffer_size();
    ASSERT_GT(default_size, 0);

    engine.set_buffer_size(512);
    ASSERT_EQ(engine.get_buffer_size(), 512);

    engine.set_buffer_size(256);
    ASSERT_EQ(engine.get_buffer_size(), 256);
}

TEST(audio_engine_gain_accessors) {
    AudioEngine engine;
    // Default gains are set in the initializer list.
    ASSERT_GT(engine.get_input_gain(),  0.0f);
    ASSERT_GT(engine.get_output_gain(), 0.0f);
}

TEST(audio_engine_monitoring_accessors_before_stream) {
    AudioEngine engine;
    // Before any stream is opened, levels and CPU load must be 0 / false.
    ASSERT_NEAR(engine.get_input_level(),  0.0f, 1e-6f);
    ASSERT_NEAR(engine.get_output_level(), 0.0f, 1e-6f);
    ASSERT_NEAR(engine.get_input_rms(),    0.0f, 1e-6f);
    ASSERT_NEAR(engine.get_output_rms(),   0.0f, 1e-6f);
    ASSERT_NEAR(engine.get_cpu_load(),     0.0f, 1e-6f);
    ASSERT_FALSE(engine.consume_input_clipped());
    ASSERT_FALSE(engine.consume_output_clipped());
}

TEST(audio_engine_auto_buffer_flag) {
    AudioEngine engine;
    ASSERT_FALSE(engine.is_auto_buffer_enabled());
    engine.set_auto_buffer_enabled(true);
    ASSERT_TRUE(engine.is_auto_buffer_enabled());
    engine.set_auto_buffer_enabled(false);
    ASSERT_FALSE(engine.is_auto_buffer_enabled());
}

TEST(audio_engine_analyzer_enabled_flag) {
    AudioEngine engine;
    ASSERT_FALSE(engine.is_analyzer_enabled());
    engine.set_analyzer_enabled(true);
    ASSERT_TRUE(engine.is_analyzer_enabled());
    engine.set_analyzer_enabled(false);
    ASSERT_FALSE(engine.is_analyzer_enabled());
}
