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
