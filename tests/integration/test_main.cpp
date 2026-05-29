#include "test_framework.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <csignal>

// On Windows, SDL_main.h (pulled in by SDL.h inside main.cpp) does
// `#define main SDL_main` unless SDL_MAIN_HANDLED is already defined.
// That rewrite would overwrite our own `#define main app_main` and leave
// app_main undeclared — causing the Windows CI build error.  Define
// SDL_MAIN_HANDLED here, before any SDL headers are transitively included.
#define SDL_MAIN_HANDLED

// Redefine main → app_main to avoid a linker conflict with the test runner's
// own main(), then include the production entry-point for black-box testing.
#define main app_main
#include "main.cpp"
#undef main

TEST(main_cli_help_exits_with_zero) {
    char* argv[] = { (char*)"amplitron", (char*)"--help" };
    
    // Redirect std::cout and std::cerr to keep test suite output clean
    std::streambuf* old_out = std::cout.rdbuf();
    std::streambuf* old_err = std::cerr.rdbuf();
    std::stringstream ss_out, ss_err;
    std::cout.rdbuf(ss_out.rdbuf());
    std::cerr.rdbuf(ss_err.rdbuf());
    
    int exit_code = app_main(2, argv);
    
    std::cout.rdbuf(old_out);
    std::cerr.rdbuf(old_err);
    
    ASSERT_EQ(exit_code, 0);
    ASSERT_TRUE(ss_out.str().find("Usage: amplitron") != std::string::npos);
}

TEST(main_cli_version_exits_with_zero) {
    char* argv[] = { (char*)"amplitron", (char*)"--version" };
    
    std::streambuf* old_out = std::cout.rdbuf();
    std::streambuf* old_err = std::cerr.rdbuf();
    std::stringstream ss_out, ss_err;
    std::cout.rdbuf(ss_out.rdbuf());
    std::cerr.rdbuf(ss_err.rdbuf());
    
    int exit_code = app_main(2, argv);
    
    std::cout.rdbuf(old_out);
    std::cerr.rdbuf(old_err);
    
    ASSERT_EQ(exit_code, 0);
    ASSERT_TRUE(ss_out.str().find("Amplitron v1.0") != std::string::npos);
}

TEST(main_signal_handler_resets_running_flag) {
    // Call the signal_handler directly to verify execution stability
    signal_handler(SIGINT);
    ASSERT_FALSE(g_running);
    
    // Reset g_running to true so subsequent frames or main loops aren't disrupted
    g_running = true;
    signal_handler(SIGTERM);
    ASSERT_FALSE(g_running);
    
    g_running = true; // reset
}
