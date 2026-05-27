#include "test_framework.h"
#include "test_fixtures.h"
#include <string>
#include <memory>
#include <cmath>
#include <functional>

#include "gui/components/screen.h"
#include "gui/views/gui_midi.h"
#include "midi/midi_manager.h"

#define private public
#include "audio/effects/tuner.h"
#include "audio/effects/cabinet_sim.h"
#include "audio/effects/looper.h"
#include "audio/effects/multiband_compressor.h"
#undef private

using namespace Amplitron;
using namespace TestFramework;

// Reusable helper to complete the current frame and begin a new one within a TestWindow context
static inline void advance_frame() {
    ImGui::End();
    ImGui::Render();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");
}

TEST_F(PresetTest, test_screen_component_comprehensive) {
    ScopedImGuiContext imgui;
    
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImVec2(100, 100);
    float width = 220.0f;
    float zoom = 1.0f;

    MidiManager midi_manager;
    GuiMidi gui_midi(midi_manager);

    ScreenProps props;
    props.index = 0;
    props.engine = &engine;
    props.gui_midi = &gui_midi;
    props.on_commit_param_change = [](int, float, float) {};

    // 1. Tuner Screen
    auto tuner = std::make_shared<TunerPedal>();
    props.type = ScreenType::Tuner;
    props.effect = tuner;
    
    // Disabled tuner
    tuner->set_enabled(false);
    ScreenComponent::render(dl, p0, width, zoom, props);
    advance_frame();
    
    // Active tuner, no signal
    tuner->set_enabled(true);
    ScreenComponent::render(dl, p0, width, zoom, props);
    advance_frame();
    
    // Active tuner, with signal
    tuner->detected_note.store(9);
    tuner->detected_octave.store(2);
    tuner->detected_cents.store(10.5f);
    tuner->detected_freq.store(110.0f);
    tuner->signal_detected.store(true);
    ScreenComponent::render(dl, p0, width, zoom, props);
    advance_frame();

    // Test mute toggle button
    ImGuiIO& io = ImGui::GetIO();
    io.MousePos = ImVec2(p0.x + width * 0.5f, p0.y + 155.0f * zoom);
    io.MouseClicked[0] = true;
    ScreenComponent::render(dl, p0, width, zoom, props);
    advance_frame();
    io.MouseClicked[0] = false;
    ScreenComponent::render(dl, p0, width, zoom, props);
    advance_frame();

    // 2. Cabinet Sim Screen
    auto cab = std::make_shared<CabinetSim>();
    props.type = ScreenType::Cabinet;
    props.effect = cab;
    
    // Parametric mode (no IR)
    ScreenComponent::render(dl, p0, width, zoom, props);
    advance_frame();
    
    // Simulate mic placement dragging
    io.MousePos = ImVec2(p0.x + width * 0.5f, p0.y + 40.0f);
    io.MouseDown[0] = true;
    io.MouseClicked[0] = true;
    ScreenComponent::render(dl, p0, width, zoom, props);
    advance_frame();
    io.MouseDown[0] = false;
    io.MouseClicked[0] = false;
    ScreenComponent::render(dl, p0, width, zoom, props);
    advance_frame();

    // Mock having an IR
    cab->raw_ir_samples_.push_back(1.0f);
    cab->ir_name_ = "TestCabinetIR.wav";
    cab->ir_duration_ms_ = 20.0f;
    ScreenComponent::render(dl, p0, width, zoom, props);
    advance_frame();

    // Simulate clicking the Clear button inside the Cabinet display
    io.MousePos = ImVec2(p0.x + width * 0.5f, p0.y + 118.0f * zoom);
    io.MouseClicked[0] = true;
    ScreenComponent::render(dl, p0, width, zoom, props);
    advance_frame();
    io.MouseClicked[0] = false;
    ScreenComponent::render(dl, p0, width, zoom, props);
    advance_frame();

    // Clear IR direct call
    cab->clear_ir();
    ASSERT_FALSE(cab->has_ir());

    // 3. Looper Screen
    auto looper = std::make_shared<Looper>();
    props.type = ScreenType::Looper;
    props.effect = looper;

    // State: Empty
    looper->ui_state_.store(static_cast<uint32_t>(Looper::State::Empty));
    looper->ui_has_loop_.store(0);
    ScreenComponent::render(dl, p0, width, zoom, props);
    advance_frame();

    // Hover and Click Record button
    io.MousePos = ImVec2(p0.x + 30 * zoom, p0.y + 110 * zoom);
    io.MouseClicked[0] = true;
    ScreenComponent::render(dl, p0, width, zoom, props);
    advance_frame();
    io.MouseClicked[0] = false;
    ScreenComponent::render(dl, p0, width, zoom, props);
    advance_frame();

    // State: Recording
    looper->ui_state_.store(static_cast<uint32_t>(Looper::State::Recording));
    looper->ui_has_loop_.store(0);
    looper->ui_playhead_samples_.store(48000); // 1 second recorded
    ScreenComponent::render(dl, p0, width, zoom, props);
    advance_frame();

    // State: Playing
    looper->ui_state_.store(static_cast<uint32_t>(Looper::State::Playing));
    looper->ui_has_loop_.store(1);
    looper->ui_playhead_samples_.store(24000);
    looper->ui_loop_length_samples_.store(96000);
    ScreenComponent::render(dl, p0, width, zoom, props);
    advance_frame();

    // State: Overdubbing
    looper->ui_state_.store(static_cast<uint32_t>(Looper::State::Overdubbing));
    ScreenComponent::render(dl, p0, width, zoom, props);
    advance_frame();

    // 4. MultiBand Compressor Screen
    auto mb = std::make_shared<MultiBandCompressor>();
    props.type = ScreenType::MultiBandCompressor;
    props.effect = mb;

    // Standard drawing
    ScreenComponent::render(dl, p0, width, zoom, props);
    advance_frame();

    // Drag low crossover slider
    io.MousePos = ImVec2(p0.x + width * 0.33f, p0.y + 120.0f);
    io.MouseDown[0] = true;
    io.MouseClicked[0] = true;
    ScreenComponent::render(dl, p0, width, zoom, props);
    advance_frame();
    io.MouseClicked[0] = false;
    
    // Drag to prevent overlap (low beyond high)
    mb->params()[0].value = 800.0f;
    mb->params()[1].value = 1000.0f;
    io.MousePos = ImVec2(p0.x + width * 0.33f, p0.y + 80.0f);
    ScreenComponent::render(dl, p0, width, zoom, props);
    advance_frame();
    io.MouseDown[0] = false;
    ScreenComponent::render(dl, p0, width, zoom, props);
    advance_frame();

    // Double click crossover slider
    io.MousePos = ImVec2(p0.x + width * 0.33f, p0.y + 120.0f);
    io.MouseDoubleClicked[0] = true;
    ScreenComponent::render(dl, p0, width, zoom, props);
    advance_frame();
    io.MouseDoubleClicked[0] = false;
    ScreenComponent::render(dl, p0, width, zoom, props);
    advance_frame();

    // Scroll crossover slider
    io.MousePos = ImVec2(p0.x + width * 0.33f, p0.y + 120.0f);
    io.MouseWheel = 1.0f;
    ScreenComponent::render(dl, p0, width, zoom, props);
    advance_frame();
    io.MouseWheel = 0.0f;
    ScreenComponent::render(dl, p0, width, zoom, props);
    advance_frame();

    // Interact with low threshold knob (pi=2)
    ImVec2 knob_center = ImVec2(p0.x + 30.0f, p0.y + 120.0f);
    io.MousePos = knob_center;
    io.MouseDown[0] = true;
    io.MouseClicked[0] = true;
    ScreenComponent::render(dl, p0, width, zoom, props);
    advance_frame();
    io.MouseClicked[0] = false;

    io.MouseDelta = ImVec2(0.0f, 10.0f);
    ScreenComponent::render(dl, p0, width, zoom, props);
    advance_frame();
    io.MouseDown[0] = false;
    ScreenComponent::render(dl, p0, width, zoom, props);
    advance_frame();

    // Scroll knob
    io.MousePos = knob_center;
    io.MouseWheel = -1.0f;
    ScreenComponent::render(dl, p0, width, zoom, props);
    advance_frame();
    io.MouseWheel = 0.0f;
    ScreenComponent::render(dl, p0, width, zoom, props);
    advance_frame();

    // Double click knob
    io.MouseDoubleClicked[0] = true;
    ScreenComponent::render(dl, p0, width, zoom, props);
    advance_frame();
    io.MouseDoubleClicked[0] = false;
    ScreenComponent::render(dl, p0, width, zoom, props);
    advance_frame();

    // Right-click to open popup
    io.MouseClicked[1] = true;
    ScreenComponent::render(dl, p0, width, zoom, props);
    advance_frame();
    io.MouseClicked[1] = false;
    
    // Call render again when popup is open so that the popup content is rendered
    ScreenComponent::render(dl, p0, width, zoom, props);

    ImGui::End();
}
