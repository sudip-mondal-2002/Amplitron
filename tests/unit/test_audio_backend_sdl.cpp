#include <SDL.h>

#include <cstdlib>
#include <vector>

#include "audio/backend/sdl_backend.h"
#include "audio/engine/audio_engine.h"
#include "test_framework.h"

// Declare the non-static callback we just exposed
namespace Amplitron {
void sdl_audio_callback(void* userdata, Uint8* stream, int len);
}

static Uint32 g_mock_sdl_queued_audio_size = 0;
static int g_mock_sdl_init_subsystem_result = 0;
static bool g_mock_sdl_open_audio_device_fail_out = false;
static bool g_mock_sdl_open_audio_device_fail_cap = false;
static int g_mock_sdl_num_audio_devices = 0;
static std::vector<const char*> g_mock_sdl_device_names;
static const char* g_mock_sdl_last_opened_device = nullptr;
static std::string g_mock_sdl_error = "Mock SDL Error";

extern "C" {
int SDL_QueueAudio(SDL_AudioDeviceID dev, const void* data, Uint32 len) {
    (void)dev;
    (void)data;
    g_mock_sdl_queued_audio_size += len;
    return 0;
}

Uint32 SDL_GetQueuedAudioSize(SDL_AudioDeviceID dev) {
    (void)dev;
    return g_mock_sdl_queued_audio_size;
}

void SDL_ClearQueuedAudio(SDL_AudioDeviceID dev) {
    (void)dev;
    g_mock_sdl_queued_audio_size = 0;
}

Uint32 SDL_DequeueAudio(SDL_AudioDeviceID dev, void* data, Uint32 len) {
    (void)dev;
    Uint32 got = g_mock_sdl_queued_audio_size < len ? g_mock_sdl_queued_audio_size : len;
    if (got > 0) {
        if (data) {
            float* f_data = reinterpret_cast<float*>(data);
            std::fill(f_data, f_data + (got / sizeof(float)), 1.0f);
        }
        g_mock_sdl_queued_audio_size -= got;
    }
    return got;
}

int SDL_InitSubSystem(Uint32 flags) {
    if (flags & SDL_INIT_AUDIO) {
        return g_mock_sdl_init_subsystem_result;
    }
    return 0;
}

void SDL_QuitSubSystem(Uint32 flags) { (void)flags; }

const char* SDL_GetError(void) { return g_mock_sdl_error.c_str(); }

SDL_AudioDeviceID SDL_OpenAudioDevice(const char* device, int iscapture,
                                      const SDL_AudioSpec* desired, SDL_AudioSpec* obtained,
                                      int allowed_changes) {
    (void)allowed_changes;
    if (iscapture) {
        if (g_mock_sdl_open_audio_device_fail_cap) {
            return 0;
        }
        g_mock_sdl_last_opened_device = device;
        if (obtained && desired) {
            *obtained = *desired;
        }
        return 2;  // Capture device ID
    } else {
        if (g_mock_sdl_open_audio_device_fail_out) {
            return 0;
        }
        if (obtained && desired) {
            *obtained = *desired;
        }
        return 1;  // Playback/output device ID
    }
}

void SDL_CloseAudioDevice(SDL_AudioDeviceID dev) { (void)dev; }

void SDL_PauseAudioDevice(SDL_AudioDeviceID dev, int pause_on) {
    (void)dev;
    (void)pause_on;
}

int SDL_GetNumAudioDevices(int iscapture) {
    (void)iscapture;
    return g_mock_sdl_num_audio_devices;
}

const char* SDL_GetAudioDeviceName(int index, int iscapture) {
    (void)iscapture;
    if (index >= 0 && index < static_cast<int>(g_mock_sdl_device_names.size())) {
        return g_mock_sdl_device_names[index];
    }
    return nullptr;
}
}

class MockAudioEngine : public Amplitron::AudioEngine {
   public:
    MockAudioEngine() : Amplitron::AudioEngine(nullptr, nullptr) {}
    mutable int process_audio_calls = 0;
    mutable std::vector<float> last_input_data;

    void process_audio(const float* input, float* output, int frame_count) override {
        process_audio_calls++;
        if (input) {
            last_input_data.assign(input, input + frame_count);
        } else {
            last_input_data.clear();
        }
        Amplitron::AudioEngine::process_audio(input, output, frame_count);
    }
};

