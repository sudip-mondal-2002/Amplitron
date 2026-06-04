#include "audio/engine/audio_engine.h"
#include "audio/engine/analyzer_capture.h"
#include "audio/backend/audio_backend.h"
#ifdef AMPLITRON_ANDROID_OBOE
#include "audio/backend/oboe_backend.h"
#endif
#include <iostream>
#include <algorithm>
#include <nlohmann/json.hpp>

namespace Amplitron
{

    AudioEngine::AudioEngine(std::unique_ptr<IRecorder> recorder,
                             std::unique_ptr<IMetronome> metronome)
        : metronome_(std::move(metronome)),
          recorder_(std::move(recorder)),
          analyzer_capture_(std::make_unique<AnalyzerCapture>())
    {
        if (!recorder_) recorder_ = std::make_unique<Recorder>();
        if (!metronome_) metronome_ = std::make_unique<Metronome>();

        process_buffer_.resize(16384, 0.0f);
        process_buffer_right_.resize(16384, 0.0f);
#if defined(AMPLITRON_ANDROID_OBOE)
        backend_ = AudioBackendFactory::create_backend("oboe");
#elif defined(__EMSCRIPTEN__) || (defined(__APPLE__) && TARGET_OS_IPHONE)
        backend_ = AudioBackendFactory::create_backend("sdl");
#else
        backend_ = AudioBackendFactory::create_backend("portaudio");
#endif
        metronome_->set_sample_rate(sample_rate_);

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
    }

    // --- Serialization Methods ---

