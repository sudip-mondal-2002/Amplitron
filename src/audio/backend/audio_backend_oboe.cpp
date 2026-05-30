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

#include "audio/engine/audio_engine.h"
#include "audio/backend/audio_backend.h"

#include <oboe/Oboe.h>
#include <android/log.h>

#include <atomic>
#include <array>
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
//
// Ring buffer design (fix: coderabbit — wrap-around safety):
//   capture_write_pos_ is owned exclusively by the capture (producer) thread.
//   capture_read_pos_  is owned exclusively by the playback (consumer) thread.
//   capture_filled_    is an atomic count shared between both threads.
//   Both positions are always advanced modulo kRingSize and copies are split
//   at the buffer boundary so no read/write ever crosses kRingSize.
// -----------------------------------------------------------------------------

class OboeCallback : public oboe::AudioStreamDataCallback {
public:
    explicit OboeCallback(AudioEngine* engine) : engine_(engine) {}

    // Pre-size the scratch buffer to avoid heap allocation on the audio thread.
    // Called once after the stream is opened with the negotiated callback size.
    // Fix: coderabbit — avoid allocating in onAudioReady()
    void preallocate(int framesPerCallback) {
        capture_buffer_.assign(static_cast<size_t>(framesPerCallback), 0.0f);
    }

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream* /*stream*/,
                                           void* audioData,
                                           int32_t numFrames) override {
        auto* output = static_cast<float*>(audioData);

        // Scratch buffer is pre-sized; no heap allocation here.
        const int bufSize = static_cast<int>(capture_buffer_.size());
        if (bufSize < numFrames) {
            // Safety fallback: should not happen if preallocate() was called.
            std::memset(output, 0, static_cast<size_t>(numFrames) * 2 * sizeof(float));
            return oboe::DataCallbackResult::Continue;
        }

        // Drain from ring buffer; pass silence if starved.
        int available = capture_filled_.load(std::memory_order_acquire);
        if (available >= numFrames) {
            // Fix: coderabbit — split copy at wrap-around boundary
            int firstChunk = std::min(numFrames, kRingSize - capture_read_pos_);
            std::memcpy(capture_buffer_.data(),
                        capture_ring_.data() + capture_read_pos_,
                        static_cast<size_t>(firstChunk) * sizeof(float));
            if (firstChunk < numFrames) {
                int secondChunk = numFrames - firstChunk;
                std::memcpy(capture_buffer_.data() + firstChunk,
                            capture_ring_.data(),
                            static_cast<size_t>(secondChunk) * sizeof(float));
            }
            capture_read_pos_ = (capture_read_pos_ + numFrames) % kRingSize;
            capture_filled_.fetch_sub(numFrames, std::memory_order_release);
        } else {
            std::memset(capture_buffer_.data(), 0,
                        static_cast<size_t>(numFrames) * sizeof(float));
        }

        engine_->process_audio(capture_buffer_.data(), output, numFrames);
        return oboe::DataCallbackResult::Continue;
    }

    // Called by the capture stream callback (producer thread) to deposit samples.
    // Fix: coderabbit — separate write position owned by producer; split copy at wrap-around
    void feedCaptureData(const float* data, int numFrames) {
        int space = kRingSize - capture_filled_.load(std::memory_order_acquire);
        int toCopy = std::min(numFrames, space);
        if (toCopy <= 0) return;

        // Split copy at wrap-around boundary
        int firstChunk = std::min(toCopy, kRingSize - capture_write_pos_);
        std::memcpy(capture_ring_.data() + capture_write_pos_,
                    data,
                    static_cast<size_t>(firstChunk) * sizeof(float));
        if (firstChunk < toCopy) {
            int secondChunk = toCopy - firstChunk;
            std::memcpy(capture_ring_.data(),
                        data + firstChunk,
                        static_cast<size_t>(secondChunk) * sizeof(float));
        }
        capture_write_pos_ = (capture_write_pos_ + toCopy) % kRingSize;
        capture_filled_.fetch_add(toCopy, std::memory_order_release);
    }

    // Returns the actual sharing mode Oboe negotiated (AAudio exclusive or shared/OpenSL).
    // Fix: coderabbit — expose runtime negotiated mode so UI is not hardcoded
    oboe::SharingMode get_sharing_mode() const { return negotiated_sharing_mode_; }
    void set_sharing_mode(oboe::SharingMode m) { negotiated_sharing_mode_ = m; }

private:
    AudioEngine* engine_;
    std::vector<float> capture_buffer_;  // pre-sized scratch; no alloc on audio thread

    static constexpr int kRingSize = 16384;
    std::array<float, kRingSize> capture_ring_{};
    int capture_read_pos_  = 0;   // consumer (playback callback) only
    int capture_write_pos_ = 0;   // producer (capture callback) only
    std::atomic<int> capture_filled_{0};

    oboe::SharingMode negotiated_sharing_mode_ = oboe::SharingMode::Shared;
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

    double measured_latency_ms = -1.0;

    // Fix: coderabbit — separate input and output device IDs
    int usb_input_device_id  = -1;
    int usb_output_device_id = -1;

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

static void close_stream(std::shared_ptr<oboe::AudioStream>& stream) {
    if (stream) {
        stream->requestStop();
        stream->close();
        stream.reset();
    }
}

// -----------------------------------------------------------------------------
// AudioEngine member functions — Oboe / Android implementations
// -----------------------------------------------------------------------------