TEST(SdlBackend_LifecycleAndCallback) {
    // Set SDL to use the dummy driver
#ifdef _WIN32
    _putenv_s("SDL_AUDIODRIVER", "dummy");
#else
    setenv("SDL_AUDIODRIVER", "dummy", 1);
#endif

    MockAudioEngine engine;
    Amplitron::SdlBackend backend;

    // Check pre-init getters
    ASSERT_TRUE(backend.get_engine() == nullptr);
    ASSERT_EQ(backend.get_capture_device(), 0u);
    ASSERT_EQ(backend.get_input_device(), 0);
    ASSERT_EQ(backend.get_output_device(), 0);

    // Initialize
    ASSERT_TRUE(backend.initialize(&engine));
    ASSERT_TRUE(backend.get_engine() == &engine);

    // Check device lists and names
    auto inputs = backend.get_input_devices();
    ASSERT_EQ(inputs.size(), 1u);
    ASSERT_EQ(inputs[0].name, "Browser Microphone");

    auto outputs = backend.get_output_devices();
    ASSERT_EQ(outputs.size(), 1u);
    ASSERT_EQ(outputs[0].name, "Browser Audio Output");

    ASSERT_EQ(backend.get_input_device_name(), "Browser Microphone");
    ASSERT_EQ(backend.get_output_device_name(), "Browser Audio Output");

    ASSERT_TRUE(backend.set_input_device(0));
    ASSERT_TRUE(backend.set_output_device(0));

    // Start streaming (under dummy audio driver, this succeeds)
    ASSERT_TRUE(backend.start());
    ASSERT_EQ(backend.get_sample_rate(), 48000);
    ASSERT_GT(backend.get_buffer_size(), 0);

    // Start again when already running should return false
    ASSERT_FALSE(backend.start());

    SDL_AudioDeviceID cap_dev = backend.get_capture_device();
    ASSERT_NE(cap_dev, 0u);

    // 1. Normal callback test with queued capture data
    std::vector<float> input_data(512, 1.0f);
    SDL_QueueAudio(cap_dev, input_data.data(), input_data.size() * sizeof(float));

    std::vector<float> output_buffer(1024, 0.0f);
    int bytes_len = 512 * 2 * sizeof(float);  // 512 stereo frames = 1024 floats = 4096 bytes

    engine.process_audio_calls = 0;
    engine.last_input_data.clear();
    Amplitron::sdl_audio_callback(&backend, reinterpret_cast<Uint8*>(output_buffer.data()),
                                  bytes_len);

    ASSERT_EQ(engine.process_audio_calls, 1);
    ASSERT_EQ(engine.last_input_data.size(), 512u);
    for (float sample : engine.last_input_data) {
        ASSERT_NEAR(sample, 1.0f, 0.0001f);
    }
    ASSERT_EQ(SDL_GetQueuedAudioSize(cap_dev), 0u);

    // 2. Test callback with junk/excess queue clearing
    std::vector<float> excess_input(4096 * 4, 1.0f);
    SDL_QueueAudio(cap_dev, excess_input.data(), excess_input.size() * sizeof(float));

    engine.process_audio_calls = 0;
    engine.last_input_data.clear();
    Amplitron::sdl_audio_callback(&backend, reinterpret_cast<Uint8*>(output_buffer.data()),
                                  bytes_len);

    ASSERT_EQ(engine.process_audio_calls, 1);
    ASSERT_EQ(engine.last_input_data.size(), 512u);
    for (float sample : engine.last_input_data) {
        ASSERT_NEAR(sample, 1.0f, 0.0001f);
    }
    ASSERT_TRUE(SDL_GetQueuedAudioSize(cap_dev) <= 512u * sizeof(float));

    // 3. Test callback with insufficient queued data (triggers partial memset)
    SDL_ClearQueuedAudio(cap_dev);
    std::vector<float> short_input(100, 1.0f);
    SDL_QueueAudio(cap_dev, short_input.data(), short_input.size() * sizeof(float));

    engine.process_audio_calls = 0;
    engine.last_input_data.clear();
    Amplitron::sdl_audio_callback(&backend, reinterpret_cast<Uint8*>(output_buffer.data()),
                                  bytes_len);

    ASSERT_EQ(engine.process_audio_calls, 1);
    ASSERT_EQ(engine.last_input_data.size(), 512u);
    for (int i = 0; i < 100; ++i) {
        ASSERT_NEAR(engine.last_input_data[i], 1.0f, 0.0001f);
    }
    for (int i = 100; i < 512; ++i) {
        ASSERT_NEAR(engine.last_input_data[i], 0.0f, 0.0001f);
    }
    ASSERT_EQ(SDL_GetQueuedAudioSize(cap_dev), 0u);

    // 4. Test callback when capture device is missing (cap_dev = 0, should memset 0)
    backend.stop();
    ASSERT_EQ(backend.get_capture_device(), 0u);

    engine.process_audio_calls = 0;
    engine.last_input_data.clear();
    Amplitron::sdl_audio_callback(&backend, reinterpret_cast<Uint8*>(output_buffer.data()),
                                  bytes_len);

    ASSERT_EQ(engine.process_audio_calls, 1);
    ASSERT_EQ(engine.last_input_data.size(), 512u);
    for (float sample : engine.last_input_data) {
        ASSERT_NEAR(sample, 0.0f, 0.0001f);
    }

    // 5. Test callback with null engine (early return, no-op)
    Amplitron::SdlBackend backend_null;
    ASSERT_TRUE(backend_null.get_engine() == nullptr);
    std::vector<float> output_buffer_null(1024, 9.9f);
    Amplitron::sdl_audio_callback(&backend_null,
                                  reinterpret_cast<Uint8*>(output_buffer_null.data()), bytes_len);
    for (float sample : output_buffer_null) {
        ASSERT_NEAR(sample, 9.9f, 0.0001f);
    }

    // Cleanup
    backend.shutdown();
}

