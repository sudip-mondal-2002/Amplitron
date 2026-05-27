#pragma once

#include "common.h"
#include <fstream>
#include <chrono>
#include <array>
#include <thread>

namespace Amplitron {

class AudioEngine;

class Recorder {
public:
    static constexpr int WAVEFORM_SIZE = 512;

    Recorder();
    ~Recorder();

    // Start recording to a temporary WAV file
    bool start(const std::string& filepath, int sample_rate, int channels = 1);

    // Stop recording and finalize the WAV file
    void stop();

    // Pause / resume recording
    void pause();
    void resume();

    // Write audio samples (called from audio callback)
    void write_samples(const float* buffer, int num_samples);

    // Write metadata JSON sidecar file
    void write_metadata(const std::string& wav_path, AudioEngine& engine);

    // Move the recorded temp file to a user-chosen path
    bool save_to(const std::string& dest_path);

    // Discard the recorded temp file
    void discard();

    bool is_recording() const { return recording_.load(); }
    bool is_paused() const { return paused_.load(); }
    bool has_unsaved() const { return has_unsaved_.load(); }
    float get_duration() const;
    int64_t get_samples_written() const { return samples_written_.load(); }
    int get_channels() const { return channels_; }
    const std::string& filepath() const { return filepath_; }

    // Waveform visualization data (lock-free ring buffer of peak values)
    void get_waveform(float* out, int count) const;
    float get_current_peak() const { return current_peak_.load(); }

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
