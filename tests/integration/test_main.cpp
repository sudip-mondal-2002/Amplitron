#include <csignal>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "../fixtures/portaudio_mock.h"
#include "test_framework.h"

using namespace Amplitron;
using namespace TestFramework;

// On Windows, SDL_main.h (pulled in by SDL.h inside main.cpp) does
// `#define main SDL_main` unless SDL_MAIN_HANDLED is already defined.
// That rewrite would overwrite our own `#define main app_main` and leave
// app_main undeclared — causing the Windows CI build error. Define
// SDL_MAIN_HANDLED here, before any SDL headers are transitively included.
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif

// Redefine main → app_main to avoid a linker conflict with the test runner's
// own main(), then include the production entry-point for black-box testing.
#define main app_main
#include "main.cpp"
#undef main

static PaDeviceInfo mock_main_in1;
static PaDeviceInfo mock_main_in2;
static PaDeviceInfo mock_main_out1;
static PaDeviceInfo mock_main_out2;
static PaHostApiInfo mock_main_api;

static void setup_main_pa_mocks() {
    mock_main_api.type = paCoreAudio;
    mock_main_api.name = "MockAPI";
    mock_main_api.deviceCount = 4;

    mock_main_in1.name = "Mock Guitar Input";
    mock_main_in1.maxInputChannels = 2;
    mock_main_in1.maxOutputChannels = 0;
    mock_main_in1.hostApi = 0;
    mock_main_in1.defaultSampleRate = 44100;

    mock_main_in2.name = "Mock Aux Input";
    mock_main_in2.maxInputChannels = 2;
    mock_main_in2.maxOutputChannels = 0;
    mock_main_in2.hostApi = 0;
    mock_main_in2.defaultSampleRate = 44100;

    mock_main_out1.name = "Mock Speakers Output";
    mock_main_out1.maxInputChannels = 0;
    mock_main_out1.maxOutputChannels = 2;
    mock_main_out1.hostApi = 0;
    mock_main_out1.defaultSampleRate = 44100;

    mock_main_out2.name = "Mock Headphones Output";
    mock_main_out2.maxInputChannels = 0;
    mock_main_out2.maxOutputChannels = 2;
    mock_main_out2.hostApi = 0;
    mock_main_out2.defaultSampleRate = 44100;

    g_mock_pa_initialize = []() -> PaError { return paNoError; };
    g_mock_pa_get_host_api_count = []() { return 1; };
    g_mock_pa_get_device_count = []() { return 4; };
    g_mock_pa_get_host_api_info = [](int) -> const PaHostApiInfo* { return &mock_main_api; };
    g_mock_pa_host_api_device_index_to_device_index = [](int, int d) { return d; };
    g_mock_pa_get_device_info = [](int idx) -> const PaDeviceInfo* {
        if (idx == 0) return &mock_main_in1;
        if (idx == 1) return &mock_main_in2;
        if (idx == 2) return &mock_main_out1;
        if (idx == 3) return &mock_main_out2;
        return nullptr;
    };
    g_mock_pa_get_default_input_device = []() { return 0; };
    g_mock_pa_get_default_output_device = []() { return 2; };
    g_mock_pa_open_stream = [](PaStream** stream, const PaStreamParameters*,
                               const PaStreamParameters*, double, unsigned long, PaStreamFlags,
                               PaStreamCallback*, void*) -> PaError {
        *stream = (PaStream*)0x1234;
        return paNoError;
    };
    g_mock_pa_start_stream = [](PaStream*) -> PaError { return paNoError; };
    g_mock_pa_stop_stream = [](PaStream*) -> PaError { return paNoError; };
    g_mock_pa_close_stream = [](PaStream*) -> PaError { return paNoError; };
}

static void clear_main_pa_mocks() {
    g_mock_pa_initialize = nullptr;
    g_mock_pa_get_host_api_count = nullptr;
    g_mock_pa_get_device_count = nullptr;
    g_mock_pa_get_host_api_info = nullptr;
    g_mock_pa_host_api_device_index_to_device_index = nullptr;
    g_mock_pa_get_device_info = nullptr;
    g_mock_pa_get_default_input_device = nullptr;
    g_mock_pa_get_default_output_device = nullptr;
    g_mock_pa_open_stream = nullptr;
    g_mock_pa_start_stream = nullptr;
    g_mock_pa_stop_stream = nullptr;
    g_mock_pa_close_stream = nullptr;
}

TEST(main_cli_help_exits_with_zero) {
    char* argv[] = {(char*)"amplitron", (char*)"--help"};

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
    char* argv[] = {(char*)"amplitron", (char*)"--version"};

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

    g_running = true;  // reset
}

