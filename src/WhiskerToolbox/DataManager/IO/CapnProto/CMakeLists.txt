# CapnProto Plugin CMakeLists.txt

# Find CapnProto - make it conditional
if(ENABLE_CAPNPROTO)
    find_package(CapnProto CONFIG REQUIRED)
else()
    # Create a dummy target so linking doesn't fail
    add_library(DataManagerIO_CapnProto INTERFACE)
    return()
endif()

# Generate CapnProto files
capnp_generate_cpp(CAPNP_SRCS CAPNP_HDRS line_data.capnp)

set(CAPNPROTO_PLUGIN_SOURCES
    CapnProtoLoader.hpp
    CapnProtoLoader.cpp
    Serialization.hpp
    Serialization.cpp
    Line_Data_Binary.hpp
    Line_Data_Binary.cpp
    ${CAPNP_SRCS}
    ${CAPNP_HDRS}
)

# Create the CapnProto plugin library
add_library(DataManagerIO_CapnProto ${CAPNPROTO_PLUGIN_SOURCES})

target_include_directories(DataManagerIO_CapnProto PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}  # For generated CapnProto headers
)

target_link_libraries(DataManagerIO_CapnProto PRIVATE
    DataManagerIO
    WhiskerToolbox::LineData  # Link to LineData library instead of accessing headers
    CapnProto::capnp
    nlohmann_json::nlohmann_json
    NEURALYZER_GEOMETRY
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(DataManagerIO_CapnProto PRIVATE ${CLANG_OPTIONS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(DataManagerIO_CapnProto PRIVATE ${GCC_WARNINGS})
endif()

if (MSVC)
    target_compile_options(DataManagerIO_CapnProto PRIVATE ${MSVC_WARNINGS})
endif()

# The plugin will be linked into DataManager which has access to the data types
# so we don't need to link them here - we'll use forward declarations and 
# the parent will provide the implementations

set_target_properties(DataManagerIO_CapnProto PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
