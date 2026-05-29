#include "audio/effects/cabinet_sim.h"
#include "audio/effects/effect_factory.h"
#include "audio/dsp/wav_loader.h"
#include <algorithm>

namespace Amplitron {

static EffectRegistrar<CabinetSim> reg("Cabinet");

CabinetSim::CabinetSim() {
    params_ = {
        {"Type",    0.0f, 0.0f, 2.0f, 0.0f, "", "Speaker cabinet type. 0 = 1x12 (bright/focused), 1 = 2x12 (balanced), 2 = 4x12 (huge low-end)."},
        {"Bright",  0.5f, 0.0f, 1.0f, 0.5f, "", "Simulates microphone placement. Higher values add a high-frequency resonance peak for more cut."},
    };

    // Default LP at ~5kHz (speaker rolloff)
    lp_.b0 = 0.067455f; lp_.b1 = 0.134911f; lp_.b2 = 0.067455f;
    lp_.a1 = -1.14298f; lp_.a2 = 0.41280f;

    // Default HP at ~80Hz (low cut)
    hp_.b0 = 0.9565f; hp_.b1 = -1.9131f; hp_.b2 = 0.9565f;
    hp_.a1 = -1.9112f; hp_.a2 = 0.9150f;

    // Resonance peak ~2kHz
    peak_.b0 = 1.05f; peak_.b1 = -1.65f; peak_.b2 = 0.65f;
    peak_.a1 = -1.65f; peak_.a2 = 0.70f;

    bright_smooth_ = params_[1].value;
    const float sr = static_cast<float>(std::max(sample_rate_, 1));
    bright_alpha_ = 1.0f - std::exp(-1.0f / (sr * 0.01f));
    dry_buffer_.reserve(1024);
}

CabinetSim::~CabinetSim() {
    // Clean up any unconsumed pending kernel
    ConvolutionKernel* pending = pending_kernel_.exchange(nullptr);
    delete pending;

    // Clean up active kernel
    delete active_kernel_;

    // Clean up old garbage kernel
    const ConvolutionKernel* old = old_kernel_to_delete_.exchange(nullptr);
    delete old;
}

int CabinetSim::max_ir_samples() const {
    // 500ms at current sample rate
    return sample_rate_ / 2;
}

bool CabinetSim::load_ir(const std::string& filepath) {
    WavData wav = load_wav_file(filepath, sample_rate_, max_ir_samples());
    if (wav.samples.empty()) return false;

    raw_ir_samples_ = wav.samples;
    ir_path_ = filepath;

    // Extract filename from path
    size_t sep = filepath.find_last_of("/\\");
    ir_name_ = (sep != std::string::npos) ? filepath.substr(sep + 1) : filepath;
    ir_duration_ms_ = static_cast<float>(raw_ir_samples_.size()) /
                      static_cast<float>(sample_rate_) * 1000.0f;

    // Build kernel with current expected block size, or a reasonable default
    int bs = expected_block_size_.load();
    if (bs <= 0) bs = 256;
    build_kernel(bs);

    return true;
}

void CabinetSim::clear_ir() {
    raw_ir_samples_.clear();
    ir_path_.clear();
    ir_name_.clear();
    ir_duration_ms_ = 0.0f;

    ConvolutionKernel* old = pending_kernel_.exchange(nullptr);
    delete old;

    // Safe clear via audio thread callback
    clear_pending_.store(true, std::memory_order_release);
    expected_block_size_.store(0, std::memory_order_release);
    pending_block_size_.store(0);
}

bool CabinetSim::has_ir() const {
    // Sweep and clean up any old kernels on the GUI thread (thread-safe, lock-free GC)
    const ConvolutionKernel* to_delete = old_kernel_to_delete_.exchange(nullptr, std::memory_order_acquire);
    delete to_delete;

    return !raw_ir_samples_.empty();
}

void CabinetSim::build_kernel(int block_size) {
    if (raw_ir_samples_.empty() || block_size <= 0) return;

    // Perform a GC sweep on the GUI thread here as well
    const ConvolutionKernel* to_delete = old_kernel_to_delete_.exchange(nullptr, std::memory_order_acquire);
    delete to_delete;

    auto* kernel = new ConvolutionKernel(raw_ir_samples_, block_size);
    kernel->source_path = ir_path_;
    kernel->source_name = ir_name_;
    kernel->duration_ms = ir_duration_ms_;

    expected_block_size_.store(block_size, std::memory_order_release);

    ConvolutionKernel* old = pending_kernel_.exchange(kernel);
    delete old;
}

void CabinetSim::check_pending_kernel() {
    // 1. Process pending clear commands
    if (clear_pending_.exchange(false, std::memory_order_acq_rel)) {
        const ConvolutionKernel* old = active_kernel_;
        active_kernel_ = nullptr;
        conv_engine_.set_kernel(nullptr);
        if (old) {
            const ConvolutionKernel* prev_old = old_kernel_to_delete_.exchange(old, std::memory_order_release);
            if (prev_old) {
                delete prev_old;
            }
        }
    }

    // 2. Process pending kernel updates
    ConvolutionKernel* pending = pending_kernel_.exchange(nullptr,
                                                          std::memory_order_acquire);
    if (pending) {
        const ConvolutionKernel* old = active_kernel_;
        active_kernel_ = pending;
        conv_engine_.set_kernel(active_kernel_);
        expected_block_size_.store(pending->block_size(), std::memory_order_release);
        if (old) {
            const ConvolutionKernel* prev_old = old_kernel_to_delete_.exchange(old, std::memory_order_release);
            if (prev_old) {
                delete prev_old;
            }
        }
    }

    // Block size mismatch is handled via pending_kernel_ rebuild on the
    // GUI thread. The audio thread only consumes pre-built kernels.
    pending_block_size_.store(0);
}

void CabinetSim::set_sample_rate(int sample_rate) {
    Effect::set_sample_rate(sample_rate);

    const float sr = static_cast<float>(std::max(sample_rate_, 1));
    bright_alpha_ = 1.0f - std::exp(-1.0f / (sr * 0.01f));

    // Reload IR at new sample rate if one is loaded
    if (!ir_path_.empty()) {
        load_ir(ir_path_);
    }
}

void CabinetSim::process(float* buffer, int num_samples) {
    if (!enabled_) return;

    // If an IR is loaded, convolve for cabinet response.
    check_pending_kernel();
    if (conv_engine_.has_kernel()) {
        // Audio thread: single acquire load of the atomic block size.
        // raw_ir_samples_ is NOT read here (GUI-thread owned) — the
        // expected_block_size_ > 0 check is sufficient because clear_ir()
        // stores 0 before clearing the kernel, and no IR means size is 0.
        int ebs = expected_block_size_.load(std::memory_order_acquire);
        if (num_samples != ebs && num_samples > 0 && ebs > 0) {
            pending_block_size_.store(num_samples, std::memory_order_release);
        }

        if (dry_buffer_.size() < static_cast<size_t>(num_samples)) {
            dry_buffer_.resize(num_samples);
        }
        std::copy(buffer, buffer + num_samples, dry_buffer_.begin());

        conv_engine_.process(buffer, num_samples);

        if (mix_ < 1.0f) {
            apply_mix(dry_buffer_.data(), buffer, num_samples);
        }
        return;
    }

    const float bright_target = params_[1].value;

    for (int i = 0; i < num_samples; ++i) {
        bright_smooth_ += bright_alpha_ * (bright_target - bright_smooth_);
        float bright = bright_smooth_;
        float dry = buffer[i];
        float x = buffer[i];

        x = hp_.process(x);
        x = lp_.process(x);

        // Blend in resonance based on brightness
        float peaked = peak_.process(x);
        x = x * (1.0f - bright * 0.3f) + peaked * bright * 0.3f;

        buffer[i] = dry * (1.0f - mix_) + x * mix_;
    }
}

void CabinetSim::reset() {
    lp_.reset();
    hp_.reset();
    peak_.reset();
    conv_engine_.reset();
    bright_smooth_ = params_[1].value;
    if (dry_buffer_.capacity() < 1024) {
        dry_buffer_.reserve(1024);
    }
}

} // namespace Amplitron
