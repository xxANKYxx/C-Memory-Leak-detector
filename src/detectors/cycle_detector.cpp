// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry — cycle_detector.cpp                                             ║
// ║  Tarjan's Strongly Connected Components for shared_ptr cycle detection      ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#include "memsentry/detectors/cycle_detector.hpp"

#include <algorithm>
#include <sstream>
#include <stack>

#ifdef MEMSENTRY_HAS_FORMAT
    #include <format>
#endif

namespace memsentry {

void CycleDetector::on_allocate(const AllocationEvent& /*evt*/) {
    // Cycle detection uses its own graph API (add_edge/remove_edge)
}

void CycleDetector::on_deallocate(const DeallocationEvent& evt) {
    std::lock_guard lock(mutex_);
    remove_node(evt.address);
}

void CycleDetector::on_access(const AccessEvent& /*evt*/) {
    // Not used for cycle detection
}

void CycleDetector::add_edge(std::uintptr_t owner, std::uintptr_t owned) {
    std::lock_guard lock(mutex_);
    graph_[owner].insert(owned);
    // Ensure owned exists as a node even if it has no outgoing edges
    if (!graph_.contains(owned)) {
        graph_[owned] = {};
    }
}

void CycleDetector::remove_edge(std::uintptr_t owner, std::uintptr_t owned) {
    std::lock_guard lock(mutex_);
    auto it = graph_.find(owner);
    if (it != graph_.end()) {
        it->second.erase(owned);
    }
}

void CycleDetector::remove_node(std::uintptr_t node) {
    // Already locked by on_deallocate caller, or user must lock externally
    graph_.erase(node);
    // Remove all edges pointing to this node
    for (auto& [owner, edges] : graph_) {
        edges.erase(node);
    }
}

auto CycleDetector::detect_cycles() -> std::vector<std::vector<std::uintptr_t>> {
    std::lock_guard lock(mutex_);

    std::unordered_map<std::uintptr_t, TarjanState> state;
    std::vector<std::uintptr_t> stack;
    int index = 0;
    std::vector<std::vector<std::uintptr_t>> sccs;

    for (const auto& [node, _] : graph_) {
        if (state[node].index == -1) {
            strongconnect(node, state, stack, index, sccs);
        }
    }

    // Filter: only return SCCs with size > 1 (actual cycles)
    std::vector<std::vector<std::uintptr_t>> cycles;
    for (auto& scc : sccs) {
        if (scc.size() > 1) {
            cycles.push_back(std::move(scc));
        }
    }
    return cycles;
}

void CycleDetector::strongconnect(
    std::uintptr_t node,
    std::unordered_map<std::uintptr_t, TarjanState>& state,
    std::vector<std::uintptr_t>& stack,
    int& index,
    std::vector<std::vector<std::uintptr_t>>& sccs)
{
    state[node].index   = index;
    state[node].lowlink = index;
    ++index;
    stack.push_back(node);
    state[node].on_stack = true;

    // Visit successors
    auto it = graph_.find(node);
    if (it != graph_.end()) {
        for (auto successor : it->second) {
            if (state[successor].index == -1) {
                strongconnect(successor, state, stack, index, sccs);
                state[node].lowlink = std::min(state[node].lowlink,
                                                state[successor].lowlink);
            } else if (state[successor].on_stack) {
                state[node].lowlink = std::min(state[node].lowlink,
                                                state[successor].index);
            }
        }
    }

    // Root of SCC
    if (state[node].lowlink == state[node].index) {
        std::vector<std::uintptr_t> scc;
        std::uintptr_t w;
        do {
            w = stack.back();
            stack.pop_back();
            state[w].on_stack = false;
            scc.push_back(w);
        } while (w != node);

        sccs.push_back(std::move(scc));
    }
}

auto CycleDetector::generate_report() -> DetectorReport {
    DetectorReport report;
    report.detector_name = name();

    auto cycles = detect_cycles();

    for (const auto& cycle : cycles) {
        DetectorViolation violation;
        violation.severity = Severity::Error;
        violation.category = "REFERENCE_CYCLE";

#ifdef MEMSENTRY_HAS_FORMAT
        std::string cycle_str;
        for (std::size_t i = 0; i < cycle.size(); ++i) {
            if (i > 0) cycle_str += " -> ";
            cycle_str += std::format("0x{:X}", cycle[i]);
        }
        cycle_str += std::format(" -> 0x{:X}", cycle[0]); // close the loop

        violation.description = std::format(
            "Reference cycle detected ({} nodes): {}",
            cycle.size(), cycle_str
        );
#else
        std::ostringstream oss;
        for (std::size_t i = 0; i < cycle.size(); ++i) {
            if (i > 0) oss << " -> ";
            oss << "0x" << std::hex << cycle[i];
        }
        oss << " -> 0x" << std::hex << cycle[0];
        violation.description = "Reference cycle detected (" +
            std::to_string(cycle.size()) + " nodes): " + oss.str();
#endif

        report.violations.push_back(std::move(violation));
    }

    report.passed = report.violations.empty();
    return report;
}

void CycleDetector::reset() {
    std::lock_guard lock(mutex_);
    graph_.clear();
    clear_violations();
}

} // namespace memsentry
