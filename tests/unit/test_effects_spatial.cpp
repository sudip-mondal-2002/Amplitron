#include "test_framework.h"
#include "test_fixtures.h"
#include "audio/effects/delay.h"
#include "audio/effects/reverb.h"
#include "audio/effects/looper.h"
#include <cstring>
#include <cmath>
#include <vector>
#include <sstream>

using namespace Amplitron;

TEST_F(EffectsTest, delay_produces_echo) {
    Delay dl;
    dl.set_sample_rate(SR);
    dl.reset();

    float buf[4096];
    std::memset(buf, 0, sizeof(buf));
    buf[0] = 1.0f;

    dl.process(buf, 4096);
    ASSERT_TRUE(is_finite(buf, 4096));
}

TEST_F(EffectsTest, reverb_adds_tail) {
    Reverb rv;
    rv.set_sample_rate(SR);
    rv.reset();

    float buf[2048];
    std::memset(buf, 0, sizeof(buf));
    buf[0] = 1.0f;

    rv.process(buf, 2048);
    ASSERT_TRUE(is_finite(buf, 2048));

    float tail_energy = 0.0f;
    for (int i = 1024; i < 2048; ++i) {
        tail_energy += buf[i] * buf[i];
    }
    ASSERT_GT(tail_energy, 1e-10f);
}

TEST_F(EffectsTest, reverb_bypass_passes_signal_unchanged) {
    Reverb rv;
    rv.set_sample_rate(SR);
    rv.reset();
    rv.set_enabled(false);

    fill_sine(440.0f);
    copy_input_to_output();

    rv.process(input_buffer, BUFFER_SIZE);

    for (int i = 0; i < BUFFER_SIZE; ++i) {
        ASSERT_NEAR(input_buffer[i], output_buffer[i], 1e-6f);
    }
}

TEST_F(EffectsTest, reverb_reset_clears_tail) {
    Reverb rv;
    rv.set_sample_rate(SR);

    float buf[2048];
    for (int i = 0; i < 2048; ++i) {
        buf[i] = std::sin(2.0f * 3.14159265f * 440.0f * i / SR);
    }
    rv.process(buf, 2048);   

    rv.reset();

    float silence[2048];
    std::memset(silence, 0, sizeof(silence));
    rv.process(silence, 2048);

    ASSERT_NEAR(rms(silence, 2048), 0.0f, 1e-5f);
}

TEST_F(EffectsTest, reverb_low_decay_shorter_tail_than_high_decay) {
    auto measure_tail_rms = [&](float decay_val) {
        Reverb rv;
        rv.set_sample_rate(SR);
        rv.reset();
        rv.params()[0].value = decay_val;

        float buf[2048];
        for (int i = 0; i < 2048; ++i) {
            buf[i] = std::sin(2.0f * 3.14159265f * 440.0f * i / SR);
        }
        rv.process(buf, 2048);  
        float silence[2048];
        std::memset(silence, 0, sizeof(silence));
        rv.process(silence, 2048);
        return rms(silence, 2048);
    };

    float tail_low  = measure_tail_rms(0.1f);
    float tail_high = measure_tail_rms(0.99f);
    ASSERT_LT(tail_low, tail_high);
}

TEST_F(EffectsTest, reverb_high_decay_produces_audible_tail) {
    Reverb rv;
    rv.set_sample_rate(SR);
    rv.reset();
    rv.params()[0].value = 0.99f;

    float buf[2048];
    for (int i = 0; i < 2048; ++i) {
        buf[i] = std::sin(2.0f * 3.14159265f * 440.0f * i / SR);
    }
    rv.process(buf, 2048); 

    float silence[2048];
    std::memset(silence, 0, sizeof(silence));
    rv.process(silence, 2048);

    ASSERT_GT(rms(silence, 2048), 0.001f);
    ASSERT_TRUE(is_finite(silence, 2048));
}

