if (APPLE)
    message(STATUS "DeepLearning testing currently not supported on MacOS")
    return()
endif()

if (WIN32)
    message(STATUS "DeepLearning testing currently not supported on Windows")
    return()
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})

add_executable(test_channel_encoding
    channel_encoding/ImageEncoder.test.cpp
    channel_encoding/Point2DEncoder.test.cpp
    channel_encoding/Mask2DEncoder.test.cpp
    channel_encoding/Line2DEncoder.test.cpp
    channel_encoding/EncoderFactory.test.cpp
)

target_link_libraries(test_channel_encoding PRIVATE
    Catch2::Catch2WithMain
    WhiskerToolbox::DeepLearning
    NEURALYZER_GEOMETRY
    "${TORCH_LIBRARIES}"
)

target_include_directories(test_channel_encoding PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DeepLearning>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/CoreGeometry/include>"
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(test_channel_encoding PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(test_channel_encoding PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(test_channel_encoding PRIVATE ${MSVC_WARNINGS})
endif()

if (ENABLE_DETAILED_TEST_DISCOVERY)
    if (WIN32)
        catch_discover_tests(test_channel_encoding
                DL_PATHS
                "$ENV{PATH}"
                "${CMAKE_BINARY_DIR}"
        )
    else()
        catch_discover_tests(test_channel_encoding)
    endif()
else()
    add_test(NAME test_channel_encoding_all
        COMMAND test_channel_encoding
        )
endif()

# --- Channel Decoding Tests ---
add_executable(test_channel_decoding
    channel_decoding/TensorToPoint2D.test.cpp
    channel_decoding/TensorToMask2D.test.cpp
    channel_decoding/TensorToLine2D.test.cpp
    channel_decoding/DecoderFactory.test.cpp
    channel_decoding/RoundTrip.test.cpp
)

target_link_libraries(test_channel_decoding PRIVATE
    Catch2::Catch2WithMain
    WhiskerToolbox::DeepLearning
    NEURALYZER_GEOMETRY
    "${TORCH_LIBRARIES}"
)

target_include_directories(test_channel_decoding PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DeepLearning>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/CoreGeometry/include>"
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(test_channel_decoding PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(test_channel_decoding PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(test_channel_decoding PRIVATE ${MSVC_WARNINGS})
endif()

if (ENABLE_DETAILED_TEST_DISCOVERY)
    if (WIN32)
        catch_discover_tests(test_channel_decoding
                DL_PATHS
                "$ENV{PATH}"
                "${CMAKE_BINARY_DIR}"
        )
    else()
        catch_discover_tests(test_channel_decoding)
    endif()
else()
    add_test(NAME test_channel_decoding_all
        COMMAND test_channel_decoding
        )
endif()

# --- Phase 3: Model Wrapper Tests ---
add_executable(test_model_wrapper
    models_v2/TensorSlotDescriptor.test.cpp
    models_v2/ModelBase.test.cpp
    models_v2/ModelExecution.test.cpp
    device/DeviceManager.test.cpp
)

target_link_libraries(test_model_wrapper PRIVATE
    Catch2::Catch2WithMain
    WhiskerToolbox::DeepLearning
    NEURALYZER_GEOMETRY
    "${TORCH_LIBRARIES}"
)

target_include_directories(test_model_wrapper PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DeepLearning>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/CoreGeometry/include>"
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(test_model_wrapper PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(test_model_wrapper PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(test_model_wrapper PRIVATE ${MSVC_WARNINGS})
endif()

if (ENABLE_DETAILED_TEST_DISCOVERY)
    if (WIN32)
        catch_discover_tests(test_model_wrapper
                DL_PATHS
                "$ENV{PATH}"
                "${CMAKE_BINARY_DIR}"
        )
    else()
        catch_discover_tests(test_model_wrapper)
    endif()
else()
    add_test(NAME test_model_wrapper_all
        COMMAND test_model_wrapper
        )
endif()

# --- Phase 4: Model Registry Tests ---
add_executable(test_model_registry
    registry/ModelRegistry.test.cpp
)

target_link_libraries(test_model_registry PRIVATE
    Catch2::Catch2WithMain
    WhiskerToolbox::DeepLearning
    NEURALYZER_GEOMETRY
    "${TORCH_LIBRARIES}"
)

target_include_directories(test_model_registry PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DeepLearning>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/CoreGeometry/include>"
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(test_model_registry PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(test_model_registry PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(test_model_registry PRIVATE ${MSVC_WARNINGS})
endif()

if (ENABLE_DETAILED_TEST_DISCOVERY)
    if (WIN32)
        catch_discover_tests(test_model_registry
                DL_PATHS
                "$ENV{PATH}"
                "${CMAKE_BINARY_DIR}"
        )
    else()
        catch_discover_tests(test_model_registry)
    endif()
else()
    add_test(NAME test_model_registry_all
        COMMAND test_model_registry
        )
endif()

# --- Phase 5: Runtime Model Tests ---
find_package(reflectcpp CONFIG REQUIRED)

# --- Phase 7: NeuroSAM Model Tests ---
add_executable(test_neurosam_model
    models_v2/NeuroSAMModel.test.cpp
)

target_link_libraries(test_neurosam_model PRIVATE
    Catch2::Catch2WithMain
    WhiskerToolbox::DeepLearning
    NEURALYZER_GEOMETRY
    "${TORCH_LIBRARIES}"
)

target_include_directories(test_neurosam_model PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DeepLearning>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/CoreGeometry/include>"
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(test_neurosam_model PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(test_neurosam_model PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(test_neurosam_model PRIVATE ${MSVC_WARNINGS})
endif()

if (ENABLE_DETAILED_TEST_DISCOVERY)
    if (WIN32)
        catch_discover_tests(test_neurosam_model
                DL_PATHS
                "$ENV{PATH}"
                "${CMAKE_BINARY_DIR}"
        )
    else()
        catch_discover_tests(test_neurosam_model)
    endif()
else()
    add_test(NAME test_neurosam_model_all
        COMMAND test_neurosam_model
        )
endif()

add_executable(test_runtime_model
    runtime/RuntimeModelSpec.test.cpp
)

target_link_libraries(test_runtime_model PRIVATE
    Catch2::Catch2WithMain
    WhiskerToolbox::DeepLearning
    NEURALYZER_GEOMETRY
    "${TORCH_LIBRARIES}"
    reflectcpp::reflectcpp
)

target_include_directories(test_runtime_model PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DeepLearning>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/CoreGeometry/include>"
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(test_runtime_model PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(test_runtime_model PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(test_runtime_model PRIVATE ${MSVC_WARNINGS})
endif()

if (ENABLE_DETAILED_TEST_DISCOVERY)
    if (WIN32)
        catch_discover_tests(test_runtime_model
                DL_PATHS
                "$ENV{PATH}"
                "${CMAKE_BINARY_DIR}"
        )
    else()
        catch_discover_tests(test_runtime_model)
    endif()
else()
    add_test(NAME test_runtime_model_all
        COMMAND test_runtime_model
        )
endif()

# ────────────────────────────────────────────────────────────────────────────
# Standalone test driver for NeuroSAM debugging
# ────────────────────────────────────────────────────────────────────────────

add_executable(neurosam_test_driver
    neurosam_test_driver.cpp
)

target_link_libraries(neurosam_test_driver PRIVATE
    WhiskerToolbox::DeepLearning
    NEURALYZER_GEOMETRY
    "${TORCH_LIBRARIES}"
)

target_include_directories(neurosam_test_driver PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DeepLearning>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/CoreGeometry/include>"
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(neurosam_test_driver PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(neurosam_test_driver PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(neurosam_test_driver PRIVATE ${MSVC_WARNINGS})
endif()
