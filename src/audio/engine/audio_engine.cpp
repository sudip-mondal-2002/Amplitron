#include "audio/engine/audio_engine.h"
#include "audio/backend/audio_backend.h"
#include <iostream>
#include <algorithm>
#include <nlohmann/json.hpp>

namespace Amplitron
{

    AudioEngine::AudioEngine()
    {
        process_buffer_.resize(MAX_BUFFER_SIZE, 0.0f);
        process_buffer_right_.resize(MAX_BUFFER_SIZE, 0.0f);
        backend_ = create_audio_backend();

        // Pre-allocate the graph memory pools immediately on startup
        main_executor_ = std::make_shared<AudioGraphExecutor>();
        // Assuming standard values, use your engine's actual sample rate / block size variables here
        main_executor_->prepare(48000, 512, 32);

        // Seed the shadow executor so the audio thread has something safe to read instantly
        audio_shadow_executor_ = main_executor_;
    }

    AudioEngine::~AudioEngine()
    {
        shutdown();

        destroy_audio_backend(backend_);
        backend_ = nullptr;
    }

    // --- Serialization Methods ---

    nlohmann::json AudioEngine::serialize()
    {
        std::lock_guard<std::mutex> lock(effect_mutex_);
        nlohmann::json j;

        // Read atomic variables safely
        j["input_gain"] = input_gain_.load(std::memory_order_relaxed);

        auto effects_array = nlohmann::json::array();
        for (const auto &fx : dummy_effects_)
        {
            if (fx)
            {
                nlohmann::json fx_json;
                fx_json["name"] = fx->name();
                fx_json["params"] = fx->get_params();
                effects_array.push_back(fx_json);
            }
        }
        j["effects"] = effects_array;
        return j;
    }

    void AudioEngine::deserialize(const nlohmann::json &j)
    {
        std::lock_guard<std::mutex> lock(effect_mutex_);

        if (j.contains("input_gain"))
        {
            set_input_gain(j["input_gain"]);
        }

        if (j.contains("effects"))
        {
            for (const auto &fx_data : j["effects"])
            {
                std::string name = fx_data["name"];
                for (auto &fx : dummy_effects_)
                {
                    if (fx && std::string(fx->name()) == name)
                    {
                        fx->set_params(fx_data["params"]);
                    }
                }
            }
        }
    }

    // --- Existing Methods ---

    void AudioEngine::set_buffer_size(int size)
    {
        size = std::max(MIN_BUFFER_SIZE, std::min(MAX_BUFFER_SIZE, size));
        int prev_size = buffer_size_;
        bool was_running = running_;
        if (was_running)
            stop();
        buffer_size_ = size;
        if (was_running)
        {
            if (!start())
            {
                last_error_ = "Failed with buffer size " + std::to_string(size) + ". Reverting.";
                std::cerr << "[Amplitron] " << last_error_ << std::endl;
                buffer_size_ = prev_size;
                start();
            }
            else
            {
                last_error_.clear();
            }
        }
    }

    void AudioEngine::set_sample_rate(int rate)
    {
        int prev_rate = sample_rate_;
        bool was_running = running_;
        if (was_running)
            stop();
        sample_rate_ = rate;

        {
            std::lock_guard<std::mutex> lock(effect_mutex_);
            // FIX: Iterate over the nodes in the new AudioGraph
            for (const auto &node : main_graph_.get_nodes())
            {
                if (node.pedal)
                { // Check if it's a standard effect and not a bare merge node
                    node.pedal->set_sample_rate(rate);
                    node.pedal->reset();
                }
            }
            if (tuner_tap_)
            {
                tuner_tap_->set_sample_rate(rate);
                tuner_tap_->reset();
            }
        }

        if (was_running)
        {
            if (!start())
            {
                last_error_ = "Failed with sample rate " + std::to_string(rate) + " Hz. Reverting.";
                std::cerr << "[Amplitron] " << last_error_ << std::endl;
                sample_rate_ = prev_rate;

                std::lock_guard<std::mutex> lock(effect_mutex_);
                // FIX: Revert the sample rates using the graph nodes
                for (const auto &node : main_graph_.get_nodes())
                {
                    if (node.pedal)
                    {
                        node.pedal->set_sample_rate(prev_rate);
                        node.pedal->reset();
                    }
                }
                if (tuner_tap_)
                {
                    tuner_tap_->set_sample_rate(prev_rate);
                    tuner_tap_->reset();
                }
                start();
            }
            else
            {
                last_error_.clear();
            }
        }
    }
    void AudioEngine::commit_graph_changes()
    {
        std::lock_guard<std::mutex> lock(effect_mutex_);

        // 1. Create a brand new executor (so we don't mutate memory the audio thread is currently reading)
        auto new_executor = std::make_shared<AudioGraphExecutor>();
        new_executor->prepare(sample_rate_, buffer_size_, 32);

        // 2. Compile the latest UI graph into the new executor
        new_executor->compile(main_graph_);

        // 3. Promote it to the main slot. The audio thread will grab it on the next try_lock!
        main_executor_ = new_executor;
        topology_dirty_.store(true, std::memory_order_release);
    }

#ifdef AMPLITRON_TESTS
    void AudioEngine::replace_backend_for_test(AudioBackendState *backend)
    {
        if (backend_ == backend)
        {
            return;
        }

        destroy_audio_backend(backend_);
        backend_ = backend;
    }
#endif
}