#include "common.h"
#include "audio/audio_engine.h"
#include "gui/gui_manager.h"
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

#include <iostream>
#include <csignal>
#include <atomic>
#include <filesystem>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// SDL2's Android and iOS JNI/UIKit bridge locates the app entry point by
// looking up the symbol "SDL_main" at runtime.  SDL_main.h renames our
// main() to SDL_main() via a macro so the bridge can find it.  Without this
// include the app crashes immediately on launch on both platforms.
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

    std::cout << "=== Amplitron v1.0 - Guitar Amp Simulator ===" << std::endl;
    std::cout << "Starting up..." << std::endl;

    // Initialize audio engine
    Amplitron::AudioEngine engine;
    if (!engine.initialize()) {
        std::cerr << "Failed to initialize audio engine!" << std::endl;
        return 1;
    }

    // Set up default signal chain
    // Only EQ and light reverb are enabled by default — clean acoustic tone.
    // All other effects start bypassed; click the footswitch to enable them.

    auto noise_gate = std::make_shared<Amplitron::NoiseGate>();
    noise_gate->set_enabled(false);
    engine.add_effect(noise_gate);

    auto compressor = std::make_shared<Amplitron::Compressor>();
    compressor->set_enabled(false);
    engine.add_effect(compressor);

    auto overdrive = std::make_shared<Amplitron::Overdrive>();
    overdrive->set_enabled(false);
    engine.add_effect(overdrive);

    auto distortion = std::make_shared<Amplitron::Distortion>();
    distortion->set_enabled(false);
    engine.add_effect(distortion);

    auto eq = std::make_shared<Amplitron::Equalizer>();
    eq->set_enabled(true);  // ON: gentle EQ for tone shaping
    engine.add_effect(eq);

    auto chorus = std::make_shared<Amplitron::Chorus>();
    chorus->set_enabled(false);
    engine.add_effect(chorus);

    auto delay = std::make_shared<Amplitron::Delay>();
    delay->set_enabled(false);
    engine.add_effect(delay);

    auto reverb = std::make_shared<Amplitron::Reverb>();
    reverb->set_enabled(true);  // ON: light room reverb
    reverb->params()[0].value = 0.3f;  // Decay: 30% (short, natural room)
    reverb->params()[1].value = 0.4f;  // Damping: mellow
    reverb->set_mix(0.25f);  // 25% wet — subtle, doesn't wash out the signal
    engine.add_effect(reverb);

    auto cabinet = std::make_shared<Amplitron::CabinetSim>();
    cabinet->set_enabled(false);  // OFF: not needed for acoustic guitar
    engine.add_effect(cabinet);

    // Lower input gain for piezo/USB guitar cable (they tend to run hot)
    engine.set_input_gain(0.7f);

    std::cout << "Default signal chain loaded (clean acoustic preset)." << std::endl;
    std::cout << "  EQ and Reverb are ON. All other effects are bypassed." << std::endl;
    std::cout << "  Click pedal footswitches in the GUI to enable more effects." << std::endl;

    // Initialize preset manager with bundled presets if available
    if (std::filesystem::exists("presets")) {
        Amplitron::PresetManager::set_presets_dir("presets");
    }

     // Initialize GUI first — audio stream only starts if GUI succeeds
    Amplitron::GuiManager gui(engine);
    if (!gui.initialize(1280, 720)) {
        std::cerr << "Failed to initialize GUI!" << std::endl;
        engine.shutdown();
        return 1;
    }
    
    if (!engine.start()) {
        std::cerr << "Warning: Could not start audio stream." << std::endl;
    }

    if (!engine.start()) {
        std::cerr << "Warning: Could not start audio stream." << std::endl;
    }

    std::cout << "Amplitron is ready. Let's play!" << std::endl;

    // Main loop
#ifdef __EMSCRIPTEN__
    g_gui = &gui;
    // 0 = use requestAnimationFrame, 1 = simulate infinite loop
    emscripten_set_main_loop(em_main_loop, 0, 1);
#else
    while (g_running && gui.run_frame()) {
        // GUI drives the frame rate via vsync
    }
#endif

    // Cleanup
    std::cout << "Shutting down..." << std::endl;
    gui.shutdown();
    engine.shutdown();

    std::cout << "Goodbye!" << std::endl;
    return 0;
}
