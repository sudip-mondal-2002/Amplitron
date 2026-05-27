/**
 * @file test_spectrum_analyzer.cpp
 * @brief Headless-safe tests for SpectrumAnalyzer DSP logic.
 *
 * Only update() (pure DSP) and draw(nullptr, ...) (early-return guard) are
 * tested — no ImGui draw context is required.
 */
#include "test_framework.h"
#include "gui/spectrum_analyzer.h"
#include <cmath>
#include <vector>

using namespace Amplitron;

TEST(spectrum_analyzer_construction_does_not_crash) {
    SpectrumAnalyzer sa;
    (void)sa;
}

TEST(spectrum_analyzer_update_silence_is_stable) {
    SpectrumAnalyzer sa;
    std::vector<float> zeros(SpectrumAnalyzer::FFT_SIZE, 0.0f);
    // Many frames of silence must remain stable (no NaN / Inf)
    for (int frame = 0; frame < 10; ++frame) {
        sa.update(zeros.data(), zeros.data(), 48000, 1.0f / 60.0f);
    }
}

TEST(spectrum_analyzer_update_sine_tone_does_not_produce_nan) {
    SpectrumAnalyzer sa;
    std::vector<float> tone(SpectrumAnalyzer::FFT_SIZE);
    for (int i = 0; i < SpectrumAnalyzer::FFT_SIZE; ++i) {
        tone[i] = 0.5f * std::sin(2.0f * 3.14159265f * 1000.0f * i / 48000.0f);
    }
    sa.update(tone.data(), tone.data(), 48000, 1.0f / 60.0f);
    // draw with nullptr must be a silent no-op — not crash
    sa.draw(nullptr, ImVec2(0, 0), ImVec2(800, 200), SpectrumAnalyzer::DisplayMode::Input);
}

TEST(spectrum_analyzer_update_null_input_is_safe) {
    SpectrumAnalyzer sa;
    std::vector<float> zeros(SpectrumAnalyzer::FFT_SIZE, 0.0f);
    sa.update(nullptr,       zeros.data(), 48000, 1.0f / 60.0f);
    sa.update(zeros.data(),  nullptr,      48000, 1.0f / 60.0f);
    sa.update(nullptr,       nullptr,      48000, 1.0f / 60.0f);
}

TEST(spectrum_analyzer_update_zero_sample_rate_is_safe) {
    SpectrumAnalyzer sa;
    std::vector<float> samples(SpectrumAnalyzer::FFT_SIZE, 0.1f);
    sa.update(samples.data(), samples.data(), 0,  1.0f / 60.0f);
    sa.update(samples.data(), samples.data(), -1, 1.0f / 60.0f);
}

TEST(spectrum_analyzer_draw_null_drawlist_all_modes) {
    SpectrumAnalyzer sa;
    sa.draw(nullptr, ImVec2(0,0), ImVec2(400,100), SpectrumAnalyzer::DisplayMode::Input);
    sa.draw(nullptr, ImVec2(0,0), ImVec2(400,100), SpectrumAnalyzer::DisplayMode::Output);
    sa.draw(nullptr, ImVec2(0,0), ImVec2(400,100), SpectrumAnalyzer::DisplayMode::Overlay);
}

TEST(spectrum_analyzer_draw_degenerate_size_is_safe) {
    SpectrumAnalyzer sa;
    // size.x <= 2 or size.y <= 2 triggers early return
    sa.draw(nullptr, ImVec2(10,10), ImVec2(1,  1  ), SpectrumAnalyzer::DisplayMode::Output);
    sa.draw(nullptr, ImVec2(10,10), ImVec2(0,  200), SpectrumAnalyzer::DisplayMode::Output);
    sa.draw(nullptr, ImVec2(10,10), ImVec2(200,0  ), SpectrumAnalyzer::DisplayMode::Output);
}

TEST(spectrum_analyzer_update_high_frequency_tone) {
    SpectrumAnalyzer sa;
    std::vector<float> tone(SpectrumAnalyzer::FFT_SIZE);
    // 10 kHz tone near Nyquist
    for (int i = 0; i < SpectrumAnalyzer::FFT_SIZE; ++i) {
        tone[i] = 0.3f * std::sin(2.0f * 3.14159265f * 10000.0f * i / 48000.0f);
    }
    sa.update(tone.data(), tone.data(), 48000, 1.0f / 60.0f);
}

TEST(spectrum_analyzer_update_full_scale_clipping_safe) {
    SpectrumAnalyzer sa;
    std::vector<float> clip(SpectrumAnalyzer::FFT_SIZE, 2.0f);  // over-range
    sa.update(clip.data(), clip.data(), 44100, 1.0f / 60.0f);
}

TEST(spectrum_analyzer_different_sample_rates) {
    SpectrumAnalyzer sa;
    std::vector<float> samples(SpectrumAnalyzer::FFT_SIZE, 0.1f);
    sa.update(samples.data(), samples.data(), 44100, 1.0f / 60.0f);
    sa.update(samples.data(), samples.data(), 48000, 1.0f / 60.0f);
    sa.update(samples.data(), samples.data(), 96000, 1.0f / 60.0f);
}
