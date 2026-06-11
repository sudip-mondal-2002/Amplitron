#include "audio/backend/audio_backend.h"
#include "audio/backend/audio_backend_registry.h"
#include "test_framework.h"

using namespace Amplitron;

class DummyBackend : public IAudioBackend {
   public:
    bool initialize(IAudioEngine*) override { return true; }
    int get_sample_rate() const override { return 44100; }
    int get_buffer_size() const override { return 256; }
    void shutdown() override {}
    bool start() override { return true; }
    void stop() override {}
    std::vector<AudioDeviceInfo> get_input_devices() const override { return {}; }
    std::vector<AudioDeviceInfo> get_output_devices() const override { return {}; }
    bool set_input_device(int) override { return true; }
    bool set_output_device(int) override { return true; }
    std::string get_input_device_name() const override { return "dummy"; }
    std::string get_output_device_name() const override { return "dummy"; }
    int get_input_device() const override { return -1; }
    int get_output_device() const override { return -1; }
};

TEST(AudioBackendRegistry_RegisterAndCreate) {
    auto& registry = AudioBackendRegistry::instance();
    registry.register_backend("dummy_test", []() { return std::make_unique<DummyBackend>(); });

    auto backend = registry.create("dummy_test");
    ASSERT_TRUE(backend != nullptr);
    ASSERT_EQ(backend->get_input_device_name(), "dummy");

    auto invalid = registry.create("invalid_name");
    ASSERT_TRUE(invalid == nullptr);
}

TEST(AudioBackendRegistry_Available) {
    auto& registry = AudioBackendRegistry::instance();
    auto list = registry.available();
    bool found = false;
    for (const auto& name : list) {
        if (name == "dummy_test") {
            found = true;
            break;
        }
    }
    ASSERT_TRUE(found);
}

TEST(AudioBackendFactory_CreateBackend) {
    auto backend = AudioBackendFactory::create_backend("dummy_test");
    ASSERT_TRUE(backend != nullptr);

    // Test nonexistent type -> fallback to first available
    auto fallback = AudioBackendFactory::create_backend("nonexistent_type");
    ASSERT_TRUE(fallback != nullptr);
}

TEST(AudioBackendFactory_GetAvailableBackends) {
    auto list = AudioBackendFactory::get_available_backends();
    ASSERT_FALSE(list.empty());
}
