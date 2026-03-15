// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — allocation_tracker.cpp                                         ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#include "memsentry/core/allocation_tracker.hpp"

#include <algorithm>

#ifdef __cpp_lib_ranges
    #include <ranges>
#endif

namespace memsentry {

auto AllocationTracker::instance() -> AllocationTracker& {
    static AllocationTracker tracker;
    return tracker;
}

AllocationTracker::AllocationTracker()
    : config_(Config::default_config()) {}

void AllocationTracker::configure(const Config& cfg) {
    std::unique_lock lock(mutex_);
    config_ = cfg;
}

auto AllocationTracker::config() const noexcept -> const Config& {
    return config_;
}

void AllocationTracker::register_allocation(std::uintptr_t ptr, AllocationRecord record) {
    std::unique_lock lock(mutex_);

    record.address = ptr;
    record.state   = AllocationState::Allocated;

    total_allocated_bytes_ += record.size;
    current_bytes_         += record.size;
    peak_allocated_bytes_   = std::max(peak_allocated_bytes_, current_bytes_);

    allocations_[ptr] = std::move(record);
}

auto AllocationTracker::register_deallocation(std::uintptr_t ptr,
                                               DeallocationType dealloc_type)
    -> DeallocResult
{
    std::unique_lock lock(mutex_);

    // Case 1: Pointer not found in active allocations
    auto it = allocations_.find(ptr);
    if (it == allocations_.end()) {
        // Check quarantine (double free)
        auto freed_it = freed_.find(ptr);
        if (freed_it != freed_.end()) {
            freed_it->second.state = AllocationState::DoubleFree;
            DeallocResult result = DoubleFreeError{freed_it->second};
            lock.unlock();
            notify_observers(result);
            return result;
        }
        // Completely unknown pointer (invalid free)
        DeallocResult result = InvalidFreeError{ptr};
        lock.unlock();
        notify_observers(result);
        return result;
    }

    auto& record = it->second;

    // Case 2: Allocation/deallocation type mismatch
    if (!types_match(record.alloc_type, dealloc_type)) {
        DeallocResult result = MismatchError{record, dealloc_type};
        lock.unlock();
        notify_observers(result);
        return result;
    }

    // Case 3: Successful deallocation
    record.state = AllocationState::Freed;
    current_bytes_ -= record.size;

    DeallocResult result = DeallocSuccess{record};

    // Move to quarantine (for use-after-free detection)
    freed_[ptr] = std::move(record);
    allocations_.erase(it);

    // Trim quarantine if it exceeds capacity
    while (freed_.size() > config_.quarantine_capacity) {
        freed_.erase(freed_.begin());
    }

    lock.unlock();
    notify_observers(result);
    return result;
}

auto AllocationTracker::find(std::uintptr_t ptr) const -> std::optional<AllocationRecord> {
    std::shared_lock lock(mutex_);
    auto it = allocations_.find(ptr);
    if (it != allocations_.end()) {
        return it->second;
    }
    auto freed_it = freed_.find(ptr);
    if (freed_it != freed_.end()) {
        return freed_it->second;
    }
    return std::nullopt;
}

auto AllocationTracker::is_tracked(std::uintptr_t ptr) const -> bool {
    std::shared_lock lock(mutex_);
    return allocations_.contains(ptr);
}

auto AllocationTracker::active_allocation_count() const -> std::size_t {
    std::shared_lock lock(mutex_);
    return allocations_.size();
}

auto AllocationTracker::total_allocated_bytes() const -> std::size_t {
    std::shared_lock lock(mutex_);
    return total_allocated_bytes_;
}

auto AllocationTracker::peak_allocated_bytes() const -> std::size_t {
    std::shared_lock lock(mutex_);
    return peak_allocated_bytes_;
}

auto AllocationTracker::get_leaked_allocations() const -> std::vector<AllocationRecord> {
    std::shared_lock lock(mutex_);
    std::vector<AllocationRecord> leaked;

#ifdef __cpp_lib_ranges
    // C++20 Ranges pipeline: filter only ALLOCATED (never freed) entries
    for (const auto& [addr, record] :
            allocations_ | std::views::filter([](const auto& pair) {
                return pair.second.state == AllocationState::Allocated;
            }))
    {
        leaked.push_back(record);
    }
#else
    for (const auto& [addr, record] : allocations_) {
        if (record.state == AllocationState::Allocated) {
            leaked.push_back(record);
        }
    }
#endif

    // Sort by address for deterministic output
    std::sort(leaked.begin(), leaked.end());
    return leaked;
}

auto AllocationTracker::get_all_allocations() const -> std::vector<AllocationRecord> {
    std::shared_lock lock(mutex_);
    std::vector<AllocationRecord> all;
    all.reserve(allocations_.size() + freed_.size());

    for (const auto& [addr, record] : allocations_) {
        all.push_back(record);
    }
    for (const auto& [addr, record] : freed_) {
        all.push_back(record);
    }

    std::sort(all.begin(), all.end());
    return all;
}

auto AllocationTracker::get_freed_allocations() const -> std::vector<AllocationRecord> {
    std::shared_lock lock(mutex_);
    std::vector<AllocationRecord> result;
    result.reserve(freed_.size());

    for (const auto& [addr, record] : freed_) {
        result.push_back(record);
    }
    return result;
}

void AllocationTracker::register_observer(DetectionCallback callback) {
    std::unique_lock lock(mutex_);
    observers_.push_back(std::move(callback));
}

void AllocationTracker::notify_observers(const DeallocResult& result) {
    // Called outside the lock (observers may re-enter)
    for (const auto& cb : observers_) {
        cb(result);
    }
}

void AllocationTracker::reset() {
    std::unique_lock lock(mutex_);
    allocations_.clear();
    freed_.clear();
    observers_.clear();
    total_allocated_bytes_ = 0;
    peak_allocated_bytes_  = 0;
    current_bytes_         = 0;
    config_ = Config::default_config();
}

} // namespace memsentry
