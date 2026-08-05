// RTNeural's bundled modules/json/json.hpp has been replaced by a shim that
// redirects to this project's nlohmann/json v3.11.3, eliminating the dual
// inline-namespace ABI conflict.  Include order no longer matters.
#include "audio/effects/nam_loader.h"

#include <RTNeural/RTNeural.h>

#include <fstream>
#include <future>
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

    // Fallbacks for relative test paths depending on process CWD (e.g. running from build/ vs project root)
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
        if (layers.is_array() && !layers.empty() && layers[0].contains("type") && layers[0].contains("weights")) {
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
                dense_layer["weights"] = nlohmann::json::array({head_w, nlohmann::json::array({head_b})});
                rt_layers.push_back(dense_layer);
            }

            res["layers"] = rt_layers;
            return res;
        } else if (arch == "WaveNet") {
            size_t idx = 0;
            if (config.contains("layers") && config["layers"].is_array()) {
                int current_in = 1;
                for (const auto& l_cfg : config["layers"]) {
                    int channels = l_cfg.value("channels", 16);
                    int kernel_size = l_cfg.value("kernel_size", 3);
                    int head_size = l_cfg.value("head_size", 8);
                    bool gated = l_cfg.value("gated", false);
                    bool head_bias = l_cfg.value("head_bias", false);
                    std::string act = l_cfg.value("activation", "Tanh");
                    std::string act_lower = "tanh";
                    if (act == "ReLU" || act == "relu") act_lower = "relu";

                    std::vector<int> dilations;
                    if (l_cfg.contains("dilations") && l_cfg["dilations"].is_array()) {
                        for (const auto& d : l_cfg["dilations"]) dilations.push_back(d.get<int>());
                    } else {
                        dilations.push_back(1);
                    }

                    // 1. Receptive field input conv: PyTorch shape [channels, current_in, 1]
                    int rf_out = channels;
                    std::vector<std::vector<std::vector<float>>> rf_3d(1, std::vector<std::vector<float>>(current_in, std::vector<float>(rf_out, 0.0f)));
                    for (int co = 0; co < rf_out; ++co) {
                        for (int ci = 0; ci < current_in; ++ci) {
                            for (int k = 0; k < 1; ++k) {
                                float val = (idx < weights.size()) ? weights[idx++].get<float>() : 0.0f;
                                rf_3d[k][ci][co] = val;
                            }
                        }
                    }
                    nlohmann::json rf_w = rf_3d;
                    nlohmann::json rf_b = nlohmann::json::array();
                    for (int co = 0; co < rf_out; ++co) rf_b.push_back(0.0f);

                    nlohmann::json rf_conv;
                    rf_conv["type"] = "conv1d";
                    rf_conv["shape"] = nlohmann::json::array({nullptr, rf_out});
                    rf_conv["kernel_size"] = nlohmann::json::array({1});
                    rf_conv["dilation"] = nlohmann::json::array({1});
                    rf_conv["weights"] = nlohmann::json::array({rf_w, rf_b});
                    rt_layers.push_back(rf_conv);

                    int layer_in = rf_out;
                    for (int dil : dilations) {
                        int conv_out = gated ? channels * 2 : channels;
                        // Dilated Conv1D weights: PyTorch [conv_out, layer_in, kernel_size]
                        std::vector<std::vector<std::vector<float>>> c_3d(kernel_size, std::vector<std::vector<float>>(layer_in, std::vector<float>(conv_out, 0.0f)));
                        for (int co = 0; co < conv_out; ++co) {
                            for (int ci = 0; ci < layer_in; ++ci) {
                                for (int k = 0; k < kernel_size; ++k) {
                                    float val = (idx < weights.size()) ? weights[idx++].get<float>() : 0.0f;
                                    c_3d[k][ci][co] = val;
                                }
                            }
                        }
                        nlohmann::json c_w = c_3d;
                        nlohmann::json c_b = nlohmann::json::array();
                        for (int co = 0; co < conv_out; ++co) {
                            float val = (idx < weights.size()) ? weights[idx++].get<float>() : 0.0f;
                            c_b.push_back(val);
                        }

                        nlohmann::json dil_conv;
                        dil_conv["type"] = "conv1d";
                        dil_conv["shape"] = nlohmann::json::array({nullptr, conv_out});
                        dil_conv["kernel_size"] = nlohmann::json::array({kernel_size});
                        dil_conv["dilation"] = nlohmann::json::array({dil});
                        dil_conv["weights"] = nlohmann::json::array({c_w, c_b});
                        dil_conv["activation"] = act_lower;
                        rt_layers.push_back(dil_conv);

                        // 1x1 conv (Dense) residual: PyTorch [channels, conv_out]
                        std::vector<std::vector<float>> res_2d(conv_out, std::vector<float>(channels, 0.0f));
                        for (int co = 0; co < channels; ++co) {
                            for (int ci = 0; ci < conv_out; ++ci) {
                                float val = (idx < weights.size()) ? weights[idx++].get<float>() : 0.0f;
                                res_2d[ci][co] = val;
                            }
                        }
                        nlohmann::json res_w = res_2d;
                        nlohmann::json res_b = nlohmann::json::array();
                        for (int co = 0; co < channels; ++co) {
                            float val = (idx < weights.size()) ? weights[idx++].get<float>() : 0.0f;
                            res_b.push_back(val);
                        }
                        nlohmann::json res_dense;
                        res_dense["type"] = "dense";
                        res_dense["shape"] = nlohmann::json::array({nullptr, channels});
                        res_dense["weights"] = nlohmann::json::array({res_w, res_b});
                        rt_layers.push_back(res_dense);

                        // 1x1 head conv (Dense) weights: advance idx past side-branch weights
                        for (int co = 0; co < head_size; ++co) {
                            for (int ci = 0; ci < channels; ++ci) {
                                if (idx < weights.size()) idx++;
                            }
                        }
                        for (int co = 0; co < head_size; ++co) {
                            if (head_bias && idx < weights.size()) idx++;
                        }

                        layer_in = channels;
                    }
                    current_in = channels;
                }

                // Final output projection to 1 channel: PyTorch [1, current_in]
                std::vector<std::vector<float>> final_2d(current_in, std::vector<float>(1, 0.0f));
                for (int ci = 0; ci < current_in; ++ci) {
                    float val = (idx < weights.size()) ? weights[idx++].get<float>() : 0.0f;
                    final_2d[ci][0] = val;
                }
                nlohmann::json final_w = final_2d;
                float head_scale = config.value("head_scale", 1.0f);
                float final_b = (idx < weights.size()) ? weights[idx++].get<float>() : 0.0f;

                nlohmann::json final_dense;
                final_dense["type"] = "dense";
                final_dense["shape"] = nlohmann::json::array({nullptr, 1});
                final_dense["weights"] = nlohmann::json::array({final_w, nlohmann::json::array({final_b * head_scale})});
                rt_layers.push_back(final_dense);

                res["layers"] = rt_layers;
                return res;
            }
        }
    }

    return j;
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
        nlohmann::json rt_j = convert_nam_json_to_rtneural(raw_j);
        auto temp_model = RTNeural::json_parser::parseJson<float>(rt_j);
        loading_.store(false, std::memory_order_release);
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
        loading_.store(false, std::memory_order_release);
        // Parse threw — leave prior model intact.
        return false;
    }
}

