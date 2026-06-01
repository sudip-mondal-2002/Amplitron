#pragma once

#include <functional>

namespace Amplitron {

/**
 * @brief Sentinel type for components that have no local render state.
 *
 * Used as the default State parameter in UIComponent so that components
 * without internal state don't carry a useless `int state_` member.
 */
struct EmptyState {};

/**
 * @brief Base template class for all reactive UI components.
 *
 * Enforces a React-like component structure:
 * - Props: Immutable configuration/data passed from the parent GuiManager
 *          every frame before render() is called.
 * - State: Local, mutable state owned by the component (e.g., animation
 *          timers, transient open/close flags). Defaults to EmptyState for
 *          components that need no persistent local state.
 *
 * Contract:
 *   1. Parent calls set_props(p) to push new data into the component.
 *   2. Parent calls render() — the component only reads props_ and state_.
 *   3. Mutations are communicated back through callbacks stored in Props,
 *      NOT by letting the component reach into the engine directly.
 */
template <typename Props, typename State = EmptyState>
class UIComponent {
public:
    virtual ~UIComponent() = default;

    /** @brief Push new props (inputs) into the component before render(). */
    virtual void set_props(const Props& new_props) {
        props_ = new_props;
    }

    /**
     * @brief Render the component using current props_ and state_.
     * Must be implemented by every concrete component.
     */
    virtual void render() = 0;

    /** @brief Read the current local state. */
    const State& get_state() const { return state_; }

    /**
     * @brief React-style setState: replace local state and trigger the
     * optional on_state_change() hook.
     */
    void set_state(const State& new_state) {
        state_ = new_state;
        on_state_change();
    }

    /**
     * @brief Functional setState: apply an updater lambda to a copy of the
     * current state, then commit and notify.
     */
    void set_state(std::function<void(State&)> updater) {
        State next = state_;
        updater(next);
        set_state(next);
    }

protected:
    Props props_;
    State state_;

    /** @brief Optional hook called after state has been updated. */
    virtual void on_state_change() {}
};

} // namespace Amplitron
