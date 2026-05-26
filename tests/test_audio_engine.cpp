#include "test_framework.h"
#include "audio/audio_engine.h"
#include "audio/effects/distortion.h"
#include "audio/effects/overdrive.h"
#include <vector>
#include <memory>
#include <filesystem>
#include <chrono>
#include <cmath>

using namespace Amplitron;

// ---------------------------------------------------------
// audio_engine_process.cpp & audio_engine_api.cpp Tests
// ---------------------------------------------------------

TEST(AudioEngineProcess_ProcessSilenceGivesZeroOutput) {
    AudioEngine engine;
    engine.initialize();
    engine.set_buffer_size(64);
    std::vector<float> in(64, 0.0f), out(128, 0.0f);
    engine.process_audio(in.data(), out.data(), 64);
    for (auto s : out) ASSERT_NEAR(s, 0.0f, 1e-6f);
}

TEST(AudioEngineProcess_InputGainScalesOutput) {
    AudioEngine engine;
    engine.initialize();
    engine.set_buffer_size(64);
    engine.set_input_gain(0.5f);
    engine.set_output_gain(1.0f);
    std::vector<float> in(64, 1.0f), out(128, 0.0f);
    engine.process_audio(in.data(), out.data(), 64);
    ASSERT_NEAR(out[0], 0.5f, 0.01f);
}

TEST(AudioEngineProcess_OutputGainScalesOutput) {
    AudioEngine engine;
    engine.initialize();
    engine.set_buffer_size(64);
    // Input gain defaults to 1.0f. Output defaults to 0.8f? Wait, let's explicitly set it.
    engine.set_input_gain(1.0f);
    engine.set_output_gain(0.25f);
    std::vector<float> in(64, 1.0f), out(128, 0.0f);
    engine.process_audio(in.data(), out.data(), 64);
    ASSERT_NEAR(out[0], 0.25f, 0.01f);
    ASSERT_NEAR(out[1], 0.25f, 0.01f);
}

TEST(AudioEngineProcess_OutputIsClampedToSafetyLimit) {
    AudioEngine engine;
    engine.initialize();
    engine.set_buffer_size(64);
    engine.set_input_gain(10.0f); // Massive gain to exceed +/- 1.0
    engine.set_output_gain(1.0f);
    std::vector<float> in(64, 1.0f), out(128, 0.0f);
    engine.process_audio(in.data(), out.data(), 64);
    // Should be clamped to 1.0
    ASSERT_NEAR(out[0], 1.0f, 1e-6f);
    
    std::vector<float> in_neg(64, -1.0f);
    engine.process_audio(in_neg.data(), out.data(), 64);
    // Should be clamped to -1.0
    ASSERT_NEAR(out[0], -1.0f, 1e-6f);
}

TEST(AudioEngineProcess_RMSCalculationSilenceVsTone) {
    AudioEngine engine;
    engine.initialize();
    engine.set_buffer_size(64);
    engine.set_analyzer_enabled(true);
    
    // Silence
    std::vector<float> in_silence(64, 0.0f), out(128, 0.0f);
    engine.process_audio(in_silence.data(), out.data(), 64);
    ASSERT_NEAR(engine.get_input_rms(), 0.0f, 1e-6f);
    ASSERT_NEAR(engine.get_output_rms(), 0.0f, 1e-6f);

    // DC Tone (1.0)
    std::vector<float> in_tone(64, 1.0f);
    engine.process_audio(in_tone.data(), out.data(), 64);
    
    // RMS is smoothed, so it won't be exactly 1.0 immediately, but should be > 0
    ASSERT_TRUE(engine.get_input_rms() > 0.0f);
    ASSERT_TRUE(engine.get_output_rms() > 0.0f);
}

// ---------------------------------------------------------
// audio_engine_chain.cpp Tests
// ---------------------------------------------------------

TEST(AudioEngineChain_AddAndRemoveEffect) {
    AudioEngine engine;
    ASSERT_EQ(engine.effects().size(), 0u);
    
    auto dist = std::make_shared<Distortion>();
    engine.add_effect(dist);
    ASSERT_EQ(engine.effects().size(), 1u);

    engine.remove_effect(0);
    ASSERT_EQ(engine.effects().size(), 0u);
}

TEST(AudioEngineChain_InsertEffect) {
    AudioEngine engine;
    auto dist = std::make_shared<Distortion>();
    auto od = std::make_shared<Overdrive>();
    
    engine.add_effect(dist);
    engine.insert_effect(0, od); // Insert Overdrive at the beginning
    
    ASSERT_EQ(engine.effects().size(), 2u);
    ASSERT_EQ(engine.effects()[0], od);
    ASSERT_EQ(engine.effects()[1], dist);
}

TEST(AudioEngineChain_ClearEffects) {
    AudioEngine engine;
    engine.add_effect(std::make_shared<Distortion>());
    engine.add_effect(std::make_shared<Overdrive>());
    ASSERT_EQ(engine.effects().size(), 2u);
    
    engine.clear_effects();
    ASSERT_EQ(engine.effects().size(), 0u);
}

