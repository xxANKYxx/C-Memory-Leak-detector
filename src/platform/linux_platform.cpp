// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — linux_platform.cpp                                             ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#include "memsentry/platform/linux_platform.hpp"

#if defined(MEMSENTRY_PLATFORM_LINUX)

#include <cxxabi.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstdlib>

namespace memsentry {

auto LinuxPlatform::page_size() const -> std::size_t {
    return static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
}

auto LinuxPlatform::protect_memory(void* addr, std::size_t size) -> bool {
    return mprotect(addr, size, PROT_NONE) == 0;
}

auto LinuxPlatform::unprotect_memory(void* addr, std::size_t size) -> bool {
    return mprotect(addr, size, PROT_READ | PROT_WRITE) == 0;
}

auto LinuxPlatform::demangle_symbol(const std::string& mangled) -> std::string {
    int status = 0;
    char* demangled = abi::__cxa_demangle(mangled.c_str(), nullptr, nullptr, &status);
    if (status == 0 && demangled) {
        std::string result(demangled);
        std::free(demangled);
        return result;
    }
    if (demangled) std::free(demangled);
    return mangled;
}

} // namespace memsentry

#endif // MEMSENTRY_PLATFORM_LINUX
