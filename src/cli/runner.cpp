// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry CLI — Runner (implementation)                                    ║
// ╚══════════════════════════════════════════════════════════════════════════════╝

#include "runner.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <Windows.h>
    #include <process.h>  // _spawnl
#else
    #include <cstdlib>
#endif

namespace memsentry::cli {

Runner::Runner(const fs::path& binary_path, const std::string& args)
    : binary_(binary_path), args_(args)
{}

auto Runner::run() -> Result<int> {
    if (!fs::exists(binary_)) {
        return Result<int>::err("Binary not found: " + binary_.string());
    }

    // The report is written to a temp file by the instrumented wrapper
    auto report_file = fs::temp_directory_path() / "memsentry_build" / "_memsentry_report.txt";
    auto stdout_file = fs::temp_directory_path() / "memsentry_build" / "_memsentry_stdout.txt";
    // Remove stale files from previous runs
    std::error_code ec;
    fs::remove(report_file, ec);
    fs::remove(stdout_file, ec);

    int exit_code = -1;

#ifdef _WIN32
    // Use CreateProcess on Windows.
    // Redirect BOTH stdout and stderr to files to avoid pipe deadlocks.
    // (If the parent's stderr is a pipe and nobody drains it, the child
    //  blocks when the pipe buffer fills — classic deadlock.)
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    // Create files for child stdout and stderr capture
    HANDLE hStdout = CreateFileA(
        stdout_file.string().c_str(),
        GENERIC_WRITE, FILE_SHARE_READ, &sa,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr
    );

    auto stderr_file = fs::temp_directory_path() / "memsentry_build" / "_memsentry_stderr.txt";
    fs::remove(stderr_file, ec);

    HANDLE hStderr = CreateFileA(
        stderr_file.string().c_str(),
        GENERIC_WRITE, FILE_SHARE_READ, &sa,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr
    );

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = hStdout;
    si.hStdError  = hStderr;

    PROCESS_INFORMATION pi{};

    std::string cmd_line = binary_.string();
    if (!args_.empty()) {
        cmd_line += " " + args_;
    }

    std::vector<char> cmd_buf(cmd_line.begin(), cmd_line.end());
    cmd_buf.push_back('\0');

    BOOL ok = CreateProcessA(
        binary_.string().c_str(),
        cmd_buf.data(),
        nullptr, nullptr,
        TRUE,   // inherit handles (for stdout/stderr redirect)
        0,
        nullptr, nullptr,
        &si, &pi
    );

    if (!ok) {
        DWORD err = GetLastError();
        CloseHandle(hStdout);
        CloseHandle(hStderr);
        return Result<int>::err(
            "CreateProcess failed (error " + std::to_string(err) + ") for: " + binary_.string()
        );
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    exit_code = static_cast<int>(code);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hStdout);
    CloseHandle(hStderr);

    // Read stderr capture (this has the MemSentry report written by default sink)
    if (fs::exists(stderr_file)) {
        std::ifstream in(stderr_file);
        if (in) {
            std::ostringstream ss;
            ss << in.rdbuf();
            // Don't store in stderr_ — we use the report file instead
        }
        fs::remove(stderr_file, ec);
    }

#else
    std::string cmd = binary_.string();
    if (!args_.empty()) {
        cmd += " " + args_;
    }
    cmd += " > \"" + stdout_file.string() + "\"";
    exit_code = std::system(cmd.c_str());
#endif

    // Read user stdout and print it to the console
    if (fs::exists(stdout_file)) {
        std::ifstream in(stdout_file);
        if (in) {
            std::ostringstream ss;
            ss << in.rdbuf();
            stdout_ = ss.str();
        }
        fs::remove(stdout_file, ec);
    }
    if (!stdout_.empty()) {
        std::cout << stdout_;
    }

    // Read the MemSentry report from the temp file
    if (fs::exists(report_file)) {
        std::ifstream in(report_file);
        if (in) {
            std::ostringstream ss;
            ss << in.rdbuf();
            stderr_ = ss.str();
        }
        fs::remove(report_file, ec);
    }

    if (exit_code != 0) {
        return Result<int>::err(
            "Program exited with code " + std::to_string(exit_code)
        );
    }

    return exit_code;
}

auto Runner::captured_stdout() const -> const std::string& { return stdout_; }
auto Runner::captured_stderr() const -> const std::string& { return stderr_; }

} // namespace memsentry::cli
