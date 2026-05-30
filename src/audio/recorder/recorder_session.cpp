#include "audio/recorder/recorder.h"
#include "audio/recorder/recorder_impl.h"
#include <iostream>
#include <cmath>
#include <algorithm>

namespace Amplitron {

bool Recorder::start(const std::string& filepath, int sample_rate, int channels) {
    if (recording_) return false;

    filepath_ = filepath;
    sample_rate_ = sample_rate;
    channels_ = channels;
    samples_written_ = 0;
    paused_ = false;
    has_unsaved_ = false;
    pause_duration_ = 0.0f;

    // Reset waveform buffer
    for (auto& v : waveform_buf_) v.store(0.0f);
    waveform_write_pos_ = 0;
    current_peak_ = 0.0f;
    bin_sample_count_ = 0;
    bin_peak_ = 0.0f;
    // ~60 waveform updates per second (at 48kHz, ~800 samples per bin)
    samples_per_bin_ = std::max(1, sample_rate / (WAVEFORM_SIZE * 2));

    // Initialize ring buffer and pre-allocated PCM conversion buffer
    ring_buffer_.resize(RING_BUFFER_SIZE, 0.0f);
    ring_write_pos_ = 0;
    ring_read_pos_ = 0;
    pcm_buffer_.resize(4096);

    // Ensure the parent directory exists for the target filepath
    {
        std::string parent;
        size_t sep = filepath.find_last_of("/\\");
        if (sep != std::string::npos) {
            parent = filepath.substr(0, sep);
            mkdirs(parent);
        }
    }

    file_.open(filepath, std::ios::binary);
    if (!file_.is_open()) {
        std::cerr << "Recorder: failed to open " << filepath << std::endl;
        return false;
    }

    write_wav_header();
    recording_ = true;
    start_time_ = std::chrono::steady_clock::now();

    // Start the disk writer thread (keeps file I/O off the real-time audio thread)
    writer_running_ = true;
    writer_thread_ = std::thread(&Recorder::writer_thread_func, this);

    std::cout << "Recording started: " << filepath << std::endl;
    return true;
}

void Recorder::stop() {
    if (!recording_) return;
    recording_ = false;
    paused_ = false;

    // Stop writer thread and wait for it to drain remaining buffered data
    writer_running_ = false;
    if (writer_thread_.joinable()) {
        writer_thread_.join();
    }

    finalize_wav_header();
    file_.close();

    has_unsaved_ = true;

    float dur = get_duration();
    std::cout << "Recording stopped: " << samples_written_.load() << " samples ("
              << dur << "s) saved to " << filepath_ << std::endl;
}

void Recorder::pause() {
    if (!recording_ || paused_) return;
    paused_ = true;
    pause_start_ = std::chrono::steady_clock::now();
}

void Recorder::resume() {
    if (!recording_ || !paused_) return;
    auto now = std::chrono::steady_clock::now();
    float elapsed = std::chrono::duration<float>(now - pause_start_).count();
    pause_duration_ += elapsed;
    paused_ = false;
}

bool Recorder::save_to(const std::string& dest_path) {
    if (!has_unsaved_) return false;
    if (filepath_ == dest_path) {
        has_unsaved_ = false;
        return true;
    }
    // Copy file to destination
    std::ifstream src(filepath_, std::ios::binary);
    std::ofstream dst(dest_path, std::ios::binary);
    if (!src.is_open() || !dst.is_open()) return false;
    dst << src.rdbuf();
    // Check for errors during copy
    if (!dst.good() || !src.good()) {
        std::cerr << "Recorder: failed to copy recording to " << dest_path << std::endl;
        src.close();
        dst.close();
        return false;
    }
    src.close();
    dst.close();
    // Remove temp file
    std::remove(filepath_.c_str());
    // Remove metadata sidecar if exists
    std::string meta = filepath_;
    size_t dot = meta.rfind('.');
    if (dot != std::string::npos) meta = meta.substr(0, dot);
    meta += ".meta.json";
    std::remove(meta.c_str());
    filepath_ = dest_path;
    has_unsaved_ = false;
    return true;
}

void Recorder::discard() {
    if (!has_unsaved_ && !recording_) return;
    if (recording_) stop();
    std::remove(filepath_.c_str());
    // Remove metadata sidecar if exists
    std::string meta = filepath_;
    size_t dot = meta.rfind('.');
    if (dot != std::string::npos) meta = meta.substr(0, dot);
    meta += ".meta.json";
    std::remove(meta.c_str());
    has_unsaved_ = false;
    filepath_.clear();
}

} // namespace Amplitron