TEST(AudioEngineChain_MoveEffect) {
    AudioEngine engine;
    auto dist = std::make_shared<Distortion>();
    auto od = std::make_shared<Overdrive>();
    
    engine.add_effect(dist);
    engine.add_effect(od);
    
    ASSERT_EQ(engine.effects()[0], dist);
    ASSERT_EQ(engine.effects()[1], od);
    
    engine.move_effect(0, 1);
    
    ASSERT_EQ(engine.effects()[0], od);
    ASSERT_EQ(engine.effects()[1], dist);
    
    // Invalid moves and self-moves to hit branch coverage
    auto snap_count = engine.effects().size();
    auto snap_0 = engine.effects()[0];
    auto snap_1 = engine.effects()[1];

    engine.move_effect(-1, 0);
    ASSERT_EQ(engine.effects().size(), snap_count);
    ASSERT_EQ(engine.effects()[0], snap_0);
    ASSERT_EQ(engine.effects()[1], snap_1);

    engine.move_effect(0, 5);
    ASSERT_EQ(engine.effects().size(), snap_count);
    ASSERT_EQ(engine.effects()[0], snap_0);
    ASSERT_EQ(engine.effects()[1], snap_1);

    engine.move_effect(0, 0); // Self-move
    ASSERT_EQ(engine.effects().size(), snap_count);
    ASSERT_EQ(engine.effects()[0], snap_0);
    ASSERT_EQ(engine.effects()[1], snap_1);
}

TEST(AudioEngineApi_MetronomeState) {
    AudioEngine engine;
    ASSERT_EQ(engine.get_metronome_enabled(), false);
    engine.toggle_metronome();
    ASSERT_EQ(engine.get_metronome_enabled(), true);
    
    engine.set_metronome_bpm(150);
    ASSERT_EQ(engine.get_metronome_bpm(), 150);
    
    // Bounds check
    engine.set_metronome_bpm(10); // min 40
    ASSERT_EQ(engine.get_metronome_bpm(), 40);
    
    engine.set_metronome_volume(0.8f);
    ASSERT_NEAR(engine.get_metronome_volume(), 0.8f, 1e-6f);
    
    // Process audio to cover the metronome click generation in audio_engine_process.cpp
    engine.initialize();
    engine.set_buffer_size(64);
    std::vector<float> in(64, 0.0f), out(128, 0.0f);
    
    // Compute deterministic number of process_audio calls
    double sampleRate = 48000.0;
    double bpm = 40.0; // The bounded BPM from above
    int samplesPerClick = static_cast<int>(sampleRate * 60.0 / bpm);
    int bufferSize = 64;
    int processCalls = (samplesPerClick / bufferSize) + 1;

    bool clickDetected = false;
    for(int i = 0; i < processCalls; ++i) {
        std::fill(out.begin(), out.end(), 0.0f);
        engine.process_audio(in.data(), out.data(), 64);
        for (float s : out) {
            if (std::abs(s) > 1e-6f) {
                clickDetected = true;
                break;
            }
        }
    }
    ASSERT_TRUE(clickDetected);
}

TEST(AudioEngineApi_SuggestedBufferSize) {
    AudioEngine engine;
    engine.set_buffer_size(512); // load is 0, so it should suggest half
    int suggested = engine.get_suggested_buffer_size();
    ASSERT_EQ(suggested, 256);
    
    // We cannot easily set cpu_load_ directly without friend access or processing massive audio,
    // but the branch logic is covered for the low-load case.
}

TEST(AudioEngineApi_CopyAnalyzerSnapshot) {
    AudioEngine engine;
    engine.initialize();
    engine.set_buffer_size(1024);
    engine.set_analyzer_enabled(true);
    std::vector<float> in(1024, 0.5f), out(2048, 0.0f);
    // Process twice to fill the 2048-sample analyzer ring buffer completely
    engine.process_audio(in.data(), out.data(), 1024);
    engine.process_audio(in.data(), out.data(), 1024);
    
    std::vector<float> snap_in(2048, 0.0f), snap_out(2048, 0.0f);
    bool success = engine.copy_analyzer_snapshot(snap_in.data(), snap_out.data(), 1024);
    ASSERT_EQ(success, true);
    ASSERT_NEAR(snap_in[0], 0.5f, 0.01f);
    
    // Test analyzer copy error conditions for branch coverage
    ASSERT_EQ(engine.copy_analyzer_snapshot(nullptr, snap_out.data(), 1024), false);
    ASSERT_EQ(engine.copy_analyzer_snapshot(snap_in.data(), nullptr, 1024), false);
    ASSERT_EQ(engine.copy_analyzer_snapshot(snap_in.data(), snap_out.data(), -1), false);
    
    AudioEngine new_engine;
    ASSERT_EQ(new_engine.copy_analyzer_snapshot(snap_in.data(), snap_out.data(), 1024), false); // seq == 0
}

