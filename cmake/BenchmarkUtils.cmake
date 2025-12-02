#[[
BenchmarkUtils.cmake
=====================

Provides utilities for creating selective, modular benchmarks with Google Benchmark.

Key Features:
- Create individual benchmark executables per function/module
- Enable/disable benchmarks via CMake options
- Support for performance analysis tools (perf, heaptrack, etc.)
- Assembly inspection for micro-optimization
- Consistent naming and organization

Usage Example:
    # In benchmark/CMakeLists.txt
    add_selective_benchmark(
        NAME MaskArea
        SOURCES MaskArea.benchmark.cpp
        LINK_LIBRARIES DataManager
        DEFAULT ON
    )

This creates:
- CMake option: BENCHMARK_MASK_AREA (default: ON)
- Executable: benchmark_MaskArea (if enabled)
- All necessary linkage and configuration

Performance Analysis Integration:
    # Run with perf
    perf record -g ./benchmark_MaskArea
    perf report
    
    # Run with heaptrack
    heaptrack ./benchmark_MaskArea
    heaptrack_gui heaptrack.benchmark_MaskArea.*.gz
    
    # View assembly
    objdump -d -C -S ./benchmark_MaskArea | less
]]

include_guard(GLOBAL)

#[[
add_selective_benchmark
-----------------------

Creates a benchmark executable with optional compilation controlled by a CMake option.

Parameters:
  NAME              - Base name for the benchmark (e.g., "MaskArea")
  SOURCES           - List of source files for the benchmark
  LINK_LIBRARIES    - List of libraries to link against
  INCLUDE_DIRS      - (Optional) Additional include directories
  DEFAULT           - (Optional) Whether to build by default (ON/OFF, default: ON)
  COMPILE_OPTIONS   - (Optional) Additional compiler flags

Generated Artifacts:
  - CMake Option: BENCHMARK_<UPPER_NAME> (e.g., BENCHMARK_MASK_AREA)
  - Executable: benchmark_<Name> (e.g., benchmark_MaskArea)
  - Test: benchmark_<Name> (registered with CTest)