TEST(main_headless_full_execution) {
    setup_main_pa_mocks();

    // 1. Create a temporary preset file
    std::string preset_path = "temp_headless_preset_test.json";
    std::ofstream ofs(preset_path);
    ofs << R"({
        "format_version": 2,
        "nodes": [
            {
                "id": "n0",
                "type": "Input",
                "x": 0.0,
                "y": 0.0
            },
            {
                "id": "n1",
                "type": "Output",
                "x": 100.0,
                "y": 0.0
            }
        ],
        "links": [
            {
                "src": "n0.out0",
                "dst": "n1.in0"
            }
        ],
        "routing": "graph",
        "input_gain": 0.7,
        "output_gain": 0.8,
        "effects": [],
        "midi_mappings": []
    })";
    ofs.close();

    // 2. Prepare command arguments with matching inputs/outputs (single match)
    char* argv[] = {(char*)"amplitron", (char*)"--preset",  (char*)preset_path.c_str(),
                    (char*)"--input",   (char*)"Guitar",    (char*)"--output",
                    (char*)"Speakers",  (char*)"--headless"};

    // Redirect stdin/stdout/stderr
    std::streambuf* old_in = std::cin.rdbuf();
    std::streambuf* old_out = std::cout.rdbuf();
    std::streambuf* old_err = std::cerr.rdbuf();

    std::stringstream ss_in, ss_out, ss_err;
    ss_in << "gain 0.85\n";
    ss_in << "bypass 0\n";
    ss_in << "enable 0\n";
    ss_in << "telemetry off\n";
    ss_in << "telemetry on\n";
    ss_in << "chain\n";
    ss_in << "invalidcommand\n";

    std::cin.rdbuf(ss_in.rdbuf());
    std::cout.rdbuf(ss_out.rdbuf());
    std::cerr.rdbuf(ss_err.rdbuf());

    g_running = true;

    // 3. Spin up app_main in a background thread
    int exit_code = -999;
    std::thread main_thread([&exit_code, &argv]() { exit_code = app_main(8, argv); });

    // 4. Let it execute the loop for a short duration
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // 5. Stop the loop and join the thread
    g_running = false;
    if (main_thread.joinable()) {
        main_thread.join();
    }

    // Restore standard streams
    std::cin.rdbuf(old_in);
    std::cout.rdbuf(old_out);
    std::cerr.rdbuf(old_err);

    // Clean up file
    std::remove(preset_path.c_str());
    clear_main_pa_mocks();

    ASSERT_EQ(exit_code, 0);
}

TEST(main_headless_preset_not_found) {
    char* argv[] = {(char*)"amplitron", (char*)"--preset", (char*)"nonexistent_preset_file.json",
                    (char*)"--headless"};

    std::streambuf* old_out = std::cout.rdbuf();
    std::streambuf* old_err = std::cerr.rdbuf();
    std::stringstream ss_out, ss_err;
    std::cout.rdbuf(ss_out.rdbuf());
    std::cerr.rdbuf(ss_err.rdbuf());

    int exit_code = app_main(4, argv);

    std::cout.rdbuf(old_out);
    std::cerr.rdbuf(old_err);

    ASSERT_EQ(exit_code, 1);
}

TEST(main_headless_strict_requirement) {
    char* argv[] = {(char*)"amplitron", (char*)"--headless"};

    std::streambuf* old_out = std::cout.rdbuf();
    std::streambuf* old_err = std::cerr.rdbuf();
    std::stringstream ss_out, ss_err;
    std::cout.rdbuf(ss_out.rdbuf());
    std::cerr.rdbuf(ss_err.rdbuf());

    int exit_code = app_main(2, argv);

    std::cout.rdbuf(old_out);
    std::cerr.rdbuf(old_err);

    ASSERT_EQ(exit_code, 1);
}

