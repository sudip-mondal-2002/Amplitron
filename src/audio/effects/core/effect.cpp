#include "audio/effects/core/effect.h"

#include <algorithm>
#include <nlohmann/json.hpp>

#include "audio/effects/core/effect_factory.h"

namespace Amplitron {

std::shared_ptr<Effect> Effect::clone() const {
    auto new_effect = EffectFactory::instance().create(type_id());
    if (new_effect) {
        new_effect->set_params(get_params());
        new_effect->set_enabled(enabled_);
        new_effect->set_mix(mix_);
    }
    return new_effect;
}

nlohmann::json Effect::get_params() const {
    nlohmann::json j;
    const auto& p_list = params();
    for (const auto& p : p_list) {
        j[p.name] = p.value;
    }
    j["enabled"] = enabled_.load();
    j["mix"] = mix_.load(std::memory_order_relaxed);
    return j;
}

void Effect::set_params(const nlohmann::json& j) {
    if (j.contains("enabled")) enabled_.store(j["enabled"].get<bool>());
    if (j.contains("mix")) {
        float m = j["mix"].get<float>();
        mix_.store(std::clamp(m, 0.0f, 1.0f), std::memory_order_relaxed);
    }

    auto& p_list = params();
    for (auto& p : p_list) {
        if (j.contains(p.name)) {
            float v = j[p.name].get<float>();
            p.value = std::clamp(v, p.min_val, p.max_val);
        }
    }
}

}  // namespace Amplitron
