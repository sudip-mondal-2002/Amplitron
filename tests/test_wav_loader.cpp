#include "test_framework.h"
#include "audio/dsp/wav_loader.h"

#include <fstream>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <vector>

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

static bool write_wav_mono_pcm16(
    const std::string& path,
    const std::vector<float>& samples,
    int sample_rate) {

    std::ofstream out(path, std::ios::binary);

    if (!out.is_open()) {
        return false;
    }

    const uint16_t num_channels = 1;
    const uint16_t bits_per_sample = 16;

    const uint16_t block_align =
        num_channels * (bits_per_sample / 8);

    const uint32_t byte_rate =
        static_cast<uint32_t>(sample_rate) *
        block_align;

    const uint32_t data_bytes =
        static_cast<uint32_t>(samples.size()) *
        block_align;

    const uint32_t riff_size =
        36 + data_bytes;

    out.write("RIFF", 4);
    write_le32(out, riff_size);
    out.write("WAVE", 4);

    out.write("fmt ", 4);
    write_le32(out, 16);

    write_le16(out, 1);
    write_le16(out, num_channels);

    write_le32(out,
        static_cast<uint32_t>(sample_rate));

    write_le32(out, byte_rate);

    write_le16(out, block_align);
    write_le16(out, bits_per_sample);

    out.write("data", 4);
    write_le32(out, data_bytes);

    for (float s : samples) {

        float x =
            std::fmax(-1.0f,
            std::fmin(1.0f, s));

        int16_t v =
            static_cast<int16_t>(
                std::lrint(x * 32767.0f));

        write_le16(out,
            static_cast<uint16_t>(v));
    }

    out.close();

    return true;
}

static bool write_wav_stereo_pcm16(
    const std::string& path,
    const std::vector<float>& left,
    const std::vector<float>& right,
    int sample_rate) {

    if (left.size() != right.size()) {
        return false;
    }

    std::ofstream out(path, std::ios::binary);

    if (!out.is_open()) {
        return false;
    }

    const uint16_t audio_format = 3;
    const uint16_t num_channels = 2;
    const uint16_t bits_per_sample = 32;

    const uint16_t block_align =
        num_channels * (bits_per_sample / 8);

    const uint32_t byte_rate =
        sample_rate * block_align;

    const uint32_t data_bytes =
        static_cast<uint32_t>(left.size()) *
        block_align;

    const uint32_t riff_size =
        36 + data_bytes;

    out.write("RIFF", 4);
    write_le32(out, riff_size);
    out.write("WAVE", 4);

    out.write("fmt ", 4);
    write_le32(out, 16);

    write_le16(out, audio_format);
    write_le16(out, num_channels);

    write_le32(out, sample_rate);
    write_le32(out, byte_rate);

    write_le16(out, block_align);
    write_le16(out, bits_per_sample);

    out.write("data", 4);
    write_le32(out, data_bytes);

    for (size_t i = 0; i < left.size(); ++i) {

        out.write(
            reinterpret_cast<const char*>(&left[i]),
            sizeof(float));

        out.write(
            reinterpret_cast<const char*>(&right[i]),
            sizeof(float));
    }

    out.close();

    return true;
}

TEST(WavLoader_TruncatedHeaderReturnsEmpty) {
    const std::string path =
        "truncated.wav";

    {
        std::ofstream out(path,
            std::ios::binary);

        out.write("RIFF", 4);
    }

    WavData wav =
        load_wav_file(path);

    ASSERT_TRUE(wav.samples.empty());

    std::remove(path.c_str());
}

TEST(WavLoader_InvalidResampleRate) {
    std::vector<float> input(128, 0.5f);

    auto output =
        resample_linear(input, 48000, 0);

    for (float s : output) {
        ASSERT_TRUE(std::isfinite(s));
    }
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

TEST(WavLoader_ResampleLinearSameRate) {
    std::vector<float> input(128);

    for (size_t i = 0; i < input.size(); ++i) {
        input[i] = static_cast<float>(i) * 0.01f;
    }

    auto output =
        resample_linear(input, 48000, 48000);

    ASSERT_EQ(
        static_cast<int>(output.size()),
        static_cast<int>(input.size()));

    for (size_t i = 0; i < input.size(); ++i) {
        ASSERT_NEAR(output[i], input[i], 1e-6f);
    }
}

TEST(WavLoader_ResampleLinearEmptyInput) {
    std::vector<float> input;

    auto output =
        resample_linear(input, 44100, 48000);

    ASSERT_TRUE(output.empty());
}

TEST(WavLoader_ResampleLinearDownsample) {
    std::vector<float> input(2048);

    for (size_t i = 0; i < input.size(); ++i) {
        input[i] = std::sin(
            static_cast<float>(i) * 0.01f);
    }

    auto output =
        resample_linear(input, 48000, 22050);

    ASSERT_FALSE(output.empty());

    ASSERT_TRUE(
        output.size() < input.size());

    for (float s : output) {
        ASSERT_TRUE(std::isfinite(s));
    }
}

TEST(WavLoader_StereoMixdown) {
    const std::string path =
        "stereo_test.wav";

    std::vector<float> left(512, 1.0f);
    std::vector<float> right(512, -1.0f);

    ASSERT_TRUE(
        write_wav_stereo_pcm16(
            path,
            left,
            right,
            48000));

    WavData wav =
        load_wav_file(path);

    ASSERT_FALSE(wav.samples.empty());

    ASSERT_EQ(wav.channels, 2);

    ASSERT_EQ(wav.sample_rate, 48000);

    for (float s : wav.samples) {
        ASSERT_TRUE(std::isfinite(s));
    }

    std::remove(path.c_str());
}

