#include "audio_metrics_service.h"

namespace Amplitron {

void AudioMetricsService::update(IAudioEngine& engine, float dt) {
    // 1. Update Level Analyzer
    const float input_rms       = engine.get_input_rms();
    const float output_rms      = engine.get_output_rms();
    const bool  input_clipped   = engine.consume_input_clipped();
    const bool  output_clipped  = engine.consume_output_clipped();
    level_analyzer_.update(input_rms, output_rms, input_clipped, output_clipped, dt);

    // 2. Update Spectrum Analyzer
    const uint64_t seq = engine.get_analyzer_sequence();
    if (seq == analyzer_last_sequence_ && seq != 0) {
        spectrum_analyzer_.update(analyzer_input_buf_.data(),
                                  analyzer_output_buf_.data(),
                                  engine.get_sample_rate(),
                                  dt);
        return;
    }

    if (engine.copy_analyzer_snapshot(analyzer_input_buf_.data(),
                                      analyzer_output_buf_.data(),
                                      IAudioEngine::ANALYZER_FFT_SIZE)) {
        spectrum_analyzer_.update(analyzer_input_buf_.data(),
                                  analyzer_output_buf_.data(),
                                  engine.get_sample_rate(),
                                  dt);
        analyzer_last_sequence_ = seq;
    }
}

} // namespace Amplitron
