#pragma once

#include <atomic>
#include <cstddef>
#include <type_traits>
#include <vector>

namespace Amplitron {

// Lock-free Single-Producer Single-Consumer ring buffer.
// Producer: GUI thread pushes parameter changes.
// Consumer: Audio thread drains at the start of each callback.
template <typename T, size_t Capacity>
class SPSCQueue {
    static_assert(Capacity > 0 && (Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of two");
    static_assert(std::is_trivially_copyable<T>::value,
                  "T must be trivially copyable");

public:
    SPSCQueue() : head_(0), tail_(0) {}

    // Called from the producer (GUI) thread.
    // Returns false if the queue is full (message dropped by caller).
    bool try_push(const T& item) {
        const size_t h = head_.load(std::memory_order_relaxed);
        const size_t next = (h + 1) & kMask;
        if (next == tail_.load(std::memory_order_acquire)) {
            return false;  // full
        }
        buf_[h] = item;
        head_.store(next, std::memory_order_release);
        return true;
    }

    // Called from the consumer (audio) thread.
    // Returns false if the queue is empty.
    bool try_pop(T& item) {
        const size_t t = tail_.load(std::memory_order_relaxed);
        if (t == head_.load(std::memory_order_acquire)) {
            return false;  // empty
        }
        item = buf_[t];
        tail_.store((t + 1) & kMask, std::memory_order_release);
        return true;
    }

    // Inspect the front item without consuming it (consumer thread only).
    // Returns false if the queue is empty.
    bool try_peek(T& item) const {
        const size_t t = tail_.load(std::memory_order_relaxed);
        if (t == head_.load(std::memory_order_acquire)) {
            return false;  // empty
        }
        item = buf_[t];
        return true;
    }

    size_t try_pop_all(std::vector<T>& out_vec) {
        size_t count = 0;
        T item;
        while (try_pop(item)) {
            out_vec.push_back(item);
            ++count;
        }
        return count;
    }

    size_t size() const {
        return (head_.load(std::memory_order_acquire) - tail_.load(std::memory_order_acquire)) & kMask;
    }

    size_t capacity() const {
        return Capacity - 1;
    }

private:
    static constexpr size_t kMask = Capacity - 1;

    // Pad to avoid false sharing between producer and consumer cache lines
    alignas(64) std::atomic<size_t> head_;
    alignas(64) std::atomic<size_t> tail_;
    T buf_[Capacity];
};

// A command that the GUI thread sends to the audio thread.
struct AudioCommand {
    enum Type : uint8_t {
        SetEffectParam,      // Change an effect parameter value
        SetEffectEnabled,    // Enable/disable an effect
        SetEffectMix,        // Change effect wet/dry mix
        SetInputGain,        // Change master input gain
        SetOutputGain,       // Change master output gain
        AddEffect,           // Signal that effect list changed (swap pointer)
        RemoveEffect,        // Signal that effect list changed
        MoveEffect,          // Signal that effect list changed
    };

    Type type;
    int effect_index;     // Which effect in the chain
    int param_index;      // Which parameter within the effect
    float value;          // The new value
};

} // namespace Amplitron
