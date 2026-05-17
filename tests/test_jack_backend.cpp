#ifdef WITH_JACK
#include "audio/audio_backend_jack_internal.h"
#include "audio/audio_engine.h"
#include <gtest/gtest.h>
#include <atomic>
#include <thread>

using namespace Amplitron;

TEST(JackBackend, InitializeAndStart)
{
    AudioEngine engine;
    // initialization requires jackd running; this test is guarded by WITH_JACK
    bool ok = engine.initialize();
    if (!ok)
    {
        GTEST_SKIP() << "jackd not running; skipping integration test";
    }
    EXPECT_TRUE(ok);
    EXPECT_TRUE(engine.start());
    engine.stop();
    engine.shutdown();
}

TEST(JackBackend, StartStopSmoke)
{
    AudioEngine engine;
    if (!engine.initialize())
    {
        GTEST_SKIP() << "jackd not running; skipping integration test";
    }
    EXPECT_TRUE(engine.start());
    // wait briefly
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    engine.stop();
    engine.shutdown();
}

#endif // WITH_JACK
