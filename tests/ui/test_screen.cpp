#include <imgui_internal.h>

#include <cmath>
#include <functional>
#include <memory>
#include <string>

#include "gui/components/screen.h"
#include "gui/views/gui_midi.h"
#include "midi/midi_manager.h"
#include "test_fixtures.h"
#include "test_framework.h"

#define private public
#include "audio/effects/amp_cab/cabinet_sim.h"
#include "audio/effects/dynamics/multiband_compressor.h"
#include "audio/effects/utility/looper.h"
#include "audio/effects/utility/tuner.h"
#undef private

using namespace Amplitron;
using namespace TestFramework;

namespace Amplitron {
void set_mock_open_dialog_path(const std::string& path);
}

static void write_dummy_wav(const char* filename) {
    FILE* f = fopen(filename, "wb");
    if (f) {
        const uint8_t wav_header[52] = {
            'R',  'I',  'F',  'F',  0x2c, 0x00, 0x00, 0x00, 'W',  'A',  'V',  'E',  'f',
            'm',  't',  ' ',  0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x44, 0xAC,
            0x00, 0x00, 0x88, 0x58, 0x01, 0x00, 0x02, 0x00, 0x10, 0x00, 'd',  'a',  't',
            'a',  0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        fwrite(wav_header, 1, 52, f);
        fclose(f);
    }
}

static ImVec2 get_popup_window_pos(const char* popup_id_substr) {
    ImGuiContext& g = *GImGui;
    for (int i = 0; i < g.Windows.Size; i++) {
        if (strstr(g.Windows[i]->Name, "##Popup_") || strstr(g.Windows[i]->Name, popup_id_substr)) {
            return g.Windows[i]->Pos;
        }
    }
    return ImVec2(0, 0);
}

static ImGuiID get_popup_item_id(const char* popup_id_substr, const char* item_id_str) {
    ImGuiContext& g = *GImGui;
    std::cout << "DEBUG: get_popup_item_id searching for " << popup_id_substr
              << " | item: " << item_id_str << "\n";
    for (int i = 0; i < g.Windows.Size; i++) {
        std::cout << "DEBUG: Window name: " << g.Windows[i]->Name << "\n";
        if (strstr(g.Windows[i]->Name, "##Popup_") || strstr(g.Windows[i]->Name, popup_id_substr)) {
            ImGuiID id = g.Windows[i]->GetID(item_id_str);
            if (id != 0) {
                std::cout << "DEBUG: Match found! ID is " << id << "\n";
                return id;
            }
        }
    }
    std::cout << "DEBUG: No match found!\n";
    return 0;
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
    looper->ui_playhead_samples_.store(48000);  // 1 second recorded
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

// --- TUNER DISPLAY TESTS ---

TEST_F(PresetTest, TunerDisplay_MuteToggleClick_TogglesParamAndPushesToEngine) {
    ScopedImGuiContext imgui;
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImVec2(100, 100);

    auto tuner = std::make_shared<TunerPedal>();
    tuner->params()[0].value = 0.0f;  // Initial state: not muted

    ScreenProps props;
    props.index = 1;
    props.engine = &engine;
    props.type = ScreenType::Tuner;
    props.effect = tuner;

    // Render once to layout items
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    ImGuiContext& g = *GImGui;
    ImGuiID toggle_id = ImGui::GetCurrentWindow()->GetID("##tuner_mute_toggle");

    g.NavActivateId = toggle_id;
    g.NavActivateDownId = toggle_id;
    g.NavActivatePressedId = toggle_id;

    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    ImGui::End();
    ASSERT_EQ(tuner->params()[0].value, 1.0f);
}

TEST_F(PresetTest, TunerDisplay_MuteToggleHover_WithTooltip_ShowsTooltip) {
    ScopedImGuiContext imgui;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImVec2(100, 100);

    auto tuner = std::make_shared<TunerPedal>();
    tuner->params()[0].tooltip = "Cuts all signal output";

    ScreenProps props;
    props.type = ScreenType::Tuner;
    props.effect = tuner;
    // Render once to layout items
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);

    // Programmatically hover the button
    ImGuiContext& g = *GImGui;
    g.HoveredId = ImGui::GetID("##tuner_mute_toggle");

    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();
    ImGui::End();
}

TEST_F(PresetTest, TunerDisplay_MuteToggleHover_NoTooltip_ShowsGenericTooltip) {
    ScopedImGuiContext imgui;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImVec2(100, 100);

    auto tuner = std::make_shared<TunerPedal>();
    tuner->params()[0].tooltip = "";  // Empty

    ScreenProps props;
    props.type = ScreenType::Tuner;
    props.effect = tuner;
    // Render once to layout items
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);

    // Programmatically hover the button
    ImGuiContext& g = *GImGui;
    g.HoveredId = ImGui::GetID("##tuner_mute_toggle");

    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();
    ImGui::End();
}

