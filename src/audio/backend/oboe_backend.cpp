#include "audio/backend/oboe_backend.h"
#include "audio/engine/i_audio_engine.h"

#ifdef AMPLITRON_ANDROID_OBOE

#include <oboe/Oboe.h>
#include <android/log.h>
#include <vector>
#include <array>
#include <cstring>
#include <atomic>

#define LOG_TAG "Amplitron/Oboe"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace Amplitron {

class OboeCallback : public oboe::AudioStreamDataCallback {
public:
    explicit OboeCallback(IAudioEngine* engine) : engine_(engine) {}

    void preallocate(int framesPerCallback) {
        capture_buffer_.assign(static_cast<size_t>(framesPerCallback), 0.0f);
    }

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream* /*stream*/,
                                           void* audioData,
                                           int32_t numFrames) override {
        auto* output = static_cast<float*>(audioData);

        const int bufSize = static_cast<int>(capture_buffer_.size());
        if (bufSize < numFrames) {
            std::memset(output, 0, static_cast<size_t>(numFrames) * 2 * sizeof(float));
            return oboe::DataCallbackResult::Continue;
        }

        int available = capture_filled_.load(std::memory_order_acquire);
        if (available >= numFrames) {
            int read_pos = capture_read_pos_.load(std::memory_order_acquire);
            int firstChunk = std::min(numFrames, kRingSize - read_pos);
            std::memcpy(capture_buffer_.data(),
                        capture_ring_.data() + read_pos,
                        static_cast<size_t>(firstChunk) * sizeof(float));
            if (firstChunk < numFrames) {
                int secondChunk = numFrames - firstChunk;
                std::memcpy(capture_buffer_.data() + firstChunk,
                            capture_ring_.data(),
                            static_cast<size_t>(secondChunk) * sizeof(float));
            }
            capture_read_pos_.store((read_pos + numFrames) % kRingSize, std::memory_order_release);
            capture_filled_.fetch_sub(numFrames, std::memory_order_release);
        } else {
            std::memset(capture_buffer_.data(), 0,
                        static_cast<size_t>(numFrames) * sizeof(float));
        }

        engine_->process_audio(capture_buffer_.data(), output, numFrames);
        return oboe::DataCallbackResult::Continue;
    }

    void feedCaptureData(const float* data, int numFrames) {
        int space = kRingSize - capture_filled_.load(std::memory_order_acquire);
        int toCopy = std::min(numFrames, space);
        if (toCopy <= 0) return;

        int write_pos = capture_write_pos_.load(std::memory_order_acquire);
        int firstChunk = std::min(toCopy, kRingSize - write_pos);
        std::memcpy(capture_ring_.data() + write_pos,
                    data,
                    static_cast<size_t>(firstChunk) * sizeof(float));
        if (firstChunk < toCopy) {
            int secondChunk = toCopy - firstChunk;
            std::memcpy(capture_ring_.data(),
                        data + firstChunk,
                        static_cast<size_t>(secondChunk) * sizeof(float));
        }
        capture_write_pos_.store((write_pos + toCopy) % kRingSize, std::memory_order_release);
        capture_filled_.fetch_add(toCopy, std::memory_order_release);
    }

    oboe::SharingMode get_sharing_mode() const { return negotiated_sharing_mode_; }
    void set_sharing_mode(oboe::SharingMode m) { negotiated_sharing_mode_ = m; }

private:
    IAudioEngine* engine_;
    std::vector<float> capture_buffer_;

    static constexpr int kRingSize = 16384;
    std::array<float, kRingSize> capture_ring_{};
    std::atomic<int> capture_read_pos_{0};
    std::atomic<int> capture_write_pos_{0};
    std::atomic<int> capture_filled_{0};

    oboe::SharingMode negotiated_sharing_mode_ = oboe::SharingMode::Shared;
};

class OboeCaptureCallback : public oboe::AudioStreamDataCallback {
public:
    explicit OboeCaptureCallback(OboeCallback* sink) : sink_(sink) {}

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream* /*stream*/,
                                           void* audioData,
                                           int32_t numFrames) override {
        sink_->feedCaptureData(static_cast<const float*>(audioData), numFrames);
        return oboe::DataCallbackResult::Continue;
    }