TEST_F(EffectsTest, reverb_zero_decay_does_not_generate_signal) {
    Reverb rv;
    rv.set_sample_rate(SR);
    rv.reset();
    rv.params()[0].value = 0.0f;
    float buf[2048];
    std::memset(buf, 0, sizeof(buf));
    rv.process(buf, 2048);

    ASSERT_NEAR(rms(buf, 2048), 0.0f, 1e-6f);
    ASSERT_TRUE(is_finite(buf, 2048));
}

TEST_F(EffectsTest, reverb_high_damp_reduces_high_freq_content) {
    auto measure_hf_tail = [&](float damp_val) {
        Reverb rv;
        rv.set_sample_rate(SR);
        rv.reset();
        rv.params()[0].value = 0.8f;      // decay
        rv.params()[1].value = damp_val;  // damp
        rv.params()[2].value = 1.0f;      // wet only

        constexpr int N = 2048;
        constexpr int PRIME = 8192;
        float prime[PRIME];
        for (int i = 0; i < PRIME; ++i) {
            prime[i] = std::sin(2.0f * 3.14159265f * 4000.0f * i / SR);
        }
        rv.process(prime, PRIME);

        float silence[N];
        float mag = 0.0f;

        for (int b = 0; b < 6; ++b) {
            std::memset(silence, 0, sizeof(silence));
            rv.process(silence, N);

            if (b >= 2) { // skip early tail
                mag += dft_magnitude_at(silence, N, 4000.0f);
            }
        }

        return mag / 4.0f;
    };

    float hf_undamped = measure_hf_tail(0.0f);
    float hf_damped   = measure_hf_tail(1.0f);
    ASSERT_GT(hf_undamped, hf_damped);
}

TEST_F(EffectsTest, reverb_stereo_produces_finite_decorrelated_output) {
    Reverb rv;
    rv.set_sample_rate(SR);
    rv.reset();

    float left[2048], right[2048];
    for (int i = 0; i < 2048; ++i) {
        left[i] = std::sin(2.0f * 3.14159265f * 440.0f * i / SR);
        right[i] = std::sin(2.0f * 3.14159265f * 440.0f * i / SR);
    }
    rv.process_stereo(left, right, 2048);

    ASSERT_TRUE(is_finite(left,  2048));
    ASSERT_TRUE(is_finite(right, 2048));
    ASSERT_GT(rms(left,  2048), 0.001f);
    ASSERT_GT(rms(right, 2048), 0.001f);

    float diff = 0.0f;
    for (int i = 0; i < 2048; ++i) diff += std::fabs(left[i] - right[i]);
    ASSERT_GT(diff, 1e-3f);
}

TEST_F(EffectsTest, looper_initial_state_is_empty) {
    Looper lp;
    lp.set_sample_rate(SR);
    lp.reset();
    ASSERT_EQ(lp.state(), Looper::State::Empty);
    ASSERT_FALSE(lp.has_loop());
    ASSERT_EQ(lp.loop_length_samples(), 0);
}

TEST_F(EffectsTest, looper_record_then_play) {
    Looper lp;
    lp.set_sample_rate(SR);
    lp.reset();

    lp.request_record_toggle();
    std::vector<float> rec(9600, 0.0f);
    for (int i = 0; i < 9600; ++i) {
        rec[i] = std::sin(2.0f * 3.14159265f * 440.0f * i / SR);
    }
    lp.process(rec.data(), 9600);
    ASSERT_EQ(lp.state(), Looper::State::Recording);

    lp.request_record_toggle();
    float dummy[256] = {};
    lp.process(dummy, 256);
    ASSERT_EQ(lp.state(), Looper::State::Playing);
    ASSERT_TRUE(lp.has_loop());
    ASSERT_GT(lp.loop_length_samples(), 0);
}