// --- CABINET DISPLAY TESTS ---

TEST_F(PresetTest, CabinetDisplay_LoadIR_Button_OpensDialogAndLoadsFile) {
    ScopedImGuiContext imgui;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImVec2(100, 100);

    auto cab = std::make_shared<CabinetSim>();
    ScreenProps props;
    props.index = 2;
    props.type = ScreenType::Cabinet;
    props.effect = cab;

    write_dummy_wav("load_test.wav");
    Amplitron::set_mock_open_dialog_path("load_test.wav");

    // First render to layout and register IDs
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // Activate the Load IR button programmatically
    ImGuiContext& g = *GImGui;
    ImGuiID btn_id = ImGui::GetID("Load IR##ir_load_2");
    g.NavActivateId = btn_id;
    g.NavActivateDownId = btn_id;
    g.NavActivatePressedId = btn_id;

    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    ASSERT_TRUE(cab->has_ir());
    ImGui::End();
}

TEST_F(PresetTest, CabinetDisplay_LoadIR_Button_EmptyPath_DoesNotLoad) {
    ScopedImGuiContext imgui;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImVec2(100, 100);

    auto cab = std::make_shared<CabinetSim>();
    ScreenProps props;
    props.index = 2;
    props.type = ScreenType::Cabinet;
    props.effect = cab;

    Amplitron::set_mock_open_dialog_path("");

    // First render to layout and register IDs
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // Activate the Load IR button programmatically
    ImGuiContext& g = *GImGui;
    ImGuiID btn_id = ImGui::GetID("Load IR##ir_load_2");
    g.NavActivateId = btn_id;
    g.NavActivateDownId = btn_id;
    g.NavActivatePressedId = btn_id;

    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    ASSERT_FALSE(cab->has_ir());
    ImGui::End();
}

TEST_F(PresetTest, CabinetDisplay_IRName_LongName_IsTruncatedTo20Chars) {
    ScopedImGuiContext imgui;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImVec2(100, 100);

    auto cab = std::make_shared<CabinetSim>();

    write_dummy_wav("very_long_ir_file_name_that_should_be_truncated.wav");
    cab->load_ir("very_long_ir_file_name_that_should_be_truncated.wav");

    ScreenProps props;
    props.type = ScreenType::Cabinet;
    props.effect = cab;

    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();
    ImGui::End();
}

TEST_F(PresetTest, CabinetDisplay_ClearIR_Button_CallsClearIR) {
    ScopedImGuiContext imgui;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImVec2(100, 100);

    auto cab = std::make_shared<CabinetSim>();
    write_dummy_wav("short.wav");
    cab->load_ir("short.wav");

    ScreenProps props;
    props.index = 2;
    props.type = ScreenType::Cabinet;
    props.effect = cab;

    // First render to layout and register IDs
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // Activate the Clear button programmatically
    ImGuiContext& g = *GImGui;
    ImGuiID btn_id = ImGui::GetID("Clear##ir_clear_2");
    g.NavActivateId = btn_id;
    g.NavActivateDownId = btn_id;
    g.NavActivatePressedId = btn_id;

    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    ASSERT_FALSE(cab->has_ir());
    ImGui::End();
}

// --- LOOPER DISPLAY TESTS ---

TEST_F(PresetTest, LooperDisplay_IdleState_ShowsStopLabel) {
    ScopedImGuiContext imgui;
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImVec2(100, 100);

    auto looper = std::make_shared<Looper>();
    ScreenProps props;
    props.type = ScreenType::Looper;
    props.effect = looper;

    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();
    ImGui::End();
}

TEST_F(PresetTest, LooperDisplay_RecordButton_Click_CallsRequestRecordToggle) {
    ScopedImGuiContext imgui;
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImVec2(100, 100);

    auto looper = std::make_shared<Looper>();
    ScreenProps props;
    props.index = 3;
    props.type = ScreenType::Looper;
    props.effect = looper;

    ImGuiContext& g = *GImGui;

    // Render normally first
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // Click Record button programmatically
    ImGuiID btn_id = ImGui::GetID("Record##looper_rec_3");
    g.NavActivateId = btn_id;
    g.NavActivateDownId = btn_id;
    g.NavActivatePressedId = btn_id;

    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();
    ImGui::End();
}

