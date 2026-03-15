// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — Modern C++20 Memory Leak Detector                              ║
// ║  shadow_memory.hpp — Byte-level metadata bitmap                             ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <unordered_map>

namespace memsentry {

// ─── Per-byte state (2 bits) ─────────────────────────────────────────────────
enum class ByteState : std::uint8_t {
    Uninitialized = 0b00,   // Allocated but never written
    Initialized   = 0b01,   // Written at least once
    Freed         = 0b10,   // Memory has been freed
    Redzone       = 0b11,   // Guard/canary region — must not be touched
};

// ─── Shadow Memory ──────────────────────────────────────────────────────────
// Maintains a 2-bit-per-byte bitmap for tracked allocations.
// Used by UninitDetector and OverflowDetector.
class ShadowMemory {
public:
    [[nodiscard]] static auto instance() -> ShadowMemory&;

    ShadowMemory(const ShadowMemory&) = delete;
    ShadowMemory& operator=(const ShadowMemory&) = delete;

    // ── Region lifecycle ─────────────────────────────────────────────────────
    void register_region(std::uintptr_t base, std::size_t size, ByteState initial);
    void unregister_region(std::uintptr_t base);

    // ── Byte-level access ────────────────────────────────────────────────────
    void set_state(std::uintptr_t addr, ByteState state);
    void set_range(std::uintptr_t base, std::size_t size, ByteState state);
    [[nodiscard]] auto get_state(std::uintptr_t addr) const -> ByteState;

    // ── Queries ──────────────────────────────────────────────────────────────
    [[nodiscard]] auto is_initialized(std::uintptr_t addr) const -> bool;
    [[nodiscard]] auto is_redzone(std::uintptr_t addr) const -> bool;
    [[nodiscard]] auto is_freed(std::uintptr_t addr) const -> bool;

    // ── Canary/Redzone verification ──────────────────────────────────────────
    // Returns the number of corrupted redzone bytes in the given range.
    [[nodiscard]] auto check_redzone(std::uintptr_t base, std::size_t size) const
        -> std::size_t;

    void reset();

private:
    ShadowMemory() = default;

    // Each tracked region stores a vector of 2-bit states packed 4 per byte.
    struct RegionShadow {
        std::uintptr_t         base = 0;
        std::size_t            size = 0;
        std::vector<std::uint8_t> bitmap;  // ceil(size/4) bytes

        void set(std::size_t offset, ByteState state);
        [[nodiscard]] auto get(std::size_t offset) const -> ByteState;
    };

    [[nodiscard]] auto find_region(std::uintptr_t addr) const
        -> const RegionShadow*;

    auto find_region_mut(std::uintptr_t addr) -> RegionShadow*;

    mutable std::mutex                                          mutex_;
    std::unordered_map<std::uintptr_t, RegionShadow>            regions_;
};

} // namespace memsentry
