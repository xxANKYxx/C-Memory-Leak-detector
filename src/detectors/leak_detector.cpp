// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — leak_detector.cpp                                              ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#include "memsentry/detectors/leak_detector.hpp"

#ifdef MEMSENTRY_HAS_FORMAT
    #include <format>
#endif

#ifdef __cpp_lib_ranges
    #include <ranges>
#endif

namespace memsentry {

void LeakDetector::on_allocate(const AllocationEvent& /*evt*/) {
    std::lock_guard lock(mutex_);
    ++allocation_count_;
}

void LeakDetector::on_deallocate(const DeallocationEvent& /*evt*/) {
    std::lock_guard lock(mutex_);
    ++deallocation_count_;
}

void LeakDetector::on_access(const AccessEvent& /*evt*/) {
    // LeakDetector does not monitor access events
}

auto LeakDetector::generate_report() -> DetectorReport {
    DetectorReport report;
    report.detector_name = name();

    // Query the AllocationTracker for all unfreed allocations
    auto leaked = AllocationTracker::instance().get_leaked_allocations();

#ifdef __cpp_lib_ranges
    // C++20 Ranges: transform leaked records into violations
    for (const auto& record : leaked
            | std::views::filter([](const AllocationRecord& r) {
                return r.state == AllocationState::Allocated;
            }))
    {
        DetectorViolation violation;
        violation.severity    = Severity::Error;
        violation.category    = "MEMORY_LEAK";
        violation.address     = record.address;
        violation.size        = record.size;
        violation.source_location = record.source_location.to_string();

#ifdef MEMSENTRY_HAS_FORMAT
        violation.description = std::format(
            "Leaked {} bytes allocated via {} at {}",
            record.size,
            allocation_type_name(record.alloc_type),
            record.source_location.to_string()
        );
#else
        violation.description = "Leaked " + std::to_string(record.size) +
            " bytes allocated via " + allocation_type_name(record.alloc_type) +
            " at " + record.source_location.to_string();
#endif

        // Format stack trace
        for (const auto& frame : record.stack_trace) {
            violation.stack_trace += "  " + frame.function_name + " (" +
                                    frame.file_name + ":" +
                                    std::to_string(frame.line_number) + ")\n";
        }

        report.violations.push_back(std::move(violation));
    }
#else
    for (const auto& record : leaked) {
        if (record.state != AllocationState::Allocated) continue;

        DetectorViolation violation;
        violation.severity    = Severity::Error;
        violation.category    = "MEMORY_LEAK";
        violation.address     = record.address;
        violation.size        = record.size;
        violation.source_location = record.source_location.to_string();
        violation.description = "Leaked " + std::to_string(record.size) +
            " bytes allocated via " + allocation_type_name(record.alloc_type) +
            " at " + record.source_location.to_string();

        for (const auto& frame : record.stack_trace) {
            violation.stack_trace += "  " + frame.function_name + " (" +
                                    frame.file_name + ":" +
                                    std::to_string(frame.line_number) + ")\n";
        }

        report.violations.push_back(std::move(violation));
    }
#endif

    report.passed = report.violations.empty();
    return report;
}

void LeakDetector::reset() {
    std::lock_guard lock(mutex_);
    allocation_count_   = 0;
    deallocation_count_ = 0;
    clear_violations();
}

} // namespace memsentry
