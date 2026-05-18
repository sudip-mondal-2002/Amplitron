#ifdef WITH_JACK
#include "test_framework.h"
#include "audio/audio_backend_jack_internal.h"
#include "audio/audio_engine.h"
#include <thread>

using namespace Amplitron;

TEST(JackBackend_InitializeAndStart)
{
    AudioEngine engine;
    if (!engine.initialize())
    {
        return;
    }
    ASSERT_TRUE(engine.start());
    engine.stop();
    engine.shutdown();
}

TEST(JackBackend_StartStopSmoke)
{
    AudioEngine engine;
    if (!engine.initialize())
    {
        return;
    }
    ASSERT_TRUE(engine.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    engine.stop();
    engine.shutdown();
}

#endif // WITH_JACK