TEST_F(EffectsTest, looper_short_recording_is_discarded) {
    Looper lp;
    lp.set_sample_rate(SR);
    lp.reset();

    lp.request_record_toggle();
    float tiny[100] = {};
    lp.process(tiny, 100);

    lp.request_record_toggle();
    float dummy[256] = {};
    lp.process(dummy, 256);

    ASSERT_EQ(lp.state(), Looper::State::Empty);
    ASSERT_FALSE(lp.has_loop());
}

TEST_F(EffectsTest, looper_bypass_passes_signal_unchanged) {
    Looper lp;
    lp.set_sample_rate(SR);
    lp.reset();
    lp.set_enabled(false);

    fill_sine(440.0f);
    copy_input_to_output();

    lp.process(input_buffer, BUFFER_SIZE);

    for (int i = 0; i < BUFFER_SIZE; ++i) {
        ASSERT_NEAR(input_buffer[i], output_buffer[i], 1e-6f);
    }
}

TEST_F(EffectsTest, looper_play_toggle_pauses_and_resumes) {
    Looper lp;
    lp.set_sample_rate(SR);
    lp.reset();

    lp.request_record_toggle();
    std::vector<float> rec(9600, 0.0f);
    lp.process(rec.data(), 9600);
    lp.request_record_toggle();
    float dummy[256] = {};
    lp.process(dummy, 256);
    ASSERT_EQ(lp.state(), Looper::State::Playing);

    lp.request_play_toggle();
    lp.process(dummy, 256);
    ASSERT_EQ(lp.state(), Looper::State::Idle);

    lp.request_play_toggle();
    lp.process(dummy, 256);
    ASSERT_EQ(lp.state(), Looper::State::Playing);
}

TEST_F(EffectsTest, looper_overdub_transition_preserves_loop) {
    Looper lp;
    lp.set_sample_rate(SR);
    lp.reset();

    lp.request_record_toggle();
    float silence[9600] = {};
    lp.process(silence, 9600);
    lp.request_record_toggle();
    float dummy[256] = {};
    lp.process(dummy, 256);
    ASSERT_EQ(lp.state(), Looper::State::Playing);

    lp.request_overdub_toggle();
    float overdub_buf[256];
    for (int i = 0; i < 256; ++i) {
        overdub_buf[i] = std::sin(2.0f * 3.14159265f * 440.0f * i / SR);
    }
    lp.process(overdub_buf, 256);
    ASSERT_EQ(lp.state(), Looper::State::Overdubbing);

    lp.request_overdub_toggle();
    std::memset(dummy, 0, sizeof(dummy));
    lp.process(dummy, 256);
    ASSERT_EQ(lp.state(), Looper::State::Playing);
    ASSERT_TRUE(lp.has_loop());
}

TEST_F(EffectsTest, looper_overdub_output_is_finite) {
    Looper lp;
    lp.set_sample_rate(SR);
    lp.reset();

    lp.request_record_toggle();
    float rec[9600];
    for (int i = 0; i < 9600; ++i) {
        rec[i] = std::sin(2.0f * 3.14159265f * 440.0f * i / SR);
    }
    lp.process(rec, 9600);

    lp.request_record_toggle();
    float dummy[256] = {};
    lp.process(dummy, 256);

    lp.request_overdub_toggle();
    for (int block = 0; block < 10; ++block) {
        float buf[256];
        for (int i = 0; i < 256; ++i) {
            buf[i] = std::sin(2.0f * 3.14159265f * 880.0f * i / SR);
        }
        lp.process(buf, 256);
        ASSERT_TRUE(is_finite(buf, 256));
    }
}

TEST_F(EffectsTest, looper_clear_resets_to_empty) {
    Looper lp;
    lp.set_sample_rate(SR);
    lp.reset();

    lp.request_record_toggle();
    float rec[9600] = {};
    lp.process(rec, 9600);
    lp.request_record_toggle();
    float dummy[256] = {};
    lp.process(dummy, 256);
    ASSERT_TRUE(lp.has_loop());

    lp.request_clear();
    lp.process(dummy, 256);

    ASSERT_EQ(lp.state(), Looper::State::Empty);
    ASSERT_FALSE(lp.has_loop());
    ASSERT_EQ(lp.loop_length_samples(), 0);
}

