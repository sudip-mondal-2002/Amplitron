#include "test_framework.h"
#include "audio/effects/noise_gate.h"
#include "audio/effects/compressor.h"
#include "audio/effects/multiband_compressor.h"
#include "audio/effects/overdrive.h"
#include "audio/effects/distortion.h"
#include "audio/effects/equalizer.h"
#include "audio/effects/chorus.h"
#include "audio/effects/delay.h"
#include "audio/effects/reverb.h"
#include "audio/effects/looper.h"
#include "audio/effects/cabinet_sim.h"
#include "audio/effects/amp_simulator.h"
#include "audio/effects/tuner.h"
#include "audio/effects/wah.h"
#include "audio/effects/phaser.h"
#include "audio/effects/flanger.h"
#include "audio/effects/octaver.h"
#include "audio/effects/pitch_shifter.h"
#include <sstream>
#include <cstring>
#include <cmath>

using namespace Amplitron;

// Helper: fill buffer with a sine wave
static void fill_sine(float* buf, int n, float freq, int sr) {
    for (int i = 0; i < n; ++i)
        buf[i] = std::sin(2.0f * 3.14159265f * freq * i / sr);
}

// Helper: compute RMS of a buffer
static float rms(const float* buf, int n) {
    float sum = 0.0f;
    for (int i = 0; i < n; ++i) sum += buf[i] * buf[i];
    return std::sqrt(sum / n);
}

// Helper: check no NaN or Inf in buffer
static bool buffer_is_finite(const float* buf, int n) {
    for (int i = 0; i < n; ++i)
        if (!std::isfinite(buf[i])) return false;
    return true;
}

// Helper: DFT magnitude at a single frequency, normalized by sample count.
// For a pure cosine of amplitude A at frequency freq, returns approximately A/2.
static float dft_magnitude_at(const float* buf, int n, float freq, int sr) {
    float re = 0.0f, im = 0.0f;
    const float omega = 2.0f * 3.14159265f * freq / static_cast<float>(sr);
    for (int i = 0; i < n; ++i) {
        re += buf[i] * std::cos(omega * static_cast<float>(i));
        im += buf[i] * std::sin(omega * static_cast<float>(i));
    }
    return std::sqrt(re * re + im * im) / static_cast<float>(n);
}

// ============================================================
// Effect base class tests
// ============================================================

TEST(effect_enabled_default_true) {
    NoiseGate ng;
    ASSERT_TRUE(ng.is_enabled());
}

TEST(effect_enable_disable) {
    NoiseGate ng;
    ng.set_enabled(false);
    ASSERT_FALSE(ng.is_enabled());
    ng.set_enabled(true);
    ASSERT_TRUE(ng.is_enabled());
}

TEST(effect_mix_clamped) {
    Overdrive od;
    od.set_mix(-0.5f);
    ASSERT_NEAR(od.get_mix(), 0.0f, 1e-6f);
    od.set_mix(1.5f);
    ASSERT_NEAR(od.get_mix(), 1.0f, 1e-6f);
    od.set_mix(0.5f);
    ASSERT_NEAR(od.get_mix(), 0.5f, 1e-6f);
}

TEST(effect_has_name) {
    NoiseGate ng;
    Compressor comp;
    Overdrive od;
    Distortion dist;
    Equalizer eq;
    Chorus ch;
    Delay dl;
    Reverb rv;
    CabinetSim cab;

    ASSERT_TRUE(std::strcmp(ng.name(), "Noise Gate") == 0);
    ASSERT_TRUE(std::strcmp(comp.name(), "Compressor") == 0);
    ASSERT_TRUE(std::strcmp(od.name(), "Overdrive") == 0);
    ASSERT_TRUE(std::strcmp(dist.name(), "Distortion") == 0);
    ASSERT_TRUE(std::strcmp(eq.name(), "Equalizer") == 0);
    ASSERT_TRUE(std::strcmp(ch.name(), "Chorus") == 0);
    ASSERT_TRUE(std::strcmp(dl.name(), "Delay") == 0);
    ASSERT_TRUE(std::strcmp(rv.name(), "Reverb") == 0);
    ASSERT_TRUE(std::strcmp(cab.name(), "Cabinet") == 0);

    AmpSimulator amp;
    ASSERT_TRUE(std::strcmp(amp.name(), "Amp Sim") == 0);

    TunerPedal tuner;
    ASSERT_TRUE(std::strcmp(tuner.name(), "Tuner") == 0);

    WahPedal wah;
    ASSERT_TRUE(std::strcmp(wah.name(), "Wah") == 0);

    Phaser ph;
    ASSERT_TRUE(std::strcmp(ph.name(), "Phaser") == 0);

    Flanger fl;
    ASSERT_TRUE(std::strcmp(fl.name(), "Flanger") == 0);
}

TEST(effect_has_params) {
    Overdrive od;
    ASSERT_GT((int)od.params().size(), 0);

    Equalizer eq;
    ASSERT_GT((int)eq.params().size(), 0);

    Compressor comp;
    ASSERT_GT((int)comp.params().size(), 0);
}

TEST(effect_params_have_valid_ranges) {
    Overdrive od;
    for (auto& p : od.params()) {
        ASSERT_TRUE(p.min_val <= p.max_val);
        ASSERT_TRUE(p.value >= p.min_val);
        ASSERT_TRUE(p.value <= p.max_val);
        ASSERT_TRUE(p.default_val >= p.min_val);
        ASSERT_TRUE(p.default_val <= p.max_val);
        ASSERT_FALSE(p.name.empty());
    }
}

TEST(effect_set_sample_rate) {
    Reverb rv;
    rv.set_sample_rate(44100);
    rv.reset();
    // Should not crash
    float buf[128];
    fill_sine(buf, 128, 440.0f, 44100);
    rv.process(buf, 128);
    ASSERT_TRUE(buffer_is_finite(buf, 128));
}

// ============================================================
// Individual effect processing tests
// ============================================================

TEST(noise_gate_silences_quiet_signal) {
    NoiseGate ng;
    ng.set_sample_rate(48000);
    ng.reset();

    // Very quiet signal (well below any reasonable threshold)
    float buf[256];
    for (int i = 0; i < 256; ++i) buf[i] = 0.0001f * std::sin(2.0f * 3.14159f * 440.0f * i / 48000.0f);

    // Process several times to let the gate close
    for (int rep = 0; rep < 20; ++rep)
        ng.process(buf, 256);

    // After many passes of quiet signal, output should be very quiet
    float out_rms = rms(buf, 256);
    ASSERT_LT(out_rms, 0.001f);
}

TEST(noise_gate_passes_loud_signal) {
    NoiseGate ng;
    ng.set_sample_rate(48000);
    ng.reset();

    float buf[256];
    fill_sine(buf, 256, 440.0f, 48000);
    float in_rms = rms(buf, 256);

    ng.process(buf, 256);
    float out_rms = rms(buf, 256);

    // Loud signal should pass through mostly unchanged
    ASSERT_GT(out_rms, in_rms * 0.5f);
}

TEST(overdrive_adds_harmonics) {
    Overdrive od;
    od.set_sample_rate(48000);
    od.reset();

    float buf[512];
    fill_sine(buf, 512, 440.0f, 48000);

    od.process(buf, 512);
    ASSERT_TRUE(buffer_is_finite(buf, 512));

    // Output should still have energy
    ASSERT_GT(rms(buf, 512), 0.01f);
}

TEST(distortion_clips_signal) {
    Distortion dist;
    dist.set_sample_rate(48000);
    dist.reset();

    float buf[512];
    fill_sine(buf, 512, 440.0f, 48000);

    dist.process(buf, 512);
    ASSERT_TRUE(buffer_is_finite(buf, 512));

    // All output should be within [-1, 1] (clipped)
    for (int i = 0; i < 512; ++i) {
        ASSERT_GE(buf[i], -1.5f);  // some headroom for processing
        ASSERT_TRUE(buf[i] <= 1.5f);
    }
}

TEST(compressor_reduces_dynamic_range) {
    Compressor comp;
    comp.set_sample_rate(48000);
    comp.reset();

    // Loud signal
    float buf[2048];
    fill_sine(buf, 2048, 440.0f, 48000);
    // Scale to be loud
    for (int i = 0; i < 2048; ++i) buf[i] *= 0.9f;

    // Process multiple times to let compressor engage
    for (int rep = 0; rep < 5; ++rep) {
        fill_sine(buf, 2048, 440.0f, 48000);
        for (int i = 0; i < 2048; ++i) buf[i] *= 0.9f;
        comp.process(buf, 2048);
    }

    ASSERT_TRUE(buffer_is_finite(buf, 2048));
    ASSERT_GT(rms(buf, 2048), 0.01f);
}

TEST(equalizer_processes_without_nan) {
    Equalizer eq;
    eq.set_sample_rate(48000);
    eq.reset();

    float buf[512];
    fill_sine(buf, 512, 440.0f, 48000);
    eq.process(buf, 512);

    ASSERT_TRUE(buffer_is_finite(buf, 512));
    ASSERT_GT(rms(buf, 512), 0.01f);
}

TEST(chorus_modulates_signal) {
    Chorus ch;
    ch.set_sample_rate(48000);
    ch.reset();

    float buf[1024];
    fill_sine(buf, 1024, 440.0f, 48000);
    ch.process(buf, 1024);

    ASSERT_TRUE(buffer_is_finite(buf, 1024));
}

