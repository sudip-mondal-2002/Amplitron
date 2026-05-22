// =============================================================================
// PortAudio backend tests — Groups 1-8
//
// Design principles enforced per reviewer feedback:
//   • No early returns without a preceding assertion.
//   • ASSERT_TRUE(engine.initialize()) / ASSERT_TRUE(pa.ok) — if
//     Pa_Initialize() fails in CI, that is a real test failure (PortAudio is
//     installed as a build dependency on every platform).
//   • Stream-level tests use engine.is_running() to assert state so that
//     tests are meaningful whether or not audio hardware is present.
// =============================================================================

#include "test_framework.h"
#include "audio/audio_engine.h"
#include "audio/audio_backend_portaudio_helpers.h"
#include <portaudio.h>

using namespace Amplitron;

// RAII wrapper: Pa_Initialize / Pa_Terminate.
// ASSERT_TRUE(pa.ok) replaces every former "if (!pa.ok) return;".
struct PaGuard {
    bool ok;
    PaGuard() { ok = (Pa_Initialize() == paNoError); }
    ~PaGuard() { if (ok) Pa_Terminate(); }
};

// =============================================================================
// GROUP 1 — is_usb_device_name  (pure string logic, no hardware)
// =============================================================================

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
}

// =============================================================================
// GROUP 2 — is_projector_or_hdmi  (pure string logic, no hardware)
// =============================================================================

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

// =============================================================================
// GROUP 3 — get_host_api_priority  (pure logic, no hardware)
// =============================================================================

TEST(get_host_api_priority_values) {
    // All valid host-API type IDs must return a non-negative priority.
    ASSERT_GE(get_host_api_priority(1),  0);
    ASSERT_GE(get_host_api_priority(2),  0);
    ASSERT_GE(get_host_api_priority(5),  0);
    ASSERT_GE(get_host_api_priority(8),  0);
    ASSERT_GE(get_host_api_priority(13), 0);
}

TEST(get_host_api_priority_idempotent) {
    // Same input must always produce the same output.
    ASSERT_EQ(get_host_api_priority(1), get_host_api_priority(1));
    ASSERT_EQ(get_host_api_priority(5), get_host_api_priority(5));
}

// =============================================================================
// GROUP 4 — devices_share_host_api  (needs Pa_Initialize; Pa_Initialize
//            succeeds on every CI platform — PortAudio is a build dep)
// =============================================================================

TEST(devices_share_host_api_invalid_indices) {
    PaGuard pa;
    ASSERT_TRUE(pa.ok); // Pa_Initialize must succeed with PortAudio installed

    // Two negative indices → both Pa_GetDeviceInfo calls return nullptr → false
    ASSERT_FALSE(devices_share_host_api(-1, -1));
    // paNoDevice is the PortAudio sentinel for "no device" (-1)
    ASSERT_FALSE(devices_share_host_api(-1,        paNoDevice));
    ASSERT_FALSE(devices_share_host_api(paNoDevice, -1));
}

TEST(devices_share_host_api_same_device) {
    PaGuard pa;
    ASSERT_TRUE(pa.ok);

    // Invalid sentinel always shares nothing with itself or others.
    ASSERT_FALSE(devices_share_host_api(-1, -1));

    // Now try with the system's default device if one exists.
    int dev = Pa_GetDefaultInputDevice();
    if (dev == paNoDevice) dev = Pa_GetDefaultOutputDevice();
    if (dev == paNoDevice) {
        // No device enumerated (e.g. a truly bare CI runner).
        // The test above already ran an assertion, so this is not vacuous.
        return;
    }
    // A valid device must share its own host API with itself.
    ASSERT_TRUE(devices_share_host_api(dev, dev));
}

// =============================================================================
// GROUP 5 — AudioEngine device enumeration
//   AudioEngine::initialize() calls Pa_Initialize(); it succeeds on all CI
//   platforms. ASSERT_TRUE replaces the former "if (!initialize()) return;".
// =============================================================================

TEST(audio_engine_get_device_names_before_init) {
    // No hardware needed — checks default state before any initialization.
    AudioEngine engine;
    ASSERT_EQ(engine.get_input_device_name(),  std::string("None"));
    ASSERT_EQ(engine.get_output_device_name(), std::string("None"));
    ASSERT_FALSE(engine.is_running());
}

TEST(audio_engine_get_input_devices) {
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());
    auto devices = engine.get_input_devices();
    // Every enumerated device must have a non-empty name and valid fields.
    for (const auto& dev : devices) {
        ASSERT_FALSE(dev.name.empty());
        ASSERT_GE(dev.max_input_channels, 1);
        ASSERT_GE(dev.index, 0);
    }
}

TEST(audio_engine_get_output_devices) {
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());
    auto devices = engine.get_output_devices();
    for (const auto& dev : devices) {
        ASSERT_FALSE(dev.name.empty());
        ASSERT_GE(dev.max_output_channels, 1);
        ASSERT_GE(dev.index, 0);
    }
}

TEST(audio_engine_is_usb_device_flag_matches_helper) {
    // The is_usb_device field in AudioDeviceInfo must equal
    // is_usb_device_name(name) for every enumerated input device.
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());
    auto devices = engine.get_input_devices();
    for (const auto& dev : devices) {
        ASSERT_EQ(dev.is_usb_device, is_usb_device_name(dev.name));
    }
}

