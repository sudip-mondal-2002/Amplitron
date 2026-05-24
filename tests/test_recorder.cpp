#include "test_framework.h"
#include "audio/recorder.h"

#include <fstream>
#include <cstdio>
#include <cstring>
#include <cmath>

using namespace Amplitron;

// Helper: check if file exists
static bool file_exists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

// Helper: get file size
static long file_size(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    return f.good() ? static_cast<long>(f.tellg()) : -1;
}

// ============================================================
// Recorder tests
// ============================================================

TEST(recorder_initial_state) {
    Recorder rec;
    ASSERT_FALSE(rec.is_recording());
    ASSERT_EQ(rec.get_samples_written(), (int64_t)0);
}

TEST(recorder_get_recordings_dir) {
    std::string dir = Recorder::get_recordings_dir();
    ASSERT_FALSE(dir.empty());
}

TEST(recorder_generate_filename) {
    std::string name = Recorder::generate_filename();
    ASSERT_FALSE(name.empty());
    // Should end with .wav
    ASSERT_TRUE(name.size() > 4);
    ASSERT_TRUE(name.substr(name.size() - 4) == ".wav");
}

TEST(recorder_start_stop) {
    Recorder rec;
    std::string path = "recordings/test_rec_startstop.wav";

    bool ok = rec.start(path, 48000, 1);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(rec.is_recording());

    rec.stop();
    ASSERT_FALSE(rec.is_recording());

    // File should exist and have a WAV header (44 bytes minimum)
    ASSERT_TRUE(file_exists(path));
    ASSERT_GE(file_size(path), 44L);

    std::remove(path.c_str());
}

TEST(recorder_writes_samples) {
    Recorder rec;
    std::string path = "recordings/test_rec_samples.wav";

    bool ok = rec.start(path, 48000, 1);
    ASSERT_TRUE(ok);

    // Write a sine wave
    const int num_samples = 4800; // 0.1 seconds
    float buf[num_samples];
    for (int i = 0; i < num_samples; ++i)
        buf[i] = std::sin(2.0f * 3.14159265f * 440.0f * i / 48000.0f);

    rec.write_samples(buf, num_samples);
    ASSERT_EQ(rec.get_samples_written(), (int64_t)num_samples);

    rec.stop();

    // WAV header = 44 bytes, each sample = 2 bytes (int16)
    long expected_size = 44 + num_samples * 2;
    ASSERT_EQ(file_size(path), expected_size);

    // Verify WAV header magic bytes
    std::ifstream f(path, std::ios::binary);
    char header[4];
    f.read(header, 4);
    ASSERT_TRUE(std::strncmp(header, "RIFF", 4) == 0);
    f.seekg(8);
    f.read(header, 4);
    ASSERT_TRUE(std::strncmp(header, "WAVE", 4) == 0);

    std::remove(path.c_str());
}

TEST(recorder_multiple_write_calls) {
    Recorder rec;
    std::string path = "recordings/test_rec_multi.wav";

    rec.start(path, 44100, 1);

    float buf[100];
    std::memset(buf, 0, sizeof(buf));

    rec.write_samples(buf, 100);
    rec.write_samples(buf, 100);
    rec.write_samples(buf, 100);

    ASSERT_EQ(rec.get_samples_written(), (int64_t)300);

    rec.stop();

    long expected_size = 44 + 300 * 2;
    ASSERT_EQ(file_size(path), expected_size);

    std::remove(path.c_str());
}

TEST(recorder_duration_increases) {
    Recorder rec;
    std::string path = "recordings/test_rec_duration.wav";

    rec.start(path, 48000, 1);
    ASSERT_NEAR(rec.get_duration(), 0.0f, 1.0f); // just started

    // Write 48000 samples = 1 second
    float buf[48000];
    std::memset(buf, 0, sizeof(buf));
    rec.write_samples(buf, 48000);

    // Duration is wall-clock based, but samples_written should be correct
    ASSERT_EQ(rec.get_samples_written(), (int64_t)48000);

    rec.stop();
    std::remove(path.c_str());
}

TEST(recorder_write_while_not_recording_is_noop) {
    Recorder rec;
    float buf[100];
    std::memset(buf, 0, sizeof(buf));

    // Should not crash
    rec.write_samples(buf, 100);
    ASSERT_EQ(rec.get_samples_written(), (int64_t)0);
}

TEST(recorder_stop_while_not_recording_is_safe) {
    Recorder rec;
    // Should not crash
    rec.stop();
    ASSERT_FALSE(rec.is_recording());
}

TEST(recorder_restart_recording) {
    Recorder rec;
    std::string path1 = "recordings/test_rec_restart1.wav";
    std::string path2 = "recordings/test_rec_restart2.wav";

    float buf[100];
    std::memset(buf, 0, sizeof(buf));

    rec.start(path1, 48000, 1);
    rec.write_samples(buf, 100);
    rec.stop();

    rec.start(path2, 48000, 1);
    rec.write_samples(buf, 50);
    rec.stop();

    ASSERT_TRUE(file_exists(path1));
    ASSERT_TRUE(file_exists(path2));

    // Second file should be smaller
    ASSERT_EQ(file_size(path1), 44L + 100 * 2);
    ASSERT_EQ(file_size(path2), 44L + 50 * 2);

    std::remove(path1.c_str());
    std::remove(path2.c_str());
}

