#include "audio/recorder/recorder.h"
#include <cstring>
#include <chrono>

namespace Amplitron {

void Recorder::write_samples(const float* buffer, int num_samples) {
    if (!recording_ || paused_) return;

    // Push samples into lock-free ring buffer (real-time safe: no alloc, no I/O)
    int64_t wp = ring_write_pos_.load(std::memory_order_relaxed);
    int64_t rp = ring_read_pos_.load(std::memory_order_acquire);
    int64_t available_space = RING_BUFFER_SIZE - (wp - rp);

    int to_write = num_samples * channels_;
    if (to_write > static_cast<int>(available_space)) {
        to_write = static_cast<int>(available_space);
    }

    for (int i = 0; i < to_write; ++i) {
        ring_buffer_[static_cast<int>(wp % RING_BUFFER_SIZE)] = buffer[i];
        wp++;
    }
    ring_write_pos_.store(wp, std::memory_order_release);
    samples_written_ += num_samples;

    // Update waveform ring buffer (lock-free)
    for (int i = 0; i < num_samples; ++i) {
        float abs_val = std::fabs(buffer[i]);
        if (abs_val > bin_peak_) bin_peak_ = abs_val;
        bin_sample_count_++;
        if (bin_sample_count_ >= samples_per_bin_) {
            int pos = waveform_write_pos_.load() % WAVEFORM_SIZE;
            waveform_buf_[pos].store(bin_peak_);
            waveform_write_pos_.fetch_add(1);
            current_peak_.store(bin_peak_);
            bin_peak_ = 0.0f;
            bin_sample_count_ = 0;
        }
    }
}

void Recorder::writer_thread_func() {
    while (true) {
        int64_t rp = ring_read_pos_.load(std::memory_order_relaxed);
        int64_t wp = ring_write_pos_.load(std::memory_order_acquire);
        int64_t available = wp - rp;

        if (available > 0) {
            // Drain available samples: convert float -> int16 PCM and write to disk
            while (available > 0) {
                int chunk = static_cast<int>(std::min(available,
                            static_cast<int64_t>(pcm_buffer_.size())));
                for (int i = 0; i < chunk; ++i) {
                    float s = ring_buffer_[static_cast<int>((rp + i) % RING_BUFFER_SIZE)];
                    if (s > 1.0f) s = 1.0f;
                    if (s < -1.0f) s = -1.0f;
                    pcm_buffer_[i] = static_cast<int16_t>(s * 32767.0f);
                }
                file_.write(reinterpret_cast<const char*>(pcm_buffer_.data()),
                            chunk * sizeof(int16_t));
                rp += chunk;
                available -= chunk;
            }
            ring_read_pos_.store(rp, std::memory_order_release);
        } else {
            // No data available — exit if stopped, otherwise poll briefly
            if (!writer_running_.load(std::memory_order_acquire)) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
}

void Recorder::get_waveform(float* out, int count) const {
    int wp = waveform_write_pos_.load();
    for (int i = 0; i < count; ++i) {
        int idx = (wp - count + i + WAVEFORM_SIZE * 2) % WAVEFORM_SIZE;
        out[i] = waveform_buf_[idx].load();
    }
}

float Recorder::get_duration() const {
    int64_t total = samples_written_.load();
    if (sample_rate_ <= 0) return 0.0f;
    return static_cast<float>(total) / sample_rate_;
}

void Recorder::write_samples_stereo(const float* left, const float* right, int num_samples) {
    if (!recording_ || paused_) return;

    int64_t wp = ring_write_pos_.load(std::memory_order_relaxed);
    int64_t rp = ring_read_pos_.load(std::memory_order_acquire);
    int64_t available_space = RING_BUFFER_SIZE - (wp - rp);

    // Each frame = 2 floats (left + right interleaved)
    int frames_to_write = num_samples;
    if (frames_to_write * 2 > static_cast<int>(available_space)) {
        frames_to_write = static_cast<int>(available_space) / 2;
    }

    for (int i = 0; i < frames_to_write; ++i) {
        ring_buffer_[static_cast<int>(wp % RING_BUFFER_SIZE)] = left[i];
        wp++;
        ring_buffer_[static_cast<int>(wp % RING_BUFFER_SIZE)] = right[i];
        wp++;
    }
    ring_write_pos_.store(wp, std::memory_order_release);
    samples_written_ += frames_to_write;

    // Update waveform display using left channel
    for (int i = 0; i < frames_to_write; ++i) {
        float abs_val = std::fabs(left[i]);
        if (abs_val > bin_peak_) bin_peak_ = abs_val;
        bin_sample_count_++;
        if (bin_sample_count_ >= samples_per_bin_) {
            int pos = waveform_write_pos_.load() % WAVEFORM_SIZE;
            waveform_buf_[pos].store(bin_peak_);
            waveform_write_pos_.fetch_add(1);
            current_peak_.store(bin_peak_);
            bin_peak_ = 0.0f;
            bin_sample_count_ = 0;
        }
    }
}
} // namespace Amplitron
