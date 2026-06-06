#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "audio/backend/i_audio_backend.h"

namespace Amplitron {

/**
 * @brief Registry for available audio backends.
 * Satisfies the Open/Closed Principle (OCP) and Registry pattern.
 */
class AudioBackendRegistry {
   public:
    using Creator = std::function<std::unique_ptr<IAudioBackend>()>;

    static AudioBackendRegistry& instance() {
        static AudioBackendRegistry inst;
        return inst;
    }

    void register_backend(const std::string& name, Creator creator) { creators_[name] = creator; }

    std::unique_ptr<IAudioBackend> create(const std::string& name) {
        auto it = creators_.find(name);
        if (it != creators_.end()) {
            return it->second();
        }
        return nullptr;
    }

    std::vector<std::string> available() const {
        std::vector<std::string> keys;
        keys.reserve(creators_.size());
        for (const auto& pair : creators_) {
            keys.push_back(pair.first);
        }
        return keys;
    }

   private:
    AudioBackendRegistry() = default;
    ~AudioBackendRegistry() = default;
    AudioBackendRegistry(const AudioBackendRegistry&) = delete;
    AudioBackendRegistry& operator=(const AudioBackendRegistry&) = delete;

    std::map<std::string, Creator> creators_;
};

template <typename T>
class BackendRegistrar {
   public:
    BackendRegistrar(const std::string& name) {
        AudioBackendRegistry::instance().register_backend(name,
                                                          []() { return std::make_unique<T>(); });
    }
};

}  // namespace Amplitron
