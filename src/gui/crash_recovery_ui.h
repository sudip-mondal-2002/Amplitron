#pragma once
#include <SDL.h>

/**
 * @brief Displays a native modal popup to ask the user if they wish to restore 
 * the previous session after a crash.
 * * @return true if the user clicks "Restore", false if "Discard".
 */

inline bool promptRestoreSession() {
#if defined(__EMSCRIPTEN__) || defined(AMPLITRON_HEADLESS)
    // Blocking message boxes not supported in WebAssembly or headless test builds.
    return false;
#else
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) return false;
    }

    const SDL_MessageBoxButtonData buttons[] = {
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Restore" },
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Discard" },
    };
    
    const SDL_MessageBoxData messageboxdata = {
        SDL_MESSAGEBOX_WARNING,
        nullptr,
        "Amplitron Recovery",
        "Amplitron closed unexpectedly during your last session.\n\n"
        "Would you like to restore your previous pedal chain configuration?",
        SDL_arraysize(buttons),
        buttons,
        nullptr
    };
    
    int buttonid;
    if (SDL_ShowMessageBox(&messageboxdata, &buttonid) < 0) return false;
    return buttonid == 1;
#endif
}
