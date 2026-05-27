cmake_minimum_required(VERSION 3.16)
project(Amplitron VERSION 1.0.0 LANGUAGES CXX C)
include(GNUInstallDirs)

set(AMPLITRON_VERSION "0.0.0" CACHE STRING "Application version (set by CI)")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Option to enable JACK backend on Linux
if(UNIX AND NOT APPLE)
    set(DEFAULT_WITH_JACK ON)
else()
    set(DEFAULT_WITH_JACK OFF)
endif()
option(WITH_JACK "Enable JACK audio backend (Linux only)" ${DEFAULT_WITH_JACK})

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
if(EMSCRIPTEN)
    add_compile_options(-O3 -ffast-math -pthread "-sUSE_SDL=2" -Wno-nontrivial-memaccess)
elseif(MSVC)
    add_compile_options(/W4 /fp:fast)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(/O2 /arch:AVX)
    endif()
else()
    add_compile_options(-Wall -Wextra -ffast-math)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # nlohmann/json intentionally uses infinity for some binary parsing paths;
        # with -ffast-math Clang warns in external headers.
        add_compile_options(-Wno-nan-infinity-disabled)
    endif()
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -ftree-vectorize)
        # -march=native is only valid for native (non-cross) builds
        if(NOT ANDROID AND NOT IOS AND NOT EMSCRIPTEN)
            add_compile_options(-march=native)
        endif()
    endif()
endif()

# --- nlohmann/json (header-only, fetched at configure time) ----------------
# Single-header C++17 JSON library used for preset serialization /
# deserialization.  We disable nlohmann's own tests and install rules so
# they don't pollute the Amplitron build.
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
        GIT_SHALLOW    TRUE
    )
    set(JSON_BuildTests OFF CACHE INTERNAL "Disable nlohmann/json tests")
    set(JSON_Install    OFF CACHE INTERNAL "Disable nlohmann/json install")
    FetchContent_MakeAvailable(nlohmann_json)
endif()
# ---------------------------------------------------------------------------

file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/external")
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/external/nanosvg.h" OR
   NOT EXISTS "${CMAKE_SOURCE_DIR}/external/nanosvgrast.h" OR
   NOT EXISTS "${CMAKE_SOURCE_DIR}/external/dr_wav.h")
    message(FATAL_ERROR
        "Missing external header dependencies in external/. "
        "Run scripts/setup_dependencies.sh (Linux/macOS) or "
        "scripts/setup_dependencies.ps1 (Windows) to fetch nanosvg and dr_wav.")
endif()


# --- Dependencies ---
# Emscripten provides SDL2, OpenGL ES, and audio via built-in ports.
# Native builds find PortAudio, SDL2, and OpenGL from the system.
if(NOT EMSCRIPTEN AND NOT ANDROID AND NOT IOS)

# PortAudio
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PORTAUDIO portaudio-2.0)
endif()
if(NOT PORTAUDIO_FOUND)
    find_path(PORTAUDIO_INCLUDE_DIRS portaudio.h
        PATHS /usr/include /usr/local/include /opt/homebrew/include
              "C:/Program Files/portaudio/include"
              "${CMAKE_SOURCE_DIR}/external/portaudio/include")
    find_library(PORTAUDIO_LIBRARIES NAMES portaudio portaudio_x64
        PATHS /usr/lib /usr/local/lib /opt/homebrew/lib
              "C:/Program Files/portaudio/lib"
              "${CMAKE_SOURCE_DIR}/external/portaudio/lib")
endif()
# pkg_check_modules can populate PORTAUDIO_LIBRARIES with just the library
# name and PORTAUDIO_LIBRARY_DIRS with the matching prefix path.  Add the
# directory explicitly so macOS / Homebrew links resolve correctly.
if(PORTAUDIO_LIBRARY_DIRS)
    link_directories(${PORTAUDIO_LIBRARY_DIRS})
endif()

    # JACK (optional, Linux only)
    if(NOT JACK_LIBRARIES)
        if(PkgConfig_FOUND)
            pkg_check_modules(JACK jack)
        endif()
        if(NOT JACK_FOUND)
            find_path(JACK_INCLUDE_DIRS jack/jack.h
                PATHS /usr/include /usr/local/include)
            find_library(JACK_LIBRARIES NAMES jack jackserver
                PATHS /usr/lib /usr/local/lib)
        endif()
    endif()
    if(JACK_LIBRARY_DIRS)
        link_directories(${JACK_LIBRARY_DIRS})
    endif()

