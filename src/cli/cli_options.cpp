// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry CLI — Command-line option parser (implementation)                ║
// ╚══════════════════════════════════════════════════════════════════════════════╝

#include "cli_options.hpp"
#include <algorithm>
#include <optional>
#include <string_view>

namespace memsentry::cli {

namespace fs = std::filesystem;

auto CliOptions::parse(int argc, char* argv[])
    -> Result<CliOptions>
{
    CliOptions opts;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);

        // ── Flags ────────────────────────────────────────────────────────────
        if (arg == "-h" || arg == "--help") {
            opts.show_help = true;
            return opts;
        }
        if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
            continue;
        }
        if (arg == "--full") {
            opts.analysis_mode = AnalysisMode::Full;
            continue;
        }
        if (arg == "--minimal") {
            opts.analysis_mode = AnalysisMode::Minimal;
            continue;
        }
        if (arg == "--no-run") {
            opts.no_run = true;
            continue;
        }
        if (arg == "--keep-binary") {
            opts.keep_binary = true;
            continue;
        }

        // ── Options with values ──────────────────────────────────────────────
        auto next_arg = [&]() -> std::optional<std::string_view> {
            if (i + 1 >= argc) return std::nullopt;
            return std::string_view(argv[++i]);
        };

        auto missing_value = [&]() {
            return Result<CliOptions>::err(
                "Option '" + std::string(arg) + "' requires a value");
        };

        if (arg == "--dir") {
            auto val = next_arg();
            if (!val) return missing_value();

            fs::path dir_path(*val);
            if (!fs::is_directory(dir_path)) {
                return Result<CliOptions>::err("--dir path is not a directory: " + std::string(*val));
            }

            // Recursively collect all .cpp files
            for (const auto& entry : fs::recursive_directory_iterator(dir_path)) {
                if (entry.is_regular_file()) {
                    auto ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".c") {
                        opts.source_files.push_back(entry.path());
                    }
                }
            }
            continue;
        }

        if (arg == "--format") {
            auto val = next_arg();
            if (!val) return missing_value();

            if (*val == "text")      opts.output_format = OutputFormat::Text;
            else if (*val == "json") opts.output_format = OutputFormat::JSON;
            else if (*val == "html") opts.output_format = OutputFormat::HTML;
            else return Result<CliOptions>::err("Unknown format: " + std::string(*val));
            continue;
        }

        if (arg == "--output") {
            auto val = next_arg();
            if (!val) return missing_value();
            opts.output_file = std::string(*val);
            continue;
        }

        if (arg == "--compiler") {
            auto val = next_arg();
            if (!val) return missing_value();
            opts.compiler_path = std::string(*val);
            continue;
        }

        if (arg == "--std") {
            auto val = next_arg();
            if (!val) return missing_value();
            opts.cpp_standard = std::string(*val);
            continue;
        }

        if (arg == "--include") {
            auto val = next_arg();
            if (!val) return missing_value();
            opts.include_dirs.emplace_back(std::string(*val));
            continue;
        }

        if (arg == "--args") {
            auto val = next_arg();
            if (!val) return missing_value();
            opts.program_args = std::string(*val);
            continue;
        }

        // ── Positional: treat as source file ────────────────────────────────
        if (arg.starts_with("-")) {
            return Result<CliOptions>::err("Unknown option: " + std::string(arg));
        }

        fs::path src_path(arg);
        if (src_path.extension() == ".cpp" || src_path.extension() == ".cc" ||
            src_path.extension() == ".cxx" || src_path.extension() == ".c" ||
            src_path.extension() == ".h"   || src_path.extension() == ".hpp") {
            opts.source_files.push_back(src_path);
        } else {
            // Could be a directory passed positionally
            if (fs::is_directory(src_path)) {
                for (const auto& entry : fs::recursive_directory_iterator(src_path)) {
                    if (entry.is_regular_file()) {
                        auto ext = entry.path().extension().string();
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                        if (ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".c") {
                            opts.source_files.push_back(entry.path());
                        }
                    }
                }
            } else {
                return Result<CliOptions>::err("Unknown argument or unsupported file: " + std::string(arg));
            }
        }
    }

    return opts;
}

} // namespace memsentry::cli
