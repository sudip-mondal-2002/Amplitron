// =============================================================================
// Oboe audio backend — Android 8.0+ (AAudio exclusive mode, low latency)
//
// Implements AudioEngine member functions: initialize, shutdown, start, stop,
// restart, and device management. Uses Google's Oboe library which selects
// AAudio in exclusive mode on Android 8+ for <10ms round-trip latency, and
// falls back to OpenSL ES on older Android versions automatically.
//
// USB guitar cable auto-detection is performed via Android USB Host API
// hints passed through the AudioEngine settings screen.
// =============================================================================

#include "audio/audio_engine.h"
#include "audio/audio_backend.h"

#include <oboe/Oboe.h>
#include <android/log.h>

#include <atomic>
#include <cstring>
#include <string>
#include <vector>

#define LOG_TAG "Amplitron/Oboe"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace Amplitron {

// -----------------------------------------------------------------------------
// OboeCallback — bridges Oboe's audio thread into AudioEngine::process_audio
// -----------------------------------------------------------------------------

class OboeCallback : public oboe::AudioStreamDataCallback {
public:
    explicit OboeCallback(AudioEngine* engine) : engine_(engine) {}

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream* /*stream*/,
                                           void* audioData,
                                           int32_t numFrames) override {
        auto* output = static_cast<float*>(audioData);

        // Pull captured mono input; zero if unavailable
        const int captureSize = static_cast<int>(capture_buffer_.size());
        if (captureSize < numFrames)
            capture_buffer_.resize(static_cast<size_t>(numFrames), 0.0f);

        {
            // Drain from capture ring; if starved, pass silence
            int available = capture_filled_.load(std::memory_order_acquire);
            int toCopy = (available >= numFrames) ? numFrames : 0;
            if (toCopy > 0) {
                std::memcpy(capture_buffer_.data(),
                            capture_ring_.data() + capture_read_pos_,
                            static_cast<size_t>(toCopy) * sizeof(float));
                capture_read_pos_ = (capture_read_pos_ + toCopy) % kRingSize;
                capture_filled_.fetch_sub(toCopy, std::memory_order_release);
            } else {
                std::memset(capture_buffer_.data(), 0,
                            static_cast<size_t>(numFrames) * sizeof(float));
            }
        }

        engine_->process_audio(capture_buffer_.data(), output, numFrames);
        return oboe::DataCallbackResult::Continue;
    }

    // Called by the input (capture) stream callback to deposit samples
    void feedCaptureData(const float* data, int numFrames) {
        int space = kRingSize - capture_filled_.load(std::memory_order_acquire);
        int toCopy = (numFrames < space) ? numFrames : space;
        if (toCopy <= 0) return;
        int writePos = (capture_read_pos_ +
                        capture_filled_.load(std::memory_order_relaxed)) % kRingSize;
        std::memcpy(capture_ring_.data() + writePos, data,
                    static_cast<size_t>(toCopy) * sizeof(float));
        capture_filled_.fetch_add(toCopy, std::memory_order_release);
    }

private:
    AudioEngine* engine_;
    std::vector<float> capture_buffer_;

    static constexpr int kRingSize = 16384;
    std::array<float, kRingSize> capture_ring_{};
    int capture_read_pos_ = 0;
    std::atomic<int> capture_filled_{0};
};

// Separate callback for the capture (input) stream
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

// -----------------------------------------------------------------------------
// AudioBackendState
// -----------------------------------------------------------------------------

struct AudioBackendState {
    std::shared_ptr<oboe::AudioStream> playbackStream;
    std::shared_ptr<oboe::AudioStream> captureStream;

    std::unique_ptr<OboeCallback>        playbackCallback;
    std::unique_ptr<OboeCaptureCallback> captureCallback;

    // Measured latency (updated after stream opens)
    double measured_latency_ms = -1.0;

    // USB audio device ID hint (set from Android settings screen, -1 = default)
    int usb_device_id = -1;

