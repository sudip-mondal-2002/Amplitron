#pragma once

#include <vector>
#include <memory>
#include <string>

// Forward-declare kiss_fft types to avoid exposing the C header
struct kiss_fft_state;
typedef struct kiss_fft_state* kiss_fft_cfg;

namespace Amplitron {

// =============================================================================
// ConvolutionKernel — immutable, prepared on the GUI thread
// =============================================================================

class ConvolutionKernel {
public:
    // Construct from time-domain IR samples.
    // block_size is the audio buffer size (e.g. 64, 128, 256).
    ConvolutionKernel(const std::vector<float>& ir_samples, int block_size);
    ~ConvolutionKernel();

    int block_size() const { return block_size_; }
    int fft_size() const { return fft_size_; }
    int num_partitions() const { return num_partitions_; }
    int ir_length() const { return ir_length_; }

    // Access the i-th partition in frequency domain.
    // Returns fft_size complex values as kiss_fft_cpx array.
    const void* partition_freq(int index) const;

    // Time-domain IR (for direct convolution fallback)
    const std::vector<float>& ir_time_domain() const { return ir_time_; }

    // Metadata
    std::string source_path;
    std::string source_name;  // filename only
    float duration_ms = 0.0f;

private:
    int block_size_;
    int fft_size_;        // = 2 * block_size (for linear convolution via overlap-add)
    int num_partitions_;
    int ir_length_;

    std::vector<float> ir_time_;  // original IR (for direct convolution fallback)

    // Frequency-domain partitions: each is fft_size kiss_fft_cpx values
    std::vector<std::vector<char>> partitions_freq_;  // raw storage

    kiss_fft_cfg fft_cfg_ = nullptr;
};

// =============================================================================
// ConvolutionEngine — mutable state for the audio thread
// =============================================================================

class ConvolutionEngine {
public:
    ConvolutionEngine();
    ~ConvolutionEngine();

    // Set the active kernel. Resets internal state if kernel changes.
    void set_kernel(const ConvolutionKernel* kernel);

    // Process a block of audio in-place.
    void process(float* buffer, int num_samples);

    // Reset all internal state.
    void reset();

    bool has_kernel() const { return kernel_ != nullptr; }

private:
    const ConvolutionKernel* kernel_ = nullptr;

    // Frequency-domain delay line (circular buffer of past input blocks in freq domain)
    std::vector<std::vector<char>> fdl_;  // raw storage for kiss_fft_cpx arrays
    int fdl_index_ = 0;

    // Overlap-add tail from previous block
    std::vector<float> overlap_;

    // FFT workspace
    kiss_fft_cfg fft_cfg_ = nullptr;   // forward
    kiss_fft_cfg ifft_cfg_ = nullptr;  // inverse

    int current_fft_size_ = 0;

    // Preallocated FFT buffers (raw storage for kiss_fft_cpx arrays)
    // Kept here to avoid per-block allocations in the audio callback.
    std::vector<char> input_cpx_;
    std::vector<char> accum_cpx_;
    std::vector<char> ifft_out_cpx_;

    void init_fft(int fft_size);
    void cleanup_fft();

    // Direct time-domain convolution fallback
    void process_direct(float* buffer, int num_samples);
    std::vector<float> direct_input_;    // scratch copy of input (allocation-free in callback)
    std::vector<float> direct_overlap_;  // tail from direct convolution
};

} // namespace Amplitron