class TestTunerEffect : public Effect {
public:
    bool processed = false;
    TestTunerEffect() : Effect() {}
    void process(float* /*buffer*/, int /*num_samples*/) override {
        processed = true;
    }
    void reset() override {}
    const char* name() const override { return "TestTuner"; }
    std::vector<EffectParam>& params() override { static std::vector<EffectParam> p; return p; }
};

TEST(AudioEngineChain_TunerTap) {
    AudioEngine engine;
    ASSERT_EQ(engine.has_tuner_tap(), false);
    
    auto tap = std::make_shared<TestTunerEffect>();
    engine.set_tuner_tap(tap);
    ASSERT_EQ(engine.has_tuner_tap(), true);
    
    // Process audio to cover the tuner tap execution branch in audio_engine_process.cpp
    std::vector<float> in(64, 0.5f), out(128, 0.0f);
    engine.process_audio(in.data(), out.data(), 64);
    
    ASSERT_TRUE(tap->processed);
    
    engine.clear_tuner_tap();
    ASSERT_EQ(engine.has_tuner_tap(), false);
}

TEST(AudioEngineChain_RestoreEffectsState) {
    AudioEngine engine;
    auto dist = std::make_shared<Distortion>();
    auto od = std::make_shared<Overdrive>();
    
    std::vector<std::shared_ptr<Effect>> state = {dist, od};
    engine.restore_effects_state(state);
    
    ASSERT_EQ(engine.effects().size(), 2u);
    ASSERT_EQ(engine.effects()[0], dist);
}

TEST(AudioEngineApi_CommandQueuePushes) {
    AudioEngine engine;
    engine.initialize();
    engine.set_buffer_size(64);
    
    auto dist = std::make_shared<Distortion>();
    engine.add_effect(dist);
    
    // Interleave gain commands and effect commands to trigger both drain loops
    engine.set_input_gain(1.0f); 
    engine.push_effect_enabled(0, 0.0f); // disable
    engine.push_effect_mix(0, 0.25f);
    engine.set_input_gain(0.5f); // this gets stuck behind effect commands and handled by drain_commands
    engine.push_param_change(0, 0, 0.8f); // Param 0 is probably Gain or Drive
    
    // Process audio to drain commands
    std::vector<float> in(64, 0.0f), out(128, 0.0f);
    engine.process_audio(in.data(), out.data(), 64);
    
    // Check if dist state changed
    ASSERT_EQ(dist->is_enabled(), false);
    ASSERT_NEAR(dist->get_mix(), 0.25f, 1e-6f);
    ASSERT_NEAR(dist->params()[0].value, 0.8f, 1e-6f);
    ASSERT_NEAR(engine.get_input_gain(), 0.5f, 1e-6f);
}

TEST(AudioEngineProcess_RecorderBranch) {
    AudioEngine engine;
    engine.initialize();
    engine.set_buffer_size(64);
    
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::string temp_file = (std::filesystem::temp_directory_path() / ("test_record_" + std::to_string(now) + ".wav")).string();
    
    // Start recorder
    engine.recorder().start(temp_file, 48000, 1);
    ASSERT_EQ(engine.recorder().is_recording(), true);
    
    std::vector<float> in(64, 0.5f), out(128, 0.0f);
    // Process audio to trigger the recorder_.write_samples() branch
    engine.process_audio(in.data(), out.data(), 64);
    
    // Check if samples were passed into the recorder
    engine.recorder().stop();
    ASSERT_EQ(engine.recorder().is_recording(), false);
    
    ASSERT_TRUE(std::filesystem::exists(temp_file));
    ASSERT_GT(std::filesystem::file_size(temp_file), 0u);
    std::filesystem::remove(temp_file);
}

TEST(AudioEngineProcess_FullCoverageSweep) {
    AudioEngine engine;
    engine.initialize();
    engine.set_buffer_size(64);
    
    // 1. Zero/Negative sample rate
    engine.set_sample_rate(0);
    std::vector<float> in(64, 1.5f), out(128, 0.0f);
    engine.process_audio(in.data(), out.data(), 64);
    
    // 2. Restore sample rate, turn on analyzer, trigger clipping
    engine.set_sample_rate(48000);
    engine.set_analyzer_enabled(true);
    engine.set_input_gain(2.0f); // Make it clip input
    engine.set_output_gain(2.0f); // Make it clip output
    
    // 3. Process to hit clipping
    engine.process_audio(in.data(), out.data(), 64);
    ASSERT_TRUE(engine.consume_input_clipped());
    ASSERT_TRUE(engine.consume_output_clipped());
    
    // 4. Invalid effect index in command queue
    engine.push_effect_enabled(999, 1.0f);
    engine.push_effect_mix(999, 0.5f);
    engine.push_param_change(999, 0, 1.0f);
    engine.process_audio(in.data(), out.data(), 64);

    // 5. Test metronome with invalid counter logic coverage
    engine.toggle_metronome();
    engine.set_metronome_bpm(200); // Trigger BPM change
    engine.set_metronome_volume(0.1f);
    engine.process_audio(in.data(), out.data(), 64);

    // 6. Test metronome disable mid-click
    engine.toggle_metronome();
    engine.process_audio(in.data(), out.data(), 64);
}