TEST(SdlBackend_FailurePathsAndDeviceSelection) {
    MockAudioEngine engine;

    // 1. Initialization failure
    g_mock_sdl_init_subsystem_result = -1;
    g_mock_sdl_error = "Mock Init SubSystem Failure";
    {
        Amplitron::SdlBackend backend;
        ASSERT_FALSE(backend.initialize(&engine));
    }
    // Restore init success
    g_mock_sdl_init_subsystem_result = 0;

    // 2. Output device open failure
    {
        Amplitron::SdlBackend backend;
        ASSERT_TRUE(backend.initialize(&engine));
        g_mock_sdl_open_audio_device_fail_out = true;
        g_mock_sdl_error = "Mock Open Output Device Failure";
        ASSERT_FALSE(backend.start());
        g_mock_sdl_open_audio_device_fail_out = false;
    }

    // 3. Preferred device detection (Case A: No capture devices)
    {
        Amplitron::SdlBackend backend;
        ASSERT_TRUE(backend.initialize(&engine));
        g_mock_sdl_num_audio_devices = 0;
        g_mock_sdl_device_names.clear();
        g_mock_sdl_last_opened_device = nullptr;
        ASSERT_TRUE(backend.start());
        ASSERT_EQ(g_mock_sdl_last_opened_device, nullptr);
        backend.stop();
    }

    // 4. Preferred device detection (Case B: Devices with no matching keywords, including a null
    // name)
    {
        Amplitron::SdlBackend backend;
        ASSERT_TRUE(backend.initialize(&engine));
        g_mock_sdl_num_audio_devices = 2;
        g_mock_sdl_device_names = {nullptr, "Internal Microphone"};
        g_mock_sdl_last_opened_device = nullptr;
        ASSERT_TRUE(backend.start());
        ASSERT_EQ(g_mock_sdl_last_opened_device, nullptr);
        backend.stop();
    }

    // 5. Preferred device detection (Case C: Scarlett keyword matched)
    {
        Amplitron::SdlBackend backend;
        ASSERT_TRUE(backend.initialize(&engine));
        g_mock_sdl_num_audio_devices = 3;
        g_mock_sdl_device_names = {"Internal Microphone", "Focusrite Scarlett 2i2", "iRig Pro"};
        g_mock_sdl_last_opened_device = nullptr;
        ASSERT_TRUE(backend.start());
        ASSERT_NE(g_mock_sdl_last_opened_device, nullptr);
        ASSERT_EQ(std::string(g_mock_sdl_last_opened_device), "Focusrite Scarlett 2i2");
        backend.stop();
    }

    // 6. Capture device fails to open (does not block start(), but capture_device_ is 0)
    {
        Amplitron::SdlBackend backend;
        ASSERT_TRUE(backend.initialize(&engine));
        g_mock_sdl_open_audio_device_fail_cap = true;
        g_mock_sdl_last_opened_device = nullptr;
        ASSERT_TRUE(backend.start());
        ASSERT_EQ(backend.get_capture_device(), 0u);
        backend.stop();
        g_mock_sdl_open_audio_device_fail_cap = false;
    }
}
