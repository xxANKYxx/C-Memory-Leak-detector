// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — Modern C++20 Memory Leak Detector                              ║
// ║  allocation_record.hpp — Per-allocation metadata                            ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#pragma once

#include <cstddef>
#include <cstdint>
#include <chrono>
#include <compare>
#include <string>
#include <thread>
#include <vector>

#ifdef MEMSENTRY_HAS_SOURCE_LOCATION
    #include <source_location>
#endif

#ifdef MEMSENTRY_HAS_FORMAT
    #include <format>
#endif

namespace memsentry {

// ─── Allocation type tag ─────────────────────────────────────────────────────
enum class AllocationType : std::uint8_t {
    New,         // operator new
    NewArray,    // operator new[]
    Malloc,      // malloc / calloc / realloc
};

[[nodiscard]] constexpr auto allocation_type_name(AllocationType t) noexcept -> const char* {
    switch (t) {
        case AllocationType::New:      return "new";
        case AllocationType::NewArray: return "new[]";
        case AllocationType::Malloc:   return "malloc";
    }
    return "unknown";
}

// ─── Expected deallocation for each type ─────────────────────────────────────
[[nodiscard]] constexpr auto expected_deallocation(AllocationType t) noexcept -> const char* {
    switch (t) {
        case AllocationType::New:      return "delete";
        case AllocationType::NewArray: return "delete[]";
        case AllocationType::Malloc:   return "free";
    }
    return "unknown";
}

// ─── Deallocation type tag ───────────────────────────────────────────────────
enum class DeallocationType : std::uint8_t {
    Delete,
    DeleteArray,
    Free,
};

[[nodiscard]] constexpr auto deallocation_type_name(DeallocationType t) noexcept -> const char* {
    switch (t) {
        case DeallocationType::Delete:      return "delete";
        case DeallocationType::DeleteArray: return "delete[]";
        case DeallocationType::Free:        return "free";
    }
    return "unknown";
}

// ─── Check if alloc/dealloc types match ──────────────────────────────────────
[[nodiscard]] constexpr auto types_match(AllocationType a, DeallocationType d) noexcept -> bool {
    switch (a) {
        case AllocationType::New:      return d == DeallocationType::Delete;
        case AllocationType::NewArray: return d == DeallocationType::DeleteArray;
        case AllocationType::Malloc:   return d == DeallocationType::Free;
    }
    return false;
}

// ─── Allocation lifecycle state (state machine) ──────────────────────────────
enum class AllocationState : std::uint8_t {
    Allocated,      // Active allocation
    Freed,          // Successfully freed
    DoubleFree,     // Error: freed more than once
    InvalidFree,    // Error: freed a pointer we don't track
};

// ─── Stack frame (single entry in a captured call stack) ─────────────────────
struct StackFrame {
    std::uintptr_t  address         = 0;
    std::string     function_name;
    std::string     file_name;
    std::uint32_t   line_number     = 0;

    auto operator<=>(const StackFrame&) const = default;
};

// ─── Source location snapshot ────────────────────────────────────────────────
struct SourceLocationInfo {
    std::string     file_name;
    std::string     function_name;
    std::uint32_t   line            = 0;
    std::uint32_t   column          = 0;

#ifdef MEMSENTRY_HAS_SOURCE_LOCATION
    static auto capture(const std::source_location& loc =
                            std::source_location::current()) -> SourceLocationInfo {
        return SourceLocationInfo{
            .file_name     = loc.file_name(),
            .function_name = loc.function_name(),
            .line          = loc.line(),
            .column        = loc.column(),
        };
    }
#endif

    [[nodiscard]] auto to_string() const -> std::string {
#ifdef MEMSENTRY_HAS_FORMAT
        return std::format("{}:{}:{} in {}", file_name, line, column, function_name);
#else
        return file_name + ":" + std::to_string(line) + ":" +
               std::to_string(column) + " in " + function_name;
#endif
    }
};

// ─── Allocation Record ──────────────────────────────────────────────────────
// The core metadata struct for every tracked allocation.
struct AllocationRecord {
    // Identity
    std::uintptr_t              address         = 0;
    std::size_t                 size            = 0;
    AllocationType              alloc_type      = AllocationType::New;
    AllocationState             state           = AllocationState::Allocated;

    // Where it was allocated
    SourceLocationInfo          source_location;
    std::vector<StackFrame>     stack_trace;

    // Timing
    using Clock = std::chrono::steady_clock;
    Clock::time_point           timestamp       = Clock::now();

    // Thread that performed the allocation
    std::thread::id             thread_id       = std::this_thread::get_id();

    // ── Ordering by address (C++20 spaceship) ────────────────────────────────
    auto operator<=>(const AllocationRecord& other) const noexcept {
        return address <=> other.address;
    }

    auto operator==(const AllocationRecord& other) const noexcept -> bool {
        return address == other.address;
    }

    // ── Human-readable string (C++20 std::format) ────────────────────────────
    [[nodiscard]] auto to_string() const -> std::string {
#ifdef MEMSENTRY_HAS_FORMAT
        return std::format(
            "[0x{:X}] {} bytes via {} — state: {} — at {}",
            address, size,
            allocation_type_name(alloc_type),
            state_name(),
            source_location.to_string()
        );
#else
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "[0x%llX] %zu bytes via %s — state: %s — at %s",
            static_cast<unsigned long long>(address), size,
            allocation_type_name(alloc_type),
            state_name(),
            source_location.to_string().c_str()
        );
        return std::string(buf);
#endif
    }

    [[nodiscard]] constexpr auto state_name() const noexcept -> const char* {
        switch (state) {
            case AllocationState::Allocated:   return "ALLOCATED";
            case AllocationState::Freed:       return "FREED";
            case AllocationState::DoubleFree:  return "DOUBLE_FREE";
            case AllocationState::InvalidFree: return "INVALID_FREE";
        }
        return "UNKNOWN";
    }
};

} // namespace memsentry
