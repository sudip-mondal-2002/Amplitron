#include "test_framework.h"
#include "audio/engine/audio_engine.h"
#include "audio/backend/audio_backend.h"

using namespace Amplitron;

#if defined(AMPLITRON_TESTS) && defined(WITH_JACK) && defined(__unix__) && !defined(__APPLE__)
namespace Amplitron
{
    AudioBackendState *create_disconnected_audio_backend_for_test();
    bool jack_backend_has_active_client_for_test(const AudioBackendState *state);
}
#endif

#if defined(AMPLITRON_TESTS) && defined(WITH_JACK) && defined(__unix__) && !defined(__APPLE__)
TEST(AudioBackend_Jack_DoesNotStartWithoutBackend)
{
    AudioEngine engine;
    ASSERT_TRUE(engine.initialize());

    AudioBackendState *state = create_disconnected_audio_backend_for_test();
    ASSERT_TRUE(state != nullptr);
    ASSERT_FALSE(jack_backend_has_active_client_for_test(state));
    engine.replace_backend_for_test(state);

    ASSERT_FALSE(engine.start());
    ASSERT_FALSE(engine.is_running());
    ASSERT_FALSE(engine.get_last_error().empty());
}
#else
TEST(AudioBackend_Jack_NotAvailable)
{
    ASSERT_TRUE(true);
}
#endif
