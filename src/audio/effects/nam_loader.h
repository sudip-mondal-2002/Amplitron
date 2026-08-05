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
    /** Returns the path of the currently loaded .nam file (side-effect-free). */
    const std::string& model_path() const;
    /**
     * Collects deferred old-model garbage on the GUI/caller thread.
     * Call once per frame from any GUI path that reads model state so
     * old RTNeural models are freed without relying on model_path().
     */
    void collect_garbage();

   private:
    std::vector<EffectParam> params_;
    std::string model_path_;
    std::atomic<bool> model_loaded_{false};
    // Set to true while an async background load is running.
    std::atomic<bool> loading_{false};

    // Holds the background-thread future so it can be joined on destruction.
    std::future<void> load_future_;

    // Atomic model swap: GUI thread stores, audio thread consumes
    std::atomic<RTNeural::Model<float>*> pending_model_{nullptr};
    RTNeural::Model<float>* active_model_ = nullptr;
    static constexpr size_t kMaxGarbageSlots = 16;
    mutable std::atomic<RTNeural::Model<float>*> old_models_to_delete_[kMaxGarbageSlots]{};
    std::atomic<bool> clear_pending_{false};

    void check_pending_model();
    void push_garbage(RTNeural::Model<float>* old);
};
}  // namespace Amplitron
