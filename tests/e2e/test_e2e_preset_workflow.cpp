#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

#include "audio/effects/delay_reverb/delay.h"
#include "audio/effects/distortion/distortion.h"
#include "audio/effects/dynamics/noise_gate.h"
#include "audio/engine/audio_engine.h"
#include "preset_manager.h"
#include "test_fixtures.h"
#include "test_framework.h"

using namespace Amplitron;

TEST_F(PresetTest, e2e_preset_workflow_roundtrip_and_processing) {
    // 1. Setup the save path under presets/
    std::string path = "presets/test_e2e_preset_workflow.json";
    register_temp_file(path);

    // 2. Configure a standard signal chain on the engine
    auto ng = std::make_shared<NoiseGate>();
    ng->set_enabled(true);
    if (!ng->params().empty()) {
        ng->params()[0].value = -30.0f;  // Set custom threshold within [-80.0f, 0.0f]
    }
    engine.add_effect(ng);

    auto dist = std::make_shared<Distortion>();
    dist->set_enabled(true);
    if (!dist->params().empty()) {
        dist->params()[0].value = 8.5f;  // Set custom parameter within [1.0f, 20.0f]
    }
    engine.add_effect(dist);

    auto delay = std::make_shared<Delay>();
    delay->set_enabled(true);
    delay->set_mix(0.4f);
    engine.add_effect(delay);

    // Set input and output gains
    engine.set_input_gain(0.75f);
    engine.set_output_gain(0.85f);

    // 3. Save the preset
    bool saved = manager.save_preset(path, "E2E Preset", "E2E Complete Signal Chain Test", engine);
    ASSERT_TRUE(saved);
    ASSERT_TRUE(std::filesystem::exists(path));

    // 4. Clear the engine completely to prepare for loading
    engine.clear_effects();
    engine.set_input_gain(1.0f);
    engine.set_output_gain(1.0f);
    engine.commit_graph_changes();

    // 5. Reload the preset
    bool loaded = manager.load_preset(path, engine);
    ASSERT_TRUE(loaded);

    // Propagate the active sample rate to all newly deserialized nodes in the graph
    engine.set_sample_rate(48000);

    // 6. Verify the loaded engine matches the original configuration exactly
    ASSERT_NEAR(engine.get_input_gain(), 0.75f, 0.01f);
    ASSERT_NEAR(engine.get_output_gain(), 0.85f, 0.01f);

    std::vector<std::shared_ptr<Effect>> loaded_effects;
    for (const auto& node : engine.graph().get_nodes()) {
        if (node.routing_type == NodeRoutingType::StandardEffect && node.pedal != nullptr) {
            loaded_effects.push_back(node.pedal);
        }
    }
    ASSERT_EQ(loaded_effects.size(), 3u);
    ASSERT_EQ(std::string(loaded_effects[0]->name()), "Noise Gate");
    ASSERT_EQ(std::string(loaded_effects[1]->name()), "Distortion");
    ASSERT_EQ(std::string(loaded_effects[2]->name()), "Delay");

    ASSERT_TRUE(loaded_effects[0]->is_enabled());
    if (!loaded_effects[0]->params().empty()) {
        ASSERT_NEAR(loaded_effects[0]->params()[0].value, -30.0f, 0.01f);
    }

    ASSERT_TRUE(loaded_effects[1]->is_enabled());
    if (!loaded_effects[1]->params().empty()) {
        ASSERT_NEAR(loaded_effects[1]->params()[0].value, 8.5f, 0.01f);
    }

    ASSERT_TRUE(loaded_effects[2]->is_enabled());
    ASSERT_NEAR(loaded_effects[2]->get_mix(), 0.4f, 0.01f);

    // 7. Test DSP processing of a simulated input tone.
    // Use 512 frames so the NoiseGate attack envelope (default 0.5 ms = ~24
    // samples at 48 kHz) has sufficient time to fully open before we measure.
    const int num_frames = 512;
    std::vector<float> input(num_frames);
    std::vector<float> output(num_frames * 2, 0.0f);  // stereo interleaved
    for (int i = 0; i < num_frames; ++i) {
        input[i] = std::sin(2.0f * 3.14159f * 440.0f * i / 48000.0f);
    }

    engine.process_audio(input.data(), output.data(), num_frames);

    // Ensure the output contains processed data and doesn't contain NaNs
    float output_sum = 0.0f;
    for (int i = 0; i < num_frames * 2; ++i) {
        ASSERT_TRUE(std::isfinite(output[i]));
        output_sum += std::abs(output[i]);
    }
    ASSERT_TRUE(output_sum > 0.001f);
}
