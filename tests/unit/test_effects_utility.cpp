#include "test_framework.h"
#include "test_fixtures.h"
#include "audio/effects/noise_gate.h"
#include "audio/effects/compressor.h"
#include "audio/effects/multiband_compressor.h"
#include "audio/effects/overdrive.h"
#include "audio/effects/distortion.h"
#include "audio/effects/equalizer.h"
#include "audio/effects/chorus.h"
#include "audio/effects/delay.h"
#include "audio/effects/reverb.h"
#include "audio/effects/cabinet_sim.h"
#include "audio/effects/amp_simulator.h"
#include "audio/effects/tuner.h"
#include "audio/effects/wah.h"
#include "audio/effects/phaser.h"
#include "audio/effects/flanger.h"
#include "audio/effects/octaver.h"
#include "audio/effects/pitch_shifter.h"
#include "audio/utils/spsc_queue.h"
#include "audio/effects/effect_factory.h"
#include <cstring>
#include <cmath>
#include <vector>
#include <sstream>

using namespace Amplitron;

TEST_F(EffectsTest, effect_enabled_default_true) {
    NoiseGate ng;
    ASSERT_TRUE(ng.is_enabled());
}

TEST_F(EffectsTest, effect_enable_disable) {
    NoiseGate ng;
    ng.set_enabled(false);
    ASSERT_FALSE(ng.is_enabled());
    ng.set_enabled(true);
    ASSERT_TRUE(ng.is_enabled());
}

TEST_F(EffectsTest, effect_mix_clamped) {
    Overdrive od;
    od.set_mix(-0.5f);
    ASSERT_NEAR(od.get_mix(), 0.0f, 1e-6f);
    od.set_mix(1.5f);
    ASSERT_NEAR(od.get_mix(), 1.0f, 1e-6f);
    od.set_mix(0.5f);
    ASSERT_NEAR(od.get_mix(), 0.5f, 1e-6f);
}