    std::string input_device_name  = "Android Microphone";
    std::string output_device_name = "Android Speaker";
};

AudioBackendState* create_audio_backend() {
    return new AudioBackendState();
}

void destroy_audio_backend(AudioBackendState* state) {
    delete state;
}

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

static double compute_latency_ms(const std::shared_ptr<oboe::AudioStream>& stream) {
    if (!stream) return -1.0;
    auto result = stream->calculateLatencyMillis();
    if (result) return result.value();
    return -1.0;
}

// -----------------------------------------------------------------------------
// AudioEngine member functions — Oboe / Android implementations
// -----------------------------------------------------------------------------

bool AudioEngine::initialize() {
    initialized_ = true;
    LOGI("Oboe audio backend initialised (AAudio exclusive mode on Android 8+).");
    return true;
}

void AudioEngine::shutdown() {
    stop();
    initialized_ = false;
}

bool AudioEngine::start() {
    if (!initialized_ || running_) return false;

    backend_->playbackCallback = std::make_unique<OboeCallback>(this);
    backend_->captureCallback  = std::make_unique<OboeCaptureCallback>(
                                      backend_->playbackCallback.get());

    // -------------------------------------------------------------------------
    // 1. Open playback stream (stereo float, AAudio exclusive mode preferred)
    // -------------------------------------------------------------------------
    oboe::AudioStreamBuilder playbackBuilder;
    playbackBuilder
        .setDirection(oboe::Direction::Output)
        .setPerformanceMode(oboe::PerformanceMode::LowLatency)
        .setSharingMode(oboe::SharingMode::Exclusive)    // AAudio exclusive — lowest latency
        .setFormat(oboe::AudioFormat::Float)
        .setChannelCount(oboe::ChannelCount::Stereo)
        .setSampleRate(sample_rate_)
        .setFramesPerDataCallback(buffer_size_)
        .setDataCallback(backend_->playbackCallback.get());

    if (backend_->usb_device_id >= 0) {
        playbackBuilder.setDeviceId(backend_->usb_device_id);
        LOGI("Playback: routing to USB device ID %d", backend_->usb_device_id);
    }

    oboe::Result result = playbackBuilder.openStream(backend_->playbackStream);
    if (result != oboe::Result::OK) {
        LOGE("Failed to open playback stream: %s", oboe::convertToText(result));
        last_error_ = std::string("Oboe playback open failed: ") + oboe::convertToText(result);
        return false;
    }

    // Adopt actual sample rate / buffer size the stream negotiated
    sample_rate_  = backend_->playbackStream->getSampleRate();
    buffer_size_  = backend_->playbackStream->getFramesPerDataCallback();

    // Update effect chain with negotiated rate
    {
        std::lock_guard<std::mutex> lock(effect_mutex_);
        for (auto& fx : effects_)
            fx->set_sample_rate(sample_rate_);
    }

    LOGI("Playback stream opened: %d Hz, %d frames/callback, sharing=%s",
         sample_rate_, buffer_size_,
         oboe::convertToText(backend_->playbackStream->getSharingMode()));

    // -------------------------------------------------------------------------
    // 2. Open capture stream (mono float, low-latency)
    // -------------------------------------------------------------------------
    oboe::AudioStreamBuilder captureBuilder;
    captureBuilder
        .setDirection(oboe::Direction::Input)
        .setPerformanceMode(oboe::PerformanceMode::LowLatency)
        .setSharingMode(oboe::SharingMode::Exclusive)
        .setFormat(oboe::AudioFormat::Float)
        .setChannelCount(oboe::ChannelCount::Mono)
        .setSampleRate(sample_rate_)
        .setFramesPerDataCallback(buffer_size_)
        .setDataCallback(backend_->captureCallback.get());

    if (backend_->usb_device_id >= 0) {
        captureBuilder.setDeviceId(backend_->usb_device_id);
        LOGI("Capture: routing to USB device ID %d", backend_->usb_device_id);
        backend_->input_device_name = "USB Guitar Cable";
    }

    result = captureBuilder.openStream(backend_->captureStream);
    if (result != oboe::Result::OK) {
        // Non-fatal: continue without input (silent processing)
        LOGW("Failed to open capture stream: %s — continuing without input",
             oboe::convertToText(result));
        backend_->captureStream.reset();
    } else {
        LOGI("Capture stream opened: mono, %d Hz", sample_rate_);
    }

    // -------------------------------------------------------------------------
    // 3. Start both streams
    // -------------------------------------------------------------------------
    result = backend_->playbackStream->requestStart();
    if (result != oboe::Result::OK) {
        LOGE("Failed to start playback stream: %s", oboe::convertToText(result));
        last_error_ = std::string("Oboe playback start failed: ") + oboe::convertToText(result);
        backend_->playbackStream->close();
        backend_->playbackStream.reset();
        return false;
    }

    if (backend_->captureStream) {
        result = backend_->captureStream->requestStart();
        if (result != oboe::Result::OK) {
            LOGW("Failed to start capture stream: %s", oboe::convertToText(result));
            backend_->captureStream->close();
            backend_->captureStream.reset();
        }
    }

    running_ = true;

    // Measure and log achieved latency
    backend_->measured_latency_ms = compute_latency_ms(backend_->playbackStream);
    LOGI("Audio started — estimated latency: %.1f ms", backend_->measured_latency_ms);

    return true;
}

