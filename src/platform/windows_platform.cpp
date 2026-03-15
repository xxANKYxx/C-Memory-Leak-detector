// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — windows_platform.cpp                                           ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#include "memsentry/platform/windows_platform.hpp"

#if defined(MEMSENTRY_PLATFORM_WINDOWS)

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace memsentry {

auto WindowsPlatform::page_size() const -> std::size_t {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwPageSize;
}

auto WindowsPlatform::protect_memory(void* addr, std::size_t size) -> bool {
    DWORD old_protect;
    return VirtualProtect(addr, size, PAGE_NOACCESS, &old_protect) != 0;
}

auto WindowsPlatform::unprotect_memory(void* addr, std::size_t size) -> bool {
    DWORD old_protect;
    return VirtualProtect(addr, size, PAGE_READWRITE, &old_protect) != 0;
}

auto WindowsPlatform::demangle_symbol(const std::string& mangled) -> std::string {
    // On Windows, SymFromAddr with SYMOPT_UNDNAME handles demangling
    return mangled;
}

} // namespace memsentry

#endif // MEMSENTRY_PLATFORM_WINDOWS
