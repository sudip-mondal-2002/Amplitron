/**
 * @file test_crash_recovery.cpp
 * @brief Tests for crash_recovery_ui headless behaviour.
 *
 * promptRestoreSession() is guarded by AMPLITRON_HEADLESS (defined in the test
 * binary via CMake) so it returns false immediately without opening any SDL
 * message box — no hang on macOS, no X11 error on Linux CI.
 */
#include "test_framework.h"
#include "gui/crash_recovery_ui.h"



TEST(crash_recovery_prompt_returns_false_in_headless_build) {
    // With AMPLITRON_HEADLESS defined, promptRestoreSession() must return false
    // immediately and must not block waiting for any GUI dialog.
    bool restore = promptRestoreSession();
    ASSERT_FALSE(restore);
}

TEST(crash_recovery_prompt_multiple_calls_are_stable) {
    // Repeated calls must each return false without side effects.
    for (int i = 0; i < 5; ++i) {
        ASSERT_FALSE(promptRestoreSession());
    }
}
