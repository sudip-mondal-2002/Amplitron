#pragma once

#include "common.h"
#include "gui/ui_component.h"
#include "gui/state/snapshot_manager.h"
#include <functional>
#include <array>
#include <string>

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

    char  status_msg_[64] = {};
    float status_timer_   = 0.0f;
};

} // namespace Amplitron