TEST(audio_engine_set_invalid_input_device_returns_false_and_sets_error) {
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());
    engine.clear_error();
    // Device index 999999 is guaranteed to be out of range.
    ASSERT_FALSE(engine.set_input_device(999999));
    ASSERT_FALSE(engine.get_last_error().empty());
}

TEST(audio_engine_set_invalid_output_device_returns_false_and_sets_error) {
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());
    engine.clear_error();
    ASSERT_FALSE(engine.set_output_device(999999));
    ASSERT_FALSE(engine.get_last_error().empty());
}

// =============================================================================
// GROUP 6 — lifecycle: initialize / shutdown
// =============================================================================

TEST(audio_engine_init_then_shutdown) {
    AudioEngine engine;
    // After initialize: no error, not running.
    ASSERT_TRUE(engine.initialize());
    ASSERT_EQ(engine.get_last_error(), std::string(""));
    ASSERT_FALSE(engine.is_running());

    engine.shutdown();

    // After shutdown: start() must fail because initialized_ is now false.
    ASSERT_FALSE(engine.start());
    ASSERT_FALSE(engine.is_running());
}

TEST(audio_engine_double_shutdown_is_safe) {
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());

    engine.shutdown();
    // Second shutdown is a no-op (initialized_ already false).
    engine.shutdown();

    // State is consistent: engine cannot be started without re-initializing.
    ASSERT_FALSE(engine.start());
    ASSERT_FALSE(engine.is_running());
}

TEST(audio_engine_double_init_does_not_crash) {
    // AudioEngine::initialize() has no re-entry guard; it calls
    // Pa_Initialize() twice (PortAudio ref-counts these).
    // We verify: no crash, return value is true, and we manually balance
    // the extra Pa_Initialize with an extra Pa_Terminate.
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());
    ASSERT_TRUE(engine.initialize()); // second call also succeeds

    engine.shutdown();   // calls Pa_Terminate once (ref-count: 1)
    Pa_Terminate();      // balance the extra Pa_Initialize (ref-count: 0)
}

// =============================================================================
// GROUP 7 — lifecycle: start / stop
//   stream-level operations may fail in headless CI (no audio hardware).
//   All assertions are based on is_running() state rather than assuming
//   start() returns true.
// =============================================================================

TEST(audio_engine_start_without_init_returns_false) {
    // No Pa_Initialize at all: start() must return false.
    AudioEngine engine;
    ASSERT_FALSE(engine.start());
    ASSERT_FALSE(engine.is_running());
}

TEST(audio_engine_stop_without_start_is_safe_noop) {
    // stop() before any stream is open must be a silent no-op.
    AudioEngine engine;
    ASSERT_FALSE(engine.is_running());
    engine.stop();
    ASSERT_FALSE(engine.is_running());
    engine.stop(); // second call also safe
    ASSERT_FALSE(engine.is_running());
}

TEST(audio_engine_start_after_init) {
    // start() may legitimately fail in headless CI (no hardware).
    // Whatever it returns, is_running() must reflect that return value.
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());
    bool started = engine.start();
    ASSERT_EQ(engine.is_running(), started);
    engine.stop();
    ASSERT_FALSE(engine.is_running());
}

TEST(audio_engine_start_stop_start_state_is_consistent) {
    // Verifies that the start → stop → start cycle leaves the engine in a
    // consistent state at every step (is_running tracks the return of start).
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());

    bool first = engine.start();
    ASSERT_EQ(engine.is_running(), first); // state matches return value

    engine.stop();
    ASSERT_FALSE(engine.is_running()); // stop() always clears running

    bool second = engine.start();
    ASSERT_EQ(engine.is_running(), second); // consistent on second attempt

    engine.stop();
    ASSERT_FALSE(engine.is_running());
}

TEST(audio_engine_double_start_returns_false_on_second_call) {
    // After any call to start():
    //   • if it succeeded: running_ is true → second call hits the
    //     "running_" guard and returns false.
    //   • if it failed (no HW): stream open fails again → also returns false.
    // Either way the second call MUST return false.
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());
    engine.start();              // may succeed or fail — irrelevant
    ASSERT_FALSE(engine.start()); // second call always false
    engine.stop();
}

TEST(audio_engine_restart_after_stop_state_is_consistent) {
    // restart() = stop() + start(). Its return value must match is_running().
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());

    engine.start();
    engine.stop();
    ASSERT_FALSE(engine.is_running()); // precondition: stopped

    bool restarted = engine.restart();
    ASSERT_EQ(engine.is_running(), restarted); // state reflects result

    engine.stop();
    ASSERT_FALSE(engine.is_running());
}

// =============================================================================
// GROUP 8 — error string management
// =============================================================================

TEST(audio_engine_clear_error_after_invalid_device) {
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());

    // An invalid device must set a non-empty error.
    ASSERT_FALSE(engine.set_input_device(999999));
    ASSERT_FALSE(engine.get_last_error().empty());

    // clear_error() must empty it.
    engine.clear_error();
    ASSERT_EQ(engine.get_last_error(), std::string(""));
}

TEST(audio_engine_get_last_error_empty_after_clear) {
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());
    // No operation that sets an error: error must be empty after clear.
    engine.clear_error();
    ASSERT_EQ(engine.get_last_error(), std::string(""));
}