# SDL2
if(PkgConfig_FOUND)
    pkg_check_modules(SDL2 sdl2)
endif()
if(NOT SDL2_FOUND)
    find_package(SDL2 QUIET)
    if(SDL2_FOUND)
        set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIRS})
        set(SDL2_LIBRARIES SDL2::SDL2 SDL2::SDL2main)
    else()
        find_path(SDL2_INCLUDE_DIRS SDL.h
            PATH_SUFFIXES SDL2
            PATHS /usr/include /usr/local/include /opt/homebrew/include
                  "C:/Program Files/SDL2/include"
                  "${CMAKE_SOURCE_DIR}/external/SDL2/include")
        find_library(SDL2_LIBRARIES NAMES SDL2
            PATHS /usr/lib /usr/local/lib /opt/homebrew/lib
                  "C:/Program Files/SDL2/lib"
                  "${CMAKE_SOURCE_DIR}/external/SDL2/lib")
    endif()
endif()
# pkg_check_modules may provide a bare library name and a matching library
# directory; add the path explicitly so macOS/Homebrew links resolve.
if(SDL2_LIBRARY_DIRS)
    link_directories(${SDL2_LIBRARY_DIRS})
endif()

# RtMidi (MIDI input)
# Skip auto-detection when paths are provided explicitly (e.g. CI -D flags).
if(NOT RTMIDI_LIBRARIES)
    if(PkgConfig_FOUND)
        pkg_check_modules(RTMIDI rtmidi)
    endif()
    if(NOT RTMIDI_FOUND)
        find_path(RTMIDI_INCLUDE_DIRS rtmidi/RtMidi.h
            PATHS /usr/include /usr/local/include /opt/homebrew/include
                  "C:/Program Files/rtmidi/include"
                  "${CMAKE_SOURCE_DIR}/external/rtmidi")
        find_library(RTMIDI_LIBRARIES NAMES rtmidi
            PATHS /usr/lib /usr/local/lib /opt/homebrew/lib
                  "C:/Program Files/rtmidi/lib"
                  "${CMAKE_SOURCE_DIR}/external/rtmidi")
    endif()
endif()
# pkg_check_modules sets RTMIDI_LIBRARIES to just the name ("rtmidi") and
# RTMIDI_LIBRARY_DIRS to the directory (/opt/homebrew/lib).  Without
# link_directories the linker never searches that path, so we add it
# explicitly.  This is a no-op when RTMIDI_LIBRARIES is a full path.
if(RTMIDI_LIBRARY_DIRS)
    link_directories(${RTMIDI_LIBRARY_DIRS})
endif()

# OpenGL
find_package(OpenGL REQUIRED)

endif() # NOT EMSCRIPTEN AND NOT ANDROID AND NOT IOS

# --- Dear ImGui (bundled) ---
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/external/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# Suppress warnings on vendored imgui sources
if(NOT MSVC)
    set_source_files_properties(${IMGUI_SOURCES} PROPERTIES COMPILE_FLAGS "-w")
endif()

# --- kiss_fft (bundled, BSD-3-Clause) ---
set(KISS_FFT_SOURCES ${CMAKE_SOURCE_DIR}/external/kiss_fft/kiss_fft.c)
if(NOT MSVC)
    set_source_files_properties(${KISS_FFT_SOURCES} PROPERTIES COMPILE_FLAGS "-w")
endif()

