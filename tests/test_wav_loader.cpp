#include "test_framework.h"
#include "audio/dsp/wav_loader.h"
#include "common.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>

using namespace Amplitron;
using namespace TestFramework;
static void wle16(std::ofstream& f, uint16_t v) {
    uint8_t b[2] = { uint8_t(v), uint8_t(v >> 8) };
    f.write(reinterpret_cast<char*>(b), 2);
}
static void wle32(std::ofstream& f, uint32_t v) {
    uint8_t b[4] = { uint8_t(v), uint8_t(v>>8),
                     uint8_t(v>>16), uint8_t(v>>24) };
    f.write(reinterpret_cast<char*>(b), 4);
}
static bool write_pcm16_wav(const std::string& path,
                             uint16_t num_channels,
                             uint32_t sample_rate,
                             const std::vector<int16_t>& samples) {
    std::filesystem::create_directories("tests/assets");                           
    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    const uint16_t bits        = 16;
    const uint16_t block_align = num_channels * (bits / 8);
    const uint32_t byte_rate   = sample_rate * block_align;
    const uint32_t data_bytes  =
        static_cast<uint32_t>(samples.size()) * sizeof(int16_t);
    const uint32_t riff_size   = 36 + data_bytes;
    f.write("RIFF", 4);  wle32(f, riff_size);  f.write("WAVE", 4);
    f.write("fmt ", 4);  wle32(f, 16);
    wle16(f, 1);                    
    wle16(f, num_channels);
    wle32(f, sample_rate);
    wle32(f, byte_rate);
    wle16(f, block_align);
    wle16(f, bits);
    f.write("data", 4);  wle32(f, data_bytes);
    for (int16_t s : samples) wle16(f, static_cast<uint16_t>(s));
    return f.good();
}
static bool write_zero_frame_wav(const std::string& path,
                                  uint32_t sample_rate = 44100) {
    std::filesystem::create_directories("tests/assets");                               
    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    const uint32_t riff_size = 36;
    f.write("RIFF", 4);  wle32(f, riff_size);  f.write("WAVE", 4);
    f.write("fmt ", 4);  wle32(f, 16);
    wle16(f, 1);           
    wle16(f, 1);           
    wle32(f, sample_rate);
    wle32(f, sample_rate * 2);  
    wle16(f, 2);           
    wle16(f, 16);          
    f.write("data", 4);  wle32(f, 0);   
    return f.good();
}
struct TempFile {
    explicit TempFile(std::string p) : path(std::move(p)) {}
    ~TempFile() { std::remove(path.c_str()); }
    const std::string path;
};


TEST(WavLoader_EmptyFrames_ReturnsEmpty) {
    const std::string path = "tests/assets/wl_test_empty_frames.wav";
    TempFile guard(path);
    ASSERT_TRUE(write_zero_frame_wav(path, 44100));
    WavData wav = load_wav_file(path);
    ASSERT_TRUE(wav.samples.empty());
}
TEST(WavLoader_EmptyFrames_NonStandardRate_ReturnsEmpty) {
    const std::string path = "tests/assets/wl_test_empty_frames_22k.wav";
    TempFile guard(path);
    ASSERT_TRUE(write_zero_frame_wav(path, 22050));
    WavData wav = load_wav_file(path, 48000);
    ASSERT_TRUE(wav.samples.empty());
}
static bool write_header_only_wav(const std::string& path,
                                   uint32_t claimed_frames = 64,
                                   uint32_t sample_rate    = 44100) {
    std::filesystem::create_directories("tests/assets");                                
    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    const uint32_t fake_data_bytes = claimed_frames * 2; 
    const uint32_t riff_size       = 36 + fake_data_bytes;
    f.write("RIFF", 4);  wle32(f, riff_size);  f.write("WAVE", 4);
    f.write("fmt ", 4);  wle32(f, 16);
    wle16(f, 1);              
    wle16(f, 1);              
    wle32(f, sample_rate);
    wle32(f, sample_rate * 2);
    wle16(f, 2);
    wle16(f, 16);
    f.write("data", 4);  wle32(f, fake_data_bytes);
    return f.good();
}
TEST(WavLoader_TruncatedData_FramesReadZero_ReturnsEmpty) {
    const std::string path = "tests/assets/wl_test_hdr_only.wav";
    TempFile guard(path);
    ASSERT_TRUE(write_header_only_wav(path, 64, 44100));
    WavData wav = load_wav_file(path);
    ASSERT_TRUE(wav.samples.empty());
}
TEST(WavLoader_TruncatedData_WithResampleTarget_ReturnsEmpty) {
    const std::string path = "tests/assets/wl_test_hdr_only_48k.wav";
    TempFile guard(path);
    ASSERT_TRUE(write_header_only_wav(path, 128, 22050));
    WavData wav = load_wav_file(path, 48000);
    ASSERT_TRUE(wav.samples.empty());
}