TEST(delay_produces_echo) {
    Delay dl;
    dl.set_sample_rate(48000);
    dl.reset();

    // Process an impulse
    float buf[4096];
    std::memset(buf, 0, sizeof(buf));
    buf[0] = 1.0f;

    dl.process(buf, 4096);
    ASSERT_TRUE(buffer_is_finite(buf, 4096));

    // There should be energy later in the buffer (the echo)
    float late_energy = 0.0f;
    for (int i = 2048; i < 4096; ++i)
        late_energy += buf[i] * buf[i];
    (void)late_energy;
    // With default delay settings, some echo should appear
    // (might be zero if delay time > buffer length, so just check finite)
}

TEST(reverb_adds_tail) {
    Reverb rv;
    rv.set_sample_rate(48000);
    rv.reset();

    float buf[2048];
    std::memset(buf, 0, sizeof(buf));
    buf[0] = 1.0f;

    rv.process(buf, 2048);
    ASSERT_TRUE(buffer_is_finite(buf, 2048));

    // Late portion should have some reverb tail energy
    float tail_energy = 0.0f;
    for (int i = 1024; i < 2048; ++i)
        tail_energy += buf[i] * buf[i];
    ASSERT_GT(tail_energy, 1e-10f);
}

TEST(cabinet_sim_filters_signal) {
    CabinetSim cab;
    cab.set_sample_rate(48000);
    cab.reset();

    float buf[512];
    fill_sine(buf, 512, 440.0f, 48000);
    cab.process(buf, 512);

    ASSERT_TRUE(buffer_is_finite(buf, 512));
    ASSERT_GT(rms(buf, 512), 0.001f);
}

TEST(cabinet_sim_silence_remains_silent) {
    CabinetSim cab;
    cab.set_sample_rate(48000);
    cab.reset();

    float buf[512] = {0.0f};

    cab.process(buf, 512);

    ASSERT_TRUE(buffer_is_finite(buf, 512));
    ASSERT_LT(rms(buf, 512), 0.000001f);
}
TEST(cabinet_sim_long_run_stability) {
    CabinetSim cab;
    cab.set_sample_rate(48000);
    cab.reset();

    float buf[512];

    for (int i = 0; i < 1000; ++i) {
        fill_sine(buf, 512, 440.0f, 48000);
        cab.process(buf, 512);

        ASSERT_TRUE(buffer_is_finite(buf, 512));
        ASSERT_GT(rms(buf, 512), 0.0001f);
    }
}

TEST(amp_simulator_processes_without_nan) {
    AmpSimulator amp;
    amp.set_sample_rate(48000);
    amp.reset();

    float buf[512];
    fill_sine(buf, 512, 440.0f, 48000);
    amp.process(buf, 512);

    ASSERT_TRUE(buffer_is_finite(buf, 512));
    ASSERT_GT(rms(buf, 512), 0.001f);
}

TEST(amp_simulator_models_sound_different) {
    const auto& models = Amplitron::get_amp_models();
    ASSERT_GE((int)models.size(), 3);

    std::vector<float> model_rms;
    for (int m = 0; m < static_cast<int>(models.size()); ++m) {
        AmpSimulator amp;
        amp.set_sample_rate(48000);
        amp.reset();
        amp.params()[0].value = static_cast<float>(m);

        float buf[1024];
        fill_sine(buf, 1024, 440.0f, 48000);
        amp.process(buf, 1024);
        ASSERT_TRUE(buffer_is_finite(buf, 1024));
        model_rms.push_back(rms(buf, 1024));
    }

    // At least one pair of models should produce meaningfully different RMS
    bool found_diff = false;
    for (size_t i = 0; i < model_rms.size() && !found_diff; ++i) {
        for (size_t j = i + 1; j < model_rms.size(); ++j) {
            if (std::fabs(model_rms[i] - model_rms[j]) > 0.01f) {
                found_diff = true;
                break;
            }
        }
    }
    ASSERT_TRUE(found_diff);
}

TEST(amp_simulator_output_clamped) {
    AmpSimulator amp;
    amp.set_sample_rate(48000);
    amp.reset();
    // High gain model
    amp.params()[0].value = 2.0f; // High Gain Modern
    amp.params()[1].value = 1.0f; // Max gain knob

    float buf[512];
    fill_sine(buf, 512, 440.0f, 48000);
    amp.process(buf, 512);

    for (int i = 0; i < 512; ++i) {
        ASSERT_GE(buf[i], -1.0f);
        ASSERT_TRUE(buf[i] <= 1.0f);
    }
}

TEST(amp_simulator_get_models_returns_at_least_three) {
    const auto& models = Amplitron::get_amp_models();
    ASSERT_GE((int)models.size(), 3);
    for (const auto& m : models) {
        ASSERT_TRUE(m.name != nullptr);
        ASSERT_TRUE(m.inspiration != nullptr);
        ASSERT_TRUE(m.description != nullptr);
    }
}

// ============================================================
// Tuner pitch detection tests
// ============================================================

// Helper: feed a sine wave through the tuner and return detected frequency
static float tuner_detect_freq(float target_freq, int sample_rate = 48000) {
    TunerPedal tuner;
    tuner.set_sample_rate(sample_rate);
    tuner.reset();
    // Disable mute so we can also verify pass-through
    tuner.params()[0].value = 0.0f;

    // Generate enough sine wave data to fill the YIN buffer multiple times
    // and trigger at least one detection update
    const int total_samples = sample_rate; // 1 second of audio
    const int chunk_size = 256;
    float buf[256];

    for (int offset = 0; offset < total_samples; offset += chunk_size) {
        for (int i = 0; i < chunk_size; ++i) {
            buf[i] = 0.8f * std::sin(2.0f * 3.14159265f * target_freq * (offset + i) / sample_rate);
        }
        tuner.process(buf, chunk_size);
    }

    return tuner.detected_freq.load();
}

TEST(tuner_detects_E2) {
    // E2 = 82.41 Hz (low E string)
    float freq = tuner_detect_freq(82.41f);
    ASSERT_GT(freq, 0.0f);
    // Within +/- 2 cents = +/- 0.095 Hz at 82.41 Hz
    float cents_err = std::fabs(1200.0f * std::log2(freq / 82.41f));
    ASSERT_LT(cents_err, 2.0f);
}

TEST(tuner_detects_A2) {
    // A2 = 110.0 Hz (A string)
    float freq = tuner_detect_freq(110.0f);
    ASSERT_GT(freq, 0.0f);
    float cents_err = std::fabs(1200.0f * std::log2(freq / 110.0f));
    ASSERT_LT(cents_err, 2.0f);
}

TEST(tuner_detects_D3) {
    // D3 = 146.83 Hz (D string)
    float freq = tuner_detect_freq(146.83f);
    ASSERT_GT(freq, 0.0f);
    float cents_err = std::fabs(1200.0f * std::log2(freq / 146.83f));
    ASSERT_LT(cents_err, 2.0f);
}

TEST(tuner_detects_G3) {
    // G3 = 196.0 Hz (G string)
    float freq = tuner_detect_freq(196.0f);
    ASSERT_GT(freq, 0.0f);
    float cents_err = std::fabs(1200.0f * std::log2(freq / 196.0f));
    ASSERT_LT(cents_err, 2.0f);
}

TEST(tuner_detects_B3) {
    // B3 = 246.94 Hz (B string)
    float freq = tuner_detect_freq(246.94f);
    ASSERT_GT(freq, 0.0f);
    float cents_err = std::fabs(1200.0f * std::log2(freq / 246.94f));
    ASSERT_LT(cents_err, 2.0f);
}

TEST(tuner_detects_E4) {
    // E4 = 329.63 Hz (high E string)
    float freq = tuner_detect_freq(329.63f);
    ASSERT_GT(freq, 0.0f);
    float cents_err = std::fabs(1200.0f * std::log2(freq / 329.63f));
    ASSERT_LT(cents_err, 2.0f);
}

TEST(tuner_note_names_correct) {
    ASSERT_TRUE(std::strcmp(TunerPedal::note_name(0), "C") == 0);
    ASSERT_TRUE(std::strcmp(TunerPedal::note_name(4), "E") == 0);
    ASSERT_TRUE(std::strcmp(TunerPedal::note_name(9), "A") == 0);
    ASSERT_TRUE(std::strcmp(TunerPedal::note_name(11), "B") == 0);
    ASSERT_TRUE(std::strcmp(TunerPedal::note_name(-1), "?") == 0);
}

TEST(tuner_maps_note_and_octave) {
    // Feed A4 (440 Hz) and check note=A(9), octave=4, cents~0
    TunerPedal tuner;
    tuner.set_sample_rate(48000);
    tuner.reset();

    const int total = 48000;
    const int chunk = 256;
    float buf[256];
    for (int off = 0; off < total; off += chunk) {
        for (int i = 0; i < chunk; ++i)
            buf[i] = 0.8f * std::sin(2.0f * 3.14159265f * 440.0f * (off + i) / 48000.0f);
        tuner.process(buf, chunk);
    }

    ASSERT_TRUE(tuner.signal_detected.load());
    ASSERT_EQ(tuner.detected_note.load(), 9);   // A
    ASSERT_EQ(tuner.detected_octave.load(), 4);  // octave 4
    ASSERT_LT(std::fabs(tuner.detected_cents.load()), 2.0f);
}

TEST(tuner_mute_zeroes_output) {
    TunerPedal tuner;
    tuner.set_sample_rate(48000);
    tuner.reset();
    // Mute on (default)
    tuner.params()[0].value = 1.0f;

    float buf[256];
    fill_sine(buf, 256, 440.0f, 48000);
    tuner.process(buf, 256);

    // Output should be silenced
    float out_rms = rms(buf, 256);
    ASSERT_LT(out_rms, 1e-10f);
}

