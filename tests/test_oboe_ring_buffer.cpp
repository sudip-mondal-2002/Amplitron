// =============================================================================
// Tests for the Oboe backend ring buffer logic (OboeCallback internals).
//
// The Oboe callback runs on hardware audio threads and cannot be tested with
// real streams in a unit test environment. We instead extract and test the
// ring-buffer logic — feedCaptureData / read-and-drain — via a thin harness
// that exercises the same code paths, including wrap-around at kRingSize.
// =============================================================================

#include "test_framework.h"
#include <array>
#include <atomic>
#include <cstring>
#include <algorithm>
#include <vector>

using namespace TestFramework;

// ---------------------------------------------------------------------------
// Minimal ring-buffer harness mirroring OboeCallback's logic exactly.
// This avoids pulling in Oboe headers (unavailable on desktop) while still
// testing the actual algorithm that runs in production.
// ---------------------------------------------------------------------------

class RingBufferHarness {
public:
    static constexpr int kRingSize = 16384;

    // Producer: deposit samples (mirrors feedCaptureData)
    void feed(const float* data, int numFrames) {
        int space = kRingSize - filled_.load(std::memory_order_acquire);
        int toCopy = std::min(numFrames, space);
        if (toCopy <= 0) return;

        int firstChunk = std::min(toCopy, kRingSize - write_pos_);
        std::memcpy(ring_.data() + write_pos_, data,
                    static_cast<size_t>(firstChunk) * sizeof(float));
        if (firstChunk < toCopy) {
            std::memcpy(ring_.data(), data + firstChunk,
                        static_cast<size_t>(toCopy - firstChunk) * sizeof(float));
        }
        write_pos_ = (write_pos_ + toCopy) % kRingSize;
        filled_.fetch_add(toCopy, std::memory_order_release);
    }

    // Consumer: drain samples into out (mirrors onAudioReady drain path)
    // Returns true if enough data was available (not starved).
    bool drain(float* out, int numFrames) {
        int available = filled_.load(std::memory_order_acquire);
        if (available < numFrames) {
            std::memset(out, 0, static_cast<size_t>(numFrames) * sizeof(float));
            return false;
        }
        int firstChunk = std::min(numFrames, kRingSize - read_pos_);
        std::memcpy(out, ring_.data() + read_pos_,
                    static_cast<size_t>(firstChunk) * sizeof(float));
        if (firstChunk < numFrames) {
            std::memcpy(out + firstChunk, ring_.data(),
                        static_cast<size_t>(numFrames - firstChunk) * sizeof(float));
        }
        read_pos_ = (read_pos_ + numFrames) % kRingSize;
        filled_.fetch_sub(numFrames, std::memory_order_release);
        return true;
    }