static bool write_constant_mono_wav(const std::string& path,
                                     float value,
                                     int total_frames,
                                     uint32_t sample_rate) {
    const int16_t pcm =
        static_cast<int16_t>(std::lrint(
            std::fmax(-1.f, std::fmin(1.f, value)) * 32767.f));
    std::vector<int16_t> s(total_frames, pcm);
    return write_pcm16_wav(path, 1, sample_rate, s);
}
TEST(WavLoader_Truncation_ExactLength) {
    const std::string path = "tests/assets/wl_test_truncation_exact.wav";
    TempFile guard(path);
    ASSERT_TRUE(write_constant_mono_wav(path, 0.5f, 2048, 48000));
    const int limit = 256;
    WavData wav = load_wav_file(path, 48000, limit);
    ASSERT_EQ(static_cast<int>(wav.samples.size()), limit);
}
TEST(WavLoader_Truncation_PrefixContentPreserved) {
    const std::string path = "tests/assets/wl_test_truncation_content.wav";
    TempFile guard(path);
    const int N = 1024;
    std::vector<int16_t> pcm(N);
    for (int i = 0; i < N; ++i)
        pcm[i] = static_cast<int16_t>(
            std::lrint(0.7f * std::sin(Amplitron::TWO_PI * i / 64.f) * 32767.f));
    ASSERT_TRUE(write_pcm16_wav(path, 1, 44100, pcm));
    const int limit = 64;
    WavData wav = load_wav_file(path, 44100, limit);
    ASSERT_EQ(static_cast<int>(wav.samples.size()), limit);
    ASSERT_NEAR(wav.samples[16], 0.7f, 5e-3f);
}
TEST(WavLoader_Truncation_AfterResample) {
    const std::string path = "tests/assets/wl_test_truncation_resample.wav";
    TempFile guard(path);
    ASSERT_TRUE(write_constant_mono_wav(path, 0.3f, 1024, 22050));
    const int limit = 256;
    WavData wav = load_wav_file(path, 44100, limit);
    ASSERT_EQ(static_cast<int>(wav.samples.size()), limit);
    ASSERT_EQ(wav.sample_rate, 44100);
    for (float s : wav.samples)
        ASSERT_NEAR(s, 0.3f, 5e-3f);
}
TEST(WavLoader_Truncation_LimitEqualsLength_NoTruncation) {
    const std::string path = "tests/assets/wl_test_truncation_exact_match.wav";
    TempFile guard(path);
    const int N = 128;
    ASSERT_TRUE(write_constant_mono_wav(path, 0.2f, N, 44100));
    WavData wav = load_wav_file(path, 44100, N);
    ASSERT_EQ(static_cast<int>(wav.samples.size()), N);
}
TEST(WavLoader_Truncation_LimitOfOne) {
    const std::string path = "tests/assets/wl_test_truncation_one.wav";
    TempFile guard(path);
    ASSERT_TRUE(write_constant_mono_wav(path, 0.9f, 512, 48000));
    WavData wav = load_wav_file(path, 48000, 1);
    ASSERT_EQ(static_cast<int>(wav.samples.size()), 1);
    ASSERT_NEAR(wav.samples[0], 0.9f, 5e-3f);
}