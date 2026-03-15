// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — allocator_hooks.cpp                                            ║
// ║  Global new/delete override with re-entrance guard                          ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#include "memsentry/core/allocator_hooks.hpp"
#include "memsentry/core/allocation_tracker.hpp"
#include "memsentry/core/shadow_memory.hpp"
#include "memsentry/capture/stacktrace_capture.hpp"

#include <atomic>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <new>

namespace memsentry {

namespace {
    std::atomic<bool> g_hooks_enabled{false};
}

void enable_hooks() noexcept  { g_hooks_enabled.store(true, std::memory_order_release); }
void disable_hooks() noexcept { g_hooks_enabled.store(false, std::memory_order_release); }
auto hooks_enabled() noexcept -> bool { return g_hooks_enabled.load(std::memory_order_acquire); }

// ─── Re-entrance guard ──────────────────────────────────────────────────────
thread_local bool ReentranceGuard::inside_ = false;

ReentranceGuard::ReentranceGuard() noexcept
    : was_inside_(inside_)
{
    inside_ = true;
}

ReentranceGuard::~ReentranceGuard() noexcept {
    inside_ = was_inside_;
}

auto ReentranceGuard::is_reentrant() noexcept -> bool {
    return inside_;
}

namespace detail {

// ─── Allocation hook logic ───────────────────────────────────────────────────
static void* tracked_allocate(std::size_t size, AllocationType type) {
    auto& tracker = AllocationTracker::instance();
    const auto& cfg = tracker.config();

    // Total = redzone + user + redzone
    std::size_t total_size = cfg.redzone_size + size + cfg.redzone_size;
    void* raw = std::malloc(total_size);
    if (!raw) return nullptr;

    // Pointer arithmetic: [redzone_front | user_data | redzone_back]
    auto* base = static_cast<std::uint8_t*>(raw);
    auto* user_ptr = base + cfg.redzone_size;

    // Paint redzones with sentinel
    std::memset(base, Config::redzone_fill, cfg.redzone_size);
    std::memset(user_ptr + size, Config::redzone_fill, cfg.redzone_size);

    // Paint user region as uninitialized
    std::memset(user_ptr, Config::uninit_fill, size);

    // Register with tracker
    {
        ReentranceGuard guard;

        AllocationRecord record;
        record.address    = reinterpret_cast<std::uintptr_t>(user_ptr);
        record.size       = size;
        record.alloc_type = type;
        record.state      = AllocationState::Allocated;
        record.stack_trace = StacktraceCapture::capture(cfg.max_stack_depth);

        // Shadow memory: register full region (redzones + user data)
        auto& shadow = ShadowMemory::instance();
        auto region_base = reinterpret_cast<std::uintptr_t>(base);
        shadow.register_region(region_base, total_size, ByteState::Uninitialized);
        shadow.set_range(region_base, cfg.redzone_size, ByteState::Redzone);
        shadow.set_range(reinterpret_cast<std::uintptr_t>(user_ptr + size),
                         cfg.redzone_size, ByteState::Redzone);

        tracker.register_allocation(
            reinterpret_cast<std::uintptr_t>(user_ptr),
            std::move(record)
        );
    }

    return user_ptr;
}

// ─── Deallocation hook logic ─────────────────────────────────────────────────
static void tracked_deallocate(void* ptr, DeallocationType type) noexcept {
    if (!ptr) return;

    auto& tracker = AllocationTracker::instance();
    const auto& cfg = tracker.config();

    auto addr = reinterpret_cast<std::uintptr_t>(ptr);

    ReentranceGuard guard;
    auto result = tracker.register_deallocation(addr, type);

    // Only actually free if it was a successful deallocation
    if (std::holds_alternative<DeallocSuccess>(result)) {
        auto& success = std::get<DeallocSuccess>(result);
        auto* user_ptr = static_cast<std::uint8_t*>(ptr);
        auto* base = user_ptr - cfg.redzone_size;

        // Paint freed memory with sentinel (for use-after-free detection)
        std::memset(user_ptr, Config::freed_fill, success.record.size);

        // Update shadow memory
        auto& shadow = ShadowMemory::instance();
        shadow.set_range(addr, success.record.size, ByteState::Freed);

        // Free the raw allocation
        std::free(base);

        // Unregister shadow region
        shadow.unregister_region(reinterpret_cast<std::uintptr_t>(base));
    }
    // On error (double free, mismatch, etc.), do NOT free — let the error
    // be reported. The memory stays in quarantine or remains allocated.
}

} // namespace detail

} // namespace memsentry

