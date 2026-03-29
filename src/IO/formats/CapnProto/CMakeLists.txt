# CapnProto Plugin CMakeLists.txt

find_package(nlohmann_json CONFIG REQUIRED)

# Find CapnProto - make it conditional
if(ENABLE_CAPNPROTO)
    find_package(CapnProto CONFIG REQUIRED)
else()
    # Create a dummy target so linking doesn't fail
    add_library(DataManagerIO_CapnProto INTERFACE)
    return()
endif()

# Generate CapnProto files
capnp_generate_cpp(CAPNP_SRCS CAPNP_HDRS linedata/line_data.capnp)

set(CAPNPROTO_PLUGIN_SOURCES
    CapnProtoLoader.hpp
    CapnProtoLoader.cpp
    CapnProtoFormatLoader.hpp
    CapnProtoFormatLoader.cpp
    common/Serialization.hpp
    common/Serialization.cpp
    linedata/Line_Data_Binary.hpp
    linedata/Line_Data_Binary.cpp
    ${CAPNP_SRCS}
    ${CAPNP_HDRS}
)

# Create the CapnProto plugin library
add_library(DataManagerIO_CapnProto SHARED ${CAPNPROTO_PLUGIN_SOURCES})

include(GenerateExportHeader)

set_target_properties(DataManagerIO_CapnProto
    PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
)

generate_export_header(DataManagerIO_CapnProto 
                       EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/datamanagerio_capnproto_export.h)

target_include_directories(DataManagerIO_CapnProto PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>"  # src/DataManager for IO/core/ headers
    ${CMAKE_CURRENT_BINARY_DIR}  # For generated CapnProto headers
    PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR} #datamanagerio_capnproto_export.h
)

target_sources(
  DataManagerIO_CapnProto PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}
    FILES ${CMAKE_CURRENT_BINARY_DIR}/datamanagerio_capnproto_export.h
)


target_link_libraries(DataManagerIO_CapnProto PRIVATE
    DataManagerIO_Core        # For IFormatLoader interface
    WhiskerToolbox::LineData  # Link to LineData library instead of accessing headers
    nlohmann_json::nlohmann_json
    NEURALYZER_GEOMETRY
    ParameterSchema
    DataTypeEnum
    spdlog::spdlog_header_only
)

if(MSVC AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Adjust the path to match your vcpkg install location
    set(CAPNP_RELEASE_LIB "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/lib/capnp.lib")
    set(KJ_RELEASE_LIB "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/lib/kj.lib")
    target_link_libraries(DataManagerIO_CapnProto PRIVATE ${CAPNP_RELEASE_LIB} ${KJ_RELEASE_LIB})

    set_property(TARGET DataManagerIO_CapnProto PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreadedDLL"
    )
else()
    target_link_libraries(DataManagerIO_CapnProto PRIVATE CapnProto::capnp)
endif()

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

# Prevent static-archive symbols (e.g. TimeFrame) from leaking into the .so
target_link_options(DataManagerIO_CapnProto PRIVATE
    $<$<CXX_COMPILER_ID:Clang,GNU>:LINKER:--exclude-libs,ALL>)

set_target_properties(DataManagerIO_CapnProto PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
enable_whiskertoolbox_pch(DataManagerIO_CapnProto)