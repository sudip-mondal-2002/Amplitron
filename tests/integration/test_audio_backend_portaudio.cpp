#if !defined(WITH_JACK)
#include "audio/backend/audio_backend_portaudio_helpers.h"
#include "audio/engine/audio_engine.h"
#include "test_framework.h"
#include "../fixtures/portaudio_mock.cpp"

using namespace Amplitron;
using namespace TestFramework;

TEST(PortAudioDevices_IsUsbDeviceDetectsUsbInName) {
    ASSERT_TRUE(is_usb_device_name("USB Audio Device"));
    ASSERT_TRUE(is_usb_device_name("Behringer UCG102 [USB]"));
    ASSERT_TRUE(is_usb_device_name("Focusrite Scarlett Solo"));
    ASSERT_FALSE(is_usb_device_name("Built-in Microphone"));
    ASSERT_FALSE(is_usb_device_name("Realtek High Definition Audio"));
}

TEST(PortAudioDevices_ProjectorDetectsDisplayPort) {
    ASSERT_TRUE(is_projector_or_hdmi("Epson Projector"));
    ASSERT_TRUE(is_projector_or_hdmi("HDMI Display"));
    ASSERT_TRUE(is_projector_or_hdmi("DisplayPort Output"));
    ASSERT_FALSE(is_projector_or_hdmi("Built-in Output"));
}

TEST(PortAudioDevices_HostApiPriority) {
    ASSERT_GT(get_host_api_priority(paJACK), 0);
    ASSERT_GT(get_host_api_priority(paALSA), 0);
    ASSERT_GT(get_host_api_priority(paWASAPI), 0);
    ASSERT_GT(get_host_api_priority(paCoreAudio), 0);
    ASSERT_GT(get_host_api_priority(paInDevelopment), 0);
}

TEST(PortAudioLifecycle_OpenStreamWithNullDevice) {
    AudioEngine engine;
    
    // Force invalid devices
    bool ok_in = engine.set_input_device(paNoDevice);
    bool ok_out = engine.set_output_device(paNoDevice);
    ASSERT_FALSE(ok_in);
    ASSERT_FALSE(ok_out);

    // Should fail because initialized_ is false
    bool started = engine.start();
    ASSERT_FALSE(started);
    ASSERT_FALSE(engine.is_running());
}

TEST(PortAudioLifecycle_StartStopStartCycle) {
    AudioEngine engine;
    engine.initialize();

    bool started = engine.start();
    if (started) {
        ASSERT_TRUE(engine.is_running());
        engine.stop();
        ASSERT_FALSE(engine.is_running());
        
        bool restarted = engine.start();
        ASSERT_TRUE(restarted);
        ASSERT_TRUE(engine.is_running());
    }

    engine.shutdown();
}

TEST(PortAudioLifecycle_Restart) {
    AudioEngine engine;
    engine.initialize();
    engine.restart();
    engine.shutdown();
}

TEST(PortAudioDevices_EnumerateDevicesReturnsVector) {
    AudioEngine engine;
    engine.initialize();

    auto in_devs = engine.get_input_devices();
    auto out_devs = engine.get_output_devices();

    std::string in_name = engine.get_input_device_name();
    std::string out_name = engine.get_output_device_name();
    
    ASSERT_FALSE(in_name.empty());
    ASSERT_FALSE(out_name.empty());
    
    engine.shutdown();
}

TEST(PortAudioDevices_DeviceNamesUninitialized) {
    AudioEngine engine; // uninitialized, input_device_ is -1
    ASSERT_EQ(engine.get_input_device_name(), "None");
    ASSERT_EQ(engine.get_output_device_name(), "None");
}

TEST(PortAudioDevices_DevicesShareHostApiSafe) {
    bool shared = devices_share_host_api(paNoDevice, paNoDevice);
    ASSERT_FALSE(shared);
}

