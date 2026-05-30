#pragma once

#include <imgui.h>
#include <memory>
#include <functional>
#include <string>

namespace Amplitron {

class Effect;
class AudioEngine;
class GuiMidi;

enum class ScreenType {
    Tuner,
    Cabinet,
    Looper,
    MultiBandCompressor
};

struct ScreenProps {
    ScreenType type;
    std::shared_ptr<Effect> effect;
    int index = 0;
    AudioEngine* engine = nullptr;
    GuiMidi* gui_midi = nullptr;

    // Callback events for undo-able parameter changes
    std::function<void(int, float, float)> on_commit_param_change; // param_index, old_val, new_val
};

class ScreenComponent {
public:
    /**
     * @brief Render a reusable screen display component for complex pedals (Tuner, Cabinet, Looper, MultiBandCompressor).
     * @param dl          ImDrawList pointer to draw custom shapes.
     * @param p0          Top-left corner position of the screen or display region.
     * @param pedal_width Width of the pedal containing this screen.
     * @param zoom        DPI zoom multiplier.
     * @param props       Configuration, references, and event callbacks.
     */
    static void render(ImDrawList* dl, ImVec2 p0, float pedal_width, float zoom, const ScreenProps& props);

private:
    static void render_tuner_display(ImDrawList* dl, ImVec2 p0, float pedal_width, float zoom, const ScreenProps& props);
    static void render_ir_cabinet_display(ImDrawList* dl, ImVec2 p0, float pedal_width, float zoom, const ScreenProps& props);
    static void render_looper_display(ImDrawList* dl, ImVec2 p0, float pedal_width, float zoom, const ScreenProps& props);
    static void render_multiband_compressor_display(ImDrawList* dl, ImVec2 p0, float pedal_width, float zoom, const ScreenProps& props);
};

} // namespace Amplitron
