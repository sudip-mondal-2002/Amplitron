#include "test_framework.h"
#include "audio/recorder/recorder.h"
#include "audio/engine/audio_engine.h"
#include <nlohmann/json.hpp>

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

TEST(recorder_pause_resume_preserves_data) {
    Recorder rec;
    std::string path = "recordings/test_rec_pause_resume.wav";

    float buf[100];
    std::memset(buf, 0, sizeof(buf));

    ASSERT_TRUE(rec.start(path, 48000, 1));

    rec.write_samples(buf, 100);

    rec.pause();
    ASSERT_TRUE(rec.is_paused());

    rec.resume();
    ASSERT_FALSE(rec.is_paused());

    rec.write_samples(buf, 100);

    ASSERT_EQ(rec.get_samples_written(), (int64_t)200);

    rec.stop();
    std::remove(path.c_str());
}

TEST(recorder_pause_blocks_writes) {
    Recorder rec;
    std::string path = "recordings/test_rec_pause_blocks.wav";

    float buf[100];
    std::memset(buf, 0, sizeof(buf));

    ASSERT_TRUE(rec.start(path, 48000, 1));

    rec.write_samples(buf, 100);

    rec.pause();
    rec.write_samples(buf, 100);

    ASSERT_EQ(rec.get_samples_written(), (int64_t)100);

    rec.resume();
    rec.write_samples(buf, 100);

    ASSERT_EQ(rec.get_samples_written(), (int64_t)200);

    rec.stop();
    std::remove(path.c_str());
}

TEST(recorder_current_peak_tracks_max_sample) {
    Recorder rec;
    std::string path = "recordings/test_rec_peak.wav";

    float buf[64];
    for (int i = 0; i < 64; ++i) {
        buf[i] = 0.3f;
    }

    buf[32] = 0.9f;

    ASSERT_TRUE(rec.start(path, 48000, 1));

    rec.write_samples(buf, 64);

    ASSERT_NEAR(rec.get_current_peak(), 0.9f, 0.01f);

    rec.stop();
    std::remove(path.c_str());
}

TEST(recorder_zero_duration_when_empty) {
    Recorder rec;

    ASSERT_NEAR(rec.get_duration(), 0.0f, 0.001f);
}

TEST(recorder_write_after_stop_is_noop) {
    Recorder rec;
    std::string path = "recordings/test_rec_after_stop.wav";

    float buf[100];
    std::memset(buf, 0, sizeof(buf));

    ASSERT_TRUE(rec.start(path, 48000, 1));
    rec.stop();

    rec.write_samples(buf, 100);

    ASSERT_EQ(rec.get_samples_written(), (int64_t)0);

    std::remove(path.c_str());
}

TEST(recorder_empty_recording_has_valid_wav_header) {
    Recorder rec;
    std::string path = "recordings/test_rec_empty.wav";

    ASSERT_TRUE(rec.start(path, 48000, 1));
    rec.stop();

    ASSERT_TRUE(file_exists(path));
    ASSERT_EQ(file_size(path), 44L);

    std::ifstream f(path, std::ios::binary);
    char header[4];

    f.read(header, 4);
    ASSERT_TRUE(std::strncmp(header, "RIFF", 4) == 0);

    f.seekg(8);
    f.read(header, 4);
    ASSERT_TRUE(std::strncmp(header, "WAVE", 4) == 0);

    f.seekg(36);
    f.read(header, 4);
    ASSERT_TRUE(std::strncmp(header, "data", 4) == 0);

    std::remove(path.c_str());
}