# --- Application Sources (platform-agnostic) ---
set(APP_SOURCES
    src/main.cpp
    src/session_manager.h
    src/presets/preset_manager.cpp
    src/presets/preset_json.cpp
    src/audio/engine/audio_engine.cpp
    src/audio/engine/audio_engine_chain.cpp
    src/audio/engine/audio_engine_process.cpp
    src/audio/engine/audio_engine_api.cpp
    src/audio/recorder/recorder.cpp
    src/audio/recorder/recorder_session.cpp
    src/audio/recorder/recorder_io.cpp
    src/audio/recorder/recorder_wav.cpp
    src/audio/effects/distortion.cpp
    src/audio/effects/overdrive.cpp
    src/audio/effects/delay.cpp
    src/audio/effects/reverb.cpp
    src/audio/effects/chorus.cpp
    src/audio/effects/phaser.cpp
    src/audio/effects/flanger.cpp
    src/audio/effects/equalizer.cpp
    src/audio/effects/noise_gate.cpp
    src/audio/effects/compressor.cpp
    src/audio/effects/multiband_compressor.cpp
    src/audio/effects/looper.cpp
    src/audio/effects/cabinet_sim.cpp
    src/audio/effects/amp_simulator.cpp
    src/audio/dsp/wav_loader.cpp
    src/audio/dsp/convolution_engine.cpp
    src/audio/dsp/spectrum_analyzer.cpp
    src/audio/dsp/level_analyzer.cpp
    src/audio/effects/tuner.cpp
    src/audio/effects/wah.cpp
    src/audio/effects/octaver.cpp
    src/audio/effects/pitch_shifter.cpp
    src/gui/crash_recovery_ui.h
    src/gui/gui_manager.cpp
    src/gui/gui_manager_frame.cpp
    src/gui/gui_manager_menu.cpp
    src/gui/gui_manager_update.cpp
    src/gui/views/gui_settings.cpp
    src/gui/views/gui_presets.cpp
    src/gui/views/gui_recording.cpp
    src/gui/views/gui_tuner.cpp
    src/gui/views/gui_analyzer.cpp
    src/gui/pedalboard/pedal_board.cpp
    src/gui/pedalboard/pedal_board_menu.cpp
    src/gui/pedalboard/pedal_board_chain.cpp
    src/gui/pedalboard/pedal_widget.cpp
    src/gui/pedalboard/pedal_widget_body.cpp
    src/gui/pedalboard/pedal_widget_knobs.cpp
    src/gui/commands/command_history.cpp
    src/gui/views/gui_snapshots.cpp
    src/gui/components/knob.cpp
    src/gui/components/footswitch.cpp
    src/gui/components/led.cpp
    src/gui/components/screen.cpp
    src/presets/preset_manager_dirs.cpp
    src/presets/preset_manager_io.cpp
    src/audio/engine/audio_graph.cpp
    src/audio/engine/audio_graph_executor.cpp
)

# --- MIDI Support (desktop only) ---
set(MIDI_SOURCES)
if(NOT ANDROID AND NOT IOS)
    list(APPEND MIDI_SOURCES
        src/midi/midi_manager.cpp
        src/midi/midi_manager_mapping.cpp
        src/midi/midi_manager_persist.cpp
        src/gui/views/gui_midi.cpp
    )
endif()

# --- Platform-specific sources (selected at configure time) ---
# Mobile (Android/iOS) and Web use SDL audio backend + stub file dialog
# Android 8+ uses Oboe (AAudio exclusive mode) for low-latency audio.
if(EMSCRIPTEN OR IOS)
    set(BACKEND_SOURCES src/audio/backend/audio_backend_sdl.cpp)
    set(DIALOG_SOURCES  src/gui/dialogs/file_dialog_web.cpp)
elseif(ANDROID)
    set(BACKEND_SOURCES src/audio/backend/audio_backend_oboe.cpp)
    set(DIALOG_SOURCES  src/gui/dialogs/file_dialog_web.cpp)
else()
    if(WITH_JACK AND UNIX AND NOT APPLE)
        set(BACKEND_SOURCES
            src/audio/backend/audio_backend_jack.cpp
        )
    else()
        set(BACKEND_SOURCES
            src/audio/backend/audio_backend_portaudio.cpp
            src/audio/backend/audio_backend_portaudio_lifecycle.cpp
            src/audio/backend/audio_backend_portaudio_devices.cpp
        )
    endif()
    set(DIALOG_SOURCES
        src/gui/dialogs/file_dialog_native.cpp
        src/gui/dialogs/file_dialog_native_open.cpp
        src/gui/dialogs/file_dialog_native_folder.cpp
    )
endif()

