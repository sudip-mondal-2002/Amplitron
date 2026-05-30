#include "test_framework.h"
#include "audio/engine/audio_engine.h"
#include "audio/effects/noise_gate.h"
#include "audio/effects/amp_simulator.h"
#include "cli.h" 

using namespace Amplitron;

//Offline dsp test(verification of math without audio hardware)
TEST(headless_offline_dsp_gain_staging) {
    AudioEngine engine;
    engine.initialize();
    
    // Set up a headless-style empty graph
    engine.clear_effects();
    
    // Set master input and output gain to 0.5
    engine.set_input_gain(0.5f);
    engine.set_output_gain(0.5f);
    
    // Create a dummy audio buffer of 64 samples at maximum digital amplitude
    float input_buffer[64];
    for (int i = 0; i < 64; ++i) {
        input_buffer[i] = 1.0f; 
    }
    float output_buffer[128] = {0.0f}; // Stereo output (64 * 2)
    
    // Process exactly one block
    engine.process_audio(input_buffer, output_buffer, 64);
    
    // Verify the math: 1.0 (input) * 0.5 (in_gain) * 0.5 (out_gain) = 0.25
    ASSERT_NEAR(output_buffer[0], 0.25f, 1e-5f);
    
    engine.shutdown();
}

//Lock-free queue test(simulating background stdin thread)
TEST(headless_command_queue_execution) {
    AudioEngine engine;
    engine.initialize();
    
    // Setup a dummy rig with a disabled Noise Gate
    auto noise_gate = std::make_shared<NoiseGate>();
    noise_gate->set_enabled(false);
    engine.add_initial_effects({noise_gate});
    
    // Verify initial state
    ASSERT_FALSE(noise_gate->is_enabled());
    
    // Push "enable 0" command into the queue
    engine.push_effect_enabled(0, 1.0f); // 1.0f = True
    
    // Process one block of silence to trigger command draining
    float input_buffer[64] = {0.0f};
    float output_buffer[128] = {0.0f};
    engine.process_audio(input_buffer, output_buffer, 64);
    
    // Verify pedal update
    ASSERT_TRUE(noise_gate->is_enabled());
    
    engine.shutdown();
}

//CLI daemon test
TEST(headless_cli_parser_flags) {
    // Simulate command: ./Amplitron --no-gui --preset live_rig.json
    const char* argv[] = {
        "Amplitron",
        "--no-gui",
        "--preset",
        "live_rig.json"
    };
    int argc = 4;
    
    // Parse the arguments
    CliOptions opts = handle_cli_args(argc, const_cast<char**>(argv));
    
    // Assert the flags were caught correctly
    ASSERT_TRUE(opts.is_headless);
    ASSERT_EQ(opts.preset_path, std::string("live_rig.json"));
    ASSERT_FALSE(opts.exit_early);
}

TEST(headless_cli_parser_missing_preset_trap) {
    // Simulate command: ./Amplitron --no-gui
    const char* argv[] = {
        "Amplitron",
        "--no-gui"
    };
    int argc = 2;
    
    CliOptions opts = handle_cli_args(argc, const_cast<char**>(argv));
    
    ASSERT_TRUE(opts.is_headless);
    ASSERT_TRUE(opts.exit_early);
}