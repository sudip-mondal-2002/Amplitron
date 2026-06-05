#pragma once

#include <atomic>
#include <mutex>
#include <vector>

namespace Amplitron {

class TempoEngine {
 public:
  TempoEngine();
  ~TempoEngine();

  // Call from audio thread: Write input samples to the 4-second ring buffer.
  void write_input(const float* input, int num_samples);

  // Call from UI thread: Re-allocate buffers based on sample rate.
  void set_sample_rate(int sample_rate);

  // Call from UI thread: Analyze buffer and detect BPM. Returns -1.0f on
  // failure.
  float detect_bpm();

 private:
  int sample_rate_ = 48000;
  std::vector<float> buffer_;
  size_t write_pos_ = 0;
  std::atomic<size_t> total_samples_written_{0};

  // Preallocated/cached variables for detect_bpm()
  std::vector<float> local_audio_;
  std::vector<float> prev_mag_;
  std::vector<float> hann_window_;
};

}  // namespace Amplitron
