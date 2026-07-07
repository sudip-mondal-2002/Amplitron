#include <SDL.h>

// Global mock state variables
static Uint32 g_mock_sdl_was_init_return = 0;
static int g_mock_sdl_init_subsystem_return = 0;
static int g_mock_sdl_show_messagebox_return = 0;
static int g_mock_sdl_show_messagebox_buttonid = 0;
static bool g_mock_sdl_was_init_called = false;
static bool g_mock_sdl_init_subsystem_called = false;
static bool g_mock_sdl_show_messagebox_called = false;

// Mock implementations
inline Uint32 mock_SDL_WasInit(Uint32 /*flags*/) {
    g_mock_sdl_was_init_called = true;
    return g_mock_sdl_was_init_return;
}

inline int mock_SDL_InitSubSystem(Uint32 /*flags*/) {
    g_mock_sdl_init_subsystem_called = true;
    return g_mock_sdl_init_subsystem_return;
}

inline int mock_SDL_ShowMessageBox(const SDL_MessageBoxData* /*messageboxdata*/, int* buttonid) {
    g_mock_sdl_show_messagebox_called = true;
    if (buttonid) {
        *buttonid = g_mock_sdl_show_messagebox_buttonid;
    }
    return g_mock_sdl_show_messagebox_return;
}

// Redirect SDL calls in the header to our mocks.
// SDL.h must be included before these macros are defined and before including
// gui/crash_recovery_ui.h so the macro redirection applies, and crash_recovery_ui.h
// must not re-include SDL.h in a way that bypasses the macros; also note that the test
// undefines AMPLITRON_HEADLESS to exercise non-headless code paths.
#define SDL_WasInit mock_SDL_WasInit
#define SDL_InitSubSystem mock_SDL_InitSubSystem
#define SDL_ShowMessageBox mock_SDL_ShowMessageBox

#undef AMPLITRON_HEADLESS

namespace mocked {
#include "gui/crash_recovery_ui.h"
}  // namespace mocked

#include "test_framework.h"

// Helper to reset mocks
static void reset_mocks() {
    g_mock_sdl_was_init_return = 0;
    g_mock_sdl_init_subsystem_return = 0;
    g_mock_sdl_show_messagebox_return = 0;
    g_mock_sdl_show_messagebox_buttonid = 0;
    g_mock_sdl_was_init_called = false;
    g_mock_sdl_init_subsystem_called = false;
    g_mock_sdl_show_messagebox_called = false;
}

TEST(crash_recovery_already_initialized_restore) {
    reset_mocks();
    // SDL is already initialized for video
    g_mock_sdl_was_init_return = SDL_INIT_VIDEO;
    // MessageBox returns success (0) and button 1 is selected ("Restore")
    g_mock_sdl_show_messagebox_return = 0;
    g_mock_sdl_show_messagebox_buttonid = 1;

    bool result = mocked::promptRestoreSession();

    ASSERT_TRUE(result);
    ASSERT_TRUE(g_mock_sdl_was_init_called);
    ASSERT_FALSE(g_mock_sdl_init_subsystem_called);  // should not initialize again
    ASSERT_TRUE(g_mock_sdl_show_messagebox_called);
}

TEST(crash_recovery_already_initialized_discard) {
    reset_mocks();
    g_mock_sdl_was_init_return = SDL_INIT_VIDEO;
    g_mock_sdl_show_messagebox_return = 0;
    g_mock_sdl_show_messagebox_buttonid = 0;  // "Discard"

    bool result = mocked::promptRestoreSession();

    ASSERT_FALSE(result);
    ASSERT_TRUE(g_mock_sdl_was_init_called);
    ASSERT_FALSE(g_mock_sdl_init_subsystem_called);
    ASSERT_TRUE(g_mock_sdl_show_messagebox_called);
}

TEST(crash_recovery_not_initialized_init_fail) {
    reset_mocks();
    g_mock_sdl_was_init_return = 0;         // not initialized
    g_mock_sdl_init_subsystem_return = -1;  // init fails

    bool result = mocked::promptRestoreSession();

    ASSERT_FALSE(result);
    ASSERT_TRUE(g_mock_sdl_was_init_called);
    ASSERT_TRUE(g_mock_sdl_init_subsystem_called);
    ASSERT_FALSE(g_mock_sdl_show_messagebox_called);  // should exit early
}

TEST(crash_recovery_not_initialized_init_success_show_fail) {
    reset_mocks();
    g_mock_sdl_was_init_return = 0;
    g_mock_sdl_init_subsystem_return = 0;    // init succeeds
    g_mock_sdl_show_messagebox_return = -1;  // show message box fails

    bool result = mocked::promptRestoreSession();

    ASSERT_FALSE(result);
    ASSERT_TRUE(g_mock_sdl_was_init_called);
    ASSERT_TRUE(g_mock_sdl_init_subsystem_called);
    ASSERT_TRUE(g_mock_sdl_show_messagebox_called);
}

TEST(crash_recovery_not_initialized_init_success_show_success_restore) {
    reset_mocks();
    g_mock_sdl_was_init_return = 0;
    g_mock_sdl_init_subsystem_return = 0;
    g_mock_sdl_show_messagebox_return = 0;
    g_mock_sdl_show_messagebox_buttonid = 1;

    bool result = mocked::promptRestoreSession();

    ASSERT_TRUE(result);
    ASSERT_TRUE(g_mock_sdl_was_init_called);
    ASSERT_TRUE(g_mock_sdl_init_subsystem_called);
    ASSERT_TRUE(g_mock_sdl_show_messagebox_called);
}