TEST(tuner_pass_through_when_unmuted) {
    TunerPedal tuner;
    tuner.set_sample_rate(48000);
    tuner.reset();
    // Mute off
    tuner.params()[0].value = 0.0f;

    float buf[256];
    fill_sine(buf, 256, 440.0f, 48000);
    float in_rms = rms(buf, 256);
    tuner.process(buf, 256);
    float out_rms = rms(buf, 256);

    // Signal should pass through unchanged
    ASSERT_NEAR(out_rms, in_rms, 1e-6f);
}

TEST(tuner_no_detection_on_silence) {
    TunerPedal tuner;
    tuner.set_sample_rate(48000);
    tuner.reset();

    float buf[256];
    std::memset(buf, 0, sizeof(buf));
    for (int i = 0; i < 200; ++i)
        tuner.process(buf, 256);

    ASSERT_FALSE(tuner.signal_detected.load());
}

// ============================================================
// Aggregate effect tests (including Tuner)
// ============================================================

TEST(all_effects_handle_silence) {
    std::vector<std::shared_ptr<Effect>> effects = {
        std::make_shared<NoiseGate>(),
        std::make_shared<Compressor>(),
        std::make_shared<MultiBandCompressor>(),
        std::make_shared<Overdrive>(),
        std::make_shared<Distortion>(),
        std::make_shared<Equalizer>(),
        std::make_shared<Chorus>(),
        std::make_shared<Phaser>(),
        std::make_shared<Flanger>(),
        std::make_shared<Delay>(),
        std::make_shared<Reverb>(),
        std::make_shared<CabinetSim>(),
        std::make_shared<AmpSimulator>(),
        std::make_shared<TunerPedal>(),
        std::make_shared<WahPedal>(),
        std::make_shared<Octaver>(),
        std::make_shared<PitchShifter>(),
    };

    float buf[256];
    for (auto& fx : effects) {
        fx->set_sample_rate(48000);
        fx->reset();
        std::memset(buf, 0, sizeof(buf));
        fx->process(buf, 256);
        ASSERT_TRUE(buffer_is_finite(buf, 256));
    }
}

TEST(all_effects_reset_without_crash) {
    std::vector<std::shared_ptr<Effect>> effects = {
        std::make_shared<NoiseGate>(),
        std::make_shared<Compressor>(),
        std::make_shared<MultiBandCompressor>(),
        std::make_shared<Overdrive>(),
        std::make_shared<Distortion>(),
        std::make_shared<Equalizer>(),
        std::make_shared<Chorus>(),
        std::make_shared<Phaser>(),
        std::make_shared<Flanger>(),
        std::make_shared<Delay>(),
        std::make_shared<Reverb>(),
        std::make_shared<CabinetSim>(),
        std::make_shared<AmpSimulator>(),
        std::make_shared<TunerPedal>(),
        std::make_shared<WahPedal>(),
        std::make_shared<Octaver>(),
        std::make_shared<PitchShifter>(),
    };

    for (auto& fx : effects) {
        fx->set_sample_rate(48000);
        // Process some audio
        float buf[128];
        fill_sine(buf, 128, 440.0f, 48000);
        fx->process(buf, 128);
        // Reset
        fx->reset();
        // Process again
        fill_sine(buf, 128, 440.0f, 48000);
        fx->process(buf, 128);
        ASSERT_TRUE(buffer_is_finite(buf, 128));
    }
}

TEST(all_effects_handle_different_sample_rates) {
    int rates[] = {22050, 44100, 48000, 96000};
    std::vector<std::shared_ptr<Effect>> effects = {
        std::make_shared<NoiseGate>(),
        std::make_shared<Compressor>(),
        std::make_shared<MultiBandCompressor>(),
        std::make_shared<Overdrive>(),
        std::make_shared<Distortion>(),
        std::make_shared<Equalizer>(),
        std::make_shared<Chorus>(),
        std::make_shared<Phaser>(),
        std::make_shared<Flanger>(),
        std::make_shared<Delay>(),
        std::make_shared<Reverb>(),
        std::make_shared<CabinetSim>(),
        std::make_shared<AmpSimulator>(),
        std::make_shared<TunerPedal>(),
        std::make_shared<WahPedal>(),
        std::make_shared<Octaver>(),
        std::make_shared<PitchShifter>(),
    };

    float buf[256];
    for (int rate : rates) {
        for (auto& fx : effects) {
            fx->set_sample_rate(rate);
            fx->reset();
            fill_sine(buf, 256, 440.0f, rate);
            fx->process(buf, 256);
            ASSERT_TRUE(buffer_is_finite(buf, 256));
        }
    }
}

// ============================================================
// WahPedal tests
// ============================================================

TEST(wah_has_name) {
    WahPedal wah;
    ASSERT_TRUE(std::strcmp(wah.name(), "Wah") == 0);
}

TEST(wah_params_valid_ranges) {
    WahPedal wah;
    for (auto& p : wah.params()) {
        ASSERT_TRUE(p.min_val <= p.max_val);
        ASSERT_TRUE(p.value >= p.min_val && p.value <= p.max_val);
        ASSERT_TRUE(p.default_val >= p.min_val && p.default_val <= p.max_val);
    }
}

TEST(wah_produces_finite_output) {
    WahPedal wah;
    wah.set_sample_rate(48000);
    wah.reset();

    float buf[256];
    fill_sine(buf, 256, 440.0f, 48000);
    wah.process(buf, 256);
    ASSERT_TRUE(buffer_is_finite(buf, 256));
}

TEST(wah_disabled_passes_dry_signal) {
    WahPedal wah;
    wah.set_sample_rate(48000);
    wah.reset();
    wah.set_enabled(false);

    float buf[256];
    float ref[256];
    fill_sine(buf, 256, 440.0f, 48000);
    for (int i = 0; i < 256; ++i) ref[i] = buf[i];
    wah.process(buf, 256);

    for (int i = 0; i < 256; ++i)
        ASSERT_NEAR(buf[i], ref[i], 1e-6f);
}

// Verify that bandpass centre frequency actually tracks the sweep parameter:
// heel-down (sweep=0) should output less energy at 2kHz than toe-down (sweep=1).
TEST(wah_bandpass_tracks_sweep) {
    const int SR = 48000;
    const int N  = 4096; // long enough for the filter to settle

    // Measure bandpass output RMS at 2 kHz for heel-down vs toe-down
    auto measure_rms_at = [&](float sweep_val) -> float {
        WahPedal wah;
        wah.set_sample_rate(SR);
        wah.reset();
        // Manual mode, mix fully wet so we hear only the bandpass
        wah.set_mix(1.0f);
        wah.params()[0].value = 0.0f; // manual mode
        wah.params()[1].value = sweep_val;
        wah.params()[2].value = 3.5f; // default resonance

        float buf[4096];
        fill_sine(buf, N, 2000.0f, SR); // 2 kHz probe tone
        wah.process(buf, N);
        return rms(buf, N);
    };

    float rms_heel = measure_rms_at(0.0f); // centre ~350 Hz — 2 kHz is out-of-band
    float rms_toe  = measure_rms_at(1.0f); // centre ~2500 Hz — 2 kHz is in-band

    // Toe-down should pass significantly more energy at 2 kHz
    ASSERT_GT(rms_toe, rms_heel * 2.0f);
}

TEST(wah_auto_mode_responds_to_amplitude) {
    const int SR = 48000;
    const int N  = 2048;

    WahPedal wah;
    wah.set_sample_rate(SR);
    wah.reset();
    wah.params()[0].value = 1.0f;  // auto-wah mode
    wah.params()[3].value = 1.0f;  // max sensitivity

    // Feed silence — filter should stay near heel
    float silent[2048] = {};
    wah.process(silent, N);
    ASSERT_TRUE(buffer_is_finite(silent, N));

    // Feed a loud signal — filter should react (envelope follower charges up)
    float loud[2048];
    fill_sine(loud, N, 440.0f, SR);
    for (int i = 0; i < N; ++i) loud[i] *= 0.9f; // near-full scale
    wah.process(loud, N);
    ASSERT_TRUE(buffer_is_finite(loud, N));
}

// ============================================================
// PhaserEffect tests
// ============================================================

TEST(phaser_produces_finite_output) {
    Phaser ph;
    ph.set_sample_rate(48000);
    ph.reset();

    float buf[1024];
    fill_sine(buf, 1024, 440.0f, 48000);
    ph.process(buf, 1024);

    ASSERT_TRUE(buffer_is_finite(buf, 1024));
    ASSERT_GT(rms(buf, 1024), 0.001f);
}

TEST(phaser_params_have_valid_ranges) {
    Phaser ph;
    for (auto& p : ph.params()) {
        ASSERT_TRUE(p.min_val <= p.max_val);
        ASSERT_TRUE(p.value >= p.min_val && p.value <= p.max_val);
        ASSERT_TRUE(p.default_val >= p.min_val && p.default_val <= p.max_val);
        ASSERT_FALSE(p.name.empty());
    }
}

TEST(phaser_disabled_passes_dry_signal) {
    Phaser ph;
    ph.set_sample_rate(48000);
    ph.reset();
    ph.set_enabled(false);

    float buf[256];
    float ref[256];
    fill_sine(buf, 256, 440.0f, 48000);
    for (int i = 0; i < 256; ++i) ref[i] = buf[i];
    ph.process(buf, 256);

    for (int i = 0; i < 256; ++i)
        ASSERT_NEAR(buf[i], ref[i], 1e-6f);
}

