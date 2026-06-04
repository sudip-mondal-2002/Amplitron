#pragma once

#include "common.h"
#include <fstream>
#include <chrono>
#include <array>
#include <thread>

#include "audio/recorder/i_recorder.h"

namespace Amplitron {

class IAudioEngine;

class Recorder : public IRecorder {
public:
    Recorder();
    ~Recorder() override;

    // Start recording to a temporary WAV file
    bool start(const std::string& filepath, int sample_rate, int channels = 1) override;

    // Stop recording and finalize the WAV file
    void stop() override;

    // Pause / resume recording
    void pause() override;
    void resume() override;

    // Write audio samples (called from audio callback)
    void write_samples(const float* buffer, int num_samples) override;

    void write_samples_stereo(const float* left, const float* right, int num_samples) override;

    // Write metadata JSON sidecar file
    void write_metadata(const std::string& wav_path, IAudioEngine& engine) override;

    // Move the recorded temp file to a user-chosen path
    bool save_to(const std::string& dest_path) override;

    // Discard the recorded temp file
    void discard() override;

    bool is_recording() const override { return recording_.load(); }
    bool is_paused() const override { return paused_.load(); }
    bool has_unsaved() const override { return has_unsaved_.load(); }
    float get_duration() const override;
    int64_t get_samples_written() const override { return samples_written_.load(); }
    int get_channels() const override { return channels_; }
    const std::string& filepath() const override { return filepath_; }

    // Waveform visualization data (lock-free ring buffer of peak values)
    void get_waveform(float* out, int count) const override;
    float get_current_peak() const override { return current_peak_.load(); }

    // Get default recordings directory
    static std::string get_recordings_dir();

    // Generate a timestamped filename
    static std::string generate_filename();

private:
    std::ofstream file_;
    std::atomic<bool> recording_{false};
    std::atomic<bool> paused_{false};
    std::atomic<bool> has_unsaved_{false};
    std::atomic<int64_t> samples_written_{0};
    int sample_rate_ = 48000;
    int channels_ = 1;
    std::string filepath_;

    std::chrono::steady_clock::time_point start_time_;
    float pause_duration_ = 0.0f;  // GUI thread only (accessed from pause/resume)
    std::chrono::steady_clock::time_point pause_start_;

    // Waveform ring buffer (written from audio thread, read from UI thread)
    std::array<std::atomic<float>, WAVEFORM_SIZE> waveform_buf_;
    std::atomic<int> waveform_write_pos_{0};
    std::atomic<float> current_peak_{0.0f};
    int samples_per_bin_ = 0;
    int bin_sample_count_ = 0;
    float bin_peak_ = 0.0f;

    // WAV header helpers
    void write_wav_header();
    void finalize_wav_header();

    // Lock-free ring buffer for real-time audio thread -> disk writer thread
    static constexpr int RING_BUFFER_SIZE = 48000 * 4;
    std::vector<float> ring_buffer_;
    std::atomic<int64_t> ring_write_pos_{0};
    std::atomic<int64_t> ring_read_pos_{0};

    // Disk writer thread (keeps file I/O off the real-time audio thread)
    std::thread writer_thread_;
    std::atomic<bool> writer_running_{false};
    std::vector<int16_t> pcm_buffer_;

    void writer_thread_func();
};

} // namespace Amplitron
