// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — lock_free_queue.hpp                                            ║
// ║  MPSC (Multiple-Producer Single-Consumer) lock-free ring buffer             ║
// ║  for piping allocation events from hooks → analysis thread.                 ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <optional>

namespace memsentry {

// ─── Lock-Free MPSC Ring Buffer ──────────────────────────────────────────────
// Fixed-capacity, wait-free for producers (try_push returns false on full).
// Single consumer can pop without blocking.
template<typename T, std::size_t Capacity = 4096>
class LockFreeQueue {
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of 2 for efficient modulo");

public:
    LockFreeQueue() : head_(0), tail_(0) {
        for (auto& slot : buffer_) {
            slot.sequence.store(0, std::memory_order_relaxed);
        }
        for (std::size_t i = 0; i < Capacity; ++i) {
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    // ── Producer: try to enqueue (lock-free, wait-free) ──────────────────────
    auto try_push(const T& value) -> bool {
        Slot* slot;
        std::size_t pos = tail_.load(std::memory_order_relaxed);

        for (;;) {
            slot = &buffer_[pos & (Capacity - 1)];
            std::size_t seq = slot->sequence.load(std::memory_order_acquire);
            auto diff = static_cast<std::intptr_t>(seq) -
                        static_cast<std::intptr_t>(pos);

            if (diff == 0) {
                // Slot is available — try to claim it
                if (tail_.compare_exchange_weak(pos, pos + 1,
                                                 std::memory_order_relaxed)) {
                    break;
                }
            } else if (diff < 0) {
                // Queue is full
                return false;
            } else {
                // Another producer claimed this slot, retry
                pos = tail_.load(std::memory_order_relaxed);
            }
        }

        slot->data = value;
        slot->sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

    // ── Consumer: try to dequeue ─────────────────────────────────────────────
    auto try_pop() -> std::optional<T> {
        Slot* slot;
        std::size_t pos = head_.load(std::memory_order_relaxed);

        for (;;) {
            slot = &buffer_[pos & (Capacity - 1)];
            std::size_t seq = slot->sequence.load(std::memory_order_acquire);
            auto diff = static_cast<std::intptr_t>(seq) -
                        static_cast<std::intptr_t>(pos + 1);

            if (diff == 0) {
                // Data is ready
                if (head_.compare_exchange_weak(pos, pos + 1,
                                                 std::memory_order_relaxed)) {
                    break;
                }
            } else if (diff < 0) {
                // Queue is empty
                return std::nullopt;
            } else {
                pos = head_.load(std::memory_order_relaxed);
            }
        }

        T value = std::move(slot->data);
        slot->sequence.store(pos + Capacity, std::memory_order_release);
        return value;
    }

    // ── Queries ──────────────────────────────────────────────────────────────
    [[nodiscard]] auto approximate_size() const -> std::size_t {
        auto h = head_.load(std::memory_order_relaxed);
        auto t = tail_.load(std::memory_order_relaxed);
        return (t >= h) ? (t - h) : 0;
    }

    [[nodiscard]] auto is_empty() const -> bool {
        return approximate_size() == 0;
    }

    [[nodiscard]] static constexpr auto capacity() -> std::size_t {
        return Capacity;
    }

private:
    struct Slot {
        std::atomic<std::size_t> sequence;
        T data;
    };

    alignas(64) std::array<Slot, Capacity> buffer_;
    alignas(64) std::atomic<std::size_t> head_;
    alignas(64) std::atomic<std::size_t> tail_;
};

} // namespace memsentry
