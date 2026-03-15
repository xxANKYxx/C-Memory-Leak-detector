// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry CLI — Standalone Memory Analysis Tool                            ║
// ║                                                                              ║
// ║  Usage:                                                                      ║
// ║    memsentry <source.cpp> [source2.cpp ...] [options]                        ║
// ║    memsentry --dir <folder>                  [options]                        ║
// ║                                                                              ║
// ║  The tool compiles user code with MemSentry linked in, runs it, and          ║
// ║  prints a full memory diagnostics report.                                    ║
// ╚══════════════════════════════════════════════════════════════════════════════╝

#include "memsentry/memsentry.hpp"
#include "cli_options.hpp"
#include "compiler_driver.hpp"
#include "runner.hpp"

#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <cstdlib>

namespace fs = std::filesystem;

// ─── Banner ──────────────────────────────────────────────────────────────────
static void print_banner() {
    std::cout
        << "\n"
        << "  ╔══════════════════════════════════════════════════════════╗\n"
        << "  ║   __  __                ____             _              ║\n"
        << "  ║  |  \\/  | ___ _ __ ___ / ___|  ___ _ __ | |_ _ __ _   _║\n"
        << "  ║  | |\\/| |/ _ \\ '_ ` _ \\\\___ \\ / _ \\ '_ \\| __| '__| | | ║\n"
        << "  ║  | |  | |  __/ | | | | |___) |  __/ | | | |_| |  | |_| ║\n"
        << "  ║  |_|  |_|\\___|_| |_| |_|____/ \\___|_| |_|\\__|_|   \\__, ║\n"
        << "  ║                                                    |___/ ║\n"
        << "  ║         Modern C++20 Memory Diagnostics Tool             ║\n"
        << "  ╚══════════════════════════════════════════════════════════╝\n"
        << "\n";
}

// ─── Help ────────────────────────────────────────────────────────────────────
static void print_usage(const char* program) {
    std::cout
        << "Usage:\n"
        << "  " << program << " <file.cpp> [file2.cpp ...] [options]\n"
        << "  " << program << " --dir <folder>              [options]\n"
        << "\n"
        << "Options:\n"
        << "  --dir <path>       Scan all .cpp files in <path> recursively\n"
        << "  --args <args>      Arguments to pass to your program at runtime\n"
        << "  --format <fmt>     Report format: text (default), json, html\n"
        << "  --output <file>    Write report to file (default: stdout)\n"
        << "  --full             Enable all 9 detectors (default)\n"
        << "  --minimal          Enable only leak + double-free + use-after-free\n"
        << "  --no-run           Compile only, don't run (just verify it builds)\n"
        << "  --keep-binary      Don't delete the compiled binary after analysis\n"
        << "  --compiler <path>  Path to compiler (auto-detected if omitted)\n"
        << "  --std <std>        C++ standard: c++17, c++20 (default: c++20)\n"
        << "  --include <path>   Extra include path(s), can be repeated\n"
        << "  -v, --verbose      Verbose output\n"
        << "  -h, --help         Show this help\n"
        << "\n"
        << "Examples:\n"
        << "  " << program << " my_app.cpp\n"
        << "  " << program << " --dir src/ --format json --output report.json\n"
        << "  " << program << " main.cpp utils.cpp --args \"--flag value\"\n"
        << "\n";
}

int main(int argc, char* argv[]) {
    using namespace memsentry::cli;

    if (argc < 2) {
        print_banner();
        print_usage(argv[0]);
        return 1;
    }

    // ── Parse command-line options ───────────────────────────────────────────
    auto opts = CliOptions::parse(argc, argv);
    if (!opts) {
        std::cerr << "[MemSentry] Error: " << opts.error() << "\n";
        return 1;
    }

    if (opts.value().show_help) {
        print_banner();
        print_usage(argv[0]);
        return 0;
    }

    print_banner();

    // ── Validate source files exist ─────────────────────────────────────────
    if (opts.value().source_files.empty()) {
        std::cerr << "[MemSentry] Error: No source files specified.\n";
        return 1;
    }

    for (const auto& src : opts.value().source_files) {
        if (!fs::exists(src)) {
            std::cerr << "[MemSentry] Error: Source file not found: " << src << "\n";
            return 1;
        }
    }

    std::cout << "[MemSentry] Source files to analyze:\n";
    for (const auto& src : opts.value().source_files) {
        std::cout << "    " << fs::relative(src) << "\n";
    }
    std::cout << "\n";

    // ── Step 1: Compile user code with MemSentry ────────────────────────────
    std::cout << "[MemSentry] Step 1/3 — Compiling with MemSentry instrumentation...\n";

    CompilerDriver compiler(opts.value());
    auto compile_result = compiler.compile();
    if (!compile_result) {
        std::cerr << "\n[MemSentry] COMPILATION FAILED:\n" << compile_result.error() << "\n";
        return 2;
    }

    auto binary_path = compile_result.value();
    std::cout << "[MemSentry] Compiled: " << binary_path << "\n\n";

    if (opts.value().no_run) {
        std::cout << "[MemSentry] --no-run specified, skipping execution.\n";
        return 0;
    }

    // ── Step 2: Run the instrumented binary ─────────────────────────────────
    std::cout << "[MemSentry] Step 2/3 — Running instrumented binary...\n";
    std::cout << std::string(60, '-') << "\n";

    Runner runner(binary_path, opts.value().program_args);
    auto run_result = runner.run();

    std::cout << std::string(60, '-') << "\n";

    if (!run_result) {
        std::cerr << "[MemSentry] Warning: Program exited with errors.\n";
        std::cerr << "            " << run_result.error() << "\n\n";
    } else {
        std::cout << "[MemSentry] Program exited normally (code "
                  << run_result.value() << ")\n\n";
    }

    // ── Step 3: Display the memory analysis report ──────────────────────────
    std::cout << "[MemSentry] Step 3/3 — Memory Analysis Report\n";
    std::cout << std::string(60, '=') << "\n\n";

    // The report is captured from the instrumented binary's stderr output
    // (MemSentry's hooks write the report on shutdown via MemSentryGuard)
    std::cout << runner.captured_stderr();

    if (runner.captured_stderr().empty()) {
        std::cout << "  No memory issues detected. Your code is clean!\n";
    }

    std::cout << "\n" << std::string(60, '=') << "\n";

    // ── Cleanup ─────────────────────────────────────────────────────────────
    if (!opts.value().keep_binary && fs::exists(binary_path)) {
        fs::remove(binary_path);
        if (opts.value().verbose) {
            std::cout << "[MemSentry] Cleaned up temporary binary.\n";
        }
    }

    return run_result ? 0 : 3;
}