TEST_F(EffectsTest, effect_has_name) {
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

TEST_F(EffectsTest, effect_has_params) {
    Overdrive od;
    ASSERT_GT((int)od.params().size(), 0);

    Equalizer eq;
    ASSERT_GT((int)eq.params().size(), 0);

    Compressor comp;
    ASSERT_GT((int)comp.params().size(), 0);
}

TEST_F(EffectsTest, effect_params_have_valid_ranges) {
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

TEST_F(EffectsTest, effect_set_sample_rate) {
    Reverb rv;
    rv.set_sample_rate(44100);
    rv.reset();
    
    float buf[128];
    for (int i = 0; i < 128; ++i) {
        buf[i] = std::sin(2.0f * 3.14159265f * 440.0f * i / 44100.0f);
    }
    rv.process(buf, 128);
    ASSERT_TRUE(is_finite(buf, 128));
}

static float tuner_detect_freq(float target_freq, int sample_rate = 48000) {
    TunerPedal tuner;
    tuner.set_sample_rate(sample_rate);
    tuner.reset();
    tuner.params()[0].value = 0.0f;

    const int total_samples = sample_rate;
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

TEST_F(EffectsTest, tuner_detects_E2) {
    float freq = tuner_detect_freq(82.41f);
    ASSERT_GT(freq, 0.0f);
    float cents_err = std::fabs(1200.0f * std::log2(freq / 82.41f));
    ASSERT_LT(cents_err, 2.0f);
}

TEST_F(EffectsTest, tuner_detects_A2) {
    float freq = tuner_detect_freq(110.0f);
    ASSERT_GT(freq, 0.0f);
    float cents_err = std::fabs(1200.0f * std::log2(freq / 110.0f));
    ASSERT_LT(cents_err, 2.0f);
}

TEST_F(EffectsTest, tuner_detects_D3) {
    float freq = tuner_detect_freq(146.83f);
    ASSERT_GT(freq, 0.0f);
    float cents_err = std::fabs(1200.0f * std::log2(freq / 146.83f));
    ASSERT_LT(cents_err, 2.0f);
}

TEST_F(EffectsTest, tuner_detects_G3) {
    float freq = tuner_detect_freq(196.0f);
    ASSERT_GT(freq, 0.0f);
    float cents_err = std::fabs(1200.0f * std::log2(freq / 196.0f));
    ASSERT_LT(cents_err, 2.0f);
}

TEST_F(EffectsTest, tuner_detects_B3) {
    float freq = tuner_detect_freq(246.94f);
    ASSERT_GT(freq, 0.0f);
    float cents_err = std::fabs(1200.0f * std::log2(freq / 246.94f));
    ASSERT_LT(cents_err, 2.0f);
}

TEST_F(EffectsTest, tuner_detects_E4) {
    float freq = tuner_detect_freq(329.63f);
    ASSERT_GT(freq, 0.0f);
    float cents_err = std::fabs(1200.0f * std::log2(freq / 329.63f));
    ASSERT_LT(cents_err, 2.0f);
}

TEST_F(EffectsTest, tuner_note_names_correct) {
    ASSERT_TRUE(std::strcmp(TunerPedal::note_name(0), "C") == 0);
    ASSERT_TRUE(std::strcmp(TunerPedal::note_name(4), "E") == 0);
    ASSERT_TRUE(std::strcmp(TunerPedal::note_name(9), "A") == 0);
    ASSERT_TRUE(std::strcmp(TunerPedal::note_name(11), "B") == 0);
    ASSERT_TRUE(std::strcmp(TunerPedal::note_name(-1), "?") == 0);
}

TEST_F(EffectsTest, tuner_maps_note_and_octave) {
    TunerPedal tuner;
    tuner.set_sample_rate(SR);
    tuner.reset();

    const int total = SR;
    const int chunk = 256;
    float buf[256];
    for (int off = 0; off < total; off += chunk) {
        for (int i = 0; i < chunk; ++i)
            buf[i] = 0.8f * std::sin(2.0f * 3.14159265f * 440.0f * (off + i) / SR);
        tuner.process(buf, chunk);
    }

    ASSERT_TRUE(tuner.signal_detected.load());
    ASSERT_EQ(tuner.detected_note.load(), 9);   // A
    ASSERT_EQ(tuner.detected_octave.load(), 4);  // octave 4
    ASSERT_LT(std::fabs(tuner.detected_cents.load()), 2.0f);
}

TEST_F(EffectsTest, tuner_mute_zeroes_output) {
    TunerPedal tuner;
    tuner.set_sample_rate(SR);
    tuner.reset();
    tuner.params()[0].value = 1.0f;

    fill_sine(440.0f);
    tuner.process(input_buffer, 256);

    float out_rms = rms(input_buffer, 256);
    ASSERT_LT(out_rms, 1e-10f);
}

TEST_F(EffectsTest, tuner_pass_through_when_unmuted) {
    TunerPedal tuner;
    tuner.set_sample_rate(SR);
    tuner.reset();
    tuner.params()[0].value = 0.0f;

    fill_sine(440.0f);
    float in_rms = rms(input_buffer, 256);
    tuner.process(input_buffer, 256);
    float out_rms = rms(input_buffer, 256);

    ASSERT_NEAR(out_rms, in_rms, 1e-6f);
}

TEST_F(EffectsTest, tuner_no_detection_on_silence) {
    TunerPedal tuner;
    tuner.set_sample_rate(SR);
    tuner.reset();

    float buf[256];
    std::memset(buf, 0, sizeof(buf));
    for (int i = 0; i < 200; ++i)
        tuner.process(buf, 256);

    ASSERT_FALSE(tuner.signal_detected.load());
}

TEST_F(EffectsTest, all_effects_handle_silence) {
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
        fx->set_sample_rate(SR);
        fx->reset();
        std::memset(input_buffer, 0, sizeof(input_buffer));
        fx->process(input_buffer, 256);
        ASSERT_TRUE(is_finite(input_buffer, 256));
    }
}

TEST_F(EffectsTest, all_effects_reset_without_crash) {
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
        fx->set_sample_rate(SR);
        fill_sine(440.0f);
        fx->process(input_buffer, 128);
        fx->reset();
        fill_sine(440.0f);
        fx->process(input_buffer, 128);
        ASSERT_TRUE(is_finite(input_buffer, 128));
    }
}

TEST_F(EffectsTest, all_effects_handle_different_sample_rates) {
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
            for (int i = 0; i < 256; ++i) {
                buf[i] = std::sin(2.0f * 3.14159265f * 440.0f * i / rate);
            }
            fx->process(buf, 256);
            ASSERT_TRUE(is_finite(buf, 256));
        }
    }
}

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
