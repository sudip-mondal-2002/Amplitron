#include "audio/engine/tempo_engine.h"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace Amplitron {

TempoEngine::TempoEngine() {
    set_sample_rate(48000);
}

void TempoEngine::set_sample_rate(int sample_rate) {
    if (sample_rate <= 0) return;
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    sample_rate_ = sample_rate;
    // 4 seconds of audio buffer
    ring_buffer_.assign(4 * sample_rate, 0.0f);
    write_pos_ = 0;
}

void TempoEngine::write_input(const float* data, int count) {
    std::unique_lock<std::mutex> lock(buffer_mutex_, std::try_to_lock);
    if (!lock.owns_lock()) return; // Non-blocking: skip if UI thread is currently reading/copying
    
    int size = static_cast<int>(ring_buffer_.size());
    if (size == 0) return;
    
    for (int i = 0; i < count; ++i) {
        ring_buffer_[write_pos_] = data[i];
        write_pos_ = (write_pos_ + 1) % size;
    }
}

float TempoEngine::detect_bpm() {
    std::vector<float> samples;
    int sr = 48000;
    
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        if (ring_buffer_.empty()) return -1.0f;
        samples.resize(ring_buffer_.size());
        // Linearize the circular buffer in chronological order
        for (size_t i = 0; i < ring_buffer_.size(); ++i) {
            samples[i] = ring_buffer_[(write_pos_ + i) % ring_buffer_.size()];
        }
        sr = sample_rate_;
    }

    // Check if there is enough energy in the signal (RMS gate)
    float energy = 0.0f;
    for (float val : samples) {
        energy += val * val;
    }
    float rms = std::sqrt(energy / samples.size());
    if (rms < 0.005f) { // Too quiet - no tempo detected
        return -1.0f;
    }

    // Compute frame energies
    // Hop size corresponds to 200Hz frame rate (5ms interval)
    int hop_size = sr / 200;
    if (hop_size < 1) hop_size = 1;
    int frame_size = hop_size * 2;
    int num_frames = (static_cast<int>(samples.size()) - frame_size) / hop_size;
    if (num_frames < 10) return -1.0f;

    std::vector<float> frame_energies(num_frames, 0.0f);
    for (int f = 0; f < num_frames; ++f) {
        float sum = 0.0f;
        int start_idx = f * hop_size;
        for (int i = 0; i < frame_size; ++i) {
            float val = samples[start_idx + i];
            sum += val * val;
        }
        frame_energies[f] = sum;
    }

    // Compute half-wave rectified first-order difference (ODF)
    std::vector<float> odf(num_frames - 1, 0.0f);
    float mean_odf = 0.0f;
    for (int f = 1; f < num_frames; ++f) {
        float diff = frame_energies[f] - frame_energies[f - 1];
        odf[f - 1] = std::max(0.0f, diff);
        mean_odf += odf[f - 1];
    }
    if (!odf.empty()) {
        mean_odf /= odf.size();
    }

    // Mean subtraction
    for (float& val : odf) {
        val -= mean_odf;
    }

    // Run autocorrelation
    // Target BPM: 50 to 240
    // Lag range (at f_odf = 200 Hz):
    // lag = (60 * f_odf) / BPM = 12000 / BPM
    // For BPM 240: lag = 50
    // For BPM 50: lag = 240
    int min_lag = 50;
    int max_lag = 240;
    min_lag = std::max(1, min_lag);
    max_lag = std::min(static_cast<int>(odf.size() - 1), max_lag);

    float best_correlation = -1.0f;
    int best_lag = -1;

    std::vector<float> ac(max_lag + 1, 0.0f);
    for (int lag = min_lag; lag <= max_lag; ++lag) {
        float sum = 0.0f;
        int count = 0;
        for (size_t i = 0; i < odf.size() - lag; ++i) {
            sum += odf[i] * odf[i + lag];
            count++;
        }
        if (count > 0) {
            ac[lag] = sum / count;
        }
    }

    for (int lag = min_lag; lag <= max_lag; ++lag) {
        if (ac[lag] > best_correlation) {
            best_correlation = ac[lag];
            best_lag = lag;
        }
    }

    if (best_lag > 0) {
        float f_odf = static_cast<float>(sr) / static_cast<float>(hop_size);
        float bpm = (60.0f * f_odf) / best_lag;
        // Clamp to valid range (40 - 240 BPM)
        if (bpm < 40.0f) bpm = 40.0f;
        if (bpm > 240.0f) bpm = 240.0f;
        return bpm;
    }

    return -1.0f;
}

} // namespace Amplitron
