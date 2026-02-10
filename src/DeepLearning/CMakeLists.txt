
#I need this before torch include to avoid protobuf
# target conflicts
find_package(nlohmann_json CONFIG REQUIRED)
find_package(reflectcpp CONFIG REQUIRED)
find_package(OpenCV CONFIG REQUIRED)

set(CMAKE_CUDA_FLAGS "-allow-unsupported-compiler")
find_package(Torch REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")

set(DEEP_LEARNING_SOURCES
    torch_helpers.hpp

    models/efficientvit/efficientvit.hpp
    models/efficientvit/efficientvit.cpp

    models/EfficientSAM/EfficientSAM.hpp
    models/EfficientSAM/EfficientSAM.cpp

    models/scm/scm.hpp
    models/scm/scm.cpp

    channel_encoding/ChannelEncoder.hpp
    channel_encoding/ImageEncoder.hpp
    channel_encoding/ImageEncoder.cpp
    channel_encoding/Point2DEncoder.hpp
    channel_encoding/Point2DEncoder.cpp
    channel_encoding/Mask2DEncoder.hpp
    channel_encoding/Mask2DEncoder.cpp
    channel_encoding/Line2DEncoder.hpp
    channel_encoding/Line2DEncoder.cpp
    channel_encoding/EncoderFactory.hpp
    channel_encoding/EncoderFactory.cpp

    channel_decoding/ChannelDecoder.hpp
    channel_decoding/TensorToPoint2D.hpp
    channel_decoding/TensorToPoint2D.cpp
    channel_decoding/TensorToMask2D.hpp
    channel_decoding/TensorToMask2D.cpp
    channel_decoding/TensorToLine2D.hpp
    channel_decoding/TensorToLine2D.cpp
    channel_decoding/DecoderFactory.hpp
    channel_decoding/DecoderFactory.cpp

    device/DeviceManager.hpp
    device/DeviceManager.cpp

    models_v2/TensorSlotDescriptor.hpp
    models_v2/ModelBase.hpp
    models_v2/ModelExecution.hpp
    models_v2/ModelExecution.cpp

    registry/ModelRegistry.hpp
    registry/ModelRegistry.cpp

    runtime/RuntimeModelSpec.hpp
    runtime/RuntimeModelSpec.cpp
    runtime/RuntimeModel.hpp
    runtime/RuntimeModel.cpp
)

add_library(DeepLearning STATIC ${DEEP_LEARNING_SOURCES})

target_include_directories(DeepLearning PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<INSTALL_INTERFACE:include>"
)

target_link_libraries(DeepLearning PRIVATE "${TORCH_LIBRARIES}")
target_link_libraries(DeepLearning PRIVATE NEURALYZER_GEOMETRY)
target_link_libraries(DeepLearning PRIVATE executorch)
target_link_libraries(DeepLearning PRIVATE extension_module_static)
target_link_libraries(DeepLearning PRIVATE extension_tensor)
target_link_libraries(DeepLearning PUBLIC reflectcpp::reflectcpp)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(DeepLearning PRIVATE ${CLANG_OPTIONS})
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(DeepLearning PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(DeepLearning PRIVATE ${MSVC_WARNINGS})
endif()

set_target_properties(DeepLearning PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_library(WhiskerToolbox::DeepLearning ALIAS DeepLearning)