private:
    OboeCallback* sink_;
};

struct OboeBackendImpl {
    std::shared_ptr<oboe::AudioStream> playbackStream;
    std::shared_ptr<oboe::AudioStream> captureStream;

    std::unique_ptr<OboeCallback>        playbackCallback;
    std::unique_ptr<OboeCaptureCallback> captureCallback;

    double measured_latency_ms = -1.0;

    int usb_input_device_id  = -1;
    int usb_output_device_id = -1;

    std::string input_device_name  = "Android Microphone";
    std::string output_device_name = "Android Speaker";
};

static void close_stream(std::shared_ptr<oboe::AudioStream>& stream) {
    if (stream) {
        stream->requestStop();
        stream->close();
        stream.reset();
    }
}

OboeBackend::OboeBackend() {
    impl_ = new OboeBackendImpl();
}

OboeBackend::~OboeBackend() {
    shutdown();
    delete static_cast<OboeBackendImpl*>(impl_);
}

bool OboeBackend::initialize(IAudioEngine* engine) {
    if (initialized_) return true;
    engine_ = engine;
    initialized_ = true;
    LOGI("Oboe audio backend initialised.");
    return true;
}

void OboeBackend::shutdown() {
    stop();
    initialized_ = false;
}

bool OboeBackend::start() {
    if (!initialized_ || running_) return false;

    auto* impl = static_cast<OboeBackendImpl*>(impl_);
    impl->playbackCallback = std::make_unique<OboeCallback>(engine_);
    impl->captureCallback  = std::make_unique<OboeCaptureCallback>(impl->playbackCallback.get());

    int target_buffer = engine_->get_buffer_size();
    int target_rate = engine_->get_sample_rate();

    oboe::AudioStreamBuilder playbackBuilder;
    playbackBuilder.setDirection(oboe::Direction::Output);
    playbackBuilder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
    playbackBuilder.setSharingMode(oboe::SharingMode::Exclusive);
    playbackBuilder.setFormat(oboe::AudioFormat::Float);
    playbackBuilder.setChannelCount(oboe::ChannelCount::Stereo);
    playbackBuilder.setSampleRate(target_rate);
    playbackBuilder.setFramesPerDataCallback(target_buffer);
    playbackBuilder.setDataCallback(impl->playbackCallback.get());

    if (impl->usb_output_device_id >= 0) {
        playbackBuilder.setDeviceId(impl->usb_output_device_id);
    }

    oboe::Result result = playbackBuilder.openStream(impl->playbackStream);
    if (result != oboe::Result::OK) {
        LOGE("Failed to open playback stream: %s", oboe::convertToText(result));
        return false;
    }

    oboe::SharingMode actualMode = impl->playbackStream->getSharingMode();
    impl->playbackCallback->set_sharing_mode(actualMode);

    sample_rate_ = impl->playbackStream->getSampleRate();
    buffer_size_ = impl->playbackStream->getFramesPerDataCallback();

    impl->playbackCallback->preallocate(buffer_size_);

    oboe::AudioStreamBuilder captureBuilder;
    captureBuilder.setDirection(oboe::Direction::Input);
    captureBuilder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
    captureBuilder.setSharingMode(oboe::SharingMode::Exclusive);
    captureBuilder.setFormat(oboe::AudioFormat::Float);
    captureBuilder.setChannelCount(oboe::ChannelCount::Mono);
    captureBuilder.setSampleRate(sample_rate_);
    captureBuilder.setFramesPerDataCallback(buffer_size_);
    captureBuilder.setDataCallback(impl->captureCallback.get());

    if (impl->usb_input_device_id >= 0) {
        captureBuilder.setDeviceId(impl->usb_input_device_id);
        impl->input_device_name = "USB Guitar Cable";
    }

    result = captureBuilder.openStream(impl->captureStream);
    if (result != oboe::Result::OK) {
        LOGW("Failed to open capture stream: %s", oboe::convertToText(result));
        impl->captureStream.reset();
    }

    result = impl->playbackStream->requestStart();
    if (result != oboe::Result::OK) {
        LOGE("Failed to start playback stream: %s", oboe::convertToText(result));
        close_stream(impl->captureStream);
        close_stream(impl->playbackStream);
        return false;
    }

    if (impl->captureStream) {
        result = impl->captureStream->requestStart();
        if (result != oboe::Result::OK) {
            close_stream(impl->captureStream);
        }
    }

    running_ = true;
    return true;
}

