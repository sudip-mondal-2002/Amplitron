// RTNeural's bundled modules/json/json.hpp has been replaced by a shim that
// redirects to this project's nlohmann/json v3.11.3, eliminating the dual
// inline-namespace ABI conflict.  Include order no longer matters.
#include "audio/effects/nam_loader.h"

#include <RTNeural/RTNeural.h>

#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include "audio/effects/core/effect_factory.h"

namespace Amplitron {

static EffectRegistrar<NamLoader> reg("NamLoader");

NamLoader::NamLoader() {
    params_ = {
        {"Level", 1.0f, 0.0f, 1.0f, 1.0f, "", "Output level of the NAM model."},
    };
}

NamLoader::~NamLoader() {
    // Clean up any unconsumed pending model
    auto* pending = pending_model_.exchange(nullptr);
    delete pending;

    // Clean up active model
    delete active_model_;

    // Clean up old garbage model
    auto* old = old_model_to_delete_.exchange(nullptr);
    delete old;
}

bool NamLoader::load_model(const std::string& path) {
    // Sweep any deferred old-model garbage accumulated since the last load.
    collect_garbage();

    std::ifstream f(path);
    if (!f.good()) {
        // Do NOT call clear_model() — a failed open must leave any
        // previously loaded model active and unchanged.
        return false;
    }
    try {
        auto temp_model = RTNeural::json_parser::parseJson<float>(f);
        if (!temp_model) {
            // Parse returned null — leave prior model intact.
            return false;
        }
        temp_model->reset();

        auto* old_pending = pending_model_.exchange(temp_model.release());
        delete old_pending;

        model_path_ = path;
        model_loaded_.store(true, std::memory_order_release);
        clear_pending_.store(false, std::memory_order_release);
        return true;
    } catch (...) {
        // Parse threw — leave prior model intact.
        return false;
    }
}

void NamLoader::clear_model() {
    model_path_.clear();
    model_loaded_.store(false, std::memory_order_release);

    auto* old = pending_model_.exchange(nullptr);
    delete old;

    clear_pending_.store(true, std::memory_order_release);
}

const std::string& NamLoader::model_path() const {
    // Pure accessor — no side effects.  Callers responsible for GC via
    // collect_garbage() (load_model calls it; GUI calls it once per frame).
    return model_path_;
}

void NamLoader::collect_garbage() {
    auto* to_delete = old_model_to_delete_.exchange(nullptr, std::memory_order_acquire);
    delete to_delete;
}

void NamLoader::process(float* buffer, int num_samples) {
    if (!enabled_) return;

    check_pending_model();
    if (!model_loaded_.load(std::memory_order_acquire) || !active_model_) return;

    const float level = params_[0].value;

    for (int i = 0; i < num_samples; ++i) {
        float input = buffer[i];
        buffer[i] = active_model_->forward(&input) * level;
    }
}

void NamLoader::check_pending_model() {
    // 1. Process pending clear commands
    if (clear_pending_.exchange(false, std::memory_order_acq_rel)) {
        auto* old = active_model_;
        active_model_ = nullptr;
        if (old) {
            auto* prev_old = old_model_to_delete_.exchange(old, std::memory_order_release);
            if (prev_old) {
                delete prev_old;
            }
        }
    }

    // 2. Process pending model updates
    auto* pending = pending_model_.exchange(nullptr, std::memory_order_acquire);
    if (pending) {
        auto* old = active_model_;
        active_model_ = pending;
        if (old) {
            auto* prev_old = old_model_to_delete_.exchange(old, std::memory_order_release);
            if (prev_old) {
                delete prev_old;
            }
        }
    }
}

void NamLoader::reset() {
    auto* model = active_model_;
    if (model) {
        model->reset();
    }
}

}  // namespace Amplitron
