#include "gui/commands/command_history.h"

namespace Amplitron {

/** @brief Execute cmd, attempt coalescing, push to undo stack, clear redo. */
void CommandHistory::execute(std::unique_ptr<Command> cmd) {
    if (!cmd->execute()) return; // no-op — skip history recording

    // Try coalescing with the top of the undo stack
    if (!undo_stack_.empty() && undo_stack_.back()->merge_with(*cmd)) {
        // Merged into existing top — no new entry needed
    } else {
        undo_stack_.push_back(std::move(cmd));
        trim();
    }

    // New action invalidates the redo branch
    redo_stack_.clear();
}

/** @brief Push an already-applied command; attempt coalescing, clear redo. */
void CommandHistory::push_executed(std::unique_ptr<Command> cmd) {
    if (!undo_stack_.empty() && undo_stack_.back()->merge_with(*cmd)) {
        // Merged into existing top — no new entry needed
    } else {
        undo_stack_.push_back(std::move(cmd));
        trim();
    }

    // New action invalidates the redo branch
    redo_stack_.clear();
}

/** @brief Pop the top undo command, call its undo(), and move it to redo. */
bool CommandHistory::undo() {
    if (undo_stack_.empty()) return false;

    auto cmd = std::move(undo_stack_.back());
    undo_stack_.pop_back();

    cmd->undo();
    redo_stack_.push_back(std::move(cmd));
    return true;
}

/** @brief Pop the top redo command, call its execute(), and move it to undo. */
bool CommandHistory::redo() {
    if (redo_stack_.empty()) return false;

    auto cmd = std::move(redo_stack_.back());
    redo_stack_.pop_back();

    cmd->execute();
    undo_stack_.push_back(std::move(cmd));
    return true;
}

/** @brief Discard all undo and redo history. */
void CommandHistory::clear() {
    undo_stack_.clear();
    redo_stack_.clear();
}

/** @brief Return the description of the top undo command, or nullptr. */
const char* CommandHistory::undo_description() const {
    if (undo_stack_.empty()) return nullptr;
    return undo_stack_.back()->description();
}

/** @brief Return the description of the top redo command, or nullptr. */
const char* CommandHistory::redo_description() const {
    if (redo_stack_.empty()) return nullptr;
    return redo_stack_.back()->description();
}

/** @brief Remove oldest undo entries until stack size <= max_depth_. */
void CommandHistory::trim() {
    while (static_cast<int>(undo_stack_.size()) > max_depth_) {
        undo_stack_.erase(undo_stack_.begin());
    }
}

} // namespace Amplitron
