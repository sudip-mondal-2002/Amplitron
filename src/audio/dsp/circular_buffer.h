#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace Amplitron {

/**
 * Fixed-size circular buffer for delay lines.
 * Used by Delay, Chorus, and Reverb.
 */
class CircularBuffer {
   public:
    CircularBuffer() = default;

    void resize(int size) {
        if (size <= 0) size = 1;
        buffer_.assign(size, 0.0f);
        write_pos_ = 0;
        size_ = size;
    }

    // Returns a valid index in [0, size_-1] for any signed input.
    int normalize_index(int idx) const { return ((idx % size_) + size_) % size_; }

    void write(float sample) {
        buffer_[write_pos_] = sample;
        write_pos_ = normalize_index(write_pos_ + 1);
    }

    float read(int delay) const { return buffer_[normalize_index(write_pos_ - delay - 1)]; }

    float read_at(int index) const { return buffer_[normalize_index(index)]; }

    void write_at(int index, float sample) { buffer_[normalize_index(index)] = sample; }

    float read_linear(float delay) const {
        float read_pos_f = static_cast<float>(write_pos_) - delay - 1.0f;
        int pos_int = static_cast<int>(std::floor(read_pos_f));
        int pos0 = normalize_index(pos_int);
        int pos1 = normalize_index(pos_int + 1);
        float frac = read_pos_f - static_cast<float>(pos_int);
        return buffer_[pos0] * (1.0f - frac) + buffer_[pos1] * frac;
    }

    int write_pos() const { return write_pos_; }
    int size() const { return size_; }

    void reset() {
        std::fill(buffer_.begin(), buffer_.end(), 0.0f);
        write_pos_ = 0;
    }

   private:
    std::vector<float> buffer_;
    int write_pos_ = 0;
    int size_ = 0;
};

}  // namespace Amplitron
