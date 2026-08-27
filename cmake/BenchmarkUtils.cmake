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
        NAME ScatterPlot
        SOURCES ScatterPlot.benchmark.cpp
        LINK_LIBRARIES DataManager
        DEFAULT ON
    )

This creates:
- CMake option: BENCHMARK_SCATTERPLOT (default: ON)
- Executable: benchmark_ScatterPlot (if enabled)
- All necessary linkage and configuration

Performance Analysis Integration:
    # Run with perf
    perf record -g ./benchmark_ScatterPlot
    perf report
    
    # Run with heaptrack
    heaptrack ./benchmark_ScatterPlot
    heaptrack_gui heaptrack.benchmark_ScatterPlot.*.gz
    
    # View assembly
    objdump -d -C -S ./benchmark_ScatterPlot | less
]]

include_guard(GLOBAL)

define_property(GLOBAL PROPERTY NEURALYZER_BENCHMARK_TARGETS
    BRIEF_DOCS "Benchmark executable targets"
    FULL_DOCS "Executable targets created by add_selective_benchmark for the local benchmark runner"
)

define_property(GLOBAL PROPERTY NEURALYZER_GOOGLE_BENCHMARK_TARGETS
    BRIEF_DOCS "Google Benchmark executable targets"
    FULL_DOCS "Targets registered for run_benchmarks JSON output (excludes STRESS_ONLY executables)"
)

define_property(GLOBAL PROPERTY NEURALYZER_HEAPTRACK_PROBES
    BRIEF_DOCS "Heaptrack probe registrations"
    FULL_DOCS "Semicolon-separated entries: probe_name|executable_target|arg1 arg2 ..."
)

#[[
add_selective_benchmark
-----------------------

Creates a benchmark executable with optional compilation controlled by a CMake option.

Parameters:
  NAME              - Base name for the benchmark (e.g., "ScatterPlot")
  SOURCES           - List of source files for the benchmark
  LINK_LIBRARIES    - List of libraries to link against
  INCLUDE_DIRS      - (Optional) Additional include directories
  DEFAULT           - (Optional) Whether to build by default (ON/OFF, default: ON)
  COMPILE_OPTIONS   - (Optional) Additional compiler flags

Generated Artifacts:
  - CMake Option: BENCHMARK_<UPPER_NAME> (e.g., BENCHMARK_SCATTERPLOT)
  - Executable: benchmark_<Name> (e.g., benchmark_ScatterPlot)
  - Registration with the local benchmark suite target

Example:
  add_selective_benchmark(
      NAME ScatterPlot
      SOURCES 
          ScatterPlot.benchmark.cpp
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
        "STRESS_ONLY;SKIP_HEAPTRACK_REGRESSION" # Options (boolean flags)
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
        if(BENCH_STRESS_ONLY)
            if(BENCH_LINK_LIBRARIES)
                target_link_libraries(${target_name} PRIVATE ${BENCH_LINK_LIBRARIES})
            endif()
        elseif(BENCH_LINK_LIBRARIES)
            target_link_libraries(${target_name} PRIVATE
                ${BENCH_LINK_LIBRARIES}
                benchmark::benchmark
            )
        else()
            target_link_libraries(${target_name} PRIVATE benchmark::benchmark)
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

        # Set output directory
        set_target_properties(${target_name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/benchmark"
        )

        if(BENCH_SKIP_HEAPTRACK_REGRESSION)
            set_target_properties(${target_name} PROPERTIES BENCHMARK_SKIP_HEAPTRACK_REGRESSION TRUE)
        endif()

        set_property(GLOBAL APPEND PROPERTY NEURALYZER_BENCHMARK_TARGETS ${target_name})
        if(NOT BENCH_STRESS_ONLY)
            set_property(GLOBAL APPEND PROPERTY NEURALYZER_GOOGLE_BENCHMARK_TARGETS ${target_name})
        endif()

        message(STATUS "Benchmark enabled: ${BENCH_NAME} (${target_name})")
    else()
        message(STATUS "Benchmark disabled: ${BENCH_NAME}")
    endif()
endfunction()

#[[
register_benchmark_heaptrack_probe
----------------------------------

Registers a named heaptrack probe that runs an existing benchmark executable with
fixed CLI arguments. Used when one stress binary supports multiple probe scenarios.

Parameters:
  NAME       - Baseline file stem (e.g., benchmark_DataViewerView_1ch_init)
  EXECUTABLE - CMake target to run (e.g., benchmark_DataViewerView)
  ARGS       - Command-line arguments passed to the executable
]]
function(register_benchmark_heaptrack_probe)
    cmake_parse_arguments(
        PROBE
        ""
        "NAME;EXECUTABLE"
        "ARGS"
        ${ARGN}
    )

    if(NOT PROBE_NAME)
        message(FATAL_ERROR "register_benchmark_heaptrack_probe: NAME is required")
    endif()
    if(NOT PROBE_EXECUTABLE)
        message(FATAL_ERROR "register_benchmark_heaptrack_probe: EXECUTABLE is required")
    endif()

    string(JOIN " " probe_args ${PROBE_ARGS})
    set_property(GLOBAL APPEND PROPERTY NEURALYZER_HEAPTRACK_PROBES
        "${PROBE_NAME}|${PROBE_EXECUTABLE}|${probe_args}"
    )
