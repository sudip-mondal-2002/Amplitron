#pragma once

// Neural Amp Modeler (NAM) pedal — loads and runs .nam ML model files
// for realistic amp simulation using RTNeural inference engine.
//
// IMPORTANT: Do NOT #include <RTNeural/RTNeural.h> here.
// RTNeural bundles its own copy of nlohmann/json (v3.11.1) which conflicts
// with our project's nlohmann/json (v3.11.3) when both are included in the
// same translation unit.  We forward-declare RTNeural::Model<float> here and
// include the full RTNeural headers only in nam_loader.cpp.

#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <string>

#include "audio/effects/core/effect.h"

// Forward-declare only the template — the .cpp includes the full header.
// std::atomic<T*> works legally with an incomplete T.
namespace RTNeural {
template <typename T>
class Model;
}  // namespace RTNeural

namespace Amplitron {

/**
 * NAM Loader pedal — loads a .nam model file and runs ML-based
 * amp simulation using the RTNeural inference engine.
 */
class NamLoader : public Effect {
   public:
    /** Initializes the pedal with default Level parameter. */
    NamLoader();
    /** Cleans up model resources. */
    ~NamLoader() override;
    /** Runs NAM inference on the audio buffer in place. */
    void process(float* buffer, int num_samples) override;
    /** Resets model state. */
    void reset() override;
    const char* name() const override { return "NAM Loader"; }
    const char* type_id() const override { return "NamLoader"; }
    std::vector<EffectParam>& params() override { return params_; }
    const std::vector<EffectParam>& params() const override { return params_; }
    /** Loads a .nam file from the given path. Returns true on success.
     *  On web (Emscripten), use load_model_async() instead to avoid
     *  blocking the browser's main thread. */
    bool load_model(const std::string& path);
    /** Non-blocking model load — kicks off a background thread and returns
     *  immediately.  Check is_loading() to show a progress indicator.
     *  Resolves into the normal pending_model_ slot; no extra polling needed. */
    void load_model_async(const std::string& path);
    /** Returns true while an async load is in progress. */
    bool is_loading() const { return loading_.load(std::memory_order_acquire); }
    /** Clears the loaded model. */
    void clear_model();
    /** Returns a copy of the path of the currently loaded .nam file.
     *  Thread-safe: acquires model_path_mutex_ internally. */
    std::string model_path() const;
    /** Returns the last load error message, or "" if there was none.
     *  Only populated when load_model / load_model_async fail with an
     *  exception; file-open failures leave it empty. */
    std::string load_error() const;
    /**
     * Blocks until the background load future completes.  Useful in
     * tests to avoid polling loops that time out on slow CI runners.
     */
    void wait_for_load();
    /**
     * Collects deferred old-model garbage on the GUI/caller thread.
     * Call once per frame from any GUI path that reads model state so
     * old RTNeural models are freed without relying on model_path().
     */
    void collect_garbage();

   private:
    std::vector<EffectParam> params_;
    // Protects model_path_ and load_error_ for GUI/loader thread access.
    // The audio thread never touches these fields.
    mutable std::mutex model_path_mutex_;
    std::string model_path_;
    std::string load_error_;
    std::atomic<bool> model_loaded_{false};
    // Set to true while an async background load is running.
    std::atomic<bool> loading_{false};

    // Holds the background-thread future so it can be joined on destruction.
    std::future<void> load_future_;

    // Atomic model swap: GUI thread stores, audio thread consumes
    std::atomic<RTNeural::Model<float>*> pending_model_{nullptr};
    RTNeural::Model<float>* active_model_ = nullptr;
    // IMPORTANT: push_garbage() silently drops the pointer when all slots are
    // occupied because the audio thread cannot delete it safely, causing a
    // leak.  Do NOT lower kMaxGarbageSlots — it exists precisely to prevent
    // this from happening under any realistic rapid-reload scenario.
    static constexpr size_t kMaxGarbageSlots = 16;
    mutable std::atomic<RTNeural::Model<float>*> old_models_to_delete_[kMaxGarbageSlots]{};
    std::atomic<bool> clear_pending_{false};
    // Incremented by clear_model() to invalidate any in-flight async load.
    // The async lambda captures the value at launch; if the stored value has
    // changed by the time parsing finishes, the result is silently discarded.
    std::atomic<uint32_t> load_generation_{0};

    void check_pending_model();
    void push_garbage(RTNeural::Model<float>* old);
};
}  // namespace Amplitron
