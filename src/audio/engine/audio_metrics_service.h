#pragma once
#include "audio/engine/i_audio_engine.h"
#include "audio/dsp/level_analyzer.h"
#include "audio/dsp/spectrum_analyzer.h"
#include <array>
#include <cstdint>

namespace Amplitron {

class AudioMetricsService {
public:
    void update(IAudioEngine& engine, float dt);

    const LevelAnalyzer& level_analyzer() const { return level_analyzer_; }
    const SpectrumAnalyzer& spectrum_analyzer() const { return spectrum_analyzer_; }

private:
    LevelAnalyzer level_analyzer_;
    SpectrumAnalyzer spectrum_analyzer_;
    uint64_t analyzer_last_sequence_ = 0;
    std::array<float, IAudioEngine::ANALYZER_FFT_SIZE> analyzer_input_buf_{};
    std::array<float, IAudioEngine::ANALYZER_FFT_SIZE> analyzer_output_buf_{};
};

} // namespace Amplitron