TEST(PortAudioDevices_SetDeviceBranchCoverage) {
    Pa_Initialize(); // Manual init to keep valid state
    
    AudioEngine engine_valid;
    engine_valid.initialize();
    
    auto in_devs = engine_valid.get_input_devices();
    auto out_devs = engine_valid.get_output_devices();
    
    if (!in_devs.empty() && !out_devs.empty()) {
        int valid_in = in_devs[0].index;
        int valid_out = out_devs[0].index;

        // Hit maxChannels < 1 branches
        int output_only = -1;
        for (const auto& dev : out_devs) {
            if (dev.max_input_channels == 0) {
                output_only = dev.index;
                break;
            }
        }
        if (output_only >= 0) {
            engine_valid.set_input_device(output_only);
        }

        int input_only = -1;
        for (const auto& dev : in_devs) {
            if (dev.max_output_channels == 0) {
                input_only = dev.index;
                break;
            }
        }
        if (input_only >= 0) {
            engine_valid.set_output_device(input_only);
        }

        // 1. Hit !devices_share_host_api by leaving output as -1
        AudioEngine engine_uninit;
        engine_uninit.set_input_device(valid_in);
        engine_uninit.set_output_device(valid_out); // also hits !share when input was just set
    }
    
    Pa_Terminate();
}


static PaDeviceInfo mock_info_usb_in;
static PaDeviceInfo mock_info_out;
static PaDeviceInfo mock_info_normal_in;
static PaHostApiInfo mock_api_info;
static PaStreamInfo mock_stream_info;

static void setup_mocks() {
    mock_api_info.type = paCoreAudio;
    mock_api_info.name = "MockAPI";
    mock_api_info.deviceCount = 3;

    mock_info_usb_in.name = "USB Guitar Interface";
    mock_info_usb_in.maxInputChannels = 2;
    mock_info_usb_in.maxOutputChannels = 0;
    mock_info_usb_in.hostApi = 0;
    mock_info_usb_in.defaultSampleRate = 48000;

    mock_info_out.name = "Mock Speakers";
    mock_info_out.maxInputChannels = 0;
    mock_info_out.maxOutputChannels = 2;
    mock_info_out.hostApi = 0;
    mock_info_out.defaultSampleRate = 48000;

    mock_info_normal_in.name = "Internal Mic";
    mock_info_normal_in.maxInputChannels = 2;
    mock_info_normal_in.maxOutputChannels = 0;
    mock_info_normal_in.hostApi = 0;
    mock_info_normal_in.defaultSampleRate = 48000;
    
    mock_stream_info.sampleRate = 44100;
    mock_stream_info.inputLatency = 0.01;
    mock_stream_info.outputLatency = 0.01;

    Amplitron::g_mock_pa_stop_stream = [](PaStream*) -> PaError { return paNoError; };
    Amplitron::g_mock_pa_close_stream = [](PaStream*) -> PaError { return paNoError; };
}

static void clear_mocks() {
    Amplitron::g_mock_pa_get_device_count = nullptr;
    Amplitron::g_mock_pa_get_device_info = nullptr;
    Amplitron::g_mock_pa_get_host_api_info = nullptr;
    Amplitron::g_mock_pa_get_host_api_count = nullptr;
    Amplitron::g_mock_pa_host_api_device_index_to_device_index = nullptr;
    Amplitron::g_mock_pa_get_default_input_device = nullptr;
    Amplitron::g_mock_pa_get_default_output_device = nullptr;
    Amplitron::g_mock_pa_open_stream = nullptr;
    Amplitron::g_mock_pa_start_stream = nullptr;
    Amplitron::g_mock_pa_stop_stream = nullptr;
    Amplitron::g_mock_pa_close_stream = nullptr;
    Amplitron::g_mock_pa_get_stream_info = nullptr;
    Amplitron::g_mock_pa_initialize = nullptr;
}

struct MockGuard {
    MockGuard() { setup_mocks(); }
    ~MockGuard() { clear_mocks(); }
};

