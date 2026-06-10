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

    IAudioEngine* engine_ptr = nullptr;

    bool initialize(IAudioEngine* eng) override {
        engine_ptr = eng;
        initialized = true;
        return true;
    }

    int get_sample_rate() const override {
        return engine_ptr ? engine_ptr->get_sample_rate() : 48000;
    }
    int get_buffer_size() const override {
        return engine_ptr ? engine_ptr->get_buffer_size() : 512;
    }

    void shutdown() override {
        initialized = false;
        engine_ptr = nullptr;
    }

    bool fail_start = false;

    bool start() override {
        if (fail_start) {
            fail_start = false;
            return false;
        }
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

    engine.shutdown();
    ASSERT_FALSE(mock->initialized);
    engine.clear_backend_for_test();
}

TEST(AudioEngine_NewFeaturesAndFailures) {
    auto mock = std::make_unique<MockAudioBackend>();
    AudioEngine engine;
    engine.replace_backend_for_test(mock.get());

    ASSERT_TRUE(engine.initialize());
    ASSERT_TRUE(engine.start());

    // 1. Test get/set global BPM
    engine.set_global_bpm(135.0f);
    ASSERT_NEAR(engine.get_global_bpm(), 135.0f, 0.01f);

    // Clamp check
    engine.set_global_bpm(300.0f);
    ASSERT_NEAR(engine.get_global_bpm(), 240.0f, 0.01f);
    engine.set_global_bpm(20.0f);
    ASSERT_NEAR(engine.get_global_bpm(), 40.0f, 0.01f);

    // 2. Test Serialize / Deserialize with BPM
    engine.set_global_bpm(125.0f);
    nlohmann::json j = engine.serialize();
    ASSERT_TRUE(j.contains("global_bpm"));
    ASSERT_NEAR(j["global_bpm"].get<float>(), 125.0f, 0.01f);

    // Deserialize
    engine.set_global_bpm(80.0f);
    engine.deserialize(j);
    ASSERT_NEAR(engine.get_global_bpm(), 125.0f, 0.01f);

    // 3. Test Tempo Engine reset and detect_bpm from AudioEngine API
    engine.reset_bpm_detection();
    float detected = engine.detect_bpm();
    ASSERT_NEAR(detected, -1.0f, 0.01f);  // No input written, should return -1

    // 4. Test set_sample_rate when running
    engine.set_sample_rate(44100);
    ASSERT_EQ(engine.get_sample_rate(), 44100);

    // 5. Test set_sample_rate failure and revert
    mock->fail_start = true;
    engine.set_sample_rate(96000);
    // Should revert back to 44100
    ASSERT_EQ(engine.get_sample_rate(), 44100);

    // 6. Test set_buffer_size failure and revert
    int initial_buf = engine.get_buffer_size();
    mock->fail_start = true;
    engine.set_buffer_size(initial_buf == 256 ? 512 : 256);
    ASSERT_EQ(engine.get_buffer_size(), initial_buf);

    engine.stop();
    engine.shutdown();
    engine.clear_backend_for_test();
}
