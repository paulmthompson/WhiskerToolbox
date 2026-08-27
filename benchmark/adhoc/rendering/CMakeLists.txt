# Ad-hoc rendering pipeline benchmarks
# Measures data read, vertex generation, cache extraction, decimation,
# per-pixel buffering, and multi-channel scaling performance.

find_package(Boost REQUIRED)
find_package(glm CONFIG REQUIRED)

# Common rendering benchmark include directory (for SyntheticDataFactory.hpp, etc.)
set(RENDERING_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# Common link libraries for all rendering benchmarks
set(RENDERING_COMMON_LIBS
    AnalogTimeSeries
    TensorData
    TimeFrame
)

# Common compile options
set(RENDERING_COMPILE_OPTS -O3 -march=native -fno-omit-frame-pointer -g)

# ============================================================================
# A1: Storage read throughput
# ============================================================================
if(TARGET benchmark::benchmark AND TARGET AnalogTimeSeries AND TARGET TensorData)
    add_executable(bm_storage_read bm_storage_read.cpp)
    target_link_libraries(bm_storage_read PRIVATE
        ${RENDERING_COMMON_LIBS}
        benchmark::benchmark
    )
    target_include_directories(bm_storage_read PRIVATE ${RENDERING_INCLUDE_DIR})
    target_compile_options(bm_storage_read PRIVATE ${RENDERING_COMPILE_OPTS})
    set_target_properties(bm_storage_read PROPERTIES
        CXX_STANDARD 23
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/adhoc"
    )
    message(STATUS "[adhoc/rendering] bm_storage_read: enabled")
else()
    message(STATUS "[adhoc/rendering] bm_storage_read: skipped (requires Google Benchmark + AnalogTimeSeries + TensorData)")
endif()

# ============================================================================
# A2: Vertex generation pipeline
# ============================================================================
if(TARGET benchmark::benchmark AND TARGET CorePlotting)
    add_executable(bm_vertex_generation bm_vertex_generation.cpp)
    target_link_libraries(bm_vertex_generation PRIVATE
        ${RENDERING_COMMON_LIBS}
        CorePlotting
        benchmark::benchmark
    )
    target_include_directories(bm_vertex_generation PRIVATE ${RENDERING_INCLUDE_DIR})
    target_compile_options(bm_vertex_generation PRIVATE ${RENDERING_COMPILE_OPTS})
    set_target_properties(bm_vertex_generation PROPERTIES
        CXX_STANDARD 23
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/adhoc"
    )
    message(STATUS "[adhoc/rendering] bm_vertex_generation: enabled")
else()
    message(STATUS "[adhoc/rendering] bm_vertex_generation: skipped (requires Google Benchmark + CorePlotting)")
endif()

# ============================================================================
# A3: Cache extraction (compiles AnalogVertexCache.cpp directly)
# ============================================================================
set(ANALOG_VERTEX_CACHE_SRC
    "${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataViewer_Widget/Rendering/AnalogVertexCache.cpp"
)
set(ANALOG_VERTEX_CACHE_INCLUDE
    "${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DataViewer_Widget/Rendering"
)
# AnalogVertexCache.hpp uses #include "TimeFrame/TimeFrame.hpp" which needs the
# parent DataObjects dir on the include path
set(DATAOBJECTS_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/src/DataObjects")

# AnalogVertexCache.hpp includes "TimeFrame/TimeFrame.hpp" which expects src/ on include path
set(SRC_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/src")

if(TARGET benchmark::benchmark AND TARGET TimeFrame)
    add_executable(bm_cache_extraction bm_cache_extraction.cpp ${ANALOG_VERTEX_CACHE_SRC})
    target_link_libraries(bm_cache_extraction PRIVATE
        TimeFrame
        Boost::boost
        glm::glm
        benchmark::benchmark
        spdlog::spdlog_header_only
    )
    target_include_directories(bm_cache_extraction PRIVATE
        ${RENDERING_INCLUDE_DIR}
        ${ANALOG_VERTEX_CACHE_INCLUDE}
        ${SRC_INCLUDE_DIR}
    )
    target_compile_options(bm_cache_extraction PRIVATE ${RENDERING_COMPILE_OPTS})
    set_target_properties(bm_cache_extraction PROPERTIES
        CXX_STANDARD 23
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/adhoc"
    )
    message(STATUS "[adhoc/rendering] bm_cache_extraction: enabled")
else()
    message(STATUS "[adhoc/rendering] bm_cache_extraction: skipped (requires Google Benchmark + TimeFrame)")
endif()

# ============================================================================
# B1: Min/max decimation
# ============================================================================
if(TARGET benchmark::benchmark AND TARGET AnalogTimeSeries)
    add_executable(bm_minmax_decimation bm_minmax_decimation.cpp)
    target_link_libraries(bm_minmax_decimation PRIVATE
        ${RENDERING_COMMON_LIBS}
        benchmark::benchmark
    )
    target_include_directories(bm_minmax_decimation PRIVATE ${RENDERING_INCLUDE_DIR})
    target_compile_options(bm_minmax_decimation PRIVATE ${RENDERING_COMPILE_OPTS})
    set_target_properties(bm_minmax_decimation PROPERTIES
        CXX_STANDARD 23
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/adhoc"
    )
    message(STATUS "[adhoc/rendering] bm_minmax_decimation: enabled")
else()
    message(STATUS "[adhoc/rendering] bm_minmax_decimation: skipped")
endif()

# ============================================================================
# B2: Per-pixel buffer
# ============================================================================
if(TARGET benchmark::benchmark AND TARGET AnalogTimeSeries)
    add_executable(bm_perpixel_buffer bm_perpixel_buffer.cpp)
    target_link_libraries(bm_perpixel_buffer PRIVATE
        ${RENDERING_COMMON_LIBS}
        benchmark::benchmark
    )
    target_include_directories(bm_perpixel_buffer PRIVATE ${RENDERING_INCLUDE_DIR})
    target_compile_options(bm_perpixel_buffer PRIVATE ${RENDERING_COMPILE_OPTS})
    set_target_properties(bm_perpixel_buffer PROPERTIES
        CXX_STANDARD 23
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/adhoc"
    )
    message(STATUS "[adhoc/rendering] bm_perpixel_buffer: enabled")
else()
    message(STATUS "[adhoc/rendering] bm_perpixel_buffer: skipped")
endif()

# ============================================================================
# C1: Multi-channel scaling
# ============================================================================
if(TARGET benchmark::benchmark AND TARGET AnalogTimeSeries)
    add_executable(bm_multichannel_scaling bm_multichannel_scaling.cpp)
    target_link_libraries(bm_multichannel_scaling PRIVATE
        ${RENDERING_COMMON_LIBS}
        benchmark::benchmark
    )
    target_include_directories(bm_multichannel_scaling PRIVATE ${RENDERING_INCLUDE_DIR})
    target_compile_options(bm_multichannel_scaling PRIVATE ${RENDERING_COMPILE_OPTS})
    set_target_properties(bm_multichannel_scaling PROPERTIES
        CXX_STANDARD 23
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/adhoc"
    )
    message(STATUS "[adhoc/rendering] bm_multichannel_scaling: enabled")
else()
    message(STATUS "[adhoc/rendering] bm_multichannel_scaling: skipped")
endif()

# ============================================================================
# D1: Memory profiling target (standalone main, not Google Benchmark)
# ============================================================================
if(TARGET AnalogTimeSeries AND TARGET TensorData)
    add_executable(bm_rendering_memory_profile bm_rendering_memory_profile.cpp)
    target_link_libraries(bm_rendering_memory_profile PRIVATE
        ${RENDERING_COMMON_LIBS}
    )
    target_include_directories(bm_rendering_memory_profile PRIVATE ${RENDERING_INCLUDE_DIR})
    target_compile_options(bm_rendering_memory_profile PRIVATE
        -O2 -g -fno-omit-frame-pointer
    )
    set_target_properties(bm_rendering_memory_profile PROPERTIES
        CXX_STANDARD 23
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/adhoc"
    )
    message(STATUS "[adhoc/rendering] bm_rendering_memory_profile: enabled")
else()
    message(STATUS "[adhoc/rendering] bm_rendering_memory_profile: skipped")
endif()
