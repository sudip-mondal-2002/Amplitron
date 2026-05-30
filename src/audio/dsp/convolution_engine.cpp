#include "audio/dsp/convolution_engine.h"
#include "kiss_fft.h"
#include <cstring>
#include <cmath>
#include <algorithm>

namespace Amplitron {

// Helper: complex multiply-accumulate (accum += a * b)
static void complex_multiply_accumulate(kiss_fft_cpx* accum,
                                         const kiss_fft_cpx* a,
                                         const kiss_fft_cpx* b,
                                         int n) {
    for (int i = 0; i < n; ++i) {
        accum[i].r += a[i].r * b[i].r - a[i].i * b[i].i;
        accum[i].i += a[i].r * b[i].i + a[i].i * b[i].r;
    }
}

// =============================================================================
// ConvolutionKernel
// =============================================================================

ConvolutionKernel::ConvolutionKernel(const std::vector<float>& ir_samples,
                                     int block_size)
    : block_size_(block_size)
    , fft_size_(block_size * 2)
    , ir_length_(static_cast<int>(ir_samples.size()))
    , ir_time_(ir_samples) {

    if (ir_samples.empty() || block_size <= 0) {
        num_partitions_ = 0;
        return;
    }

    // Number of partitions = ceil(ir_length / block_size)
    num_partitions_ = (ir_length_ + block_size_ - 1) / block_size_;

    // Allocate forward FFT config
    fft_cfg_ = kiss_fft_alloc(fft_size_, 0, nullptr, nullptr);

    // Pre-compute frequency-domain representation of each partition
    std::vector<kiss_fft_cpx> time_buf(static_cast<size_t>(fft_size_));
    std::vector<kiss_fft_cpx> freq_buf(static_cast<size_t>(fft_size_));

    partitions_freq_.resize(static_cast<size_t>(num_partitions_));

    for (int p = 0; p < num_partitions_; ++p) {
        // Zero the time buffer
        std::fill(time_buf.begin(), time_buf.end(), kiss_fft_cpx{0.0f, 0.0f});

        // Copy this partition's IR samples into the real part
        int offset = p * block_size_;
        int count = std::min(block_size_, ir_length_ - offset);
        for (int i = 0; i < count; ++i) {
            time_buf[static_cast<size_t>(i)].r = ir_samples[static_cast<size_t>(offset + i)];
            time_buf[static_cast<size_t>(i)].i = 0.0f;
        }

        // Forward FFT
        kiss_fft(fft_cfg_, time_buf.data(), freq_buf.data());

        // Store the frequency-domain partition
        size_t byte_size = sizeof(kiss_fft_cpx) * static_cast<size_t>(fft_size_);
        partitions_freq_[static_cast<size_t>(p)].resize(byte_size);
        std::memcpy(partitions_freq_[static_cast<size_t>(p)].data(),
                    freq_buf.data(), byte_size);
    }
}

ConvolutionKernel::~ConvolutionKernel() {
    if (fft_cfg_) kiss_fft_free(fft_cfg_);
}

const void* ConvolutionKernel::partition_freq(int index) const {
    if (index < 0 || index >= num_partitions_) return nullptr;
    return partitions_freq_[static_cast<size_t>(index)].data();
}

// =============================================================================
// ConvolutionEngine
// =============================================================================

ConvolutionEngine::ConvolutionEngine() = default;

ConvolutionEngine::~ConvolutionEngine() {
    cleanup_fft();
}

void ConvolutionEngine::init_fft(int fft_size) {
    cleanup_fft();
    fft_cfg_ = kiss_fft_alloc(fft_size, 0, nullptr, nullptr);   // forward
    ifft_cfg_ = kiss_fft_alloc(fft_size, 1, nullptr, nullptr);  // inverse
    current_fft_size_ = fft_size;
}

void ConvolutionEngine::cleanup_fft() {
    if (fft_cfg_) { kiss_fft_free(fft_cfg_); fft_cfg_ = nullptr; }
    if (ifft_cfg_) { kiss_fft_free(ifft_cfg_); ifft_cfg_ = nullptr; }
    current_fft_size_ = 0;
}

void ConvolutionEngine::set_kernel(const ConvolutionKernel* kernel) {
    kernel_ = kernel;
    reset();
}

void ConvolutionEngine::reset() {
    if (!kernel_) {
        cleanup_fft();
        fdl_.clear();
        overlap_.clear();
        direct_input_.clear();
        direct_overlap_.clear();
        input_cpx_.clear();
        accum_cpx_.clear();
        ifft_out_cpx_.clear();
        fdl_index_ = 0;
        return;
    }

    int fft_size = kernel_->fft_size();
    int num_parts = kernel_->num_partitions();

    // Initialize FFT if size changed
    if (current_fft_size_ != fft_size) {
        init_fft(fft_size);
    }

    // Initialize frequency-domain delay line
    if (fft_size <= 0 || fft_size > 65536) return; // Sanity check

    size_t cpx_bytes = sizeof(kiss_fft_cpx) * static_cast<size_t>(fft_size);
    fdl_.resize(static_cast<size_t>(num_parts));
    for (auto& buf : fdl_) {
        buf.assign(cpx_bytes, 0);
    }
    fdl_index_ = 0;

    // Initialize overlap buffer
    overlap_.assign(static_cast<size_t>(kernel_->block_size()), 0.0f);

    // Initialize direct convolution overlap
    int ir_len = kernel_->ir_length();
    if (ir_len > 0) {
        direct_overlap_.assign(static_cast<size_t>(ir_len - 1), 0.0f);
    } else {
        direct_overlap_.clear();
    }

    // Scratch input copy for direct convolution fallback
    direct_input_.assign(static_cast<size_t>(kernel_->block_size()), 0.0f);

    // Initialize FFT workspace buffers (allocation-free during process())
    input_cpx_.assign(cpx_bytes, 0);
    accum_cpx_.assign(cpx_bytes, 0);
    ifft_out_cpx_.assign(cpx_bytes, 0);
}

void ConvolutionEngine::process_direct(float* buffer, int num_samples) {
    const auto& ir = kernel_->ir_time_domain();
    int ir_len = static_cast<int>(ir.size());
    if (ir_len == 0) return;

    // Output length = num_samples + ir_len - 1.
    // We output num_samples and carry over the tail (overlap-add) in direct_overlap_.
    const int tail_len = ir_len - 1;
    if (tail_len <= 0) return;

    // direct_overlap_ should be pre-sized in reset(); avoid allocations here.
    if (static_cast<int>(direct_overlap_.size()) != tail_len) return;

    // Need original input; avoid per-call allocations by using direct_input_ scratch.
    if (static_cast<int>(direct_input_.size()) < num_samples) return;
    std::memcpy(direct_input_.data(), buffer, sizeof(float) * static_cast<size_t>(num_samples));

    // First num_samples samples: convolution + previous overlap
    for (int n = 0; n < num_samples; ++n) {
        float y = (n < tail_len) ? direct_overlap_[static_cast<size_t>(n)] : 0.0f;

        // y += sum_{j=0}^{ir_len-1} x[n-j] * h[j]
        int j0 = std::max(0, n - (num_samples - 1));
        int j1 = std::min(ir_len - 1, n);
        for (int j = j0; j <= j1; ++j) {
            y += direct_input_[static_cast<size_t>(n - j)] * ir[static_cast<size_t>(j)];
        }
        buffer[n] = y;
    }

    // Tail samples for next block: y[num_samples .. num_samples+tail_len-1]
    for (int t = 0; t < tail_len; ++t) {
        const int n = num_samples + t;
        float y = 0.0f;

        // y += sum_{j=0}^{ir_len-1} x[n-j] * h[j], where (n-j) in [0, num_samples-1]
        int j0 = std::max(0, n - (num_samples - 1));
        int j1 = std::min(ir_len - 1, n);
        for (int j = j0; j <= j1; ++j) {
            int xi = n - j;
            if (xi >= 0 && xi < num_samples) {
                y += direct_input_[static_cast<size_t>(xi)] * ir[static_cast<size_t>(j)];
            }
        }
        direct_overlap_[static_cast<size_t>(t)] = y;
    }
}

void ConvolutionEngine::process(float* buffer, int num_samples) {
    if (!kernel_ || kernel_->ir_length() == 0) return;

    int block_size = kernel_->block_size();
    int fft_size = kernel_->fft_size();
    int num_parts = kernel_->num_partitions();

    // If block size doesn't match, fall back to direct convolution
    if (num_samples != block_size) {
        process_direct(buffer, num_samples);
        return;
    }

    // Also use direct convolution for very short IRs (1 partition, IR <= block_size)
    if (num_parts == 1 && kernel_->ir_length() <= block_size) {
        process_direct(buffer, num_samples);
        return;
    }

    // --- Partitioned overlap-add convolution ---

    // 1. Prepare input: zero-pad to fft_size
    auto* input_cpx = reinterpret_cast<kiss_fft_cpx*>(input_cpx_.data());
    for (int i = 0; i < block_size; ++i) {
        input_cpx[static_cast<size_t>(i)].r = buffer[i];
        input_cpx[static_cast<size_t>(i)].i = 0.0f;
    }
    for (int i = block_size; i < fft_size; ++i) {
        input_cpx[static_cast<size_t>(i)].r = 0.0f;
        input_cpx[static_cast<size_t>(i)].i = 0.0f;
    }

    // 2. Forward FFT of input -> store in FDL at current index
    auto* fdl_data = reinterpret_cast<kiss_fft_cpx*>(
        fdl_[static_cast<size_t>(fdl_index_)].data());
    kiss_fft(fft_cfg_, input_cpx, fdl_data);

    // 3. Complex multiply-accumulate across all partitions
    auto* accum = reinterpret_cast<kiss_fft_cpx*>(accum_cpx_.data());
    std::memset(accum, 0, sizeof(kiss_fft_cpx) * static_cast<size_t>(fft_size));

    for (int k = 0; k < num_parts; ++k) {
        // FDL index for partition k (circular)
        int fdl_idx = (fdl_index_ - k + num_parts) % num_parts;
        const auto* fdl_block = reinterpret_cast<const kiss_fft_cpx*>(
            fdl_[static_cast<size_t>(fdl_idx)].data());
        const auto* ir_block = reinterpret_cast<const kiss_fft_cpx*>(
            kernel_->partition_freq(k));

        complex_multiply_accumulate(accum, fdl_block, ir_block, fft_size);
    }

    // 4. Inverse FFT
    auto* ifft_out = reinterpret_cast<kiss_fft_cpx*>(ifft_out_cpx_.data());
    kiss_fft(ifft_cfg_, accum, ifft_out);

    // kiss_fft inverse does NOT normalize -- divide by fft_size
    float norm = 1.0f / static_cast<float>(fft_size);

    // 5. Output = first block_size samples + overlap from previous block
    for (int i = 0; i < block_size; ++i) {
        buffer[i] = ifft_out[static_cast<size_t>(i)].r * norm +
                    overlap_[static_cast<size_t>(i)];
    }

    // 6. Store new overlap (last block_size samples of IFFT result)
    for (int i = 0; i < block_size; ++i) {
        overlap_[static_cast<size_t>(i)] =
            ifft_out[static_cast<size_t>(block_size + i)].r * norm;
    }

    // 7. Advance FDL index
    fdl_index_ = (fdl_index_ + 1) % num_parts;
}

} // namespace Amplitron
