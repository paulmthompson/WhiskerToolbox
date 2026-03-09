#[[
EnablePCH.cmake — Precompiled header support for WhiskerToolbox

Usage in any CMakeLists.txt that defines a library target:

    enable_whiskertoolbox_pch(<target>)

This attaches the project-wide PCH (src/whiskertoolbox_pch.hpp) to the given
target. CMake compiles the PCH once per unique set of compile options and
reuses it across targets that share the same flags.

To disable PCH globally (e.g. for CI checks that should verify includes are
self-contained), configure with:

    cmake -DDISABLE_PCH=ON ...
]]

option(DISABLE_PCH "Disable precompiled headers" OFF)

function(enable_whiskertoolbox_pch target)
    if(DISABLE_PCH)
        return()
    endif()

    # Only apply to targets that actually exist
    if(NOT TARGET ${target})
        message(WARNING "enable_whiskertoolbox_pch: target '${target}' does not exist, skipping PCH")
        return()
    endif()

    target_precompile_headers(${target}
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src/whiskertoolbox_pch.hpp
    )
endfunction()

#[[
enable_whiskertoolbox_test_pch(<target>)

Same as enable_whiskertoolbox_pch but uses the test PCH which additionally
precompiles Catch2 headers (catch_test_macros, floating-point matchers, etc.).
Use this for test executables.
]]
function(enable_whiskertoolbox_test_pch target)
    if(DISABLE_PCH)
        return()
    endif()

    if(NOT TARGET ${target})
        message(WARNING "enable_whiskertoolbox_test_pch: target '${target}' does not exist, skipping PCH")
        return()
    endif()

    target_precompile_headers(${target}
        PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/whiskertoolbox_test_pch.hpp
    )
endfunction()
