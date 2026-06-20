#pragma once

#include <imgui.h>

#include <array>
#include <functional>
#include <string>

#include "common.h"
#include "gui/state/snapshot_manager.h"
#include "gui/ui_component.h"

namespace Amplitron {

struct SnapshotSlotInfo {
    bool is_filled = false;
    bool is_active = false;
    const char* label = "";
};

struct SnapshotsProps {
    std::array<SnapshotSlotInfo, SnapshotManager::NUM_SLOTS> slots{};

    std::function<void(int)> on_recall_slot;
    std::function<void(int)> on_save_slot;
    std::function<void(int)> on_clear_slot;
};

/**
 * @brief Reactive A/B/C/D snapshot toolbar component.
 *
 * Receives slot state through SnapshotsProps. All mutations go through
 * callbacks which are executed in GuiManager (where command history lives).
 */
class GuiSnapshots : public UIComponent<SnapshotsProps> {
   public:
    GuiSnapshots() = default;

    /** @brief Render the snapshot toolbar row. */
    void render() override;

   private:
    static constexpr float STATUS_DISPLAY_SECONDS = 2.0f;

    char status_msg_[64] = {};
    float status_timer_ = 0.0f;

    ImVec2 button_mins_[SnapshotManager::NUM_SLOTS] = {};
    ImVec2 button_maxs_[SnapshotManager::NUM_SLOTS] = {};
    ImVec2 menu_item_mins_[2] = {};
    ImVec2 menu_item_maxs_[2] = {};
};

}  // namespace Amplitron
