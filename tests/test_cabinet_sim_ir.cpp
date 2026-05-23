#include "test_framework.h"
#include "audio/effects/cabinet_sim.h"
#include <cstdint>
#include <fstream>
#include <vector>
#include <cmath>
#include <cstdio>

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

static bool write_wav_mono_pcm16(const std::string& path,
                                 const std::vector<float>& samples,
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
    write_le32(out, 16);               // PCM fmt chunk size
    write_le16(out, 1);                // audio format = PCM
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

    ASSERT_TRUE(
        write_wav_mono_pcm16(path, {1.0f}, 48000));

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
