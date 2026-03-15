// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — Modern C++20 Memory Leak Detector                              ║
// ║  detector_base.hpp — C++20 Concept for memory detectors + CRTP mixin        ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#pragma once

#include "memsentry/core/allocation_record.hpp"
#include "memsentry/reporting/report_types.hpp"

#include <concepts>
#include <string>
#include <type_traits>
#include <vector>

namespace memsentry {

// ─── Detection event types ───────────────────────────────────────────────────
struct AllocationEvent {
    std::uintptr_t   address;
    std::size_t      size;
    AllocationType   type;
};

struct DeallocationEvent {
    std::uintptr_t   address;
    DeallocationType type;
};

struct AccessEvent {
    std::uintptr_t address;
    std::size_t    size;
    bool           is_write;
};

// ─── C++20 Concept: MemoryDetector ──────────────────────────────────────────
// Any detector must satisfy this interface.
template<typename T>
concept MemoryDetector = requires(T det,
                                   const AllocationEvent& alloc_evt,
                                   const DeallocationEvent& dealloc_evt,
                                   const AccessEvent& access_evt)
{
    { det.name() }                      -> std::convertible_to<std::string>;
    { det.on_allocate(alloc_evt) }      -> std::same_as<void>;
    { det.on_deallocate(dealloc_evt) }  -> std::same_as<void>;
    { det.on_access(access_evt) }       -> std::same_as<void>;
    { det.generate_report() }           -> std::same_as<DetectorReport>;
    { det.is_enabled() }                -> std::convertible_to<bool>;
    { det.reset() }                     -> std::same_as<void>;
};

// ─── CRTP base mixin ────────────────────────────────────────────────────────
// Provides common enable/disable and logging to derived detectors.
template<typename Derived>
class DetectorMixin {
public:
    void enable()  noexcept { enabled_ = true; }
    void disable() noexcept { enabled_ = false; }
    [[nodiscard]] auto is_enabled() const noexcept -> bool { return enabled_; }

    [[nodiscard]] auto name() const -> std::string {
        return static_cast<const Derived*>(this)->detector_name();
    }

protected:
    DetectorMixin() = default;
    ~DetectorMixin() = default;

    // Convenience: add a finding to the internal violations list
    void add_violation(DetectorViolation violation) {
        violations_.push_back(std::move(violation));
    }

    [[nodiscard]] auto violations() const -> const std::vector<DetectorViolation>& {
        return violations_;
    }

    void clear_violations() { violations_.clear(); }

private:
    bool enabled_ = true;
    std::vector<DetectorViolation> violations_;
};

// ─── Detector pipeline (variadic fold) ──────────────────────────────────────
// Chains multiple detectors using C++17 fold expressions.
template<MemoryDetector... Detectors>
class DetectorPipeline {
public:
    explicit DetectorPipeline(Detectors&... dets) : detectors_(dets...) {}

    void on_allocate(const AllocationEvent& evt) {
        std::apply([&](auto&... det) {
            (void(det.is_enabled() ? (det.on_allocate(evt), 0) : 0), ...);
        }, detectors_);
    }

    void on_deallocate(const DeallocationEvent& evt) {
        std::apply([&](auto&... det) {
            (void(det.is_enabled() ? (det.on_deallocate(evt), 0) : 0), ...);
        }, detectors_);
    }

    void on_access(const AccessEvent& evt) {
        std::apply([&](auto&... det) {
            (void(det.is_enabled() ? (det.on_access(evt), 0) : 0), ...);
        }, detectors_);
    }

    auto generate_reports() -> std::vector<DetectorReport> {
        std::vector<DetectorReport> reports;
        std::apply([&](auto&... det) {
            (reports.push_back(det.generate_report()), ...);
        }, detectors_);
        return reports;
    }

private:
    std::tuple<Detectors&...> detectors_;
};

} // namespace memsentry
