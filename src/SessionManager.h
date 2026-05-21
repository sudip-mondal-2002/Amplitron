#pragma once
#include <string>
#include <fstream>
#include <chrono>
#include <filesystem>
#include <SDL.h>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace Amplitron {

    class SessionManager {
    private:
        fs::path autoSavePath;
        fs::path tempSavePath;
        std::chrono::steady_clock::time_point lastSaveTime;
        const int autoSaveIntervalSeconds = 30;

    public:
        SessionManager(const std::string& orgName, const std::string& appName) 
            : lastSaveTime(std::chrono::steady_clock::now()) 
        {
            char* prefPath = SDL_GetPrefPath(orgName.c_str(), appName.c_str());
            if (prefPath) {
                std::string basePath(prefPath);
                SDL_free(prefPath);
                
                autoSavePath = fs::path(basePath) / "autosave.json";
                tempSavePath = fs::path(basePath) / "autosave.tmp";
            } else {
                autoSavePath = "autosave.json";
                tempSavePath = "autosave.tmp";
            }
        }

        bool hasUnsavedSession() const {
            return fs::exists(autoSavePath);
        }

        bool shouldSave() const {
            auto now = std::chrono::steady_clock::now();
            return std::chrono::duration_cast<std::chrono::seconds>(now - lastSaveTime).count() >= autoSaveIntervalSeconds;
        }

        void saveSession(const nlohmann::json& state) {
            std::ofstream tempFile(tempSavePath);
            if (tempFile.is_open()) {
                tempFile << state.dump(4);
                tempFile.close();

                std::error_code ec;
                fs::rename(tempSavePath, autoSavePath, ec);
            }
            lastSaveTime = std::chrono::steady_clock::now();
        }

        nlohmann::json loadSession() {
            nlohmann::json state;
            std::ifstream file(autoSavePath);
            if (file.is_open()) {
                file >> state;
            }
            return state;
        }

        void clearSession() {
            std::error_code ec;
            if (fs::exists(autoSavePath)) fs::remove(autoSavePath, ec);
            if (fs::exists(tempSavePath)) fs::remove(tempSavePath, ec);
        }
    };

}