// ═══════════════════════════════════════════════════════════════════════════════
// Global operator new/delete overrides
// Only active when hooks are enabled AND we're not in a re-entrant call.
// ═══════════════════════════════════════════════════════════════════════════════

void* operator new(std::size_t size) {
    if (memsentry::hooks_enabled() && !memsentry::ReentranceGuard::is_reentrant()) {
        if (void* ptr = memsentry::detail::tracked_allocate(size, memsentry::AllocationType::New)) {
            return ptr;
        }
        throw std::bad_alloc();
    }
    void* ptr = std::malloc(size);
    if (!ptr) throw std::bad_alloc();
    return ptr;
}

void* operator new[](std::size_t size) {
    if (memsentry::hooks_enabled() && !memsentry::ReentranceGuard::is_reentrant()) {
        if (void* ptr = memsentry::detail::tracked_allocate(size, memsentry::AllocationType::NewArray)) {
            return ptr;
        }
        throw std::bad_alloc();
    }
    void* ptr = std::malloc(size);
    if (!ptr) throw std::bad_alloc();
    return ptr;
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    if (memsentry::hooks_enabled() && !memsentry::ReentranceGuard::is_reentrant()) {
        return memsentry::detail::tracked_allocate(size, memsentry::AllocationType::New);
    }
    return std::malloc(size);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    if (memsentry::hooks_enabled() && !memsentry::ReentranceGuard::is_reentrant()) {
        return memsentry::detail::tracked_allocate(size, memsentry::AllocationType::NewArray);
    }
    return std::malloc(size);
}

void operator delete(void* ptr) noexcept {
    if (memsentry::hooks_enabled() && !memsentry::ReentranceGuard::is_reentrant()) {
        memsentry::detail::tracked_deallocate(ptr, memsentry::DeallocationType::Delete);
        return;
    }
    std::free(ptr);
}

void operator delete[](void* ptr) noexcept {
    if (memsentry::hooks_enabled() && !memsentry::ReentranceGuard::is_reentrant()) {
        memsentry::detail::tracked_deallocate(ptr, memsentry::DeallocationType::DeleteArray);
        return;
    }
    std::free(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept {
    if (memsentry::hooks_enabled() && !memsentry::ReentranceGuard::is_reentrant()) {
        memsentry::detail::tracked_deallocate(ptr, memsentry::DeallocationType::Delete);
        return;
    }
    std::free(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept {
    if (memsentry::hooks_enabled() && !memsentry::ReentranceGuard::is_reentrant()) {
        memsentry::detail::tracked_deallocate(ptr, memsentry::DeallocationType::DeleteArray);
        return;
    }
    std::free(ptr);
}

void operator delete(void* ptr, const std::nothrow_t&) noexcept {
    if (memsentry::hooks_enabled() && !memsentry::ReentranceGuard::is_reentrant()) {
        memsentry::detail::tracked_deallocate(ptr, memsentry::DeallocationType::Delete);
        return;
    }
    std::free(ptr);
}

void operator delete[](void* ptr, const std::nothrow_t&) noexcept {
    if (memsentry::hooks_enabled() && !memsentry::ReentranceGuard::is_reentrant()) {
        memsentry::detail::tracked_deallocate(ptr, memsentry::DeallocationType::DeleteArray);
        return;
    }
    std::free(ptr);
}
