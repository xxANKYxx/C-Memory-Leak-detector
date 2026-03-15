// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — thread_safe_map.hpp                                            ║
// ║  A std::shared_mutex-guarded concurrent map wrapper                         ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#pragma once

#include <functional>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace memsentry {

// ─── Thread-safe concurrent map ──────────────────────────────────────────────
// Uses std::shared_mutex for optimal read-heavy workloads:
//   - Reads acquire shared_lock (multiple concurrent readers)
//   - Writes acquire unique_lock (exclusive access)
template<typename Key, typename Value,
         typename Hash = std::hash<Key>,
         typename Equal = std::equal_to<Key>>
class ThreadSafeMap {
public:
    ThreadSafeMap() = default;

    // ── Write operations (unique_lock) ───────────────────────────────────────
    void insert(const Key& key, Value value) {
        std::unique_lock lock(mutex_);
        map_[key] = std::move(value);
    }

    template<typename... Args>
    void emplace(const Key& key, Args&&... args) {
        std::unique_lock lock(mutex_);
        map_.emplace(key, Value(std::forward<Args>(args)...));
    }

    auto erase(const Key& key) -> bool {
        std::unique_lock lock(mutex_);
        return map_.erase(key) > 0;
    }

    void clear() {
        std::unique_lock lock(mutex_);
        map_.clear();
    }

    // ── Read operations (shared_lock) ────────────────────────────────────────
    [[nodiscard]] auto find(const Key& key) const -> std::optional<Value> {
        std::shared_lock lock(mutex_);
        auto it = map_.find(key);
        if (it != map_.end()) return it->second;
        return std::nullopt;
    }

    [[nodiscard]] auto contains(const Key& key) const -> bool {
        std::shared_lock lock(mutex_);
        return map_.contains(key);
    }

    [[nodiscard]] auto size() const -> std::size_t {
        std::shared_lock lock(mutex_);
        return map_.size();
    }

    [[nodiscard]] auto empty() const -> bool {
        std::shared_lock lock(mutex_);
        return map_.empty();
    }

    // ── Bulk operations ──────────────────────────────────────────────────────
    [[nodiscard]] auto keys() const -> std::vector<Key> {
        std::shared_lock lock(mutex_);
        std::vector<Key> result;
        result.reserve(map_.size());
        for (const auto& [k, v] : map_) {
            result.push_back(k);
        }
        return result;
    }

    [[nodiscard]] auto values() const -> std::vector<Value> {
        std::shared_lock lock(mutex_);
        std::vector<Value> result;
        result.reserve(map_.size());
        for (const auto& [k, v] : map_) {
            result.push_back(v);
        }
        return result;
    }

    // ── Visitor pattern (read-locked iteration) ──────────────────────────────
    template<typename Func>
    void for_each(Func&& func) const {
        std::shared_lock lock(mutex_);
        for (const auto& [key, value] : map_) {
            func(key, value);
        }
    }

    // ── Visitor pattern (write-locked mutation) ──────────────────────────────
    template<typename Func>
    void mutate(const Key& key, Func&& func) {
        std::unique_lock lock(mutex_);
        auto it = map_.find(key);
        if (it != map_.end()) {
            func(it->second);
        }
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<Key, Value, Hash, Equal> map_;
};

} // namespace memsentry