namespace Amplitron {
class PortAudioTestSaboteur {
public:
    static void sabotage_and_test() {
        AudioEngine engine;
        engine.initialize();
        
        auto in_devs = engine.get_input_devices();
        auto out_devs = engine.get_output_devices();
        
        if (!in_devs.empty() && !out_devs.empty()) {
            if (engine.start()) {
                // Sabotage the engine so that the next start() fails!
                engine.initialized_ = false;
                
                // Try to set input device. It will stop, set, and then try to start() again.
                // start() will immediately return false because initialized_ is false.
                // It will then hit the revert logic and fail to revert too (since initialized_ is still false).
                bool in_ok = engine.set_input_device(in_devs[0].index);
                ASSERT_FALSE(in_ok);
                
                engine.initialized_ = true;
            }
            
            if (engine.start()) {
                engine.initialized_ = false;
                
                bool out_ok = engine.set_output_device(out_devs[0].index);
                ASSERT_FALSE(out_ok);
                
                engine.initialized_ = true; // Restore for clean shutdown
            }
        }
    }

    static void success_device_set() {
        AudioEngine engine;
        g_mock_pa_initialize = []() -> PaError { return paNoError; };
        g_mock_pa_open_stream = [](PaStream** stream, const PaStreamParameters*, const PaStreamParameters*, double, unsigned long, PaStreamFlags, PaStreamCallback*, void*) -> PaError { 
            *stream = reinterpret_cast<PaStream*>(1);
            return paNoError; 
        };
        g_mock_pa_start_stream = [](PaStream*) -> PaError { return paNoError; };
        g_mock_pa_get_stream_info = [](PaStream*) -> const PaStreamInfo* { return &mock_stream_info; };
        g_mock_pa_get_device_info = [](int idx) -> const PaDeviceInfo* { 
            return idx == 1 ? &mock_info_out : &mock_info_usb_in; 
        };
        g_mock_pa_get_host_api_info = [](int) -> const PaHostApiInfo* { return &mock_api_info; };
        
        ASSERT_TRUE(engine.initialize());
        ASSERT_TRUE(engine.start()); 
        
        ASSERT_TRUE(engine.set_input_device(0));
        ASSERT_TRUE(engine.set_output_device(1));
        
        // Hit !share_host_api with missing out info
        engine.output_device_ = -1;
        engine.set_input_device(0);

        // Hit !share_host_api with missing in info
        engine.input_device_ = -1;
        engine.set_output_device(1);
        
        engine.stop();
    }

    static void devices_coverage() {
        AudioEngine engine;
        engine.initialize(); // running_ is false
        
        g_mock_pa_get_device_count = []() { return 3; };
        g_mock_pa_get_device_info = [](int i) -> const PaDeviceInfo* { 
            if (i == 0) return nullptr;
            if (i == 1) return &mock_info_out; // 0 input, 2 output
            if (i == 2) return &mock_info_usb_in; // 2 input, 0 output
            return nullptr;
        };
        
        // Hits branches in get_input_devices and get_output_devices
        auto in_devs = engine.get_input_devices();
        auto out_devs = engine.get_output_devices();
        ASSERT_EQ(in_devs.size(), 1);
        ASSERT_EQ(out_devs.size(), 1);
        
        // Hit !share_host_api with null APIs to test the warning formatting branch
        g_mock_pa_get_host_api_info = [](int) -> const PaHostApiInfo* { return nullptr; };
        engine.output_device_ = -1; // so out_info is null
        engine.set_input_device(2);
        
        engine.input_device_ = -1; // so in_info is null
        engine.set_output_device(1);
        
        // Hit revert success in set_input_device / set_output_device
        g_mock_pa_initialize = []() -> PaError { return paNoError; };
        g_mock_pa_open_stream = [](PaStream** stream, const PaStreamParameters*, const PaStreamParameters*, double, unsigned long, PaStreamFlags, PaStreamCallback*, void*) -> PaError { 
            *stream = reinterpret_cast<PaStream*>(1);
            return paNoError; 
        };
        g_mock_pa_start_stream = [](PaStream*) -> PaError { return paNoError; };
        g_mock_pa_get_stream_info = [](PaStream*) -> const PaStreamInfo* { return &mock_stream_info; };
        
        engine.input_device_ = 2;
        engine.output_device_ = 1;
        ASSERT_TRUE(engine.start());
        
        static int start_calls = 0;
        start_calls = 0;
        
        g_mock_pa_open_stream = [](PaStream** stream, const PaStreamParameters*, const PaStreamParameters*, double, unsigned long, PaStreamFlags, PaStreamCallback*, void*) -> PaError {
            start_calls++;
            if (start_calls == 2 || start_calls == 3) return paNotInitialized; // Fail on new device start() and its retry
            *stream = reinterpret_cast<PaStream*>(1);
            return paNoError; // Succeed otherwise
        };
        
        // Initial start was success. 
        // set_input_device -> stop() -> start() [fails, start_calls=2] -> revert -> start() [succeeds, start_calls=3]
        start_calls = 1;
        ASSERT_FALSE(engine.set_input_device(2));
        
        start_calls = 1;
        ASSERT_FALSE(engine.set_output_device(1));
        
        engine.stop();
    }
};
} // namespace Amplitron

