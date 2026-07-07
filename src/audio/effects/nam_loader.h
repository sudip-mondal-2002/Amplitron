#pragma once

// Neural Amp Modeler (NAM) pedal — loads and runs .nam ML model files
// for realistic amp simulation using RTNeural inference engine.

#include "audio/effects/core/effect.h"
#include <string>
#include <memory>
#include <atomic>
#include <RTNeural/RTNeural.h>

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
    /** Loads a .nam file from the given path. Returns true on success. */
    bool load_model(const std::string& path);
    /** Clears the loaded model. */
    void clear_model();
    /** Returns the path of the currently loaded .nam file. */
    const std::string& model_path() const;

private:
    std::vector<EffectParam> params_;
    std::string model_path_;
    std::atomic<bool> model_loaded_{false};

    // Atomic model swap: GUI thread stores, audio thread consumes
    std::atomic<RTNeural::Model<float>*> pending_model_{nullptr};
    RTNeural::Model<float>* active_model_ = nullptr;
    mutable std::atomic<RTNeural::Model<float>*> old_model_to_delete_{nullptr};
    std::atomic<bool> clear_pending_{false};

    void check_pending_model();
};
} // namespace Amplitron

