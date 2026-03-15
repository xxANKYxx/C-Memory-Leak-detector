// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — overflow_detector.cpp                                          ║
// ║  Red-zone canary verification on deallocation                               ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#include "memsentry/detectors/overflow_detector.hpp"

#include <cstring>
#include <span>

#ifdef MEMSENTRY_HAS_FORMAT
    #include <format>
#endif

namespace memsentry {

OverflowDetector::OverflowDetector(std::size_t redzone_size)
    : redzone_size_(redzone_size) {}

void OverflowDetector::on_allocate(const AllocationEvent& evt) {
    std::lock_guard lock(mutex_);
    tracked_[evt.address] = AllocationInfo{evt.size};
}

void OverflowDetector::on_deallocate(const DeallocationEvent& evt) {
    std::lock_guard lock(mutex_);

    auto it = tracked_.find(evt.address);
    if (it == tracked_.end()) return;

    // Verify canary integrity before freeing
    if (!check_canary(evt.address, it->second.user_size)) {
        DetectorViolation violation;
        violation.severity = Severity::Critical;
        violation.category = "BUFFER_OVERFLOW";
        violation.address  = evt.address;
        violation.size     = it->second.user_size;

#ifdef MEMSENTRY_HAS_FORMAT
        violation.description = std::format(
            "Buffer overflow/underflow detected at 0x{:X} ({} bytes): "
            "red-zone canary was corrupted",
            evt.address, it->second.user_size
        );
#else
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "Buffer overflow/underflow detected at 0x%llX (%zu bytes): "
            "red-zone canary was corrupted",
            static_cast<unsigned long long>(evt.address), it->second.user_size);
        violation.description = buf;
#endif

        add_violation(std::move(violation));
    }

    tracked_.erase(it);
}

void OverflowDetector::on_access(const AccessEvent& evt) {
    std::lock_guard lock(mutex_);

    // Check if access falls in a redzone region
    for (const auto& [addr, info] : tracked_) {
        auto* user_start = reinterpret_cast<const std::uint8_t*>(addr);
        auto* redzone_front = user_start - redzone_size_;
        auto* redzone_back  = user_start + info.user_size;

        auto access_addr = reinterpret_cast<const std::uint8_t*>(evt.address);

        if ((access_addr >= redzone_front && access_addr < user_start) ||
            (access_addr >= redzone_back &&
             access_addr < redzone_back + redzone_size_))
        {
            DetectorViolation violation;
            violation.severity = Severity::Critical;
            violation.category = "BUFFER_OVERFLOW";
            violation.address  = evt.address;

#ifdef MEMSENTRY_HAS_FORMAT
            violation.description = std::format(
                "Out-of-bounds {} at 0x{:X} — within red-zone of allocation 0x{:X}",
                evt.is_write ? "write" : "read",
                evt.address, addr
            );
#else
            char buf[512];
            std::snprintf(buf, sizeof(buf),
                "Out-of-bounds %s at 0x%llX — within red-zone of allocation 0x%llX",
                evt.is_write ? "write" : "read",
                static_cast<unsigned long long>(evt.address),
                static_cast<unsigned long long>(addr));
            violation.description = buf;
#endif

            add_violation(std::move(violation));
            return;
        }
    }
}

auto OverflowDetector::check_canary(std::uintptr_t user_ptr,
                                      std::size_t user_size) const -> bool
{
    auto* user_start = reinterpret_cast<const std::uint8_t*>(user_ptr);

    // Check front canary (C++20 std::span for safe access)
    auto front_canary = std::span<const std::uint8_t>(
        user_start - redzone_size_, redzone_size_
    );
    for (auto byte : front_canary) {
        if (byte != CANARY_BYTE) return false;
    }

    // Check back canary
    auto back_canary = std::span<const std::uint8_t>(
        user_start + user_size, redzone_size_
    );
    for (auto byte : back_canary) {
        if (byte != CANARY_BYTE) return false;
    }

    return true;
}

auto OverflowDetector::generate_report() -> DetectorReport {
    std::lock_guard lock(mutex_);
    DetectorReport report;
    report.detector_name = name();
    report.violations = violations();
    report.passed = violations().empty();
    return report;
}

void OverflowDetector::reset() {
    std::lock_guard lock(mutex_);
    tracked_.clear();
    clear_violations();
}

} // namespace memsentry
