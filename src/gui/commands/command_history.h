#pragma once

#include "gui/commands/command.h"
#include <vector>
#include <memory>

namespace Amplitron {

/**
 * @brief Manages undo and redo stacks for Command objects.
 *
 * Provides execute(), undo(), redo(), and push_executed() operations.
 * Supports coalescing of rapid parameter changes and enforces a
 * configurable maximum history depth (default DEFAULT_MAX_DEPTH).
 */
class CommandHistory {
public:
    /** @brief Default maximum number of undo entries. */
    static constexpr int DEFAULT_MAX_DEPTH = 100;

    /**
     * @brief Construct a CommandHistory.
     * @param max_depth Maximum undo stack size. Negative values are clamped to 0.
     */
    explicit CommandHistory(int max_depth = DEFAULT_MAX_DEPTH)
        : max_depth_(max_depth < 0 ? 0 : max_depth) {}

    /**
     * @brief Execute a command and push it onto the undo stack.
     *
     * Calls cmd->execute(), attempts coalescing with the stack top, and
     * clears the redo stack (new action invalidates the redo branch).
     *
     * @param cmd Owning pointer to the command to execute.
     */
    void execute(std::unique_ptr<Command> cmd);

    /**
     * @brief Record a command that was already applied by the caller.
     *
     * Useful for knob changes that are applied directly by the widget;
     * the command is pushed for undo without calling execute().
     *
     * @param cmd Owning pointer to the already-executed command.
     */
    void push_executed(std::unique_ptr<Command> cmd);

    /**
     * @brief Undo the most recent command.
     * @return true if an undo was performed, false if the stack was empty.
     */
    bool undo();

    /**
     * @brief Redo the most recently undone command.
     * @return true if a redo was performed, false if the stack was empty.
     */
    bool redo();

    /** @brief Clear both undo and redo stacks (e.g. when loading a preset). */
    void clear();

    /** @brief Return true if the undo stack is non-empty. */
    bool can_undo() const { return !undo_stack_.empty(); }

    /** @brief Return true if the redo stack is non-empty. */
    bool can_redo() const { return !redo_stack_.empty(); }

    /** @brief Number of commands on the undo stack. */
    int undo_size() const { return static_cast<int>(undo_stack_.size()); }

    /** @brief Number of commands on the redo stack. */
    int redo_size() const { return static_cast<int>(redo_stack_.size()); }

    /** @brief Current maximum undo depth. */
    int max_depth() const { return max_depth_; }

    /**
     * @brief Change the maximum undo depth and trim excess entries.
     * @param depth New maximum depth. Negative values are clamped to 0.
     */
    void set_max_depth(int depth) { max_depth_ = (depth < 0) ? 0 : depth; trim(); }

    /** @brief Description string of the top undo command, or nullptr if empty. */
    const char* undo_description() const;

    /** @brief Description string of the top redo command, or nullptr if empty. */
    const char* redo_description() const;

private:
    /** @brief Evict the oldest undo entries until the stack fits max_depth_. */
    void trim();

    std::vector<std::unique_ptr<Command>> undo_stack_;
    std::vector<std::unique_ptr<Command>> redo_stack_;
    int max_depth_;
};

} // namespace Amplitron