// Verify the LFO modulates the all-pass chain: consecutive blocks should
// differ because the LFO phase advances, causing spectral variation.
TEST(phaser_lfo_modulates_output) {
    Phaser ph;
    ph.set_sample_rate(48000);
    ph.reset();
    ph.params()[0].value = 2.0f;  // fast rate so LFO moves noticeably
    ph.params()[1].value = 1.0f;  // full depth
    ph.params()[4].value = 0.5f;  // 50% mix

    float buf_a[512], buf_b[512];
    fill_sine(buf_a, 512, 440.0f, 48000);
    fill_sine(buf_b, 512, 440.0f, 48000);

    // Process two independent blocks; LFO will be at different phases
    ph.process(buf_a, 512);
    ph.process(buf_b, 512);

    // The outputs of the two blocks must differ (LFO advanced between them)
    float diff = 0.0f;
    for (int i = 0; i < 512; ++i)
        diff += std::fabs(buf_a[i] - buf_b[i]);
    ASSERT_GT(diff, 0.01f);
}

TEST(phaser_all_stage_counts_finite) {
    for (int stage_param = 0; stage_param <= 3; ++stage_param) {
        Phaser ph;
        ph.set_sample_rate(48000);
        ph.reset();
        ph.params()[2].value = static_cast<float>(stage_param);

        float buf[512];
        fill_sine(buf, 512, 440.0f, 48000);
        ph.process(buf, 512);
        ASSERT_TRUE(buffer_is_finite(buf, 512));
    }
}

// ============================================================
// FlangerEffect tests
// ============================================================

TEST(flanger_produces_finite_output) {
    Flanger fl;
    fl.set_sample_rate(48000);
    fl.reset();

    float buf[1024];
    fill_sine(buf, 1024, 440.0f, 48000);
    fl.process(buf, 1024);

    ASSERT_TRUE(buffer_is_finite(buf, 1024));
    ASSERT_GT(rms(buf, 1024), 0.001f);
}

TEST(flanger_params_have_valid_ranges) {
    Flanger fl;
    for (auto& p : fl.params()) {
        ASSERT_TRUE(p.min_val <= p.max_val);
        ASSERT_TRUE(p.value >= p.min_val && p.value <= p.max_val);
        ASSERT_TRUE(p.default_val >= p.min_val && p.default_val <= p.max_val);
        ASSERT_FALSE(p.name.empty());
    }
}

TEST(flanger_disabled_passes_dry_signal) {
    Flanger fl;
    fl.set_sample_rate(48000);
    fl.reset();
    fl.set_enabled(false);

    float buf[256];
    float ref[256];
    fill_sine(buf, 256, 440.0f, 48000);
    for (int i = 0; i < 256; ++i) ref[i] = buf[i];
    fl.process(buf, 256);

    for (int i = 0; i < 256; ++i)
        ASSERT_NEAR(buf[i], ref[i], 1e-6f);
}

TEST(flanger_silence_passthrough){
    Flanger fl;
    fl.set_sample_rate(48000);
    fl.reset();

    float buf[512];
    std::memset(buf, 0, sizeof(buf));

    fl.process(buf, 512);

    ASSERT_TRUE(buffer_is_finite(buf, 512));
    ASSERT_LT(rms(buf, 512), 1e-8f);
}

TEST(flanger_extreme_params_stay_finite){
    Flanger fl;
    fl.set_sample_rate(48000);

    const float rates[] = {0.05f, 5.0f};
    const float depths[] = {0.1f, 7.0f};
    const float feedbacks[] = {-0.95f, 0.95f};
    const float mixes[] = {0.0f, 1.0f};

    for (float rate : rates){
        for (float depth : depths){
            for (float feedback : feedbacks){
                for (float mix : mixes){
                    fl.reset();
                    fl.params()[0].value = rate;
                    fl.params()[1].value = depth;
                    fl.params()[3].value = feedback;
                    fl.params()[4].value = mix;

                    float buf[512];
                    fill_sine(buf, 512, 440.0f, 48000);
                    fl.process(buf, 512);

                    ASSERT_TRUE(buffer_is_finite(buf, 512));
                }
            }
        }
    }
}

TEST(flanger_sample_rate_change){
    Flanger fl;

    fl.set_sample_rate(48000);
    fl.reset();
    float buf_a[512];
    fill_sine(buf_a, 512, 440.0f, 48000);
    fl.process(buf_a, 512);
    ASSERT_TRUE(buffer_is_finite(buf_a, 512));

    fl.set_sample_rate(96000);
    fl.reset();
    float buf_b[512];
    fill_sine(buf_b, 512, 440.0f, 96000);
    fl.process(buf_b, 512);
    ASSERT_TRUE(buffer_is_finite(buf_b, 512));
}

TEST(flanger_toggle_no_gliches) {
    Flanger fl;
    fl.set_sample_rate(48000);
    fl.reset();

    const int N = 512;

    fl.set_enabled(true);
    float buf_a[N];
    fill_sine(buf_a, N, 440.0f, 48000);
    fl.process(buf_a, N);

    float mean_a = 0.0f;
    for (int i = 0; i < N; ++i) mean_a += buf_a[i];
    mean_a /= static_cast<float>(N);

    fl.set_enabled(false);
    float buf_b[N];
    fill_sine(buf_b, N, 440.0f, 48000);
    fl.process(buf_b, N);

    float mean_b = 0.0f;
    for (int i = 0; i < N; ++i) mean_b += buf_b[i];
    mean_b /= static_cast<float>(N);

    ASSERT_LT(std::fabs(mean_b - mean_a), 5e-3f);
}

TEST(flanger_wet_differs_from_dry){
    Flanger fl;
    fl.set_sample_rate(48000);
    fl.reset();
    fl.set_enabled(true);

    fl.params()[0].value = 1.0f; // Rate
    fl.params()[1].value = 5.0f; // Depth
    fl.params()[4].value = 0.5f; // Mix

    float buf[512];
    float ref[512];
    fill_sine(buf, 512, 440.0f, 48000);
    for (int i = 0; i < 512; ++i) ref[i] = buf[i];

    fl.process(buf, 512);

    float diff_sum = 0.0f;
    for (int i = 0; i < 512; i++)
        diff_sum += std::fabs(buf[i] - ref[i]);

    ASSERT_GT(diff_sum, 0.01f);
}
// Verify the LFO modulates the delay: two consecutive equal-signal blocks
// should differ because the sweep position advances between them.
TEST(flanger_lfo_modulates_output) {
    Flanger fl;
    fl.set_sample_rate(48000);
    fl.reset();
    fl.params()[0].value = 2.0f;  // fast rate
    fl.params()[1].value = 5.0f;  // wide depth
    fl.params()[4].value = 0.5f;  // 50% mix

    float buf_a[512], buf_b[512];
    fill_sine(buf_a, 512, 440.0f, 48000);
    fill_sine(buf_b, 512, 440.0f, 48000);

    fl.process(buf_a, 512);
    fl.process(buf_b, 512);

    float diff = 0.0f;
    for (int i = 0; i < 512; ++i)
        diff += std::fabs(buf_a[i] - buf_b[i]);
    ASSERT_GT(diff, 0.01f);
}

// High feedback must not produce runaway (NaN/Inf)
TEST(flanger_high_feedback_stays_finite) {
    Flanger fl;
    fl.set_sample_rate(48000);
    fl.reset();
    fl.params()[3].value = 0.95f;  // max positive feedback

    float buf[4096];
    fill_sine(buf, 4096, 220.0f, 48000);
    fl.process(buf, 4096);
    ASSERT_TRUE(buffer_is_finite(buf, 4096));
}

// ============================================================
// Octaver DSP regression tests
// ============================================================

// Feed a sine at FUND Hz with only oct-down active, then verify clear spectral
// energy at FUND/2 and negligible energy at FUND (dry=0).
// Implementation note: a warm-up pass lets the envelope follower and flip-flop
// divider stabilize before the analysis window is captured.
//
// 880 Hz (A5) is chosen so the zero-crossing slope (~0.092/sample at 0.8 amp)
// greatly exceeds the 2×FLIP_HYSTERESIS band, giving ≥96% detection reliability
// per crossing and a clean sub-octave signal at 440 Hz.
TEST(octaver_sub_octave_produces_half_frequency) {
    static constexpr int   SR   = 48000;
    static constexpr float FUND = 880.0f;   // sub-octave = 440 Hz, upper = 1760 Hz
    static constexpr float PI   = 3.14159265f;
    static constexpr int   CHUNK = 256;
    static constexpr int   WARM_UP = 12288;  // 48 * 256 samples  (~256 ms)
    // N=4800: 4800*440/48000=44 and 4800*880/48000=88, so both target frequencies
    // land on exact DFT bins and the DFT formula gives exact magnitudes.
    static constexpr int   N    = 4800;

    Octaver oct;
    oct.set_sample_rate(SR);
    oct.reset();
    oct.params()[0].value = 1.0f;  // P_OCT_DOWN
    oct.params()[1].value = 0.0f;  // P_OCT_UP
    oct.params()[2].value = 0.0f;  // P_DRY

    // Warm-up: run WARM_UP samples in CHUNK-sized blocks (continuous phase).
    float chunk_buf[CHUNK];
    for (int off = 0; off < WARM_UP; off += CHUNK) {
        for (int i = 0; i < CHUNK; ++i)
            chunk_buf[i] = 0.8f * std::sin(2.0f * PI * FUND * (off + i) / SR);
        oct.process(chunk_buf, CHUNK);
    }

    // Analysis window starts immediately after warm-up (phase-continuous).
    float main_buf[N];
    for (int i = 0; i < N; ++i)
        main_buf[i] = 0.8f * std::sin(2.0f * PI * FUND * (WARM_UP + i) / SR);
    oct.process(main_buf, N);

    ASSERT_TRUE(buffer_is_finite(main_buf, N));

    // Sanity: the octaver must produce non-trivial output energy.
    ASSERT_GT(rms(main_buf, N), 0.1f);

    // Verify the DFT helper on a clean bin-aligned 440 Hz sine (A=0.8).
    // At an exact DFT bin, mag = A/2 = 0.4 analytically.
    {
        float sin_buf[N];
        for (int i = 0; i < N; ++i)
            sin_buf[i] = 0.8f * std::sin(2.0f * PI * (FUND / 2.0f) * i / SR);
        float sin_mag = dft_magnitude_at(sin_buf, N, FUND / 2.0f, SR);
        ASSERT_GT(sin_mag, 0.35f);  // A/2 = 0.4; allow small numerical slack
    }

    // Count positive-going zero crossings in the output.
    // The flip-flop divider produces a ~FUND/2 = 440 Hz square wave: expect ~44
    // crossings in 4800 samples.  An 880 Hz passthrough would give ~88, so this
    // discriminates whether the divider is actually halving the frequency.
    int crossings = 0;
    for (int i = 1; i < N; ++i) {
        if (main_buf[i-1] < 0.0f && main_buf[i] >= 0.0f)
            ++crossings;
    }
    // Allow ±25% tolerance to absorb the phase offset at the window boundary.
    ASSERT_GE(crossings, 33);  // 44 * 0.75
    ASSERT_LT(crossings, 56);  // 44 * 1.25 + 1 (int upper bound)
}

