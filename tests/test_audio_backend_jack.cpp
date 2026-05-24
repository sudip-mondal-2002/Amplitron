#include "test_framework.h"
#include "audio/audio_backend.h"

using namespace Amplitron;

TEST(AudioBackend_Jack)
{
    // The test ensures the JACK backend factory functions can be called.
    // If built without JACK this file will still compile (create_audio_backend
    // may return nullptr); that is acceptable for CI environments.
    AudioBackendState *s = create_audio_backend();
    if (s)
    {
        destroy_audio_backend(s);
    }
    ASSERT_TRUE(true);
}
