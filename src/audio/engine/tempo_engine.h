#pragma once
#include <vector>
#include <mutex>

namespace Amplitron {

class TempoEngine {
public:
    TempoEngine();
    
    void set_sample_rate(int sample_rate);
    void write_input(const float* data, int count);
    float detect_bpm();
    
private:
    std::mutex buffer_mutex_;
    std::vector<float> ring_buffer_;
    int write_pos_ = 0;
    int sample_rate_ = 48000;
};

} // namespace Amplitron
