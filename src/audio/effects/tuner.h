#pragma once

// Chromatic tuner based on pitch detection from the input signal.
// Uses the YIN difference d(tau)=sum_j(x[j]-x[j+tau])^2 and cumulative mean
// normalized difference to estimate period tau; frequency is Fs/tau and cents
// error is 1200*log2(freq / nearest_note_freq).

#include "audio/effects/effect.h"
#include <atomic>

namespace Amplitron {

class TunerPedal : public Effect {
public:
    TunerPedal();
    void process(float* buffer, int num_samples) override;
    void set_sample_rate(int sample_rate) override;
    void reset() override;
    const char* name() const override { return "Tuner"; }
    const char* type_id() const override { return "Tuner"; }
    std::vector<EffectParam>& params() override { return params_; }

    // Tuner detection results (audio thread writes, UI thread reads)
    std::atomic<float> detected_freq{0.0f};
    std::atomic<float> detected_cents{0.0f};
    std::atomic<int> detected_note{-1};    // 0=C, 1=C#, 2=D, ... 11=B
    std::atomic<int> detected_octave{-1};
    std::atomic<bool> signal_detected{false};

    static const char* note_name(int note_index);

private:
    std::vector<EffectParam> params_;

    // YIN pitch detection buffer
    // 4096 samples at 48kHz gives ~85ms window, ample headroom for E2 (82.41Hz, period ~582 samples)
    static constexpr int YIN_BUFFER_SIZE = 4096;
    std::vector<float> yin_buffer_;
    int yin_write_pos_ = 0;
    bool yin_buffer_full_ = false;

    // YIN internals
    float yin_detect_pitch(float a4_ref);
    void freq_to_note(float freq, float a4_ref);

    // Preallocated work buffers for yin_detect_pitch() (avoids RT heap allocs)
    std::vector<float> yin_buf_;   // linearized circular buffer (YIN_BUFFER_SIZE)
    std::vector<float> yin_d_;     // cumulative mean normalized difference (YIN_BUFFER_SIZE/2)

    // Update rate control (~15 updates/sec)
    int samples_since_update_ = 0;
    int update_interval_ = 0;
    void recalc_update_interval();
};

} // namespace Amplitron