# =============================================================================
# Target configuration: Emscripten (web) vs Native (desktop)
# =============================================================================
# =============================================================================
# Shared helper: SDL2 for mobile builds (Android / iOS)
# Prefer a pre-installed SDL2 (e.g. from CI cache via -DSDL2_DIR=...).
# Fall back to building from source via FetchContent for local/first-time builds.
# =============================================================================
if(ANDROID)
    # Oboe: low-latency audio for Android 8+ (AAudio exclusive mode)
    # Follow the repo's external-library pattern: allow CI to inject pre-built
    # paths via -DOBOE_INCLUDE_DIRS=... / -DOBOE_LIBRARIES=... flags.
    # Fall back to FetchContent when those overrides are absent.
    if(NOT OBOE_LIBRARIES)
        include(FetchContent)
        FetchContent_Declare(oboe
            GIT_REPOSITORY https://github.com/google/oboe.git
            GIT_TAG        1.8.0
        )
        FetchContent_MakeAvailable(oboe)
        set(OBOE_INCLUDE_DIRS ${oboe_SOURCE_DIR}/include)
        set(OBOE_LIBRARIES    oboe)
    endif()
endif()

if(ANDROID OR IOS)
    find_package(SDL2 QUIET CONFIG)
    if(NOT TARGET SDL2::SDL2-static)
        include(FetchContent)
        FetchContent_Declare(SDL2
            URL https://github.com/libsdl-org/SDL/archive/refs/tags/release-2.30.12.tar.gz
        )
        set(SDL_STATIC  ON  CACHE BOOL "" FORCE)
        set(SDL_SHARED  OFF CACHE BOOL "" FORCE)
        set(SDL_TEST    OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(SDL2)
        # Suppress warnings from SDL2's own source (unused-parameter, etc.)
        if(TARGET SDL2-static)
            target_compile_options(SDL2-static PRIVATE -w)
        endif()
    endif()
endif()

if(EMSCRIPTEN)

    # --- Emscripten / WebAssembly build ---
    add_executable(Amplitron ${APP_SOURCES} ${MIDI_SOURCES} ${BACKEND_SOURCES} ${DIALOG_SOURCES} ${IMGUI_SOURCES} ${KISS_FFT_SOURCES})

    target_include_directories(Amplitron PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/presets
        ${CMAKE_SOURCE_DIR}/external
        ${CMAKE_SOURCE_DIR}/external/kiss_fft
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
    )

    # OpenGL ES 3 for ImGui's OpenGL3 backend on WebGL 2
    target_compile_definitions(Amplitron PRIVATE IMGUI_IMPL_OPENGL_ES3 AMPLITRON_VERSION="${AMPLITRON_VERSION}")

    target_link_libraries(Amplitron PRIVATE nlohmann_json::nlohmann_json)

    # Emscripten linker flags for Web Audio + AudioWorklet + SharedArrayBuffer
    target_link_options(Amplitron PRIVATE
        "-sWASM=1"
        "-sUSE_SDL=2"
        "-sMIN_WEBGL_VERSION=2"
        "-sMAX_WEBGL_VERSION=2"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sAUDIO_WORKLET=1"
        "-sWASM_WORKERS=1"
        "-pthread"
        "-sSHARED_MEMORY=1"
        "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
        "-sSTACK_SIZE=1048576"
        "-sPTHREAD_POOL_SIZE=4"
        "-Wno-pthreads-mem-growth"
        "--shell-file" "${CMAKE_SOURCE_DIR}/web/shell.html"
        "--preload-file" "${CMAKE_SOURCE_DIR}/presets@/presets"
    )

    # Output name: index.html (+ index.js, index.wasm generated by emcc)
    set_target_properties(Amplitron PROPERTIES
        OUTPUT_NAME "index"
        SUFFIX ".html"
    )

    # Copy coi-serviceworker.js next to the build output for SharedArrayBuffer support
    add_custom_command(TARGET Amplitron POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/web/coi-serviceworker.js"
            "$<TARGET_FILE_DIR:Amplitron>/coi-serviceworker.js"
    )

elseif(ANDROID)

    # --- Android build: shared library loaded by SDLActivity via JNI ---
    # SDL2 is linked statically so only libmain.so needs to be packaged.
    # AmplitronActivity.java overrides getLibraries() to load only "main".
    add_library(main SHARED
        ${APP_SOURCES}
        ${MIDI_SOURCES}
        ${BACKEND_SOURCES}
        ${DIALOG_SOURCES}
        ${IMGUI_SOURCES}
        ${KISS_FFT_SOURCES}
    )

    target_include_directories(main PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/presets
        ${CMAKE_SOURCE_DIR}/external
        ${CMAKE_SOURCE_DIR}/external/kiss_fft
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
    )
    if(SDL2_SOURCE_DIR)
        target_include_directories(main PRIVATE ${SDL2_SOURCE_DIR}/include)
    endif()

    target_compile_definitions(main PRIVATE
        IMGUI_IMPL_OPENGL_ES3
        AMPLITRON_VERSION="${AMPLITRON_VERSION}"
        AMPLITRON_NO_MIDI
        AMPLITRON_ANDROID_OBOE   # signals that Oboe backend is in use
    )

    target_include_directories(main PRIVATE ${OBOE_INCLUDE_DIRS})

    target_link_libraries(main PRIVATE
        nlohmann_json::nlohmann_json
        ${OBOE_LIBRARIES}
        SDL2::SDL2-static
        android
        log
        EGL
        GLESv3
    )

elseif(IOS)

    # --- iOS build: application bundle, SDL2 statically linked ---
    add_executable(Amplitron MACOSX_BUNDLE
        ${APP_SOURCES}
        ${MIDI_SOURCES}
        ${BACKEND_SOURCES}
        ${DIALOG_SOURCES}
        ${IMGUI_SOURCES}
        ${KISS_FFT_SOURCES}
    )

    target_include_directories(Amplitron PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/presets
        ${CMAKE_SOURCE_DIR}/external
        ${CMAKE_SOURCE_DIR}/external/kiss_fft
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
    )
    if(SDL2_SOURCE_DIR)
        target_include_directories(Amplitron PRIVATE ${SDL2_SOURCE_DIR}/include)
    endif()

    target_compile_definitions(Amplitron PRIVATE
        IMGUI_IMPL_OPENGL_ES3
        AMPLITRON_VERSION="${AMPLITRON_VERSION}"
        AMPLITRON_NO_DESKTOP_SHELL
        AMPLITRON_NO_MIDI
    )

    target_link_libraries(Amplitron PRIVATE
        nlohmann_json::nlohmann_json
        SDL2::SDL2main
        SDL2::SDL2-static
        "-framework UIKit"
        "-framework OpenGLES"
        "-framework AudioToolbox"
        "-framework CoreAudio"
        "-framework CoreMotion"
        "-framework CoreHaptics"
        "-framework GameController"
        "-framework Foundation"
        "-framework AVFoundation"
        iconv
    )

    set_target_properties(Amplitron PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST              "${CMAKE_SOURCE_DIR}/ios/Info.plist"
        MACOSX_BUNDLE_BUNDLE_NAME             "Amplitron"
        MACOSX_BUNDLE_BUNDLE_VERSION          "${AMPLITRON_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING    "${AMPLITRON_VERSION}"
        XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "15.0"
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY    ""
        XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
        XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED  "NO"
    )

else() # NOT EMSCRIPTEN, ANDROID, or IOS

    # --- Native desktop build (unchanged) ---

    # Windows resource file for app icon
    if(WIN32 AND NOT MSVC)
        set(WIN_RC ${CMAKE_SOURCE_DIR}/assets/app.rc)
        set(CMAKE_RC_COMPILER_INIT windres)
        enable_language(RC)
    endif()

    add_executable(Amplitron ${APP_SOURCES} ${MIDI_SOURCES} ${BACKEND_SOURCES} ${DIALOG_SOURCES} ${IMGUI_SOURCES} ${KISS_FFT_SOURCES} $<$<PLATFORM_ID:Windows>:${WIN_RC}>)

    target_include_directories(Amplitron PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/presets
        ${CMAKE_SOURCE_DIR}/external
        ${CMAKE_SOURCE_DIR}/external/kiss_fft
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
        ${PORTAUDIO_INCLUDE_DIRS}
        ${SDL2_INCLUDE_DIRS}
        ${OPENGL_INCLUDE_DIR}
        ${RTMIDI_INCLUDE_DIRS}
    )

    if(WITH_JACK AND UNIX AND NOT APPLE AND JACK_INCLUDE_DIRS)
        target_include_directories(Amplitron PRIVATE ${JACK_INCLUDE_DIRS})
    endif()

    # ============================================================
    # DEFINE AMPLITRON_HAS_MIDI FOR DESKTOP BUILDS
    # ============================================================
    target_compile_definitions(Amplitron PRIVATE 
        AMPLITRON_VERSION="${AMPLITRON_VERSION}"
        AMPLITRON_HAS_MIDI
    )

    if(WITH_JACK AND UNIX AND NOT APPLE)
        target_compile_definitions(Amplitron PRIVATE WITH_JACK)
    endif()

    target_link_libraries(Amplitron PRIVATE
        nlohmann_json::nlohmann_json
        ${PORTAUDIO_LIBRARIES}
        ${SDL2_LIBRARIES}
        ${OPENGL_LIBRARIES}
        ${RTMIDI_LIBRARIES}
    )

    if(WITH_JACK AND UNIX AND NOT APPLE)
        target_link_libraries(Amplitron PRIVATE ${JACK_LIBRARIES})
    endif()

    if(WIN32)
        target_link_libraries(Amplitron PRIVATE winmm ole32 setupapi comdlg32 shell32)
        if(NOT MSVC)
            target_link_options(Amplitron PRIVATE -mwindows)
        else()
            set_target_properties(Amplitron PROPERTIES WIN32_EXECUTABLE TRUE)
        endif()
    endif()

    if(UNIX AND NOT APPLE)
        target_link_libraries(Amplitron PRIVATE dl pthread)
    endif()

    # --- Test Target ---
    set(TEST_SOURCES
        tests/test_main.cpp
        tests/unit/test_common.cpp
        tests/unit/test_oboe_ring_buffer.cpp
        tests/unit/test_effects_dynamics.cpp
        tests/unit/test_effects_saturation.cpp
        tests/unit/test_effects_filter.cpp
        tests/unit/test_effects_modulation.cpp
        tests/unit/test_effects_spatial.cpp
        tests/unit/test_effects_pitch.cpp
        tests/unit/test_effects_utility.cpp
        tests/unit/test_looper.cpp
        tests/unit/test_theme.cpp
        tests/unit/test_command_history.cpp
        tests/unit/test_cabinet_sim_ir.cpp
        tests/unit/test_wav_loader.cpp
        tests/unit/test_convolution_engine.cpp
        tests/unit/test_level_analyzer.cpp
        tests/integration/test_preset_manager.cpp
        tests/integration/test_json_serialization.cpp
        tests/integration/test_recorder.cpp
        tests/integration/test_snapshot_manager.cpp
        tests/integration/test_midi_manager.cpp
        tests/integration/test_unsaved_preset.cpp
        tests/integration/test_audio_graph.cpp
        tests/integration/test_metronome.cpp
        tests/integration/test_clipboard_preset.cpp
        tests/integration/test_audio_engine.cpp
        tests/integration/test_audio_backend_portaudio.cpp
        tests/e2e/test_e2e_preset_workflow.cpp
        tests/ui/test_gui_manager.cpp
        tests/ui/test_pedal_board.cpp
        tests/ui/test_crash_recovery.cpp
        tests/ui/test_gui_commands.cpp
        tests/ui/test_spectrum_analyzer.cpp
        tests/ui/test_gui_presets.cpp
        tests/ui/test_gui_snapshots.cpp
        tests/ui/test_gui_midi.cpp
        tests/ui/test_gui_recording.cpp
        tests/ui/test_gui_tuner.cpp
        tests/ui/test_gui_components.cpp
        tests/ui/test_footswitch.cpp
        tests/ui/test_led.cpp
        tests/ui/test_knob.cpp
        tests/ui/test_screen.cpp
        tests/ui/test_pedal_board_chain.cpp
        tests/ui/test_pedal_board_menu.cpp
        tests/ui/test_pedal_widget_knobs.cpp
        tests/unit/test_session_manager.cpp
        tests/unit/test_cli.cpp
        tests/integration/test_main.cpp
        tests/integration/test_audio_backend_jack.cpp

    )

    # Effect + core sources needed by tests (no GUI, no main)
    set(CORE_SOURCES
        src/presets/preset_manager.cpp
        src/presets/preset_json.cpp
        src/presets/preset_manager_dirs.cpp
        src/presets/preset_manager_io.cpp
        src/audio/engine/audio_engine.cpp
        src/audio/engine/audio_engine_chain.cpp
        src/audio/engine/audio_engine_process.cpp
        src/audio/engine/audio_engine_api.cpp


        src/audio/recorder/recorder.cpp
        src/audio/recorder/recorder_session.cpp
        src/audio/recorder/recorder_io.cpp
        src/audio/recorder/recorder_wav.cpp
        src/gui/commands/command_history.cpp

        src/audio/effects/distortion.cpp
        src/audio/effects/overdrive.cpp
        src/audio/effects/delay.cpp
        src/audio/effects/reverb.cpp
        src/audio/effects/chorus.cpp
        src/audio/effects/phaser.cpp
        src/audio/effects/flanger.cpp
        src/audio/effects/equalizer.cpp
        src/audio/effects/noise_gate.cpp
        src/audio/effects/compressor.cpp
        src/audio/effects/multiband_compressor.cpp
        src/audio/effects/looper.cpp
        src/audio/effects/cabinet_sim.cpp
        src/audio/effects/amp_simulator.cpp
        src/audio/effects/tuner.cpp
        src/audio/effects/wah.cpp
        src/audio/effects/octaver.cpp
        src/audio/effects/pitch_shifter.cpp
        src/audio/dsp/wav_loader.cpp
        src/audio/dsp/convolution_engine.cpp
        src/audio/dsp/spectrum_analyzer.cpp
        src/audio/dsp/level_analyzer.cpp
        src/audio/engine/audio_graph.cpp
        src/audio/engine/audio_graph_executor.cpp
    )

    if(WITH_JACK AND UNIX AND NOT APPLE)
        list(APPEND CORE_SOURCES
            src/audio/backend/audio_backend_jack.cpp
        )
    else()
        list(APPEND CORE_SOURCES
            src/audio/backend/audio_backend_portaudio.cpp
            src/audio/backend/audio_backend_portaudio_lifecycle.cpp
            src/audio/backend/audio_backend_portaudio_devices.cpp
        )
    endif()

    # Desktop-only MIDI support for tests
    list(APPEND CORE_SOURCES
        src/midi/midi_manager.cpp
        src/midi/midi_manager_mapping.cpp
        src/midi/midi_manager_persist.cpp
    )

    # GUI + ImGui + platform-specific sources for testing
    set(TEST_GUI_SOURCES
        src/gui/gui_manager.cpp
        src/gui/gui_manager_frame.cpp
        src/gui/gui_manager_menu.cpp
        src/gui/gui_manager_update.cpp
        src/gui/views/gui_settings.cpp
        src/gui/views/gui_presets.cpp
        src/gui/views/gui_recording.cpp
        src/gui/views/gui_tuner.cpp
        src/gui/views/gui_analyzer.cpp
        src/gui/pedalboard/pedal_board.cpp
        src/gui/pedalboard/pedal_board_menu.cpp
        src/gui/pedalboard/pedal_board_chain.cpp
        src/gui/pedalboard/pedal_widget.cpp
        src/gui/pedalboard/pedal_widget_body.cpp
        src/gui/pedalboard/pedal_widget_knobs.cpp
        src/gui/views/gui_snapshots.cpp
        src/gui/components/knob.cpp
        src/gui/components/footswitch.cpp
        src/gui/components/led.cpp
        src/gui/components/screen.cpp
        ${IMGUI_SOURCES}
    )
    if(EMSCRIPTEN OR IOS OR ANDROID)
        list(APPEND TEST_GUI_SOURCES src/gui/dialogs/file_dialog_web.cpp)
    else()
        list(APPEND TEST_GUI_SOURCES
            src/gui/views/gui_midi.cpp
            src/gui/dialogs/file_dialog_native.cpp
            src/gui/dialogs/file_dialog_native_open.cpp
            src/gui/dialogs/file_dialog_native_folder.cpp
        )
    endif()

    add_executable(amplitron-tests ${TEST_SOURCES} ${CORE_SOURCES} ${TEST_GUI_SOURCES} ${KISS_FFT_SOURCES})

    target_include_directories(amplitron-tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/presets
        ${CMAKE_SOURCE_DIR}/external
        ${CMAKE_SOURCE_DIR}/external/kiss_fft
        ${CMAKE_SOURCE_DIR}/tests
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
        ${PORTAUDIO_INCLUDE_DIRS}
        ${SDL2_INCLUDE_DIRS}
        ${OPENGL_INCLUDE_DIR}
        ${RTMIDI_INCLUDE_DIRS}
    )

    if(WITH_JACK AND UNIX AND NOT APPLE AND JACK_INCLUDE_DIRS)
        target_include_directories(amplitron-tests PRIVATE ${JACK_INCLUDE_DIRS})
    endif()

    # ============================================================
    # DEFINE AMPLITRON_HAS_MIDI AND AMPLITRON_VERSION FOR TEST BUILDS
    # ============================================================
    target_compile_definitions(amplitron-tests PRIVATE
        AMPLITRON_HAS_MIDI
        AMPLITRON_VERSION="${AMPLITRON_VERSION}"
        AMPLITRON_HEADLESS
        AMPLITRON_TESTS
        SDL_MAIN_HANDLED
        "Pa_GetDeviceCount=MOCK_Pa_GetDeviceCount"
        "Pa_GetDeviceInfo=MOCK_Pa_GetDeviceInfo"
        "Pa_GetHostApiInfo=MOCK_Pa_GetHostApiInfo"
        "Pa_GetHostApiCount=MOCK_Pa_GetHostApiCount"
        "Pa_HostApiDeviceIndexToDeviceIndex=MOCK_Pa_HostApiDeviceIndexToDeviceIndex"
        "Pa_GetDefaultInputDevice=MOCK_Pa_GetDefaultInputDevice"
        "Pa_GetDefaultOutputDevice=MOCK_Pa_GetDefaultOutputDevice"
        "Pa_OpenStream=MOCK_Pa_OpenStream"
        "Pa_StartStream=MOCK_Pa_StartStream"
        "Pa_StopStream=MOCK_Pa_StopStream"
        "Pa_CloseStream=MOCK_Pa_CloseStream"
        "Pa_GetStreamInfo=MOCK_Pa_GetStreamInfo"
        "Pa_Initialize=MOCK_Pa_Initialize"
    )

    if(WITH_JACK AND UNIX AND NOT APPLE)
        target_compile_definitions(amplitron-tests PRIVATE WITH_JACK)
    endif()


    target_link_libraries(amplitron-tests PRIVATE
        nlohmann_json::nlohmann_json
        ${SDL2_LIBRARIES}
        ${OPENGL_LIBRARIES}
        ${RTMIDI_LIBRARIES}
    )

    if(WITH_JACK AND UNIX AND NOT APPLE)
        target_link_libraries(amplitron-tests PRIVATE ${JACK_LIBRARIES})
    else()
        target_link_libraries(amplitron-tests PRIVATE ${PORTAUDIO_LIBRARIES})
    endif()

    if(WIN32)
        target_link_libraries(amplitron-tests PRIVATE winmm ole32 setupapi)
    endif()

    if(UNIX AND NOT APPLE)
        target_link_libraries(amplitron-tests PRIVATE dl pthread)
    endif()

    # Copy presets into build directory alongside the executable
    add_custom_target(copy_presets ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/presets"
            "$<TARGET_FILE_DIR:Amplitron>/presets"
        COMMENT "Copying presets to build directory"
    )
    add_dependencies(copy_presets Amplitron)

    # Copy fonts into build directory alongside the executable
    add_custom_target(copy_fonts ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets/fonts"
            "$<TARGET_FILE_DIR:Amplitron>/assets/fonts"
        COMMENT "Copying fonts to build directory"
    )
    add_dependencies(copy_fonts Amplitron)

    # Install
    install(TARGETS Amplitron RUNTIME DESTINATION bin)
    install(DIRECTORY presets DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/amplitron)
    install(DIRECTORY assets/fonts DESTINATION bin/assets)

endif() # EMSCRIPTEN / ANDROID / IOS / desktop