Example:
  add_selective_benchmark(
      NAME MaskArea
      SOURCES 
          MaskArea.benchmark.cpp
          fixtures/MaskDataFixture.cpp
      LINK_LIBRARIES 
          DataManager
          Catch2::Catch2WithMain
      DEFAULT ON
  )
]]
function(add_selective_benchmark)
    cmake_parse_arguments(
        BENCH                          # Prefix for parsed variables
        ""                             # Options (boolean flags)
        "NAME;DEFAULT"                 # Single-value keywords
        "SOURCES;LINK_LIBRARIES;INCLUDE_DIRS;COMPILE_OPTIONS" # Multi-value keywords
        ${ARGN}                        # Arguments to parse
    )

    # Validate required arguments
    if(NOT BENCH_NAME)
        message(FATAL_ERROR "add_selective_benchmark: NAME is required")
    endif()
    if(NOT BENCH_SOURCES)
        message(FATAL_ERROR "add_selective_benchmark: SOURCES is required")
    endif()

    # Default to ON if not specified
    if(NOT DEFINED BENCH_DEFAULT)
        set(BENCH_DEFAULT ON)
    endif()

    # Create normalized names
    string(TOUPPER "${BENCH_NAME}" BENCH_NAME_UPPER)
    string(REPLACE " " "_" BENCH_NAME_UPPER "${BENCH_NAME_UPPER}")
    string(REPLACE "-" "_" BENCH_NAME_UPPER "${BENCH_NAME_UPPER}")

    # Create the CMake option
    option(BENCHMARK_${BENCH_NAME_UPPER} 
           "Build ${BENCH_NAME} benchmark" 
           ${BENCH_DEFAULT})

    # Only create target if enabled
    if(BENCHMARK_${BENCH_NAME_UPPER})
        set(target_name "benchmark_${BENCH_NAME}")

        # Create the executable
        add_executable(${target_name} ${BENCH_SOURCES})

        # Link libraries
        if(BENCH_LINK_LIBRARIES)
            target_link_libraries(${target_name} PRIVATE 
                ${BENCH_LINK_LIBRARIES}
                benchmark::benchmark
                benchmark::benchmark_main
            )
        else()
            target_link_libraries(${target_name} PRIVATE 
                benchmark::benchmark
                benchmark::benchmark_main
            )
        endif()

        # Add include directories
        if(BENCH_INCLUDE_DIRS)
            target_include_directories(${target_name} PRIVATE ${BENCH_INCLUDE_DIRS})
        endif()

        # Add compile options
        if(BENCH_COMPILE_OPTIONS)
            target_compile_options(${target_name} PRIVATE ${BENCH_COMPILE_OPTIONS})
        endif()

        # Standard benchmark settings
        target_compile_features(${target_name} PRIVATE cxx_std_20)

        # Optimization flags for benchmarks
        if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
            # Enable optimizations but keep debug symbols for profiling
            target_compile_options(${target_name} PRIVATE
                $<$<CXX_COMPILER_ID:GNU,Clang>:-O3 -march=native -fno-omit-frame-pointer>
                $<$<CXX_COMPILER_ID:MSVC>:/O2 /Oy->
            )
        endif()

        # Additional flags for assembly inspection and profiling
        target_compile_options(${target_name} PRIVATE
            # Keep symbols for profiling
            $<$<CXX_COMPILER_ID:GNU,Clang>:-g>
            $<$<CXX_COMPILER_ID:MSVC>:/Zi>
            
            # Disable some optimizations that make debugging harder
            $<$<CXX_COMPILER_ID:GNU,Clang>:-fno-inline-small-functions>
        )

        # Register with CTest for easy execution
        add_test(NAME ${target_name} COMMAND ${target_name})

        # Set output directory
        set_target_properties(${target_name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/benchmark"
        )

        message(STATUS "Benchmark enabled: ${BENCH_NAME} (${target_name})")
    else()
        message(STATUS "Benchmark disabled: ${BENCH_NAME}")
    endif()
endfunction()

#[[
configure_benchmark_for_profiling
----------------------------------

Adds additional configuration to a benchmark target for profiling tools.
Call this after add_selective_benchmark if you need special profiling setup.

Parameters:
  TARGET            - The benchmark target name (e.g., benchmark_MaskArea)
  ENABLE_PERF       - (Optional) Add perf-specific flags
  ENABLE_HEAPTRACK  - (Optional) Add heaptrack-specific flags
  GENERATE_ASM      - (Optional) Generate assembly listing files

Example:
  configure_benchmark_for_profiling(
      TARGET benchmark_MaskArea
      ENABLE_PERF ON
      GENERATE_ASM ON
  )
]]
function(configure_benchmark_for_profiling)
    cmake_parse_arguments(
        PROF
        "ENABLE_PERF;ENABLE_HEAPTRACK;GENERATE_ASM"
        "TARGET"
        ""
        ${ARGN}
    )

    if(NOT PROF_TARGET)
        message(FATAL_ERROR "configure_benchmark_for_profiling: TARGET is required")
    endif()

    if(NOT TARGET ${PROF_TARGET})
        message(FATAL_ERROR "configure_benchmark_for_profiling: ${PROF_TARGET} is not a valid target")
    endif()

    # Perf optimization
    if(PROF_ENABLE_PERF)
        target_compile_options(${PROF_TARGET} PRIVATE
            $<$<CXX_COMPILER_ID:GNU,Clang>:-fno-omit-frame-pointer>
        )
        message(STATUS "  Perf profiling enabled for ${PROF_TARGET}")
    endif()

    # Heaptrack doesn't need special compile flags, but we can add runtime hints
    if(PROF_ENABLE_HEAPTRACK)
        message(STATUS "  Heaptrack profiling enabled for ${PROF_TARGET}")
        message(STATUS "    Run with: heaptrack ${CMAKE_BINARY_DIR}/benchmark/${PROF_TARGET}")
    endif()

    # Generate assembly listings
    if(PROF_GENERATE_ASM)
        target_compile_options(${PROF_TARGET} PRIVATE
            $<$<CXX_COMPILER_ID:GNU,Clang>:-save-temps=obj -fverbose-asm>
        )
        message(STATUS "  Assembly generation enabled for ${PROF_TARGET}")
        message(STATUS "    Assembly files will be in build directory")
    endif()
endfunction()

#[[
print_benchmark_summary
-----------------------

Prints a summary of all configured benchmarks.
Call this at the end of benchmark/CMakeLists.txt.

Example:
  print_benchmark_summary()
]]
function(print_benchmark_summary)
    message(STATUS "")
    message(STATUS "==============================================")
    message(STATUS "Benchmark Configuration Summary")
    message(STATUS "==============================================")
    
    get_cmake_property(_all_vars VARIABLES)
    set(benchmark_count 0)
    set(enabled_benchmarks "")
    
    foreach(_var ${_all_vars})
        if(_var MATCHES "^BENCHMARK_")
            math(EXPR benchmark_count "${benchmark_count} + 1")
            if(${_var})
                string(REPLACE "BENCHMARK_" "" bench_name "${_var}")
                list(APPEND enabled_benchmarks "${bench_name}")
            endif()
        endif()
    endforeach()
    
    if(benchmark_count EQUAL 0)
        message(STATUS "No benchmarks configured")
    else()
        message(STATUS "Total benchmarks: ${benchmark_count}")
        if(enabled_benchmarks)
            message(STATUS "Enabled benchmarks:")
            foreach(bench ${enabled_benchmarks})
                message(STATUS "  - ${bench}")
            endforeach()
        else()
            message(STATUS "No benchmarks enabled")
        endif()
    endif()
    
    message(STATUS "")
    message(STATUS "Profiling Tools:")
    message(STATUS "  perf:      perf record -g ./benchmark_<name>; perf report")
    message(STATUS "  heaptrack: heaptrack ./benchmark_<name>; heaptrack_gui *.gz")
    message(STATUS "  asm:       objdump -d -C -S ./benchmark_<name> | less")
    message(STATUS "  time:      /usr/bin/time -v ./benchmark_<name>")
    message(STATUS "==============================================")
    message(STATUS "")
endfunction()