void OboeBackend::stop() {
    if (!running_) return;
    running_ = false;
    auto* impl = static_cast<OboeBackendImpl*>(impl_);
    close_stream(impl->captureStream);
    close_stream(impl->playbackStream);
}

std::vector<AudioDeviceInfo> OboeBackend::get_input_devices() const {
    auto* impl = static_cast<OboeBackendImpl*>(impl_);
    std::vector<AudioDeviceInfo> devices;
    devices.push_back({0, "Default (Auto-select)", 1, 0, static_cast<double>(sample_rate_), false});
    if (impl->usb_input_device_id >= 0) {
        devices.push_back({impl->usb_input_device_id, "USB Guitar Cable", 1, 0, static_cast<double>(sample_rate_), true});
    }
    return devices;
}

std::vector<AudioDeviceInfo> OboeBackend::get_output_devices() const {
    auto* impl = static_cast<OboeBackendImpl*>(impl_);
    std::vector<AudioDeviceInfo> devices;
    devices.push_back({0, "Default (Auto-select)", 0, 2, static_cast<double>(sample_rate_), false});
    if (impl->usb_output_device_id >= 0) {
        devices.push_back({impl->usb_output_device_id, "USB Guitar Cable (output)", 0, 2, static_cast<double>(sample_rate_), true});
    }
    return devices;
}

bool OboeBackend::set_input_device(int device_index) {
    auto* impl = static_cast<OboeBackendImpl*>(impl_);
    impl->usb_input_device_id = (device_index > 0) ? device_index : -1;
    impl->input_device_name   = (device_index > 0) ? "USB Guitar Cable" : "Android Microphone";
    return true;
}

bool OboeBackend::set_output_device(int device_index) {
    auto* impl = static_cast<OboeBackendImpl*>(impl_);
    impl->usb_output_device_id = (device_index > 0) ? device_index : -1;
    impl->output_device_name   = (device_index > 0) ? "USB Guitar Cable (output)" : "Android Speaker";
    return true;
}

std::string OboeBackend::get_input_device_name() const {
    return static_cast<OboeBackendImpl*>(impl_)->input_device_name;
}

std::string OboeBackend::get_output_device_name() const {
    return static_cast<OboeBackendImpl*>(impl_)->output_device_name;
}

int OboeBackend::get_sample_rate() const {
    return sample_rate_;
}

int OboeBackend::get_buffer_size() const {
    return buffer_size_;
}

const char* OboeBackend::get_oboe_sharing_mode_label() const {
    auto* impl = static_cast<OboeBackendImpl*>(impl_);
    if (!impl || !impl->playbackCallback) return "Oboe";
    return (impl->playbackCallback->get_sharing_mode() == oboe::SharingMode::Exclusive)
           ? "AAudio exclusive mode"
           : "OpenSL ES (shared mode)";
}

} // namespace Amplitron

#else

namespace Amplitron {

OboeBackend::OboeBackend() = default;
OboeBackend::~OboeBackend() = default;
bool OboeBackend::initialize(IAudioEngine*) { return false; }
void OboeBackend::shutdown() {}
bool OboeBackend::start() { return false; }
void OboeBackend::stop() {}
std::vector<AudioDeviceInfo> OboeBackend::get_input_devices() const { return {}; }
std::vector<AudioDeviceInfo> OboeBackend::get_output_devices() const { return {}; }
bool OboeBackend::set_input_device(int) { return false; }
bool OboeBackend::set_output_device(int) { return false; }
std::string OboeBackend::get_input_device_name() const { return ""; }
std::string OboeBackend::get_output_device_name() const { return ""; }
int OboeBackend::get_sample_rate() const { return sample_rate_; }
int OboeBackend::get_buffer_size() const { return buffer_size_; }
const char* OboeBackend::get_oboe_sharing_mode_label() const { return ""; }

} // namespace Amplitron

#endif
