// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — stacktrace_capture.cpp                                         ║
// ║  Platform-specific stack capture implementations                            ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#include "memsentry/capture/stacktrace_capture.hpp"

#include <sstream>

// ─── Platform-specific includes ──────────────────────────────────────────────
#if defined(MEMSENTRY_PLATFORM_WINDOWS)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <Windows.h>
    #include <DbgHelp.h>
    #pragma comment(lib, "dbghelp.lib")
#elif defined(MEMSENTRY_PLATFORM_LINUX)
    #include <execinfo.h>
    #include <cxxabi.h>
    #include <cstdlib>
    #include <cstring>
#endif

#ifdef MEMSENTRY_HAS_FORMAT
    #include <format>
#endif

namespace memsentry {

auto StacktraceCapture::capture(std::size_t max_depth, std::size_t skip)
    -> std::vector<StackFrame>
{
    std::vector<StackFrame> frames;

#if defined(MEMSENTRY_PLATFORM_WINDOWS)
    // ── Windows: CaptureStackBackTrace + SymFromAddr ─────────────────────────
    static bool sym_initialized = false;
    if (!sym_initialized) {
        SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
        SymInitialize(GetCurrentProcess(), nullptr, TRUE);
        sym_initialized = true;
    }

    std::vector<void*> stack(max_depth + skip);
    USHORT captured = CaptureStackBackTrace(
        static_cast<DWORD>(skip),
        static_cast<DWORD>(max_depth),
        stack.data(),
        nullptr
    );

    // Symbol info buffer
    constexpr DWORD MAX_NAME_LEN = 256;
    auto symbol_buf = std::make_unique<char[]>(
        sizeof(SYMBOL_INFO) + MAX_NAME_LEN * sizeof(TCHAR)
    );
    auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbol_buf.get());
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen   = MAX_NAME_LEN;

    IMAGEHLP_LINE64 line_info{};
    line_info.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

    for (USHORT i = 0; i < captured; ++i) {
        StackFrame frame;
        frame.address = reinterpret_cast<std::uintptr_t>(stack[i]);

        DWORD64 displacement64 = 0;
        if (SymFromAddr(GetCurrentProcess(), frame.address, &displacement64, symbol)) {
            frame.function_name = symbol->Name;
        } else {
            frame.function_name = "<unknown>";
        }

        DWORD displacement32 = 0;
        if (SymGetLineFromAddr64(GetCurrentProcess(), frame.address,
                                  &displacement32, &line_info)) {
            frame.file_name   = line_info.FileName;
            frame.line_number = line_info.LineNumber;
        }

        frames.push_back(std::move(frame));
    }

#elif defined(MEMSENTRY_PLATFORM_LINUX)
    // ── Linux: backtrace() + backtrace_symbols() ─────────────────────────────
    std::vector<void*> buffer(max_depth + skip);
    int captured = backtrace(buffer.data(), static_cast<int>(buffer.size()));

    if (captured <= 0) return frames;

    char** symbols = backtrace_symbols(buffer.data(), captured);
    if (!symbols) return frames;

    auto start = static_cast<int>(std::min(skip, static_cast<std::size_t>(captured)));

    for (int i = start; i < captured && frames.size() < max_depth; ++i) {
        StackFrame frame;
        frame.address       = reinterpret_cast<std::uintptr_t>(buffer[i]);
        frame.function_name = demangle(symbols[i]);
        frames.push_back(std::move(frame));
    }

    free(symbols);

#else
    // ── Unsupported platform: return empty ───────────────────────────────────
    (void)max_depth;
    (void)skip;
#endif

    return frames;
}

auto StacktraceCapture::format(const std::vector<StackFrame>& frames)
    -> std::string
{
    std::ostringstream oss;

    for (std::size_t i = 0; i < frames.size(); ++i) {
        const auto& f = frames[i];
#ifdef MEMSENTRY_HAS_FORMAT
        oss << std::format("  #{:<3} 0x{:016X} in {} ({}:{})\n",
                           i, f.address, f.function_name,
                           f.file_name.empty() ? "??" : f.file_name,
                           f.line_number);
#else
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "  #%-3zu 0x%016llX in %s (%s:%u)\n",
            i,
            static_cast<unsigned long long>(f.address),
            f.function_name.c_str(),
            f.file_name.empty() ? "??" : f.file_name.c_str(),
            f.line_number);
        oss << buf;
#endif
    }

    return oss.str();
}

auto StacktraceCapture::demangle(const std::string& mangled) -> std::string {
#if defined(MEMSENTRY_PLATFORM_LINUX)
    // Try to extract the mangled name from backtrace_symbols format:
    // "./program(mangled+0x1a) [0x400a1a]"
    std::size_t begin = mangled.find('(');
    std::size_t end   = mangled.find('+', begin);
    if (begin == std::string::npos || end == std::string::npos) {
        return mangled;
    }

    std::string symbol = mangled.substr(begin + 1, end - begin - 1);
    if (symbol.empty()) return mangled;

    int status = 0;
    char* demangled = abi::__cxa_demangle(symbol.c_str(), nullptr, nullptr, &status);
    if (status == 0 && demangled) {
        std::string result(demangled);
        free(demangled);
        return result;
    }
    if (demangled) free(demangled);
    return symbol;
#else
    // Windows already uses SymFromAddr with SYMOPT_UNDNAME
    return mangled;
#endif
}

} // namespace memsentry