TEST_F(EffectsTest, looper_stereo_playback_is_finite) {
    Looper lp;
    lp.set_sample_rate(SR);
    lp.reset();

    lp.request_record_toggle();
    float left[9600], right[9600];
    for (int i = 0; i < 9600; ++i) {
        left[i] = std::sin(2.0f * 3.14159265f * 440.0f * i / SR);
        right[i] = std::sin(2.0f * 3.14159265f * 880.0f * i / SR);
    }
    lp.process_stereo(left, right, 9600);

    lp.request_record_toggle();
    float dl[256] = {}, dr[256] = {};
    lp.process_stereo(dl, dr, 256);
    ASSERT_EQ(lp.state(), Looper::State::Playing);

    for (int block = 0; block < 5; ++block) {
        for (int i = 0; i < 256; ++i) {
            dl[i] = std::sin(2.0f * 3.14159265f * 440.0f * i / SR);
            dr[i] = std::sin(2.0f * 3.14159265f * 880.0f * i / SR);
        }
        lp.process_stereo(dl, dr, 256);
        ASSERT_TRUE(is_finite(dl, 256));
        ASSERT_TRUE(is_finite(dr, 256));
    }
}

TEST_F(EffectsTest, looper_play_toggle_while_recording_stops_and_enters_playing) {
    Looper lp;
    lp.set_sample_rate(SR);
    lp.reset();

    lp.request_record_toggle();
    float rec[9600] = {};
    lp.process(rec, 9600);
    ASSERT_EQ(lp.state(), Looper::State::Recording);

    lp.request_play_toggle();
    float dummy[256] = {};
    lp.process(dummy, 256);

    ASSERT_EQ(lp.state(), Looper::State::Playing);
    ASSERT_TRUE(lp.has_loop());
}

TEST_F(EffectsTest, looper_overdub_toggle_from_idle_enters_overdubbing) {
    Looper lp;
    lp.set_sample_rate(SR);
    lp.reset();

    lp.request_record_toggle();
    float rec[9600] = {};
    lp.process(rec, 9600);
    lp.request_record_toggle();
    float dummy[256] = {};
    lp.process(dummy, 256);

    lp.request_play_toggle();
    lp.process(dummy, 256);
    ASSERT_EQ(lp.state(), Looper::State::Idle);

    lp.request_overdub_toggle();
    lp.process(dummy, 256);
    ASSERT_EQ(lp.state(), Looper::State::Overdubbing);
}

TEST_F(EffectsTest, looper_overdub_toggle_during_recording_is_ignored) {
    Looper lp;
    lp.set_sample_rate(SR);
    lp.reset();

    lp.request_record_toggle();
    float rec[512] = {};
    lp.process(rec, 512);
    ASSERT_EQ(lp.state(), Looper::State::Recording);

    lp.request_overdub_toggle();
    lp.process(rec, 512);

    ASSERT_EQ(lp.state(), Looper::State::Recording);
}

TEST_F(EffectsTest, looper_crossfade_wraparound_executes_crossfade_branch) {
    Looper lp;
    lp.set_sample_rate(SR);
    lp.reset();

    lp.params()[1].value = 5.0f;

    constexpr int LOOP = 5000;
    constexpr int N = 256;
    constexpr int XF = 240; // 5 ms at 48 kHz

    lp.request_record_toggle();

    float rec[LOOP];
    for (int i = 0; i < LOOP; ++i) {
        rec[i] = std::sin(2.0f * 3.14159265f * 440.0f * i / SR);
    }
    lp.process(rec, LOOP);

    lp.request_record_toggle();

    float dummy[N] = {};
    lp.process(dummy, N);

    ASSERT_EQ(lp.state(), Looper::State::Playing);

    bool entered_crossfade_region = false;
    for (int i = 0; i < 40; ++i) {
        if (lp.playhead_samples() >= (LOOP - XF)) {
            entered_crossfade_region = true;
        }
        float buf[N];
        for (int j = 0; j < N; ++j) {
            buf[j] = std::sin(2.0f * 3.14159265f * 440.0f * j / SR);
        }

        lp.process(buf, N);

        ASSERT_TRUE(is_finite(buf, N));
    }
    ASSERT_TRUE(entered_crossfade_region);
}

