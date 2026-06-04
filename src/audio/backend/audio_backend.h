#pragma once

#include "audio/backend/i_audio_backend.h"
#include <memory>
#include <string>
#include <vector>

namespace Amplitron {

class AudioBackendFactory {
public:
    static std::unique_ptr<IAudioBackend> create_backend(const std::string& type);
    static std::vector<std::string> get_available_backends();
};

} // namespace Amplitron
