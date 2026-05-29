#include "audio/effects/looper.h"
#include "audio/effects/effect_factory.h"

#include <ostream>
#include <algorithm>
#include <cmath>

namespace Amplitron {

static EffectRegistrar<Looper> reg("Looper");

Looper::Looper() {
    params_ = {
        {"Loop Level", 0.80f, 0.0f, 1.0f, 0.80f, "", "Playback volume of the recorded loop mixed with live input."},
        {"Crossfade",  5.0f,  0.0f, 20.0f, 5.0f,  "ms", "Crossfade length at the loop boundary to reduce clicks/pops."},
    };
    ensure_capacity();
    const float sr = static_cast<float>(std::max(sample_rate_, 1));
    loop_level_alpha_ = 1.0f - std::exp(-1.0f / (sr * kLoopLevelSmoothingSeconds));
    crossfade_alpha_ = 1.0f - std::exp(-1.0f / (sr * kLoopLevelSmoothingSeconds));
    reset();
}

void Looper::set_sample_rate(int sample_rate) {
    Effect::set_sample_rate(sample_rate);
    const float sr = static_cast<float>(std::max(sample_rate_, 1));
    loop_level_alpha_ = 1.0f - std::exp(-1.0f / (sr * kLoopLevelSmoothingSeconds));
    crossfade_alpha_ = 1.0f - std::exp(-1.0f / (sr * kLoopLevelSmoothingSeconds));
    ensure_capacity();
    reset();
}

void Looper::ensure_capacity() {
    const int sr = std::max(sample_rate_, 1);
    const int cap = std::max(sr * kMaxSeconds, 1);
    if (cap == max_samples_) return;
    max_samples_ = cap;
    buffer_l_.assign(static_cast<size_t>(max_samples_), 0.0f);
    buffer_r_.assign(static_cast<size_t>(max_samples_), 0.0f);
}

void Looper::reset() {
    state_rt_ = State::Empty;
    has_loop_rt_ = false;
    record_pos_ = 0;
    playhead_ = 0;
    loop_length_ = 0;
    loop_level_smoothed_ = clamp(params_[0].value, 0.0f, 1.0f);
    crossfade_ms_smoothed_ = clamp(params_[1].value, 0.0f, 20.0f);
    pending_commands_.store(0, std::memory_order_relaxed);
    publish_ui_snapshot();
}

void Looper::request_record_toggle() {
    pending_commands_.fetch_or(CmdRecordToggle, std::memory_order_relaxed);
}

void Looper::request_play_toggle() {
    pending_commands_.fetch_or(CmdPlayToggle, std::memory_order_relaxed);
}

void Looper::request_overdub_toggle() {
    pending_commands_.fetch_or(CmdOverdubToggle, std::memory_order_relaxed);
}

void Looper::request_clear() {
    pending_commands_.fetch_or(CmdClear, std::memory_order_relaxed);
}

int Looper::crossfade_samples_rt(float ms) const {
    const int xf = static_cast<int>(std::round((ms / 1000.0f) * static_cast<float>(sample_rate_)));
    return std::clamp(xf, 0, std::max(loop_length_ / 2, 0));
}

void Looper::publish_ui_snapshot() {
    ui_state_.store(static_cast<uint32_t>(state_rt_), std::memory_order_relaxed);
    ui_has_loop_.store(has_loop_rt_ ? 1 : 0, std::memory_order_relaxed);
    ui_loop_length_samples_.store(loop_length_, std::memory_order_relaxed);
    ui_playhead_samples_.store(playhead_, std::memory_order_relaxed);
}

void Looper::clear_loop_rt() {
    has_loop_rt_ = false;
    loop_length_ = 0;
    record_pos_ = 0;
    playhead_ = 0;
    state_rt_ = State::Empty;
}

void Looper::start_recording_rt() {
    has_loop_rt_ = false;
    loop_length_ = 0;
    record_pos_ = 0;
    playhead_ = 0;
    state_rt_ = State::Recording;
}

void Looper::stop_recording_rt_and_play_rt() {
    loop_length_ = std::clamp(record_pos_, 0, max_samples_);
    const int min_len = static_cast<int>(std::round(kMinLoopSeconds * static_cast<float>(sample_rate_)));
    if (loop_length_ < min_len) {
        clear_loop_rt();
        return;
    }
    has_loop_rt_ = true;
    playhead_ = 0;
    state_rt_ = State::Playing;
}

void Looper::toggle_play_rt() {
    if (!has_loop_rt_) return;
    if (state_rt_ == State::Playing || state_rt_ == State::Overdubbing) {
        state_rt_ = State::Idle;
    } else if (state_rt_ == State::Idle || state_rt_ == State::Empty) {
        state_rt_ = State::Playing;
    }
}

void Looper::toggle_overdub_rt() {
    if (!has_loop_rt_) return;
    if (state_rt_ == State::Overdubbing) {
        state_rt_ = State::Playing;
    } else if (state_rt_ == State::Playing) {
        state_rt_ = State::Overdubbing;
    } else if (state_rt_ == State::Idle) {
        state_rt_ = State::Overdubbing;
    }
}

void Looper::apply_pending_commands() {
    const uint32_t cmds = pending_commands_.exchange(0, std::memory_order_relaxed);
    if (cmds == 0) return;

    if (cmds & CmdClear) {
        clear_loop_rt();
    }

    if (cmds & CmdRecordToggle) {
        if (state_rt_ == State::Recording) {
            stop_recording_rt_and_play_rt();
        } else {
            start_recording_rt();
        }
    }

    if (cmds & CmdPlayToggle) {
        if (state_rt_ == State::Recording) {
            stop_recording_rt_and_play_rt();
        } else if (state_rt_ == State::Empty || state_rt_ == State::Idle ||
                   state_rt_ == State::Playing || state_rt_ == State::Overdubbing) {
            toggle_play_rt();
        }
    }

    if (cmds & CmdOverdubToggle) {
        if (state_rt_ != State::Recording) {
            toggle_overdub_rt();
        }
    }
}

void Looper::process(float* buffer, int num_samples) {
    if (!enabled_) {
        apply_pending_commands();
        publish_ui_snapshot();
        return;
    }
    process_core(buffer, nullptr, num_samples, false);
}

void Looper::process_stereo(float* left, float* right, int num_samples) {
    if (!enabled_) {
        apply_pending_commands();
        publish_ui_snapshot();
        return;
    }
    process_core(left, right, num_samples, true);
}

void Looper::process_core(float* left, float* right, int num_samples, bool stereo) {
    apply_pending_commands();

    const float loop_level_target = clamp(params_[0].value, 0.0f, 1.0f);
    const float crossfade_target_ms = clamp(params_[1].value, 0.0f, 20.0f);
    crossfade_ms_smoothed_ += crossfade_alpha_ * (crossfade_target_ms - crossfade_ms_smoothed_);
    const int cap = max_samples_;
    if (cap <= 0) {
        publish_ui_snapshot();
        return;
    }

    if (state_rt_ == State::Recording) {
        for (int i = 0; i < num_samples; ++i) {
            if (record_pos_ >= cap) {
                stop_recording_rt_and_play_rt();
                break;
            }
            buffer_l_[record_pos_] = left[i];
            if (stereo && right) buffer_r_[record_pos_] = right[i];
            ++record_pos_;
        }
    }

    if (has_loop_rt_ && loop_length_ > 0 &&
        (state_rt_ == State::Playing || state_rt_ == State::Overdubbing)) {
        const int xf = crossfade_samples_rt(crossfade_ms_smoothed_);
        for (int i = 0; i < num_samples; ++i) {
            loop_level_smoothed_ += loop_level_alpha_ * (loop_level_target - loop_level_smoothed_);
            const float loop_level = loop_level_smoothed_;
            const int pos = playhead_;
            float loop_l = buffer_l_[pos];
            float loop_r = (stereo && right) ? buffer_r_[pos] : loop_l;

            if (xf > 0 && pos >= loop_length_ - xf) {
                const int t = pos - (loop_length_ - xf); // 0..xf-1
                const float w_end = static_cast<float>(xf - t) / static_cast<float>(xf);
                const float w_start = 1.0f - w_end;
                const int start_pos = t;
                loop_l = buffer_l_[pos] * w_end + buffer_l_[start_pos] * w_start;
                if (stereo && right) {
                    loop_r = buffer_r_[pos] * w_end + buffer_r_[start_pos] * w_start;
                } else {
                    loop_r = loop_l;
                }
            }

            const float in_l = left[i];
            const float in_r = (stereo && right) ? right[i] : in_l;

            float out_l = in_l + loop_l * loop_level;
            float out_r = in_r + loop_r * loop_level;

            if (state_rt_ == State::Overdubbing) {
                buffer_l_[pos] = soft_clip(buffer_l_[pos] + in_l);
                if (stereo && right) {
                    buffer_r_[pos] = soft_clip(buffer_r_[pos] + in_r);
                }
            }

            left[i] = soft_clip(out_l);
            if (stereo && right) right[i] = soft_clip(out_r);

            ++playhead_;
            if (playhead_ >= loop_length_) playhead_ = 0;
        }
    } else {
        // Keep smoothing responsive even when not actively mixing the loop.
        loop_level_smoothed_ += loop_level_alpha_ * (loop_level_target - loop_level_smoothed_);
    }

    publish_ui_snapshot();
}

std::ostream& operator<<(std::ostream& os, Looper::State s)
{
    switch (s)
    {
        case Looper::State::Empty:
            return os << "Empty";

        case Looper::State::Idle:
            return os << "Idle";

        case Looper::State::Recording:
            return os << "Recording";

        case Looper::State::Playing:
            return os << "Playing";

        case Looper::State::Overdubbing:
            return os << "Overdubbing";

        default:
            return os << "Unknown";
    }
}


} // namespace Amplitron
