#include "common.h"
#include "audio/audio_engine.h"
#include "gui/gui_manager.h"
#include "gui/gui_graph_state.h"
#include "preset_manager.h"
#include "cli.h"

#include "audio/effects/noise_gate.h"
#include "audio/effects/compressor.h"
#include "audio/effects/overdrive.h"
#include "audio/effects/distortion.h"
#include "audio/effects/equalizer.h"
#include "audio/effects/chorus.h"
#include "audio/effects/delay.h"
#include "audio/effects/reverb.h"
#include "audio/effects/cabinet_sim.h"
#include "audio/effects/amp_simulator.h"

#include <iostream>
#include <csignal>
#include <atomic>
#include <filesystem>

// New includes for Autosave and Recovery
#include "SessionManager.h"
#include "gui/CrashRecoveryUI.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#ifdef __ANDROID__
#include <SDL_main.h>
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IOS
#include <SDL_main.h>
#endif
#endif

static std::atomic<bool> g_running{true};

#ifdef __EMSCRIPTEN__
static Amplitron::GuiManager* g_gui = nullptr;

static void em_main_loop() {
    if (!g_gui || !g_gui->run_frame()) {
        g_running = false;
        emscripten_cancel_main_loop();
    }
}

extern "C" EMSCRIPTEN_KEEPALIVE void on_midi_cc(int channel, int cc, int value) {
    // Validate MIDI ranges before processing
    if (!g_gui) {
        emscripten_log(EM_LOG_WARN, "[MIDI] GUI not initialized, dropping event");
        return;
    }
    
    // Range validation (standard MIDI)
    if (channel < 0 || channel > 15) {
        emscripten_log(EM_LOG_DEBUG, "[MIDI] Invalid channel: %d", channel);
        return;
    }
    if (cc < 0 || cc > 127) {
        emscripten_log(EM_LOG_DEBUG, "[MIDI] Invalid CC number: %d", cc);
        return;
    }
    if (value < 0 || value > 127) {
        emscripten_log(EM_LOG_DEBUG, "[MIDI] Invalid CC value: %d", value);
        return;
    }
    
    // Create MIDI event
    Amplitron::MidiEvent event;
    event.status = static_cast<uint8_t>(0xB0 | (channel & 0x0F));  // CC message
    event.data1 = static_cast<uint8_t>(cc);
    event.data2 = static_cast<uint8_t>(value);
    event.pad = 0;
    
    // Inject into MIDI queue
    g_gui->midi_manager().inject_event(event);
    
    emscripten_log(EM_LOG_DEBUG, "[MIDI] CC %d = %d on channel %d", cc, value, channel);
}

extern "C" EMSCRIPTEN_KEEPALIVE void on_midi_device_connected(const char* device_name) {
    if (!g_gui || !device_name) return;
    
    emscripten_log(EM_LOG_INFO, "[MIDI] Device connected: %s", device_name);
}

extern "C" EMSCRIPTEN_KEEPALIVE void on_canvas_touch_gesture(float dx, float dy, float dscale, float local_x, float local_y) {
    auto& ui = Amplitron::GuiGraphState::get_instance();
    if (dscale != 0.0f) {
        float factor = 1.0f + dscale;
        float old_zoom = ui.target_zoom;
        float new_zoom = old_zoom * factor;
        if (new_zoom < 0.2f) new_zoom = 0.2f;
        if (new_zoom > 5.0f) new_zoom = 5.0f;
        float actual_factor = new_zoom / old_zoom;
        ui.target_scrolling.x = local_x - (local_x - ui.target_scrolling.x) * actual_factor;
        ui.target_scrolling.y = local_y - (local_y - ui.target_scrolling.y) * actual_factor;
        ui.target_zoom = new_zoom;
    }
    ui.target_scrolling.x += dx;
    ui.target_scrolling.y += dy;
}

extern "C" EMSCRIPTEN_KEEPALIVE bool is_canvas_hovered() {
    return Amplitron::GuiGraphState::get_instance().canvas_hovered;
}

extern "C" EMSCRIPTEN_KEEPALIVE float get_canvas_zoom() {
    return Amplitron::GuiGraphState::get_instance().target_zoom;
}

extern "C" EMSCRIPTEN_KEEPALIVE float get_canvas_scroll_x() {
    return Amplitron::GuiGraphState::get_instance().target_scrolling.x;
}

extern "C" EMSCRIPTEN_KEEPALIVE float get_canvas_scroll_y() {
    return Amplitron::GuiGraphState::get_instance().target_scrolling.y;
}

extern "C" EMSCRIPTEN_KEEPALIVE int get_node_count() {
    if (!g_gui) return 0;
    return static_cast<int>(g_gui->audio_engine().graph().get_nodes().size());
}

extern "C" EMSCRIPTEN_KEEPALIVE int get_link_count() {
    if (!g_gui) return 0;
    return static_cast<int>(g_gui->audio_engine().graph().get_links().size());
}

extern "C" EMSCRIPTEN_KEEPALIVE bool has_node_of_type(int routing_type) {
    if (!g_gui) return false;
    for (const auto& n : g_gui->audio_engine().graph().get_nodes()) {
        if (static_cast<int>(n.routing_type) == routing_type) return true;
    }
    return false;
}