bool AudioEngine::initialize() {
    initialized_ = true;
    LOGI("Oboe audio backend initialised (AAudio exclusive mode preferred on Android 8+).");
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
    // 1. Open playback stream
    // -------------------------------------------------------------------------
    oboe::AudioStreamBuilder playbackBuilder;
    playbackBuilder.setDirection(oboe::Direction::Output);
    playbackBuilder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
    playbackBuilder.setSharingMode(oboe::SharingMode::Exclusive);
    playbackBuilder.setFormat(oboe::AudioFormat::Float);
    playbackBuilder.setChannelCount(oboe::ChannelCount::Stereo);
    playbackBuilder.setSampleRate(sample_rate_);
    playbackBuilder.setFramesPerDataCallback(buffer_size_);
    playbackBuilder.setDataCallback(backend_->playbackCallback.get());

    // Fix: coderabbit — use dedicated output device ID
    if (backend_->usb_output_device_id >= 0) {
        playbackBuilder.setDeviceId(backend_->usb_output_device_id);
        LOGI("Playback: routing to USB output device ID %d", backend_->usb_output_device_id);
    }

    oboe::Result result = playbackBuilder.openStream(backend_->playbackStream);
    if (result != oboe::Result::OK) {
        LOGE("Failed to open playback stream: %s", oboe::convertToText(result));
        last_error_ = std::string("Oboe playback open failed: ") + oboe::convertToText(result);
        return false;
    }

    // Store negotiated sharing mode for accurate UI display
    oboe::SharingMode actualMode = backend_->playbackStream->getSharingMode();
    backend_->playbackCallback->set_sharing_mode(actualMode);

    sample_rate_ = backend_->playbackStream->getSampleRate();
    buffer_size_ = backend_->playbackStream->getFramesPerDataCallback();

    // Fix: coderabbit — pre-size capture scratch buffer before requestStart()
    backend_->playbackCallback->preallocate(buffer_size_);

    {
        std::lock_guard<std::mutex> lock(effect_mutex_);
        for (auto& node : main_graph_.get_nodes()) {
            if (node.pedal) {
            node.pedal->set_sample_rate(sample_rate_);
            }  
        }
    }

    LOGI("Playback stream opened: %d Hz, %d frames/callback, sharing=%s",
         sample_rate_, buffer_size_,
         oboe::convertToText(actualMode));

    // -------------------------------------------------------------------------
    // 2. Open capture stream
    // -------------------------------------------------------------------------
    oboe::AudioStreamBuilder captureBuilder;
    captureBuilder.setDirection(oboe::Direction::Input);
    captureBuilder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
    captureBuilder.setSharingMode(oboe::SharingMode::Exclusive);
    captureBuilder.setFormat(oboe::AudioFormat::Float);
    captureBuilder.setChannelCount(oboe::ChannelCount::Mono);
    captureBuilder.setSampleRate(sample_rate_);
    captureBuilder.setFramesPerDataCallback(buffer_size_);
    captureBuilder.setDataCallback(backend_->captureCallback.get());

    // Fix: coderabbit — use dedicated input device ID
    if (backend_->usb_input_device_id >= 0) {
        captureBuilder.setDeviceId(backend_->usb_input_device_id);
        LOGI("Capture: routing to USB input device ID %d", backend_->usb_input_device_id);
        backend_->input_device_name = "USB Guitar Cable";
    }

    result = captureBuilder.openStream(backend_->captureStream);
    if (result != oboe::Result::OK) {
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
        // Fix: coderabbit — close capture stream too on playback-start failure
        close_stream(backend_->captureStream);
        close_stream(backend_->playbackStream);
        return false;
    }

    if (backend_->captureStream) {
        result = backend_->captureStream->requestStart();
        if (result != oboe::Result::OK) {
            LOGW("Failed to start capture stream: %s", oboe::convertToText(result));
            close_stream(backend_->captureStream);
        }
    }

    running_ = true;
    backend_->measured_latency_ms = compute_latency_ms(backend_->playbackStream);
    LOGI("Audio started — estimated latency: %.1f ms", backend_->measured_latency_ms);

    return true;
}

void AudioEngine::stop() {
    if (!running_) return;
    running_ = false;
    close_stream(backend_->captureStream);
    close_stream(backend_->playbackStream);
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
    std::vector<AudioDeviceInfo> devices;
    devices.push_back({0, "Default (Auto-select)", 1, 0, static_cast<double>(sample_rate_), false});
    if (backend_->usb_input_device_id >= 0) {
        devices.push_back({backend_->usb_input_device_id,
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
    // Fix: coderabbit — output devices use their own ID
    if (backend_->usb_output_device_id >= 0) {
        devices.push_back({backend_->usb_output_device_id,
                           "USB Guitar Cable (output)",
                           0, 2,
                           static_cast<double>(sample_rate_),
                           true});
    }
    return devices;
}

bool AudioEngine::set_input_device(int device_index) {
    input_device_ = device_index;
    backend_->usb_input_device_id = (device_index > 0) ? device_index : -1;
    backend_->input_device_name   = (device_index > 0) ? "USB Guitar Cable" : "Android Microphone";
    if (running_) restart();
    return true;
}

bool AudioEngine::set_output_device(int device_index) {
    output_device_ = device_index;
    // Fix: coderabbit — wire output device ID into playback routing
    backend_->usb_output_device_id = (device_index > 0) ? device_index : -1;
    backend_->output_device_name   = (device_index > 0) ? "USB Guitar Cable (output)" : "Android Speaker";
    if (running_) restart();
    return true;
}

} // namespace Amplitron

// Fix: coderabbit — expose runtime sharing mode to UI
const char* Amplitron::AudioEngine::get_oboe_sharing_mode_label() const {
    if (!backend_ || !backend_->playbackCallback) return "Oboe";
    return (backend_->playbackCallback->get_sharing_mode() == oboe::SharingMode::Exclusive)
           ? "AAudio exclusive mode"
           : "OpenSL ES (shared mode)";
}