void NamLoader::load_model_async(const std::string& path) {
    // If already loading, ignore the duplicate request.
    if (loading_.load(std::memory_order_acquire)) return;

    // Collect deferred garbage before kicking off a new load.
    collect_garbage();

    loading_.store(true, std::memory_order_release);

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
            nlohmann::json rt_j = convert_nam_json_to_rtneural(raw_j);
            auto temp_model = RTNeural::json_parser::parseJson<float>(rt_j);
            if (!temp_model) {
                loading_.store(false, std::memory_order_release);
                return;
            }
            temp_model->reset();

            auto* old_pending = pending_model_.exchange(temp_model.release());
            delete old_pending;

            // model_path_ is only read on the GUI thread so writing it from
            // the loader thread is safe here (GUI won't be in load_model_async
            // again because loading_ is still true).
            model_path_ = path;
            model_loaded_.store(true, std::memory_order_release);
            clear_pending_.store(false, std::memory_order_release);
        } catch (...) {
            // Silently swallow parse errors — leave prior model intact.
        }
        loading_.store(false, std::memory_order_release);
    });
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

void NamLoader::push_garbage(RTNeural::Model<float>* old) {
    if (!old) return;
    for (size_t i = 0; i < kMaxGarbageSlots; ++i) {
        RTNeural::Model<float>* expected = nullptr;
        if (old_models_to_delete_[i].compare_exchange_strong(expected, old, std::memory_order_release,
                                                            std::memory_order_relaxed)) {
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
    auto* model = active_model_;
    if (model) {
        model->reset();
    }
}

}  // namespace Amplitron
