#include "test_framework.h"
#include "audio/dsp/convolution_engine.h"

#include <vector>
#include <cmath>
#include <memory>

using namespace Amplitron;
using namespace TestFramework;

TEST(ConvolutionEngine_ResetClearsState) {
    std::vector<float> ir = {1.0f, 0.5f, 0.25f};

    auto kernel =
        std::make_shared<ConvolutionKernel>(ir, 256);

    ConvolutionEngine conv;

    conv.set_kernel(kernel);

    std::vector<float> buffer(256, 1.0f);

    conv.process(buffer.data(), 256);

    conv.reset();

    std::vector<float> silent(256, 0.0f);

    conv.process(silent.data(), 256);

    for (float s : silent) {
        ASSERT_TRUE(std::isfinite(s));
        ASSERT_NEAR(s, 0.0f, 1e-4f);
    }
}

TEST(ConvolutionEngine_PartitionedFFTPath) {
    std::vector<float> ir(1024, 0.0f);

    // simple impulse-like IR
    ir[0] = 1.0f;
    ir[128] = 0.5f;
    ir[512] = 0.25f;

    auto kernel =
        std::make_shared<ConvolutionKernel>(ir, 256);

    ConvolutionEngine conv;
    conv.set_kernel(kernel);

    std::vector<float> buffer(256);

    for (size_t i = 0; i < buffer.size(); ++i) {
        buffer[i] =
            std::sin(static_cast<float>(i) * 0.01f);
    }

    conv.process(buffer.data(), 256);

    for (float s : buffer) {
        ASSERT_TRUE(std::isfinite(s));
    }
}

TEST(ConvolutionEngine_HasKernelAfterSetKernel) {
    std::vector<float> ir = {1.0f};

    auto kernel =
        std::make_shared<ConvolutionKernel>(ir, 256);

    ConvolutionEngine conv;

    ASSERT_FALSE(conv.has_kernel());

    conv.set_kernel(kernel);

    ASSERT_TRUE(conv.has_kernel());
}

TEST(ConvolutionEngine_OverlapAddConsistency) {
    std::vector<float> ir = {1.0f, 0.5f, 0.25f};

    auto kernel =
        std::make_shared<ConvolutionKernel>(ir, 256);

    ConvolutionEngine conv;

    conv.set_kernel(kernel);

    std::vector<float> buffer(1024);

    for (size_t i = 0; i < buffer.size(); ++i) {
        buffer[i] = std::sin(
            static_cast<float>(i) * 0.01f);
    }

    for (int i = 0; i < 1024; i += 256) {
        conv.process(buffer.data() + i, 256);
    }

    for (float s : buffer) {
        ASSERT_TRUE(std::isfinite(s));
    }
}

TEST(ConvolutionEngine_ClearKernel) {
    std::vector<float> ir = {1.0f};

    auto kernel =
        std::make_shared<ConvolutionKernel>(ir, 256);

    ConvolutionEngine conv;

    conv.set_kernel(kernel);

    ASSERT_TRUE(conv.has_kernel());

    conv.set_kernel(nullptr);

    ASSERT_FALSE(conv.has_kernel());
}

TEST(ConvolutionEngine_SmallBlockProcessing) {
    std::vector<float> ir = {1.0f, 0.5f};

    auto kernel =
        std::make_shared<ConvolutionKernel>(ir, 64);

    ConvolutionEngine conv;

    conv.set_kernel(kernel);

    std::vector<float> buffer(32, 1.0f);

    conv.process(buffer.data(), 32);

    for (float s : buffer) {
        ASSERT_TRUE(std::isfinite(s));
    }
}

TEST(ConvolutionEngine_ZeroInputRemainsSilent) {
    std::vector<float> ir = {1.0f, 0.5f};

    auto kernel =
        std::make_shared<ConvolutionKernel>(ir, 128);

    ConvolutionEngine conv;

    conv.set_kernel(kernel);

    std::vector<float> buffer(128, 0.0f);

    conv.process(buffer.data(), 128);

    for (float s : buffer) {
        ASSERT_NEAR(s, 0.0f, 1e-5f);
    }
}

TEST(ConvolutionKernel_PartitionFreqBounds) {
    std::vector<float> ir(512, 0.5f);

    ConvolutionKernel kernel(ir, 256);

    ASSERT_TRUE(kernel.partition_freq(0) != nullptr);
    ASSERT_TRUE(kernel.partition_freq(-1) == nullptr);
    ASSERT_TRUE(kernel.partition_freq(999) == nullptr);
}

TEST(ConvolutionKernel_EmptyIR) {
    std::vector<float> ir;

    ConvolutionKernel kernel(ir, 256);

    ASSERT_EQ(kernel.num_partitions(), 0);
}