endfunction()

#[[
configure_benchmark_for_profiling
----------------------------------

Adds additional configuration to a benchmark target for profiling tools.
Call this after add_selective_benchmark if you need special profiling setup.

Parameters:
  TARGET            - The benchmark target name (e.g., benchmark_ScatterPlot)
  ENABLE_PERF       - (Optional) Add perf-specific flags
  ENABLE_HEAPTRACK  - (Optional) Add heaptrack-specific flags
  GENERATE_ASM      - (Optional) Generate assembly listing files

Example:
  configure_benchmark_for_profiling(
      TARGET benchmark_ScatterPlot
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
add_benchmark_suite_target
--------------------------

Creates a local benchmark runner target that executes all benchmark targets
registered via add_selective_benchmark. Benchmarks are deliberately not CTest
tests, so normal ctest invocations run correctness tests only.

Parameters:
  NAME        - (Optional) Custom target name (default: run_benchmarks)
  OUTPUT_DIR  - (Optional) Output directory for Google Benchmark JSON files
]]
function(add_benchmark_suite_target)
    cmake_parse_arguments(
        SUITE
        ""
        "NAME;OUTPUT_DIR"
        ""
        ${ARGN}
    )

    if(NOT SUITE_NAME)
        set(SUITE_NAME run_benchmarks)
    endif()

    if(NOT SUITE_OUTPUT_DIR)
        set(SUITE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/benchmark-results")
    endif()

    get_property(benchmark_targets GLOBAL PROPERTY NEURALYZER_GOOGLE_BENCHMARK_TARGETS)

    if(NOT benchmark_targets)
        add_custom_target(${SUITE_NAME}
            COMMAND ${CMAKE_COMMAND} -E echo "No Google Benchmark targets are enabled."
            VERBATIM
        )
        return()
    endif()

    set(commands
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SUITE_OUTPUT_DIR}"
    )

    foreach(benchmark_target ${benchmark_targets})
        list(APPEND commands
            COMMAND ${CMAKE_COMMAND} -E echo "Running ${benchmark_target}"
            COMMAND $<TARGET_FILE:${benchmark_target}>
                    --benchmark_format=json
                    --benchmark_out=${SUITE_OUTPUT_DIR}/${benchmark_target}.json
                    --benchmark_out_format=json
        )
    endforeach()

    add_custom_target(${SUITE_NAME}
        ${commands}
        COMMENT "Running local benchmark suite"
        USES_TERMINAL
        VERBATIM
    )

    message(STATUS "Benchmark runner target: ${SUITE_NAME}")
    message(STATUS "Benchmark results: ${SUITE_OUTPUT_DIR}")
endfunction()

#[[
add_benchmark_regression_targets
---------------------------------

Creates record_benchmark_baselines and check_benchmark_regressions targets that
run each registered benchmark executable under heaptrack and compare summary
statistics to local baseline files.

