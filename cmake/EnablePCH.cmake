# EnablePCH.cmake
# 
# CMake module to enable precompiled headers for WhiskerToolbox
# This significantly reduces build times by precompiling expensive STL headers
#
# Usage in CMakeLists.txt:
#   include(EnablePCH)
#   enable_whiskertoolbox_pch(target_name)
#
# Based on ClangBuildAnalyzer results showing:
# - <functional>: 99.4s total (332 includes)
# - <vector>: 78.0s total (361 includes)
# - <chrono>: 77.4s total (231 includes)
#
# Expected savings: 40-50 seconds (15-20% of total build time)

function(enable_whiskertoolbox_pch TARGET_NAME)
    # Only enable PCH for CMake 3.16+
    if(${CMAKE_VERSION} VERSION_LESS "3.16.0")
        message(WARNING "Precompiled headers require CMake 3.16+, skipping for ${TARGET_NAME}")
        return()
    endif()
    
    # Check if PCH is explicitly disabled
    if(DISABLE_PCH)
        message(STATUS "Precompiled headers disabled for ${TARGET_NAME}")
        return()
    endif()

    message(STATUS "Enabling precompiled headers for ${TARGET_NAME}")
    
    # Use the PCH file we created
    target_precompile_headers(${TARGET_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/src/whiskertoolbox_pch.hpp
    )
endfunction()

# Alternative function for projects that want to specify their own PCH list
function(enable_custom_pch TARGET_NAME)
    if(${CMAKE_VERSION} VERSION_LESS "3.16.0")
        message(WARNING "Precompiled headers require CMake 3.16+, skipping for ${TARGET_NAME}")
        return()
    endif()
    
    if(DISABLE_PCH)
        message(STATUS "Precompiled headers disabled for ${TARGET_NAME}")
        return()
    endif()

    message(STATUS "Enabling custom precompiled headers for ${TARGET_NAME}")
    
    # Most expensive STL headers based on build analysis
    target_precompile_headers(${TARGET_NAME} PRIVATE
        <vector>
        <string>
        <memory>
        <functional>
        <optional>
        <variant>
        <chrono>
        <map>
        <unordered_map>
        <algorithm>
    )
endfunction()