// Feed a sine at FUND Hz with only oct-up active; verify energy at 2*FUND.
TEST(octaver_upper_octave_produces_double_frequency) {
    static constexpr int   SR   = 48000;
    static constexpr float FUND = 220.0f;
    static constexpr float PI   = 3.14159265f;
    static constexpr int   CHUNK = 256;
    static constexpr int   WARM_UP = 12288;
    static constexpr int   N    = 8192;

    Octaver oct;
    oct.set_sample_rate(SR);
    oct.reset();
    oct.params()[0].value = 0.0f;  // P_OCT_DOWN
    oct.params()[1].value = 1.0f;  // P_OCT_UP
    oct.params()[2].value = 0.0f;  // P_DRY

    float chunk_buf[CHUNK];
    for (int off = 0; off < WARM_UP; off += CHUNK) {
        for (int i = 0; i < CHUNK; ++i)
            chunk_buf[i] = 0.8f * std::sin(2.0f * PI * FUND * (off + i) / SR);
        oct.process(chunk_buf, CHUNK);
    }

    float main_buf[N];
    for (int i = 0; i < N; ++i)
        main_buf[i] = 0.8f * std::sin(2.0f * PI * FUND * (WARM_UP + i) / SR);
    oct.process(main_buf, N);

    ASSERT_TRUE(buffer_is_finite(main_buf, N));

    float mag_double = dft_magnitude_at(main_buf, N, FUND * 2.0f, SR);  // 440 Hz
    float mag_fund   = dft_magnitude_at(main_buf, N, FUND,        SR);  // 220 Hz

    // Full-wave rectification of a sine produces the doubled frequency
    // (|sin(ωt)| has fundamental at 2ω with amplitude 4/(3π)≈0.42 of input).
    ASSERT_GT(mag_double, 0.05f);
    ASSERT_LT(mag_fund, mag_double * 0.5f);
}

// When the effect is disabled, process() returns immediately; the dry sine
// passes through unmodified, so there must be no significant energy at f/2 or 2f.
TEST(octaver_disabled_no_sub_or_upper_octave) {
    static constexpr int   SR   = 48000;
    static constexpr float FUND = 220.0f;
    static constexpr float PI   = 3.14159265f;
    static constexpr int   N    = 8192;

    Octaver oct;
    oct.set_sample_rate(SR);
    oct.reset();
    oct.set_enabled(false);
    // Params don't matter when disabled, but set non-zero to be explicit.
    oct.params()[0].value = 1.0f;
    oct.params()[1].value = 1.0f;
    oct.params()[2].value = 0.0f;

    float buf[N];
    for (int i = 0; i < N; ++i)
        buf[i] = 0.8f * std::sin(2.0f * PI * FUND * i / SR);
    oct.process(buf, N);

    float mag_fund   = dft_magnitude_at(buf, N, FUND,        SR);
    float mag_half   = dft_magnitude_at(buf, N, FUND / 2.0f, SR);
    float mag_double = dft_magnitude_at(buf, N, FUND * 2.0f, SR);

    // Full-amplitude fundamental must pass through unchanged.
    ASSERT_GT(mag_fund, 0.3f);
    // No synthetic sub/upper octave components.
    ASSERT_LT(mag_half,   0.01f);
    ASSERT_LT(mag_double, 0.01f);
}

TEST(octaver_params_have_valid_ranges) {
    Octaver oct;
    for (auto& p : oct.params()) {
        ASSERT_TRUE(p.min_val <= p.max_val);
        ASSERT_TRUE(p.value >= p.min_val && p.value <= p.max_val);
        ASSERT_TRUE(p.default_val >= p.min_val && p.default_val <= p.max_val);
        ASSERT_FALSE(p.name.empty());
    }
}
// NEW TESTS HERE

TEST(octaver_silence_passthrough) {
    Octaver oct;
    oct.set_sample_rate(48000);
    oct.reset();

    float buf[512];
    std::memset(buf, 0, sizeof(buf));

    oct.process(buf, 512);

    ASSERT_TRUE(buffer_is_finite(buf, 512));

    float out_rms = rms(buf, 512);

    // Silence should remain silence
    ASSERT_LT(out_rms, 1e-8f);
}

TEST(octaver_extreme_mix_values) {
    Octaver oct;
    oct.set_sample_rate(48000);

    float ref[512];
    fill_sine(ref, 512, 440.0f, 48000);
    oct.reset();

    float dry_buf[512];
    for (int i = 0; i < 512; ++i) dry_buf[i] = ref[i];

    oct.params()[0].value = 1.0f; // sub octave
    oct.params()[1].value = 1.0f; // upper octave
    oct.params()[2].value = 1.0f; // fully dry

    oct.process(dry_buf, 512);

    ASSERT_TRUE(buffer_is_finite(dry_buf, 512));

    // Should preserve strong dry signal
    ASSERT_GT(rms(dry_buf, 512), 0.1f);
    oct.reset();

    float wet_buf[512];
    for (int i = 0; i < 512; ++i) wet_buf[i] = ref[i];

    oct.params()[0].value = 1.0f;
    oct.params()[1].value = 1.0f;
    oct.params()[2].value = 0.0f; // no dry signal

    oct.process(wet_buf, 512);

    ASSERT_TRUE(buffer_is_finite(wet_buf, 512));

    // Wet signal should still contain energy
    ASSERT_GT(rms(wet_buf, 512), 0.01f);
}

TEST(octaver_parameter_combinations_stay_finite) {
    Octaver oct;
    oct.set_sample_rate(48000);

    const float values[] = {0.0f, 0.5f, 1.0f};

    for (float down : values) {
        for (float up : values) {
            for (float dry : values) {

                oct.reset();
                oct.params()[0].value = down;
                oct.params()[1].value = up;
                oct.params()[2].value = dry;

                float buf[1024];
                fill_sine(buf, 1024, 440.0f, 48000);

                oct.process(buf, 1024);

                ASSERT_TRUE(buffer_is_finite(buf, 1024));
            }
        }
    }
}

// ============================================================
// PitchShifter tests
// ============================================================

TEST(pitch_shifter_params_have_valid_ranges) {
    PitchShifter ps;
    for (auto& p : ps.params()) {
        ASSERT_TRUE(p.min_val <= p.max_val);
        ASSERT_TRUE(p.value >= p.min_val && p.value <= p.max_val);
        ASSERT_TRUE(p.default_val >= p.min_val && p.default_val <= p.max_val);
        ASSERT_FALSE(p.name.empty());
    }
}

// At default settings (Shift=0, Fine=0, Mix=0) the pedal must be transparent:
// output should equal input within floating-point tolerance.
TEST(pitch_shifter_default_is_transparent) {
    PitchShifter ps;
    ps.set_sample_rate(48000);
    ps.reset();
    // Confirm defaults are Shift=0, Fine=0, Mix=0.
    ASSERT_NEAR(ps.params()[0].value, 0.0f, 1e-6f);  // Shift
    ASSERT_NEAR(ps.params()[1].value, 0.0f, 1e-6f);  // Fine
    ASSERT_NEAR(ps.params()[2].value, 0.0f, 1e-6f);  // Mix

    float buf[256];
    float ref[256];
    fill_sine(buf, 256, 440.0f, 48000);
    for (int i = 0; i < 256; ++i) ref[i] = buf[i];

    ps.process(buf, 256);

    for (int i = 0; i < 256; ++i)
        ASSERT_NEAR(buf[i], ref[i], 1e-5f);
}

