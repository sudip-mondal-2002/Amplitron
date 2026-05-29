#include "common.h"
#include "audio/engine/audio_engine.h"
#ifndef AMPLITRON_HEADLESS
#include "gui/gui_manager.h"
#include "gui/state/gui_graph_state.h"
// New include for Recovery
#include "gui/crash_recovery_ui.h"
#endif
#include "preset_manager.h"
#include "cli.h"
#include "gui/commands/command_graph.h"
#include "gui/commands/command_history.h"

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
#include <thread>
#include <chrono>
#include <mutex>
#include <vector>

// New include for Autosave
#include "session_manager.h"

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

extern "C" EMSCRIPTEN_KEEPALIVE void on_canvas_touch_down(float x, float y) {
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent(x, y);
    io.AddMouseButtonEvent(0, true);
}

extern "C" EMSCRIPTEN_KEEPALIVE void on_canvas_touch_move(float x, float y) {
    ImGui::GetIO().AddMousePosEvent(x, y);
}

extern "C" EMSCRIPTEN_KEEPALIVE void on_canvas_touch_up(float x, float y) {
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent(x, y);
    io.AddMouseButtonEvent(0, false);
}

extern "C" EMSCRIPTEN_KEEPALIVE void on_canvas_touch_cancel() {
    auto& io = ImGui::GetIO();
    io.AddMouseButtonEvent(0, false);
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
    auto cmd = std::make_unique<Amplitron::AddGraphNodeCommand>(
        g_gui->audio_engine(), "Splitter", Amplitron::NodeRoutingType::Splitter, nullptr, ImVec2(0, 0));
    auto* raw = cmd.get();
    g_gui->command_history().execute(std::move(cmd));
    return (raw->node_id != -1) ? raw->node_id : -1;
}

extern "C" EMSCRIPTEN_KEEPALIVE int trigger_add_link(int src_pin, int dst_pin) {
    if (!g_gui) return -1;
    auto cmd = std::make_unique<Amplitron::AddGraphLinkCommand>(g_gui->audio_engine(), src_pin, dst_pin);
    auto* raw = cmd.get();
    g_gui->command_history().execute(std::move(cmd));
    return raw->was_successful ? raw->link.id : -1;
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
        
        auto& ui_positions = Amplitron::GuiGraphState::get_instance().node_positions;
        ImVec2 pos(0, 0);
        auto pos_it = ui_positions.find(node.id);
        if (pos_it != ui_positions.end()) pos = pos_it->second.position;

        g_gui->command_history().execute(
            std::make_unique<Amplitron::RemoveGraphNodeCommand>(
                g_gui->audio_engine(), node.id, node.routing_type, pos
            )
        );
        // Note: node_positions.erase is handled inside RemoveGraphNodeCommand::execute()
        return true;
    }
    return false; // No deletable node found
}

#endif

void signal_handler(int /*signal*/) {
    g_running = false;
}

