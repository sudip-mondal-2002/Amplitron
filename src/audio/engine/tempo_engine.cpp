#include "audio/engine/tempo_engine.h"
#include "kiss_fft.h"
#include <cmath>
#include <algorithm>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Amplitron {

TempoEngine::TempoEngine() {
    hann_window_.resize(512);
    for (int i = 0; i < 512; ++i) {
        hann_window_[i] = static_cast<float>(0.5 * (1.0 - std::cos(2.0 * M_PI * i / 511.0)));
    }
    set_sample_rate(48000);
}

TempoEngine::~TempoEngine() {
}

void TempoEngine::set_sample_rate(int sample_rate) {
    if (sample_rate <= 0) sample_rate = 48000;
    sample_rate_ = sample_rate;
    buffer_.assign(4 * sample_rate_, 0.0f);
    write_pos_ = 0;
    total_samples_written_ = 0;
}

void TempoEngine::write_input(const float* input, int num_samples) {
    if (buffer_.empty() || num_samples <= 0) return;

    for (int i = 0; i < num_samples; ++i) {
        buffer_[write_pos_] = input[i];
        write_pos_++;
        if (write_pos_ >= buffer_.size()) {
            write_pos_ = 0;
        }
    }
    total_samples_written_.fetch_add(num_samples, std::memory_order_relaxed);
}

float TempoEngine::detect_bpm() {
    size_t written = total_samples_written_.load(std::memory_order_relaxed);
    // Need at least 2 seconds of audio to detect BPM reliably (especially down to 60 BPM)
    if (written < static_cast<size_t>(sample_rate_ * 2)) {
        return -1.0f;
    }

    size_t size_to_copy = std::min(written, buffer_.size());
    local_audio_.resize(size_to_copy);

    if (written < buffer_.size()) {
        std::copy(buffer_.begin(), buffer_.begin() + size_to_copy, local_audio_.begin());
    } else {
        size_t pos = write_pos_;
        std::copy(buffer_.begin() + pos, buffer_.end(), local_audio_.begin());
        std::copy(buffer_.begin(), buffer_.begin() + pos, local_audio_.begin() + (buffer_.size() - pos));
    }

    // Parameters for frame-by-frame analysis
    constexpr int frame_size = 512;
    constexpr int hop_size = 256;
    int num_frames = (static_cast<int>(local_audio_.size()) - frame_size) / hop_size;
    if (num_frames <= 2) {
        return -1.0f;
    }

    // Allocate kiss_fft
    kiss_fft_cfg cfg = kiss_fft_alloc(frame_size, 0, nullptr, nullptr);
    if (!cfg) return -1.0f;

    std::vector<kiss_fft_cpx> time_in(frame_size);
    std::vector<kiss_fft_cpx> freq_out(frame_size);
    std::vector<float> prev_mag(frame_size / 2 + 1, 0.0f);
    std::vector<float> mag(frame_size / 2 + 1, 0.0f);
    std::vector<float> flux_envelope;
    flux_envelope.reserve(num_frames);

    for (int f = 0; f < num_frames; ++f) {
        int start_idx = f * hop_size;
        for (int i = 0; i < frame_size; ++i) {
            time_in[i].r = local_audio_[start_idx + i] * hann_window_[i];
            time_in[i].i = 0.0f;
        }

        kiss_fft(cfg, time_in.data(), freq_out.data());

        for (int i = 0; i <= frame_size / 2; ++i) {
            mag[i] = std::sqrt(freq_out[i].r * freq_out[i].r + freq_out[i].i * freq_out[i].i);
        }

        float flux = 0.0f;
        if (f > 0) {
            for (int i = 0; i <= frame_size / 2; ++i) {
                float diff = mag[i] - prev_mag[i];
                if (diff > 0.0f) {
                    flux += diff;
                }
            }
        }
        prev_mag = mag;
        flux_envelope.push_back(flux);
    }

    kiss_fft_free(cfg);

    if (flux_envelope.size() < 4) {
        return -1.0f;
    }

    // Subtract mean from flux envelope to remove DC component
    float mean_flux = 0.0f;
    for (float val : flux_envelope) mean_flux += val;
    mean_flux /= flux_envelope.size();
    for (float& val : flux_envelope) val -= mean_flux;

    // Run autocorrelation
    float Fs = static_cast<float>(sample_rate_);
    float F_sf = Fs / static_cast<float>(hop_size); // frame rate (e.g. 187.5 Hz)

    // Range: 60 to 180 BPM
    int min_lag = static_cast<int>(std::floor((60.0f / 180.0f) * F_sf));
    int max_lag = static_cast<int>(std::ceil((60.0f / 60.0f) * F_sf));

    if (min_lag < 2) min_lag = 2;
    if (max_lag >= static_cast<int>(flux_envelope.size())) {
        max_lag = static_cast<int>(flux_envelope.size()) - 1;
    }
    if (min_lag >= max_lag) return -1.0f;

    std::vector<float> r_values(max_lag - min_lag + 1, 0.0f);
    float max_r = -9999.0f;
    int best_lag = -1;

    for (int lag = min_lag; lag <= max_lag; ++lag) {
        float sum = 0.0f;
        int count = 0;
        for (int t = lag; t < static_cast<int>(flux_envelope.size()); ++t) {
            sum += flux_envelope[t] * flux_envelope[t - lag];
            count++;
        }
        if (count > 0) {
            sum /= count;
        }
        r_values[lag - min_lag] = sum;
        if (sum > max_r) {
            max_r = sum;
            best_lag = lag;
        }
    }

    if (best_lag == -1 || max_r <= 0.0f) {
        return -1.0f; // Not enough periodicity
    }

    // Sub-harmonic / octave correction: prefer smaller lags (higher BPM) if they have a strong peak
    int check_factors[] = { 3, 2 };
    for (int factor : check_factors) {
        int candidate_lag = static_cast<int>(std::round(static_cast<float>(best_lag) / factor));
        if (candidate_lag >= min_lag) {
            float candidate_r = r_values[candidate_lag - min_lag];
            if (candidate_r >= 0.80f * max_r) {
                best_lag = candidate_lag;
                max_r = candidate_r;
            }
        }
    }

    // Parabolic interpolation for sub-lag accuracy
    float refined_lag = static_cast<float>(best_lag);
    if (best_lag > min_lag && best_lag < max_lag) {
        float s0 = r_values[best_lag - 1 - min_lag];
        float s1 = r_values[best_lag - min_lag];
        float s2 = r_values[best_lag + 1 - min_lag];
        float denom = 2.0f * (s0 - 2.0f * s1 + s2);
        if (std::fabs(denom) > 1e-9f) {
            refined_lag += (s0 - s2) / denom;
        }
    }

    float detected_bpm = (60.0f * F_sf) / refined_lag;
    if (detected_bpm < 40.0f || detected_bpm > 240.0f) {
        return -1.0f;
    }

    return detected_bpm;
}

} // namespace Amplitron
