#pragma once
#include "test_framework.h"
#define private public
#include "audio/engine/audio_engine.h"
#undef private
#include "preset_manager.h"
#include <imgui.h>
#include <vector>
#include <string>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <cstdio>

namespace Amplitron {

class ScopedImGuiContext {
public:
    ScopedImGuiContext() {
        ctx_ = ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(1024, 768);
        io.DeltaTime = 1.0f / 60.0f;
        
        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        
        ImGui::NewFrame();
    }

    ~ScopedImGuiContext() {
        ImGui::Render();
        ImGui::DestroyContext(ctx_);
    }

private:
    ImGuiContext* ctx_ = nullptr;
};

class AudioEngineTest : public TestFramework::Test {
public:
    void SetUp() override {
        engine.initialize();
        engine.set_buffer_size(64);
    }
    
    void TearDown() override {
        engine.shutdown();
    }

protected:
    AudioEngine engine;
};

class PresetTest : public TestFramework::Test {
public:
    void SetUp() override {
        engine.initialize();
        original_presets_dir = PresetManager::get_presets_dir();
    }
    
    void TearDown() override {
        engine.shutdown();
        PresetManager::set_presets_dir(original_presets_dir);
        for (const auto& file : temp_files) {
            std::remove(file.c_str());
        }
        for (const auto& dir : temp_dirs) {
            std::filesystem::remove_all(dir);
        }
    }

protected:
    AudioEngine engine;
    std::string original_presets_dir;
    std::vector<std::string> temp_files;
    std::vector<std::string> temp_dirs;
    
    void register_temp_file(const std::string& path) {
        temp_files.push_back(path);
    }
    
    void register_temp_dir(const std::string& path) {
        temp_dirs.push_back(path);
    }
};

class EffectsTest : public TestFramework::Test {
public:
    void SetUp() override {
        std::memset(input_buffer, 0, sizeof(input_buffer));
        std::memset(output_buffer, 0, sizeof(output_buffer));
    }

protected:
    static constexpr int SR = 48000;
    static constexpr int BUFFER_SIZE = 512;
    float input_buffer[BUFFER_SIZE];
    float output_buffer[BUFFER_SIZE];
    
    void fill_sine(float freq, float amplitude = 1.0f) {
        for (int i = 0; i < BUFFER_SIZE; ++i) {
            input_buffer[i] = amplitude * std::sin(2.0f * 3.14159265f * freq * i / SR);
        }
    }
    
    void copy_input_to_output() {
        std::memcpy(output_buffer, input_buffer, sizeof(input_buffer));
    }
    
    float rms(const float* buf, int n) const {
        float sum = 0.0f;
        for (int i = 0; i < n; ++i) sum += buf[i] * buf[i];
        return std::sqrt(sum / n);
    }
    
    bool is_finite(const float* buf, int n) const {
        for (int i = 0; i < n; ++i) {
            if (!std::isfinite(buf[i])) return false;
        }
        return true;
    }
    
    float dft_magnitude_at(const float* buf, int n, float freq) const {
        float re = 0.0f, im = 0.0f;
        const float omega = 2.0f * 3.14159265f * freq / static_cast<float>(SR);
        for (int i = 0; i < n; ++i) {
            re += buf[i] * std::cos(omega * static_cast<float>(i));
            im += buf[i] * std::sin(omega * static_cast<float>(i));
        }
        return std::sqrt(re * re + im * im) / static_cast<float>(n);
    }
};

} // namespace Amplitron
