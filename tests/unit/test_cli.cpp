#include "test_framework.h"
#include "cli.h"
#include <sstream>
#include <iostream>
#include <vector>
#include <string>

using namespace Amplitron;

TEST(cli_args_no_params_returns_false) {
    char* argv[] = { (char*)"amplitron" };
    bool exit_req = handle_cli_args(1, argv);
    ASSERT_FALSE(exit_req);
}

TEST(cli_args_help_returns_true_and_prints) {
    char* argv[] = { (char*)"amplitron", (char*)"--help" };
    
    // Redirect std::cout to capture the help message
    std::streambuf* old_buf = std::cout.rdbuf();
    std::stringstream ss;
    std::cout.rdbuf(ss.rdbuf());
    
    bool exit_req = handle_cli_args(2, argv);
    
    // Restore std::cout
    std::cout.rdbuf(old_buf);
    
    ASSERT_TRUE(exit_req);
    std::string output = ss.str();
    ASSERT_TRUE(output.find("Usage: amplitron") != std::string::npos);
    ASSERT_TRUE(output.find("--help") != std::string::npos);
}

TEST(cli_args_short_help_returns_true) {
    char* argv[] = { (char*)"amplitron", (char*)"-h" };
    
    std::streambuf* old_buf = std::cout.rdbuf();
    std::stringstream ss;
    std::cout.rdbuf(ss.rdbuf());
    
    bool exit_req = handle_cli_args(2, argv);
    
    std::cout.rdbuf(old_buf);
    
    ASSERT_TRUE(exit_req);
    ASSERT_TRUE(ss.str().find("Usage: amplitron") != std::string::npos);
}

TEST(cli_args_version_returns_true_and_prints) {
    char* argv[] = { (char*)"amplitron", (char*)"--version" };
    
    std::streambuf* old_buf = std::cout.rdbuf();
    std::stringstream ss;
    std::cout.rdbuf(ss.rdbuf());
    
    bool exit_req = handle_cli_args(2, argv);
    
    std::cout.rdbuf(old_buf);
    
    ASSERT_TRUE(exit_req);
    std::string output = ss.str();
    ASSERT_TRUE(output.find("Amplitron v1.0") != std::string::npos);
}

TEST(cli_args_short_version_returns_true) {
    char* argv[] = { (char*)"amplitron", (char*)"-v" };
    
    std::streambuf* old_buf = std::cout.rdbuf();
    std::stringstream ss;
    std::cout.rdbuf(ss.rdbuf());
    
    bool exit_req = handle_cli_args(2, argv);
    
    std::cout.rdbuf(old_buf);
    
    ASSERT_TRUE(exit_req);
    ASSERT_TRUE(ss.str().find("Amplitron v1.0") != std::string::npos);
}

TEST(cli_args_unknown_returns_false) {
    char* argv[] = { (char*)"amplitron", (char*)"--some-random-flag" };
    
    std::streambuf* old_buf = std::cout.rdbuf();
    std::stringstream ss;
    std::cout.rdbuf(ss.rdbuf());
    
    bool exit_req = handle_cli_args(2, argv);
    
    std::cout.rdbuf(old_buf);
    
    ASSERT_FALSE(exit_req);
    ASSERT_TRUE(ss.str().empty());
}

TEST(cli_args_multiple_with_help_returns_true) {
    char* argv[] = { (char*)"amplitron", (char*)"--unknown", (char*)"--help" };
    
    std::streambuf* old_buf = std::cout.rdbuf();
    std::stringstream ss;
    std::cout.rdbuf(ss.rdbuf());
    
    bool exit_req = handle_cli_args(3, argv);
    
    std::cout.rdbuf(old_buf);
    
    ASSERT_TRUE(exit_req);
    ASSERT_TRUE(ss.str().find("Usage: amplitron") != std::string::npos);
}
