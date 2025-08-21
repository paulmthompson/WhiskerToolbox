

find_package(nlohmann_json CONFIG REQUIRED)

if(TENSOR_BACKEND_LIBTORCH)
    set(CMAKE_CUDA_FLAGS "-allow-unsupported-compiler")
    find_package(Torch REQUIRED)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")
endif()



# Set backend-specific variables
if(TENSOR_BACKEND_LIBTORCH)
    message(STATUS "TensorData: Using LibTorch backend")
    
    # Find LibTorch
    find_package(Torch REQUIRED)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")
    
    # Backend sources
    set(BACKEND_SOURCES
        Tensor_Data_LibTorch.cpp
    )
    
    # Backend libraries
    set(BACKEND_LIBRARIES "${TORCH_LIBRARIES}")
    
    # Backend definitions
    set(BACKEND_DEFINITIONS TENSOR_BACKEND_LIBTORCH)
    
else()
    message(STATUS "TensorData: Using Armadillo backend")
    
    # Find Armadillo
    find_package(Armadillo REQUIRED)
    find_package(LAPACK REQUIRED)
    
    # Backend sources
    set(BACKEND_SOURCES
        Tensor_Data_Armadillo.cpp
    )
    
    # Backend libraries
    set(BACKEND_LIBRARIES armadillo LAPACK::LAPACK)
    
    # Backend definitions
    set(BACKEND_DEFINITIONS TENSOR_BACKEND_ARMADILLO)
    
endif()

# Common tensor sources
set(TENSOR_SOURCES
    Tensor_Data.hpp
    IO/numpy/Tensor_Data_numpy.hpp
    IO/numpy/Tensor_Data_numpy.cpp
    ${BACKEND_SOURCES}
)

# Create the TensorData shared library
add_library(TensorData SHARED ${TENSOR_SOURCES})

# Set up include directories
target_include_directories(TensorData PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>  # For DataManager includes
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>     # For local includes
    $<INSTALL_INTERFACE:include>
    "${npy_SOURCE_DIR}/include"  # For numpy loading
)

# Link required libraries
target_link_libraries(TensorData PUBLIC nlohmann_json::nlohmann_json)
target_link_libraries(TensorData PUBLIC WhiskerToolbox::ObserverData)
target_link_libraries(TensorData PUBLIC WhiskerToolbox::TimeFrame)

# Link backend-specific libraries
target_link_libraries(TensorData PUBLIC ${BACKEND_LIBRARIES})

# Set backend-specific compile definitions
message(STATUS "Using Backend definition: ${BACKEND_DEFINITIONS}")
target_compile_definitions(TensorData PUBLIC ${BACKEND_DEFINITIONS})

# Compiler-specific options
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(TensorData PRIVATE ${CLANG_OPTIONS})
    if(TENSOR_BACKEND_LIBTORCH)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
    endif()
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(TensorData PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(TensorData PRIVATE ${MSVC_WARNINGS})
endif()

# Set output properties
set_target_properties(TensorData PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Create alias for consistent naming
add_library(WhiskerToolbox::TensorData ALIAS TensorData)