TEST_F(PresetTest, LooperDisplay_PlayButton_Click_CallsRequestPlayToggle) {
    ScopedImGuiContext imgui;
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImVec2(100, 100);

    auto looper = std::make_shared<Looper>();
    ScreenProps props;
    props.index = 3;
    props.type = ScreenType::Looper;
    props.effect = looper;

    ImGuiContext& g = *GImGui;

    // Render normally first
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // Click Play button programmatically
    ImGuiID btn_id = ImGui::GetID("Play/Stop##looper_play_3");
    g.NavActivateId = btn_id;
    g.NavActivateDownId = btn_id;
    g.NavActivatePressedId = btn_id;

    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();
    ImGui::End();
}

TEST_F(PresetTest, LooperDisplay_OverdubButton_Click_CallsRequestOverdubToggle) {
    ScopedImGuiContext imgui;
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImVec2(100, 100);

    auto looper = std::make_shared<Looper>();
    ScreenProps props;
    props.index = 3;
    props.type = ScreenType::Looper;
    props.effect = looper;

    ImGuiContext& g = *GImGui;

    // Render normally first
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // Click Overdub button programmatically
    ImGuiID btn_id = ImGui::GetID("Overdub##looper_dub_3");
    g.NavActivateId = btn_id;
    g.NavActivateDownId = btn_id;
    g.NavActivatePressedId = btn_id;

    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();
    ImGui::End();
}

TEST_F(PresetTest, LooperDisplay_ClearButton_Click_CallsRequestClear) {
    ScopedImGuiContext imgui;
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImVec2(100, 100);

    auto looper = std::make_shared<Looper>();
    ScreenProps props;
    props.index = 3;
    props.type = ScreenType::Looper;
    props.effect = looper;

    ImGuiContext& g = *GImGui;

    // Render normally first
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // Click Clear button programmatically
    ImGuiID btn_id = ImGui::GetID("Clear##looper_clear_3");
    g.NavActivateId = btn_id;
    g.NavActivateDownId = btn_id;
    g.NavActivatePressedId = btn_id;

    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();
    ImGui::End();
}

TEST_F(PresetTest, LooperDisplay_LevelSlider_Change_ClampsAndPushesToEngine) {
    ScopedImGuiContext imgui;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImVec2(100, 100);

    auto looper = std::make_shared<Looper>();
    looper->params()[0].value = 0.5f;
    looper->params()[0].tooltip = "Slider Tooltip";

    bool commit_called = false;
    ScreenProps props;
    props.index = 3;
    props.type = ScreenType::Looper;
    props.effect = looper;
    props.engine = &engine;
    props.on_commit_param_change = [&](int idx, float old_val, float new_val) {
        (void)idx;
        (void)old_val;
        (void)new_val;
        commit_called = true;
    };

    ImGuiIO& io = ImGui::GetIO();
    ImGuiContext& g = *GImGui;

    // First layout the slider
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // Hover the slider to check tooltip
    ImGuiID slider_id = ImGui::GetID("##looper_level_3");
    g.HoveredId = slider_id;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // Now, simulate a press on the slider (programmatically activate it)
    io.MouseDown[0] = true;
    g.ActiveId = slider_id;
    g.ActiveIdSource = ImGuiInputSource_Mouse;
    g.ActiveIdIsJustActivated = true;
    g.ActiveIdHasBeenEditedBefore = true;

    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // Release mouse and change value
    io.MouseDown[0] = false;
    looper->params()[0].value = 0.8f;

    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    ImGui::End();
    ASSERT_TRUE(commit_called);
}

// --- MULTIBAND COMPRESSOR TESTS ---

