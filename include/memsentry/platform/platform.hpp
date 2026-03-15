// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — platform.hpp                                                   ║
// ║  Abstract platform interface with compile-time Factory (if constexpr)       ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace memsentry {

// ─── Platform interface ──────────────────────────────────────────────────────
class Platform {
public:
    virtual ~Platform() = default;

    // Get the OS page size
    [[nodiscard]] virtual auto page_size() const -> std::size_t = 0;

    // Protect a memory region (make it inaccessible)
    virtual auto protect_memory(void* addr, std::size_t size) -> bool = 0;

    // Unprotect a memory region (restore read/write)
    virtual auto unprotect_memory(void* addr, std::size_t size) -> bool = 0;

    // Demangle a C++ symbol
    [[nodiscard]] virtual auto demangle_symbol(const std::string& mangled)
        -> std::string = 0;

    // Get platform name
    [[nodiscard]] virtual auto platform_name() const -> std::string = 0;

    // ── Factory (compile-time platform selection) ────────────────────────────
    [[nodiscard]] static auto create() -> std::unique_ptr<Platform>;
};

} // namespace memsentry
