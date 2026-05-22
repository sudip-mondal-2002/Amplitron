#pragma once
#include <iostream>
#include <string>

namespace Amplitron {

// Returns true if the program should exit (--help or --version was passed)
inline bool handle_cli_args(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: amplitron [options]\n\n"
                      << "Options:\n"
                      << "  -h, --help      Show this help message and exit\n"
                      << "  -v, --version   Show version information and exit\n"
                      << "\nAudio devices are configured via File -> Settings in the GUI.\n"
                      << "Visit https://github.com/sudip-mondal-2002/Amplitron for docs.\n";
            return true;
        }
        if (arg == "--version" || arg == "-v") {
            std::cout << "Amplitron v1.0\n";
            return true;
        }
    }
    return false;
}

} // namespace Amplitron