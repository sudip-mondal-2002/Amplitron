#pragma once
#include <iostream>
#include <string>

namespace Amplitron {

//Container for all terminal args
struct CliOptions {
    bool exit_early = false;
    bool is_headless = false;
    int exit_code = 0;
    std::string preset_path;
    std::string input_device;
    std::string output_device;
    std::string exit_reason;
};

inline CliOptions handle_cli_args(int argc, char* argv[]) {
    CliOptions options;
    auto has_value_token = [&](int idx) {
        return idx < argc && argv[idx] != nullptr && argv[idx][0] != '-';
    };
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: amplitron [options]\n\n"
                      << "Options:\n"
                      << "  -h, --help      Show this help message and exit\n"
                      << "  -v, --version   Show version information and exit\n"
                      << "\nAudio devices are configured via File -> Settings in the GUI.\n"
                      << "Visit https://github.com/sudip-mondal-2002/Amplitron for docs.\n";
            options.exit_early=true;
            options.exit_reason="Help Requested";
            return options;
        }
        else if (arg == "--version" || arg == "-v") {
            std::cout << "Amplitron v1.0\n";
            options.exit_early=true;
            options.exit_reason="Version Requested";
            return options;
        }
        else if(arg == "--headless" || arg == "--no-gui"){
            options.is_headless = true;
        }
        //i+1<argc prevents segfault if user does not provide sufficient arguments
        else if(arg == "--preset"){
            if(has_value_token(i + 1)){ 
                options.preset_path = argv[++i];
            } else{
                std::cerr << "Error: --preset requires a file path argument.\n";
                options.exit_early = true;
                options.exit_code = 1;
                options.exit_reason="Preset Field Empty";
                return options;
            }
        }
        else if(arg == "--input"){
            if(has_value_token(i + 1)){ 
                options.input_device = argv[++i];
            } else{
                std::cerr << "Error: --input requires a device name argument.\n";
                options.exit_early = true;
                options.exit_code = 1;
                options.exit_reason="Input Field Empty.";
                return options;
            }
        }
        else if(arg == "--output"){
            if(has_value_token(i + 1)){ 
                options.output_device = argv[++i];
            }else{
                std::cerr << "Error: --output requires a device name argument.\n";
                options.exit_early = true;
                options.exit_code = 1;
                options.exit_reason="Output Field Empty";
                return options;
            }
        }
    }

    //Check: can't run headless without a preset.
    if(options.is_headless && options.preset_path.empty()){
        std::cerr << "Error: --headless/--no-gui mode requires a --preset <path> argument.\n";
        options.exit_early = true;
        options.exit_code = 1;
        options.exit_reason="Insufficient Arguments(no --preset)";
    }
    return options;
}

} // namespace Amplitron