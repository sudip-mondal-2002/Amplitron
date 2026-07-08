#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "test_framework.h"

#define private public
#define protected public
#include "audio/effects/amp_cab/cabinet_sim.h"
#undef private
#undef protected

using namespace Amplitron;
using namespace TestFramework;

static void write_le16(std::ofstream& out, uint16_t v) {
    char b[2];
    b[0] = static_cast<char>(v & 0xFF);
    b[1] = static_cast<char>((v >> 8) & 0xFF);
    out.write(b, 2);
}

static void write_le32(std::ofstream& out, uint32_t v) {
    char b[4];
    b[0] = static_cast<char>(v & 0xFF);
    b[1] = static_cast<char>((v >> 8) & 0xFF);
    b[2] = static_cast<char>((v >> 16) & 0xFF);
    b[3] = static_cast<char>((v >> 24) & 0xFF);
    out.write(b, 4);
}

static bool write_wav_mono_pcm16(const std::string& path, const std::vector<float>& samples,
                                 int sample_rate) {
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) return false;

    const uint16_t num_channels = 1;
    const uint16_t bits_per_sample = 16;
    const uint16_t block_align = static_cast<uint16_t>(num_channels * (bits_per_sample / 8));
    const uint32_t byte_rate = static_cast<uint32_t>(sample_rate) * block_align;
    const uint32_t data_bytes = static_cast<uint32_t>(samples.size()) * block_align;
    const uint32_t riff_size = 36 + data_bytes;

    // RIFF header
    out.write("RIFF", 4);
    write_le32(out, riff_size);
    out.write("WAVE", 4);

    // fmt chunk
    out.write("fmt ", 4);
    write_le32(out, 16);  // PCM fmt chunk size
    write_le16(out, 1);   // audio format = PCM
    write_le16(out, num_channels);
    write_le32(out, static_cast<uint32_t>(sample_rate));
    write_le32(out, byte_rate);
    write_le16(out, block_align);
    write_le16(out, bits_per_sample);

    // data chunk
    out.write("data", 4);
    write_le32(out, data_bytes);

    // samples (clamped)
    for (float s : samples) {
        float x = std::fmax(-1.0f, std::fmin(1.0f, s));
        int16_t v = static_cast<int16_t>(std::lrint(x * 32767.0f));
        write_le16(out, static_cast<uint16_t>(v));
    }
    out.close();
    return true;
}

TEST(CabinetSim_IR_UnitImpulse_Identity) {
    const int block_size = 256;

    // IR = [1.0] should act as identity
    std::string path = "test_cabinet_ir_unit_impulse.wav";
    ASSERT_TRUE(write_wav_mono_pcm16(path, {1.0f}, 48000));

    CabinetSim cab;
    cab.set_sample_rate(48000);
    ASSERT_TRUE(cab.load_ir(path));
    ASSERT_TRUE(cab.has_ir());
    cab.set_enabled(true);

    std::vector<float> buf(block_size);
    for (int i = 0; i < block_size; ++i) {
        buf[i] = std::sin(2.0f * 3.14159265f * 440.0f * static_cast<float>(i) / 48000.0f);
    }
    std::vector<float> expected(buf);

    cab.process(buf.data(), block_size);

    for (int i = 0; i < block_size; ++i) {
        ASSERT_NEAR(buf[i], expected[i], 1e-4f);
    }

    std::remove(path.c_str());
}

TEST(CabinetSim_IR_MissingFileReturnsFalse) {
    std::string valid = "valid_ir.wav";

    ASSERT_TRUE(write_wav_mono_pcm16(valid, {1.0f}, 48000));

    CabinetSim cab;
    cab.set_sample_rate(48000);

    ASSERT_TRUE(cab.load_ir(valid));
    ASSERT_TRUE(cab.has_ir());

    ASSERT_FALSE(cab.load_ir("definitely_missing_ir.wav"));

    // Ensure failed load clears previous state
    ASSERT_TRUE(cab.has_ir());

    std::remove(valid.c_str());
}

TEST(CabinetSim_IR_MalformedFileReturnsFalse) {
    const std::string path = "bad_ir.wav";

    {
        std::ofstream out(path, std::ios::binary);
        out << "this is not a wav";
    }

    CabinetSim cab;
    cab.set_sample_rate(48000);

    ASSERT_FALSE(cab.load_ir(path));
    ASSERT_FALSE(cab.has_ir());

    std::remove(path.c_str());
}

