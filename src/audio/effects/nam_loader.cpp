// RTNeural's bundled modules/json/json.hpp has been replaced by a shim that
// redirects to this project's nlohmann/json v3.11.3, eliminating the dual
// inline-namespace ABI conflict.  Include order no longer matters.
#include "audio/effects/nam_loader.h"

#include <RTNeural/RTNeural.h>

#include <fstream>
#include <future>
#include <mutex>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <thread>

#include "audio/effects/core/effect_factory.h"

namespace Amplitron {

static EffectRegistrar<NamLoader> reg("NamLoader");

NamLoader::NamLoader() {
    params_ = {
        {"Level", 1.0f, 0.0f, 1.0f, 1.0f, "", "Output level of the NAM model."},
    };
}

NamLoader::~NamLoader() {
    // Wait for any in-progress async load to finish before freeing state.
    if (load_future_.valid()) {
        load_future_.wait();
    }

    // Clean up any unconsumed pending model
    auto* pending = pending_model_.exchange(nullptr);
    delete pending;

    // Clean up active model
    delete active_model_;

    // Clean up old garbage models
    collect_garbage();
}

static std::ifstream open_model_file(const std::string& path) {
    if (path.empty()) {
        return std::ifstream();
    }
    std::ifstream f(path);
    if (f.good()) return f;

    // Fallbacks for relative test paths depending on process CWD (e.g. running
    // from build/ vs project root)
    if (path.rfind("../", 0) == 0) {
        std::string stripped = path.substr(3);
        std::ifstream f2(stripped);
        if (f2.good()) return f2;
    } else {
        std::string prefixed = "../" + path;
        std::ifstream f3(prefixed);
        if (f3.good()) return f3;
    }
    return f;
}

static nlohmann::json convert_nam_json_to_rtneural(const nlohmann::json& j) {
    if (j.contains("in_shape") && j.contains("layers")) {
        return j;
    }

    if (j.contains("layers") && j["layers"].is_array()) {
        nlohmann::json res;
        res["in_shape"] = nlohmann::json::array({nullptr, 1});
        res["layers"] = j["layers"];
        return res;
    }

    if (j.contains("config") && j["config"].is_object() && j["config"].contains("layers")) {
        const auto& layers = j["config"]["layers"];
        if (layers.is_array() && !layers.empty() && layers[0].contains("type") &&
            layers[0].contains("weights")) {
            nlohmann::json res;
            res["in_shape"] = nlohmann::json::array({nullptr, 1});
            res["layers"] = layers;
            return res;
        }
    }

    if (j.contains("architecture") && j.contains("weights")) {
        std::string arch = j["architecture"].get<std::string>();
        const auto& weights = j["weights"];
        const auto& config = j.value("config", nlohmann::json::object());

        nlohmann::json res;
        res["in_shape"] = nlohmann::json::array({nullptr, 1});
        nlohmann::json rt_layers = nlohmann::json::array();

        if (arch == "Linear") {
            int in_size = config.value("input_size", 1);
            int out_size = config.value("output_size", 1);
            bool has_bias = config.value("bias", true);

            size_t idx = 0;
            nlohmann::json w_matrix = nlohmann::json::array();
            for (int i = 0; i < in_size; ++i) {
                nlohmann::json row = nlohmann::json::array();
                for (int j_idx = 0; j_idx < out_size; ++j_idx) {
                    float val = (idx < weights.size()) ? weights[idx++].get<float>() : 0.0f;
                    row.push_back(val);
                }
                w_matrix.push_back(row);
            }
            nlohmann::json b_vector = nlohmann::json::array();
            if (has_bias) {
                for (int j_idx = 0; j_idx < out_size; ++j_idx) {
                    float val = (idx < weights.size()) ? weights[idx++].get<float>() : 0.0f;
                    b_vector.push_back(val);
                }
            } else {
                for (int j_idx = 0; j_idx < out_size; ++j_idx) b_vector.push_back(0.0f);
            }

            nlohmann::json dense_layer;
            dense_layer["type"] = "dense";
            dense_layer["shape"] = nlohmann::json::array({nullptr, out_size});
            dense_layer["weights"] = nlohmann::json::array({w_matrix, b_vector});
            rt_layers.push_back(dense_layer);
            res["layers"] = rt_layers;
            return res;
        } else if (arch == "LSTM") {
            int in_size = config.value("input_size", 1);
            int hidden_size = config.value("hidden_size", 16);
            int num_layers = config.value("num_layers", 1);

            size_t idx = 0;
            int current_in = in_size;

            for (int layer_i = 0; layer_i < num_layers; ++layer_i) {
                nlohmann::json w_ih = nlohmann::json::array();
                for (int i = 0; i < current_in; ++i) {
                    nlohmann::json row = nlohmann::json::array();
                    for (int h = 0; h < 4 * hidden_size; ++h) {
                        float val = (idx < weights.size()) ? weights[idx++].get<float>() : 0.0f;
                        row.push_back(val);
                    }
                    w_ih.push_back(row);
                }
                nlohmann::json w_hh = nlohmann::json::array();
                for (int i = 0; i < hidden_size; ++i) {
                    nlohmann::json row = nlohmann::json::array();
                    for (int h = 0; h < 4 * hidden_size; ++h) {
                        float val = (idx < weights.size()) ? weights[idx++].get<float>() : 0.0f;
                        row.push_back(val);
                    }
                    w_hh.push_back(row);
                }
                nlohmann::json b_vec = nlohmann::json::array();
                for (int h = 0; h < 4 * hidden_size; ++h) {
                    float b1 = (idx < weights.size()) ? weights[idx++].get<float>() : 0.0f;
                    b_vec.push_back(b1);
                }
                for (int h = 0; h < 4 * hidden_size; ++h) {
                    float b2 = (idx < weights.size()) ? weights[idx++].get<float>() : 0.0f;
                    b_vec[h] = b_vec[h].get<float>() + b2;
                }

                nlohmann::json lstm_layer;
                lstm_layer["type"] = "lstm";
                lstm_layer["shape"] = nlohmann::json::array({nullptr, hidden_size});
                lstm_layer["weights"] = nlohmann::json::array({w_ih, w_hh, b_vec});
                rt_layers.push_back(lstm_layer);
                current_in = hidden_size;
            }

            if (idx < weights.size()) {
                nlohmann::json head_w = nlohmann::json::array();
                for (int i = 0; i < hidden_size; ++i) {
                    float val = (idx < weights.size()) ? weights[idx++].get<float>() : 0.0f;
                    head_w.push_back(nlohmann::json::array({val}));
                }
                float head_b = (idx < weights.size()) ? weights[idx++].get<float>() : 0.0f;
                nlohmann::json dense_layer;
                dense_layer["type"] = "dense";
                dense_layer["shape"] = nlohmann::json::array({nullptr, 1});
                dense_layer["weights"] =
                    nlohmann::json::array({head_w, nlohmann::json::array({head_b})});
                rt_layers.push_back(dense_layer);
            }

            res["layers"] = rt_layers;
            return res;
        }
    }

    return j;
}

static float next_weight(const nlohmann::json& weights, size_t& index) {
    return index < weights.size() ? weights[index++].get<float>() : 0.0f;
}

// Standard NAM WaveNet files store one large, flat weight array. Building the RTNeural
// layers directly avoids expanding that array into a second, deeply nested JSON tree and
// then asking RTNeural to traverse it again. This is especially important in WebAssembly,
// where the temporary JSON allocations dominate load time.
static std::unique_ptr<RTNeural::Model<float>> build_wavenet_model(const nlohmann::json& j) {
    if (j.value("architecture", "") != "WaveNet" || !j.contains("weights")) return {};

    const auto& config = j.value("config", nlohmann::json::object());
    if (!config.contains("layers") || !config["layers"].is_array()) return {};

    const auto& weights = j["weights"];
    auto model = std::make_unique<RTNeural::Model<float>>(1);
    size_t index = 0;
    int current_in = 1;

    for (const auto& layer_config : config["layers"]) {
        const int channels = layer_config.value("channels", 16);
        const int kernel_size = layer_config.value("kernel_size", 3);
        const int head_size = layer_config.value("head_size", 8);
        const bool gated = layer_config.value("gated", false);
        const bool head_bias = layer_config.value("head_bias", false);
        const std::string activation = layer_config.value("activation", "Tanh");

        std::vector<int> dilations{1};
        if (layer_config.contains("dilations") && layer_config["dilations"].is_array()) {
            dilations = layer_config["dilations"].get<std::vector<int>>();
        }

        auto* receptive = new RTNeural::Conv1D<float>(current_in, channels, 1, 1);
        std::vector<std::vector<std::vector<float>>> receptive_weights(
            channels, std::vector<std::vector<float>>(current_in, std::vector<float>(1)));
        for (int out = 0; out < channels; ++out) {
            for (int in = 0; in < current_in; ++in) {
                receptive_weights[out][in][0] = next_weight(weights, index);
            }
        }
        receptive->setWeights(receptive_weights);
        receptive->setBias(std::vector<float>(channels, 0.0f));
        model->addLayer(receptive);

        int layer_in = channels;
        for (int dilation : dilations) {
            const int convolution_outputs = gated ? channels * 2 : channels;
            auto* convolution =
                new RTNeural::Conv1D<float>(layer_in, convolution_outputs, kernel_size, dilation);
            std::vector<std::vector<std::vector<float>>> convolution_weights(
                convolution_outputs,
                std::vector<std::vector<float>>(layer_in, std::vector<float>(kernel_size)));
            for (int out = 0; out < convolution_outputs; ++out) {
                for (int in = 0; in < layer_in; ++in) {
                    for (int kernel = 0; kernel < kernel_size; ++kernel) {
                        convolution_weights[out][in][kernel_size - 1 - kernel] =
                            next_weight(weights, index);
                    }
                }
            }
            std::vector<float> convolution_bias(convolution_outputs);
            for (float& bias : convolution_bias) bias = next_weight(weights, index);
            convolution->setWeights(convolution_weights);
            convolution->setBias(convolution_bias);
            model->addLayer(convolution);
            if (activation == "ReLU" || activation == "relu") {
                model->addLayer(new RTNeural::ReLuActivation<float>(convolution_outputs));
            } else {
                model->addLayer(new RTNeural::TanhActivation<float>(convolution_outputs));
            }

            auto* residual = new RTNeural::Dense<float>(convolution_outputs, channels);
            std::vector<std::vector<float>> residual_weights(
                channels, std::vector<float>(convolution_outputs));
            for (int out = 0; out < channels; ++out) {
                for (int in = 0; in < convolution_outputs; ++in) {
                    residual_weights[out][in] = next_weight(weights, index);
                }
            }
            std::vector<float> residual_bias(channels);
            for (float& bias : residual_bias) bias = next_weight(weights, index);
            residual->setWeights(residual_weights);
            residual->setBias(residual_bias.data());
            model->addLayer(residual);

            // The NAM head is a side branch. The existing sequential approximation does not
            // evaluate it, but its weights still need to be skipped in the flat array.
            index +=
                std::min(weights.size() - std::min(index, weights.size()),
                         static_cast<size_t>(head_size * channels + (head_bias ? head_size : 0)));
            layer_in = channels;
        }
        current_in = channels;
    }

    auto* output = new RTNeural::Dense<float>(current_in, 1);
    const float head_scale = config.value("head_scale", 1.0f);
    std::vector<std::vector<float>> output_weights(1, std::vector<float>(current_in));
    for (int in = 0; in < current_in; ++in) {
        output_weights[0][in] = next_weight(weights, index) * head_scale;
    }
    const float output_bias = next_weight(weights, index) * head_scale;
    output->setWeights(output_weights);
    output->setBias(&output_bias);
    model->addLayer(output);
    return model;
}

static std::unique_ptr<RTNeural::Model<float>> parse_model(const nlohmann::json& raw_json) {
    const nlohmann::json* model_json = &raw_json;
    if (raw_json.value("architecture", "") == "SlimmableContainer") {
        const auto& submodels = raw_json.value("config", nlohmann::json::object())
                                    .value("submodels", nlohmann::json::array());
        if (submodels.empty() || !submodels.back().contains("model")) return {};
        // Use submodels.back() — SlimmableContainer stores submodels in
        // ascending quality order, so the last entry is the largest/most
        // accurate variant available.
        model_json = &submodels.back()["model"];
    }

    // A2 WaveNet models use residual connections, per-layer kernel sizes, and activation
    // arrays that RTNeural's sequential Model cannot represent faithfully. Reject them
    // before entering RTNeural so browser workers fail quickly and cleanly instead of
    // terminating with an uncaught exception and leaving the UI stuck on "Loading...".
    if (model_json->value("architecture", "") == "WaveNet") {
        const auto& layers = model_json->value("config", nlohmann::json::object())
                                 .value("layers", nlohmann::json::array());
        for (const auto& layer : layers) {
            if (layer.contains("kernel_sizes") ||
                (layer.contains("activation") && layer["activation"].is_array())) {
                return {};
            }
        }
    }

    if (auto model = build_wavenet_model(*model_json)) return model;
    return RTNeural::json_parser::parseJson<float>(convert_nam_json_to_rtneural(*model_json));
}

bool NamLoader::load_model(const std::string& path) {
    // Sweep any deferred old-model garbage accumulated since the last load.
    collect_garbage();

    std::ifstream f = open_model_file(path);
    if (!f.good()) {
        // Do NOT call clear_model() — a failed open must leave any
        // previously loaded model active and unchanged.
        return false;
    }
    try {
        loading_.store(true, std::memory_order_release);
        nlohmann::json raw_j;
        f >> raw_j;
        auto temp_model = parse_model(raw_j);
        loading_.store(false, std::memory_order_release);
        if (!temp_model) {
            // Parse returned null — leave prior model intact.
            return false;
        }
        temp_model->reset();

        auto* old_pending = pending_model_.exchange(temp_model.release());
        delete old_pending;

        {
            std::lock_guard<std::mutex> lk(model_path_mutex_);
            model_path_ = path;
            load_error_.clear();
        }
        model_loaded_.store(true, std::memory_order_release);
        clear_pending_.store(false, std::memory_order_release);
        return true;
    } catch (const std::exception& ex) {
        loading_.store(false, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lk(model_path_mutex_);
            load_error_ = ex.what();
        }
        // Parse threw — leave prior model intact.
        return false;
    } catch (...) {
        loading_.store(false, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lk(model_path_mutex_);
            load_error_ = "Unknown parse error";
        }
        // Parse threw — leave prior model intact.
        return false;
    }
}

void NamLoader::load_model_async(const std::string& path) {
    // Atomically claim the loading slot.  If loading_ is already true the
    // compare_exchange fails and we return immediately, preserving single-load
    // semantics without a TOCTOU window.
    bool expected = false;
    if (!loading_.compare_exchange_strong(expected, true, std::memory_order_acq_rel,
                                          std::memory_order_acquire)) {
        return;
    }

    // Collect deferred garbage before kicking off a new load.
    collect_garbage();

    // Capture path by value so the lambda owns it.
    load_future_ = std::async(std::launch::async, [this, path]() {
        std::ifstream f = open_model_file(path);
        if (!f.good()) {
            loading_.store(false, std::memory_order_release);
            return;
        }
        try {
            nlohmann::json raw_j;
            f >> raw_j;
            auto temp_model = parse_model(raw_j);
            if (!temp_model) {
                {
                    std::lock_guard<std::mutex> lk(model_path_mutex_);
                    load_error_ = "Model format not supported or parse returned null";
                }
                loading_.store(false, std::memory_order_release);
                return;
            }
            temp_model->reset();

            auto* old_pending = pending_model_.exchange(temp_model.release());
            delete old_pending;

            // model_path_ is only read on the GUI thread; hold the mutex
            // while writing so model_path() never observes a torn string.
            {
                std::lock_guard<std::mutex> lk(model_path_mutex_);
                model_path_ = path;
                load_error_.clear();
            }
            model_loaded_.store(true, std::memory_order_release);
            clear_pending_.store(false, std::memory_order_release);
        } catch (const std::exception& ex) {
            std::lock_guard<std::mutex> lk(model_path_mutex_);
            load_error_ = ex.what();
        } catch (...) {
            std::lock_guard<std::mutex> lk(model_path_mutex_);
            load_error_ = "Unknown async parse error";
        }
        loading_.store(false, std::memory_order_release);
    });
}

void NamLoader::clear_model() {
    {
        std::lock_guard<std::mutex> lk(model_path_mutex_);
        model_path_.clear();
        load_error_.clear();
    }
    model_loaded_.store(false, std::memory_order_release);

    auto* old = pending_model_.exchange(nullptr);
    delete old;

    clear_pending_.store(true, std::memory_order_release);
}

std::string NamLoader::model_path() const {
    std::lock_guard<std::mutex> lk(model_path_mutex_);
    return model_path_;
}

std::string NamLoader::load_error() const {
    std::lock_guard<std::mutex> lk(model_path_mutex_);
    return load_error_;
}

void NamLoader::wait_for_load() {
    if (load_future_.valid()) {
        load_future_.wait();
    }
}

void NamLoader::push_garbage(RTNeural::Model<float>* old) {
    if (!old) return;
    for (size_t i = 0; i < kMaxGarbageSlots; ++i) {
        RTNeural::Model<float>* expected = nullptr;
        if (old_models_to_delete_[i].compare_exchange_strong(
                expected, old, std::memory_order_release, std::memory_order_relaxed)) {
            return;
        }
    }
}

void NamLoader::collect_garbage() {
    for (size_t i = 0; i < kMaxGarbageSlots; ++i) {
        auto* to_delete = old_models_to_delete_[i].exchange(nullptr, std::memory_order_acquire);
        delete to_delete;
    }
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
            push_garbage(old);
        }
    }

    // 2. Process pending model updates
    auto* pending = pending_model_.exchange(nullptr, std::memory_order_acquire);
    if (pending) {
        auto* old = active_model_;
        active_model_ = pending;
        if (old) {
            push_garbage(old);
        }
    }
}

void NamLoader::reset() {
    // NOTE: active_model_ is owned by the audio thread. Calling reset()
    // directly from the GUI thread is a data race. If the command-queue
    // mechanism is available this should be routed through it instead.
    // For now, reset() is only safe to call from the audio thread or
    // between process() calls (e.g. sample-rate change from the DAW host).
    auto* model = active_model_;
    if (model) {
        model->reset();
    }
}

}  // namespace Amplitron
