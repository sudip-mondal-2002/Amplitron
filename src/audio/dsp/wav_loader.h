#pragma once

#include <string>
#include <vector>

namespace Amplitron {

struct WavData {
    std::vector<float> samples;  // mono, normalized to [-1, 1]
    int sample_rate = 0;
    int channels = 0;
};

// Load a WAV file, mix down to mono, optionally resample to target_sample_rate.
// Returns empty WavData (samples.empty()) on failure.
// Caps IR length at max_length_samples if > 0.
WavData load_wav_file(const std::string& filepath,
                      int target_sample_rate = 0,
                      int max_length_samples = 0);

// Simple linear-interpolation resampler (mono only).
std::vector<float> resample_linear(const std::vector<float>& input,
                                   int from_rate, int to_rate);

// Validates that a path is safe to open as an IR file loaded from an
// untrusted source (e.g. a preset file downloaded from the internet).
//
// Rejects:
//   - empty paths
//   - UNC paths: \\server\share or //server/share  (Windows NTLM leak vector)
//   - any path component equal to ".."             (directory traversal)
//   - when allowed_dir is non-empty: any path whose weakly-canonical form
//     does not begin with the weakly-canonical form of allowed_dir
//
// Returns false on any std::filesystem::filesystem_error.
bool is_safe_ir_path(const std::string& path,
                     const std::string& allowed_dir = "");

} // namespace Amplitron
