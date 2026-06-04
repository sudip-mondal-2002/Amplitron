#pragma once
#include <string>
#include <thread>
#include <mutex>
#include <atomic>


namespace Amplitron {

class UpdateChecker {
public:
    UpdateChecker();
    ~UpdateChecker();

    void start_check();
    void shutdown();

    bool has_new_release() const;
    std::string new_release_version() const;
    std::string new_release_url() const;

private:
    void check_for_updates();

    std::thread check_thread_;
    mutable std::mutex mutex_;
    
    bool has_new_release_ = false;
    std::string new_release_version_;
    std::string new_release_url_;
    std::atomic<bool> shutdown_requested_{false};
};

} // namespace Amplitron