    nlohmann::json AudioEngine::serialize()
    {
        std::lock_guard<std::mutex> lock(effect_mutex_);
        nlohmann::json j;

        // Read atomic variables safely
        j["input_gain"] = input_gain_.load(std::memory_order_relaxed);
        j["global_bpm"] = global_bpm_.load(std::memory_order_relaxed);

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

        if (j.contains("global_bpm"))
        {
            set_global_bpm(j["global_bpm"]);
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

        // Pre-allocate buffers for the new size
        if (static_cast<size_t>(size) > process_buffer_.size()) {
            process_buffer_.resize(size, 0.0f);
            process_buffer_right_.resize(size, 0.0f);
        }

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
            metronome_->set_sample_rate(rate);
            metronome_->reset();
            tempo_engine_.set_sample_rate(rate);
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
                metronome_->set_sample_rate(prev_rate);
                metronome_->reset();
                tempo_engine_.set_sample_rate(prev_rate);
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
        int safe_block_size = std::max(buffer_size_, 8192);
        new_executor->prepare(sample_rate_, safe_block_size, 32);

        // 2. Compile the latest UI graph into the new executor
        new_executor->compile(main_graph_);

        // 3. Promote it to the main slot. The audio thread will grab it on the next try_lock!
        main_executor_ = new_executor;
        topology_dirty_.store(true, std::memory_order_release);
    }

#ifdef AMPLITRON_TESTS
    class NonOwningBackendWrapper : public IAudioBackend {
    public:
        explicit NonOwningBackendWrapper(IAudioBackend* delegate) : delegate_(delegate) {}
        
        bool initialize(IAudioEngine* engine) override { return delegate_->initialize(engine); }
        void shutdown() override { delegate_->shutdown(); }
        bool start() override { return delegate_->start(); }
        void stop() override { delegate_->stop(); }
        
        std::vector<AudioDeviceInfo> get_input_devices() const override { return delegate_->get_input_devices(); }
        std::vector<AudioDeviceInfo> get_output_devices() const override { return delegate_->get_output_devices(); }
        
        bool set_input_device(int index) override { return delegate_->set_input_device(index); }
        bool set_output_device(int index) override { return delegate_->set_output_device(index); }
        
        std::string get_input_device_name() const override { return delegate_->get_input_device_name(); }
        std::string get_output_device_name() const override { return delegate_->get_output_device_name(); }
        
        int get_sample_rate() const override { return delegate_->get_sample_rate(); }
        int get_buffer_size() const override { return delegate_->get_buffer_size(); }

        int get_input_device() const override { return delegate_->get_input_device(); }
        int get_output_device() const override { return delegate_->get_output_device(); }
        
    private:
        IAudioBackend* delegate_;
    };

    void AudioEngine::replace_backend_state_for_test(std::unique_ptr<IAudioBackend> backend)
    {
        backend_ = std::move(backend);
    }

    void AudioEngine::replace_backend_for_test(IAudioBackend* backend)
    {
        backend_ = std::make_unique<NonOwningBackendWrapper>(backend);
    }

    void AudioEngine::clear_backend_for_test()
    {
        backend_ = nullptr;
    }
#endif

    bool AudioEngine::initialize()
    {
        if (backend_) {
            initialized_ = backend_->initialize(this);
            return initialized_;
        }
        return false;
    }

    void AudioEngine::shutdown()
    {
        stop();
        if (backend_) {
            backend_->shutdown();
        }
        initialized_ = false;
    }

    bool AudioEngine::start()
    {
        if (!initialized_ || running_)
            return false;
        if (backend_) {
            running_ = backend_->start();
            if (running_) {
                sample_rate_ = backend_->get_sample_rate();
                buffer_size_ = backend_->get_buffer_size();

                // Pre-allocate the audio thread buffers to avoid allocations in the realtime callback
                if (static_cast<size_t>(buffer_size_) > process_buffer_.size()) {
                    process_buffer_.resize(buffer_size_, 0.0f);
                    process_buffer_right_.resize(buffer_size_, 0.0f);
                }

                metronome_->set_sample_rate(sample_rate_);
                metronome_->reset();
                tempo_engine_.set_sample_rate(sample_rate_);
                {
                    std::lock_guard<std::mutex> lock(effect_mutex_);
                    for (const auto& node : main_graph_.get_nodes()) {
                        if (node.pedal) {
                            node.pedal->set_sample_rate(sample_rate_);
                            node.pedal->reset();
                        }
                    }
                    if (tuner_tap_) {
                        tuner_tap_->set_sample_rate(sample_rate_);
                        tuner_tap_->reset();
                    }
                }

                // Recompile graph executor with actual sample_rate and buffer_size from audio hardware
                commit_graph_changes();

                last_error_.clear();
            } else {
                last_error_ = "Failed to start audio backend.";
            }
            return running_;
        }
        return false;
    }

    void AudioEngine::stop()
    {
        if (backend_) {
            backend_->stop();
        }
        running_ = false;
    }

    bool AudioEngine::restart()
    {
        stop();
        bool ok = start();
        if (!ok) {
            last_error_ = "Failed to restart audio stream. Check device settings.";
            std::cerr << "[Amplitron] " << last_error_ << std::endl;
        } else {
            last_error_.clear();
        }
        return ok;
    }

    std::string AudioEngine::get_input_device_name() const
    {
        return backend_ ? backend_->get_input_device_name() : "None";
    }

    std::string AudioEngine::get_output_device_name() const
    {
        return backend_ ? backend_->get_output_device_name() : "None";
    }

    std::vector<AudioDeviceInfo> AudioEngine::get_input_devices() const
    {
        return backend_ ? backend_->get_input_devices() : std::vector<AudioDeviceInfo>();
    }

    std::vector<AudioDeviceInfo> AudioEngine::get_output_devices() const
    {
        return backend_ ? backend_->get_output_devices() : std::vector<AudioDeviceInfo>();
    }

    bool AudioEngine::set_input_device(int device_index)
    {
        if (!backend_) return false;
        
        int prev_device = input_device_;
        bool was_running = running_;
        if (was_running) stop();
        
        bool ok = backend_->set_input_device(device_index);
        if (!ok) {
            if (was_running) {
                start();
            }
            return false;
        }
        
        input_device_ = device_index;
        if (was_running) {
            if (!start()) {
                last_error_ = "Failed to start with new input device. Reverting.";
                std::cerr << "[Amplitron] " << last_error_ << std::endl;
                
                backend_->set_input_device(prev_device);
                input_device_ = prev_device;
                
                if (!start()) {
                    last_error_ = "Failed to revert to previous input device. Engine stopped.";
                    std::cerr << "[Amplitron] " << last_error_ << std::endl;
                }
                return false;
            }
            last_error_.clear();
        }
        return true;
    }

    bool AudioEngine::set_output_device(int device_index)
    {
        if (!backend_) return false;
        
        int prev_device = output_device_;
        bool was_running = running_;
        if (was_running) stop();
        
        bool ok = backend_->set_output_device(device_index);
        if (!ok) {
            if (was_running) {
                start();
            }
            return false;
        }
        
        output_device_ = device_index;
        if (was_running) {
            if (!start()) {
                last_error_ = "Failed to start with new output device. Reverting.";
                std::cerr << "[Amplitron] " << last_error_ << std::endl;
                
                backend_->set_output_device(prev_device);
                output_device_ = prev_device;
                
                if (!start()) {
                    last_error_ = "Failed to revert to previous output device. Engine stopped.";
                    std::cerr << "[Amplitron] " << last_error_ << std::endl;
                }
                return false;
            }
            last_error_.clear();
        }
        return true;
    }

#ifdef AMPLITRON_ANDROID_OBOE
    const char* AudioEngine::get_oboe_sharing_mode_label() const
    {
        // Try to dynamic cast if we know OboeBackend class
        // Oboe backend has get_oboe_sharing_mode_label method.
        // We can cast using static or dynamic cast.
        auto* oboe_be = dynamic_cast<OboeBackend*>(backend_.get());
        if (oboe_be) {
            return oboe_be->get_oboe_sharing_mode_label();
        }
        return "Oboe";
    }
#endif
}