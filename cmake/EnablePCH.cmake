#[[
EnablePCH.cmake — Precompiled header support for WhiskerToolbox

Provides four functions:

  Per-target PCH (default — each target compiles its own PCH):
    enable_whiskertoolbox_pch(<target>)
    enable_whiskertoolbox_test_pch(<target>)

  Shared PCH (opt-in — many targets reuse a single compiled PCH):
    enable_whiskertoolbox_shared_pch(<target>)
    enable_whiskertoolbox_shared_test_pch(<target>)

The shared variants use CMake's REUSE_FROM mechanism: the PCH is compiled
once on a small host target, and every consumer target reuses that binary.
This dramatically reduces build times when many static libraries all include
the same standard-library / Catch2 headers.

Requirements for REUSE_FROM (CMake ≥ 3.16):
  • Same CXX_STANDARD (all targets use C++23 here).
  • Compatible compiler options — warning flags are fine; ABI-changing flags
    are not.  Targets that add non-standard compile options or definitions
    that affect standard-library parsing should stick to per-target PCH.
  • Include directories do NOT need to match.  Only the host target needs
    the include paths required to compile the PCH header (standard library
    headers are found automatically).

To disable all PCH:          cmake -DDISABLE_PCH=ON ...
To force per-target PCH only: cmake -DDISABLE_SHARED_PCH=ON ...
]]

option(DISABLE_PCH        "Disable precompiled headers entirely"       OFF)
option(DISABLE_SHARED_PCH "Force per-target PCH (disable REUSE_FROM)" OFF)

# ── Internal: guard checks shared by all four public functions ────────
macro(_whiskertoolbox_pch_guard target result_var)
    set(${result_var} FALSE)
    if(DISABLE_PCH)
        set(${result_var} TRUE)
    elseif(NOT TARGET ${target})
        message(WARNING "PCH: target '${target}' does not exist, skipping")
        set(${result_var} TRUE)
    elseif(WIN32)
        get_target_property(_pch_tt ${target} TYPE)
        if(_pch_tt STREQUAL "SHARED_LIBRARY")
            set(${result_var} TRUE)
        endif()
    endif()
endmacro()

# ── Internal: create the main PCH host (called once, lazily) ─────────
macro(_whiskertoolbox_ensure_pch_host)
    if(NOT TARGET _whiskertoolbox_pch_host)
        set(_pch_host_src "${CMAKE_BINARY_DIR}/_pch_host.cpp")
        file(WRITE "${_pch_host_src}" "// PCH host translation unit — do not edit\n")

        add_library(_whiskertoolbox_pch_host STATIC "${_pch_host_src}")

        if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            target_compile_options(_whiskertoolbox_pch_host PRIVATE ${CLANG_OPTIONS})
        elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(_whiskertoolbox_pch_host PRIVATE ${GCC_WARNINGS})
        elseif(MSVC)
            target_compile_options(_whiskertoolbox_pch_host PRIVATE ${MSVC_WARNINGS})
        endif()

        target_precompile_headers(_whiskertoolbox_pch_host PRIVATE
            ${CMAKE_SOURCE_DIR}/src/whiskertoolbox_pch.hpp
        )
    endif()
endmacro()

# ── Internal: create the test PCH host (called once, lazily) ─────────
#  Deferred because Catch2 is fetched in tests/CMakeLists.txt, which runs
#  after this file is include()'d from the root.
macro(_whiskertoolbox_ensure_test_pch_host)
    if(NOT TARGET _whiskertoolbox_test_pch_host)
        set(_test_pch_host_src "${CMAKE_BINARY_DIR}/_pch_test_host.cpp")
        file(WRITE "${_test_pch_host_src}" "// Test PCH host translation unit — do not edit\n")

        add_library(_whiskertoolbox_test_pch_host STATIC "${_test_pch_host_src}")

        # Catch2 must already be available (FetchContent_MakeAvailable in tests/)
        target_link_libraries(_whiskertoolbox_test_pch_host PRIVATE Catch2::Catch2)

        if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            target_compile_options(_whiskertoolbox_test_pch_host PRIVATE ${CLANG_OPTIONS})
        elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(_whiskertoolbox_test_pch_host PRIVATE ${GCC_WARNINGS})
        elseif(MSVC)
            target_compile_options(_whiskertoolbox_test_pch_host PRIVATE ${MSVC_WARNINGS})
        endif()

        target_precompile_headers(_whiskertoolbox_test_pch_host PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/whiskertoolbox_test_pch.hpp
        )
    endif()
endmacro()

# ══════════════════════════════════════════════════════════════════════
# Public API
# ══════════════════════════════════════════════════════════════════════

#[[
enable_whiskertoolbox_pch(<target>)

Attach a per-target PCH.  Each target compiles its own copy.
Use this for targets with non-standard compile options.
]]
function(enable_whiskertoolbox_pch target)
    _whiskertoolbox_pch_guard(${target} _skip)
    if(_skip)
        return()
    endif()

    target_precompile_headers(${target}
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src/whiskertoolbox_pch.hpp
    )
endfunction()

#[[
enable_whiskertoolbox_test_pch(<target>)

Per-target test PCH (includes Catch2 headers).
Use this for test targets with non-standard compile options.
]]
function(enable_whiskertoolbox_test_pch target)
    _whiskertoolbox_pch_guard(${target} _skip)
    if(_skip)
        return()
    endif()

    target_precompile_headers(${target}
        PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/whiskertoolbox_test_pch.hpp
    )
endfunction()

#[[
enable_whiskertoolbox_shared_pch(<target>)

Reuse a single compiled PCH across many targets.  Falls back to per-target
PCH if DISABLE_SHARED_PCH is ON.

Only use this for targets whose compile options are compatible with the
standard project warnings (CLANG_OPTIONS / GCC_WARNINGS / MSVC_WARNINGS).
Targets that add extra flags affecting ABI or standard-library parsing
should use enable_whiskertoolbox_pch() instead.
]]
function(enable_whiskertoolbox_shared_pch target)
    _whiskertoolbox_pch_guard(${target} _skip)
    if(_skip)
        return()
    endif()

    if(DISABLE_SHARED_PCH)
        # Fall back to per-target PCH
        target_precompile_headers(${target}
            PRIVATE
                ${CMAKE_SOURCE_DIR}/src/whiskertoolbox_pch.hpp
        )
        return()
    endif()

    _whiskertoolbox_ensure_pch_host()
    target_precompile_headers(${target} REUSE_FROM _whiskertoolbox_pch_host)
endfunction()

#[[
enable_whiskertoolbox_shared_test_pch(<target>)

Shared test PCH (includes Catch2 headers).  Falls back to per-target PCH
if DISABLE_SHARED_PCH is ON.
]]
function(enable_whiskertoolbox_shared_test_pch target)
    _whiskertoolbox_pch_guard(${target} _skip)
    if(_skip)
        return()
    endif()

    if(DISABLE_SHARED_PCH)
        target_precompile_headers(${target}
            PRIVATE
                ${CMAKE_SOURCE_DIR}/tests/whiskertoolbox_test_pch.hpp
        )
        return()
    endif()

    _whiskertoolbox_ensure_test_pch_host()
    target_precompile_headers(${target} REUSE_FROM _whiskertoolbox_test_pch_host)
endfunction()
