#include "audio/backend/audio_backend.h"
#include "audio/engine/audio_engine.h"
#include "test_framework.h"

using namespace Amplitron;
using namespace TestFramework;

class MockAudioBackend : public IAudioBackend {
   public:
    bool initialized = false;
    bool started = false;
    bool input_device_set = false;
    bool output_device_set = false;

    bool initialize(IAudioEngine*) override {
        initialized = true;
        return true;
    }

    int get_sample_rate() const override { return 48000; }
    int get_buffer_size() const override { return 512; }

    void shutdown() override { initialized = false; }

    bool start() override {
        if (initialized) {
            started = true;
            return true;
        }
        return false;
    }

    void stop() override { started = false; }

    std::vector<AudioDeviceInfo> get_input_devices() const override {
        return {{0, "Mock Input", 2, 0, 48000.0, false}};
    }

    std::vector<AudioDeviceInfo> get_output_devices() const override {
        return {{1, "Mock Output", 0, 2, 48000.0, false}};
    }

    bool set_input_device(int) override {
        input_device_set = true;
        return true;
    }

    bool set_output_device(int) override {
        output_device_set = true;
        return true;
    }

    std::string get_input_device_name() const override { return "Mock Input"; }

    std::string get_output_device_name() const override { return "Mock Output"; }

    int get_input_device() const override { return 0; }
    int get_output_device() const override { return 1; }
};

TEST(AudioBackend_PolymorphicMockBackendInjection) {
    // Create our mock backend using unique_ptr to prevent leakage
    auto mock = std::make_unique<MockAudioBackend>();
    AudioEngine engine;

    // Inject it!
    engine.replace_backend_for_test(mock.get());

    // Verify engine delegates to the mock!
    ASSERT_TRUE(engine.initialize());
    ASSERT_TRUE(mock->initialized);

    ASSERT_TRUE(engine.start());
    ASSERT_TRUE(mock->started);

    auto inputs = engine.get_input_devices();
    ASSERT_EQ(inputs.size(), 1u);
    ASSERT_EQ(inputs[0].name, "Mock Input");

    ASSERT_TRUE(engine.set_input_device(0));
    ASSERT_TRUE(mock->input_device_set);

    engine.stop();
    ASSERT_FALSE(mock->started);

    engine.shutdown();
    ASSERT_FALSE(mock->initialized);
    engine.clear_backend_for_test();
}

class ConfigurableMockAudioBackend : public MockAudioBackend {
   public:
    bool set_input_ok = true;
    bool set_output_ok = true;
    int start_fail_count = 0;

    bool set_input_device(int) override {
        input_device_set = true;
        return set_input_ok;
    }
    bool set_output_device(int) override {
        output_device_set = true;
        return set_output_ok;
    }
    bool start() override {
        if (start_fail_count > 0) {
            start_fail_count--;
            started = false;
            return false;
        }
        return MockAudioBackend::start();
    }
};

TEST(AudioEngine_NullBackendFallbackPaths) {
    AudioEngine engine;
    engine.clear_backend_for_test();

    ASSERT_FALSE(engine.initialize());
    ASSERT_EQ(engine.get_input_device_name(), "None");
    ASSERT_EQ(engine.get_output_device_name(), "None");
    ASSERT_FALSE(engine.set_input_device(0));
    ASSERT_FALSE(engine.set_output_device(0));
    ASSERT_EQ(engine.get_input_devices().size(), 0u);
    ASSERT_EQ(engine.get_output_devices().size(), 0u);
}

TEST(AudioEngine_DeviceControlAndFailures) {
    auto mock = std::make_unique<ConfigurableMockAudioBackend>();
    AudioEngine engine;
    engine.replace_backend_for_test(mock.get());
    engine.initialize();

    // 1. set_input_device and set_output_device when was_running is false
    mock->set_input_ok = true;
    ASSERT_TRUE(engine.set_input_device(0));
    mock->set_output_ok = true;
    ASSERT_TRUE(engine.set_output_device(1));

    // 2. set_input_device and set_output_device failures (when was_running is false)
    mock->set_input_ok = false;
    ASSERT_FALSE(engine.set_input_device(0));
    mock->set_output_ok = false;
    ASSERT_FALSE(engine.set_output_device(1));

    // 3. set_input_device and set_output_device when running is true, but backend set fails
    engine.start();
    mock->set_input_ok = false;
    ASSERT_FALSE(engine.set_input_device(0));
    mock->set_output_ok = false;
    ASSERT_FALSE(engine.set_output_device(1));
    engine.stop();

    // 4. Reversion failures (when was_running is true, device set succeeds, but start fails, then
    // revert fails)
    engine.start();
    mock->set_input_ok = true;
    mock->start_fail_count = 2;  // fails first start, and second start during revert
    ASSERT_FALSE(engine.set_input_device(0));

    // Reset and try output reversion failure
    mock->start_fail_count = 0;
    engine.start();
    mock->set_output_ok = true;
    mock->start_fail_count = 2;  // fails first start, and second start during revert
    ASSERT_FALSE(engine.set_output_device(1));

    // 5. restart() failure path
    mock->start_fail_count = 1;
    ASSERT_FALSE(engine.restart());

    engine.shutdown();
    engine.clear_backend_for_test();
}
