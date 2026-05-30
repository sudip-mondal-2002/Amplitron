#pragma once

#include "audio/effects/effect.h"
#include <functional>
#include <stdexcept>
#include <unordered_map>
#include <string>

namespace Amplitron {

/**
 * Registry-based effect factory.
 * Replaces the if-chain in PresetManager::create_effect().
 * All effect types self-register via EffectRegistrar.
 */
class EffectFactory {
public:
    using Creator = std::function<std::shared_ptr<Effect>()>;

    static EffectFactory& instance() {
        static EffectFactory factory;
        return factory;
    }

    void register_effect(const std::string& type_name, Creator creator) {
        if (creators_.count(type_name) > 0) {
            throw std::runtime_error("Duplicate effect registration: " + type_name);
        }
        creators_.emplace(type_name, std::move(creator));
    }

    std::shared_ptr<Effect> create(const std::string& type_name) const {
        auto it = creators_.find(type_name);
        if (it != creators_.end()) {
            return it->second();
        }
        return nullptr;
    }

    std::vector<std::string> registered_types() const {
        std::vector<std::string> types;
        types.reserve(creators_.size());
        for (auto& [name, _] : creators_) {
            types.push_back(name);
        }
        return types;
    }

    std::shared_ptr<Effect> create_from_type(const std::string& type_name) const {
        return create(type_name);
    }

    std::vector<std::string> get_all_type_names() const {
        return registered_types();
    }

private:
    EffectFactory() = default;
    std::unordered_map<std::string, Creator> creators_;
};

/**
 * RAII helper that registers an effect type at static-init time.
 * Usage (in .cpp file):
 *   static EffectRegistrar<MyEffect> reg("My Effect");
 */
template <typename T>
struct EffectRegistrar {
    explicit EffectRegistrar(const std::string& type_name) {
        EffectFactory::instance().register_effect(type_name, []() {
            return std::make_shared<T>();
        });
    }
};

inline std::shared_ptr<Effect> Effect::clone() const {
    auto new_effect = EffectFactory::instance().create(type_id());
    if (new_effect) {
        new_effect->set_params(get_params());
        new_effect->set_enabled(enabled_);
        new_effect->set_mix(mix_);
    }
    return new_effect;
}

} // namespace Amplitron
