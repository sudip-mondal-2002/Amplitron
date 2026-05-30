#include "test_framework.h"
#include "audio/engine/audio_engine.h"
#include "audio/backend/audio_backend.h"

#if defined(AMPLITRON_TESTS) && defined(WITH_JACK) && defined(__unix__) && !defined(__APPLE__)
#include "../fixtures/jack_mock.cpp"

namespace Amplitron
{
    AudioBackendState *create_disconnected_audio_backend_for_test();
    bool jack_backend_has_active_client_for_test(const AudioBackendState *state);
}
#endif

#if defined(AMPLITRON_TESTS) && defined(WITH_JACK) && defined(__unix__) && !defined(__APPLE__)
using namespace Amplitron;
TEST(AudioBackend_Jack_DoesNotStartWithoutBackend)
{
    // Reset flags
    g_mock_jack_client_open_fail = false;
    g_mock_jack_port_register_fail = false;
    g_mock_jack_activate_fail = false;
    g_mock_jack_process_callback = nullptr;
    g_mock_jack_process_arg = nullptr;

    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());

    AudioBackendState *state = create_disconnected_audio_backend_for_test();
    ASSERT_TRUE(state != nullptr);
    ASSERT_FALSE(jack_backend_has_active_client_for_test(state));
    engine.replace_backend_for_test(state);

    ASSERT_FALSE(engine.start());
    ASSERT_FALSE(engine.is_running());
    ASSERT_FALSE(engine.get_last_error().empty());
}

TEST(AudioBackend_Jack_FullLifecycle)
{
    // Reset flags
    g_mock_jack_client_open_fail = false;
    g_mock_jack_port_register_fail = false;
    g_mock_jack_activate_fail = false;
    g_mock_jack_process_callback = nullptr;
    g_mock_jack_process_arg = nullptr;

    AudioEngine engine;
    
    // Test device query methods on uninitialized / clean engine
    ASSERT_EQ(engine.get_input_device_name(), "JACK in_1");
    ASSERT_EQ(engine.get_output_device_name(), "JACK out_1");
    
    auto inputs = engine.get_input_devices();
    ASSERT_EQ(inputs.size(), 1);
    ASSERT_EQ(inputs[0].name, "JACK in_1");
    
    auto outputs = engine.get_output_devices();
    ASSERT_EQ(outputs.size(), 1);
    ASSERT_EQ(outputs[0].name, "JACK out_1");
    
    ASSERT_TRUE(engine.set_input_device(0));
    ASSERT_FALSE(engine.set_input_device(1)); // Invalid device index
    
    ASSERT_TRUE(engine.set_output_device(0));
    ASSERT_FALSE(engine.set_output_device(1)); // Invalid device index

    // Initialize (calls create_audio_backend() which returns mock client)
    ASSERT_TRUE(engine.initialize());
    
    // Second initialization should be a no-op
    ASSERT_TRUE(engine.initialize());

    // Start audio engine successfully
    ASSERT_TRUE(engine.start());
    ASSERT_TRUE(engine.is_running());
    
    // Start when already running should return false
    ASSERT_FALSE(engine.start());

    // Trigger process callback to test jack_process code path
    ASSERT_TRUE(g_mock_jack_process_callback != nullptr);
    ASSERT_TRUE(g_mock_jack_process_arg != nullptr);
    
    // Valid process run
    int ret = g_mock_jack_process_callback(128, g_mock_jack_process_arg);
    ASSERT_EQ(ret, 0);
    
    // Invalid args (null pointers to trigger coverage in jack_process early exits)
    ret = g_mock_jack_process_callback(128, nullptr);
    ASSERT_EQ(ret, 0);
    
    // Test restart
    ASSERT_TRUE(engine.restart());
    ASSERT_TRUE(engine.is_running());

    // Stop engine
    engine.stop();
    ASSERT_FALSE(engine.is_running());

    // Shutdown
    engine.shutdown();
}

TEST(AudioBackend_Jack_PortRegistrationFailure)
{
    g_mock_jack_client_open_fail = false;
    g_mock_jack_port_register_fail = true; // Force port registration failure
    g_mock_jack_activate_fail = false;

    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());
    ASSERT_FALSE(engine.start());
    ASSERT_FALSE(engine.get_last_error().empty());
}

TEST(AudioBackend_Jack_ActivationFailure)
{
    g_mock_jack_client_open_fail = false;
    g_mock_jack_port_register_fail = false;
    g_mock_jack_activate_fail = true; // Force activate failure

    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());
    ASSERT_FALSE(engine.start());
    ASSERT_FALSE(engine.get_last_error().empty());
}

TEST(AudioBackend_Jack_ClientOpenFailure)
{
    g_mock_jack_client_open_fail = true; // Force client open failure
    g_mock_jack_port_register_fail = false;
    g_mock_jack_activate_fail = false;

    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());
    ASSERT_FALSE(engine.start());
}

TEST(AudioBackend_Jack_NullStateDestroy)
{
    destroy_audio_backend(nullptr);
}
#else
TEST(AudioBackend_Jack_NotAvailable)
{
    ASSERT_TRUE(true);
}
#endif
