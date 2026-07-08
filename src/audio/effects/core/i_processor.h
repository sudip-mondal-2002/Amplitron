#pragma once

namespace Amplitron {

class IProcessor {
   public:
    virtual ~IProcessor() = default;

    // Process a mono buffer in place.
    virtual void process(float* buffer, int num_samples) = 0;

    // Stereo processing.
    virtual void process_stereo(float* left, float* right, int num_samples) = 0;

    // Update the processing sample rate.
    virtual void set_sample_rate(int sample_rate) = 0;

    // Clear dynamic states.
    virtual void reset() = 0;
};

}  // namespace Amplitron