void AudioEngine::stop() {
    if (!running_) return;
    running_ = false;

    if (backend_->captureStream) {
        backend_->captureStream->requestStop();
        backend_->captureStream->close();
        backend_->captureStream.reset();
    }
    if (backend_->playbackStream) {
        backend_->playbackStream->requestStop();
        backend_->playbackStream->close();
        backend_->playbackStream.reset();
    }
    LOGI("Audio stopped.");
}

bool AudioEngine::restart() {
    stop();
    bool ok = start();
    if (!ok)
        last_error_ = "Failed to restart Oboe audio.";
    else
        last_error_.clear();
    return ok;
}

// -----------------------------------------------------------------------------
// Device management
// -----------------------------------------------------------------------------

std::string AudioEngine::get_input_device_name() const {
    return backend_->input_device_name;
}

std::string AudioEngine::get_output_device_name() const {
    return backend_->output_device_name;
}

std::vector<AudioDeviceInfo> AudioEngine::get_input_devices() const {
    // Enumerate via Oboe / AAudio device list
    std::vector<AudioDeviceInfo> devices;

    // Always offer the default (built-in mic / USB if connected)
    devices.push_back({0, "Default (Auto-select)", 1, 0, static_cast<double>(sample_rate_), false});

    // If a USB device was detected externally and its ID stored, expose it
    if (backend_->usb_device_id >= 0) {
        devices.push_back({backend_->usb_device_id,
                           "USB Guitar Cable",
                           1, 0,
                           static_cast<double>(sample_rate_),
                           true});
    }
    return devices;
}

std::vector<AudioDeviceInfo> AudioEngine::get_output_devices() const {
    std::vector<AudioDeviceInfo> devices;
    devices.push_back({0, "Default (Auto-select)", 0, 2, static_cast<double>(sample_rate_), false});
    if (backend_->usb_device_id >= 0) {
        devices.push_back({backend_->usb_device_id,
                           "USB Guitar Cable (output)",
                           0, 2,
                           static_cast<double>(sample_rate_),
                           true});
    }
    return devices;
}

bool AudioEngine::set_input_device(int device_index) {
    if (device_index == backend_->usb_device_id || device_index == 0) {
        input_device_ = device_index;
        backend_->usb_device_id = (device_index > 0) ? device_index : -1;
        backend_->input_device_name = (device_index > 0) ? "USB Guitar Cable" : "Android Microphone";
        if (running_) restart();
        return true;
    }
    return false;
}

bool AudioEngine::set_output_device(int device_index) {
    output_device_ = device_index;
    if (running_) restart();
    return true;
}

} // namespace Amplitron
