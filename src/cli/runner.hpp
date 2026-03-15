// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry CLI — Runner                                                     ║
// ║                                                                              ║
// ║  Executes the compiled instrumented binary and captures its output.          ║
// ║  The binary's stderr contains the MemSentry memory analysis report.          ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#pragma once

#include "result.hpp"

#include <filesystem>
#include <string>

namespace memsentry::cli {

namespace fs = std::filesystem;

class Runner {
public:
    Runner(const fs::path& binary_path, const std::string& args = "");

    // Run the binary and capture both stdout and stderr.
    // Returns exit code on success, or error message.
    [[nodiscard]] auto run() -> Result<int>;

    // After run(), retrieve the captured outputs.
    [[nodiscard]] auto captured_stdout() const -> const std::string&;
    [[nodiscard]] auto captured_stderr() const -> const std::string&;

private:
    fs::path    binary_;
    std::string args_;
    std::string stdout_;
    std::string stderr_;
};

} // namespace memsentry::cli
