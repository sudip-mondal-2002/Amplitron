#pragma once

#include <memory>
#include <string>
#include <vector>

#include "audio/backend/i_audio_backend.h"

namespace Amplitron {

class AudioBackendFactory {
   public:
    static std::unique_ptr<IAudioBackend> create_backend(const std::string& type);
    static std::vector<std::string> get_available_backends();
};

}  // namespace Amplitron
