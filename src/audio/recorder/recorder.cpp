#include "audio/recorder/recorder.h"
#include "audio/recorder/recorder_impl.h"
#include <iostream>
#include <ctime>
#include <cstring>
#include <cstdio>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

namespace Amplitron {

void mkdirs(const std::string& path) {
    std::string current;
    for (char c : path) {
        current += c;
        if (c == '/' || c == '\\') {
            MKDIR(current.c_str());
        }
    }
    MKDIR(path.c_str());
}

Recorder::Recorder() {
    for (auto& v : waveform_buf_) v.store(0.0f);
}

Recorder::~Recorder() {
    if (recording_) stop();
}

std::string Recorder::get_recordings_dir() {
#if defined(__APPLE__)
    const char* home = std::getenv("HOME");
    if (home) {
        std::string dir = std::string(home) + "/Documents/Amplitron/recordings";
        mkdirs(dir);
        return dir;
    }
#elif !defined(_WIN32)
    const char* home = std::getenv("HOME");
    if (home) {
        std::string dir = std::string(home) + "/.local/share/amplitron/recordings";
        mkdirs(dir);
        return dir;
    }
#endif
    std::string dir = "recordings";
    MKDIR(dir.c_str());
    return dir;
}

std::string Recorder::generate_filename() {
    std::time_t now = std::time(nullptr);
    char buf[64];
    std::tm time_info;
#ifdef _WIN32
    localtime_s(&time_info, &now);
#else
    localtime_r(&now, &time_info);
#endif
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &time_info);
    return get_recordings_dir() + "/" + std::string(buf) + ".wav";
}

} // namespace Amplitron