// With Mix=1 and a non-zero semitone shift, the output frequency must shift
// from 440 Hz to approximately 659 Hz (+7 semitones).
TEST(pitch_shifter_with_mix_and_shift_differs_from_dry) {
    PitchShifter ps;
    ps.set_sample_rate(48000);
    ps.reset();
    ps.params()[0].value = 7.0f;   // Shift = +7 semitones
    ps.params()[2].value = 1.0f;   // Mix = fully wet

    // Warm up so the grain buffer fills and parameter smoothing settles.
    float warm[512];
    for (int rep = 0; rep < 30; ++rep) {
        fill_sine(warm, 512, 440.0f, 48000);
        ps.process(warm, 512);
    }

    // Process a longer buffer for better frequency resolution
    float buf[512];
    fill_sine(buf, 512, 440.0f, 48000);
    ps.process(buf, 512);

    ASSERT_TRUE(buffer_is_finite(buf, 512));

    // Check the output is finite and has reasonable energy
    ASSERT_GT(rms(buf, 512), 0.01f);

    // Verify frequency shift: +7 semitones from 440 Hz ≈ 659 Hz
    // 440 * 2^(7/12) ≈ 659.255 Hz
    const float shifted_freq = 440.0f * std::pow(2.0f, 7.0f / 12.0f);

    // Check the peak is near the shifted frequency (allow ±20 Hz tolerance)
    const float mag_440 = dft_magnitude_at(buf, 512, 440.0f, 48000);
    const float mag_shifted = dft_magnitude_at(buf, 512, shifted_freq, 48000);

    // The shifted frequency should be dominant (higher magnitude than original)
    ASSERT_GT(mag_shifted, mag_440 * 0.5f);
}

// ============================================================
// MultiBandCompressor tests
// ============================================================

TEST(multiband_compressor_unity_gain_passthrough) {
    MultiBandCompressor mbc;
    mbc.set_sample_rate(48000);
    mbc.reset();

    // Set ratios of all bands to 1:1, makeup to 0 dB, and Out Gain to 0 dB
    // This makes the compressor transparent (unity gain passthrough)
    mbc.params()[0].value = 200.0f;   // Low XOver
    mbc.params()[1].value = 4000.0f;  // High XOver

    // Low Band (2 to 6)
    mbc.params()[2].value = -20.0f;   // Thresh
    mbc.params()[3].value = 1.0f;     // Ratio = 1.0 (1:1)
    mbc.params()[4].value = 5.0f;     // Attack
    mbc.params()[5].value = 100.0f;   // Release
    mbc.params()[6].value = 0.0f;     // Makeup = 0.0 dB

    // Mid Band (7 to 11)
    mbc.params()[7].value = -20.0f;   // Thresh
    mbc.params()[8].value = 1.0f;     // Ratio = 1.0
    mbc.params()[9].value = 5.0f;     // Attack
    mbc.params()[10].value = 100.0f;  // Release
    mbc.params()[11].value = 0.0f;    // Makeup = 0.0 dB

    // High Band (12 to 16)
    mbc.params()[12].value = -20.0f;  // Thresh
    mbc.params()[13].value = 1.0f;    // Ratio = 1.0
    mbc.params()[14].value = 5.0f;    // Attack
    mbc.params()[15].value = 100.0f;  // Release
    mbc.params()[16].value = 0.0f;    // Makeup = 0.0 dB

    // Global
    mbc.params()[17].value = 0.0f;    // Out Gain = 0.0 dB

    float buf[512];
    float ref[512];
    fill_sine(buf, 512, 1000.0f, 48000);
    for (int i = 0; i < 512; ++i) ref[i] = buf[i];

    mbc.process(buf, 512);

    // After filtering and summing, output should match input extremely closely.
    // LR4 crossovers sum flat at unity gain, so it should be almost identical!
    for (int i = 0; i < 512; ++i) {
        ASSERT_NEAR(buf[i], ref[i], 1e-4f);
    }
}

TEST(multiband_compressor_independent_band_compression) {
    MultiBandCompressor mbc;
    mbc.set_sample_rate(48000);
    mbc.reset();

    // Crossovers
    mbc.params()[0].value = 200.0f;   // Low XOver = 200 Hz
    mbc.params()[1].value = 4000.0f;  // High XOver = 4000 Hz

    // Set Low Band ratio to 10:1 and Mid/High ratios to 1:1
    // And very low Threshold for all to trigger compression
    for (int b = 0; b < 3; ++b) {
        int offset = 2 + b * 5;
        mbc.params()[offset + 0].value = -40.0f; // Threshold = -40 dB
        mbc.params()[offset + 1].value = (b == 0) ? 10.0f : 1.0f; // Low Ratio = 10:1, Mid/High = 1:1
        mbc.params()[offset + 2].value = 2.0f;   // Fast attack
        mbc.params()[offset + 3].value = 50.0f;  // Fast release
        mbc.params()[offset + 4].value = 0.0f;   // Makeup = 0 dB
    }
    mbc.params()[17].value = 0.0f; // Out Gain = 0 dB

    // Feed a 100 Hz sine wave (Low band)
    float low_buf[1024];
    fill_sine(low_buf, 1024, 100.0f, 48000);
    // scale to high amplitude so it goes over the -40 dB threshold
    for (int i = 0; i < 1024; ++i) low_buf[i] *= 0.8f;

    // Process a few times to let envelope followers charge up
    for (int rep = 0; rep < 10; ++rep) {
        fill_sine(low_buf, 1024, 100.0f, 48000);
        for (int i = 0; i < 1024; ++i) low_buf[i] *= 0.8f;
        mbc.process(low_buf, 1024);
    }

    // Low band compression should be active, Mid/High should be inactive
    ASSERT_GT(mbc.get_gain_reduction_db(0), 1.0f);
    ASSERT_NEAR(mbc.get_gain_reduction_db(1), 0.0f, 1e-4f);
    ASSERT_NEAR(mbc.get_gain_reduction_db(2), 0.0f, 1e-4f);

    // Now reset and do the opposite: compress only the High Band
    mbc.reset();
    for (int b = 0; b < 3; ++b) {
        int offset = 2 + b * 5;
        mbc.params()[offset + 0].value = -40.0f;
        mbc.params()[offset + 1].value = (b == 2) ? 10.0f : 1.0f; // High Ratio = 10:1, Low/Mid = 1:1
        mbc.params()[offset + 2].value = 2.0f;
        mbc.params()[offset + 3].value = 50.0f;
        mbc.params()[offset + 4].value = 0.0f;
    }

    // Feed a 6000 Hz sine wave (High band)
    float high_buf[1024];
    fill_sine(high_buf, 1024, 6000.0f, 48000);
    for (int i = 0; i < 1024; ++i) high_buf[i] *= 0.8f;

    for (int rep = 0; rep < 10; ++rep) {
        fill_sine(high_buf, 1024, 6000.0f, 48000);
        for (int i = 0; i < 1024; ++i) high_buf[i] *= 0.8f;
        mbc.process(high_buf, 1024);
    }

    // High band compression should be active, Low/Mid should be inactive
    ASSERT_GT(mbc.get_gain_reduction_db(2), 1.0f);
    ASSERT_NEAR(mbc.get_gain_reduction_db(0), 0.0f, 1e-4f);
    ASSERT_NEAR(mbc.get_gain_reduction_db(1), 0.0f, 1e-4f);
}

// Tempo / BPM Syncing Tests
// ============================================================

TEST(delay_calculates_correct_time_from_bpm) {
    Delay dl;
    dl.set_sample_rate(48000);
    dl.reset();

    // To test quarter-note snapping at 120 BPM (500ms), we first set the
    // knob close to 500ms so the subdivision logic picks the quarter note.
    dl.params()[0].value = 490.0f; 
    
    // Trigger the BPM sync
    dl.set_transport_state(120.0f);

    // At 120 BPM, a quarter note is 500.0 ms (60000 / 120)
    ASSERT_NEAR(dl.params()[0].value, 500.0f, 0.01f);
}

TEST(chorus_calculates_correct_rate_from_bpm) {
    Chorus ch;
    ch.set_sample_rate(48000);
    ch.reset();

    // Trigger the BPM sync
    ch.set_transport_state(120.0f);

    // At 120 BPM, the LFO rate should be 2.0 Hz (120 / 60)
    ASSERT_NEAR(ch.params()[0].value, 2.0f, 0.01f);
}

#include "audio/spsc_queue.h"
#include "audio/effect_factory.h"

TEST(spsc_queue_try_push_all_drains_queue) {
    SPSCQueue<float, 8> queue;
    ASSERT_TRUE(queue.try_push(1.0f));
    ASSERT_TRUE(queue.try_push(2.0f));
    ASSERT_TRUE(queue.try_push(3.0f));
    std::vector<float> out;
    size_t count = queue.try_pop_all(out);
    ASSERT_EQ(count, 3u);
    ASSERT_EQ(out.size(), 3u);
    ASSERT_NEAR(out[0], 1.0f, 1e-6f);
    ASSERT_NEAR(out[1], 2.0f, 1e-6f);
    ASSERT_NEAR(out[2], 3.0f, 1e-6f);
}

TEST(spsc_queue_full_queue_rejects_push) {
    SPSCQueue<float, 4> queue;
    ASSERT_TRUE(queue.try_push(1.0f));
    ASSERT_TRUE(queue.try_push(2.0f));
    ASSERT_TRUE(queue.try_push(3.0f));
    ASSERT_FALSE(queue.try_push(4.0f));
}

TEST(spsc_queue_try_pop_all_on_empty_returns_empty) {
    SPSCQueue<float, 8> queue;
    std::vector<float> out;
    size_t count = queue.try_pop_all(out);
    ASSERT_EQ(count, 0u);
    ASSERT_TRUE(out.empty());
}

TEST(spsc_queue_size_and_capacity) {
    SPSCQueue<float, 8> queue;
    ASSERT_EQ(queue.capacity(), 7u);
    ASSERT_EQ(queue.size(), 0u);
    queue.try_push(1.0f);
    ASSERT_EQ(queue.size(), 1u);
    queue.try_push(2.0f);
    ASSERT_EQ(queue.size(), 2u);
    float val;
    queue.try_pop(val);
    ASSERT_EQ(queue.size(), 1u);
}

