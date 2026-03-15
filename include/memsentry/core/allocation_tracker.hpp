// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — Modern C++20 Memory Leak Detector                              ║
// ║  allocation_tracker.hpp — Central thread-safe allocation registry            ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#pragma once

#include "memsentry/core/allocation_record.hpp"
#include "memsentry/core/config.hpp"

#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <variant>
#include <vector>

#ifdef __cpp_lib_ranges
    #include <ranges>
#endif

namespace memsentry {

// ─── Deallocation result variants ────────────────────────────────────────────
struct DeallocSuccess {
    AllocationRecord record;  // The record that was freed
};

struct DoubleFreeError {
    AllocationRecord record;  // The record that was already freed
};

struct MismatchError {
    AllocationRecord   record;
    DeallocationType   attempted;
};

struct InvalidFreeError {
    std::uintptr_t address;
};

using DeallocResult = std::variant<DeallocSuccess, DoubleFreeError,
                                    MismatchError, InvalidFreeError>;

// ─── Allocation Tracker ──────────────────────────────────────────────────────
// Thread-safe singleton registry of all live & recently freed allocations.
// Uses std::shared_mutex for read-heavy workloads (many lookups, fewer writes).
class AllocationTracker {
public:
    // Meyer's singleton — thread-safe in C++11+
    [[nodiscard]] static auto instance() -> AllocationTracker&;

    // Non-copyable, non-movable
    AllocationTracker(const AllocationTracker&) = delete;
    AllocationTracker& operator=(const AllocationTracker&) = delete;
    AllocationTracker(AllocationTracker&&) = delete;
    AllocationTracker& operator=(AllocationTracker&&) = delete;

    // ── Configuration ────────────────────────────────────────────────────────
    void configure(const Config& cfg);
    [[nodiscard]] auto config() const noexcept -> const Config&;

    // ── Core operations ──────────────────────────────────────────────────────
    void register_allocation(std::uintptr_t ptr, AllocationRecord record);
    auto register_deallocation(std::uintptr_t ptr, DeallocationType dealloc_type) -> DeallocResult;

    // ── Queries (read-locked) ────────────────────────────────────────────────
    [[nodiscard]] auto find(std::uintptr_t ptr) const -> std::optional<AllocationRecord>;
    [[nodiscard]] auto is_tracked(std::uintptr_t ptr) const -> bool;
    [[nodiscard]] auto active_allocation_count() const -> std::size_t;
    [[nodiscard]] auto total_allocated_bytes() const -> std::size_t;
    [[nodiscard]] auto peak_allocated_bytes() const -> std::size_t;

    // ── Leak scanning (using C++20 ranges) ───────────────────────────────────
    [[nodiscard]] auto get_leaked_allocations() const -> std::vector<AllocationRecord>;
    [[nodiscard]] auto get_all_allocations() const -> std::vector<AllocationRecord>;
    [[nodiscard]] auto get_freed_allocations() const -> std::vector<AllocationRecord>;

    // ── Observer callback for detection events ───────────────────────────────
    using DetectionCallback = std::function<void(const DeallocResult&)>;
    void register_observer(DetectionCallback callback);

    // ── Reset (for testing) ──────────────────────────────────────────────────
    void reset();

private:
    AllocationTracker();
    ~AllocationTracker() = default;

    void notify_observers(const DeallocResult& result);

    mutable std::shared_mutex                              mutex_;
    std::unordered_map<std::uintptr_t, AllocationRecord>   allocations_;
    std::unordered_map<std::uintptr_t, AllocationRecord>   freed_;      // quarantine
    std::vector<DetectionCallback>                         observers_;

    std::size_t   total_allocated_bytes_ = 0;
    std::size_t   peak_allocated_bytes_  = 0;
    std::size_t   current_bytes_         = 0;

    Config        config_;
};

} // namespace memsentry
