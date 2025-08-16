# CapnProto Plugin CMakeLists.txt

find_package(CapnProto CONFIG REQUIRED)

# Generate CapnProto files
capnp_generate_cpp(CAPNP_SRCS CAPNP_HDRS line_data.capnp)

set(CAPNPROTO_PLUGIN_SOURCES
    CapnProtoLoader.hpp
    CapnProtoLoader.cpp
    Serialization.hpp
    Serialization.cpp
    ${CAPNP_SRCS}
    ${CAPNP_HDRS}
)

# Create the CapnProto plugin library
add_library(DataManagerIO_CapnProto ${CAPNPROTO_PLUGIN_SOURCES})

target_include_directories(DataManagerIO_CapnProto PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${CMAKE_CURRENT_BINARY_DIR}  # For generated CapnProto headers
)

target_link_libraries(DataManagerIO_CapnProto PRIVATE
    DataManagerIO
    CapnProto::capnp
    nlohmann_json::nlohmann_json
    NEURALYZER_GEOMETRY
)

# The plugin will be linked into DataManager which has access to the data types
# so we don't need to link them here - we'll use forward declarations and 
# the parent will provide the implementations

set_target_properties(DataManagerIO_CapnProto PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
