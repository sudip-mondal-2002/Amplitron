#include "gui/update_checker.h"

#include <array>
#include <cstdio>
#include <iostream>
#include <memory>

#include "common.h"

namespace Amplitron {

UpdateChecker::UpdateChecker() = default;

UpdateChecker::~UpdateChecker() { shutdown(); }

void UpdateChecker::start_check() {
    if (!check_thread_.joinable()) {
        check_thread_ = std::thread([this]() { this->check_for_updates(); });
    }
}

void UpdateChecker::shutdown() {
    shutdown_requested_ = true;
    if (check_thread_.joinable()) {
        check_thread_.join();
    }
}

bool UpdateChecker::has_new_release() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return has_new_release_;
}

std::string UpdateChecker::new_release_version() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return new_release_version_;
}

std::string UpdateChecker::new_release_url() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return new_release_url_;
}

void UpdateChecker::check_for_updates() {
#ifndef AMPLITRON_NO_DESKTOP_SHELL
    const char* cmd =
        "curl -s https://api.github.com/repos/amplitron-dsp/Amplitron/releases/latest | grep "
        "'\"tag_name\":' | sed -E 's/.*\"([^\"]+)\".*/\\1/'";
    std::array<char, 128> buffer;
    std::string result;

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        return;
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        if (shutdown_requested_) return;
        result += buffer.data();
    }

    if (!result.empty()) {
        result.erase(result.find_last_not_of(" \n\r\t") + 1);

        std::lock_guard<std::mutex> lock(mutex_);
        if (result != AMPLITRON_VERSION && !result.empty() && result[0] == 'v') {
            has_new_release_ = true;
            new_release_version_ = result;
            new_release_url_ = "https://github.com/amplitron-dsp/Amplitron/releases/tag/" + result;
        }
    }
#endif
}

}  // namespace Amplitron
