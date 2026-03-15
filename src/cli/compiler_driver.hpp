// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry CLI — Compiler Driver                                            ║
// ║                                                                              ║
// ║  Compiles user-provided source files together with the MemSentry library.    ║
// ║  The resulting binary has all memory hooks active — just run it and          ║
// ║  MemSentry will report all memory issues on shutdown.                        ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#pragma once

#include "cli_options.hpp"

#include "result.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace memsentry::cli {

namespace fs = std::filesystem;

class CompilerDriver {
public:
    explicit CompilerDriver(const CliOptions& opts);

    // Compile user source + MemSentry wrapper → executable
    // Returns path to compiled binary or error message
    [[nodiscard]] auto compile() -> Result<fs::path>;

private:
    const CliOptions& opts_;

    // Auto-detect compiler on the system
    [[nodiscard]] auto detect_compiler() const -> Result<std::string>;

    // Generate the instrumentation wrapper that includes the user's main()
    [[nodiscard]] auto generate_wrapper(const fs::path& wrapper_path) const -> bool;

    // Build the compiler command line
    [[nodiscard]] auto build_command(const std::string& compiler,
                                      const fs::path& wrapper,
                                      const fs::path& output) const -> std::string;

    // Execute a command and capture output
    struct ExecResult {
        int         exit_code;
        std::string output;
    };
    [[nodiscard]] static auto exec_command(const std::string& cmd) -> ExecResult;
};

// ─── Helpers to locate MemSentry's own sources at runtime ────────────────────
[[nodiscard]] auto get_memsentry_include_dir() -> fs::path;
[[nodiscard]] auto get_memsentry_sources()     -> std::vector<fs::path>;

} // namespace memsentry::cli