TEST(PortAudioDevices_SetDeviceSabotage) {
    PortAudioTestSaboteur::sabotage_and_test();
}

namespace Amplitron {
extern int pa_audio_callback(const void* input, void* output,
                             unsigned long frame_count,
                             const PaStreamCallbackTimeInfo* time_info,
                             PaStreamCallbackFlags status_flags,
                             void* user_data);
}

TEST(PortAudioLifecycle_CallbackCoverage) {
    AudioEngine engine;
    engine.initialize();
    
    float in_buf[64] = {0};
    float out_buf[128] = {0};
    
    // Normal processing
    pa_audio_callback(in_buf, out_buf, 64, nullptr, 0, &engine);
    
    // Missing input triggers silence fill
    out_buf[0] = 1.0f;
    pa_audio_callback(nullptr, out_buf, 64, nullptr, 0, &engine);
    ASSERT_EQ(out_buf[0], 0.0f); // Verify memset
    
    // Missing output skips processing
    pa_audio_callback(in_buf, nullptr, 64, nullptr, 0, &engine);
    
    engine.shutdown();
}

// Mocks moved up

TEST(PortAudioLifecycle_MockAutoDetectUSB) {
    MockGuard guard;
    Amplitron::g_mock_pa_get_host_api_count = []() { return 1; };
    Amplitron::g_mock_pa_get_device_count = []() { return 3; };
    Amplitron::g_mock_pa_get_host_api_info = [](int) -> const PaHostApiInfo* { return &mock_api_info; };
    Amplitron::g_mock_pa_host_api_device_index_to_device_index = [](int, int d) { return d; };
    Amplitron::g_mock_pa_get_device_info = [](int i) -> const PaDeviceInfo* {
        if (i == 0) return &mock_info_usb_in;
        if (i == 1) return &mock_info_out;
        if (i == 2) return &mock_info_normal_in;
        return nullptr;
    };
    
    Amplitron::AudioEngine engine;
    engine.initialize(); 
    
    ASSERT_EQ(engine.get_input_device(), 0);
    ASSERT_EQ(engine.get_output_device(), 1);
}

TEST(PortAudioLifecycle_MockAutoDetectNoUSB) {
    MockGuard guard;
    Amplitron::g_mock_pa_get_host_api_count = []() { return 1; };
    Amplitron::g_mock_pa_get_device_count = []() { return 2; };
    Amplitron::g_mock_pa_get_host_api_info = [](int) -> const PaHostApiInfo* { return &mock_api_info; };
    Amplitron::g_mock_pa_host_api_device_index_to_device_index = [](int, int d) { return d + 1; };
    Amplitron::g_mock_pa_get_device_info = [](int i) -> const PaDeviceInfo* {
        if (i == 1) return &mock_info_out;
        if (i == 2) return &mock_info_normal_in;
        return nullptr;
    };
    
    Amplitron::AudioEngine engine;
    engine.initialize(); 
    
    ASSERT_EQ(engine.get_input_device(), 2);
    ASSERT_EQ(engine.get_output_device(), 1);
}