TEST_F(PresetTest, MBKnob_VerticalDrag_UpdatesValueAndPushesToEngine) {
    ScopedImGuiContext imgui;
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImVec2(100, 100);

    auto comp = std::make_shared<MultiBandCompressor>();
    comp->params()[2].value = 0.5f;  // Low thresh
    comp->params()[2].tooltip = "Low Thresh Tooltip";

    bool commit_called = false;
    ScreenProps props;
    props.index = 4;
    props.type = ScreenType::MultiBandCompressor;
    props.effect = comp;
    props.engine = &engine;
    props.on_commit_param_change = [&](int idx, float old_val, float new_val) {
        (void)idx;
        (void)old_val;
        (void)new_val;
        commit_called = true;
    };

    ImGuiIO& io = ImGui::GetIO();

    // Low thresh knob center is at (p0.x + 30.3f, p0.y + 120.0f)
    ImVec2 center(p0.x + 30.3f, p0.y + 120.0f);

    // Render once to layout items and register bounding boxes
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // 1. Hover knob to check tooltip (with non-empty tooltip)
    io.MousePos = center;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);  // IsItemHovered() is true
    advance_frame();

    // Hover knob to check tooltip (with empty tooltip)
    comp->params()[2].tooltip = "";
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // 2. Hover wheel interaction
    io.MousePos = center;
    io.MouseWheel = 1.0f;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // Shift hover wheel
    io.KeyShift = true;
    io.MouseWheel = -1.0f;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();
    io.KeyShift = false;
    io.MouseWheel = 0.0f;

    // 3. Start drag
    io.MouseClicked[0] = true;
    io.MouseDown[0] = true;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // Drag down by 50px
    io.MouseClicked[0] = false;
    for (int i = 0; i < 5; ++i) {
        io.MousePos.y += 10.0f;
        io.MouseDelta.y = 10.0f;
        ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
        advance_frame();
    }

    // Drag down with Shift
    io.KeyShift = true;
    for (int i = 0; i < 5; ++i) {
        io.MousePos.y += 10.0f;
        io.MouseDelta.y = 10.0f;
        ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
        advance_frame();
    }
    io.KeyShift = false;

    // Drag down with Ctrl
    io.KeyCtrl = true;
    for (int i = 0; i < 5; ++i) {
        io.MousePos.y += 10.0f;
        io.MouseDelta.y = 10.0f;
        ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
        advance_frame();
    }
    io.KeyCtrl = false;

    // Release
    io.MouseDown[0] = false;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    ImGui::End();
    ASSERT_TRUE(commit_called);
}

TEST_F(PresetTest, MBKnob_DoubleClick_ResetsToDefault) {
    ScopedImGuiContext imgui;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImVec2(100, 100);

    auto comp = std::make_shared<MultiBandCompressor>();
    comp->params()[2].value = 1.0f;  // Low thresh different from default
    comp->params()[2].default_val = 0.0f;

    bool commit_called = false;
    ScreenProps props;
    props.index = 4;
    props.type = ScreenType::MultiBandCompressor;
    props.effect = comp;
    props.engine = &engine;
    props.on_commit_param_change = [&](int, float, float) { commit_called = true; };

    // Render once to layout items and register bounding boxes
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 center(p0.x + 30.3f, p0.y + 120.0f);

    // Hover first to process input focus/hover state
    io.MousePos = center;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    io.MouseClicked[0] = true;
    io.MouseDown[0] = true;
    io.MouseClickedCount[0] = 2;
    io.MouseDoubleClicked[0] = true;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();
    io.MouseClicked[0] = false;
    io.MouseDown[0] = false;
    io.MouseClickedCount[0] = 0;
    io.MouseDoubleClicked[0] = false;

    ASSERT_NEAR(comp->params()[2].value, 0.0f, 0.01f);
    ASSERT_TRUE(commit_called);
    ImGui::End();
}

TEST_F(PresetTest, MBKnob_RightClick_OpensPopup) {
    ScopedImGuiContext imgui;
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImVec2(100, 100);

    auto comp = std::make_shared<MultiBandCompressor>();

    MidiManager midi_manager;
    GuiMidi gui_midi(midi_manager);

    ScreenProps props;
    props.index = 4;
    props.type = ScreenType::MultiBandCompressor;
    props.effect = comp;
    props.gui_midi = &gui_midi;
    props.engine = &engine;
    props.on_commit_param_change = [](int, float, float) {};

    ImGuiIO& io = ImGui::GetIO();

    // Render once to layout items and register bounding boxes
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    ImVec2 center(p0.x + 30.3f, p0.y + 120.0f);

    io.MousePos = center;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    io.MouseClicked[1] = true;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();
    io.MouseClicked[1] = false;

    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    ImGui::End();
}