TEST(CabinetSim_IR_LongRunStability) {
    const int block_size = 256;

    std::string path = "test_longrun_ir.wav";
    ASSERT_TRUE(write_wav_mono_pcm16(path, {1.0f, 0.5f, 0.25f}, 48000));

    CabinetSim cab;
    cab.set_sample_rate(48000);

    ASSERT_TRUE(cab.load_ir(path));

    std::vector<float> buf(block_size);

    for (int iter = 0; iter < 1000; ++iter) {
        for (int i = 0; i < block_size; ++i) {
            buf[i] = std::sin(0.01f * static_cast<float>(i));
        }

        cab.process(buf.data(), block_size);

        for (float s : buf) {
            ASSERT_TRUE(std::isfinite(s));
        }
    }

    std::remove(path.c_str());
}

TEST(CabinetSim_IR_SilenceRemainsSilent) {
    const int block_size = 256;

    std::string path = "test_silence_ir.wav";
    ASSERT_TRUE(write_wav_mono_pcm16(path, {1.0f}, 48000));

    CabinetSim cab;
    cab.set_sample_rate(48000);

    ASSERT_TRUE(cab.load_ir(path));

    std::vector<float> buf(block_size, 0.0f);

    cab.process(buf.data(), block_size);

    for (float s : buf) {
        ASSERT_NEAR(s, 0.0f, 1e-6f);
    }

    std::remove(path.c_str());
}

TEST(CabinetSim_IR_DelayedImpulse) {
    const int block_size = 256;

    // IR = [0, 0, 0, 0, 1.0] delays by 4 samples
    std::string path = "test_cabinet_ir_delayed_impulse.wav";
    std::vector<float> ir(5, 0.0f);
    ir[4] = 1.0f;
    ASSERT_TRUE(write_wav_mono_pcm16(path, ir, 48000));

    CabinetSim cab;
    cab.set_sample_rate(48000);
    ASSERT_TRUE(cab.load_ir(path));
    cab.set_enabled(true);

    std::vector<float> buf(block_size, 0.0f);
    buf[0] = 1.0f;

    cab.process(buf.data(), block_size);

    for (int i = 0; i < block_size; ++i) {
        float expected = (i == 4) ? 1.0f : 0.0f;
        ASSERT_NEAR(buf[i], expected, 1e-4f);
    }

    std::remove(path.c_str());
}

TEST(CabinetSim_IR_MetadataQueries) {
    std::string path = "metadata_ir.wav";

    ASSERT_TRUE(write_wav_mono_pcm16(path, {1.0f, 0.5f}, 48000));

    CabinetSim cab;
    cab.set_sample_rate(48000);

    ASSERT_TRUE(cab.load_ir(path));

    ASSERT_TRUE(cab.has_ir());
    ASSERT_EQ(cab.ir_name(), "metadata_ir.wav");
    ASSERT_EQ(cab.ir_path(), path);
    ASSERT_GT(cab.ir_duration_ms(), 0.0f);

    std::remove(path.c_str());
}

TEST(CabinetSim_ClearIR_ResetsState) {
    std::string path = "clear_ir.wav";

    ASSERT_TRUE(write_wav_mono_pcm16(path, {1.0f}, 48000));

    CabinetSim cab;
    cab.set_sample_rate(48000);

    ASSERT_TRUE(cab.load_ir(path));
    ASSERT_TRUE(cab.has_ir());

    cab.clear_ir();

    ASSERT_FALSE(cab.has_ir());
    ASSERT_TRUE(cab.ir_name().empty());
    ASSERT_TRUE(cab.ir_path().empty());
    ASSERT_NEAR(cab.ir_duration_ms(), 0.0f, 1e-6f);

    std::remove(path.c_str());
}

TEST(CabinetSim_SetSampleRate_ReloadsIR) {
    std::string path = "reload_ir.wav";

    ASSERT_TRUE(write_wav_mono_pcm16(path, {1.0f, 0.5f}, 44100));

    CabinetSim cab;
    cab.set_sample_rate(44100);

    ASSERT_TRUE(cab.load_ir(path));

    float before = cab.ir_duration_ms();

    cab.set_sample_rate(48000);

    ASSERT_TRUE(cab.has_ir());
    ASSERT_GT(cab.ir_duration_ms(), 0.0f);
    ASSERT_NEAR(before, cab.ir_duration_ms(), 5.0f);

    std::remove(path.c_str());
}

