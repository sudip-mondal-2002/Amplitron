#pragma once

#include <atomic>

#include "audio/engine/i_metronome.h"
#include "common.h"

namespace Amplitron {

class Metronome : public IMetronome {
   public:
    Metronome();
    ~Metronome() override = default;

    void set_enabled(bool enabled) override;
    bool is_enabled() const override;
    void toggle() override;

    void set_bpm(int bpm) override;
    int get_bpm() const override;

    void set_volume(float volume) override;
    float get_volume() const override;

    void set_sample_rate(int sample_rate) override;
    void reset() override;

    // Generate next click sample
    float next_sample() override;

#ifdef AMPLITRON_TESTS
    double& test_samples_per_beat() { return metronome_samples_per_beat_; }
    double& test_sample_counter() { return metronome_sample_counter_; }
    int& test_click_samples_remaining() { return metronome_click_samples_remaining_; }
#endif

   private:
    void update_timing();

    std::atomic<bool> enabled_{false};
    std::atomic<int> bpm_{120};
    std::atomic<float> volume_{0.5f};
    std::atomic<int> sample_rate_{48000};

    // Audio thread states
    bool metronome_enabled_ = false;
    int metronome_bpm_ = 120;
    float metronome_volume_ = 0.5f;

    float metronome_volume_smoothed_ = 0.0f;
    float metronome_volume_smooth_alpha_ = 0.05f;
    float metronome_bpm_smoothed_ = 120.0f;
    float metronome_bpm_smooth_alpha_ = 0.05f;

    double metronome_samples_per_beat_ = 0.0;
    double metronome_sample_counter_ = 0.0;
    int metronome_click_samples_total_ = 0;
    int metronome_click_samples_remaining_ = 0;
    float metronome_click_phase_ = 0.0f;
    float metronome_click_phase_inc_ = 0.0f;
    float metronome_click_env_ = 0.0f;
    float metronome_click_decay_ = 0.0f;
};

}  // namespace Amplitron
