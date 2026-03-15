// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — linux_platform.hpp                                             ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#pragma once

#include "memsentry/platform/platform.hpp"

#if defined(MEMSENTRY_PLATFORM_LINUX)

namespace memsentry {

class LinuxPlatform : public Platform {
public:
    [[nodiscard]] auto page_size() const -> std::size_t override;
    auto protect_memory(void* addr, std::size_t size) -> bool override;
    auto unprotect_memory(void* addr, std::size_t size) -> bool override;
    [[nodiscard]] auto demangle_symbol(const std::string& mangled) -> std::string override;
    [[nodiscard]] auto platform_name() const -> std::string override { return "Linux"; }
};

} // namespace memsentry

#endif // MEMSENTRY_PLATFORM_LINUX
