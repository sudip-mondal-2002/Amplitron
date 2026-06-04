#include "audio/backend/audio_backend.h"
#include "audio/backend/audio_backend_registry.h"
#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#if defined(AMPLITRON_ANDROID_OBOE)
#include "audio/backend/oboe_backend.h"
static Amplitron::BackendRegistrar<Amplitron::OboeBackend> g_oboe_registrar("oboe");
#elif defined(__EMSCRIPTEN__) || (defined(__APPLE__) && TARGET_OS_IPHONE)
#include "audio/backend/sdl_backend.h"
static Amplitron::BackendRegistrar<Amplitron::SdlBackend> g_sdl_registrar("sdl");
#else
#include "audio/backend/portaudio_backend.h"
#include "audio/backend/sdl_backend.h"
static Amplitron::BackendRegistrar<Amplitron::PortAudioBackend> g_portaudio_registrar("portaudio");
static Amplitron::BackendRegistrar<Amplitron::SdlBackend> g_sdl_registrar("sdl");
#ifdef WITH_JACK
#include "audio/backend/jack_backend.h"
static Amplitron::BackendRegistrar<Amplitron::JackBackend> g_jack_registrar("jack");
#endif
#endif

namespace Amplitron {

std::unique_ptr<IAudioBackend> AudioBackendFactory::create_backend(const std::string& type) {
    auto backend = AudioBackendRegistry::instance().create(type);
    if (backend) {
        return backend;
    }
    
    // Fallback: if not found, use first available
    auto list = get_available_backends();
    if (!list.empty()) {
        return AudioBackendRegistry::instance().create(list[0]);
    }
    return nullptr;
}

std::vector<std::string> AudioBackendFactory::get_available_backends() {
    return AudioBackendRegistry::instance().available();
}

} // namespace Amplitron
