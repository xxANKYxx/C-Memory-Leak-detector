// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — Modern C++20 Memory Leak Detector                              ║
// ║  allocator_hooks.hpp — Global new/delete/malloc/free overrides              ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#pragma once

#include <cstddef>

namespace memsentry {

// ─── Hook control ────────────────────────────────────────────────────────────
// Enable/disable interception at runtime
void enable_hooks() noexcept;
void disable_hooks() noexcept;
[[nodiscard]] auto hooks_enabled() noexcept -> bool;

// ─── Re-entrance guard ──────────────────────────────────────────────────────
// The tracker itself allocates (hash map, vectors, strings).
// This thread-local guard prevents infinite recursion:
//   user new → hook → tracker.register → hash_map.insert → new → hook (blocked)
class ReentranceGuard {
public:
    ReentranceGuard() noexcept;
    ~ReentranceGuard() noexcept;

    ReentranceGuard(const ReentranceGuard&) = delete;
    ReentranceGuard& operator=(const ReentranceGuard&) = delete;

    [[nodiscard]] static auto is_reentrant() noexcept -> bool;

private:
    static thread_local bool inside_;
    bool was_inside_;
};

} // namespace memsentry