Parameters:
  BASELINE_DIR  - (Optional) Directory for saved baselines (default: source/benchmark-baselines-local)
  RESULTS_DIR   - (Optional) Directory for heaptrack artifacts and current run logs
  TOLERANCE     - (Optional) Fractional tolerance for compare script (default: 0.30)
]]
function(add_benchmark_regression_targets)
    cmake_parse_arguments(
        REG
        ""
        "BASELINE_DIR;RESULTS_DIR;TOLERANCE"
        ""
        ${ARGN}
    )

    if(NOT REG_BASELINE_DIR)
        set(REG_BASELINE_DIR "${CMAKE_SOURCE_DIR}/benchmark-baselines-local")
    endif()

    if(NOT REG_RESULTS_DIR)
        set(REG_RESULTS_DIR "${CMAKE_BINARY_DIR}/benchmark-regression-results")
    endif()

    if(NOT REG_TOLERANCE)
        set(REG_TOLERANCE "0.30")
    endif()

    set(HEAPTRACK_EXECUTABLE "" CACHE FILEPATH "Optional local heaptrack executable")

    get_property(benchmark_targets GLOBAL PROPERTY NEURALYZER_BENCHMARK_TARGETS)
    get_property(heaptrack_probes GLOBAL PROPERTY NEURALYZER_HEAPTRACK_PROBES)
    if(NOT heaptrack_probes)
        set(heaptrack_probes "")
    endif()

    set(compare_script "${CMAKE_SOURCE_DIR}/benchmark/tools/compare_heaptrack_summary.py")
    set(heaptrack_runner "${CMAKE_SOURCE_DIR}/benchmark/tools/run_heaptrack_for_regression.sh")

    if(NOT benchmark_targets AND NOT heaptrack_probes)
        add_custom_target(record_benchmark_baselines
            COMMAND ${CMAKE_COMMAND} -E echo "No benchmark targets are enabled."
            VERBATIM
        )
        add_custom_target(check_benchmark_regressions
            COMMAND ${CMAKE_COMMAND} -E echo "No benchmark targets are enabled."
            VERBATIM
        )
        return()
    endif()

    if(NOT HEAPTRACK_EXECUTABLE)
        add_custom_target(record_benchmark_baselines
            COMMAND ${CMAKE_COMMAND} -E echo
                "record_benchmark_baselines: HEAPTRACK_EXECUTABLE is not set. Configure with CMakeUserPresets.json."
            VERBATIM
        )
        add_custom_target(check_benchmark_regressions
            COMMAND ${CMAKE_COMMAND} -E echo
                "check_benchmark_regressions: HEAPTRACK_EXECUTABLE is not set. Configure with CMakeUserPresets.json."
            VERBATIM
        )
        return()
    endif()

    set(record_commands
        COMMAND ${CMAKE_COMMAND} -E make_directory "${REG_BASELINE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${REG_RESULTS_DIR}"
    )
    set(check_commands
        COMMAND ${CMAKE_COMMAND} -E make_directory "${REG_RESULTS_DIR}"
    )

    foreach(benchmark_target ${benchmark_targets})
        get_target_property(skip_regression ${benchmark_target} BENCHMARK_SKIP_HEAPTRACK_REGRESSION)
        if(skip_regression)
            continue()
        endif()

        list(APPEND record_commands
            COMMAND bash -c
                "cd '${REG_RESULTS_DIR}' && '${heaptrack_runner}' '${REG_BASELINE_DIR}/${benchmark_target}.heaptrack.txt' '${HEAPTRACK_EXECUTABLE}' '$<TARGET_FILE:${benchmark_target}>'"
        )
        list(APPEND check_commands
            COMMAND bash -c
                "cd '${REG_RESULTS_DIR}' && '${heaptrack_runner}' '${REG_RESULTS_DIR}/${benchmark_target}.heaptrack.txt' '${HEAPTRACK_EXECUTABLE}' '$<TARGET_FILE:${benchmark_target}>'"
            COMMAND ${Python3_EXECUTABLE} "${compare_script}"
                    "${REG_BASELINE_DIR}/${benchmark_target}.heaptrack.txt"
                    "${REG_RESULTS_DIR}/${benchmark_target}.heaptrack.txt"
                    --tolerance ${REG_TOLERANCE}
        )
    endforeach()

    foreach(probe_entry ${heaptrack_probes})
        string(REPLACE "|" ";" probe_parts "${probe_entry}")
        list(LENGTH probe_parts probe_part_count)
        if(probe_part_count LESS 3)
            message(WARNING "Skipping malformed heaptrack probe entry: ${probe_entry}")
            continue()
        endif()

        list(GET probe_parts 0 probe_name)
        list(GET probe_parts 1 probe_executable)
        list(SUBLIST probe_parts 2 -1 probe_args)

        string(JOIN " " probe_args_quoted ${probe_args})
        list(APPEND record_commands
            COMMAND bash -c
                "cd '${REG_RESULTS_DIR}' && '${heaptrack_runner}' '${REG_BASELINE_DIR}/${probe_name}.heaptrack.txt' '${HEAPTRACK_EXECUTABLE}' '$<TARGET_FILE:${probe_executable}>' ${probe_args_quoted}"
        )
        list(APPEND check_commands
            COMMAND bash -c
                "cd '${REG_RESULTS_DIR}' && '${heaptrack_runner}' '${REG_RESULTS_DIR}/${probe_name}.heaptrack.txt' '${HEAPTRACK_EXECUTABLE}' '$<TARGET_FILE:${probe_executable}>' ${probe_args_quoted}"
            COMMAND ${Python3_EXECUTABLE} "${compare_script}"
                    "${REG_BASELINE_DIR}/${probe_name}.heaptrack.txt"
                    "${REG_RESULTS_DIR}/${probe_name}.heaptrack.txt"
                    --tolerance ${REG_TOLERANCE}
        )
    endforeach()

    add_custom_target(record_benchmark_baselines
        ${record_commands}
        DEPENDS ${benchmark_targets}
        COMMENT "Recording local benchmark heaptrack baselines"
        USES_TERMINAL
        VERBATIM
    )

    add_custom_target(check_benchmark_regressions
        ${check_commands}
        DEPENDS ${benchmark_targets}
        COMMENT "Checking benchmark heaptrack regressions against local baselines"
        USES_TERMINAL
        VERBATIM
    )

    message(STATUS "Benchmark baseline directory: ${REG_BASELINE_DIR}")
    message(STATUS "Benchmark regression results: ${REG_RESULTS_DIR}")
    message(STATUS "Targets: record_benchmark_baselines, check_benchmark_regressions")
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