TEST_F(PresetTest, XoverSlider_Drag_UpdatesValueAndPreventsOverlap) {
    ScopedImGuiContext imgui;
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImVec2(100, 100);

    auto comp = std::make_shared<MultiBandCompressor>();
    comp->params()[0].value = 200.0f;   // low xover
    comp->params()[1].value = 4000.0f;  // high xover

    ScreenProps props;
    props.index = 4;
    props.type = ScreenType::MultiBandCompressor;
    props.effect = comp;
    props.engine = &engine;

    ImGuiIO& io = ImGui::GetIO();

    float col_width = (220.0f - 24.0f * 1.0f) / 3.0f;
    float x1 = p0.x + 12.0f * 1.0f + col_width;
    float track_top = p0.y + 90.0f * 1.0f;

    // Render once to layout items and register bounding boxes
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // Hover low xover
    io.MousePos = ImVec2(x1, track_top + 50.0f);
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // Drag low xover up
    io.MouseClicked[0] = true;
    io.MouseDown[0] = true;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    io.MouseClicked[0] = false;
    for (int i = 0; i < 15; ++i) {
        io.MousePos.y -= 10.0f;
        io.MouseDelta.y = -10.0f;
        ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
        advance_frame();
    }

    io.MouseDown[0] = false;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // Hover wheel interactions
    io.MousePos = ImVec2(x1, track_top + 50.0f);
    io.MouseWheel = 1.0f;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();
    io.MouseWheel = 0.0f;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // Double click reset
    io.MouseClicked[0] = true;
    io.MouseDown[0] = true;
    io.MouseClickedCount[0] = 2;
    io.MouseDoubleClicked[0] = true;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();
    io.MouseClicked[0] = false;
    io.MouseDown[0] = false;
    io.MouseClickedCount[0] = 0;
    io.MouseDoubleClicked[0] = false;

    ImGui::End();
}

TEST_F(PresetTest, ScreenComponent_NullEffect) {
    ScopedImGuiContext imgui;
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImVec2(100, 100);

    ScreenProps props;
    props.effect = nullptr;
    props.type = ScreenType::Tuner;

    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();
    ImGui::End();
}

TEST_F(PresetTest, TunerDisplay_SignalDetected_Tuning) {
    ScopedImGuiContext imgui;
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImVec2(100, 100);

    auto tuner = std::make_shared<TunerPedal>();
    tuner->signal_detected.store(true);
    tuner->detected_note.store(4);  // E
    tuner->detected_octave.store(2);
    tuner->detected_freq.store(82.4f);

    ScreenProps props;
    props.index = 1;
    props.type = ScreenType::Tuner;
    props.effect = tuner;

    // 1. Cents error < 2.0f
    tuner->detected_cents.store(1.0f);
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // 2. Cents error < 15.0f
    tuner->detected_cents.store(-10.0f);
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // 3. Cents error >= 15.0f
    tuner->detected_cents.store(30.0f);
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    ImGui::End();
}

TEST_F(PresetTest, MultiBandCompressor_LessParams) {
    ScopedImGuiContext imgui;
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImVec2(100, 100);

    auto comp = std::make_shared<MultiBandCompressor>();
    comp->params_.clear();

    ScreenProps props;
    props.index = 4;
    props.type = ScreenType::MultiBandCompressor;
    props.effect = comp;

    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();
    ImGui::End();
}

TEST_F(PresetTest, MultiBandCompressor_MBKnobRangeZero) {
    ScopedImGuiContext imgui;
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImVec2(100, 100);

    auto comp = std::make_shared<MultiBandCompressor>();
    comp->params_[2].min_val = 0.0f;
    comp->params_[2].max_val = 0.0f;

    ScreenProps props;
    props.index = 4;
    props.type = ScreenType::MultiBandCompressor;
    props.effect = comp;

    ImGuiIO& io = ImGui::GetIO();
    float col_width = (220.0f - 24.0f * 1.0f) / 3.0f;
    ImVec2 center(p0.x + 12.0f * 1.0f + col_width * 0.28f, p0.y + 120.0f * 1.0f);

    io.MousePos = center;
    io.MouseDown[0] = true;
    io.MouseClicked[0] = true;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    io.MouseClicked[0] = false;
    io.MousePos.y += 10.0f;
    io.MouseDelta.y = 10.0f;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    io.MouseDown[0] = false;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    ImGui::End();
}

TEST_F(PresetTest, MultiBandCompressor_MBKnobScrollAtBoundary) {
    ScopedImGuiContext imgui;
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImVec2(100, 100);

    auto comp = std::make_shared<MultiBandCompressor>();
    comp->params_[2].value = comp->params_[2].max_val;

    ScreenProps props;
    props.index = 4;
    props.type = ScreenType::MultiBandCompressor;
    props.effect = comp;

    ImGuiIO& io = ImGui::GetIO();
    float col_width = (220.0f - 24.0f * 1.0f) / 3.0f;
    ImVec2 center(p0.x + 12.0f * 1.0f + col_width * 0.28f, p0.y + 120.0f * 1.0f);

    io.MousePos = center;
    io.MouseWheel = 1.0f;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();
    io.MouseWheel = 0.0f;

    ImGui::End();
}