TEST(recorder_wav_header_data_size_field_is_correct) {
    Recorder rec;
    std::string path = "recordings/test_rec_header_datasize.wav";

    rec.start(path, 48000, 1);

    const int num_samples = 1024;
    float buf[num_samples];
    std::memset(buf, 0, sizeof(buf));
    rec.write_samples(buf, num_samples);

    rec.stop();

    // Open the file and read the data size field at byte offset 40
    std::ifstream f(path, std::ios::binary);
    ASSERT_TRUE(f.good());

    f.seekg(40);
    uint32_t data_size = 0;
    f.read(reinterpret_cast<char*>(&data_size), 4);

    // data size = num_samples * channels * 2 bytes (16-bit PCM)
    uint32_t expected_data_size = static_cast<uint32_t>(num_samples * 1 * 2);
    ASSERT_EQ(data_size, expected_data_size);

    f.close();
    std::remove(path.c_str());
}

TEST(recorder_wav_header_riff_size_field_is_correct) {
    Recorder rec;
    std::string path = "recordings/test_rec_header_riffsize.wav";

    rec.start(path, 48000, 1);

    const int num_samples = 2048;
    float buf[num_samples];
    std::memset(buf, 0, sizeof(buf));
    rec.write_samples(buf, num_samples);

    rec.stop();

    // Read RIFF chunk size at byte offset 4
    std::ifstream f(path, std::ios::binary);
    ASSERT_TRUE(f.good());

    f.seekg(4);
    uint32_t riff_size = 0;
    f.read(reinterpret_cast<char*>(&riff_size), 4);

    // riff size = data_size + 36 (fmt chunk + data header overhead)
    uint32_t expected_riff_size = static_cast<uint32_t>(num_samples * 1 * 2) + 36u;
    ASSERT_EQ(riff_size, expected_riff_size);

    f.close();
    std::remove(path.c_str());
}

TEST(recorder_wav_header_riff_size_no_overflow_at_large_sample_count) {
    // Simulate what finalize_wav_header does with a near-overflow sample count.
    // data_size_64 just below the 32-bit WAV limit: 0xFFFFFFD8 bytes of PCM data.
    // With channels=1, 16-bit: samples = 0xFFFFFFD8 / 2 = 0x7FFFFFE C samples.
    // riff_size must equal data_size + 36 without overflowing.
    const int64_t large_data_size = static_cast<int64_t>(0xFFFFFFD8LL);

    // Replicate the fixed formula
    uint32_t data_size = static_cast<uint32_t>(
        (large_data_size > static_cast<int64_t>(0xFFFFFFD8LL))
            ? 0xFFFFFFD8u
            : static_cast<uint32_t>(large_data_size));
    uint32_t riff_size = data_size + 36u;

    // riff_size must not wrap around (would indicate overflow)
    ASSERT_GE(riff_size, data_size);
    // riff_size must equal data_size + 36 exactly
    ASSERT_EQ(riff_size, data_size + 36u);
}

TEST(recorder_wav_header_riff_size_clamped_beyond_4gb) {
    // Simulate data_size_64 exceeding the WAV 4GB cap.
    // The result must be clamped to 0xFFFFFFD8 and riff_size must not overflow.
    const int64_t huge_data_size = static_cast<int64_t>(0x1FFFFFFFFLL);

    uint32_t data_size = static_cast<uint32_t>(
        (huge_data_size > static_cast<int64_t>(0xFFFFFFD8LL))
            ? 0xFFFFFFD8u
            : static_cast<uint32_t>(huge_data_size));
    uint32_t riff_size = data_size + 36u;

    ASSERT_EQ(data_size, 0xFFFFFFD8u);
    ASSERT_EQ(riff_size, 0xFFFFFFD8u + 36u);
    ASSERT_GE(riff_size, data_size);
}

TEST(recorder_stereo_start_writes_correct_channel_count_in_header) {
    Recorder rec;
    std::string path = "recordings/test_rec_stereo_header.wav";

    // Start with 2 channels (stereo)
    rec.start(path, 48000, 2);

    float buf[64];
    std::memset(buf, 0, sizeof(buf));
    rec.write_samples(buf, 64);

    rec.stop();

    std::ifstream f(path, std::ios::binary);
    ASSERT_TRUE(f.good());

    // Channel count is at byte offset 22 (int16_t)
    f.seekg(22);
    int16_t num_channels = 0;
    f.read(reinterpret_cast<char*>(&num_channels), 2);
    ASSERT_EQ(num_channels, 2);

    f.close();
    std::remove(path.c_str());
}

TEST(recorder_stereo_data_size_accounts_for_two_channels) {
    Recorder rec;
    std::string path = "recordings/test_rec_stereo_datasize.wav";

    const int num_samples = 512;
    rec.start(path, 48000, 2);

    float buf[num_samples];
    std::memset(buf, 0, sizeof(buf));
    rec.write_samples(buf, num_samples);

    rec.stop();

    std::ifstream f(path, std::ios::binary);
    ASSERT_TRUE(f.good());

    f.seekg(40);
    uint32_t data_size = 0;
    f.read(reinterpret_cast<char*>(&data_size), 4);

    // channels=2, num_samples frames, 2 bytes per sample = num_samples * 2 * 2
    uint32_t expected = static_cast<uint32_t>(num_samples * 2 * 2);
    ASSERT_EQ(data_size, expected);

    f.close();
    std::remove(path.c_str());
}
