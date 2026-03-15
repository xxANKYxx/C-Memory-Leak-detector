// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — Modern C++20 Memory Leak Detector                              ║
// ║  config.hpp — Runtime configuration for all detectors                       ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <filesystem>

namespace memsentry {

// ─── Report output format ────────────────────────────────────────────────────
enum class ReportFormat {
    Text,
    JSON,
    HTML
};

// ─── Runtime configuration struct ────────────────────────────────────────────
// Uses std::optional for fields where "not set" means "use default."
struct Config {
    // Individual detector toggles
    bool enable_leak_detection          = true;
    bool enable_double_free_detection   = true;
    bool enable_use_after_free          = true;
    bool enable_overflow_detection      = true;
    bool enable_mismatch_detection      = true;
    bool enable_uninit_detection        = true;
    bool enable_invalid_free_detection  = true;
    bool enable_cycle_detection         = true;
    bool enable_fragmentation_analysis  = true;

    // Red-zone settings (canary guard bytes before/after each allocation)
    std::size_t redzone_size            = 16;
    static constexpr std::uint8_t redzone_fill  = 0xFD;

    // Quarantine pool: number of freed blocks kept before reuse
    std::size_t quarantine_capacity     = 256;

    // Sentinel values painted on allocations
    static constexpr std::uint8_t uninit_fill   = 0xCD;  // uninitialized
    static constexpr std::uint8_t freed_fill    = 0xDD;  // freed memory

    // Stack trace capture depth
    std::size_t max_stack_depth         = 32;

    // Background scanner interval (milliseconds, 0 = disabled)
    std::size_t scanner_interval_ms     = 1000;

    // Reporting
    ReportFormat report_format          = ReportFormat::Text;
    std::optional<std::filesystem::path> report_output_path = std::nullopt;

    // Verbosity: 0 = errors only, 1 = warnings, 2 = info, 3 = debug
    int verbosity                       = 1;

    // ── Factory for common presets ───────────────────────────────────────────
    [[nodiscard]] static auto default_config() noexcept -> Config {
        return Config{};
    }

    [[nodiscard]] static auto minimal() noexcept -> Config {
        Config cfg{};
        cfg.enable_overflow_detection     = false;
        cfg.enable_uninit_detection       = false;
        cfg.enable_cycle_detection        = false;
        cfg.enable_fragmentation_analysis = false;
        cfg.scanner_interval_ms           = 0;
        cfg.verbosity                     = 0;
        return cfg;
    }

    [[nodiscard]] static auto full_analysis() noexcept -> Config {
        Config cfg{};
        cfg.redzone_size        = 32;
        cfg.quarantine_capacity = 1024;
        cfg.max_stack_depth     = 64;
        cfg.scanner_interval_ms = 500;
        cfg.verbosity           = 3;
        return cfg;
    }
};

} // namespace memsentry
