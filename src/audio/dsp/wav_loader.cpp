#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#include "audio/dsp/wav_loader.h"
#include <iostream>
#include <cmath>

namespace Amplitron {

std::vector<float> resample_linear(const std::vector<float>& input,
                                   int from_rate, int to_rate) {
    if (input.empty() || from_rate <= 0 || to_rate <= 0 || from_rate == to_rate)
        return input;

    double ratio = static_cast<double>(from_rate) / static_cast<double>(to_rate);
    size_t out_len = static_cast<size_t>(
        std::ceil(static_cast<double>(input.size()) / ratio));
    if (out_len == 0) return {};

    std::vector<float> output(out_len);
    for (size_t i = 0; i < out_len; ++i) {
        double src_idx = static_cast<double>(i) * ratio;
        size_t idx0 = static_cast<size_t>(src_idx);
        double frac = src_idx - static_cast<double>(idx0);

        if (idx0 + 1 < input.size()) {
            output[i] = static_cast<float>(
                input[idx0] * (1.0 - frac) + input[idx0 + 1] * frac);
        } else if (idx0 < input.size()) {
            output[i] = input[idx0];
        } else {
            output[i] = 0.0f;
        }
    }
    return output;
}

WavData load_wav_file(const std::string& filepath,
                      int target_sample_rate,
                      int max_length_samples) {
    WavData result;

    drwav wav;
    if (!drwav_init_file(&wav, filepath.c_str(), nullptr)) {
        std::cerr << "Cabinet IR: failed to open WAV file: " << filepath << std::endl;
        return result;
    }

    result.sample_rate = static_cast<int>(wav.sampleRate);
    result.channels = static_cast<int>(wav.channels);

    drwav_uint64 total_frames = wav.totalPCMFrameCount;
    if (total_frames == 0) {
        std::cerr << "Cabinet IR: WAV file is empty: " << filepath << std::endl;
        drwav_uninit(&wav);
        return result;
    }

    // Bound frame count to max_length_samples before allocation
    drwav_uint64 frames_to_read = std::min(total_frames,
        static_cast<drwav_uint64>(max_length_samples));

    // Read bounded frames as interleaved float
    std::vector<float> interleaved(static_cast<size_t>(frames_to_read * wav.channels));
    drwav_uint64 frames_read = drwav_read_pcm_frames_f32(&wav, frames_to_read,
                                                          interleaved.data());
    drwav_uninit(&wav);

    if (frames_read == 0) {
        std::cerr << "Cabinet IR: failed to read WAV data: " << filepath << std::endl;
        result.sample_rate = 0;
        return result;
    }

    // Mix down to mono
    size_t num_frames = static_cast<size_t>(frames_read);
    result.samples.resize(num_frames);

    if (result.channels == 1) {
        result.samples.assign(interleaved.begin(),
                              interleaved.begin() + static_cast<ptrdiff_t>(num_frames));
    } else {
        int ch = result.channels;
        for (size_t f = 0; f < num_frames; ++f) {
            float sum = 0.0f;
            for (int c = 0; c < ch; ++c)
                sum += interleaved[f * static_cast<size_t>(ch) + static_cast<size_t>(c)];
            result.samples[f] = sum / static_cast<float>(ch);
        }
    }

    // Resample if needed
    if (target_sample_rate > 0 && target_sample_rate != result.sample_rate) {
        result.samples = resample_linear(result.samples,
                                         result.sample_rate, target_sample_rate);
        result.sample_rate = target_sample_rate;
    }

    // Cap length
    if (max_length_samples > 0 &&
        static_cast<int>(result.samples.size()) > max_length_samples) {
        std::cerr << "Cabinet IR: IR truncated from "
                  << result.samples.size() << " to " << max_length_samples
                  << " samples" << std::endl;
        result.samples.resize(static_cast<size_t>(max_length_samples));
    }

    return result;
}

} // namespace Amplitron