TEST(CabinetSim_ClearIRRemovesState) {
    std::string path = "clear_ir.wav";

    ASSERT_TRUE(write_wav_mono_pcm16(path, {1.0f}, 48000));

    CabinetSim cab;

    cab.set_sample_rate(48000);

    ASSERT_TRUE(cab.load_ir(path));

    ASSERT_TRUE(cab.has_ir());

    cab.clear_ir();

    ASSERT_FALSE(cab.has_ir());

    std::remove(path.c_str());
}

TEST(CabinetSim_DisabledProcessPassthrough) {
    CabinetSim cab;

    cab.set_enabled(false);

    float buf[128];

    for (int i = 0; i < 128; ++i) {
        buf[i] = 0.25f;
    }

    cab.process(buf, 128);

    for (float s : buf) {
        ASSERT_NEAR(s, 0.25f, 1e-6f);
    }
}

TEST(CabinetSim_IR_AdvancedSwapAndMismatch) {
    std::string path1 = "test_cabinet_ir1.wav";
    ASSERT_TRUE(write_wav_mono_pcm16(path1, {1.0f, 0.5f}, 48000));

    CabinetSim cab;
    cab.set_sample_rate(48000);
    ASSERT_TRUE(cab.load_ir(path1));
    cab.set_enabled(true);

    // 1. Test wet/dry mixing
    cab.set_mix(0.5f);
    std::vector<float> buf(256, 1.0f);
    cab.process(buf.data(), 256);
    ASSERT_NE(buf[0], 1.0f);

    // 2. Test active kernel swap
    std::string path2 = "test_cabinet_ir2.wav";
    ASSERT_TRUE(write_wav_mono_pcm16(path2, {0.5f, -0.5f}, 48000));
    ASSERT_TRUE(cab.load_ir(path2));

    std::vector<float> buf2(256, 1.0f);
    cab.process(buf2.data(), 256);

    // Swap to a third IR to delete kernel 1
    std::string path3 = "test_cabinet_ir3.wav";
    ASSERT_TRUE(write_wav_mono_pcm16(path3, {0.1f, -0.1f, 0.2f}, 48000));
    ASSERT_TRUE(cab.load_ir(path3));
    cab.process(buf2.data(), 256);

    // Clear IR to swap to null and delete kernel 2
    cab.clear_ir();
    cab.process(buf2.data(), 256);

    // Trigger sweep deletion on the GUI thread
    ASSERT_FALSE(cab.has_ir());

    // Restore IR to continue subsequent tests
    ASSERT_TRUE(cab.load_ir(path1));
    cab.process(buf2.data(), 256);

    // 3. Test block size mismatch
    std::vector<float> buf3(128, 1.0f);
    cab.process(buf3.data(), 128);

    // 4. Test dry buffer resizing (larger block size)
    std::vector<float> buf4(512, 1.0f);
    cab.process(buf4.data(), 512);

    // 5. Test safe clear
    cab.clear_ir();
    cab.process(buf4.data(), 512);
    ASSERT_FALSE(cab.has_ir());

    std::remove(path1.c_str());
    std::remove(path2.c_str());
    std::remove(path3.c_str());
}

TEST(CabinetSim_IR_ManualSwapToDeleteOld) {
    CabinetSim cab;
    cab.set_sample_rate(48000);

    std::vector<float> ir = {1.0f};
    auto* k1 = new ConvolutionKernel(ir, 256);
    auto* k2 = new ConvolutionKernel(ir, 256);
    auto* k3 = new ConvolutionKernel(ir, 256);

    cab.active_kernel_ = k1;
    cab.conv_engine_.set_kernel(k1);
    cab.expected_block_size_.store(256, std::memory_order_release);

    cab.old_kernel_to_delete_.store(k2, std::memory_order_release);
    cab.pending_kernel_.store(k3, std::memory_order_release);

    std::vector<float> buf(256, 1.0f);
    cab.process(buf.data(), 256);

    ASSERT_EQ(cab.active_kernel_, k3);
    ASSERT_EQ(cab.old_kernel_to_delete_.load(), k1);
}

TEST(CabinetSim_ResetShrunkDryBuffer) {
    CabinetSim cab;
    std::vector<float> empty;
    cab.dry_buffer_.swap(empty);
    ASSERT_LT(cab.dry_buffer_.capacity(), 1024ULL);

    cab.reset();
    ASSERT_GE(cab.dry_buffer_.capacity(), 1024ULL);

    const CabinetSim& const_cab = cab;
    ASSERT_EQ(const_cab.params().size(), cab.params().size());
}
