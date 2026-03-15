// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — shadow_memory.cpp                                              ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#include "memsentry/core/shadow_memory.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>

namespace memsentry {

// ─── RegionShadow helpers ────────────────────────────────────────────────────

void ShadowMemory::RegionShadow::set(std::size_t offset, ByteState state) {
    if (offset >= size) return;

    std::size_t byte_idx = offset / 4;
    std::size_t bit_pos  = (offset % 4) * 2;
    auto val = static_cast<std::uint8_t>(state);

    bitmap[byte_idx] = static_cast<std::uint8_t>(
        (bitmap[byte_idx] & ~(0x03 << bit_pos)) | (val << bit_pos)
    );
}

auto ShadowMemory::RegionShadow::get(std::size_t offset) const -> ByteState {
    if (offset >= size) return ByteState::Uninitialized;

    std::size_t byte_idx = offset / 4;
    std::size_t bit_pos  = (offset % 4) * 2;

    return static_cast<ByteState>((bitmap[byte_idx] >> bit_pos) & 0x03);
}

// ─── ShadowMemory singleton ─────────────────────────────────────────────────

auto ShadowMemory::instance() -> ShadowMemory& {
    static ShadowMemory sm;
    return sm;
}

void ShadowMemory::register_region(std::uintptr_t base, std::size_t size, ByteState initial) {
    std::lock_guard lock(mutex_);

    RegionShadow region;
    region.base = base;
    region.size = size;
    region.bitmap.resize((size + 3) / 4, 0);

    // Fill all bytes with initial state
    auto val = static_cast<std::uint8_t>(initial);
    std::uint8_t packed_byte = static_cast<std::uint8_t>(
        val | (val << 2) | (val << 4) | (val << 6)
    );
    std::memset(region.bitmap.data(), packed_byte, region.bitmap.size());

    regions_[base] = std::move(region);
}

void ShadowMemory::unregister_region(std::uintptr_t base) {
    std::lock_guard lock(mutex_);
    regions_.erase(base);
}

void ShadowMemory::set_state(std::uintptr_t addr, ByteState state) {
    std::lock_guard lock(mutex_);
    auto* region = find_region_mut(addr);
    if (region) {
        region->set(addr - region->base, state);
    }
}

void ShadowMemory::set_range(std::uintptr_t base, std::size_t size, ByteState state) {
    std::lock_guard lock(mutex_);
    auto* region = find_region_mut(base);
    if (!region) return;

    for (std::size_t i = 0; i < size; ++i) {
        std::size_t offset = (base + i) - region->base;
        if (offset < region->size) {
            region->set(offset, state);
        }
    }
}

auto ShadowMemory::get_state(std::uintptr_t addr) const -> ByteState {
    std::lock_guard lock(mutex_);
    const auto* region = find_region(addr);
    if (region) {
        return region->get(addr - region->base);
    }
    return ByteState::Uninitialized;
}

auto ShadowMemory::is_initialized(std::uintptr_t addr) const -> bool {
    return get_state(addr) == ByteState::Initialized;
}

auto ShadowMemory::is_redzone(std::uintptr_t addr) const -> bool {
    return get_state(addr) == ByteState::Redzone;
}

auto ShadowMemory::is_freed(std::uintptr_t addr) const -> bool {
    return get_state(addr) == ByteState::Freed;
}

auto ShadowMemory::check_redzone(std::uintptr_t base, std::size_t size) const
    -> std::size_t
{
    std::lock_guard lock(mutex_);
    const auto* region = find_region(base);
    if (!region) return 0;

    std::size_t corrupted = 0;
    for (std::size_t i = 0; i < size; ++i) {
        std::size_t offset = (base + i) - region->base;
        if (offset < region->size && region->get(offset) != ByteState::Redzone) {
            ++corrupted;
        }
    }
    return corrupted;
}

void ShadowMemory::reset() {
    std::lock_guard lock(mutex_);
    regions_.clear();
}

auto ShadowMemory::find_region(std::uintptr_t addr) const -> const RegionShadow* {
    // Find which region contains this address
    for (const auto& [base, region] : regions_) {
        if (addr >= region.base && addr < region.base + region.size) {
            return &region;
        }
    }
    return nullptr;
}

auto ShadowMemory::find_region_mut(std::uintptr_t addr) -> RegionShadow* {
    for (auto& [base, region] : regions_) {
        if (addr >= region.base && addr < region.base + region.size) {
            return &region;
        }
    }
    return nullptr;
}

} // namespace memsentry
