#include "audio/effects/tuner.h"
#include "audio/effects/effect_factory.h"

#include <cmath>
#include <cstring>

namespace Amplitron {

static EffectRegistrar<TunerPedal> reg("Tuner");

static const char* NOTE_NAMES[] = {
    "C", "C#", "D", "D#", "E", "F",
    "F#", "G", "G#", "A", "A#", "B"
};

const char* TunerPedal::note_name(int note_index) {
    if (note_index < 0 || note_index > 11) return "?";
    return NOTE_NAMES[note_index];
}

TunerPedal::TunerPedal() {
    params_ = {
        {"Mute",    1.0f, 0.0f, 1.0f, 1.0f, "", "When fully engaged (1.0), the tuner silences the audio output while you tune."},
        {"A4 Ref", 440.0f, 430.0f, 450.0f, 440.0f, "Hz", "Calibration frequency for the note A4. Default is standard 440Hz."},
    };
    yin_buffer_.resize(YIN_BUFFER_SIZE, 0.0f);
    yin_buf_.resize(YIN_BUFFER_SIZE, 0.0f);
    yin_d_.resize(YIN_BUFFER_SIZE / 2, 0.0f);
    recalc_update_interval();
}

void TunerPedal::set_sample_rate(int sample_rate) {
    sample_rate_ = sample_rate;
    recalc_update_interval();
}

void TunerPedal::recalc_update_interval() {
    // ~15 updates per second for responsive display
    update_interval_ = sample_rate_ / 15;
    if (update_interval_ < YIN_BUFFER_SIZE)
        update_interval_ = YIN_BUFFER_SIZE;
}

void TunerPedal::reset() {
    std::fill(yin_buffer_.begin(), yin_buffer_.end(), 0.0f);
    yin_write_pos_ = 0;
    yin_buffer_full_ = false;
    samples_since_update_ = 0;
    detected_freq.store(0.0f, std::memory_order_relaxed);
    detected_cents.store(0.0f, std::memory_order_relaxed);
    detected_note.store(-1, std::memory_order_relaxed);
    detected_octave.store(-1, std::memory_order_relaxed);
    signal_detected.store(false, std::memory_order_relaxed);
}

void TunerPedal::process(float* buffer, int num_samples) {
    if (!enabled_) return;

    float a4_ref = params_[1].value;
    bool mute = params_[0].value >= 0.5f;

    // Accumulate samples into YIN buffer
    for (int i = 0; i < num_samples; ++i) {
        yin_buffer_[yin_write_pos_] = buffer[i];
        yin_write_pos_++;
        if (yin_write_pos_ >= YIN_BUFFER_SIZE) {
            yin_write_pos_ = 0;
            yin_buffer_full_ = true;
        }
    }

    samples_since_update_ += num_samples;

    // Run pitch detection at the update interval when buffer is full
    if (yin_buffer_full_ && samples_since_update_ >= update_interval_) {
        samples_since_update_ = 0;

        float freq = yin_detect_pitch(a4_ref);
        if (freq > 0.0f) {
            detected_freq.store(freq, std::memory_order_relaxed);
            signal_detected.store(true, std::memory_order_relaxed);
            freq_to_note(freq, a4_ref);
        } else {
            signal_detected.store(false, std::memory_order_relaxed);
        }
    }

    // Mute output when tuner is active (standard hardware tuner behavior)
    if (mute) {
        std::memset(buffer, 0, static_cast<size_t>(num_samples) * sizeof(float));
    }
}

// ============================================================
// YIN pitch detection algorithm
// Reference: de Cheveigné & Kawahara, 2002
// ============================================================

float TunerPedal::yin_detect_pitch(float /*a4_ref*/) {
    // W = integration window length. Use half the buffer.
    const int W = YIN_BUFFER_SIZE / 2;

    // Linearize the circular buffer into preallocated member (no heap alloc)
    for (int i = 0; i < YIN_BUFFER_SIZE; ++i) {
        yin_buf_[i] = yin_buffer_[(yin_write_pos_ + i) % YIN_BUFFER_SIZE];
    }

    // Check if there's enough signal energy (RMS gate)
    float energy = 0.0f;
    for (int i = 0; i < YIN_BUFFER_SIZE; ++i)
        energy += yin_buf_[i] * yin_buf_[i];
    float rms_val = std::sqrt(energy / YIN_BUFFER_SIZE);
    if (rms_val < 0.01f) return -1.0f; // Too quiet — no pitch

    // Step 1 & 2: Difference function d(tau) and cumulative mean normalized
    // difference function d'(tau)
    // Reuse preallocated member (no heap alloc)
    std::fill(yin_d_.begin(), yin_d_.begin() + W, 0.0f);

    // d'(0) is defined as 1
    yin_d_[0] = 1.0f;

    float running_sum = 0.0f;

    for (int tau = 1; tau < W; ++tau) {
        float diff = 0.0f;
        for (int j = 0; j < W; ++j) {
            float delta = yin_buf_[j] - yin_buf_[j + tau];
            diff += delta * delta;
        }

        // Cumulative mean normalized difference
        running_sum += diff;
        yin_d_[tau] = (running_sum > 0.0f) ? (diff * tau / running_sum) : 1.0f;
    }

    // Step 3: Absolute threshold — find the first dip below threshold
    // then pick the deepest local minimum within that dip.
    constexpr float YIN_THRESHOLD = 0.20f;

    // Minimum lag: highest detectable frequency ~2000Hz
    int min_tau = sample_rate_ / 2000;
    if (min_tau < 2) min_tau = 2;

    // Maximum lag: lowest detectable frequency ~60Hz (below low E)
    int max_tau = sample_rate_ / 60;
    if (max_tau >= W) max_tau = W - 1;

    // Search strategy: find the first tau below threshold, then walk to
    // the local minimum. This avoids sub-harmonic false positives by
    // preferring the earliest qualifying dip (highest frequency candidate).
    int best_tau = -1;
    for (int tau = min_tau; tau < max_tau; ++tau) {
        if (yin_d_[tau] < YIN_THRESHOLD) {
            // Walk to the local minimum of this valley
            int valley_min = tau;
            while (tau + 1 < max_tau && yin_d_[tau + 1] < yin_d_[tau]) {
                ++tau;
            }
            // tau now points at the local minimum (or end of descent)
            valley_min = tau;
            best_tau = valley_min;
            break;
        }
    }

    // Fallback: if no dip was below threshold, take the global minimum
    if (best_tau < 1) {
        float global_min = 2.0f;
        for (int tau = min_tau; tau < max_tau; ++tau) {
            if (yin_d_[tau] < global_min) {
                global_min = yin_d_[tau];
                best_tau = tau;
            }
        }
        // Only accept if reasonably periodic
        if (global_min > 0.5f) return -1.0f;
    }

    if (best_tau < 1) return -1.0f;

    // Step 4: Parabolic interpolation for sub-sample accuracy
    float refined_tau = static_cast<float>(best_tau);
    if (best_tau > min_tau && best_tau < W - 1) {
        float s0 = yin_d_[best_tau - 1];
        float s1 = yin_d_[best_tau];
        float s2 = yin_d_[best_tau + 1];
        // Only interpolate if it's a true local minimum
        if (s0 > s1 && s2 > s1) {
            float denom = 2.0f * (s0 - 2.0f * s1 + s2);
            if (std::fabs(denom) > 1e-12f) {
                refined_tau += (s0 - s2) / denom;
            }
        }
    }

    if (refined_tau <= 0.0f) return -1.0f;

    float freq = static_cast<float>(sample_rate_) / refined_tau;

    // Sanity check: guitar range ~60Hz to ~1400Hz (high frets on high E)
    if (freq < 60.0f || freq > 1400.0f) return -1.0f;

    return freq;
}

void TunerPedal::freq_to_note(float freq, float a4_ref) {
    // Semitones from A4
    float semitones_from_a4 = 12.0f * std::log2(freq / a4_ref);

    // Nearest semitone
    int nearest = static_cast<int>(std::round(semitones_from_a4));

    // Cents deviation from nearest note
    float cents = (semitones_from_a4 - static_cast<float>(nearest)) * 100.0f;

    // A4 = MIDI note 69 (note index 9 = A, octave 4)
    int midi_note = 69 + nearest;
    int note_index = ((midi_note % 12) + 12) % 12; // 0=C .. 11=B
    int octave = (midi_note / 12) - 1;

    detected_note.store(note_index, std::memory_order_relaxed);
    detected_octave.store(octave, std::memory_order_relaxed);
    detected_cents.store(cents, std::memory_order_relaxed);
}

} // namespace Amplitron
