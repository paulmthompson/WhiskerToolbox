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

find_package(reflectcpp CONFIG REQUIRED)

add_executable(test_DeepLearning
    channel_encoding/ImageEncoder.test.cpp
    channel_encoding/Point2DEncoder.test.cpp
    channel_encoding/Mask2DEncoder.test.cpp
    channel_encoding/Line2DEncoder.test.cpp
    channel_encoding/EncoderFactory.test.cpp
    channel_encoding/EncoderDispatch.test.cpp

    channel_decoding/TensorToPoint2D.test.cpp
    channel_decoding/TensorToMask2D.test.cpp
    channel_decoding/TensorToLine2D.test.cpp
    channel_decoding/TensorToFeatureVector.test.cpp
    channel_decoding/DecoderFactory.test.cpp
    channel_decoding/DecoderDispatch.test.cpp
    channel_decoding/RoundTrip.test.cpp

    models_v2/TensorSlotDescriptor.test.cpp
    models_v2/ModelBase.test.cpp
    models_v2/ModelExecution.test.cpp
    device/DeviceManager.test.cpp

    registry/ModelRegistry.test.cpp

    models_v2/NeuroSAMModel.test.cpp

    models_v2/GeneralEncoderModel.test.cpp
    models_v2/GeneralEncoderConfiguration.test.cpp

    runtime/RuntimeModelSpec.test.cpp

    post_encoder/PostEncoderModules.test.cpp
    post_encoder/PostEncoderModuleRegistry.test.cpp
    post_encoder/PostEncoderOutputTransform.test.cpp

    storage/DataBank.test.cpp
)

target_link_libraries(test_DeepLearning PRIVATE
    Catch2::Catch2WithMain
    NEURALYZER_GEOMETRY
    "${TORCH_LIBRARIES}"
    reflectcpp::reflectcpp
)

if (UNIX AND NOT APPLE)
    target_link_libraries(test_DeepLearning PRIVATE
        -Wl,--whole-archive DeepLearning -Wl,--no-whole-archive)
else()
    target_link_libraries(test_DeepLearning PRIVATE WhiskerToolbox::DeepLearning)
endif()

target_include_directories(test_DeepLearning PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DeepLearning>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/CoreGeometry/include>"
)

set_target_compiler_warnings(test_DeepLearning)

if (ENABLE_DETAILED_TEST_DISCOVERY)
    if (WIN32)
        catch_discover_tests(test_DeepLearning
                DL_PATHS
                "$ENV{PATH}"
                "${CMAKE_BINARY_DIR}"
        )
    else()
        catch_discover_tests(test_DeepLearning)
    endif()
else()
    add_test(NAME test_DeepLearning_all
        COMMAND test_DeepLearning
        )
endif()

enable_whiskertoolbox_shared_test_pch(test_DeepLearning)

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

set_target_compiler_warnings(neurosam_test_driver)

# --- Binding Data Tests (header-only, no libtorch needed) ---
add_executable(test_binding_data
    binding_data/BindingData.test.cpp
    bindings/BindingTypes.test.cpp
)

target_link_libraries(test_binding_data PRIVATE
    Catch2::Catch2WithMain
    WhiskerToolbox::ParameterSchema
    WhiskerToolbox::DeepLearning
    reflectcpp::reflectcpp
)

target_include_directories(test_binding_data PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DeepLearning>"
)

set_target_compiler_warnings(test_binding_data)

if (ENABLE_DETAILED_TEST_DISCOVERY)
    if (WIN32)
        catch_discover_tests(test_binding_data
                DL_PATHS
                "$ENV{PATH}"
                "${CMAKE_BINARY_DIR}"
        )
    else()
        catch_discover_tests(test_binding_data)
    endif()
else()
    add_test(NAME test_binding_data_all
        COMMAND test_binding_data
        )
endif()

enable_whiskertoolbox_shared_test_pch(test_binding_data)

# --- Widget-Level Param Schema Tests (no libtorch needed) ---

add_executable(test_dl_param_schemas
    param_schemas/DeepLearningParamSchemas.test.cpp
)

target_link_libraries(test_dl_param_schemas PRIVATE
    Catch2::Catch2WithMain
    WhiskerToolbox::ParameterSchema
    WhiskerToolbox::DeepLearning
    reflectcpp::reflectcpp
)

target_include_directories(test_dl_param_schemas PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/WhiskerToolbox>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/DeepLearning>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/CoreGeometry/include>"
)

target_sources(test_dl_param_schemas PRIVATE
    "${CMAKE_SOURCE_DIR}/src/WhiskerToolbox/DeepLearning_Widget/Core/DeepLearningParamSchemas.cpp"
)

set_target_compiler_warnings(test_dl_param_schemas)

if (ENABLE_DETAILED_TEST_DISCOVERY)
    if (WIN32)
        catch_discover_tests(test_dl_param_schemas
                DL_PATHS
                "$ENV{PATH}"
                "${CMAKE_BINARY_DIR}"
        )
    else()
        catch_discover_tests(test_dl_param_schemas)
    endif()
else()
    add_test(NAME test_dl_param_schemas_all
        COMMAND test_dl_param_schemas
        )
endif()

enable_whiskertoolbox_shared_test_pch(test_dl_param_schemas)
