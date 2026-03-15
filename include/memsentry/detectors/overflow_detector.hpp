// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — overflow_detector.hpp                                          ║
// ║  Detects buffer overflows/underflows via red-zone canary guards.            ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#pragma once

#include "memsentry/detectors/detector_base.hpp"
#include "memsentry/core/config.hpp"

#include <mutex>
#include <unordered_map>

namespace memsentry {

class OverflowDetector : public DetectorMixin<OverflowDetector> {
public:
    explicit OverflowDetector(std::size_t redzone_size = 16);

    [[nodiscard]] static auto detector_name() -> std::string { return "OverflowDetector"; }

    void on_allocate(const AllocationEvent& evt);
    void on_deallocate(const DeallocationEvent& evt);
    void on_access(const AccessEvent& evt);

    // Check canary integrity for a specific allocation
    [[nodiscard]] auto check_canary(std::uintptr_t user_ptr, std::size_t user_size) const -> bool;

    [[nodiscard]] auto generate_report() -> DetectorReport;
    void reset();

    // Canary pattern constants (constexpr for compile-time evaluation)
    static constexpr std::uint8_t CANARY_BYTE = Config::redzone_fill;  // 0xFD

private:
    mutable std::mutex mutex_;
    std::size_t redzone_size_;

    // Track allocations: user_ptr → user_size
    struct AllocationInfo {
        std::size_t user_size;
    };
    std::unordered_map<std::uintptr_t, AllocationInfo> tracked_;
};

static_assert(MemoryDetector<OverflowDetector>);

} // namespace memsentry
