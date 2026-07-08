#include <memory>
#include <vector>

#include "audio/effects/core/effect.h"
#include "audio/engine/audio_command_dispatcher.h"
#include "audio/engine/audio_graph.h"
#include "audio/engine/audio_graph_executor.h"
#include "test_framework.h"

using namespace Amplitron;
using namespace TestFramework;

class MockEffectForDispatcher : public Effect {
   public:
    MockEffectForDispatcher() {
        params_.push_back({"Param1", 5.0f, 0.0f, 10.0f, 5.0f, "Hz", "Tooltip1"});
    }

    void process(float*, int) override {}
    void reset() override {}
    const char* name() const override { return "MockEffectForDispatcher"; }
    const char* type_id() const override { return "MockEffectForDispatcher"; }
    std::vector<EffectParam>& params() override { return params_; }
    const std::vector<EffectParam>& params() const override { return params_; }

   private:
    std::vector<EffectParam> params_;
};

TEST(AudioCommandDispatcher_DrainGainCommands) {
    AudioCommandDispatcher dispatcher;
    std::atomic<float> input_gain{1.0f};
    std::atomic<float> output_gain{1.0f};
    std::shared_ptr<AudioGraphExecutor> executor = nullptr;

    // Push mixer gain, input gain, output gain, and standard command
    dispatcher.push_mixer_gain_change(10, 2, 0.5f);
    dispatcher.push_input_gain(0.8f);
    dispatcher.push_output_gain(1.2f);
    dispatcher.push_effect_mix(0, 0.5f);  // Non-gain command to break the loop

    // Drain with null executor
    dispatcher.drain_gain_commands(input_gain, output_gain, executor);

    // Verify gains were popped and updated
    ASSERT_NEAR(input_gain.load(), 0.8f, 0.01f);
    ASSERT_NEAR(output_gain.load(), 1.2f, 0.01f);
}

TEST(AudioCommandDispatcher_DrainCommands) {
    AudioCommandDispatcher dispatcher;
    std::atomic<float> input_gain{1.0f};
    std::atomic<float> output_gain{1.0f};
    std::shared_ptr<AudioGraphExecutor> executor = nullptr;
    AudioGraph main_graph;

    auto fx1 = std::make_shared<MockEffectForDispatcher>();
    auto fx2 = std::make_shared<MockEffectForDispatcher>();
    std::vector<std::shared_ptr<Effect>> dummy_effects = {fx1, fx2};

    // 1. SetEffectParam
    // - Valid effect_index lookup in dummy_effects, valid param_index
    dispatcher.push_param_change(0, 0, 8.0f);
    // - Valid effect_index lookup in dummy_effects, invalid param_index (out of bounds)
    dispatcher.push_param_change(0, 5, 8.0f);
    // - Valid effect_index lookup, negative param_index
    dispatcher.push_param_change(0, -1, 8.0f);

    // 2. SetEffectEnabled
    dispatcher.push_effect_enabled(0, 1.0f);

    // 3. SetEffectMix
    dispatcher.push_effect_mix(0, 0.7f);

    // 4. SetMixerGain (should bypass/skip in drain_commands)
    dispatcher.push_mixer_gain_change(0, 0, 0.4f);

    // 5. SetInputGain & SetOutputGain
    dispatcher.push_input_gain(0.2f);
    dispatcher.push_output_gain(0.3f);

    // 6. Unknown command / failure to lookup completely (effect_index out of bounds for
    // dummy_effects)
    dispatcher.push_param_change(99, 0, 1.0f);

    dispatcher.drain_commands(input_gain, output_gain, executor, main_graph, dummy_effects);

    ASSERT_NEAR(fx1->params()[0].value, 8.0f, 0.01f);
    ASSERT_TRUE(fx1->is_enabled());
    ASSERT_NEAR(fx1->get_mix(), 0.7f, 0.01f);
    ASSERT_NEAR(input_gain.load(), 0.2f, 0.01f);
    ASSERT_NEAR(output_gain.load(), 0.3f, 0.01f);
}
