#include "audio/effects/reverb.h"
#include "audio/effects/effect_factory.h"

namespace Amplitron {

static EffectRegistrar<Reverb> reg("Reverb");

// Comb filter delay lengths (in samples at 44100Hz, will be scaled)
static const int COMB_LENGTHS[]   = {1116, 1188, 1277, 1356};
static const int ALLPASS_LENGTHS[] = {556, 441};
// Right-channel lengths add a prime offset for decorrelation (classic Freeverb technique)
static constexpr int STEREO_SPREAD = 23;
static const int COMB_LENGTHS_R[]   = {1116 + STEREO_SPREAD, 1188 + STEREO_SPREAD,
                                        1277 + STEREO_SPREAD, 1356 + STEREO_SPREAD};
static const int ALLPASS_LENGTHS_R[] = {556 + STEREO_SPREAD, 441 + STEREO_SPREAD};

Reverb::Reverb() {
    params_ = {
        {"Decay",  0.6f, 0.1f, 0.99f, 0.6f, "", "Length of the reverb tail. Higher values simulate larger acoustic spaces like halls or caves."},
        {"Damp",   0.4f, 0.0f, 1.0f,  0.4f, "", "High-frequency damping. Higher values absorb treble faster, simulating softer room materials."},
        {"Level",  0.3f, 0.0f, 1.0f,  0.3f, "", "Mix volume of the reverb effect. Controls how wet or distant the overall sound feels."},
    };
    init_filters();
}

void Reverb::set_sample_rate(int sample_rate) {
    Effect::set_sample_rate(sample_rate);
    init_filters();
}

void Reverb::init_filters() {
    const float scale = static_cast<float>(sample_rate_) / 44100.0f;

    for (int i = 0; i < NUM_COMBS; ++i) {
        int len = static_cast<int>(COMB_LENGTHS[i] * scale);
        combs_[i].buffer.assign(len, 0.0f);
        combs_[i].write_pos = 0;
        combs_[i].lp_state = 0.0f;

        int len_r = static_cast<int>(COMB_LENGTHS_R[i] * scale);
        combs_r_[i].buffer.assign(len_r, 0.0f);
        combs_r_[i].write_pos = 0;
        combs_r_[i].lp_state = 0.0f;
    }

    for (int i = 0; i < NUM_ALLPASS; ++i) {
        int len = static_cast<int>(ALLPASS_LENGTHS[i] * scale);
        allpasses_[i].buffer.assign(len, 0.0f);
        allpasses_[i].write_pos = 0;
        allpasses_[i].feedback = 0.5f;

        int len_r = static_cast<int>(ALLPASS_LENGTHS_R[i] * scale);
        allpasses_r_[i].buffer.assign(len_r, 0.0f);
        allpasses_r_[i].write_pos = 0;
        allpasses_r_[i].feedback = 0.5f;
    }
}

void Reverb::process(float* buffer, int num_samples) {
    if (!enabled_) return;

    float decay = params_[0].value;
    float damp = params_[1].value;
    float level = params_[2].value;

    for (int i = 0; i < num_samples; ++i) {
        float dry = buffer[i];
        float input = buffer[i] * 0.2f;
        float out = 0.0f;

        // Parallel comb filters
        for (int c = 0; c < NUM_COMBS; ++c) {
            auto& comb = combs_[c];
            int buf_len = static_cast<int>(comb.buffer.size());
            int read_pos = comb.write_pos;

            float delayed = comb.buffer[read_pos];

            // Damping LP filter
            comb.lp_state = delayed * (1.0f - damp) + comb.lp_state * damp;
            float fb_sample = comb.lp_state * decay;

            comb.buffer[comb.write_pos] = input + fb_sample;
            comb.write_pos = (comb.write_pos + 1) % buf_len;

            out += delayed;
        }

        // Series allpass filters
        for (int a = 0; a < NUM_ALLPASS; ++a) {
            auto& ap = allpasses_[a];
            int buf_len = static_cast<int>(ap.buffer.size());

            float delayed = ap.buffer[ap.write_pos];
            float temp = out + delayed * ap.feedback;
            ap.buffer[ap.write_pos] = temp;
            out = delayed - out * ap.feedback;

            ap.write_pos = (ap.write_pos + 1) % buf_len;
        }

        buffer[i] = dry * (1.0f - level) + out * level;
    }
}

void Reverb::process_stereo(float* left, float* right, int num_samples) {
    if (!enabled_) {
        return;
    }

    const float decay = params_[0].value;
    const float damp  = params_[1].value;
    const float level = params_[2].value;

    for (int i = 0; i < num_samples; ++i) {
        const float input_l = left[i]  * 0.2f;
        const float input_r = right[i] * 0.2f;
        float out_l = 0.0f;
        float out_r = 0.0f;

        // Parallel comb filters — left
        for (int c = 0; c < NUM_COMBS; ++c) {
            auto& comb = combs_[c];
            const int buf_len = static_cast<int>(comb.buffer.size());
            float delayed = comb.buffer[comb.write_pos];
            comb.lp_state = delayed * (1.0f - damp) + comb.lp_state * damp;
            comb.buffer[comb.write_pos] = input_l + comb.lp_state * decay;
            comb.write_pos = (comb.write_pos + 1) % buf_len;
            out_l += delayed;
        }
        // Parallel comb filters — right
        for (int c = 0; c < NUM_COMBS; ++c) {
            auto& comb = combs_r_[c];
            const int buf_len = static_cast<int>(comb.buffer.size());
            float delayed = comb.buffer[comb.write_pos];
            comb.lp_state = delayed * (1.0f - damp) + comb.lp_state * damp;
            comb.buffer[comb.write_pos] = input_r + comb.lp_state * decay;
            comb.write_pos = (comb.write_pos + 1) % buf_len;
            out_r += delayed;
        }

        // Series allpass filters — left
        for (int a = 0; a < NUM_ALLPASS; ++a) {
            auto& ap = allpasses_[a];
            const int buf_len = static_cast<int>(ap.buffer.size());
            float delayed = ap.buffer[ap.write_pos];
            float temp = out_l + delayed * ap.feedback;
            ap.buffer[ap.write_pos] = temp;
            out_l = delayed - out_l * ap.feedback;
            ap.write_pos = (ap.write_pos + 1) % buf_len;
        }
        // Series allpass filters — right
        for (int a = 0; a < NUM_ALLPASS; ++a) {
            auto& ap = allpasses_r_[a];
            const int buf_len = static_cast<int>(ap.buffer.size());
            float delayed = ap.buffer[ap.write_pos];
            float temp = out_r + delayed * ap.feedback;
            ap.buffer[ap.write_pos] = temp;
            out_r = delayed - out_r * ap.feedback;
            ap.write_pos = (ap.write_pos + 1) % buf_len;
        }

        left[i]  = left[i]  * (1.0f - level) + out_l * level;
        right[i] = right[i] * (1.0f - level) + out_r * level;
    }
}

void Reverb::reset() {
    for (auto& c : combs_) {
        std::fill(c.buffer.begin(), c.buffer.end(), 0.0f);
        c.write_pos = 0;
        c.lp_state = 0.0f;
    }
    for (auto& c : combs_r_) {
        std::fill(c.buffer.begin(), c.buffer.end(), 0.0f);
        c.write_pos = 0;
        c.lp_state = 0.0f;
    }
    for (auto& a : allpasses_) {
        std::fill(a.buffer.begin(), a.buffer.end(), 0.0f);
        a.write_pos = 0;
    }
    for (auto& a : allpasses_r_) {
        std::fill(a.buffer.begin(), a.buffer.end(), 0.0f);
        a.write_pos = 0;
    }
}

} // namespace Amplitron
