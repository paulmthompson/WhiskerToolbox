# Ad-hoc debug/diagnostic executables
# These are NOT benchmarks — they are throwaway tools for debugging specific issues.
# They link directly against libtorch and project libraries as needed.
#
# Can be built standalone:
#   mkdir -p /tmp/adhoc_build && cd /tmp/adhoc_build
#   cmake /path/to/benchmark/adhoc -DCMAKE_PREFIX_PATH=/path/to/libtorch
#   cmake --build .
#
# Or as part of the benchmark suite (ENABLE_BENCHMARK=ON).

cmake_minimum_required(VERSION 3.20)

# Only declare project when built standalone (not as add_subdirectory)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(adhoc_diagnostics LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

find_package(Torch QUIET)
find_package(Armadillo QUIET)

if(Torch_FOUND)
    add_executable(debug_neurosam_gpu debug_neurosam_gpu.cpp)
    target_link_libraries(debug_neurosam_gpu PRIVATE "${TORCH_LIBRARIES}")
    target_include_directories(debug_neurosam_gpu PRIVATE
        "${TORCH_INCLUDE_DIRS}"
    )
    target_compile_options(debug_neurosam_gpu PRIVATE ${TORCH_CXX_FLAGS})
    set_target_properties(debug_neurosam_gpu PROPERTIES
        CXX_STANDARD 23
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/adhoc"
    )

    # TensorCAR GPU vs CPU benchmark
    # Compares Armadillo CPU, LibTorch CPU, and LibTorch CUDA backends for
    # mean-CAR and median-CAR across channel counts typical of neural probes.
    if(TARGET benchmark::benchmark AND ARMADILLO_FOUND)
        add_executable(tensor_car_gpu_cpu tensor_car_gpu_cpu.cpp)
        target_link_libraries(tensor_car_gpu_cpu PRIVATE
            "${TORCH_LIBRARIES}"
            benchmark::benchmark
            armadillo ${ARMADILLO_LIBRARIES}
        )
        target_include_directories(tensor_car_gpu_cpu PRIVATE
            "${TORCH_INCLUDE_DIRS}"
            "${ARMADILLO_INCLUDE_DIRS}"
        )
        target_compile_options(tensor_car_gpu_cpu PRIVATE
            ${TORCH_CXX_FLAGS}
            -O3 -march=native -fno-omit-frame-pointer -g
        )
        set_target_properties(tensor_car_gpu_cpu PROPERTIES
            CXX_STANDARD 23
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/adhoc"
        )
        message(STATUS "[adhoc] tensor_car_gpu_cpu: enabled (Torch + Google Benchmark + Armadillo)")
    else()
        message(STATUS "[adhoc] tensor_car_gpu_cpu: skipped (requires Google Benchmark + Armadillo)")
    endif()
else()
    message(STATUS "[adhoc] Torch not found — skipping GPU/libtorch targets")
endif()

# Memory profiling tool for the TensorCAR pipeline.
# Generates synthetic int16 data, loads as tensor-backed TensorData,
# runs TensorCAR, creates AnalogTimeSeries views. Profile with heaptrack.
if(TARGET DataManager AND TARGET TransformsV2)
    add_executable(tensor_car_memory_profile tensor_car_memory_profile.cpp)
    target_link_libraries(tensor_car_memory_profile PRIVATE
        DataManager
        DataManagerIO
        TransformsV2
        TensorData
        AnalogTimeSeries
        TimeFrame
        CoreUtilities
    )
    target_compile_options(tensor_car_memory_profile PRIVATE
        -O2 -g -fno-omit-frame-pointer
    )
    set_target_properties(tensor_car_memory_profile PROPERTIES
        CXX_STANDARD 23
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/adhoc"
    )
    message(STATUS "[adhoc] tensor_car_memory_profile: enabled")
else()
    message(STATUS "[adhoc] tensor_car_memory_profile: skipped (requires DataManager + TransformsV2)")
endif()

# Memory profiling tool for TensorWhitening.
# Generates synthetic float32 TensorData, runs CPU/GPU whitening, and is meant
# to be inspected with heaptrack to understand allocation behavior.
if(TARGET TensorData AND TARGET TimeFrame AND TARGET TransformsV2)
    add_executable(tensor_whitening_memory_profile tensor_whitening_memory_profile.cpp)
    target_link_libraries(tensor_whitening_memory_profile PRIVATE
        TensorData
        TimeFrame
        TransformsV2
        CoreUtilities
    )
    if(TENSOR_BACKEND_LIBTORCH)
        target_compile_definitions(tensor_whitening_memory_profile PRIVATE TENSOR_BACKEND_LIBTORCH)
    endif()
    target_compile_options(tensor_whitening_memory_profile PRIVATE
        -O2 -g -fno-omit-frame-pointer
    )
    set_target_properties(tensor_whitening_memory_profile PROPERTIES
        CXX_STANDARD 23
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/adhoc"
    )
    message(STATUS "[adhoc] tensor_whitening_memory_profile: enabled")
else()
    message(STATUS "[adhoc] tensor_whitening_memory_profile: skipped (requires TensorData + TimeFrame + TransformsV2)")
endif()

add_subdirectory(rendering)

# Timing / profiling driver for remove_small_connected_components (256×256 dense mask).
# Uses FetchContent GoogleTest for a lightweight `main` and filterable cases.
include(FetchContent)
if(NOT TARGET GTest::gtest_main)
    FetchContent_Declare(
        googletest_mask_cc_adhoc
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.16.0
    )
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest_mask_cc_adhoc)
endif()

if(TARGET DataManager AND TARGET MaskData AND TARGET GTest::gtest_main)
    add_executable(mask_connected_component_timing mask_connected_component_timing.cpp)
    target_include_directories(mask_connected_component_timing PRIVATE
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DataManager/transforms/Masks/Mask_Connected_Component>")
    target_link_libraries(mask_connected_component_timing PRIVATE
        DataManager
        MaskData
        GTest::gtest_main
    )
    target_compile_options(mask_connected_component_timing PRIVATE
        -O2
        -g
        -fno-omit-frame-pointer
    )
    set_target_properties(mask_connected_component_timing PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
        CXX_STANDARD 23
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/adhoc"
    )
    message(STATUS "[adhoc] mask_connected_component_timing: enabled (DataManager + GTest)")
else()
    message(STATUS "[adhoc] mask_connected_component_timing: skipped (needs DataManager, MaskData, GTest)")
endif()
