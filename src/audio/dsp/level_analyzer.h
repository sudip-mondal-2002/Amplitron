#pragma once

namespace Amplitron {

/**
 * @brief Utility class to analyze VU levels, RMS smoothing, peak holds, and clip indicators.
 * Moves level computation and smoothing math outside of the UI thread.
 */
class LevelAnalyzer {
public:
    LevelAnalyzer() = default;

    /**
     * @brief Update analyzer values with current block RMS and clipping info.
     * @param input_rms Current input RMS level.
     * @param output_rms Current output RMS level.
     * @param input_clipped True if clipping was detected in the input.
     * @param output_clipped True if clipping was detected in the output.
     * @param dt Time delta since last update.
     */
    void update(float input_rms, float output_rms, bool input_clipped, bool output_clipped, float dt);

    float smoothed_input_rms() const { return smoothed_input_rms_; }
    float smoothed_output_rms() const { return smoothed_output_rms_; }
    float input_peak_hold() const { return input_peak_hold_; }
    float output_peak_hold() const { return output_peak_hold_; }
    float input_clip_flash() const { return input_clip_flash_; }
    float output_clip_flash() const { return output_clip_flash_; }

private:
    float smoothed_input_rms_ = 0.0f;
    float smoothed_output_rms_ = 0.0f;
    float input_peak_hold_ = 0.0f;
    float output_peak_hold_ = 0.0f;
    float input_clip_flash_ = 0.0f;
    float output_clip_flash_ = 0.0f;
};

} // namespace Amplitron
