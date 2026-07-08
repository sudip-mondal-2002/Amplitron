#include <algorithm>

#include "audio/engine/audio_engine.h"
#include "audio/engine/audio_metrics_service.h"
#include "test_framework.h"

using namespace Amplitron;
using namespace TestFramework;

class MockAudioEngineForMetrics : public AudioEngine {
   public:
    MockAudioEngineForMetrics() : AudioEngine(nullptr, nullptr) {}

    float get_input_rms() const override { return input_rms_; }
    float get_output_rms() const override { return output_rms_; }
    bool consume_input_clipped() override { return input_clipped_; }
    bool consume_output_clipped() override { return output_clipped_; }
    uint64_t get_analyzer_sequence() const override { return seq_; }
    int get_sample_rate() const override { return sample_rate_; }
    bool copy_analyzer_snapshot(float* input_dest, float* output_dest,
                                int sample_count) const override {
        if (copy_snapshot_success_) {
            std::fill(input_dest, input_dest + sample_count, 1.0f);
            std::fill(output_dest, output_dest + sample_count, 2.0f);
            return true;
        }
        return false;
    }

    float input_rms_ = 0.5f;
    float output_rms_ = 0.6f;
    bool input_clipped_ = false;
    bool output_clipped_ = false;
    uint64_t seq_ = 0;
    int sample_rate_ = 44100;
    bool copy_snapshot_success_ = true;
};

TEST(AudioMetricsService_Update) {
    MockAudioEngineForMetrics engine;
    AudioMetricsService service;

    // 1. Initial update with seq = 0 (hits copy_analyzer_snapshot success)
    engine.seq_ = 0;
    engine.copy_snapshot_success_ = true;
    for (int i = 0; i < 100; ++i) {
        service.update(engine, 0.1f);
    }
    ASSERT_NEAR(service.level_analyzer().smoothed_input_rms(), 0.5f, 0.01f);

    // 2. Update with seq = 0, but copy fails
    engine.copy_snapshot_success_ = false;
    service.update(engine, 0.1f);

    // 3. Update with new non-zero seq = 5 (hits copy_analyzer_snapshot success)
    engine.seq_ = 5;
    engine.copy_snapshot_success_ = true;
    service.update(engine, 0.1f);

    // 4. Update with same non-zero seq = 5 (hits early return path)
    service.update(engine, 0.1f);
}
