#include "audio/recorder.h"
#include "audio/audio_engine.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <ctime>
#include <cstring>

namespace Amplitron {

void Recorder::write_wav_header() {
    // Write a placeholder WAV header (44 bytes)
    // Will be finalized when recording stops
    char header[44];
    std::memset(header, 0, 44);

    int byte_rate = sample_rate_ * channels_ * 2; // 16-bit = 2 bytes
    int block_align = channels_ * 2;

    // RIFF header
    std::memcpy(header + 0, "RIFF", 4);
    // Chunk size (placeholder — filled in finalize)
    std::memcpy(header + 8, "WAVE", 4);

    // fmt sub-chunk
    std::memcpy(header + 12, "fmt ", 4);
    int fmt_size = 16;
    std::memcpy(header + 16, &fmt_size, 4);
    int16_t audio_format = 1; // PCM
    std::memcpy(header + 20, &audio_format, 2);
    int16_t num_channels = static_cast<int16_t>(channels_);
    std::memcpy(header + 22, &num_channels, 2);
    std::memcpy(header + 24, &sample_rate_, 4);
    std::memcpy(header + 28, &byte_rate, 4);
    int16_t block_align_16 = static_cast<int16_t>(block_align);
    std::memcpy(header + 32, &block_align_16, 2);
    int16_t bits_per_sample = 16;
    std::memcpy(header + 34, &bits_per_sample, 2);

    // data sub-chunk
    std::memcpy(header + 36, "data", 4);
    // Data size (placeholder — filled in finalize)

    file_.write(header, 44);
}

void Recorder::finalize_wav_header() {
    if (!file_.is_open()) return;

    int64_t total_samples = samples_written_.load();
    // Compute data size in 64-bit to avoid overflow
    int64_t data_size_64 = total_samples * static_cast<int64_t>(channels_) * 2;
    // WAV format uses 32-bit sizes; clamp to max int32
    int data_size = static_cast<int>(
        (data_size_64 > 0x7FFFFFFF) ? 0x7FFFFFFF : data_size_64);
    int riff_size = data_size + 36;

    // Seek back and write correct sizes
    file_.seekp(4, std::ios::beg);
    file_.write(reinterpret_cast<const char*>(&riff_size), 4);

    file_.seekp(40, std::ios::beg);
    file_.write(reinterpret_cast<const char*>(&data_size), 4);

    // Seek to end
    file_.seekp(0, std::ios::end);
}

void Recorder::write_metadata(const std::string& wav_path, AudioEngine& engine) {
    // Write a JSON sidecar file with recording details
    std::string meta_path = wav_path;
    // Replace .wav with .meta.json
    size_t dot = meta_path.rfind('.');
    if (dot != std::string::npos) {
        meta_path = meta_path.substr(0, dot);
    }
    meta_path += ".meta.json";

    std::ofstream meta(meta_path);
    if (!meta.is_open()) {
        std::cerr << "Could not write metadata: " << meta_path << std::endl;
        return;
    }

    std::time_t now = std::time(nullptr);
    char timebuf[64];
    std::tm time_info;
#ifdef _WIN32
    localtime_s(&time_info, &now);
#else
    localtime_r(&now, &time_info);
#endif
    std::strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%S", &time_info);

    float duration = get_duration();

    // Build the recording metadata object
    nlohmann::ordered_json recording = nlohmann::ordered_json::object();
    recording["filename"]         = wav_path;
    recording["recorded_at"]      = timebuf;
    recording["duration_seconds"] = duration;
    recording["total_samples"]    = samples_written_.load();
    recording["format"]           = "WAV PCM 16-bit";
    recording["sample_rate"]      = sample_rate_;
    recording["channels"]         = channels_;
    recording["bit_depth"]        = 16;

    // Build the audio settings object
    nlohmann::ordered_json audio_settings = nlohmann::ordered_json::object();
    audio_settings["input_device"]       = engine.get_input_device_name();
    audio_settings["output_device"]      = engine.get_output_device_name();
    audio_settings["engine_sample_rate"] = engine.get_sample_rate();
    audio_settings["buffer_size"]        = engine.get_buffer_size();
    audio_settings["input_gain"]         = engine.get_input_gain();
    audio_settings["output_gain"]        = engine.get_output_gain();

    // Build the signal chain array
    nlohmann::ordered_json signal_chain = nlohmann::ordered_json::array();
    for (auto& fx : engine.effects()) {
        nlohmann::ordered_json jfx = nlohmann::ordered_json::object();
        jfx["name"]    = fx->name();
        jfx["enabled"] = fx->is_enabled();
        jfx["mix"]     = fx->get_mix();

        nlohmann::ordered_json params_obj = nlohmann::ordered_json::object();
        for (auto& p : fx->params()) {
            params_obj[p.name] = p.value;
        }
        jfx["parameters"] = std::move(params_obj);

        signal_chain.push_back(std::move(jfx));
    }

    // Assemble root object
    nlohmann::ordered_json root = nlohmann::ordered_json::object();
    root["recording"]      = std::move(recording);
    root["audio_settings"] = std::move(audio_settings);
    root["signal_chain"]   = std::move(signal_chain);

    meta << root.dump(2) << "\n";
    meta.close();
    std::cout << "Metadata written: " << meta_path << std::endl;
}

} // namespace Amplitron

