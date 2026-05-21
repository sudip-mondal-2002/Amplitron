#include "audio/audio_engine.h"
#include "audio/audio_backend.h"
#include <iostream>
#include <algorithm>
#include <nlohmann/json.hpp>

namespace Amplitron {

AudioEngine::AudioEngine() {
    process_buffer_.resize(MAX_BUFFER_SIZE, 0.0f);
    process_buffer_right_.resize(MAX_BUFFER_SIZE, 0.0f);
    backend_ = create_audio_backend();
}

AudioEngine::~AudioEngine() {
    shutdown();
    
    destroy_audio_backend(backend_);
    backend_ = nullptr;
}

// --- Serialization Methods ---


nlohmann::json AudioEngine::serialize() {
    std::lock_guard<std::mutex> lock(effect_mutex_);
    nlohmann::json j;
    
    // Read atomic variables safely
    j["input_gain"] = input_gain_.load(std::memory_order_relaxed);
    
    auto effects_array = nlohmann::json::array();
    for (const auto& fx : effects_) {
        if (fx) {
            effects_array.push_back({
                {"name", fx->name()}, // Corrected from get_name()
                {"params", fx->get_params()}
            });
        }
    }
    j["effects"] = effects_array;
    return j;
}

void AudioEngine::deserialize(const nlohmann::json& j) {
    std::lock_guard<std::mutex> lock(effect_mutex_);
    
    if (j.contains("input_gain")) {
        set_input_gain(j["input_gain"]);
    }
    
    if (j.contains("effects")) {
        for (const auto& fx_data : j["effects"]) {
            std::string name = fx_data["name"];
            for (auto& fx : effects_) {
                if (fx && std::string(fx->name()) == name) {
                    fx->set_params(fx_data["params"]);
                }
            }
        }
    }
}

// --- Existing Methods ---

void AudioEngine::set_buffer_size(int size) {
    size = std::max(MIN_BUFFER_SIZE, std::min(MAX_BUFFER_SIZE, size));
    int prev_size = buffer_size_;
    bool was_running = running_;
    if (was_running) stop();
    buffer_size_ = size;
    if (was_running) {
        if (!start()) {
            last_error_ = "Failed with buffer size " + std::to_string(size) + ". Reverting.";
            std::cerr << "[Amplitron] " << last_error_ << std::endl;
            buffer_size_ = prev_size;
            start();
        } else {
            last_error_.clear();
        }
    }
}

void AudioEngine::set_sample_rate(int rate) {
    int prev_rate = sample_rate_;
    bool was_running = running_;
    if (was_running) stop();
    sample_rate_ = rate;
    {
        std::lock_guard<std::mutex> lock(effect_mutex_);
        for (auto& fx : effects_) {
            fx->set_sample_rate(rate);
            fx->reset();
        }
        if (tuner_tap_) {
            tuner_tap_->set_sample_rate(rate);
            tuner_tap_->reset();
        }
    }
    if (was_running) {
        if (!start()) {
            last_error_ = "Failed with sample rate " + std::to_string(rate) + " Hz. Reverting.";
            std::cerr << "[Amplitron] " << last_error_ << std::endl;
            sample_rate_ = prev_rate;
            std::lock_guard<std::mutex> lock(effect_mutex_);
            for (auto& fx : effects_) {
                fx->set_sample_rate(prev_rate);
                fx->reset();
            }
            if (tuner_tap_) {
                tuner_tap_->set_sample_rate(prev_rate);
                tuner_tap_->reset();
            }
            start();
        } else {
            last_error_.clear();
        }
    }
}
} // namespace Amplitron
