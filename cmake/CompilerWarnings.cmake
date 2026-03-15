# ─── Compiler Warning Configuration ──────────────────────────────────────────
# Applies strict, production-grade warning flags per compiler.

function(memsentry_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4             # High warning level
            /permissive-    # Strict standard conformance
            /wd4996         # Disable deprecated warnings (for platform APIs)
            /Zc:__cplusplus # Report correct __cplusplus value
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wnon-virtual-dtor
            -Wold-style-cast
            -Wcast-align
            -Wunused
            -Woverloaded-virtual
            -Wconversion
            -Wsign-conversion
            -Wnull-dereference
            -Wdouble-promotion
            -Wformat=2
            -Wimplicit-fallthrough
        )
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
            target_compile_options(${target} PRIVATE
                -Wmisleading-indentation
                -Wduplicated-cond
                -Wduplicated-branches
                -Wlogical-op
                -Wuseless-cast
            )
        endif()
    endif()
endfunction()