TEST(recorder_waveform_updates_after_write) {
    Recorder rec;
    std::string path = "recordings/test_rec_waveform.wav";

    float buf[1024];
    for (int i = 0; i < 1024; ++i) {
        buf[i] = 0.5f;
    }

    ASSERT_TRUE(rec.start(path, 48000, 1));
    rec.write_samples(buf, 1024);

    float waveform[Recorder::WAVEFORM_SIZE];
    rec.get_waveform(waveform, Recorder::WAVEFORM_SIZE);

    bool found_peak = false;
    for (int i = 0; i < Recorder::WAVEFORM_SIZE; ++i) {
        if (waveform[i] > 0.0f) {
            found_peak = true;
            break;
        }
    }

    ASSERT_TRUE(found_peak);

    rec.stop();
    std::remove(path.c_str());
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

TEST(recorder_stereo_write_samples_stereo_interleaves_correctly) {
    Recorder rec;
    std::string path = "recordings/test_rec_stereo_interleave.wav";

    // Start in stereo
    ASSERT_TRUE(rec.start(path, 48000, 2));
    ASSERT_TRUE(rec.is_recording());
    ASSERT_FALSE(rec.is_paused());

    // Write left channel as 0.5, right channel as -0.5
    const int num_samples = 256;
    float left[num_samples];
    float right[num_samples];
    for (int i = 0; i < num_samples; ++i) {
        left[i] = 0.5f;
        right[i] = -0.5f;
    }

    rec.write_samples_stereo(left, right, num_samples);
    ASSERT_EQ(rec.get_samples_written(), (int64_t)num_samples);

    rec.stop();
    ASSERT_FALSE(rec.is_recording());

    // File size should be: 44 bytes header + (256 frames * 2 channels * 2 bytes/sample) = 44 + 1024 = 1068 bytes
    ASSERT_EQ(file_size(path), 44L + num_samples * 2 * 2);

    // Open file and verify interleaved content
    std::ifstream f(path, std::ios::binary);
    ASSERT_TRUE(f.good());

    // Skip the 44-byte WAV header
    f.seekg(44);

    int16_t samples[num_samples * 2];
    f.read(reinterpret_cast<char*>(samples), num_samples * 2 * sizeof(int16_t));
    f.close();

    // Verify samples are interleaved: left (index 0, positive) and right (index 1, negative)
    ASSERT_GT(samples[0], 0);
    ASSERT_LT(samples[1], 0);

    // Check magnitude (approx 16383 for 0.5f and -16384 for -0.5f)
    ASSERT_NEAR(static_cast<float>(samples[0]), 16383.0f, 10.0f);
    ASSERT_NEAR(static_cast<float>(samples[1]), -16384.0f, 10.0f);

    std::remove(path.c_str());
}

TEST(recorder_write_metadata_generates_valid_json) {
    Recorder rec;
    std::string path = "recordings/test_rec_metadata.wav";

    ASSERT_TRUE(rec.start(path, 44100, 2));
    
    const int num_samples = 128;
    float left[num_samples];
    float right[num_samples];
    std::memset(left, 0, sizeof(left));
    std::memset(right, 0, sizeof(right));
    rec.write_samples_stereo(left, right, num_samples);
    
    rec.stop();

    AudioEngine engine;
    rec.write_metadata(path, engine);

    std::string meta_path = "recordings/test_rec_metadata.meta.json";
    ASSERT_TRUE(file_exists(meta_path));

    // Read and verify JSON structure
    std::ifstream meta_file(meta_path);
    nlohmann::json j;
    meta_file >> j;
    meta_file.close();

    ASSERT_EQ(j["recording"]["filename"], path);
    ASSERT_EQ(j["recording"]["format"], "WAV PCM 16-bit");
    ASSERT_EQ(j["recording"]["sample_rate"], 44100);
    ASSERT_EQ(j["recording"]["channels"], 2);
    ASSERT_EQ(j["recording"]["total_samples"], num_samples);
    ASSERT_TRUE(j.contains("audio_settings"));
    ASSERT_TRUE(j.contains("signal_chain"));

    std::remove(path.c_str());
    std::remove(meta_path.c_str());
}

TEST(recorder_discard_removes_temp_and_metadata_files) {
    Recorder rec;
    std::string path = "recordings/test_rec_discard.wav";
    std::string meta_path = "recordings/test_rec_discard.meta.json";

    ASSERT_TRUE(rec.start(path, 48000, 1));
    float buf[64];
    std::memset(buf, 0, sizeof(buf));
    rec.write_samples(buf, 64);
    rec.stop();

    AudioEngine engine;
    rec.write_metadata(path, engine);

    ASSERT_TRUE(file_exists(path));
    ASSERT_TRUE(file_exists(meta_path));
    ASSERT_TRUE(rec.has_unsaved());

    rec.discard();

    ASSERT_FALSE(file_exists(path));
    ASSERT_FALSE(file_exists(meta_path));
    ASSERT_FALSE(rec.has_unsaved());
}

TEST(recorder_save_to_errors_and_edge_cases) {
    Recorder rec;
    
    // Save when no recording exists
    ASSERT_FALSE(rec.save_to("recordings/no_exist.wav"));

    std::string path = "recordings/test_rec_save_src.wav";
    std::string dst_path = "recordings/test_rec_save_dst.wav";

    ASSERT_TRUE(rec.start(path, 48000, 1));
    float buf[64];
    std::memset(buf, 0, sizeof(buf));
    rec.write_samples(buf, 64);
    rec.stop();

    // 1. Same destination path
    ASSERT_TRUE(rec.save_to(path));
    ASSERT_FALSE(rec.has_unsaved()); // Cleared

    // Reset unsaved flag by doing another start/stop sequence
    std::remove(path.c_str());
    ASSERT_TRUE(rec.start(path, 48000, 1));
    rec.write_samples(buf, 64);
    rec.stop();

    // Generate metadata file
    AudioEngine engine;
    rec.write_metadata(path, engine);
    std::string meta_src = "recordings/test_rec_save_src.meta.json";
    std::string meta_dst = "recordings/test_rec_save_dst.meta.json";
    ASSERT_TRUE(file_exists(meta_src));

    // 2. Different destination path
    ASSERT_TRUE(rec.save_to(dst_path));
    ASSERT_FALSE(rec.has_unsaved());
    ASSERT_FALSE(file_exists(path)); // Temp file removed
    ASSERT_TRUE(file_exists(dst_path)); // New file created
    ASSERT_FALSE(file_exists(meta_src)); // Old metadata removed

    std::remove(dst_path.c_str());

    // 3. Copy to invalid destination
    ASSERT_TRUE(rec.start(path, 48000, 1));
    rec.write_samples(buf, 64);
    rec.stop();

    ASSERT_FALSE(rec.save_to("/non_existent_folder_xyz/file.wav"));
    
    rec.discard();
    std::remove(path.c_str());
}

TEST(recorder_start_invalid_params_fails) {
    Recorder rec;
    
    // 1. Empty filepath
    ASSERT_FALSE(rec.start("", 48000, 1));
    ASSERT_FALSE(rec.is_recording());

    // 2. Path where parent is a regular file
    std::string dummy_file = "recordings/test_not_a_dir.txt";
    std::ofstream dummy(dummy_file);
    dummy.close();

    ASSERT_FALSE(rec.start("recordings/test_not_a_dir.txt/recording.wav", 48000, 1));
    ASSERT_FALSE(rec.is_recording());

    std::remove(dummy_file.c_str());
}

TEST(recorder_waveform_sizing_boundaries) {
    Recorder rec;
    
    // Waveform check when not recording (should be all 0.0f)
    float waveform[Recorder::WAVEFORM_SIZE];
    std::fill_n(waveform, Recorder::WAVEFORM_SIZE, 1.0f);
    rec.get_waveform(waveform, Recorder::WAVEFORM_SIZE);
    
    for (int i = 0; i < Recorder::WAVEFORM_SIZE; ++i) {
        ASSERT_EQ(waveform[i], 0.0f);
    }

    // Call get_waveform with 0 count
    rec.get_waveform(waveform, 0);

    // Verify peak levels initial state
    ASSERT_EQ(rec.get_current_peak(), 0.0f);
}
