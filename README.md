# MemSentry — Modern C++20 Memory Diagnostics CLI Tool

<p align="center">
  <strong>A standalone memory error detection tool — give it your C++ source code, it finds the bugs.</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-20-blue.svg" alt="C++20">
  <img src="https://img.shields.io/badge/build-CMake%203.20+-green.svg" alt="CMake">
  <img src="https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey.svg" alt="Platform">
</p>

---

## What Is MemSentry?

**MemSentry** is a CLI tool that takes your C++ source code, compiles it with built-in memory instrumentation, runs it, and reports **9 categories of memory bugs**:

| #  | Detector               | What It Catches                          |
|----|------------------------|------------------------------------------|
| 1  | **LeakDetector**       | Allocated memory never freed             |
| 2  | **DoubleFreeDetector** | Freeing the same block twice             |
| 3  | **UseAfterFreeDetector** | Reading/writing freed memory           |
| 4  | **OverflowDetector**   | Buffer overflows via redzone corruption  |
| 5  | **MismatchDetector**   | `new`/`delete[]` or `new[]`/`delete` mix |
| 6  | **UninitDetector**     | Reading uninitialized heap memory        |
| 7  | **InvalidFreeDetector**| Freeing non-heap or wild pointers        |
| 8  | **CycleDetector**      | Reference cycles (Tarjan's SCC)          |
| 9  | **FragmentationAnalyzer** | Heap fragmentation patterns           |

---

## Quick Start

### 1. Build MemSentry

```bash
cd MemSentry
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

### 2. Analyze Your Code

```bash
# Analyze a single file
.\build\Release\memsentry.exe  my_app.cpp

# Analyze a whole folder
.\build\Release\memsentry.exe  --dir  src/

# Get JSON report
.\build\Release\memsentry.exe  my_app.cpp  --format json  --output report.json

# Minimal mode (leak + double-free + use-after-free only)
.\build\Release\memsentry.exe  my_app.cpp  --minimal
```

### 3. Try the Samples

```bash
.\build\Release\memsentry.exe  samples\leak_demo.cpp
.\build\Release\memsentry.exe  samples\double_free_demo.cpp
.\build\Release\memsentry.exe  samples\overflow_demo.cpp
```

---

## CLI Usage

```
Usage:
  memsentry <file.cpp> [file2.cpp ...] [options]
  memsentry --dir <folder>              [options]

Options:
  --dir <path>       Scan all .cpp files in <path> recursively
  --args <args>      Arguments to pass to your program at runtime
  --format <fmt>     Report format: text (default), json, html
  --output <file>    Write report to file (default: stdout)
  --full             Enable all 9 detectors (default)
  --minimal          Enable only leak + double-free + use-after-free
  --no-run           Compile only, don't run
  --keep-binary      Don't delete the compiled binary after analysis
  --compiler <path>  Path to compiler (auto-detected if omitted)
  --std <std>        C++ standard: c++17, c++20 (default: c++20)
  --include <path>   Extra include path(s), can be repeated
  -v, --verbose      Verbose output
  -h, --help         Show this help
```

---

## How It Works

```
┌────────────────────────────────────────────────────────────────────┐
│  Step 1: You run:  memsentry your_code.cpp                         │
├────────────────────────────────────────────────────────────────────┤
│  Step 2: MemSentry generates an instrumentation wrapper that:      │
│          - Includes your source code                               │
│          - Renames your main() to user_main()                      │
│          - Creates a new main() that enables all MemSentry hooks   │
│          - Calls your user_main() inside a MemSentry scope         │
├────────────────────────────────────────────────────────────────────┤
│  Step 3: Compiles everything (your code + MemSentry engine)        │
│          using your system C++ compiler (cl.exe / g++ / clang++)   │
├────────────────────────────────────────────────────────────────────┤
│  Step 4: Runs the instrumented binary                              │
│          Every new/delete is now tracked by MemSentry              │
├────────────────────────────────────────────────────────────────────┤
│  Step 5: On exit, MemSentry reports all detected memory issues     │
│          with stack traces, allocation sizes, and violation types   │
└────────────────────────────────────────────────────────────────────┘
```

---

## Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                   CLI Layer (src/cli/)                        │
│    CliOptions - CompilerDriver - Runner - main.cpp           │
├──────────────────────────────────────────────────────────────┤
│                      Public API (memsentry.hpp)              │
│               MemSentry :: init / shutdown / report          │
│                    MemSentryGuard  (RAII)                     │
├──────────────────────────────────────────────────────────────┤
│                   Allocator Hooks Layer                       │
│    Global operator new/delete overloads (10 variants)        │
│    Redzone painting - Sentinel fill - Quarantine             │
├──────────────┬───────────────┬───────────────────────────────┤
│  Core Engine │   Detectors   │       Threading               │
│              │               │                               │
│ Allocation   │ Leak          │ ThreadSafeMap<K,V>            │
│  Tracker     │ DoubleFree    │ LockFreeQueue<T,N>            │
│ ShadowMemory │ UseAfterFree  │ BackgroundScanner             │
│  (2-bit/byte)│ Overflow      │  (jthread + stop_token)       │
│              │ Mismatch      │                               │
│              │ Uninit        │                               │
│              │ InvalidFree   │                               │
│              │ Cycle (Tarjan)│                               │
│              │ Fragmentation │                               │
├──────────────┴───────────────┴───────────────────────────────┤
│                     Reporting Engine                          │
│  IReporter (Strategy) - OutputSink (Observer)                │
│  ConsoleReporter - JsonReporter - HtmlReporter               │
├──────────────────────────────────────────────────────────────┤
│                   Platform Abstraction                        │
│  StacktraceCapture - SymbolDemangling - MemoryProtection     │
│  Windows: DbgHelp, CaptureStackBackTrace, VirtualProtect    │
│  Linux:   backtrace, cxxabi, mprotect                        │
└──────────────────────────────────────────────────────────────┘
```

---

## Project Structure

```
MemSentry/
├── CMakeLists.txt              # Build system (builds CLI exe)
├── README.md                   # This file
├── cmake/
│   ├── CompilerWarnings.cmake  # Strict warning flags per compiler
│   └── Platform.cmake          # OS detection + C++20 feature checks
├── include/memsentry/          # Public headers (compiled with user code)
│   ├── memsentry.hpp           # Facade — single include
│   ├── core/                   # AllocationTracker, Config, ShadowMemory
│   ├── detectors/              # All 9 detectors
│   ├── capture/                # Stack trace capture
│   ├── reporting/              # Console/JSON/HTML reporters
│   ├── threading/              # ThreadSafeMap, LockFreeQueue, Scanner
│   └── platform/               # Windows/Linux abstractions
├── src/
│   ├── cli/                    # CLI tool (the executable)
│   │   ├── main.cpp            # Entry point
│   │   ├── cli_options.cpp/hpp # Argument parsing
│   │   ├── compiler_driver.cpp/hpp  # Compiles user code + MemSentry
│   │   └── runner.cpp/hpp      # Runs instrumented binary
│   ├── core/                   # Engine implementation
│   ├── detectors/              # Detector implementations
│   ├── capture/                # Stack trace implementation
│   ├── reporting/              # Reporter implementations
│   ├── threading/              # Thread-safe data structures
│   └── platform/               # Platform-specific code
├── samples/                    # Sample buggy code to test with
│   ├── leak_demo.cpp
│   ├── double_free_demo.cpp
│   └── overflow_demo.cpp
└── docs/
    └── architecture.md
```

---

## Modern C++20 Features Used

| Feature                    | Where Used                                           |
|----------------------------|------------------------------------------------------|
| **Concepts**               | `MemoryDetector` concept constraining all detectors  |
| **Ranges**                 | `get_leaked_allocations()` filter/transform pipeline  |
| **`std::format`**          | All report formatters, violation descriptions        |
| **`std::source_location`** | Automatic capture of allocation call-site            |
| **`std::jthread`**         | Background scanner with cooperative cancellation     |
| **`std::stop_token`**      | Graceful shutdown of background threads              |
| **`std::expected`**        | Error handling in CLI (C++23)                        |
| **Spaceship operator**     | `AllocationRecord` comparison via `<=>`              |
| **`std::span`**            | Safe buffer views in shadow memory operations        |
| **Fold expressions**       | `DetectorPipeline<Detectors...>` dispatch            |

## Design Patterns

| Pattern          | Implementation                                          |
|------------------|---------------------------------------------------------|
| **Facade**       | `MemSentry` class — single-entry public API             |
| **RAII**         | `MemSentryGuard` — auto init/report/shutdown            |
| **Singleton**    | `AllocationTracker::instance()` (Meyer's singleton)     |
| **Strategy**     | `IReporter` -> Console / JSON / HTML reporters          |
| **Observer**     | `OutputSink` — multiple output subscribers              |
| **CRTP**         | `DetectorBase<Derived>` — static polymorphism           |

---

## Requirements

- **C++20 compiler**: MSVC 2022, GCC 12+, or Clang 15+
- **CMake 3.20+**
- **Windows**: requires `dbghelp.lib` (included with Windows SDK)
- **Linux**: requires `libdl`, optionally `libunwind`