int main(int argc, char* argv[]) {
    //breaks global iostream lock
    std::cin.tie(nullptr);
    //CLI argument parsing
    Amplitron::CliOptions cli_opts = Amplitron::handle_cli_args(argc, argv);

    #ifdef AMPLITRON_HEADLESS
    cli_opts.is_headless = true;
    
    // If the parser didn't already trigger an exit (like --help) validate the preset
    if (!cli_opts.exit_early && cli_opts.preset_path.empty()) {
        std::cerr << "Error: Strict headless build requires a --preset <path> argument." << std::endl;
        return 1; // Return non-zero failure code
    }
    #endif

    if(cli_opts.exit_early){
        std::cout << "[CLI]Application exited early :" << cli_opts.exit_reason << std::endl;
        return cli_opts.exit_code;
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

    #ifndef AMPLITRON_HEADLESS
    std::unique_ptr<Amplitron::GuiManager> gui = nullptr;
    #endif

    if(cli_opts.is_headless){
        std::cout << "=== HEADLESS MODE ===" << std::endl;
        std::cout << "Loading preset: " << cli_opts.preset_path << std::endl;
        
        //Safe preset injection
        if (!Amplitron::PresetManager::load_preset(cli_opts.preset_path, engine, nullptr)){
            std::cerr << "Fatal Error: Could not load preset for headless mode." << std::endl;
            engine.shutdown();
            return 1;
        }
        //Hardware routing(i/p)
        if(!cli_opts.input_device.empty()){
            auto devices = engine.get_input_devices();
            std::vector<int> match_indices;

            //Searching and storing all matching devices
            for(size_t i = 0; i < devices.size(); ++i){
                if (devices[i].name.find(cli_opts.input_device) != std::string::npos){
                    match_indices.push_back(i);
                }
            }
            
            if(match_indices.empty()){
                std::cerr << "Warning: Could not find requested input device: '" << cli_opts.input_device << "'" << std::endl;
            } else if (match_indices.size() == 1){
                engine.set_input_device(devices[match_indices[0]].index);
                std::cout << "Input routed to: " << devices[match_indices[0]].name << std::endl;
            } else {
                std::cerr << "Warning: Ambiguous input name '" << cli_opts.input_device << "'. Multiple matches found:" << std::endl;
                for(int idx : match_indices){
                    std::cerr << " -- " << devices[idx].name << std::endl;
                }
                std::cerr << "Auto-selecting the first match: " << devices[match_indices[0]].name << std::endl;
                engine.set_input_device(devices[match_indices[0]].index);
            }
        }
        //hardware routing(o/p)
        if(!cli_opts.output_device.empty()) {
            auto devices = engine.get_output_devices();
            std::vector<int> match_indices;

            for(size_t i = 0;i <devices.size(); ++i){
                if(devices[i].name.find(cli_opts.output_device) != std::string::npos) {
                    match_indices.push_back(i);
                }
            }

            if(match_indices.empty()){
                std::cerr << "Warning: Could not find requested output device: '" << cli_opts.output_device << "'" << std::endl;
            } else if(match_indices.size() == 1){
                engine.set_output_device(devices[match_indices[0]].index);
                std::cout<< "Output routed to: " << devices[match_indices[0]].name << std::endl;
            } else{
                std::cerr << "Warning: Ambiguous output name '" << cli_opts.output_device << "'. Multiple matches found:" <<std::endl;
                for(int idx : match_indices){
                    std::cerr << " -- " << devices[idx].name << std::endl;
                }
                std::cerr << "Auto-selecting the first match: " << devices[match_indices[0]].name << std::endl;
                engine.set_output_device(devices[match_indices[0]].index);
            }
        }

    } 
    #ifndef AMPLITRON_HEADLESS
    else {
    // GUI bootup
        gui = std::make_unique<Amplitron::GuiManager>(engine);
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

        if (!gui->initialize(1280, 720)) {
            std::cerr << "Failed to initialize GUI!" << std::endl;
            engine.shutdown();
            return 1;
        }
    }
    #endif
    
    
    if (!engine.start()) {
        std::cerr << "Warning: Could not start audio stream." << std::endl;
    }

    std::cout << "Amplitron is ready. Let's play!" << std::endl;
#ifdef __EMSCRIPTEN__
    g_gui = gui.get();
    emscripten_set_main_loop(em_main_loop, 0, 1);
#else
    std::atomic<bool> show_telemetry{true};

    if (cli_opts.is_headless){
        std::cout << "Audio Engine is running in the background." << std::endl;
        std::cout << "Commands: chain, gain <val>, bypass <idx>, enable <idx>, telemetry <on/off>" << std::endl;
        std::cout << "Press Ctrl+C to shut down." << std::endl;

        std::mutex cli_mutex;
        std::vector<std::string> cli_commands;

        //stdin thread to listen commands
        std::thread stdin_listener([&cli_mutex, &cli_commands](){
            std::string line;
            while(std::getline(std::cin, line)){
                if(line.empty()) continue;
                std::lock_guard<std::mutex> lock(cli_mutex);
                cli_commands.push_back(line);
            }
        });
        stdin_listener.detach();
        int loop_counter=0;

        //headless loop
        while(g_running){
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::vector<std::string> pending_commands;
            {
                std::lock_guard<std::mutex> lock(cli_mutex);
                std::swap(pending_commands,cli_commands);
            }

            for(const std::string& line : pending_commands){     
            if(line.find("gain ") == 0){
                    try{
                        float val = std::stof(line.substr(5));
                        engine.set_output_gain(val);
                        std::cout << ">> Output gain set to " << val << std::endl;
                    } catch (...){
                        std::cout << ">> Invalid gain" << std::endl;
                    }
                } else if (line.find("bypass ") == 0){
                    try {
                        int idx = std::stoi(line.substr(7));
                        //push_effect_enabled expects a float(>0.5 is enabled, <0.5 is bypassed)
                        engine.push_effect_enabled(idx, 0.0f);
                        std::cout << ">> Effect " << idx << " bypassed." << std::endl;
                    } catch(...){
                        std::cout << ">> Invalid index." << std::endl;
                    }
                } else if (line.find("enable ") == 0){
                    try{
                        int idx = std::stoi(line.substr(7));
                        engine.push_effect_enabled(idx, 1.0f);
                        std::cout << ">> Effect " << idx << " enabled." << std::endl;
                    } catch(...){
                        std::cout << ">> Invalid index. Try: enable 0" << std::endl;
                    }
                } else if (line == "telemetry off"){
                    show_telemetry.store(false, std::memory_order_relaxed);
                    std::cout << ">> Telemetry muted. Type 'telemetry on' to resume." << std::endl;
                }
                  else if (line == "telemetry on"){
                    show_telemetry.store(true, std::memory_order_relaxed);
                    std::cout << ">> Telemetry resumed." << std::endl;
                } else if (line == "chain"){
                    std::string chain_str = "\n=== ACTIVE SIGNAL CHAIN ===\n";
                    
                    const auto& nodes = engine.graph().get_nodes();
                    int print_index = 0;
                    
                    for (const auto& node : nodes) {
                        if (node.pedal) { // Only print actual effects
                            chain_str += "[" + std::to_string(print_index) + "] " + 
                                         node.pedal->get_display_name() + 
                                         (node.pedal->is_enabled() ? " (ON)\n" : " (BYPASSED)\n");
                            print_index++;
                        }
                    }
                    
                    // If no actual pedals were found in the graph
                    if (print_index == 0) {
                        chain_str += "(Chain is empty)\n";
                    }

                    chain_str += "===========================";
                    std::cout << chain_str << std::endl;
                }
                 else {
                    std::cout << ">> Unknown command. Available: gain <val>, bypass <idx>, enable <idx>, telemetry <on/off>" << std::endl;
                }
            }
            //telemetry logic(activates every 10s)
            if(++loop_counter >= 100){
                if(show_telemetry.load(std::memory_order_relaxed)) { 
                    float dsp_load = engine.get_cpu_load() * 100.0f;
                    float in_peak = engine.get_input_level();
                    float out_peak = engine.get_output_level();
                    std::string dashboard = "\n========================================\n";
                    dashboard += "Active Buffer Size: " + std::to_string(engine.get_buffer_size()) + 
                                 " samples @ " + std::to_string(engine.get_sample_rate()) + "Hz\n";
                    dashboard += "DSP Load: " + std::to_string(dsp_load) + "%\n";
                    dashboard += "Peak I/O : IN " + std::to_string(in_peak) + " | OUT " + std::to_string(out_peak) + "\n";
                    dashboard += "========================================\n";
                    
                    std::cout << dashboard << std::flush;
                }
                loop_counter = 0;//reset timer
            }
        }
    } 
    #ifndef AMPLITRON_HEADLESS
    else{
        //GUI loop
        while(g_running && gui->run_frame()){
            if (sessionManager.shouldSave()){
                sessionManager.saveSession(engine.serialize());
            }
        }
    }
    #endif
#endif

    // Cleanup
    std::cout << "Shutting down..." << std::endl;
    
#ifdef __EMSCRIPTEN__
    g_gui = nullptr;
#endif

#ifndef AMPLITRON_HEADLESS
    if(!cli_opts.is_headless){ 
        gui->shutdown();
    }
#endif

    if(!cli_opts.is_headless) {
        sessionManager.clearSession(); 
    }
    engine.shutdown();
    std::cout << "Goodbye!" << std::endl;

    return 0;
}
