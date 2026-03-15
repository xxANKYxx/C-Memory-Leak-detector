# MemSentry Architecture

## Table of Contents

1. [System Overview](#system-overview)
2. [Component Details](#component-details)
3. [Memory Layout & Shadow Memory](#memory-layout--shadow-memory)
4. [Detector Pipeline](#detector-pipeline)
5. [Thread Safety Model](#thread-safety-model)
6. [Allocator Hook Mechanism](#allocator-hook-mechanism)
7. [Reporting System](#reporting-system)
8. [Platform Abstraction](#platform-abstraction)

---

## System Overview

MemSentry operates by intercepting global `operator new` and `operator delete` at link time.
Every allocation passes through:

```
User code → operator new → AllocatorHooks → AllocationTracker → Detectors
                                          → ShadowMemory
                                          → StacktraceCapture

User code → operator delete → AllocatorHooks → AllocationTracker → Detectors
                                             → ShadowMemory (state update)
                                             → Quarantine (delayed reuse)
```

### Data Flow Diagram

```
┌─────────┐     ┌──────────────┐     ┌──────────────────┐
│ User    ├────►│ Allocator    ├────►│ Allocation       │
│ Code    │     │ Hooks (10    │     │ Tracker          │
│         │◄────┤ overloads)   │     │ (singleton)      │
└─────────┘     └──┬───────────┘     └──┬───────────────┘
                   │                    │
                   ▼                    ▼
            ┌──────────────┐     ┌──────────────────┐
            │ Shadow       │     │ Detector         │
            │ Memory       │     │ Pipeline         │
            │ (2-bit/byte) │     │ <Leak, DblFree,  │
            └──────────────┘     │  UAF, Overflow,  │
                                 │  Mismatch, ...>  │
                                 └──┬───────────────┘
                                    │
                                    ▼
                              ┌──────────────────┐
                              │ Report Engine    │
                              │ Strategy+Observer│
                              └──────────────────┘
```

---

## Component Details

### AllocationTracker (Singleton)

**Purpose**: Central registry of all live and recently-freed allocations.

```
┌─────────────────────────────────────────────┐
│ AllocationTracker                            │
├─────────────────────────────────────────────┤
│ - allocations_: unordered_map<uintptr_t,    │
│     AllocationRecord>                        │
│ - mutex_: shared_mutex                       │
│ - quarantine_: deque<AllocationRecord>       │
│ - observers_: vector<function<void(Event)>>  │
├─────────────────────────────────────────────┤
│ + register_allocation(addr, record)          │
│ + register_deallocation(addr, type)          │
│   → returns variant<Success, DblFree,        │
│        Mismatch, InvalidFree>                │
│ + get_leaked_allocations() → vector<Record>  │
│   [uses std::ranges pipeline]                │
│ + active_allocation_count() → size_t         │
└─────────────────────────────────────────────┘
```

**State Machine** for each allocation:

```
               register_allocation
  (none) ─────────────────────────► Allocated
                                       │
                  register_deallocation │
                                       ▼
                                     Freed ──► DoubleFree (2nd free)
                                       │
                                       └──► InvalidFree (wild pointer)
```

### ShadowMemory (Singleton)

**Purpose**: Track per-byte state of heap memory using a compact bitmap.

Each heap byte maps to a 2-bit shadow value:

| Value | Bits | Meaning          |
|-------|------|------------------|
| 0     | 00   | Uninitialized    |
| 1     | 01   | Initialized      |
| 2     | 10   | Freed            |
| 3     | 11   | Redzone          |

**Memory overhead**: 25% of tracked heap (2 bits per 8-bit byte).

```
Heap:   [RRRR DDDDDDDDDDDDDDDDD RRRR]
Shadow: [3333 000000000000000000 3333]  ← after allocation
         ^                        ^
         redzone                  redzone
```

---

## Detector Pipeline

All 9 detectors satisfy the `MemoryDetector` C++20 concept:

```cpp
template <typename T>
concept MemoryDetector = requires(T t, ...) {
    { T::detector_name() }   -> std::convertible_to<std::string>;
    { t.on_allocate(evt) }   -> std::same_as<void>;
    { t.on_deallocate(evt) } -> std::same_as<void>;
    { t.on_access(evt) }     -> std::same_as<void>;
    { t.generate_report() }  -> std::same_as<DetectorReport>;
    { t.is_enabled() }       -> std::same_as<bool>;
    { t.reset() }            -> std::same_as<void>;
};
```

### DetectorPipeline (Policy-Based Design)

```cpp
template <MemoryDetector... Detectors>
class DetectorPipeline {
    std::tuple<Detectors...> detectors_;

    void notify_allocate(const AllocationEvent& evt) {
        // Fold expression dispatches to ALL detectors
        std::apply([&](auto&... d) {
            (d.on_allocate(evt), ...);
        }, detectors_);
    }
};
```

### Detector Responsibilities

```
┌─────────────────┬────────────────────────────────────┐
│ LeakDetector     │ Queries Tracker for Allocated state │
│                  │ at report time                      │
├─────────────────┼────────────────────────────────────┤
│ DoubleFreeDetect │ Tracks free history per address     │
├─────────────────┼────────────────────────────────────┤
│ UseAfterFree     │ Checks shadow memory for Freed      │
│                  │ state on access                     │
├─────────────────┼────────────────────────────────────┤
│ OverflowDetector │ Verifies redzone sentinel 0xFD      │
│                  │ integrity on deallocation           │
├─────────────────┼────────────────────────────────────┤
│ MismatchDetector │ Compares alloc_type vs dealloc_type │
│                  │ (new↔delete, new[]↔delete[])        │
├─────────────────┼────────────────────────────────────┤
│ UninitDetector   │ Checks shadow state = Uninitialized │
│                  │ on read access                      │
├─────────────────┼────────────────────────────────────┤
│ InvalidFree      │ Checks if address exists in Tracker │
├─────────────────┼────────────────────────────────────┤
│ CycleDetector    │ Builds reference graph, runs        │
│                  │ Tarjan's SCC, reports cycles > 1    │
├─────────────────┼────────────────────────────────────┤
│ Fragmentation    │ Histogram of size classes,          │
│  Analyzer        │ fragmentation index calculation     │
└─────────────────┴────────────────────────────────────┘
```

---

## Thread Safety Model

### Concurrency Architecture

```
Thread 1 ────► operator new ──┐
Thread 2 ────► operator new ──┼──► AllocationTracker (shared_mutex)
Thread 3 ────► operator delete┘          │
                                         ▼
                               BackgroundScanner (jthread)
                               polls every 100ms via stop_token
```

### Synchronization Primitives

| Component        | Primitive                  | Pattern              |
|------------------|----------------------------|----------------------|
| AllocationTracker| `std::shared_mutex`        | Reader-writer lock   |
| ThreadSafeMap    | `std::shared_mutex`        | Reader-writer lock   |
| LockFreeQueue    | `std::atomic` + CAS        | Lock-free MPSC       |
| BackgroundScanner| `std::jthread`/`stop_token`| Cooperative cancel   |
| AllocatorHooks   | `thread_local` bool        | Reentrance guard     |

### Lock-Free Queue Design

```
┌───┬───┬───┬───┬───┬───┬───┬───┐
│ 0 │ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │ 7 │  Ring buffer (power-of-2)
└───┴───┴───┴───┴───┴───┴───┴───┘
  ▲                   ▲
  │                   │
 tail_ (consumer)   head_ (producers, CAS)

try_push: CAS head_ forward, write data
try_pop:  read tail_, advance if data available
```

---

## Allocator Hook Mechanism

### Memory Layout per Allocation

```
┌────────────┬──────────────────────┬────────────────┐
│  Redzone   │     User Memory      │    Redzone     │
│  (16 bytes)│    (requested size)  │   (16 bytes)   │
│  0xFDFDFD  │                      │   0xFDFDFD     │
└────────────┴──────────────────────┴────────────────┘
▲            ▲                      ▲
│            │                      │
raw_ptr      user_ptr (returned)    checked on free
```

### Sentinel Values

| Value | Purpose                          |
|-------|----------------------------------|
| 0xCD  | Uninitialized heap memory fill   |
| 0xDD  | Freed memory fill                |
| 0xFD  | Redzone guard bytes              |

### Reentrance Protection

```cpp
// Thread-local guard prevents infinite recursion:
// our tracking code itself calls new internally
thread_local bool g_in_hook = false;

struct ReentranceGuard {
    ReentranceGuard()  { g_in_hook = true;  }
    ~ReentranceGuard() { g_in_hook = false; }
};

void* operator new(size_t size) {
    if (g_in_hook) return std::malloc(size);  // fallback
    ReentranceGuard guard;
    return tracked_allocate(size, AllocationType::New);
}
```

---

## Reporting System

### Strategy + Observer Pattern

```
┌──────────────────────────────────────────┐
│ ReportEngine                              │
│                                           │
│ reporters_: vector<unique_ptr<IReporter>> │
│ sinks_:     vector<function<void(str)>>   │
│                                           │
│ generate_report(FullReport)               │
│   ├── for each IReporter: format(report)  │
│   └── for each OutputSink: send(line)     │
└──────────────────────────────────────────┘
        │
        ├── ConsoleReporter  → colored text
        ├── JsonReporter     → structured JSON
        └── HtmlReporter     → styled dark-theme HTML
```

---

## Platform Abstraction

### Factory Pattern

```cpp
std::unique_ptr<Platform> Platform::create() {
    #ifdef MEMSENTRY_PLATFORM_WINDOWS
        return std::make_unique<WindowsPlatform>();
    #else
        return std::make_unique<LinuxPlatform>();
    #endif
}
```

### Stack Trace Capture

| Platform | API                        | Demangling            |
|----------|----------------------------|-----------------------|
| Windows  | `CaptureStackBackTrace()`  | `SymFromAddr()`       |
|          | `SymGetLineFromAddr64()`   | DbgHelp               |
| Linux    | `backtrace()`              | `abi::__cxa_demangle()`|
|          | `backtrace_symbols()`      | cxxabi.h              |

---

## Tarjan's SCC Algorithm (CycleDetector)

Used to find reference cycles among heap objects.

```
Algorithm: Tarjan's Strongly Connected Components
Complexity: O(V + E)

1. For each unvisited node v:
   a. Set v.index = v.lowlink = counter++
   b. Push v onto stack
   c. For each edge v → w:
      - If w not visited: recurse, then v.lowlink = min(v.lowlink, w.lowlink)
      - If w on stack:    v.lowlink = min(v.lowlink, w.index)
   d. If v.lowlink == v.index:
      - Pop stack until v → this is one SCC
      - If SCC size > 1 → report as reference cycle
```

---

## Testing Strategy

| Test Category    | File                       | Coverage |
|------------------|----------------------------|----------|
| Core tracking    | test_allocation_tracker.cpp | 9 tests  |
| Leak detection   | test_leak_detector.cpp      | 4 tests  |
| Double free      | test_double_free.cpp        | 4 tests  |
| Use after free   | test_use_after_free.cpp     | 4 tests  |
| Buffer overflow  | test_overflow.cpp           | 4 tests  |
| Type mismatch    | test_mismatch.cpp           | 5 tests  |
| Uninit access    | test_uninit.cpp             | 4 tests  |
| Cycle detection  | test_cycle_detector.cpp     | 7 tests  |
| Fragmentation    | test_fragmentation.cpp      | 6 tests  |
| Thread safety    | test_thread_safety.cpp      | 5 tests  |
| Report engine    | test_report_engine.cpp      | 7 tests  |
| Integration      | test_integration.cpp        | 8 tests  |
| **Total**        |                             | **60+**  |