TEST(PortAudioLifecycle_MockAutoDetectSystemDefault) {
    MockGuard guard;
    Amplitron::g_mock_pa_get_host_api_count = []() { return 1; };
    Amplitron::g_mock_pa_get_device_count = []() { return 0; };
    Amplitron::g_mock_pa_get_host_api_info = [](int) -> const PaHostApiInfo* { return &mock_api_info; };
    Amplitron::g_mock_pa_host_api_device_index_to_device_index = [](int, int) { return -1; };
    Amplitron::g_mock_pa_get_device_info = [](int) -> const PaDeviceInfo* { return nullptr; };
    Amplitron::g_mock_pa_get_default_input_device = []() { return 99; };
    Amplitron::g_mock_pa_get_default_output_device = []() { return 98; };
    
    Amplitron::AudioEngine engine;
    engine.initialize(); 
    
    ASSERT_EQ(engine.get_input_device(), 99);
    ASSERT_EQ(engine.get_output_device(), 98);
}

TEST(PortAudioLifecycle_MockInitializeFail) {
    MockGuard guard;
    Amplitron::g_mock_pa_initialize = []() -> PaError { return paNotInitialized; };
    Amplitron::AudioEngine engine;
    ASSERT_FALSE(engine.initialize());
}

TEST(PortAudioLifecycle_MockStartFailAndRetry) {
    MockGuard guard;
    Amplitron::g_mock_pa_initialize = []() -> PaError { return paNoError; };
    Amplitron::AudioEngine engine;
    engine.initialize();
    
    static int open_count = 0;
    open_count = 0;
    Amplitron::g_mock_pa_open_stream = [](PaStream**, const PaStreamParameters*, const PaStreamParameters*, double, unsigned long, PaStreamFlags, PaStreamCallback*, void*) -> PaError {
        open_count++;
        return paNotInitialized; 
    };
    
    ASSERT_FALSE(engine.start());
    ASSERT_EQ(open_count, 2); 
}

TEST(PortAudioLifecycle_MockStartFailSecond) {
    MockGuard guard;
    Amplitron::g_mock_pa_initialize = []() -> PaError { return paNoError; };
    Amplitron::AudioEngine engine;
    engine.initialize();
    
    Amplitron::g_mock_pa_open_stream = [](PaStream** stream, const PaStreamParameters*, const PaStreamParameters*, double, unsigned long, PaStreamFlags, PaStreamCallback*, void*) -> PaError {
        *stream = reinterpret_cast<PaStream*>(1);
        return paNoError;
    };
    Amplitron::g_mock_pa_start_stream = [](PaStream*) -> PaError {
        return paNotInitialized; 
    };
    
    ASSERT_FALSE(engine.start());
}

TEST(PortAudioLifecycle_MockStartSampleRateMismatch) {
    MockGuard guard;
    Amplitron::g_mock_pa_initialize = []() -> PaError { return paNoError; };
    Amplitron::AudioEngine engine;
    engine.initialize();
    engine.set_sample_rate(48000);
    
    Amplitron::g_mock_pa_open_stream = [](PaStream** stream, const PaStreamParameters*, const PaStreamParameters*, double, unsigned long, PaStreamFlags, PaStreamCallback*, void*) -> PaError {
        *stream = reinterpret_cast<PaStream*>(1);
        return paNoError;
    };
    Amplitron::g_mock_pa_start_stream = [](PaStream*) -> PaError {
        return paNoError;
    };
    Amplitron::g_mock_pa_get_stream_info = [](PaStream*) -> const PaStreamInfo* {
        return &mock_stream_info; 
    };
    Amplitron::g_mock_pa_get_device_info = [](int) -> const PaDeviceInfo* { return &mock_info_usb_in; };
    
    ASSERT_TRUE(engine.start());
    ASSERT_EQ(engine.get_sample_rate(), 44100);
    engine.stop();
}

TEST(PortAudioDevices_SetDeviceSuccess) {
    MockGuard guard;
    Amplitron::PortAudioTestSaboteur::success_device_set();
}

TEST(PortAudioDevices_DevicesCoverage) {
    MockGuard guard;
    Amplitron::PortAudioTestSaboteur::devices_coverage();
}
#else
#include "test_framework.h"
TEST(AudioBackend_PortAudio_NotAvailable) {
  ASSERT_TRUE(true);
}
#endif
