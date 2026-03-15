# ─── Platform Detection ──────────────────────────────────────────────────────
# Sets MEMSENTRY_PLATFORM_WINDOWS / MEMSENTRY_PLATFORM_LINUX compile definitions.

if(WIN32)
    set(MEMSENTRY_PLATFORM_WINDOWS TRUE)
    set(MEMSENTRY_PLATFORM_LINUX   FALSE)
    add_compile_definitions(MEMSENTRY_PLATFORM_WINDOWS=1)
    message(STATUS "[MemSentry] Platform: Windows")
elseif(UNIX AND NOT APPLE)
    set(MEMSENTRY_PLATFORM_WINDOWS FALSE)
    set(MEMSENTRY_PLATFORM_LINUX   TRUE)
    add_compile_definitions(MEMSENTRY_PLATFORM_LINUX=1)
    message(STATUS "[MemSentry] Platform: Linux")
else()
    message(WARNING "[MemSentry] Unsupported platform: ${CMAKE_SYSTEM_NAME}")
    set(MEMSENTRY_PLATFORM_WINDOWS FALSE)
    set(MEMSENTRY_PLATFORM_LINUX   FALSE)
endif()

# ─── Feature Detection ───────────────────────────────────────────────────────
include(CheckCXXSourceCompiles)

# Check for std::source_location
check_cxx_source_compiles("
    #include <source_location>
    int main() {
        auto loc = std::source_location::current();
        return 0;
    }
" MEMSENTRY_HAS_SOURCE_LOCATION)

if(MEMSENTRY_HAS_SOURCE_LOCATION)
    add_compile_definitions(MEMSENTRY_HAS_SOURCE_LOCATION=1)
    message(STATUS "[MemSentry] std::source_location: available")
else()
    message(STATUS "[MemSentry] std::source_location: NOT available")
endif()

# Check for std::format
check_cxx_source_compiles("
    #include <format>
    int main() {
        auto s = std::format(\"{}\", 42);
        return 0;
    }
" MEMSENTRY_HAS_FORMAT)

if(MEMSENTRY_HAS_FORMAT)
    add_compile_definitions(MEMSENTRY_HAS_FORMAT=1)
    message(STATUS "[MemSentry] std::format: available")
else()
    message(STATUS "[MemSentry] std::format: NOT available (fallback to snprintf)")
endif()

# Check for std::jthread
check_cxx_source_compiles("
    #include <thread>
    int main() {
        std::jthread t([](){});
        return 0;
    }
" MEMSENTRY_HAS_JTHREAD)

if(MEMSENTRY_HAS_JTHREAD)
    add_compile_definitions(MEMSENTRY_HAS_JTHREAD=1)
    message(STATUS "[MemSentry] std::jthread: available")
else()
    message(STATUS "[MemSentry] std::jthread: NOT available")
endif()