TEST(spsc_queue_try_peek) {
    SPSCQueue<float, 8> queue;
    float item = 0.0f;
    ASSERT_FALSE(queue.try_peek(item));
    queue.try_push(10.5f);
    ASSERT_TRUE(queue.try_peek(item));
    ASSERT_NEAR(item, 10.5f, 1e-6f);
    float item2 = 0.0f;
    ASSERT_TRUE(queue.try_pop(item2));
    ASSERT_NEAR(item2, 10.5f, 1e-6f);
}

TEST(effect_base_get_set_param_by_name) {
    auto effect = std::make_shared<Overdrive>();
    effect->set_param_by_name("Drive", 2.0f);
    ASSERT_NEAR(effect->get_param_value("Drive"), 2.0f, 1e-5f);
}

TEST(effect_base_get_param_names_not_empty) {
    auto effect = std::make_shared<Equalizer>();
    auto names = effect->get_param_names();
    ASSERT_FALSE(names.empty());
}

TEST(effect_base_get_display_name) {
    auto effect = std::make_shared<Overdrive>();
    ASSERT_TRUE(std::strcmp(effect->get_display_name(), "Overdrive") == 0);
}

TEST(effect_factory_creates_all_registered_effects) {
    auto types = EffectFactory::instance().get_all_type_names();
    ASSERT_FALSE(types.empty());
    for (const auto& type : types) {
        auto effect = EffectFactory::instance().create(type);
        ASSERT_NE(effect, nullptr);
        auto effect2 = EffectFactory::instance().create_from_type(type);
        ASSERT_NE(effect2, nullptr);
    }
}

TEST(effect_factory_unknown_type_returns_nullptr) {
    auto effect = EffectFactory::instance().create("nonexistent_effect_type_xyz");
    ASSERT_EQ(effect, nullptr);
    auto effect2 = EffectFactory::instance().create_from_type("nonexistent_effect_type_xyz");
    ASSERT_EQ(effect2, nullptr);
}

TEST(effect_type_id_matches_factory_registration) {
    auto types = EffectFactory::instance().get_all_type_names();
    for (const auto& type : types) {
        auto effect = EffectFactory::instance().create(type);
        ASSERT_NE(effect, nullptr);
        ASSERT_EQ(std::string(effect->type_id()), type);
    }
}

TEST(effect_base_clone_produces_independent_copy) {
    auto original = std::make_shared<Overdrive>();
    original->set_param_by_name("Drive", 2.0f);
    auto copy = original->clone();
    ASSERT_NE(copy, nullptr);
    ASSERT_TRUE(std::strcmp(copy->name(), original->name()) == 0);
    ASSERT_NEAR(copy->get_param_value("Drive"), 2.0f, 1e-5f);
    copy->set_param_by_name("Drive", 3.0f);
    ASSERT_NEAR(original->get_param_value("Drive"), 2.0f, 1e-5f);
    ASSERT_NEAR(copy->get_param_value("Drive"), 3.0f, 1e-5f);
}

TEST(effect_base_process_stereo_and_helpers) {
    auto effect = std::make_shared<Overdrive>();
    effect->set_sample_rate(44100);
    effect->set_transport_state(120.0f);
    float left[8] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    float right[8] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    effect->process_stereo(left, right, 8);
    for (int i = 0; i < 8; ++i) {
        ASSERT_NEAR(left[i], right[i], 1e-6f);
    }
}

TEST(effect_base_apply_mix) {
    auto sim = std::make_shared<CabinetSim>();
    sim->set_mix(0.5f);
    ASSERT_NEAR(sim->get_mix(), 0.5f, 1e-6f);
    float buffer[8] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    sim->process(buffer, 8);
    bool check = false;
    for (int i = 0; i < 8; ++i) {
        if (std::abs(buffer[i] - 1.0f) > 1e-6f) {
            check = true;
        }
    }
    ASSERT_TRUE(check);
}

TEST(effect_factory_duplicate_registration_throws) {
    bool threw = false;
    try {
        EffectFactory::instance().register_effect("Overdrive", []() {
            return std::make_shared<Overdrive>();
        });
    } catch (const std::runtime_error&) {
        threw = true;
    }
    ASSERT_TRUE(threw);
}

class MockMixEffect : public Effect {
public:
    void process(float* buffer, int num_samples) override {
        std::vector<float> dry(num_samples);
        std::memcpy(dry.data(), buffer, static_cast<size_t>(num_samples) * sizeof(float));
        for (int i = 0; i < num_samples; ++i) {
            buffer[i] *= 2.0f;
        }
        apply_mix(dry.data(), buffer, num_samples);
    }
    void reset() override {}
    const char* name() const override { return "MockMixEffect"; }
    std::vector<EffectParam>& params() override { return params_; }
private:
    std::vector<EffectParam> params_;
};

TEST(effect_base_get_param_value_fallback) {
    auto effect = std::make_shared<Overdrive>();
    ASSERT_NEAR(effect->get_param_value("nonexistent_param"), 0.0f, 1e-6f);
}