TEST(main_headless_routing_warnings_and_failures) {
    setup_main_pa_mocks();

    std::string preset_path = "temp_headless_preset_test.json";
    std::ofstream ofs(preset_path);
    ofs << R"({
        "format_version": 2,
        "nodes": [],
        "links": [],
        "routing": "graph",
        "input_gain": 0.7,
        "output_gain": 0.8,
        "effects": [],
        "midi_mappings": []
    })";
    ofs.close();

    // 1. Test ambiguous matches (matches multiple) and warnings (matches 0)
    char* argv[] = {(char*)"amplitron",   (char*)"--preset", (char*)preset_path.c_str(),
                    (char*)"--input",
                    (char*)"Mock",  // Matches in1 and in2
                    (char*)"--output",
                    (char*)"NonExistent",  // Matches 0
                    (char*)"--headless"};

    std::streambuf* old_in = std::cin.rdbuf();
    std::streambuf* old_out = std::cout.rdbuf();
    std::streambuf* old_err = std::cerr.rdbuf();

    std::stringstream ss_in, ss_out, ss_err;
    ss_in << "telemetry off\n";

    std::cin.rdbuf(ss_in.rdbuf());
    std::cout.rdbuf(ss_out.rdbuf());
    std::cerr.rdbuf(ss_err.rdbuf());

    g_running = true;

    int exit_code = -999;
    std::thread main_thread([&exit_code, &argv]() { exit_code = app_main(8, argv); });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    g_running = false;
    if (main_thread.joinable()) {
        main_thread.join();
    }

    std::cin.rdbuf(old_in);
    std::cout.rdbuf(old_out);
    std::cerr.rdbuf(old_err);

    ASSERT_EQ(exit_code, 0);
    ASSERT_TRUE(ss_err.str().find("Ambiguous input name") != std::string::npos);
    ASSERT_TRUE(ss_err.str().find("Could not find requested output device") != std::string::npos);

    // 2. Test ambiguous output matches and input warning (matches 0)
    char* argv2[] = {(char*)"amplitron",   (char*)"--preset", (char*)preset_path.c_str(),
                     (char*)"--input",
                     (char*)"NonExistent",  // Matches 0
                     (char*)"--output",
                     (char*)"Mock",  // Matches out1 and out2
                     (char*)"--headless"};

    std::stringstream ss_in2, ss_out2, ss_err2;
    std::cin.rdbuf(ss_in2.rdbuf());
    std::cout.rdbuf(ss_out2.rdbuf());
    std::cerr.rdbuf(ss_err2.rdbuf());

    g_running = true;

    std::thread main_thread2([&exit_code, &argv2]() { exit_code = app_main(8, argv2); });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    g_running = false;
    if (main_thread2.joinable()) {
        main_thread2.join();
    }

    std::cin.rdbuf(old_in);
    std::cout.rdbuf(old_out);
    std::cerr.rdbuf(old_err);

    ASSERT_EQ(exit_code, 0);
    ASSERT_TRUE(ss_err2.str().find("Could not find requested input device") != std::string::npos);
    ASSERT_TRUE(ss_err2.str().find("Ambiguous output name") != std::string::npos);

    std::remove(preset_path.c_str());
    clear_main_pa_mocks();
}

TEST(main_audio_failures) {
    setup_main_pa_mocks();

    // 1. Force initialize() to fail
    g_mock_pa_initialize = []() -> PaError { return paNotInitialized; };

    char* argv[] = {(char*)"amplitron", (char*)"--preset", (char*)"not_used.json",
                    (char*)"--headless"};

    std::streambuf* old_in = std::cin.rdbuf();
    std::streambuf* old_out = std::cout.rdbuf();
    std::streambuf* old_err = std::cerr.rdbuf();
    std::stringstream ss_out, ss_err;
    std::cout.rdbuf(ss_out.rdbuf());
    std::cerr.rdbuf(ss_err.rdbuf());

    int exit_code = app_main(4, argv);

    std::cout.rdbuf(old_out);
    std::cerr.rdbuf(old_err);

    ASSERT_EQ(exit_code, 1);
    ASSERT_TRUE(ss_err.str().find("Failed to initialize audio engine!") != std::string::npos);

    // 2. Force start() to fail (warning only)
    g_mock_pa_initialize = []() -> PaError { return paNoError; };
    g_mock_pa_start_stream = [](PaStream*) -> PaError { return paInvalidChannelCount; };

    std::string preset_path = "temp_headless_preset_test.json";
    std::ofstream ofs(preset_path);
    ofs << R"({
        "format_version": 2,
        "nodes": [],
        "links": [],
        "routing": "graph",
        "input_gain": 0.7,
        "output_gain": 0.8,
        "effects": [],
        "midi_mappings": []
    })";
    ofs.close();

    char* argv2[] = {(char*)"amplitron", (char*)"--preset", (char*)preset_path.c_str(),
                     (char*)"--headless"};

    std::stringstream ss_in2, ss_out2, ss_err2;
    std::cin.rdbuf(ss_in2.rdbuf());
    std::cout.rdbuf(ss_out2.rdbuf());
    std::cerr.rdbuf(ss_err2.rdbuf());

    g_running = true;

    std::thread main_thread([&exit_code, &argv2]() { exit_code = app_main(4, argv2); });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    g_running = false;
    if (main_thread.joinable()) {
        main_thread.join();
    }

    std::cin.rdbuf(old_in);
    std::cout.rdbuf(old_out);
    std::cerr.rdbuf(old_err);

    ASSERT_EQ(exit_code, 0);
    ASSERT_TRUE(ss_err2.str().find("Warning: Could not start audio stream.") != std::string::npos);

    std::remove(preset_path.c_str());
    clear_main_pa_mocks();
}
