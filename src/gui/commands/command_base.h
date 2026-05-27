#pragma once

#include <chrono>

namespace Amplitron {

/**
 * @brief Abstract base class for all undoable commands (Gang of Four Command Pattern).
 *
 * Each concrete command encapsulates a single reversible action on the audio
 * engine (e.g. adding an effect, changing a parameter). Commands are stored
 * in a CommandHistory and invoked via execute() / undo().
 */
class Command {
public:
    virtual ~Command() = default;

    /** @brief Apply this command's action. */
    virtual void execute() = 0;

    /** @brief Reverse this command's action. */
    virtual void undo() = 0;

    /** @brief Return a short human-readable label (shown in the Edit menu). */
    virtual const char* description() const = 0;

    /**
     * @brief Attempt to merge @p other into this command (coalescing).
     *
     * Two commands can merge if they affect the same target within a short
     * time window. Returns true if this command absorbed @p other.
     */
    virtual bool merge_with(const Command& /*other*/) { return false; }

    /** @brief Return the steady-clock time point when this command was created. */
    auto timestamp() const { return timestamp_; }

protected:
    std::chrono::steady_clock::time_point timestamp_ = std::chrono::steady_clock::now();
};

} // namespace Amplitron