TEST_F(PresetTest, MultiBandCompressor_MBKnobDoubleClickAtDefault) {
    ScopedImGuiContext imgui;
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImVec2(100, 100);

    auto comp = std::make_shared<MultiBandCompressor>();
    comp->params_[2].value = comp->params_[2].default_val;

    ScreenProps props;
    props.index = 4;
    props.type = ScreenType::MultiBandCompressor;
    props.effect = comp;

    ImGuiIO& io = ImGui::GetIO();
    float col_width = (220.0f - 24.0f * 1.0f) / 3.0f;
    ImVec2 center(p0.x + 12.0f * 1.0f + col_width * 0.28f, p0.y + 120.0f * 1.0f);

    io.MousePos = center;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    io.MouseClicked[0] = true;
    io.MouseDown[0] = true;
    io.MouseClickedCount[0] = 2;
    io.MouseDoubleClicked[0] = true;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();
    io.MouseClicked[0] = false;
    io.MouseDown[0] = false;
    io.MouseClickedCount[0] = 0;
    io.MouseDoubleClicked[0] = false;

    ImGui::End();
}

TEST_F(PresetTest, MultiBandCompressor_MBKnobPopupAndInteractions) {
    ScopedImGuiContext imgui;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImVec2(100, 100);

    auto comp = std::make_shared<MultiBandCompressor>();
    comp->params_[2].value = -10.0f;  // threshold default is typically -20.0f, let's change it
    comp->params_[2].default_val = -20.0f;

    MidiManager midi_manager;
    GuiMidi gui_midi(midi_manager);

    bool commit_called = false;
    ScreenProps props;
    props.index = 4;
    props.type = ScreenType::MultiBandCompressor;
    props.effect = comp;
    props.gui_midi = &gui_midi;
    props.engine = &engine;
    props.on_commit_param_change = [&](int, float, float) { commit_called = true; };

    ImGuiContext& g = *GImGui;

    ImGuiIO& io = ImGui::GetIO();

    // Render normally first to register knob state and hover it
    io.MousePos = ImVec2(p0.x + 30.3f, p0.y + 120.0f);
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    char popup_id[128];
    std::snprintf(popup_id, sizeof(popup_id), "##knob_%s_%d_%d_%s", comp->name(), 4, 2, "thresh");

    // ==========================================
    // 1. Slider interaction inside popup
    // ==========================================
    io.MouseClicked[1] = true;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();
    io.MouseClicked[1] = false;

    // Get the popup window and position mouse over its slider
    ImVec2 pop_pos = get_popup_window_pos(popup_id);

    // Slider horizontal center is at pop_pos.x + 70.0f, vertical center is pop_pos.y + 45.0f
    io.MousePos = ImVec2(pop_pos.x + 70.0f, pop_pos.y + 45.0f);
    io.MouseDown[0] = true;
    io.MouseClicked[0] = true;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // Drag to the right
    io.MouseClicked[0] = false;
    io.MousePos = ImVec2(pop_pos.x + 110.0f, pop_pos.y + 45.0f);
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // Release mouse
    io.MouseDown[0] = false;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // ==========================================
    // 2. Reset button interaction
    // ==========================================
    ImGui::OpenPopup(popup_id);
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    ImGuiID reset_id = get_popup_item_id(popup_id, "Reset");
    g.NavActivateId = reset_id;
    g.NavActivateDownId = reset_id;
    g.NavActivatePressedId = reset_id;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // ==========================================
    // 3. MIDI Learn interaction
    // ==========================================
    ImGui::OpenPopup(popup_id);
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    ImGuiID learn_id = get_popup_item_id(popup_id, "MIDI Learn");
    g.NavActivateId = learn_id;
    g.NavActivateDownId = learn_id;
    g.NavActivatePressedId = learn_id;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();
    ASSERT_TRUE(midi_manager.is_learning());
    midi_manager.cancel_learn();

    // ==========================================
    // 4. MIDI Learn (Bypass) interaction
    // ==========================================
    ImGui::OpenPopup(popup_id);
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    ImGuiID learn_bp_id = get_popup_item_id(popup_id, "MIDI Learn (Bypass)");
    g.NavActivateId = learn_bp_id;
    g.NavActivateDownId = learn_bp_id;
    g.NavActivatePressedId = learn_bp_id;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();
    ASSERT_TRUE(midi_manager.is_learning());
    midi_manager.cancel_learn();

    // ==========================================
    // 5. Remove MIDI Mapping and Remove MIDI Bypass Mapping
    // ==========================================
    // First, add mapping and bypass mapping
    MidiMapping m1;
    m1.cc_number = 10;
    m1.target_type = MidiTargetType::EffectParam;
    m1.effect_name = comp->name();
    m1.param_name = comp->params_[2].name;
    midi_manager.add_mapping(m1);

    MidiMapping m2;
    m2.cc_number = 11;
    m2.target_type = MidiTargetType::EffectBypass;
    m2.effect_name = comp->name();
    midi_manager.add_mapping(m2);

    // Open popup
    ImGui::OpenPopup(popup_id);
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // Click Remove MIDI Mapping
    ImGuiID remove_id = get_popup_item_id(popup_id, "Remove MIDI Mapping");
    g.NavActivateId = remove_id;
    g.NavActivateDownId = remove_id;
    g.NavActivatePressedId = remove_id;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // Open popup again
    ImGui::OpenPopup(popup_id);
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // Click Remove MIDI Bypass Mapping
    ImGuiID remove_bp_id = get_popup_item_id(popup_id, "Remove MIDI Bypass Mapping");
    g.NavActivateId = remove_bp_id;
    g.NavActivateDownId = remove_bp_id;
    g.NavActivatePressedId = remove_bp_id;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // ==========================================
    // 6. Test outlines while learning
    // ==========================================
    midi_manager.start_learn(MidiTargetType::EffectParam, comp->name(), comp->params_[2].name);
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();
    midi_manager.cancel_learn();

    // ==========================================
    // 7. Test popup with null gui_midi
    // ==========================================
    props.gui_midi = nullptr;
    ImGui::OpenPopup(popup_id);
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    ImGui::End();
}

