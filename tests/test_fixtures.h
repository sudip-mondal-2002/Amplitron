#pragma once
#include <imgui.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "test_framework.h"

#define private public
#define protected public
#include "audio/engine/audio_engine.h"
#include "audio/recorder/recorder.h"
#include "gui/gui_manager.h"
#include "gui/pedalboard/pedal_board.h"
#include "gui/pedalboard/pedal_widget.h"
#include "midi/midi_manager.h"
#undef private
#undef protected

#include "preset_manager.h"

namespace Amplitron {

class ScopedImGuiContext {
   public:
    ScopedImGuiContext() {
        ctx_ = ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
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

    void TearDown() override { engine.shutdown(); }

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
    PresetManager manager;
    std::string original_presets_dir;
    std::vector<std::string> temp_files;
    std::vector<std::string> temp_dirs;

    void register_temp_file(const std::string& path) { temp_files.push_back(path); }

    void register_temp_dir(const std::string& path) { temp_dirs.push_back(path); }
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

    void copy_input_to_output() { std::memcpy(output_buffer, input_buffer, sizeof(input_buffer)); }

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

struct TestAccessor {
    // MidiManager accessors
    static bool& learn_active(MidiManager& m) { return m.learn_active_; }
    static std::string& learn_effect_name(MidiManager& m) { return m.learn_effect_name_; }
    static std::string& learn_param_name(MidiManager& m) { return m.learn_param_name_; }
    static std::vector<MidiMapping>& mappings(MidiManager& m) { return m.mappings_; }
    static int& current_port(MidiManager& m) { return m.current_port_; }
    static std::string& current_port_name(MidiManager& m) { return m.current_port_name_; }
    static void call_midi_callback(MidiManager& mgr, double timestamp,
                                   std::vector<unsigned char>* message) {
        MidiManager::midi_callback(timestamp, message, &mgr);
    }
    static size_t get_queue_size(const MidiManager& mgr) { return mgr.midi_queue_.size(); }
    static bool pop_queue(MidiManager& mgr, MidiEvent& event) {
        return mgr.midi_queue_.try_pop(event);
    }

    // PedalWidget accessors
    static void commit_param_change(PedalWidget& w, int param_index, float old_val, float new_val) {
        w.commit_param_change(param_index, old_val, new_val);
    }
    static void render_footswitch_and_extras(PedalWidget& w, ImDrawList* dl, ImVec2 p0, ImVec2 p1,
                                             float pedal_width, float pedal_height, bool is_amp,
                                             bool enabled, bool& should_remove, float zoom) {
        w.render_footswitch_and_extras(dl, p0, p1, pedal_width, pedal_height, is_amp, enabled,
                                       should_remove, zoom);
    }
    static ImVec4 pedal_color(const PedalWidget& w) { return w.pedal_color_; }
    static ImVec4 led_color(const PedalWidget& w) { return w.led_color_; }
    static void render_knobs(PedalWidget& w, ImDrawList* dl, ImVec2 p0, float pedal_width,
                             bool is_amp, bool is_tuner, bool is_ir_cab, float zoom) {
        w.render_knobs(dl, p0, pedal_width, is_amp, is_tuner, is_ir_cab, zoom);
    }

    // PedalBoard accessors
    static void render_add_pedal_menu(PedalBoard& b) { b.render_add_pedal_menu(); }
    static void render_amp_selector(PedalBoard& b) { b.render_amp_selector(); }
    static void render_midi_menu(PedalBoard& b) { b.render_midi_menu(); }
    static void add_effect_and_show(PedalBoard& b, std::shared_ptr<Effect> effect) {
        b.add_effect_and_show(effect);
    }
    static void render_signal_chain(PedalBoard& b) { b.render_signal_chain(); }
};

inline void advance_frame() {
    ImGui::End();
    ImGui::Render();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");
}

}  // namespace Amplitron