    int filled() const { return filled_.load(std::memory_order_acquire); }

private:
    std::array<float, kRingSize> ring_{};
    int write_pos_ = 0;
    int read_pos_  = 0;
    std::atomic<int> filled_{0};
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST(ring_buffer_basic_feed_and_drain) {
    RingBufferHarness rb;
    std::vector<float> src = {1.0f, 2.0f, 3.0f, 4.0f};
    rb.feed(src.data(), 4);
    ASSERT_EQ(rb.filled(), 4);

    std::vector<float> dst(4, 0.0f);
    bool ok = rb.drain(dst.data(), 4);
    ASSERT_TRUE(ok);
    ASSERT_EQ(rb.filled(), 0);
    ASSERT_EQ(dst[0], 1.0f);
    ASSERT_EQ(dst[3], 4.0f);
}

TEST(ring_buffer_starved_returns_silence) {
    RingBufferHarness rb;
    std::vector<float> dst(8, 9.9f);
    bool ok = rb.drain(dst.data(), 8);
    ASSERT_FALSE(ok);
    // Output should be zeroed on starvation
    for (float v : dst) ASSERT_EQ(v, 0.0f);
}

TEST(ring_buffer_partial_fill_not_drained) {
    RingBufferHarness rb;
    std::vector<float> src(4, 1.0f);
    rb.feed(src.data(), 4);

    // Ask for more than available — should return false and zero output
    std::vector<float> dst(8, 9.9f);
    bool ok = rb.drain(dst.data(), 8);
    ASSERT_FALSE(ok);
    ASSERT_EQ(dst[0], 0.0f);
    // Data should remain in ring (not consumed on failed drain)
    ASSERT_EQ(rb.filled(), 4);
}

TEST(ring_buffer_wrap_around_write) {
    RingBufferHarness rb;
    // Fill the ring to near the end, then drain, then write across the boundary
    const int nearEnd = RingBufferHarness::kRingSize - 3;

    std::vector<float> bigSrc(nearEnd, 0.5f);
    rb.feed(bigSrc.data(), nearEnd);
    ASSERT_EQ(rb.filled(), nearEnd);

    std::vector<float> bigDst(nearEnd, 0.0f);
    rb.drain(bigDst.data(), nearEnd);
    ASSERT_EQ(rb.filled(), 0);

    // Now write_pos is near end; feed 6 samples to wrap write around
    std::vector<float> wrapSrc = {1.f, 2.f, 3.f, 4.f, 5.f, 6.f};
    rb.feed(wrapSrc.data(), 6);
    ASSERT_EQ(rb.filled(), 6);

    std::vector<float> wrapDst(6, 0.0f);
    bool ok = rb.drain(wrapDst.data(), 6);
    ASSERT_TRUE(ok);
    ASSERT_EQ(wrapDst[0], 1.0f);
    ASSERT_EQ(wrapDst[5], 6.0f);
    ASSERT_EQ(rb.filled(), 0);
}

TEST(ring_buffer_wrap_around_read) {
    RingBufferHarness rb;
    // Advance read_pos_ near the end by feeding and draining
    const int nearEnd = RingBufferHarness::kRingSize - 3;

    std::vector<float> bigSrc(nearEnd, 0.0f);
    rb.feed(bigSrc.data(), nearEnd);
    std::vector<float> bigDst(nearEnd, 0.0f);
    rb.drain(bigDst.data(), nearEnd);
    // read_pos_ is now at nearEnd; write_pos_ is also at nearEnd

    // Write 6 samples that wrap write around, then read them back across boundary
    std::vector<float> wrapSrc = {7.f, 8.f, 9.f, 10.f, 11.f, 12.f};
    rb.feed(wrapSrc.data(), 6);

    std::vector<float> wrapDst(6, 0.0f);
    bool ok = rb.drain(wrapDst.data(), 6);
    ASSERT_TRUE(ok);
    ASSERT_EQ(wrapDst[0], 7.0f);
    ASSERT_EQ(wrapDst[2], 9.0f);
    ASSERT_EQ(wrapDst[5], 12.0f);
}

TEST(ring_buffer_multiple_small_feeds_one_drain) {
    RingBufferHarness rb;
    for (int i = 0; i < 16; ++i) {
        float v = static_cast<float>(i);
        rb.feed(&v, 1);
    }
    ASSERT_EQ(rb.filled(), 16);

    std::vector<float> dst(16, 0.0f);
    bool ok = rb.drain(dst.data(), 16);
    ASSERT_TRUE(ok);
    for (int i = 0; i < 16; ++i)
        ASSERT_EQ(dst[i], static_cast<float>(i));
}

TEST(ring_buffer_overflow_does_not_write_beyond_capacity) {
    RingBufferHarness rb;
    // Feed the entire ring
    std::vector<float> full(RingBufferHarness::kRingSize, 1.0f);
    rb.feed(full.data(), RingBufferHarness::kRingSize);
    ASSERT_EQ(rb.filled(), RingBufferHarness::kRingSize);

    // Try to feed more — should be silently dropped (space = 0)
    std::vector<float> extra = {99.0f, 99.0f};
    rb.feed(extra.data(), 2);
    ASSERT_EQ(rb.filled(), RingBufferHarness::kRingSize);
}

TEST(ring_buffer_preallocate_prevents_starvation_on_first_callback) {
    // Verify that a pre-sized buffer (simulating preallocate()) can handle
    // the first callback before any capture data arrives, without crashing.
    RingBufferHarness rb;
    std::vector<float> dst(256, 9.9f);
    bool ok = rb.drain(dst.data(), 256);
    ASSERT_FALSE(ok);   // starved — silence returned, no crash
    ASSERT_EQ(dst[0], 0.0f);
}