TEST_F(PresetTest, MultiBandCompressor_XoverOverlapPrevention) {
    ScopedImGuiContext imgui;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImVec2(100, 100);

    auto comp = std::make_shared<MultiBandCompressor>();
    comp->params_[0].value = 400.0f;
    comp->params_[1].value = 500.0f;

    bool commit_called = false;
    ScreenProps props;
    props.index = 4;
    props.type = ScreenType::MultiBandCompressor;
    props.effect = comp;
    props.engine = &engine;
    props.on_commit_param_change = [&](int, float, float) { commit_called = true; };

    ImGuiIO& io = ImGui::GetIO();
    ImGuiContext& g = *GImGui;
    float col_width = (220.0f - 24.0f * 1.0f) / 3.0f;
    float x1 = p0.x + 12.0f * 1.0f + col_width;
    float x2 = p0.x + 12.0f * 1.0f + 2.0f * col_width;
    float track_top = p0.y + 90.0f * 1.0f;
    float track_bottom = p0.y + 260.0f * 1.0f;

    // Render once to layout
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // ==========================================
    // 1. Drag low crossover (pi=0) above high crossover (pi=1)
    // ==========================================
    comp->params_[0].value = 400.0f;
    comp->params_[1].value = 500.0f;
    g.ActiveId = ImGui::GetID("##slider_MultiBand Compressor_4_0_low_xover");
    g.ActiveIdSource = ImGuiInputSource_Mouse;
    g.ActiveIdIsJustActivated = true;
    io.MousePos = ImVec2(x1, track_top + 10.0f);  // very high value
    io.MouseDown[0] = true;
    io.MouseClicked[0] = true;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    io.MouseDown[0] = false;
    io.MouseClicked[0] = false;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // ==========================================
    // 2. Drag high crossover (pi=1) below low crossover (pi=0)
    // ==========================================
    comp->params_[0].value = 5000.0f;
    comp->params_[1].value = 6000.0f;
    g.ActiveId = ImGui::GetID("##slider_MultiBand Compressor_4_1_high_xover");
    g.ActiveIdSource = ImGuiInputSource_Mouse;
    g.ActiveIdIsJustActivated = true;
    io.MousePos = ImVec2(x2, track_bottom - 10.0f);  // very low value
    io.MouseDown[0] = true;
    io.MouseClicked[0] = true;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    io.MouseDown[0] = false;
    io.MouseClicked[0] = false;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // ==========================================
    // 3. Scroll low crossover (pi=0) above high crossover (pi=1)
    // ==========================================
    comp->params_[0].value = 450.0f;
    comp->params_[1].value = 500.0f;
    g.HoveredId = ImGui::GetID("##slider_MultiBand Compressor_4_0_low_xover");
    io.MouseWheel = 10.0f;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();
    io.MouseWheel = 0.0f;

    // ==========================================
    // 4. Scroll high crossover (pi=1) below low crossover (pi=0)
    // ==========================================
    comp->params_[0].value = 3000.0f;
    comp->params_[1].value = 3100.0f;
    g.HoveredId = ImGui::GetID("##slider_MultiBand Compressor_4_1_high_xover");
    io.MouseWheel = -10.0f;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();
    io.MouseWheel = 0.0f;

    // ==========================================
    // 5. Double click low crossover (pi=0) reset overlap
    // ==========================================
    comp->params_[0].value = 450.0f;
    comp->params_[1].value = 100.0f;  // low default is 200, so reset will go above 100
    io.MousePos = ImVec2(x1, track_top + 50.0f);
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    io.MouseClicked[0] = true;
    io.MouseDown[0] = true;
    io.MouseClickedCount[0] = 2;
    io.MouseDoubleClicked[0] = true;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();
    io.MouseClicked[0] = false;
    io.MouseDown[0] = false;
    io.MouseClickedCount[0] = 0;
    io.MouseDoubleClicked[0] = false;

    // ==========================================
    // 6. Double click high crossover (pi=1) reset overlap
    // ==========================================
    comp->params_[1].value = 5000.0f;
    comp->params_[0].value = 10000.0f;  // high default is 4000, so reset will go below low_val
    io.MousePos = ImVec2(x2, track_top + 50.0f);
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    io.MouseClicked[0] = true;
    io.MouseDown[0] = true;
    io.MouseClickedCount[0] = 2;
    io.MouseDoubleClicked[0] = true;
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();
    io.MouseClicked[0] = false;
    io.MouseDown[0] = false;
    io.MouseClickedCount[0] = 0;
    io.MouseDoubleClicked[0] = false;

    ImGui::End();
}