extern "C" EMSCRIPTEN_KEEPALIVE int trigger_add_splitter_node() {
    if (!g_gui) return -1;
    auto& graph = g_gui->audio_engine().graph();
    int id = graph.add_node("Splitter", Amplitron::NodeRoutingType::Splitter);
    g_gui->audio_engine().commit_graph_changes();
    return id;
}

extern "C" EMSCRIPTEN_KEEPALIVE int trigger_add_link(int src_pin, int dst_pin) {
    if (!g_gui) return -1;
    auto& graph = g_gui->audio_engine().graph();
    int id = graph.add_link(src_pin, dst_pin);
    g_gui->audio_engine().commit_graph_changes();
    return id;
}

extern "C" EMSCRIPTEN_KEEPALIVE int get_node_output_pin_by_index(int node_index, int pin_index) {
    if (!g_gui) return -1;
    const auto& nodes = g_gui->audio_engine().graph().get_nodes();
    if (node_index < 0 || node_index >= static_cast<int>(nodes.size())) return -1;
    const auto& node = nodes[node_index];
    if (pin_index < 0 || pin_index >= static_cast<int>(node.output_pin_ids.size())) return -1;
    return node.output_pin_ids[pin_index];
}

extern "C" EMSCRIPTEN_KEEPALIVE int get_node_input_pin_by_index(int node_index, int pin_index) {
    if (!g_gui) return -1;
    const auto& nodes = g_gui->audio_engine().graph().get_nodes();
    if (node_index < 0 || node_index >= static_cast<int>(nodes.size())) return -1;
    const auto& node = nodes[node_index];
    if (pin_index < 0 || pin_index >= static_cast<int>(node.input_pin_ids.size())) return -1;
    return node.input_pin_ids[pin_index];
}

extern "C" EMSCRIPTEN_KEEPALIVE bool trigger_delete_last_node() {
    if (!g_gui) return false;
    auto& graph = g_gui->audio_engine().graph();
    const auto& nodes = graph.get_nodes();
    // Walk backwards to find the last deletable node (mirrors GUI rules: Input and Amp Sim are protected)
    for (int i = static_cast<int>(nodes.size()) - 1; i >= 0; --i) {
        const auto& node = nodes[i];
        if (node.name == "Input" || node.name == "Amp Sim") continue;
        bool ok = graph.remove_node(node.id);
        if (ok) {
            Amplitron::GuiGraphState::get_instance().node_positions.erase(node.id);
            g_gui->audio_engine().commit_graph_changes();
        }
        return ok;
    }
    return false; // No deletable node found
}

#endif

void signal_handler(int /*signal*/) {
    g_running = false;
}

int main(int argc, char* argv[]) {
    if (Amplitron::handle_cli_args(argc, argv)) {
        return 0;
    }

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Initialize Session Manager
    Amplitron::SessionManager sessionManager("SudipMondal", "Amplitron");

    std::cout << "=== Amplitron v1.0 - Guitar Amp Simulator ===" << std::endl;
    std::cout << "Starting up..." << std::endl;

    // Initialize audio engine
    Amplitron::AudioEngine engine;
    if (!engine.initialize()) {
        std::cerr << "Failed to initialize audio engine!" << std::endl;
        return 1;
    }

    // Create a small, automatically wired, and highly playable circuit
    auto cabinet = std::make_shared<Amplitron::CabinetSim>();
    cabinet->set_enabled(true);

    auto amp = std::make_shared<Amplitron::AmpSimulator>();
    amp->set_enabled(true);

    engine.add_initial_effects({cabinet, amp});

    engine.set_input_gain(0.7f);

    if (sessionManager.hasUnsavedSession()) {
        if (promptRestoreSession()) {
            try {
                nlohmann::json savedState = sessionManager.loadSession();
                engine.deserialize(savedState);
            } catch (const nlohmann::json::parse_error& e) {
                std::cerr << "Autosave file corrupted. Discarding." << std::endl;
                sessionManager.clearSession();
            }
        } else {
            sessionManager.clearSession();
        }
    }
    if (std::filesystem::exists("presets")) {
        Amplitron::PresetManager::set_presets_dir("presets");
    }

    Amplitron::GuiManager gui(engine);
    if (!gui.initialize(1280, 720)) {
        std::cerr << "Failed to initialize GUI!" << std::endl;
        engine.shutdown();
        return 1;
    }
    
    if (!engine.start()) {
        std::cerr << "Warning: Could not start audio stream." << std::endl;
    }

    std::cout << "Amplitron is ready. Let's play!" << std::endl;
#ifdef __EMSCRIPTEN__
    g_gui = &gui;
    emscripten_set_main_loop(em_main_loop, 0, 1);
#else
    while (g_running && gui.run_frame()) {
        if (sessionManager.shouldSave()) {
            sessionManager.saveSession(engine.serialize());
        }
    }
#endif

    // Cleanup
    std::cout << "Shutting down..." << std::endl;
#ifdef __EMSCRIPTEN__
    g_gui = nullptr;
#endif
    gui.shutdown();
    engine.shutdown();

    std::cout << "Goodbye!" << std::endl;
    return 0;
}
