#include "test_framework.h"
#include "audio/dsp/wav_loader.h"

#include <fstream>
#include <cmath>
#include <cstdio>
#include <cstdint>

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

TEST(WavLoader_LoadMissingFileReturnsEmpty) {
    WavData wav = load_wav_file("definitely_missing.wav");

    ASSERT_TRUE(wav.samples.empty());
}

TEST(WavLoader_LoadMalformedFileReturnsEmpty) {
    const std::string path = "bad_wav.wav";

    {
        std::ofstream out(path, std::ios::binary);
        out << "not a real wav";
    }

    WavData wav = load_wav_file(path);

    ASSERT_TRUE(wav.samples.empty());

    std::remove(path.c_str());
}

TEST(WavLoader_ResampleLinearChangesLength) {
    std::vector<float> input(441);

    for (size_t i = 0; i < input.size(); ++i) {
        input[i] = std::sin(static_cast<float>(i) * 0.01f);
    }

    auto output = resample_linear(input, 44100, 48000);

    ASSERT_FALSE(output.empty());
    ASSERT_GT(static_cast<int>(output.size()),
              static_cast<int>(input.size()));
}

TEST(WavLoader_ResampleLinearUpsample) {
    std::vector<float> input(441);

    for (size_t i = 0; i < input.size(); ++i) {
        input[i] = std::sin(
            static_cast<float>(i) * 0.01f);
    }

    auto output =
        resample_linear(input, 44100, 48000);

    ASSERT_FALSE(output.empty());

    ASSERT_TRUE(
        output.size() > input.size());

    for (float s : output) {
        ASSERT_TRUE(std::isfinite(s));
    }
}

TEST(WavLoader_MaxLengthLimit) {
    const std::string path = "limit_test.wav";

    std::vector<float> samples(4096, 0.25f);

    ASSERT_TRUE(
        write_wav_mono_pcm16(path, samples, 48000));

    WavData wav =
        load_wav_file(path, 48000, 512);

    ASSERT_TRUE(
    static_cast<int>(wav.samples.size()) <= 512);

    std::remove(path.c_str());
}
