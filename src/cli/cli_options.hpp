// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry CLI — Command-line option parser                                 ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#pragma once

#include "result.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace memsentry::cli {

namespace fs = std::filesystem;

enum class AnalysisMode {
    Full,       // All 9 detectors
    Minimal     // Leak + DoubleFree + UseAfterFree only
};

enum class OutputFormat {
    Text,
    JSON,
    HTML
};

struct CliOptions {
    // Source files to compile and analyze
    std::vector<fs::path>    source_files;

    // Extra include directories
    std::vector<fs::path>    include_dirs;

    // Analysis
    AnalysisMode             analysis_mode   = AnalysisMode::Full;
    OutputFormat             output_format   = OutputFormat::Text;
    std::string              output_file;        // empty = stdout

    // Compiler
    std::string              compiler_path;      // empty = auto-detect
    std::string              cpp_standard  = "c++20";

    // Runtime
    std::string              program_args;       // args to pass to user's program
    bool                     no_run         = false;
    bool                     keep_binary    = false;
    bool                     verbose        = false;
    bool                     show_help      = false;

    // ── Parse CLI arguments ──────────────────────────────────────────────────
    [[nodiscard]] static auto parse(int argc, char* argv[])
        -> Result<CliOptions>;
};

} // namespace memsentry::cli
