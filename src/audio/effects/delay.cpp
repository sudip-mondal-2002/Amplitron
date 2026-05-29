#include "audio/effects/delay.h"
#include "audio/effects/effect_factory.h"

namespace Amplitron {

static EffectRegistrar<Delay> reg("Delay");

Delay::Delay() {
    params_ = {
        {"Time",     350.0f, 10.0f, 2000.0f, 350.0f, "ms", "Time interval between each echo. Sets the tempo of the delay repeats."},
        {"Feedback",   0.4f,  0.0f,    0.95f,  0.4f, "", "Amount of the delayed signal fed back into the input. Higher values create more repeats."},
        {"Tone",       0.7f,  0.0f,    1.0f,   0.7f, "", "High-frequency damping on the repeats. Lower values create darker, tape-like echoes."},
        {"Level",      0.5f,  0.0f,    1.0f,   0.5f, "", "Mix volume of the delay repeats added to your dry signal."},
    };
    set_sample_rate(DEFAULT_SAMPLE_RATE);
}

void Delay::set_sample_rate(int sample_rate) {
    Effect::set_sample_rate(sample_rate);
    max_delay_samples_ = static_cast<int>(sample_rate * 2.5f); // max 2.5s
    delay_buffer_.resize(max_delay_samples_, 0.0f);
    write_pos_ = 0;
}

void Delay::process(float* buffer, int num_samples) {
    if (!enabled_) return;

    const float alpha = 1.0f - std::exp(-1.0f / (sample_rate_ * 0.020f)); // 20 ms
    smoothed_time_ms_ += alpha * (params_[0].value - smoothed_time_ms_);
    smoothed_feedback_ += alpha * (params_[1].value - smoothed_feedback_);
    smoothed_tone_     += alpha * (params_[2].value - smoothed_tone_);
    smoothed_level_    += alpha * (params_[3].value - smoothed_level_);

    float time_ms = smoothed_time_ms_;
    float feedback = smoothed_feedback_;
    float tone = smoothed_tone_;
    float level = smoothed_level_;

    int delay_samples = static_cast<int>(time_ms * 0.001f * sample_rate_);
    delay_samples = std::min(delay_samples, max_delay_samples_ - 1);
    float lp_coeff = 0.1f + tone * 0.85f;

    for (int i = 0; i < num_samples; ++i) {
        float dry = buffer[i];

        int read_pos = write_pos_ - delay_samples;
        if (read_pos < 0) read_pos += max_delay_samples_;

        float delayed = delay_buffer_[read_pos];

        // Tone filter on feedback path
        float filtered = tone_lp_.lp(delayed, lp_coeff);

        // Write to delay buffer: input + filtered feedback
        delay_buffer_[write_pos_] = buffer[i] + filtered * feedback;

        write_pos_++;
        if (write_pos_ >= max_delay_samples_) write_pos_ = 0;

        buffer[i] = dry + delayed * level;
    }
}

void Delay::reset() {
    std::fill(delay_buffer_.begin(), delay_buffer_.end(), 0.0f);
    write_pos_ = 0;
    tone_lp_.reset();
}

void Delay::set_transport_state(float bpm){
    if(bpm <= 0.0f || !std::isfinite(bpm)) return;
    if(bpm == last_bpm_) return;
    last_bpm_=bpm;

    //Quarter-note duration
    float quarter_note_ms = 60000.0f / bpm;
    //Check current knob
    float current_knob_time = params_[0].value;
    //Distance to nearest subdivisions
    float diff_quarter = std::fabs(current_knob_time - quarter_note_ms);
    float diff_eighth = std::fabs(current_knob_time - (quarter_note_ms * 0.5f));
    float diff_sixteenth = std::fabs(current_knob_time - (quarter_note_ms * 0.25f));
    //Check closest
    float target_time = quarter_note_ms; //default to 1/4 note
    if(diff_eighth < diff_quarter && diff_eighth < diff_sixteenth){
        target_time = quarter_note_ms * 0.5f; // 1/8 note
    }
    else if(diff_sixteenth < diff_quarter && diff_sixteenth < diff_eighth){
        target_time = quarter_note_ms * 0.25f; // 1/16 note
    }
    
    //Set knob
    params_[0].value = clamp(target_time, params_[0].min_val, params_[0].max_val);
}

} // namespace Amplitron
