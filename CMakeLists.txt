cmake_minimum_required(VERSION 3.20)
project(MemSentry
    VERSION 2.0.0
    DESCRIPTION "Modern C++20 Memory Diagnostics CLI Tool"
    LANGUAGES CXX
)

# ─── C++20 Standard ───────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─── CMake modules ────────────────────────────────────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Platform)

# ─── MemSentry Core Static Library ────────────────────────────────────────────
# This is the engine: allocation tracker, detectors, reporting, threading.
# It gets linked into the CLI tool AND compiled alongside user code at runtime.
file(GLOB_RECURSE MEMSENTRY_CORE_SOURCES
    "src/core/*.cpp"
    "src/detectors/*.cpp"
    "src/capture/*.cpp"
    "src/reporting/*.cpp"
    "src/threading/*.cpp"
    "src/platform/*.cpp"
)

add_library(memsentry_core STATIC ${MEMSENTRY_CORE_SOURCES})

target_include_directories(memsentry_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

memsentry_set_warnings(memsentry_core)

if(MEMSENTRY_PLATFORM_WINDOWS)
    target_link_libraries(memsentry_core PRIVATE dbghelp)
elseif(MEMSENTRY_PLATFORM_LINUX)
    target_link_libraries(memsentry_core PRIVATE dl)
    find_library(LIBUNWIND unwind)
    if(LIBUNWIND)
        target_link_libraries(memsentry_core PRIVATE ${LIBUNWIND})
        target_compile_definitions(memsentry_core PRIVATE MEMSENTRY_HAS_LIBUNWIND)
    endif()
endif()

find_package(Threads REQUIRED)
target_link_libraries(memsentry_core PUBLIC Threads::Threads)

# ─── MemSentry CLI Executable ─────────────────────────────────────────────────
# The standalone tool that takes external source code and analyzes it.
set(MEMSENTRY_CLI_SOURCES
    src/cli/main.cpp
    src/cli/cli_options.cpp
    src/cli/compiler_driver.cpp
    src/cli/runner.cpp
)

add_executable(memsentry ${MEMSENTRY_CLI_SOURCES})

target_include_directories(memsentry PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(memsentry PRIVATE memsentry_core)

memsentry_set_warnings(memsentry)

if(MEMSENTRY_PLATFORM_WINDOWS)
    target_link_libraries(memsentry PRIVATE dbghelp)
endif()

# ─── Install ──────────────────────────────────────────────────────────────────
install(TARGETS memsentry          RUNTIME DESTINATION bin)
install(TARGETS memsentry_core     ARCHIVE DESTINATION lib)
install(DIRECTORY include/memsentry DESTINATION include)
install(DIRECTORY src/core src/detectors src/capture src/reporting src/threading src/platform
        DESTINATION src
        FILES_MATCHING PATTERN "*.cpp")

# ─── Summary ──────────────────────────────────────────────────────────────────
message(STATUS "")
message(STATUS "╔══════════════════════════════════════════╗")
message(STATUS "║        MemSentry v${PROJECT_VERSION}               ║")
message(STATUS "╠══════════════════════════════════════════╣")
message(STATUS "║  C++ Standard  : C++${CMAKE_CXX_STANDARD}                  ║")
message(STATUS "║  Platform      : ${CMAKE_SYSTEM_NAME}                ║")
message(STATUS "║  Build Type    : CLI Tool (Executable)   ║")
message(STATUS "╚══════════════════════════════════════════╝")
message(STATUS "")
