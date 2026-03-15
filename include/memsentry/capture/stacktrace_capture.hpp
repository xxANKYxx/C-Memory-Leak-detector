// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — stacktrace_capture.hpp                                         ║
// ║  Platform-abstracted stack trace capture (Windows + Linux)                   ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#pragma once

#include "memsentry/core/allocation_record.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace memsentry {

// ─── Stack Trace Capture ─────────────────────────────────────────────────────
// Abstracts platform-specific stack walking behind a clean C++20 interface.
// Implements the Flyweight pattern: identical stack traces share storage.
class StacktraceCapture {
public:
    // Capture the current call stack (skipping the top `skip` frames)
    [[nodiscard]] static auto capture(std::size_t max_depth = 32,
                                       std::size_t skip = 3)
        -> std::vector<StackFrame>;

    // Format a stack trace for display
    [[nodiscard]] static auto format(const std::vector<StackFrame>& frames)
        -> std::string;

    // Demangle a C++ symbol name (platform-specific)
    [[nodiscard]] static auto demangle(const std::string& mangled) -> std::string;
};

// ─── Source Location Wrapper ─────────────────────────────────────────────────
// Wraps C++20 std::source_location for macro-free call-site capture.
class SourceLocationCapture {
public:
#ifdef MEMSENTRY_HAS_SOURCE_LOCATION
    [[nodiscard]] static auto capture(
        const std::source_location& loc = std::source_location::current())
        -> SourceLocationInfo
    {
        return SourceLocationInfo::capture(loc);
    }
#else
    // Fallback: no source location available
    [[nodiscard]] static auto capture() -> SourceLocationInfo {
        return SourceLocationInfo{"<unknown>", "<unknown>", 0, 0};
    }
#endif
};

} // namespace memsentry
