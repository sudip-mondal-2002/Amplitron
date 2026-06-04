#pragma once

#include <string>

namespace Amplitron {

class IAudioEngine;

/**
 * @brief Interface for audio recorder subsystem.
 * Satisfies the Dependency Inversion Principle (DIP).
 */
class IRecorder {
public:
    static constexpr int WAVEFORM_SIZE = 512;

    virtual ~IRecorder() = default;

    // Start recording to a temporary WAV file
    virtual bool start(const std::string& filepath, int sample_rate, int channels = 1) = 0;

    // Stop recording and finalize the WAV file
    virtual void stop() = 0;

    // Pause / resume recording
    virtual void pause() = 0;
    virtual void resume() = 0;

    // Write audio samples (called from audio callback)
    virtual void write_samples(const float* buffer, int num_samples) = 0;
    virtual void write_samples_stereo(const float* left, const float* right, int num_samples) = 0;

    // Write metadata JSON sidecar file
    virtual void write_metadata(const std::string& wav_path, IAudioEngine& engine) = 0;

    // Move the recorded temp file to a user-chosen path
    virtual bool save_to(const std::string& dest_path) = 0;

    // Discard the recorded temp file
    virtual void discard() = 0;

    virtual bool is_recording() const = 0;
    virtual bool is_paused() const = 0;
    virtual bool has_unsaved() const = 0;
    virtual float get_duration() const = 0;
    virtual int64_t get_samples_written() const = 0;
    virtual int get_channels() const = 0;
    virtual const std::string& filepath() const = 0;

    // Waveform visualization data (lock-free ring buffer of peak values)
    virtual void get_waveform(float* out, int count) const = 0;
    virtual float get_current_peak() const = 0;
};

} // namespace Amplitron