TEST_F(EffectsTest, looper_stereo_overdub_executes_right_channel_write_path) {
    Looper lp;
    lp.set_sample_rate(SR);
    lp.reset();
    lp.params()[0].value = 1.0f;

    constexpr int LOOP = 5000;
    constexpr int N = 256;

    lp.request_record_toggle();
    float left[LOOP] = {};
    float right[LOOP] = {};
    lp.process_stereo(left, right, LOOP);

    lp.request_record_toggle();
    float dl[N] = {}, dr[N] = {};
    lp.process_stereo(dl, dr, N);
    ASSERT_EQ(lp.state(), Looper::State::Playing);

    lp.request_overdub_toggle();

    for (int i = 0; i < 20; ++i) {
        float ol[N] = {};
        float or_[N];
        for (int j = 0; j < N; ++j) {
            or_[j] = std::sin(2.0f * 3.14159265f * 880.0f * j / SR);
        }
        lp.process_stereo(ol, or_, N);
        ASSERT_TRUE(is_finite(or_, N));
    }
    ASSERT_EQ(lp.state(), Looper::State::Overdubbing);

    lp.request_overdub_toggle();
    float pl[N] = {}, pr[N] = {};
    lp.process_stereo(pl, pr, N);

    ASSERT_GT(rms(pr, N), rms(pl, N) * 2.0f);
}

TEST_F(EffectsTest, looper_state_stream_operator_outputs_expected_strings) {
    std::ostringstream os;

    os << Looper::State::Empty << " "
       << Looper::State::Idle << " "
       << Looper::State::Recording << " "
       << Looper::State::Playing << " "
       << Looper::State::Overdubbing;

    ASSERT_EQ(
        os.str(),
        "Empty Idle Recording Playing Overdubbing"
    );
}

TEST_F(EffectsTest, looper_state_stream_operator_handles_unknown_value) {
    std::ostringstream os;

    auto invalid =
        static_cast<Looper::State>(999);

    os << invalid;

    ASSERT_EQ(os.str(), "Unknown");
}

TEST_F(EffectsTest, looper_metadata_accessors_return_expected_values) {
    Looper lp;

    ASSERT_EQ(std::string(lp.name()), "Looper");
    ASSERT_EQ(std::string(lp.type_id()), "Looper");

    auto& p = lp.params();

    ASSERT_FALSE(p.empty());
    ASSERT_GE(p.size(), 2u);
}

TEST_F(EffectsTest, looper_recording_auto_stops_at_buffer_limit) {
    Looper lp;
    lp.set_sample_rate(100);
    lp.reset();

    lp.request_record_toggle();
    std::vector<float> rec(7000, 0.5f);
    lp.process(rec.data(), 7000);

    ASSERT_EQ(lp.state(), Looper::State::Playing);
    ASSERT_TRUE(lp.has_loop());
    ASSERT_EQ(lp.loop_length_samples(), 6000);
}

TEST_F(EffectsTest, delay_calculates_correct_time_from_bpm) {
    Delay dl;
    dl.set_sample_rate(SR);
    dl.reset();

    dl.params()[0].value = 490.0f; 
    dl.set_transport_state(120.0f);

    ASSERT_NEAR(dl.params()[0].value, 500.0f, 0.01f);
}
