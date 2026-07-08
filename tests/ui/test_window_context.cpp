#include "gui/window_context.h"
#include "test_fixtures.h"
#include "test_framework.h"

namespace Amplitron {
extern bool g_mock_window_context_initialize_fail;
extern bool g_mock_window_context_poll_events_fail;
}  // namespace Amplitron

TEST(window_context_constructor_destructor) {
    {
        Amplitron::WindowContext ctx;
        ASSERT_EQ(ctx.get_window(), nullptr);
        ASSERT_EQ(ctx.get_width(), 1280);
        ASSERT_EQ(ctx.get_height(), 720);
        ASSERT_NEAR(ctx.get_dpi_scale(), 1.0f, 0.001f);
    }
}

TEST(window_context_initialize_success) {
    Amplitron::WindowContext ctx;
    ASSERT_TRUE(ctx.initialize(800, 600, "Test Title"));
    ASSERT_EQ(ctx.get_width(), 800);
    ASSERT_EQ(ctx.get_height(), 600);
    ASSERT_EQ(ctx.get_window(), nullptr);
    ctx.shutdown();
}

TEST(window_context_initialize_fail) {
    Amplitron::WindowContext ctx;
    Amplitron::g_mock_window_context_initialize_fail = true;
    ASSERT_FALSE(ctx.initialize(800, 600, "Test Title"));
    Amplitron::g_mock_window_context_initialize_fail = false;
}

TEST(window_context_poll_events_success) {
    Amplitron::WindowContext ctx;
    ASSERT_TRUE(ctx.initialize(800, 600, "Test Title"));
    ASSERT_TRUE(ctx.poll_events());
    ctx.shutdown();
}

TEST(window_context_poll_events_fail) {
    Amplitron::WindowContext ctx;
    ASSERT_TRUE(ctx.initialize(800, 600, "Test Title"));
    Amplitron::g_mock_window_context_poll_events_fail = true;
    ASSERT_FALSE(ctx.poll_events());
    Amplitron::g_mock_window_context_poll_events_fail = false;
    ctx.shutdown();
}

TEST(window_context_begin_end_frame) {
    Amplitron::WindowContext ctx;
    ASSERT_TRUE(ctx.initialize(800, 600, "Test Title"));
    ctx.begin_frame();
    ctx.end_frame();
    ctx.shutdown();
}

TEST(window_context_multiple_shutdowns) {
    Amplitron::WindowContext ctx;
    ASSERT_TRUE(ctx.initialize(800, 600, "Test Title"));
    ctx.shutdown();
    ctx.shutdown();
}