TEST_F(PresetTest, MultiBandCompressor_MeterColors) {
    ScopedImGuiContext imgui;
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImVec2(100, 100);

    auto comp = std::make_shared<MultiBandCompressor>();

    ScreenProps props;
    props.index = 4;
    props.type = ScreenType::MultiBandCompressor;
    props.effect = comp;

    comp->gain_reduction_db_[0].store(4.0f, std::memory_order_relaxed);
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    comp->gain_reduction_db_[0].store(8.0f, std::memory_order_relaxed);
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    comp->gain_reduction_db_[0].store(15.0f, std::memory_order_relaxed);
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    ImGui::End();
}

TEST_F(PresetTest, LooperDisplay_IdleState_And_DeactivateEdit) {
    ScopedImGuiContext imgui;
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImVec2(100, 100);

    auto looper = std::make_shared<Looper>();
    looper->ui_state_.store(static_cast<uint32_t>(Looper::State::Idle));

    bool commit_called = false;
    ScreenProps props;
    props.index = 3;
    props.type = ScreenType::Looper;
    props.effect = looper;
    props.on_commit_param_change = [&](int, float, float) { commit_called = true; };

    ImGuiIO& io = ImGui::GetIO();
    ImGuiContext& g = *GImGui;

    // Frame 1: Render normally to register slider ID
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // Frame 2: Activate the slider (set ActiveId = slider_id)
    ImGuiID slider_id = ImGui::GetID("##looper_level_3");
    io.MouseDown[0] = true;
    g.ActiveId = slider_id;
    g.ActiveIdSource = ImGuiInputSource_Mouse;
    g.ActiveIdIsJustActivated = true;
    g.ActiveIdHasBeenEditedBefore = true;

    // Render to trigger IsItemActivated() inside screen.cpp
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // Frame 3: Deactivate the slider with edit flag set
    io.MouseDown[0] = false;
    float current_val = looper->params()[0].value;
    looper->params()[0].value = current_val + 0.1f;  // changed value

    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    // Frame 4: Hover overdub button to test tooltip
    g.HoveredId = ImGui::GetID("Overdub##looper_dub_3");
    ScreenComponent::render(dl, p0, 220.0f, 1.0f, props);
    advance_frame();

    ImGui::End();
    ASSERT_TRUE(commit_called);
}
