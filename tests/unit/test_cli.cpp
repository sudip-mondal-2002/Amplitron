#include "test_framework.h"
#include "cli.h"
#include <sstream>
#include <iostream>
#include <vector>
#include <string>

using namespace Amplitron;

TEST(cli_args_no_params_returns_false) {
    char* argv[] = { (char*)"amplitron" };
    Amplitron::CliOptions options = handle_cli_args(1, argv);
    bool exit_req = options.exit_early;
    ASSERT_FALSE(exit_req);
}

TEST(cli_args_help_returns_true_and_prints) {
    char* argv[] = { (char*)"amplitron", (char*)"--help" };
    
    // Redirect std::cout to capture the help message
    std::streambuf* old_buf = std::cout.rdbuf();
    std::stringstream ss;
    std::cout.rdbuf(ss.rdbuf());
    
    Amplitron::CliOptions options = handle_cli_args(2, argv);
    bool exit_req = options.exit_early;
    
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
    
    Amplitron::CliOptions options = handle_cli_args(2, argv);
    bool exit_req = options.exit_early;
    
    std::cout.rdbuf(old_buf);
    
    ASSERT_TRUE(exit_req);
    ASSERT_TRUE(ss.str().find("Usage: amplitron") != std::string::npos);
}

TEST(cli_args_version_returns_true_and_prints) {
    char* argv[] = { (char*)"amplitron", (char*)"--version" };
    
    std::streambuf* old_buf = std::cout.rdbuf();
    std::stringstream ss;
    std::cout.rdbuf(ss.rdbuf());
    
    Amplitron::CliOptions options = handle_cli_args(2, argv);
    bool exit_req = options.exit_early;
    
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
    
    Amplitron::CliOptions options = handle_cli_args(2, argv);
    bool exit_req = options.exit_early;
    
    std::cout.rdbuf(old_buf);
    
    ASSERT_TRUE(exit_req);
    ASSERT_TRUE(ss.str().find("Amplitron v1.0") != std::string::npos);
}

TEST(cli_args_unknown_returns_false) {
    char* argv[] = { (char*)"amplitron", (char*)"--some-random-flag" };
    
    std::streambuf* old_buf = std::cout.rdbuf();
    std::stringstream ss;
    std::cout.rdbuf(ss.rdbuf());
    
    Amplitron::CliOptions options = handle_cli_args(2, argv);
    bool exit_req = options.exit_early;
    
    std::cout.rdbuf(old_buf);
    
    ASSERT_FALSE(exit_req);
    ASSERT_TRUE(ss.str().empty());
}

TEST(cli_args_multiple_with_help_returns_true) {
    char* argv[] = { (char*)"amplitron", (char*)"--unknown", (char*)"--help" };
    
    std::streambuf* old_buf = std::cout.rdbuf();
    std::stringstream ss;
    std::cout.rdbuf(ss.rdbuf());
    
    Amplitron::CliOptions options = handle_cli_args(3, argv);
    bool exit_req = options.exit_early;
    
    std::cout.rdbuf(old_buf);
    
    ASSERT_TRUE(exit_req);
    ASSERT_TRUE(ss.str().find("Usage: amplitron") != std::string::npos);
}

TEST(cli_args_preset_parses_correctly) {
    char* argv[] = { (char*)"amplitron", (char*)"--preset", (char*)"my_tone.json" };
    Amplitron::CliOptions options = handle_cli_args(3, argv);
    
    ASSERT_FALSE(options.exit_early);
    ASSERT_TRUE(options.preset_path == "my_tone.json");
}

TEST(cli_args_input_and_output_parse_correctly) {
    char* argv[] = { (char*)"amplitron", (char*)"--input", (char*)"Focusrite", (char*)"--output", (char*)"JackRouter" };
    Amplitron::CliOptions options = handle_cli_args(5, argv);
    
    ASSERT_FALSE(options.exit_early);
    ASSERT_TRUE(options.input_device == "Focusrite");
    ASSERT_TRUE(options.output_device == "JackRouter");
}

TEST(cli_args_full_headless_config_parses_correctly) {
    char* argv[] = { (char*)"amplitron", (char*)"--headless", (char*)"--preset", (char*)"metal.json" };
    Amplitron::CliOptions options = handle_cli_args(4, argv);
    
    ASSERT_FALSE(options.exit_early);
    ASSERT_TRUE(options.is_headless);
    ASSERT_TRUE(options.preset_path == "metal.json");
}

TEST(cli_args_missing_preset_argument_returns_error) {
    char* argv[] = { (char*)"amplitron", (char*)"--preset" };
    
    // Capture std::cerr instead of std::cout since errors print to cerr
    std::streambuf* old_buf = std::cerr.rdbuf();
    std::stringstream ss;
    std::cerr.rdbuf(ss.rdbuf());
    
    Amplitron::CliOptions options = handle_cli_args(2, argv);
    
    std::cerr.rdbuf(old_buf);
    
    ASSERT_TRUE(options.exit_early);
    ASSERT_TRUE(options.exit_code == 1);
    ASSERT_TRUE(options.exit_reason == "Preset Field Empty");
}

TEST(cli_args_missing_input_argument_returns_error) {
    char* argv[] = { (char*)"amplitron", (char*)"--input" };
    
    std::streambuf* old_buf = std::cerr.rdbuf();
    std::stringstream ss;
    std::cerr.rdbuf(ss.rdbuf());
    
    Amplitron::CliOptions options = handle_cli_args(2, argv);
    
    std::cerr.rdbuf(old_buf);
    
    ASSERT_TRUE(options.exit_early);
    ASSERT_TRUE(options.exit_code == 1);
    ASSERT_TRUE(options.exit_reason == "Input Field Empty.");
}

TEST(cli_args_missing_output_argument_returns_error) {
    char* argv[] = { (char*)"amplitron", (char*)"--output" };
    
    std::streambuf* old_buf = std::cerr.rdbuf();
    std::stringstream ss;
    std::cerr.rdbuf(ss.rdbuf());
    
    Amplitron::CliOptions options = handle_cli_args(2, argv);
    
    std::cerr.rdbuf(old_buf);
    
    ASSERT_TRUE(options.exit_early);
    ASSERT_TRUE(options.exit_code == 1);
    ASSERT_TRUE(options.exit_reason == "Output Field Empty");
}

TEST(cli_args_headless_without_preset_returns_error) {
    char* argv[] = { (char*)"amplitron", (char*)"--headless" };
    
    std::streambuf* old_buf = std::cerr.rdbuf();
    std::stringstream ss;
    std::cerr.rdbuf(ss.rdbuf());
    
    Amplitron::CliOptions options = handle_cli_args(2, argv);
    
    std::cerr.rdbuf(old_buf);
    
    ASSERT_TRUE(options.exit_early);
    ASSERT_TRUE(options.exit_code == 1);
    ASSERT_TRUE(options.exit_reason == "Insufficient Arguments(no --preset)");
}

TEST(cli_args_flag_as_value_returns_error) {
    // User forgets the file name and types another flag immediately
    char* argv[] = { (char*)"amplitron", (char*)"--preset", (char*)"--headless" };
    
    std::streambuf* old_buf = std::cerr.rdbuf();
    std::stringstream ss;
    std::cerr.rdbuf(ss.rdbuf());
    
    Amplitron::CliOptions options = handle_cli_args(3, argv);
    
    std::cerr.rdbuf(old_buf);
    
    ASSERT_TRUE(options.exit_early);
    ASSERT_TRUE(options.exit_code == 1);
    ASSERT_TRUE(options.exit_reason == "Preset Field Empty");
}