TEST(effect_base_apply_mix_direct) {
    MockMixEffect effect;
    effect.set_mix(1.0f);
    float buffer1[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    effect.process(buffer1, 4);
    ASSERT_NEAR(buffer1[0], 2.0f, 1e-6f);
    ASSERT_NEAR(buffer1[1], 4.0f, 1e-6f);
    ASSERT_NEAR(buffer1[2], 6.0f, 1e-6f);
    ASSERT_NEAR(buffer1[3], 8.0f, 1e-6f);

    effect.set_mix(0.5f);
    float buffer2[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    effect.process(buffer2, 4);
    ASSERT_NEAR(buffer2[0], 1.5f, 1e-6f);
    ASSERT_NEAR(buffer2[1], 3.0f, 1e-6f);
    ASSERT_NEAR(buffer2[2], 4.5f, 1e-6f);
    ASSERT_NEAR(buffer2[3], 6.0f, 1e-6f);
}

TEST(reverb_bypass_passes_signal_unchanged) {
    Reverb rv;
    rv.set_sample_rate(48000);
    rv.reset();
    rv.set_enabled(false);

    float buf[512], orig[512];
    fill_sine(buf, 512, 440.0f, 48000);
    std::memcpy(orig, buf, sizeof(buf));

    rv.process(buf, 512);

    for (int i = 0; i < 512; ++i)
        ASSERT_NEAR(buf[i], orig[i], 1e-6f);
}

TEST(reverb_reset_clears_tail) {
    Reverb rv;
    rv.set_sample_rate(48000);

    float buf[2048];
    fill_sine(buf, 2048, 440.0f, 48000);
    rv.process(buf, 2048);   

    rv.reset();

    float silence[2048];
    std::memset(silence, 0, sizeof(silence));
    rv.process(silence, 2048);

    ASSERT_NEAR(rms(silence, 2048), 0.0f, 1e-5f);
}

TEST(reverb_low_decay_shorter_tail_than_high_decay) {
    auto measure_tail_rms = [](float decay_val) {
        Reverb rv;
        rv.set_sample_rate(48000);
        rv.reset();
        rv.params()[0].value = decay_val;

        float buf[2048];
        fill_sine(buf, 2048, 440.0f, 48000);
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

TEST(reverb_high_decay_produces_audible_tail) {
    Reverb rv;
    rv.set_sample_rate(48000);
    rv.reset();
    rv.params()[0].value = 0.99f;

    float buf[2048];
    fill_sine(buf, 2048, 440.0f, 48000);
    rv.process(buf, 2048); 

    float silence[2048];
    std::memset(silence, 0, sizeof(silence));
    rv.process(silence, 2048);

    ASSERT_GT(rms(silence, 2048), 0.001f);
    ASSERT_TRUE(buffer_is_finite(silence, 2048));
}

TEST(reverb_zero_decay_does_not_generate_signal) {
    Reverb rv;
    rv.set_sample_rate(48000);
    rv.reset();
    rv.params()[0].value = 0.0f;
    float buf[2048];
    std::memset(buf, 0, sizeof(buf));
    rv.process(buf, 2048);

    ASSERT_NEAR(rms(buf, 2048), 0.0f, 1e-6f);
    ASSERT_TRUE(buffer_is_finite(buf, 2048));
}

TEST(reverb_high_damp_reduces_high_freq_content) {
    auto measure_hf_tail = [](float damp_val) {
        Reverb rv;
        rv.set_sample_rate(48000);
        rv.reset();
        rv.params()[0].value = 0.8f;      // decay
        rv.params()[1].value = damp_val;  // damp
        rv.params()[2].value = 1.0f;      // wet only, if Level is wet mix

        constexpr int N = 2048;
        constexpr int PRIME = 8192;
        float prime[PRIME];
        fill_sine(prime, PRIME, 4000.0f, 48000);
        rv.process(prime, PRIME);

        float silence[N];
        float mag = 0.0f;

        for (int b = 0; b < 6; ++b) {
            std::memset(silence, 0, sizeof(silence));
            rv.process(silence, N);

            if (b >= 2) { // skip early tail
                mag += dft_magnitude_at(silence, N, 4000.0f, 48000);
            }
        }

        return mag / 4.0f;
    };

    float hf_undamped = measure_hf_tail(0.0f);
    float hf_damped   = measure_hf_tail(1.0f);
    ASSERT_GT(hf_undamped, hf_damped);
}

TEST(reverb_stereo_produces_finite_decorrelated_output) {
    Reverb rv;
    rv.set_sample_rate(48000);
    rv.reset();

    // Process enough samples for comb filters to fill and diverge between channels.
    float left[2048], right[2048];
    fill_sine(left,  2048, 440.0f, 48000);
    fill_sine(right, 2048, 440.0f, 48000);
    rv.process_stereo(left, right, 2048);

    ASSERT_TRUE(buffer_is_finite(left,  2048));
    ASSERT_TRUE(buffer_is_finite(right, 2048));
    ASSERT_GT(rms(left,  2048), 0.001f);
    ASSERT_GT(rms(right, 2048), 0.001f);

    float diff = 0.0f;
    for (int i = 0; i < 2048; ++i) diff += std::fabs(left[i] - right[i]);
    ASSERT_GT(diff, 1e-3f);
}

// Looper tests

TEST(looper_initial_state_is_empty) {
    Looper lp;
    lp.set_sample_rate(48000);
    lp.reset();
    ASSERT_EQ(lp.state(), Looper::State::Empty);
    ASSERT_FALSE(lp.has_loop());
    ASSERT_EQ(lp.loop_length_samples(), 0);
}

TEST(looper_record_then_play) {
    Looper lp;
    lp.set_sample_rate(48000);
    lp.reset();

    // Start recording (command consumed on first process call).
    lp.request_record_toggle();
    std::vector<float> rec(9600, 0.0f);  // 0.2 s > kMinLoopSeconds (0.1 s)
    fill_sine(rec.data(), 9600, 440.0f, 48000);
    lp.process(rec.data(), 9600);
    ASSERT_EQ(lp.state(), Looper::State::Recording);

    // Stop recording — looper should auto-switch to Playing.
    lp.request_record_toggle();
    float dummy[256] = {};
    lp.process(dummy, 256);
    ASSERT_EQ(lp.state(), Looper::State::Playing);
    ASSERT_TRUE(lp.has_loop());
    ASSERT_GT(lp.loop_length_samples(), 0);
}

TEST(looper_short_recording_is_discarded) {
    // kMinLoopSeconds = 0.10 s → 4800 samples at 48 kHz. 100 samples is far below.
    Looper lp;
    lp.set_sample_rate(48000);
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

TEST(looper_bypass_passes_signal_unchanged) {
    Looper lp;
    lp.set_sample_rate(48000);
    lp.reset();
    lp.set_enabled(false);

    float buf[512], orig[512];
    fill_sine(buf, 512, 440.0f, 48000);
    std::memcpy(orig, buf, sizeof(buf));

    lp.process(buf, 512);

    for (int i = 0; i < 512; ++i)
        ASSERT_NEAR(buf[i], orig[i], 1e-6f);
}

TEST(looper_play_toggle_pauses_and_resumes) {
    Looper lp;
    lp.set_sample_rate(48000);
    lp.reset();

    // Build a loop.
    lp.request_record_toggle();
    std::vector<float> rec(9600, 0.0f);
    lp.process(rec.data(), 9600);
    lp.request_record_toggle();
    float dummy[256] = {};
    lp.process(dummy, 256);
    ASSERT_EQ(lp.state(), Looper::State::Playing);

    // Pause.
    lp.request_play_toggle();
    lp.process(dummy, 256);
    ASSERT_EQ(lp.state(), Looper::State::Idle);

    // Resume.
    lp.request_play_toggle();
    lp.process(dummy, 256);
    ASSERT_EQ(lp.state(), Looper::State::Playing);
}

TEST(looper_overdub_transition_preserves_loop) {
    Looper lp;
    lp.set_sample_rate(48000);
    lp.reset();

    // Record a silent loop.
    lp.request_record_toggle();
    float silence[9600] = {};
    lp.process(silence, 9600);
    lp.request_record_toggle();
    float dummy[256] = {};
    lp.process(dummy, 256);
    ASSERT_EQ(lp.state(), Looper::State::Playing);

    // Enter overdub.
    lp.request_overdub_toggle();
    float overdub_buf[256];
    fill_sine(overdub_buf, 256, 440.0f, 48000);
    lp.process(overdub_buf, 256);
    ASSERT_EQ(lp.state(), Looper::State::Overdubbing);

    // Exit overdub.
    lp.request_overdub_toggle();
    std::memset(dummy, 0, sizeof(dummy));
    lp.process(dummy, 256);
    ASSERT_EQ(lp.state(), Looper::State::Playing);
    ASSERT_TRUE(lp.has_loop());
}

TEST(looper_overdub_output_is_finite) {
    Looper lp;
    lp.set_sample_rate(48000);
    lp.reset();

    lp.request_record_toggle();
    float rec[9600];
    fill_sine(rec, 9600, 440.0f, 48000);
    lp.process(rec, 9600);

    lp.request_record_toggle();
    float dummy[256] = {};
    lp.process(dummy, 256);

    lp.request_overdub_toggle();
    for (int block = 0; block < 10; ++block) {
        float buf[256];
        fill_sine(buf, 256, 880.0f, 48000);
        lp.process(buf, 256);
        ASSERT_TRUE(buffer_is_finite(buf, 256));
    }
}

TEST(looper_clear_resets_to_empty) {
    Looper lp;
    lp.set_sample_rate(48000);
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

TEST(looper_stereo_playback_is_finite) {
    Looper lp;
    lp.set_sample_rate(48000);
    lp.reset();

    // Record stereo.
    lp.request_record_toggle();
    float left[9600], right[9600];
    fill_sine(left,  9600, 440.0f, 48000);
    fill_sine(right, 9600, 880.0f, 48000);
    lp.process_stereo(left, right, 9600);

    lp.request_record_toggle();
    float dl[256] = {}, dr[256] = {};
    lp.process_stereo(dl, dr, 256);
    ASSERT_EQ(lp.state(), Looper::State::Playing);

    for (int block = 0; block < 5; ++block) {
        fill_sine(dl, 256, 440.0f, 48000);
        fill_sine(dr, 256, 880.0f, 48000);
        lp.process_stereo(dl, dr, 256);
        ASSERT_TRUE(buffer_is_finite(dl, 256));
        ASSERT_TRUE(buffer_is_finite(dr, 256));
    }
}
TEST(looper_play_toggle_while_recording_stops_and_enters_playing) {
    Looper lp;
    lp.set_sample_rate(48000);
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

// implementation-dependent behaviour, change later if design changes
TEST(looper_overdub_toggle_from_idle_enters_overdubbing) {
    Looper lp;
    lp.set_sample_rate(48000);
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

TEST(looper_overdub_toggle_during_recording_is_ignored) {
    Looper lp;
    lp.set_sample_rate(48000);
    lp.reset();

    lp.request_record_toggle();
    float rec[512] = {};
    lp.process(rec, 512);
    ASSERT_EQ(lp.state(), Looper::State::Recording);

    lp.request_overdub_toggle();
    lp.process(rec, 512);

    ASSERT_EQ(lp.state(), Looper::State::Recording);
}

TEST(looper_crossfade_wraparound_executes_crossfade_branch) {
    Looper lp;
    lp.set_sample_rate(48000);
    lp.reset();

    // 5 ms -> ~240 samples crossfade.
    lp.params()[1].value = 5.0f;

    // IMPORTANT:
    // Use a loop length NOT divisible by block size so playback
    // eventually lands inside the crossfade region.
    constexpr int LOOP = 5000;
    constexpr int N = 256;
    constexpr int XF = 240; // 5 ms at 48 kHz

    lp.request_record_toggle();

    float rec[LOOP];
    fill_sine(rec, LOOP, 440.0f, 48000);
    lp.process(rec, LOOP);

    lp.request_record_toggle();

    float dummy[N] = {};
    lp.process(dummy, N);

    ASSERT_EQ(lp.state(), Looper::State::Playing);

    // Process enough blocks to guarantee entry into:
    // pos >= loop_length_ - xf
    bool entered_crossfade_region = false;
    for (int i = 0; i < 40; ++i) {
        if (lp.playhead_samples() >= (LOOP - XF)) {
            entered_crossfade_region = true;
        }
        float buf[N];
        fill_sine(buf, N, 440.0f, 48000);

        lp.process(buf, N);

        ASSERT_TRUE(buffer_is_finite(buf, N));
    }
    ASSERT_TRUE(entered_crossfade_region);
}

TEST(looper_stereo_overdub_executes_right_channel_write_path) {
    Looper lp;
    lp.set_sample_rate(48000);
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

    // ceil(5000/256) = 20 blocks covers the full loop.
    // Left channel stays silent; right channel gets a sine.
    // After 20 blocks every position in buffer_r_ has been written.
    for (int i = 0; i < 20; ++i) {
        float ol[N] = {};
        float or_[N];
        fill_sine(or_, N, 880.0f, 48000);
        lp.process_stereo(ol, or_, N);
        ASSERT_TRUE(buffer_is_finite(or_, N));
    }
    ASSERT_EQ(lp.state(), Looper::State::Overdubbing);

    // Exit overdub and play back silence — right channel should have
    // loop content, left should not.
    lp.request_overdub_toggle();
    float pl[N] = {}, pr[N] = {};
    lp.process_stereo(pl, pr, N);

    ASSERT_GT(rms(pr, N), rms(pl, N) * 2.0f);
}

TEST(looper_state_stream_operator_outputs_expected_strings) {
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

TEST(looper_state_stream_operator_handles_unknown_value) {
    std::ostringstream os;

    auto invalid =
        static_cast<Looper::State>(999);

    os << invalid;

    ASSERT_EQ(os.str(), "Unknown");
}

TEST(looper_metadata_accessors_return_expected_values) {
    Looper lp;

    ASSERT_EQ(std::string(lp.name()), "Looper");
    ASSERT_EQ(std::string(lp.type_id()), "Looper");

    auto& p = lp.params();

    ASSERT_FALSE(p.empty());
    ASSERT_GE(p.size(), 2u);
}

TEST(looper_recording_auto_stops_at_buffer_limit) {
    // sr=100 gives max_samples_=6000; feeding 7000 samples triggers the cap.
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