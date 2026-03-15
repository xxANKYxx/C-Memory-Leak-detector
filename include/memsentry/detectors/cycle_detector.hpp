// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — cycle_detector.hpp                                             ║
// ║  Detects reference cycles in shared_ptr ownership graphs (Tarjan's SCC).    ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#pragma once

#include "memsentry/detectors/detector_base.hpp"

#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace memsentry {

class CycleDetector : public DetectorMixin<CycleDetector> {
public:
    CycleDetector() = default;

    [[nodiscard]] static auto detector_name() -> std::string { return "CycleDetector"; }

    void on_allocate(const AllocationEvent& evt);
    void on_deallocate(const DeallocationEvent& evt);
    void on_access(const AccessEvent& evt);

    // ── Ownership graph management ───────────────────────────────────────────
    // Call when a shared_ptr at 'owner' points to object at 'owned'
    void add_edge(std::uintptr_t owner, std::uintptr_t owned);
    void remove_edge(std::uintptr_t owner, std::uintptr_t owned);
    void remove_node(std::uintptr_t node);

    // Run cycle detection (Tarjan's strongly connected components)
    [[nodiscard]] auto detect_cycles() -> std::vector<std::vector<std::uintptr_t>>;

    [[nodiscard]] auto generate_report() -> DetectorReport;
    void reset();

private:
    mutable std::mutex mutex_;

    // Adjacency list: owner → set of owned pointers
    std::unordered_map<std::uintptr_t, std::unordered_set<std::uintptr_t>> graph_;

    // ── Tarjan's SCC internals ───────────────────────────────────────────────
    struct TarjanState {
        int index = -1;
        int lowlink = -1;
        bool on_stack = false;
    };

    void strongconnect(std::uintptr_t node,
                       std::unordered_map<std::uintptr_t, TarjanState>& state,
                       std::vector<std::uintptr_t>& stack,
                       int& index,
                       std::vector<std::vector<std::uintptr_t>>& sccs);
};

static_assert(MemoryDetector<CycleDetector>);

} // namespace memsentry
